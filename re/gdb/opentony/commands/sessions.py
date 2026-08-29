"""Frontend and session-control commands for GDB experiments."""

from __future__ import annotations

import gdb

from ..breakpoint import CountingBreakpoint, TonyBreakpoint
from ..memory import mem
from .common import (
    argv,
    integer,
    key_loop_breakpoints,
    runtime_breakpoints,
    write,
)
from .control import TonyKeyLoopBreakpoint
from .knowledge import GLOBALS, THPS2_ADDRESSES, THPS2_LEVELS

_frontend_screen_automation = None


class TonySkipMovieBreakpoint(TonyBreakpoint):
    """Return immediately from either Bink movie entry path.

    The shared setup routine is used for startup logos, level previews, and
    level transitions.  Its callers treat a false result as "movie
    unavailable/skipped" and continue their own state machines.
    """

    def __init__(self, address: int):
        # PCMovie_PlayGameFMV (0x004e7090) is only the outer formatter/loader.
        # Startup calls the blocking wrapper at 0x004e5ec0, while level-select
        # and other frontend FMVs call the Bink setup routine 0x004e6590
        # directly.  Both must be bypassed for frontend-driven experiments.
        super().__init__(address, internal=True)

    def on_hit(self, ctx):
        return_address = ctx.return_address()
        # The callers use a false result to leave the optional movie path and
        # continue ordinary frontend/gameplay setup.  Returning success here
        # enters the Bink surface/frame loop and does not mean "already
        # completed".
        result = 0
        gdb.execute(f"set $eax = {result}")
        gdb.execute(f"set $eip = 0x{return_address:x}")
        gdb.execute(f"set $esp = 0x{ctx.esp + 4:x}")
        write(f"skipped movie playback; returning to 0x{return_address:08x} (result {result})")


_movie_skip_breakpoints = []


class TonySkipMovies(gdb.Command):
    """tony-skip-movies -- bypass blocking logo and intro movie playback."""

    def __init__(self):
        super().__init__("tony-skip-movies", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        global _movie_skip_breakpoints
        if arg.strip():
            raise gdb.GdbError("usage: tony-skip-movies")
        if _movie_skip_breakpoints and all(bp.is_valid() for bp in _movie_skip_breakpoints):
            write("startup movie bypass is already enabled")
            return
        addresses = (0x004E5EC0, 0x004E6590)
        _movie_skip_breakpoints = [TonySkipMovieBreakpoint(address) for address in addresses]
        formatted = ", ".join(f"0x{address:08x}" for address in addresses)
        write(f"startup movie bypass enabled at {formatted}")


class TonyForceLevelBreakpoint(CountingBreakpoint):
    """Replace the next Front_LaunchGameLevel level argument."""

    def __init__(self, level: int, label: str):
        address = THPS2_ADDRESSES["launch_level"][0]
        super().__init__(address, count=1, internal=True, temporary=True, should_stop=True)
        self.level = level
        self.label = label

    def on_count(self, ctx):
        original = ctx.arg(0)
        mem.write_u32(ctx.esp + 4, self.level)
        write(f"forced launch level {original} -> {self.level} ({self.label})")
        return True


class TonyForceLevel(gdb.Command):
    """tony-force-level NAME|INDEX -- replace the next level launch argument."""

    def __init__(self):
        super().__init__("tony-force-level", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = argv(arg, "tony-force-level NAME|INDEX")
        if len(values) != 1:
            raise gdb.GdbError("usage: tony-force-level NAME|INDEX")
        value = values[0].casefold()
        if value in THPS2_LEVELS:
            level = THPS2_LEVELS[value]
            label = value
        else:
            level = integer(values[0])
            label = f"level-{level}"
        if not 0 <= level < 13:
            raise gdb.GdbError("THPS2 level index must be between 0 and 12")
        TonyForceLevelBreakpoint(level, label)
        write(f"next level launch will use {level} ({label})")


class TonyFrontendLevelOverrideBreakpoint(CountingBreakpoint):
    """Replace the level selected by the real frontend level-select helper."""

    # The level-select handler enters here before its helper calls.  Synthetic
    # input can make that helper take the no-selection path, so provide the
    # desired result and resume at the original validation/store sequence.
    LEVEL_SELECT_ENTRY = 0x0045355D
    LEVEL_RESULT_CHECK = 0x0045359B

    def __init__(self, level: int, label: str):
        self.level = level
        self.label = label
        super().__init__(
            self.LEVEL_SELECT_ENTRY,
            count=1,
            internal=True,
        )

    def on_count(self, _ctx):
        gdb.execute(f"set $eax = {self.level}")
        gdb.execute(f"set $eip = 0x{self.LEVEL_RESULT_CHECK:x}")
        write(f"frontend level override: {self.label} ({self.level}) at 0x{self.LEVEL_SELECT_ENTRY:08x}")
        return True


class TonyFrontendPlayBreakpoint(TonyBreakpoint):
    """Force PLAY_GAME in the verified main-menu result slot."""

    # FrontEnd_Main calls 0x0046aee0 at 0x004532a5 and reads the result at
    # 0x004532aa.  Break after the call and write the caller's local slot so
    # the real selection helper remains on the path.
    FRONTEND_RESULT_READ = 0x004532AA
    FRONTEND_RESULT_OFFSET = 0x58
    # FrontEnd_Main's result dispatch sends 0x26 through the ordinary game
    # path.  0x2a also reaches the PLAY_GAME screen, but first loads DemoA.rec
    # and enables video-restart playback.
    PLAY_GAME_RESULT = 0x26

    def __init__(self, followup_enter_cycles: int = 0, level_index: int | None = None):
        if followup_enter_cycles < 0:
            raise ValueError("frontend follow-up cycles must not be negative")
        if level_index is not None and not 0 <= level_index < 13:
            raise ValueError("frontend level index must be between 0 and 12")
        self.followup_enter_cycles = followup_enter_cycles
        self.level_index = level_index
        super().__init__(self.FRONTEND_RESULT_READ, internal=True)

    def on_hit(self, ctx):
        mem.write_u32(
            ctx.esp + self.FRONTEND_RESULT_OFFSET,
            self.PLAY_GAME_RESULT,
        )
        self.enabled = False
        if self.followup_enter_cycles:
            global _frontend_screen_automation
            if _frontend_screen_automation is None:
                _frontend_screen_automation = TonyFrontendScreenAutomationBreakpoint(
                    self.followup_enter_cycles,
                    self.level_index,
                )
        write(
            "forced main-menu selection PLAY_GAME at result read "
            f"0x{self.FRONTEND_RESULT_READ:08x}; "
            f"follow-up Enter cycles {self.followup_enter_cycles}"
        )


class TonyFrontendScreenAutomationBreakpoint(TonyBreakpoint):
    """Select a known level, then arm Enter at the level-select boundary."""

    SCREEN_LOG = 0x0045326B

    def __init__(self, cycles: int, level_index: int | None = None):
        if cycles <= 0:
            raise ValueError("frontend automation cycles must be positive")
        if level_index is not None and not 0 <= level_index < 13:
            raise ValueError("frontend level index must be between 0 and 12")
        self.cycles = cycles
        self.level_index = level_index
        self.last_screen = None
        super().__init__(self.SCREEN_LOG, internal=True)

    def _arm_enter(self) -> None:
        key_loop_breakpoints.append(
            TonyKeyLoopBreakpoint(
                0x1C,
                press_ticks=5,
                release_ticks=15,
                cycles=self.cycles,
            )
        )
        write(f"frontend level selected: armed Enter cycles {self.cycles}")

    def _arm_summary_enter(self) -> None:
        breakpoint = TonyNetmenuSummaryBreakpoint(self.cycles)
        runtime_breakpoints.append(breakpoint)
        write(f"frontend play: waiting for NETMENU_InitSummary before final Enter cycles {self.cycles}")

    def on_hit(self, ctx):
        screen = ctx.register("eax")
        if screen == self.last_screen:
            return
        self.last_screen = screen
        if screen not in (2, 6):
            return
        if screen == 2 and self.level_index is not None:
            label = next(
                (name for name, index in THPS2_LEVELS.items() if index == self.level_index),
                f"level-{self.level_index}",
            )
            override = TonyFrontendLevelOverrideBreakpoint(self.level_index, label)
            runtime_breakpoints.append(override)
            write(f"frontend screen {screen}: real level-select result will use {label} ({self.level_index})")
            self._arm_enter()
            return
        self._arm_enter()
        self._arm_summary_enter()


class TonyNetmenuSummaryBreakpoint(TonyBreakpoint):
    """Send the final frontend confirmation after the level has loaded."""

    # NETMENU_InitSummary emits its diagnostic string at this instruction,
    # after the level/player setup and before the summary menu waits for input.
    SUMMARY_LOG = 0x0047FB36

    def __init__(self, enter_cycles: int):
        self.enter_cycles = enter_cycles
        super().__init__(self.SUMMARY_LOG, internal=True)

    def on_hit(self, _ctx):
        self.enabled = False
        key_loop_breakpoints.append(
            TonyKeyLoopBreakpoint(
                0x1C,
                press_ticks=5,
                release_ticks=15,
                cycles=self.enter_cycles,
            )
        )
        write(f"frontend NETMENU_InitSummary reached: armed final Enter cycles {self.enter_cycles}")


class TonyFrontendPlay(gdb.Command):
    """tony-frontend-play [ENTER_CYCLES] [LEVEL] -- force main-menu PLAY_GAME."""

    def __init__(self):
        super().__init__("tony-frontend-play", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        del from_tty
        values = argv(arg, "tony-frontend-play [ENTER_CYCLES] [LEVEL]") if arg.strip() else []
        if len(values) > 2:
            raise gdb.GdbError("usage: tony-frontend-play [ENTER_CYCLES] [LEVEL]")
        try:
            enter_cycles = integer(values[0]) if values else 0
            level_index = integer(values[1]) if len(values) > 1 else None
            breakpoint = TonyFrontendPlayBreakpoint(enter_cycles, level_index)
        except (ValueError, gdb.GdbError) as exc:
            raise gdb.GdbError(str(exc)) from exc
        runtime_breakpoints.append(breakpoint)
        write(
            "main-menu PLAY_GAME override armed at "
            f"0x{breakpoint.FRONTEND_RESULT_READ:08x}; "
            f"follow-up Enter cycles {enter_cycles}; "
            f"level {breakpoint.level_index if breakpoint.level_index is not None else 'current'}"
        )


class TonyFrontendConfirmBreakpoint(TonyBreakpoint):
    """Release the verified frontend selection loop at its key-state read."""

    KEY_STATE_HELPER = 0x004E41B0
    SELECTION_CALL_RETURN = 0x0046AF9F
    CONFIRM_SCAN_CODE = 0x10

    def __init__(
        self,
        scan_code: int = CONFIRM_SCAN_CODE,
        count: int = 1,
        followup_enter_cycles: int = 0,
    ):
        if not 0 <= scan_code < 0x100:
            raise ValueError("frontend confirmation scan code must be between 0 and 255")
        if count <= 0:
            raise ValueError("frontend confirmation count must be positive")
        if followup_enter_cycles < 0:
            raise ValueError("frontend follow-up cycles must not be negative")
        self.scan_code = scan_code
        self.remaining = count
        self.followup_enter_cycles = followup_enter_cycles
        super().__init__(self.KEY_STATE_HELPER, internal=True)

    def on_hit(self, ctx):
        if ctx.return_address() != self.SELECTION_CALL_RETURN:
            return
        ctx.memory.write_u8(
            GLOBALS["KeyboardState"] + self.scan_code,
            0x80,
        )
        self.remaining -= 1
        if self.remaining <= 0:
            self.enabled = False
            if self.followup_enter_cycles:
                key_loop_breakpoints.append(
                    TonyKeyLoopBreakpoint(
                        0x1C,
                        press_ticks=5,
                        release_ticks=15,
                        cycles=self.followup_enter_cycles,
                    )
                )
        write(
            "released frontend selection loop with keyboard scan "
            f"0x{self.scan_code:02x} ({self.remaining} confirmations remaining); "
            f"follow-up Enter cycles {self.followup_enter_cycles}"
        )


class TonyFrontendConfirm(gdb.Command):
    """tony-frontend-confirm [SCAN] [COUNT] [ENTER_CYCLES] -- release frontend loops."""

    def __init__(self):
        super().__init__("tony-frontend-confirm", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        del from_tty
        values = argv(arg, "tony-frontend-confirm [SCAN] [COUNT] [ENTER_CYCLES]") if arg.strip() else []
        if len(values) > 3:
            raise gdb.GdbError("usage: tony-frontend-confirm [SCAN] [COUNT] [ENTER_CYCLES]")
        try:
            scan_code = integer(values[0]) if values else TonyFrontendConfirmBreakpoint.CONFIRM_SCAN_CODE
            count = integer(values[1]) if len(values) > 1 else 1
            enter_cycles = integer(values[2]) if len(values) > 2 else 0
            breakpoint = TonyFrontendConfirmBreakpoint(scan_code, count, enter_cycles)
        except (ValueError, gdb.GdbError) as exc:
            raise gdb.GdbError(str(exc)) from exc
        runtime_breakpoints.append(breakpoint)
        write(
            "frontend confirmation armed at "
            f"0x{breakpoint.KEY_STATE_HELPER:08x}; "
            f"scan 0x{breakpoint.scan_code:02x}, count {count}, "
            f"follow-up Enter cycles {enter_cycles}"
        )
