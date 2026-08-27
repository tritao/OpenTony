"""Compatibility commands built on the reusable GDB primitives."""

from __future__ import annotations

import json
import os
import shlex
from pathlib import Path

import gdb

from .action import ActionMaskSequenceProbe
from .breakpoint import Context, CountingBreakpoint, TonyBreakpoint
from .camera import (
    ActorSubmissionProbe,
    CameraCollisionProbe,
    CameraCollisionResultProbe,
    CameraEffectProbe,
    CameraModeOverrideProbe,
    CameraPointSelectProbe,
    CameraPointStateProbe,
    CameraPositionTransformProbe,
    CameraProbe,
    CameraTimingProbe,
    CameraViewportControlProbe,
    GeometryRasterReturnProbe,
    GeometrySubmissionProbe,
    TransformedVertexProbe,
    ViewProjectionPerturbProbe,
    ViewProjectionProbe,
)
from .collision import (
    COLLISION_INIT_BOUNDARY_CASES,
    CollisionDynamicCullProbe,
    CollisionDynamicProbe,
    CollisionDynamicTransformMutationProbe,
    CollisionDynamicTransformProbe,
    CollisionFlagProbe,
    CollisionLoaderProbe,
    CollisionModelKindProbe,
    CollisionQueryInitBoundaryProbe,
    CollisionQueryProbe,
)
from .frame import FrameBreakpoint, frame_clock
from .knowledge import BUILD_SHA256, GLOBALS, known_function_addresses
from .memory import mem
from .physics import (
    GROUND_MOTION_CONTROL_WRITERS,
    GROUND_MOTION_CORRECTION_WRITERS,
    GROUND_MOTION_PROFILE_WRITERS,
    GROUND_MOTION_RANDOM_SITES,
    OLLIE_LATCH_WRITERS,
    SPECIAL_HANDLER_INFO,
    AirCollisionQueryProbe,
    GroundMotionControlWriterProbe,
    GroundMotionProducerProbe,
    GroundMotionProfileWriterProbe,
    GroundMotionRandomProbe,
    InAirHandlerProbe,
    MovementPhysicsProbe,
    OllieLatchProbe,
    PhysicsProbe,
    PhysicsStateRequestProbe,
    PhysicsStateWriterProbe,
    PlayerDiffProbe,
    SpecialPhysicsHandlerProbe,
    SyntheticPhysicsStateForceProbe,
)
from .physics import GroundMotionWriterProbe as GroundMotionCorrectionWriterProbe
from .position import POSITION_COMMIT_CALLS, PositionCommitBreakpoint
from .recording import RecordingController, RecordingError
from .replay import create_retail_replay
from .snapshot import format_diff, snapshots
from .trace import JsonlWriter
from .trg import Type192CommandProbe
from .watchpoint import watchpoints

THPS2_BUILD_SHA256 = BUILD_SHA256
THPS2_ADDRESSES = dict(known_function_addresses())
THPS2_ADDRESSES["physics_frame"] = (
    0x0049E680,
    "per-frame gameplay skater action, collision, state, and position-update wrapper",
    "re/evidence/functions/physics.md",
)

THPS2_LEVELS = {
    "hangar": 0,
    "warehouse": 12,
}

ACTION_STATE_BASE = GLOBALS["InputActionStates"]
ACTION_STATE_RECORDS = {
    "jump": (ACTION_STATE_BASE, 0x0010),
    "grind": (ACTION_STATE_BASE + 0x10, 0x0080),
    "grab": (ACTION_STATE_BASE + 0x20, 0x0020),
    "kick": (ACTION_STATE_BASE + 0x30, 0x0040),
    "spinleft": (ACTION_STATE_BASE + 0x40, 0x0004),
    "nollie": (ACTION_STATE_BASE + 0x50, 0x0001),
    "spinright": (ACTION_STATE_BASE + 0x60, 0x0008),
    "switch": (ACTION_STATE_BASE + 0x70, 0x0002),
    "left": (ACTION_STATE_BASE + 0x80, 0x8000),
    "right": (ACTION_STATE_BASE + 0x90, 0x2000),
    "up": (ACTION_STATE_BASE + 0xA0, 0x1000),
    "down": (ACTION_STATE_BASE + 0xB0, 0x4000),
}

_trace_writer = None
_WATCH_DEFAULT_LIMIT = 256
_key_loop_breakpoints = []
_recording_controller = None
_retail_replay = None
_frontend_screen_automation = None


_RECORDING_RAW_AXIS_ADDRESS = 0x0056AFBD
_RECORDING_NORMALIZED_AXIS_ADDRESS = 0x0056B140


def _recording_signed8(value: int) -> int:
    return value - 0x100 if value & 0x80 else value


def _recording_signed32(value: int) -> int:
    return value - 0x100000000 if value & 0x80000000 else value


def _recording_signed16(value: int) -> int:
    return value - 0x10000 if value & 0x8000 else value


def _recording_vec(address: int) -> dict | None:
    if not mem.readable(address, 0x0c):
        return None
    raw = list(mem.u32_vec3(address))
    return {
        "raw": raw,
        "signed": [_recording_signed32(value) for value in raw],
    }


def _recording_short_vec(address: int) -> dict | None:
    if not mem.readable(address, 6):
        return None
    raw = [mem.u16(address + offset) for offset in (0, 2, 4)]
    return {
        "raw": raw,
        "signed": [_recording_signed16(value) for value in raw],
    }


def _recording_input(controller: RecordingController) -> tuple[dict, bool]:
    keyboard = mem.bytes(GLOBALS["KeyboardState"], 0x100)
    hotkey = controller.hotkey_scan_code
    raw_axes = (
        list(mem.bytes(_RECORDING_RAW_AXIS_ADDRESS, 4))
        if mem.readable(_RECORDING_RAW_AXIS_ADDRESS, 4)
        else None
    )
    normalized_axes = (
        {
            "horizontal": _recording_signed8(mem.u8(_RECORDING_NORMALIZED_AXIS_ADDRESS)),
            "vertical": _recording_signed8(mem.u8(_RECORDING_NORMALIZED_AXIS_ADDRESS + 1)),
        }
        if mem.readable(_RECORDING_NORMALIZED_AXIS_ADDRESS, 2)
        else None
    )
    return (
        {
            "action_mask": mem.u16(GLOBALS["ActionMask"]),
            "action_mask_raw_u32": mem.u32(GLOBALS["ActionMask"]),
            "raw_controller_sample": raw_axes,
            "raw_axes": {
                "device_bytes": (
                    [{"raw": value, "signed": _recording_signed8(value)} for value in raw_axes]
                    if raw_axes is not None
                    else None
                ),
                "normalized": normalized_axes,
            },
            "normalized_axes": normalized_axes,
            "keyboard_state": keyboard.hex(),
            "held_scan_codes": [
                code
                for code, value in enumerate(keyboard)
                if value & 0x80 and code != hotkey
            ],
            "control_hotkey_scan_code": hotkey,
        },
        bool(keyboard[hotkey] & 0x80),
    )


def _recording_player_snapshot(player: int) -> dict:
    physics_state = mem.u32(player + 0x30B8)
    return {
        "player_address": f"0x{player:08x}",
        "physics_state": physics_state,
        "physics": {
            "state_raw": physics_state,
            "previous_state_raw": mem.u32(player + 0x30C0),
            "auxiliary_state_raw": mem.u32(player + 0x30C4),
        },
        "position": _recording_vec(player + 0x08),
        "position_history": _recording_vec(player + 0xBC),
        "response_velocity": _recording_vec(player + 0x4C),
        "correction": _recording_vec(player + 0x58),
        "air_motion": _recording_vec(player + 0x310C),
        "turn": {
            "accumulator_raw": mem.u32(player + 0x3144),
            "mirror_raw": mem.u32(player + 0x3148),
        },
        "basis": {
            "forward_raw": _recording_vec(player + 0x30F4),
            "up_raw": _recording_vec(player + 0x3100),
            "air_raw": _recording_vec(player + 0x310C),
        },
        "orientation": {
            "row_0": _recording_short_vec(player + 0x2E58),
            "row_1": _recording_short_vec(player + 0x2E5E),
            "row_2": _recording_short_vec(player + 0x2E64),
        },
        "animation": {
            "id_raw": mem.u16(player + 0xF6),
            "frame_raw": mem.s16(player + 0xF4),
            "fraction_raw": mem.u16(player + 0x104),
            "rate_raw": mem.u32(player + 0x108),
            "mode_raw": mem.u8(player + 0xF8),
            "direction_raw": mem.s8(player + 0x100),
            "endpoint_raw": mem.s8(player + 0x101),
            "alternate_endpoint_raw": mem.s8(player + 0x102),
            "finished_raw": mem.u8(player + 0x107),
        },
    }


def _recording_metadata(player: int) -> dict:
    level = mem.u32(GLOBALS["CurrentLevel"])
    return {
        "binary_sha256": THPS2_BUILD_SHA256,
        "retail_executable_sha256": THPS2_BUILD_SHA256,
        "instrumentation_version": "gdb-recording-v1",
        "level": {
            "index": level,
            "name": next((name for name, index in THPS2_LEVELS.items() if index == level), None),
        },
        "player_identity": {
            "slot": 0,
            "object_address": f"0x{player:08x}",
        },
    }


def _argv(arg: str, usage: str) -> list[str]:
    try:
        values = shlex.split(arg)
    except ValueError as exc:
        raise gdb.GdbError(f"invalid arguments: {exc}") from exc
    if not values:
        raise gdb.GdbError(f"usage: {usage}")
    return values


def _integer(value: str) -> int:
    try:
        # parse_and_eval also accepts useful GDB expressions such as $eip.
        return int(gdb.parse_and_eval(value))
    except (gdb.error, TypeError, ValueError) as exc:
        raise gdb.GdbError(f"expected an address or integer expression, got {value!r}") from exc


def _write(text: str) -> None:
    gdb.write(text + "\n")


class _RecordingEventSink:
    """Adapter used by existing probes to append into the active frame."""

    def __init__(self, controller: RecordingController):
        self.controller = controller

    def event(self, record: dict) -> None:
        self.controller.event(record)


class TonyRecordingInputBreakpoint(TonyBreakpoint):
    """Observe post-poll input and detect the recorder hotkey edge."""

    def __init__(self, controller: RecordingController):
        self.controller = controller
        super().__init__(THPS2_ADDRESSES["gameplay_update"][0], internal=True)

    def on_hit(self, _ctx: Context) -> None:
        self.controller.poll_control()
        input_record, hotkey_down = _recording_input(self.controller)
        try:
            self.controller.on_input(input_record, hotkey_down=hotkey_down)
        except RecordingError as exc:
            self.controller.set_error(str(exc))


class TonyRecordingFrameReturnBreakpoint(TonyBreakpoint):
    """Finalize one physics frame at the return address captured at entry."""

    def __init__(
        self,
        controller: RecordingController,
        address: int,
        frame: int,
        player: int,
    ):
        self.controller = controller
        self.frame = frame
        self.player = player
        super().__init__(address, internal=True, temporary=True)

    def on_hit(self, _ctx: Context) -> None:
        self.enabled = False
        if self.controller.active_frame != self.frame:
            return
        try:
            self.controller.end_frame(_recording_player_snapshot(self.player))
        except (OSError, TypeError, ValueError, RecordingError) as exc:
            # Recording is an observer.  A malformed/unsupported probe value
            # must end the file cleanly and never leave the gameplay thread
            # stopped at an internal temporary breakpoint.
            self.controller.close_incomplete(
                f"frame-{self.frame}-finalization-failed: {exc}"
            )


class TonyRecordingFrameEntryBreakpoint(TonyBreakpoint):
    """Begin capture at the canonical per-player physics-frame boundary."""

    def __init__(self, controller: RecordingController):
        self.controller = controller
        super().__init__(THPS2_ADDRESSES["physics_frame"][0], internal=True)

    def on_hit(self, ctx: Context) -> None:
        player = ctx.this_ptr()
        current = mem.u32(GLOBALS["Player"])
        if not mem.valid(player) or player != current:
            return
        input_record = self.controller.latest_input
        if input_record is None:
            input_record, _hotkey_down = _recording_input(self.controller)
        try:
            frame = self.controller.begin_frame(
                _recording_player_snapshot(player),
                input_record=input_record,
                metadata=_recording_metadata(player),
            )
        except RecordingError as exc:
            raise gdb.GdbError(str(exc)) from exc
        if frame is None:
            return
        return_address = ctx.return_address()
        if return_address == 0:
            raise gdb.GdbError("could not install recording frame return breakpoint")
        _runtime_breakpoints.append(
            TonyRecordingFrameReturnBreakpoint(
                self.controller,
                return_address,
                frame,
                player,
            )
        )


class TonyRecordingStart(gdb.Command):
    """tony-record-start [FILE] [--force] -- start at the next physics frame."""

    def __init__(self):
        super().__init__("tony-record-start", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        del from_tty
        values = _argv(arg, "tony-record-start [FILE] [--force]") if arg.strip() else []
        force = values[-1:] == ["--force"]
        if force:
            values.pop()
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-record-start [FILE] [--force]")
        if _recording_controller is None:
            raise gdb.GdbError("recording controller is not initialized")
        try:
            recording_id = _recording_controller.request_start(
                values[0] if values else None,
                overwrite=force,
            )
        except RecordingError as exc:
            raise gdb.GdbError(str(exc)) from exc
        _write(
            f"recording start pending: {recording_id}; "
            f"path {_recording_controller.path}"
        )


class TonyRecordingStop(gdb.Command):
    """tony-record-stop -- close after the current complete physics frame."""

    def __init__(self):
        super().__init__("tony-record-stop", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        del from_tty
        if arg.strip():
            raise gdb.GdbError("usage: tony-record-stop")
        if _recording_controller is None:
            raise gdb.GdbError("recording controller is not initialized")
        if not _recording_controller.request_stop():
            _write("no recording is active")
            return
        _write("recording stop pending at the next physics-frame return")


class TonyRecordingToggle(gdb.Command):
    """tony-record-toggle [FILE] [--force] -- share the hotkey lifecycle."""

    def __init__(self):
        super().__init__("tony-record-toggle", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        del from_tty
        values = _argv(arg, "tony-record-toggle [FILE] [--force]") if arg.strip() else []
        force = values[-1:] == ["--force"]
        if force:
            values.pop()
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-record-toggle [FILE] [--force]")
        if _recording_controller is None:
            raise gdb.GdbError("recording controller is not initialized")
        try:
            recording_id = _recording_controller.request_toggle(
                values[0] if values else None,
                overwrite=force,
            )
        except RecordingError as exc:
            raise gdb.GdbError(str(exc)) from exc
        _write(f"recording state: {_recording_controller.state.value} ({recording_id})")


class TonyRecordingStatus(gdb.Command):
    """tony-record-status -- print the controller's current state."""

    def __init__(self):
        super().__init__("tony-record-status", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        del from_tty
        if arg.strip():
            raise gdb.GdbError("usage: tony-record-status")
        if _recording_controller is None:
            raise gdb.GdbError("recording controller is not initialized")
        _write(json.dumps(_recording_controller.status(), sort_keys=True))


class TonyRetailReplay(gdb.Command):
    """tony-replay-retail FILE -- inject and compare one retail recording."""

    def __init__(self):
        super().__init__("tony-replay-retail", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        del from_tty
        values = _argv(arg, "tony-replay-retail FILE")
        if len(values) != 1:
            raise gdb.GdbError("usage: tony-replay-retail FILE")
        global _retail_replay
        if _retail_replay is not None:
            raise gdb.GdbError("a retail replay is already armed")
        try:
            _retail_replay = create_retail_replay(values[0])
        except (OSError, TypeError, ValueError, gdb.GdbError) as exc:
            raise gdb.GdbError(str(exc)) from exc
        _retail_replay.install()


def _snapshot_address(value: str) -> int:
    try:
        return _integer(value)
    except gdb.GdbError:
        if value.casefold() == "player":
            from .player import PlayerView

            player = PlayerView.current()
            if player is not None:
                return player.address
        raise


_PLAYER_WATCH_FIELDS = {
    "position.x": 0x08,
    "position.y": 0x0C,
    "position.z": 0x10,
    "position_history.x": 0xBC,
    "position_history.y": 0xC0,
    "position_history.z": 0xC4,
    "physics_state": 0x30B8,
    "previous_physics_state": 0x30C0,
    "unknown_state": 0x30C4,
}


def _watch_address(value: str) -> tuple[int, str]:
    """Resolve a raw address or a small candidate PlayerView expression."""

    lowered = value.casefold()
    if lowered == "player" or lowered.startswith(("player+", "player.")):
        from .player import PlayerView

        player = PlayerView.current()
        if player is None:
            raise gdb.GdbError("the generated Player pointer is null or unreadable")
        if lowered == "player":
            return player.address, value
        if lowered.startswith("player+"):
            try:
                offset = int(value[7:], 0)
            except ValueError as exc:
                raise gdb.GdbError(f"invalid player offset {value!r}") from exc
            if offset < 0:
                raise gdb.GdbError("player watch offset must be non-negative")
            return player.address + offset, value
        field = lowered[7:]
        try:
            return player.address + _PLAYER_WATCH_FIELDS[field], value
        except KeyError as exc:
            fields = ", ".join(sorted(_PLAYER_WATCH_FIELDS))
            raise gdb.GdbError(f"unknown PlayerView field {field!r}; choose one of: {fields}") from exc
    return _integer(value), value


def _action_state(address: int) -> dict:
    raw = mem.bytes(address, 0x10)
    return {
        "address": f"0x{address:08x}",
        "raw": raw.hex(),
        "byte0": raw[0],
        "byte1": raw[1],
        "u32_4": int.from_bytes(raw[4:8], "little"),
        "u32_8": int.from_bytes(raw[8:12], "little"),
        "u32_c": int.from_bytes(raw[12:16], "little"),
    }


class TonyReadInteger(gdb.Command):
    def __init__(self, name: str, reader, label: str):
        super().__init__(name, gdb.COMMAND_DATA)
        self.command_name = name
        self.reader = reader
        self.label = label

    def invoke(self, arg, from_tty):
        values = _argv(arg, f"{self.command_name} ADDRESS")
        if len(values) != 1:
            raise gdb.GdbError(f"usage: {self.command_name} ADDRESS")
        address = _integer(values[0])
        value = self.reader(address)
        _write(f"0x{address:08x}: {self.label} {value} (0x{value:x})")


class TonyReadFloat(gdb.Command):
    """Read a little-endian float32 from inferior memory."""

    def __init__(self):
        super().__init__("tony-readf", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-readf ADDRESS")
        if len(values) != 1:
            raise gdb.GdbError("usage: tony-readf ADDRESS")
        address = _integer(values[0])
        value = mem.f32(address)
        _write(f"0x{address:08x}: float32 {value!r}")


def _hexdump(address: int, data: bytes, width: int = 16) -> str:
    lines = []
    for offset in range(0, len(data), width):
        chunk = data[offset:offset + width]
        hex_part = " ".join(f"{byte:02x}" for byte in chunk)
        hex_part = f"{hex_part:<{width * 3 - 1}}"
        ascii_part = "".join(chr(byte) if 32 <= byte < 127 else "." for byte in chunk)
        lines.append(f"0x{address + offset:08x}:  {hex_part}  |{ascii_part}|")
    return "\n".join(lines)


class TonyHexdump(gdb.Command):
    """tony-hexdump ADDRESS LENGTH [WIDTH] -- print inferior memory."""

    def __init__(self):
        super().__init__("tony-hexdump", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-hexdump ADDRESS LENGTH [WIDTH]")
        if len(values) not in (2, 3):
            raise gdb.GdbError("usage: tony-hexdump ADDRESS LENGTH [WIDTH]")
        address = _integer(values[0])
        length = _integer(values[1])
        width = _integer(values[2]) if len(values) == 3 else 16
        if length < 0 or width <= 0 or width > 64:
            raise gdb.GdbError("LENGTH must be non-negative and WIDTH must be between 1 and 64")
        output = _hexdump(address, mem.bytes(address, length), width)
        if output:
            gdb.write(output + "\n")


class TonyDump(gdb.Command):
    """tony-dump ADDRESS LENGTH FILE [--force] -- save raw inferior memory."""

    def __init__(self):
        super().__init__("tony-dump", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-dump ADDRESS LENGTH FILE [--force]")
        force = values[-1:] == ["--force"]
        if force:
            values.pop()
        if len(values) != 3:
            raise gdb.GdbError("usage: tony-dump ADDRESS LENGTH FILE [--force]")
        address = _integer(values[0])
        length = _integer(values[1])
        if length < 0:
            raise gdb.GdbError("LENGTH must be non-negative")
        path = Path(values[2]).expanduser()
        if path.exists() and not force:
            raise gdb.GdbError(f"refusing to overwrite {path}; add --force if intended")
        try:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(mem.bytes(address, length))
        except OSError as exc:
            raise gdb.GdbError(f"could not write {path}: {exc}") from exc
        _write(f"wrote {length} bytes from 0x{address:08x} to {path}")


class TonySnapshot(gdb.Command):
    """tony-snapshot NAME ADDRESS SIZE [--force] -- capture an in-session raw snapshot."""

    def __init__(self):
        super().__init__("tony-snapshot", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-snapshot NAME ADDRESS SIZE [--force]")
        force = values[-1:] == ["--force"]
        if force:
            values.pop()
        if len(values) != 3:
            raise gdb.GdbError("usage: tony-snapshot NAME ADDRESS SIZE [--force]")
        name = values[0]
        address = _snapshot_address(values[1])
        size = _integer(values[2])
        snapshot = snapshots.capture(name, address, size, overwrite=force)
        _write(f"captured snapshot {name} at 0x{snapshot.address:08x} ({snapshot.size} bytes)")


class TonyDiff(gdb.Command):
    """tony-diff BEFORE AFTER -- show changed raw words and heuristic interpretations."""

    def __init__(self):
        super().__init__("tony-diff", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-diff BEFORE AFTER")
        if len(values) != 2:
            raise gdb.GdbError("usage: tony-diff BEFORE AFTER")
        before, after = values
        entries = snapshots.diff(before, after)
        _write(f"{before} -> {after}: {len(entries)} changed words")
        for line in format_diff(entries):
            _write(line)
        if not entries:
            _write("no changes")


class TonyModules(gdb.Command):
    """tony-modules -- show loaded module ranges from GDB/Wine."""

    def __init__(self):
        super().__init__("tony-modules", gdb.COMMAND_FILES)

    def invoke(self, arg, from_tty):
        if arg.strip():
            raise gdb.GdbError("usage: tony-modules")
        main = gdb.current_progspace().filename
        if main:
            _write(f"main: {main}")
        output = gdb.execute("info sharedlibrary", to_string=True)
        gdb.write(output)


def _set_breakpoint(address: int, temporary: bool = False) -> None:
    breakpoint = TonyBreakpoint(address, temporary=temporary)
    kind = "temporary breakpoint" if temporary else "breakpoint"
    _write(f"set {kind} {breakpoint.number} at 0x{address:08x}")


class TonyBreakpointCommand(gdb.Command):
    """tony-bp ADDRESS [temporary] -- set an address breakpoint."""

    def __init__(self):
        super().__init__("tony-bp", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-bp ADDRESS [temporary]")
        if len(values) not in (1, 2) or (len(values) == 2 and values[1] != "temporary"):
            raise gdb.GdbError("usage: tony-bp ADDRESS [temporary]")
        _set_breakpoint(_integer(values[0]), len(values) == 2)


class TonyAddresses(gdb.Command):
    """tony-thps2 [NAME] -- show known addresses for this THPS2 build."""

    def __init__(self):
        super().__init__("tony-thps2", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        try:
            values = shlex.split(arg)
        except ValueError as exc:
            raise gdb.GdbError(f"invalid arguments: {exc}") from exc
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-thps2 [NAME]")
        if not values:
            _write(f"THPS2 build SHA-256: {THPS2_BUILD_SHA256}")
            for name, (address, description, evidence) in THPS2_ADDRESSES.items():
                _write(f"{name:<16} 0x{address:08x}  {description} [{evidence}]")
            return
        name = values[0]
        try:
            address, description, evidence = THPS2_ADDRESSES[name]
        except KeyError as exc:
            raise gdb.GdbError(f"unknown THPS2 address {name!r}; run tony-thps2") from exc
        _write(f"{name}: 0x{address:08x}  {description} [{evidence}]")


class TonyTHPS2Breakpoint(gdb.Command):
    """tony-bp-thps2 NAME [temporary] -- break at a known THPS2 address."""

    def __init__(self):
        super().__init__("tony-bp-thps2", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-bp-thps2 NAME [temporary]")
        if len(values) not in (1, 2) or (len(values) == 2 and values[1] != "temporary"):
            raise gdb.GdbError("usage: tony-bp-thps2 NAME [temporary]")
        try:
            address = THPS2_ADDRESSES[values[0]][0]
        except KeyError as exc:
            raise gdb.GdbError(f"unknown THPS2 address {values[0]!r}; run tony-thps2") from exc
        _set_breakpoint(address, len(values) == 2)


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
        _write(
            f"skipped movie playback; returning to 0x{return_address:08x} "
            f"(result {result})"
        )


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
            _write("startup movie bypass is already enabled")
            return
        addresses = (0x004E5EC0, 0x004E6590)
        _movie_skip_breakpoints = [TonySkipMovieBreakpoint(address) for address in addresses]
        formatted = ", ".join(f"0x{address:08x}" for address in addresses)
        _write(f"startup movie bypass enabled at {formatted}")


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
        _write(f"forced launch level {original} -> {self.level} ({self.label})")
        return True


class TonyForceLevel(gdb.Command):
    """tony-force-level NAME|INDEX -- replace the next level launch argument."""

    def __init__(self):
        super().__init__("tony-force-level", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-force-level NAME|INDEX")
        if len(values) != 1:
            raise gdb.GdbError("usage: tony-force-level NAME|INDEX")
        value = values[0].casefold()
        if value in THPS2_LEVELS:
            level = THPS2_LEVELS[value]
            label = value
        else:
            level = _integer(values[0])
            label = f"level-{level}"
        if not 0 <= level < 13:
            raise gdb.GdbError("THPS2 level index must be between 0 and 12")
        TonyForceLevelBreakpoint(level, label)
        _write(f"next level launch will use {level} ({label})")


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
        _write(
            f"frontend level override: {self.label} ({self.level}) at "
            f"0x{self.LEVEL_SELECT_ENTRY:08x}"
        )
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
        _write(
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
        _key_loop_breakpoints.append(
            TonyKeyLoopBreakpoint(
                0x1C,
                press_ticks=5,
                release_ticks=15,
                cycles=self.cycles,
            )
        )
        _write(f"frontend level selected: armed Enter cycles {self.cycles}")

    def _arm_summary_enter(self) -> None:
        breakpoint = TonyNetmenuSummaryBreakpoint(self.cycles)
        _runtime_breakpoints.append(breakpoint)
        _write(
            "frontend play: waiting for NETMENU_InitSummary before final Enter "
            f"cycles {self.cycles}"
        )

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
            _runtime_breakpoints.append(override)
            _write(
                f"frontend screen {screen}: real level-select result will use "
                f"{label} ({self.level_index})"
            )
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
        _key_loop_breakpoints.append(
            TonyKeyLoopBreakpoint(
                0x1C,
                press_ticks=5,
                release_ticks=15,
                cycles=self.enter_cycles,
            )
        )
        _write(
            "frontend NETMENU_InitSummary reached: armed final Enter cycles "
            f"{self.enter_cycles}"
        )


class TonyFrontendPlay(gdb.Command):
    """tony-frontend-play [ENTER_CYCLES] [LEVEL] -- force main-menu PLAY_GAME."""

    def __init__(self):
        super().__init__("tony-frontend-play", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        del from_tty
        values = _argv(arg, "tony-frontend-play [ENTER_CYCLES] [LEVEL]") if arg.strip() else []
        if len(values) > 2:
            raise gdb.GdbError("usage: tony-frontend-play [ENTER_CYCLES] [LEVEL]")
        try:
            enter_cycles = _integer(values[0]) if values else 0
            level_index = _integer(values[1]) if len(values) > 1 else None
            breakpoint = TonyFrontendPlayBreakpoint(enter_cycles, level_index)
        except (ValueError, gdb.GdbError) as exc:
            raise gdb.GdbError(str(exc)) from exc
        _runtime_breakpoints.append(breakpoint)
        _write(
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
                _key_loop_breakpoints.append(
                    TonyKeyLoopBreakpoint(
                        0x1C,
                        press_ticks=5,
                        release_ticks=15,
                        cycles=self.followup_enter_cycles,
                    )
                )
        _write(
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
        values = _argv(arg, "tony-frontend-confirm [SCAN] [COUNT] [ENTER_CYCLES]") if arg.strip() else []
        if len(values) > 3:
            raise gdb.GdbError("usage: tony-frontend-confirm [SCAN] [COUNT] [ENTER_CYCLES]")
        try:
            scan_code = _integer(values[0]) if values else TonyFrontendConfirmBreakpoint.CONFIRM_SCAN_CODE
            count = _integer(values[1]) if len(values) > 1 else 1
            enter_cycles = _integer(values[2]) if len(values) > 2 else 0
            breakpoint = TonyFrontendConfirmBreakpoint(scan_code, count, enter_cycles)
        except (ValueError, gdb.GdbError) as exc:
            raise gdb.GdbError(str(exc)) from exc
        _runtime_breakpoints.append(breakpoint)
        _write(
            "frontend confirmation armed at "
            f"0x{breakpoint.KEY_STATE_HELPER:08x}; "
            f"scan 0x{breakpoint.scan_code:02x}, count {count}, "
            f"follow-up Enter cycles {enter_cycles}"
        )


class TonyPlayerSampleBreakpoint(CountingBreakpoint):
    """Collect raw player-object snapshots at level-loop entry."""

    def __init__(self, count: int, path: Path, writer=None):
        super().__init__(THPS2_ADDRESSES["frame_tick"][0], count=count, internal=True)
        self.path = path
        self.sample = 0
        self.writer = writer

    def on_count(self, ctx):
        player = mem.u32(GLOBALS["Player"])
        level = mem.u32(GLOBALS["CurrentLevel"])
        if player == 0 or level > 12:
            return False
        registers = {name: ctx.register(name) for name in ("eax", "ecx", "edx", "ebx", "ebp", "esi", "edi")}
        record = {
            "sample": self.sample,
            "eip": ctx.eip,
            "frame": ctx.frame,
            "level": level,
            "player": player,
            "owner_header": player - 0x30 if player >= 0x30 else None,
            "registers": registers,
            "owner_bytes": mem.bytes(player - 0x30, 0x30).hex() if player >= 0x30 else None,
            "player_bytes": mem.bytes(player, 0x200).hex() if player else None,
        }
        try:
            with self.path.open("a", encoding="utf-8") as stream:
                stream.write(json.dumps(record, sort_keys=True) + "\n")
        except OSError as exc:
            raise gdb.GdbError(f"could not write player sample {self.path}: {exc}") from exc
        if self.writer is not None:
            self.writer.event({"type": "player_sample", **record})
        self.sample += 1
        return True

    def on_complete(self):
        _write(f"player sampling complete: {self.sample} samples -> {self.path}")


class TonyPlayerSample(gdb.Command):
    """tony-player-sample COUNT FILE [--force] -- collect raw frame-tick snapshots."""

    def __init__(self):
        super().__init__("tony-player-sample", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-player-sample COUNT FILE [--force]")
        force = values[-1:] == ["--force"]
        if force:
            values.pop()
        if len(values) != 2:
            raise gdb.GdbError("usage: tony-player-sample COUNT FILE [--force]")
        count = _integer(values[0])
        if count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        path = Path(values[1]).expanduser()
        if path.exists() and not force:
            raise gdb.GdbError(f"refusing to overwrite {path}; add --force if intended")
        try:
            path.parent.mkdir(parents=True, exist_ok=True)
            if force:
                path.unlink(missing_ok=True)
        except OSError as exc:
            raise gdb.GdbError(f"could not prepare player sample {path}: {exc}") from exc
        TonyPlayerSampleBreakpoint(count, path, writer=_trace_writer)
        _write(f"player sampling armed for {count} level-loop hits -> {path}")


class TonyInputSampleBreakpoint(CountingBreakpoint):
    """Collect action and raw-keyboard state after the input update."""

    def __init__(self, count: int, path: Path, writer=None):
        super().__init__(THPS2_ADDRESSES["gameplay_update"][0], count=count, internal=True)
        self.path = path
        self.sample = 0
        self.writer = writer

    def on_count(self, ctx):
        level = mem.u32(GLOBALS["CurrentLevel"])
        action_word = mem.u16(GLOBALS["ActionMask"])
        action_words = mem.u32(GLOBALS["ActionMask"])
        keyboard_state = mem.bytes(GLOBALS["KeyboardState"], 0x100)
        action_states = {
            name: _action_state(address) for name, (address, _bit) in ACTION_STATE_RECORDS.items()
        }
        record = {
            "sample": self.sample,
            "eip": ctx.eip,
            "frame": ctx.frame,
            "level": level,
            "action_mask": action_word,
            "action_words": action_words,
            "held_keys": [code for code, value in enumerate(keyboard_state) if value & 0x80],
            "keyboard_state": keyboard_state.hex(),
            "action_states": action_states,
        }
        try:
            with self.path.open("a", encoding="utf-8") as stream:
                stream.write(json.dumps(record, sort_keys=True) + "\n")
        except OSError as exc:
            raise gdb.GdbError(f"could not write input sample {self.path}: {exc}") from exc
        if self.writer is not None:
            self.writer.event({"type": "input_sample", **record})
        self.sample += 1
        return True

    def on_complete(self):
        _write(f"input sampling complete: {self.sample} samples -> {self.path}")


class TonyInputSample(gdb.Command):
    """tony-input-sample COUNT FILE [--force] -- collect post-poll input state."""

    def __init__(self):
        super().__init__("tony-input-sample", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-input-sample COUNT FILE [--force]")
        force = values[-1:] == ["--force"]
        if force:
            values.pop()
        if len(values) != 2:
            raise gdb.GdbError("usage: tony-input-sample COUNT FILE [--force]")
        count = _integer(values[0])
        if count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        path = Path(values[1]).expanduser()
        if path.exists() and not force:
            raise gdb.GdbError(f"refusing to overwrite {path}; add --force if intended")
        try:
            path.parent.mkdir(parents=True, exist_ok=True)
            if force:
                path.unlink(missing_ok=True)
        except OSError as exc:
            raise gdb.GdbError(f"could not prepare input sample {path}: {exc}") from exc
        TonyInputSampleBreakpoint(count, path, writer=_trace_writer)
        _write(f"input sampling armed for {count} post-poll hits -> {path}")


class TonyActionEdgeBreakpoint(TonyBreakpoint):
    """Inject one named action after the live mask is built.

    The delay is measured in the shared physics frames, not breakpoint hits.
    That keeps a requested edge out of shell/level-loading input updates when
    the breakpoint is armed before the playable loop exists.
    """

    ACTION_MASK_ADDRESS = GLOBALS["ActionMask"]
    AFTER_ACTION_MASK_BUILD = 0x00489A15

    def __init__(self, action: str, delay=0, hold=1, writer=None):
        try:
            _address, action_mask = ACTION_STATE_RECORDS[action]
        except KeyError as exc:
            raise gdb.GdbError(f"unknown action {action!r}") from exc
        super().__init__(self.AFTER_ACTION_MASK_BUILD, internal=True)
        self.action = action
        self.action_mask = action_mask
        self.delay = delay
        self.hold = hold
        self.injected = False
        self.writer = writer

    def on_hit(self, ctx):
        if ctx.frame < self.delay:
            return
        if self.hold <= 0:
            self.enabled = False
            return
        # 0x00489a15 is the instruction immediately after the poll/build
        # call and immediately before the action-state records consume the
        # returned mask.  This produces one held action frame; the next live
        # poll supplies the release/zero mask normally.
        mem.write(self.ACTION_MASK_ADDRESS, self.action_mask.to_bytes(2, "little"))
        record = {
            "type": f"{self.action}_edge_injection" if not self.injected else f"{self.action}_hold_update",
            "frame": ctx.frame,
            "eip": f"0x{ctx.eip:08x}",
            "action_mask_address": f"0x{self.ACTION_MASK_ADDRESS:08x}",
            "action": self.action,
            "action_mask": self.action_mask,
            "action_state_address": f"0x{ACTION_STATE_BASE:08x}",
            "action_state_bit": self.action_mask,
            "hold_updates": self.hold,
        }
        if self.writer is None:
            self.emit(record)
        else:
            self.writer.event(record)
        self.hold -= 1
        self.injected = True
        if self.hold <= 0:
            self.enabled = False


class TonyActionEdge(gdb.Command):
    """tony-action-edge ACTION [DELAY [HOLD]] -- inject a bounded action."""

    def __init__(self):
        super().__init__("tony-action-edge", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-action-edge ACTION [DELAY [HOLD]]")
        if len(values) not in (1, 2, 3):
            raise gdb.GdbError("usage: tony-action-edge ACTION [DELAY [HOLD]]")
        action = values[0].casefold()
        delay = _integer(values[1]) if len(values) > 1 else 0
        hold = _integer(values[2]) if len(values) > 2 else 1
        if delay < 0:
            raise gdb.GdbError("DELAY must not be negative")
        if hold <= 0:
            raise gdb.GdbError("HOLD must be positive")
        breakpoint = TonyActionEdgeBreakpoint(action, delay=delay, hold=hold, writer=_trace_writer)
        _runtime_breakpoints.append(breakpoint)
        _write(
            f"one-shot {action.upper()} edge armed at 0x{breakpoint.address:08x}; "
            f"mask 0x{breakpoint.action_mask:04x}; "
            f"delay {delay} physics frames, hold {hold} input updates"
        )


class TonyJumpEdge(gdb.Command):
    """Compatibility alias for tony-action-edge jump [DELAY [HOLD]]."""

    def __init__(self):
        super().__init__("tony-jump-edge", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        try:
            values = shlex.split(arg)
        except ValueError as exc:
            raise gdb.GdbError(f"invalid arguments: {exc}") from exc
        if len(values) > 2:
            raise gdb.GdbError("usage: tony-jump-edge [DELAY [HOLD]]")
        delay = _integer(values[0]) if values else 0
        hold = _integer(values[1]) if len(values) > 1 else 1
        if delay < 0:
            raise gdb.GdbError("DELAY must not be negative")
        if hold <= 0:
            raise gdb.GdbError("HOLD must be positive")
        breakpoint = TonyActionEdgeBreakpoint("jump", delay=delay, hold=hold, writer=_trace_writer)
        _runtime_breakpoints.append(breakpoint)
        _write(
            f"one-shot JUMP edge armed at 0x{breakpoint.address:08x}; "
            f"mask 0x{breakpoint.action_mask:04x}; delay {delay} physics frames, "
            f"hold {hold} input updates"
        )
class TonyKeyLoopBreakpoint(CountingBreakpoint):
    """Drive one DirectInput scan-code byte with a repeatable press/release loop."""

    def __init__(
        self,
        scan_code: int,
        press_ticks: int,
        release_ticks: int,
        cycles: int,
        on_complete=None,
    ):
        period = press_ticks + release_ticks
        super().__init__(THPS2_ADDRESSES["input_state"][0], count=period * cycles, internal=True)
        self.key_address = GLOBALS["KeyboardState"] + scan_code
        self.scan_code = scan_code
        self.press_ticks = press_ticks
        self.release_ticks = release_ticks
        self.phase = 0
        self.on_complete_callback = on_complete

    def on_count(self, ctx):
        del ctx
        value = 0x80 if self.phase < self.press_ticks else 0
        mem.write(self.key_address, bytes((value,)))
        self.phase = (self.phase + 1) % (self.press_ticks + self.release_ticks)
        if self.remaining == 1:
            mem.write(self.key_address, b"\0")
        return True

    def on_complete(self):
        mem.write(self.key_address, b"\0")
        _write(f"keyboard loop complete for scan code {self.scan_code}")
        if self.on_complete_callback is not None:
            self.on_complete_callback()


class TonyKeyLoop(gdb.Command):
    """tony-key-loop SCAN PRESS RELEASE CYCLES -- synthesize repeated key edges."""

    def __init__(self):
        super().__init__("tony-key-loop", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-key-loop SCAN PRESS RELEASE CYCLES")
        if len(values) != 4:
            raise gdb.GdbError("usage: tony-key-loop SCAN PRESS RELEASE CYCLES")
        scan_code, press_ticks, release_ticks, cycles = (_integer(value) for value in values)
        if not 0 <= scan_code < 0x100:
            raise gdb.GdbError("SCAN must be a DirectInput scan code between 0 and 255")
        if press_ticks <= 0 or release_ticks <= 0 or cycles <= 0:
            raise gdb.GdbError("PRESS, RELEASE, and CYCLES must be positive")
        breakpoint = TonyKeyLoopBreakpoint(scan_code, press_ticks, release_ticks, cycles)
        _key_loop_breakpoints.append(breakpoint)
        _write(
            f"keyboard loop armed for scan code {scan_code}: "
            f"{press_ticks} pressed / {release_ticks} released ticks, {cycles} cycles"
        )


class TonyKeyClear(gdb.Command):
    """tony-key-clear [SCAN] -- disable synthesized key loops and release keys."""

    def __init__(self):
        super().__init__("tony-key-clear", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-key-clear [SCAN]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-key-clear [SCAN]")
        scan_code = _integer(values[0]) if values else None
        if scan_code is not None and not 0 <= scan_code < 0x100:
            raise gdb.GdbError("SCAN must be a DirectInput scan code between 0 and 255")
        cleared = 0
        for breakpoint in _key_loop_breakpoints:
            if scan_code is not None and breakpoint.scan_code != scan_code:
                continue
            breakpoint.enabled = False
            mem.write(breakpoint.key_address, b"\0")
            cleared += 1
        suffix = "" if scan_code is None else f" for scan code {scan_code}"
        _write(f"disabled {cleared} synthesized keyboard loop(s){suffix}")


class TonyAnimationSampleBreakpoint(CountingBreakpoint):
    """Collect the live player's animation cursor before its object update."""

    def __init__(self, count: int, path: Path, writer=None):
        super().__init__(THPS2_ADDRESSES["animation_dispatch"][0], count=count, internal=True)
        self.path = path
        self.sample = 0
        self.writer = writer

    def on_count(self, ctx):
        player = mem.u32(GLOBALS["Player"])
        if player == 0 or ctx.this_ptr() != player:
            return False
        record = {
            "sample": self.sample,
            "eip": ctx.eip,
            "frame": ctx.frame,
            "player": f"0x{player:08x}",
            "action_mask": mem.u16(GLOBALS["ActionMask"]),
            "animation": {
                "id": mem.u16(player + 0xF6),
                "frame": mem.s16(player + 0xF4),
                "fraction": mem.u16(player + 0x104),
                "rate": mem.u32(player + 0x108),
                "mode": mem.u8(player + 0xF8),
                "direction": mem.s8(player + 0x100),
                "endpoint": mem.s8(player + 0x101),
                "alternate_endpoint": mem.s8(player + 0x102),
                "finished": mem.u8(player + 0x107),
                "old_frame": mem.s16(player + 0x10C),
                "new_frame": mem.s16(player + 0x10E),
                "old_anim": mem.u16(player + 0x110),
                "old_anim_dir": mem.s8(player + 0x112),
            },
        }
        try:
            with self.path.open("a", encoding="utf-8") as stream:
                stream.write(json.dumps(record, sort_keys=True) + "\n")
        except OSError as exc:
            raise gdb.GdbError(f"could not write animation sample {self.path}: {exc}") from exc
        if self.writer is not None:
            self.writer.event({"type": "animation_sample", **record})
        self.sample += 1
        return True

    def on_complete(self):
        self.should_stop = True
        _write(f"animation sampling complete: {self.sample} samples -> {self.path}")


class TonyAnimationSample(gdb.Command):
    """tony-animation-sample COUNT FILE [--force] -- sample the player cursor."""

    def __init__(self):
        super().__init__("tony-animation-sample", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-animation-sample COUNT FILE [--force]")
        force = values[-1:] == ["--force"]
        if force:
            values.pop()
        if len(values) != 2:
            raise gdb.GdbError("usage: tony-animation-sample COUNT FILE [--force]")
        count = _integer(values[0])
        if count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        path = Path(values[1]).expanduser()
        if path.exists() and not force:
            raise gdb.GdbError(f"refusing to overwrite {path}; add --force if intended")
        try:
            path.parent.mkdir(parents=True, exist_ok=True)
            if force:
                path.unlink(missing_ok=True)
        except OSError as exc:
            raise gdb.GdbError(f"could not prepare animation sample {path}: {exc}") from exc
        breakpoint = TonyAnimationSampleBreakpoint(count, path, writer=_trace_writer)
        _runtime_breakpoints.append(breakpoint)
        _write(f"animation sampling armed for {count} player updates -> {path}")


class TonyAnimationRequestBreakpoint(CountingBreakpoint):
    """Collect calls into CSuper::RunAnim for the generated player."""

    def __init__(self, count: int | None, path: Path | None, writer=None):
        super().__init__(THPS2_ADDRESSES["animation_run"][0], count=count, internal=True)
        self.path = path
        self.sample = 0
        self.writer = writer

    def on_count(self, ctx):
        player = mem.u32(GLOBALS["Player"])
        if player == 0 or ctx.this_ptr() != player:
            return False
        wrapper_return = ctx.caller()
        # RunAnim is reached through one of the four fixed-arity request
        # wrappers.  Recover the wrapper's caller from below its saved
        # registers/arguments so a live request can be tied to a selector.
        outer_stack_offset = {
            0x00490414: 0x18,
            0x00490447: 0x18,
            0x0049047A: 0x18,
            0x004904AD: 0x18,
        }.get(wrapper_return)
        record = {
            "sample": self.sample,
            "eip": ctx.eip,
            "frame": ctx.frame,
            "player": f"0x{player:08x}",
            "caller": f"0x{wrapper_return:08x}",
            "outer_caller": (
                f"0x{mem.u32(ctx.esp + outer_stack_offset):08x}"
                if outer_stack_offset is not None else None
            ),
            "action_mask": mem.u16(GLOBALS["ActionMask"]),
            "request": {
                "animation": ctx.arg(0),
                "start": ctx.arg(1),
                "end": ctx.arg(2),
                "alternate_endpoint": ctx.arg(3),
            },
            "before": {
                "animation": mem.u16(player + 0xF6),
                "frame": mem.s16(player + 0xF4),
                "fraction": mem.u16(player + 0x104),
                "rate": mem.u32(player + 0x108),
                "mode": mem.u8(player + 0xF8),
                "direction": mem.s8(player + 0x100),
                "endpoint": mem.s8(player + 0x101),
                "alternate_endpoint": mem.s8(player + 0x102),
                "finished": mem.u8(player + 0x107),
                "old_frame": mem.s16(player + 0x10C),
                "new_frame": mem.s16(player + 0x10E),
                "old_anim": mem.u16(player + 0x110),
                "old_anim_dir": mem.s8(player + 0x112),
            },
        }
        if self.path is not None:
            try:
                with self.path.open("a", encoding="utf-8") as stream:
                    stream.write(json.dumps(record, sort_keys=True) + "\n")
            except OSError as exc:
                raise gdb.GdbError(
                    f"could not write animation request sample {self.path}: {exc}"
                ) from exc
        if self.writer is not None:
            self.writer.event({"type": "animation_request", **record})
        self.sample += 1
        return True

    def on_complete(self):
        self.should_stop = True
        _write(f"animation request sampling complete: {self.sample} samples -> {self.path}")


class TonyAnimationRequestSample(gdb.Command):
    """tony-animation-request-sample COUNT FILE [--force] -- sample RunAnim calls."""

    def __init__(self):
        super().__init__("tony-animation-request-sample", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-animation-request-sample COUNT FILE [--force]")
        force = values[-1:] == ["--force"]
        if force:
            values.pop()
        if len(values) != 2:
            raise gdb.GdbError("usage: tony-animation-request-sample COUNT FILE [--force]")
        count = _integer(values[0])
        if count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        path = Path(values[1]).expanduser()
        if path.exists() and not force:
            raise gdb.GdbError(f"refusing to overwrite {path}; add --force if intended")
        try:
            path.parent.mkdir(parents=True, exist_ok=True)
            if force:
                path.unlink(missing_ok=True)
        except OSError as exc:
            raise gdb.GdbError(f"could not prepare animation request sample {path}: {exc}") from exc
        breakpoint = TonyAnimationRequestBreakpoint(count, path, writer=_trace_writer)
        _runtime_breakpoints.append(breakpoint)
        _write(f"animation request sampling armed for {count} player requests -> {path}")


class TonyAnimationSelectorBreakpoint(CountingBreakpoint):
    """Collect the input/state gate seen by the steering animation selector."""

    def __init__(self, count: int, path: Path, writer=None):
        super().__init__(THPS2_ADDRESSES["animation_selector"][0], count=count, internal=True)
        self.path = path
        self.sample = 0
        self.writer = writer

    def on_count(self, ctx):
        player = mem.u32(GLOBALS["Player"])
        if player == 0 or ctx.this_ptr() != player:
            return False
        record = {
            "sample": self.sample,
            "eip": ctx.eip,
            "frame": ctx.frame,
            "player": f"0x{player:08x}",
            "caller": f"0x{ctx.caller():08x}",
            "action_mask": mem.u16(GLOBALS["ActionMask"]),
            "selector_state": {
                "current_animation": mem.u16(player + 0xF6),
                "current_frame": mem.s16(player + 0xF4),
                "steering_value": mem.s32(player + 0x3148),
                "transition_gate": mem.u32(player + 0x2DD8),
                "object_flags": mem.u32(player + 0xD8),
                "speed_branch": mem.s8(player + 0x31A2),
                "physics_state": mem.u32(player + 0x30B8),
            },
        }
        try:
            with self.path.open("a", encoding="utf-8") as stream:
                stream.write(json.dumps(record, sort_keys=True) + "\n")
        except OSError as exc:
            raise gdb.GdbError(f"could not write animation selector sample {self.path}: {exc}") from exc
        if self.writer is not None:
            self.writer.event({"type": "animation_selector", **record})
        self.sample += 1
        return True

    def on_complete(self):
        self.should_stop = True
        _write(f"animation selector sampling complete: {self.sample} samples -> {self.path}")


class TonyAnimationSelectorSample(gdb.Command):
    """tony-animation-selector-sample COUNT FILE [--force] -- sample steering selector state."""

    def __init__(self):
        super().__init__("tony-animation-selector-sample", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-animation-selector-sample COUNT FILE [--force]")
        force = values[-1:] == ["--force"]
        if force:
            values.pop()
        if len(values) != 2:
            raise gdb.GdbError("usage: tony-animation-selector-sample COUNT FILE [--force]")
        count = _integer(values[0])
        if count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        path = Path(values[1]).expanduser()
        if path.exists() and not force:
            raise gdb.GdbError(f"refusing to overwrite {path}; add --force if intended")
        try:
            path.parent.mkdir(parents=True, exist_ok=True)
            if force:
                path.unlink(missing_ok=True)
        except OSError as exc:
            raise gdb.GdbError(f"could not prepare animation selector sample {path}: {exc}") from exc
        breakpoint = TonyAnimationSelectorBreakpoint(count, path, writer=_trace_writer)
        _runtime_breakpoints.append(breakpoint)
        _write(f"animation selector sampling armed for {count} player hits -> {path}")


class TonyActionSequence(gdb.Command):
    """tony-action-sequence MASK... -- inject raw action masks at publish."""

    def __init__(self):
        super().__init__("tony-action-sequence", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        values = [value for value in arg.replace(",", " ").split() if value]
        if not values:
            raise gdb.GdbError(
                "usage: tony-action-sequence MASK [MASK ...]"
            )
        masks = [_integer(value) for value in values]
        probe = ActionMaskSequenceProbe(masks, writer=_trace_writer)
        _runtime_breakpoints.append(probe)
        _write(
            f"action mask sequence armed for {len(masks)} publishes "
            f"at 0x{probe.address:08x}"
        )


def _watch_limit(values: list[str], usage: str, *, default_limit: int | None) -> tuple[list[str], int | None]:
    limit = default_limit
    if "--limit" in values:
        if values.count("--limit") != 1:
            raise gdb.GdbError(f"usage: {usage}")
        index = values.index("--limit")
        if index + 1 >= len(values):
            raise gdb.GdbError(f"usage: {usage}")
        limit = _integer(values[index + 1])
        values[index:index + 2] = []
        if limit <= 0:
            raise gdb.GdbError("LIMIT must be positive")
    return values, limit


def _watch_arguments(arg: str, usage: str, *, default_limit: int | None) -> tuple[int, str, int, int | None]:
    values, limit = _watch_limit(_argv(arg, usage), usage, default_limit=default_limit)
    if len(values) not in (1, 2):
        raise gdb.GdbError(f"usage: {usage}")
    address, label = _watch_address(values[0])
    size = _integer(values[1]) if len(values) == 2 else 4
    if size not in (1, 2, 4):
        raise gdb.GdbError("SIZE must be 1, 2, or 4 bytes")
    return address, label, size, limit


def _arm_resolved_watchpoint(
    address: int,
    label: str,
    size: int,
    *,
    once: bool,
    limit: int | None,
):
    watchpoint = watchpoints.arm(
        address,
        size=size,
        label=label,
        once=once,
        limit=limit,
        writer=_trace_writer,
    )
    mode = "one-shot " if once else ""
    number = getattr(watchpoint, "number", "?")
    limit_text = "" if once else f"; limit {limit} events"
    _write(
        f"{mode}write watchpoint {number} armed at {label}"
        f" (0x{address:08x}, {size} bytes{limit_text})"
    )
    return watchpoint


def _arm_watchpoint(arg: str, *, once: bool, command: str):
    address, label, size, limit = _watch_arguments(
        arg,
        f"{command} ADDRESS [SIZE] [--limit COUNT]",
        default_limit=None if once else _WATCH_DEFAULT_LIMIT,
    )
    return _arm_resolved_watchpoint(address, label, size, once=once, limit=limit)


class TonyWatch(gdb.Command):
    """tony-watch ADDRESS [SIZE] [--limit COUNT] -- bounded write logging."""

    def __init__(self):
        super().__init__("tony-watch", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        _arm_watchpoint(arg, once=False, command="tony-watch")


class TonyWatchOnce(gdb.Command):
    """tony-watch-once ADDRESS [SIZE] -- record the next write and continue."""

    def __init__(self):
        super().__init__("tony-watch-once", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        _arm_watchpoint(arg, once=True, command="tony-watch-once")


class TonyWatchBatch(gdb.Command):
    """tony-watch-batch ADDRESS... [--limit COUNT] -- arm a safe four-watch group."""

    def __init__(self):
        super().__init__("tony-watch-batch", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        usage = "tony-watch-batch ADDRESS... [--limit COUNT]"
        values, limit = _watch_limit(_argv(arg, usage), usage, default_limit=_WATCH_DEFAULT_LIMIT)
        if not values:
            raise gdb.GdbError(f"usage: {usage}")
        available = watchpoints.available()
        if len(values) > available:
            raise gdb.GdbError(
                f"watch batch needs {len(values)} hardware slots, but only {available} are available; "
                "split the experiment into groups of four"
            )
        for value in values:
            address, label = _watch_address(value)
            _arm_resolved_watchpoint(address, label, 4, once=False, limit=limit)


class TonyWatchLog(gdb.Command):
    """tony-watch-log [ADDRESS [SIZE] [--limit COUNT]] -- list or arm bounded watches."""

    def __init__(self):
        super().__init__("tony-watch-log", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        if arg.strip():
            _arm_watchpoint(arg, once=False, command="tony-watch-log")
            return
        active = watchpoints.active()
        if not active:
            _write("no active OpenTony watchpoints")
            return
        _write(
            f"{len(active)} active OpenTony watchpoint(s); "
            f"{watchpoints.available()} hardware slot(s) available"
        )
        for watchpoint in active:
            number = getattr(watchpoint, "number", "?")
            mode = "one-shot" if watchpoint.once else f"limit {watchpoint.limit}"
            _write(
                f"  {number}: {watchpoint.label} @ 0x{watchpoint.address:08x}"
                f" ({watchpoint.size} bytes; {mode}{'; latched' if watchpoint.latched else ''})"
            )


class TonyWatchClear(gdb.Command):
    """tony-watch-clear -- disable all managed hardware watchpoints while stopped."""

    def __init__(self):
        super().__init__("tony-watch-clear", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        if arg.strip():
            raise gdb.GdbError("usage: tony-watch-clear")
        active = watchpoints.active()
        for watchpoint in active:
            watchpoint.enabled = False
        _write(f"disabled {len(active)} OpenTony hardware watchpoint(s)")


_runtime_breakpoints = []


class TonyTraceOpen(gdb.Command):
    """tony-trace-open FILE EXPERIMENT [--force] -- start a JSONL runtime trace."""

    def __init__(self):
        super().__init__("tony-trace-open", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        global _trace_writer
        values = _argv(arg, "tony-trace-open FILE EXPERIMENT [--force]")
        force = values[-1:] == ["--force"]
        if force:
            values.pop()
        if len(values) != 2:
            raise gdb.GdbError("usage: tony-trace-open FILE EXPERIMENT [--force]")
        if _trace_writer is not None:
            raise gdb.GdbError("a runtime trace is already open; run tony-trace-close first")
        writer = JsonlWriter(values[0], values[1], overwrite=force)
        try:
            writer.open()
        except OSError as exc:
            raise gdb.GdbError(str(exc)) from exc
        _trace_writer = writer
        _write(f"runtime trace opened: {writer.path}")


class TonyTraceClose(gdb.Command):
    """tony-trace-close -- write the trace footer and close the active writer."""

    def __init__(self):
        super().__init__("tony-trace-close", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        global _trace_writer
        if arg.strip():
            raise gdb.GdbError("usage: tony-trace-close")
        if _trace_writer is None:
            _write("no runtime trace is open")
            return
        writer = _trace_writer
        for breakpoint in _runtime_breakpoints:
            if getattr(breakpoint, "writer", None) is writer:
                breakpoint.enabled = False
        watchpoints.disable_writer(writer)
        try:
            writer.close()
        except OSError as exc:
            raise gdb.GdbError(str(exc)) from exc
        _trace_writer = None
        _write(f"runtime trace closed: {writer.path}")


class TonyFrameClock(gdb.Command):
    """tony-frame-clock FUNCTION -- tick the shared clock at a chosen function."""

    def __init__(self):
        super().__init__("tony-frame-clock", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-frame-clock FUNCTION")
        if len(values) != 1:
            raise gdb.GdbError("usage: tony-frame-clock FUNCTION")
        frame_clock.reset()
        try:
            breakpoint = FrameBreakpoint(
                values[0], internal=True, writer=_trace_writer)
        except KeyError as exc:
            raise gdb.GdbError(f"unknown function {values[0]!r}; run tony-thps2") from exc
        _runtime_breakpoints.append(breakpoint)
        _write(f"frame clock armed at {values[0]} (frame starts at 0)")


class TonyPhysicsProbe(gdb.Command):
    """tony-physics-probe [COUNT] -- emit conservative dispatcher observations."""

    def __init__(self):
        super().__init__("tony-physics-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        try:
            values = shlex.split(arg)
        except ValueError as exc:
            raise gdb.GdbError(f"invalid arguments: {exc}") from exc
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-physics-probe [COUNT]")
        count = _integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = PhysicsProbe(count, writer=_trace_writer)
        _runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} observations"
        _write(f"physics probe armed {limit} at 0x{probe.address:08x}")


class TonyForcePhysicsState(gdb.Command):
    """tony-force-physics-state STATE -- inject one state before dispatch."""

    def __init__(self):
        super().__init__("tony-force-physics-state", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-force-physics-state STATE")
        if len(values) != 1:
            raise gdb.GdbError("usage: tony-force-physics-state STATE")
        state = _integer(values[0])
        if state < 0 or state > 8:
            raise gdb.GdbError("STATE must be between 0 and 8")
        probe = SyntheticPhysicsStateForceProbe(state, writer=_trace_writer)
        _runtime_breakpoints.append(probe)
        _write(
            "synthetic physics state force armed for one live grounded "
            f"dispatcher entry: 0 -> {state}"
        )


class TonyCameraProbe(gdb.Command):
    """tony-camera-probe [COUNT] -- sample the per-frame camera update."""

    def __init__(self):
        super().__init__("tony-camera-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-camera-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-camera-probe [COUNT]")
        count = _integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = CameraProbe(count, writer=_trace_writer)
        _runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} observations"
        _write(f"camera probe armed {limit} at 0x{probe.address:08x}")


class TonyCameraForceMode(gdb.Command):
    """tony-camera-force-mode MODE [HOLD] -- force a raw camera mode briefly."""

    def __init__(self):
        super().__init__("tony-camera-force-mode", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-camera-force-mode MODE [HOLD]")
        if len(values) > 2:
            raise gdb.GdbError("usage: tony-camera-force-mode MODE [HOLD]")
        mode = _integer(values[0])
        hold_updates = _integer(values[1]) if len(values) == 2 else 1
        if not 1 <= mode <= 25:
            raise gdb.GdbError("MODE must be between 1 and 25")
        if hold_updates <= 0:
            raise gdb.GdbError("HOLD must be positive")
        probe = CameraModeOverrideProbe(
            mode, hold_updates, writer=_trace_writer)
        _runtime_breakpoints.append(probe)
        _write(
            f"camera mode {mode} will be written for {hold_updates} "
            f"update(s), then mode 1 will be restored"
        )


class TonyCameraViewportProbe(gdb.Command):
    """tony-camera-viewport-probe [COUNT] [AFTER_FRAME] -- exercise raw viewport controls."""

    def __init__(self):
        super().__init__("tony-camera-viewport-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-camera-viewport-probe [COUNT] [AFTER_FRAME]") if arg.strip() else []
        if len(values) > 2:
            raise gdb.GdbError("usage: tony-camera-viewport-probe [COUNT] [AFTER_FRAME]")
        count = _integer(values[0]) if values else 8
        start_frame = _integer(values[1]) if len(values) == 2 else 0
        if count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        if start_frame < 0:
            raise gdb.GdbError("AFTER_FRAME must be non-negative")
        probe = CameraViewportControlProbe(
            count, writer=_trace_writer, start_frame=start_frame)
        _runtime_breakpoints.append(probe)
        _runtime_breakpoints.append(probe.restore_breakpoint)
        _write(
            f"camera viewport-control probe armed for {count} observations "
            f"after frame {start_frame} at 0x{probe.address:08x}"
        )


class TonyCameraTimingProbe(gdb.Command):
    """tony-camera-timing-probe [COUNT] -- sample the Q8 camera-rate producer."""

    def __init__(self):
        super().__init__("tony-camera-timing-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-camera-timing-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-camera-timing-probe [COUNT]")
        count = _integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = CameraTimingProbe(count, writer=_trace_writer)
        _runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} observations"
        _write(f"camera timing probe armed {limit} at 0x{probe.address:08x}")


class TonyCameraPointSelectProbe(gdb.Command):
    """tony-camera-point-probe [COUNT] -- sample point/mode producer input."""

    def __init__(self):
        super().__init__("tony-camera-point-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-camera-point-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-camera-point-probe [COUNT]")
        count = _integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = CameraPointSelectProbe(count, writer=_trace_writer)
        _runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} observations"
        _write(f"camera point-select probe armed {limit} at 0x{probe.address:08x}")


class TonyCameraPointStateProbe(gdb.Command):
    """tony-camera-point-state-probe [COUNT] -- sample post-selector state."""

    def __init__(self):
        super().__init__("tony-camera-point-state-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-camera-point-state-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-camera-point-state-probe [COUNT]")
        count = _integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = CameraPointStateProbe(count, writer=_trace_writer)
        _runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} observations"
        _write(f"camera point-state probe armed {limit} at 0x{probe.address:08x}")


class TonyCameraEffectsProbe(gdb.Command):
    """tony-camera-effects-probe [COUNT] -- sample the effect producer boundary."""

    def __init__(self):
        super().__init__("tony-camera-effects-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-camera-effects-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-camera-effects-probe [COUNT]")
        count = _integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = CameraEffectProbe(count, writer=_trace_writer)
        _runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} observations"
        _write(f"camera effects probe armed {limit} at 0x{probe.address:08x}")


class TonyViewProjectionProbe(gdb.Command):
    """tony-view-probe [COUNT] -- sample raw view/projection preparation state."""

    def __init__(self):
        super().__init__("tony-view-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-view-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-view-probe [COUNT]")
        count = _integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = ViewProjectionProbe(count, writer=_trace_writer)
        _runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} observations"
        _write(f"view projection probe armed {limit} at 0x{probe.address:08x}")
class TonyMovementPhysicsProbe(gdb.Command):
    """tony-movement-physics-probe [COUNT] -- log action/velocity handoff."""

    def __init__(self):
        super().__init__("tony-movement-physics-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-movement-physics-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-movement-physics-probe [COUNT]")
        count = _integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = MovementPhysicsProbe(count, writer=_trace_writer)
        _runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} observations"
        _write(f"movement physics probe armed {limit} at 0x{probe.address:08x}")


class TonyGroundMotionProbe(gdb.Command):
    """tony-ground-motion-probe [COUNT] -- log B010 producer inputs."""

    def __init__(self):
        super().__init__("tony-ground-motion-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-ground-motion-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-ground-motion-probe [COUNT]")
        count = _integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = GroundMotionProducerProbe(count, writer=_trace_writer)
        _runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} observations"
        _write(f"ground-motion producer probe armed {limit} at 0x{probe.address:08x}")


class TonyGroundMotionWriters(gdb.Command):
    """tony-ground-motion-writers [COUNT] [--correction] [--control]."""

    def __init__(self):
        super().__init__("tony-ground-motion-writers", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(
            arg,
            "tony-ground-motion-writers [COUNT] [--correction] [--control]",
        ) if arg.strip() else []
        flags = {value for value in values if value.startswith("--")}
        if not flags.issubset({"--correction", "--control"}):
            raise gdb.GdbError("usage: tony-ground-motion-writers [COUNT] [--correction] [--control]")
        counts = [value for value in values if not value.startswith("--")]
        if len(counts) > 1:
            raise gdb.GdbError("usage: tony-ground-motion-writers [COUNT] [--correction] [--control]")
        count = _integer(counts[0]) if counts else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        selected = flags or {"--correction", "--control"}
        armed = []
        if "--correction" in selected:
            for address, spec in GROUND_MOTION_CORRECTION_WRITERS.items():
                probe = GroundMotionCorrectionWriterProbe(
                    address, spec, count=count, writer=_trace_writer)
                _runtime_breakpoints.append(probe)
                armed.append(address)
        if "--control" in selected:
            for address, spec in GROUND_MOTION_CONTROL_WRITERS.items():
                probe = GroundMotionControlWriterProbe(
                    address, spec, count=count, writer=_trace_writer)
                _runtime_breakpoints.append(probe)
                armed.append(address)
            for address, purpose in GROUND_MOTION_RANDOM_SITES.items():
                probe = GroundMotionRandomProbe(
                    address, purpose, count=count, writer=_trace_writer)
                _runtime_breakpoints.append(probe)
                armed.append(address)
        limit = "until disabled" if count is None else f"for {count} hits per writer"
        groups = ", ".join(sorted(flag[2:] for flag in selected))
        _write(
            f"ground-motion {groups} writer probes armed {limit}: "
            + ", ".join(f"0x{address:08x}" for address in armed)
        )


class TonyGroundMotionProfileProbe(gdb.Command):
    """tony-ground-motion-profile-probe [COUNT] -- trace B010 profile sources."""

    def __init__(self):
        super().__init__("tony-ground-motion-profile-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-ground-motion-profile-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-ground-motion-profile-probe [COUNT]")
        count = _integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        for address, spec in GROUND_MOTION_PROFILE_WRITERS.items():
            probe = GroundMotionProfileWriterProbe(
                address, spec, count=count, writer=_trace_writer)
            _runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} hits per writer"
        addresses = ", ".join(
            f"0x{address:08x}" for address in GROUND_MOTION_PROFILE_WRITERS)
        _write(f"ground-motion profile writer probes armed {limit}: {addresses}")


class TonyInAirProbe(gdb.Command):
    """tony-in-air-probe [COUNT] -- log candidate in-air handler entries."""

    def __init__(self):
        super().__init__("tony-in-air-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-in-air-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-in-air-probe [COUNT]")
        count = _integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = InAirHandlerProbe(count, writer=_trace_writer)
        _runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} observations"
        _write(f"in-air handler probe armed {limit} at 0x{probe.address:08x}")


class TonySpecialPhysicsProbe(gdb.Command):
    """tony-special-physics-probe [COUNT] -- log dedicated state handlers."""

    def __init__(self):
        super().__init__("tony-special-physics-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-special-physics-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-special-physics-probe [COUNT]")
        count = _integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        for address in SPECIAL_HANDLER_INFO:
            probe = SpecialPhysicsHandlerProbe(address, count=count, writer=_trace_writer)
            _runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} observations per handler"
        _write(f"special physics handler probes armed {limit}")


class TonyAirCollisionProbe(gdb.Command):
    """tony-air-collision-probe [COUNT] -- log raw in-air cast results."""

    def __init__(self):
        super().__init__("tony-air-collision-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-air-collision-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-air-collision-probe [COUNT]")
        count = _integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = AirCollisionQueryProbe(count, writer=_trace_writer)
        _runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} observations"
        _write(f"air-collision probe armed {limit} at 0x{probe.address:08x}")


class TonyPhysicsStateRequestProbe(gdb.Command):
    """tony-physics-state-requests [COUNT] -- log state requests and reasons."""

    def __init__(self):
        super().__init__("tony-physics-state-requests", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-physics-state-requests [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-physics-state-requests [COUNT]")
        count = _integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = PhysicsStateRequestProbe(count, writer=_trace_writer)
        _runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} observations"
        _write(f"physics state-request probe armed {limit} at 0x{probe.address:08x}")


class TonyPhysicsStateWriterProbe(gdb.Command):
    """tony-physics-state-writers [COUNT] -- log the exact +0x30b8 store."""

    def __init__(self):
        super().__init__("tony-physics-state-writers", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-physics-state-writers [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-physics-state-writers [COUNT]")
        count = _integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = PhysicsStateWriterProbe(count, writer=_trace_writer)
        _runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} observations"
        _write(f"physics state-writer probe armed {limit} at 0x{probe.address:08x}")


class TonyOllieLatchProbe(gdb.Command):
    """tony-ollie-latch-probe [COUNT] -- log exact ollie latch PCs."""

    def __init__(self):
        super().__init__("tony-ollie-latch-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-ollie-latch-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-ollie-latch-probe [COUNT]")
        count = _integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        for address in OLLIE_LATCH_WRITERS:
            probe = OllieLatchProbe(address, count=count, writer=_trace_writer)
            _runtime_breakpoints.append(probe)
        pcs = ", ".join(f"0x{address:08x}" for address in OLLIE_LATCH_WRITERS)
        limit = "until disabled" if count is None else f"for {count} hits per PC"
        _write(f"ollie latch probes armed {limit}: {pcs}")


class TonyViewProjectionPerturb(gdb.Command):
    """tony-view-perturb [COUNT] [--freeze] -- alternate view word 6."""

    def __init__(self):
        super().__init__("tony-view-perturb", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-view-perturb [COUNT] [--freeze]") if arg.strip() else []
        freeze = "--freeze" in values
        values = [value for value in values if value != "--freeze"]
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-view-perturb [COUNT] [--freeze]")
        count = _integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = ViewProjectionPerturbProbe(
            count, writer=_trace_writer, freeze_input=freeze)
        _runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} observations"
        suffix = "; baseline view input frozen" if freeze else ""
        _write(
            f"view projection perturb probe armed {limit} at 0x{probe.address:08x}"
            f"{suffix}")


class TonyCameraPositionProbe(gdb.Command):
    """tony-camera-position-probe [COUNT] -- sample final camera transform inputs."""

    def __init__(self):
        super().__init__("tony-camera-position-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-camera-position-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-camera-position-probe [COUNT]")
        count = _integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = CameraPositionTransformProbe(count, writer=_trace_writer)
        _runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} observations"
        _write(f"camera position transform probe armed {limit} at 0x{probe.address:08x}")


class TonyCameraCollisionProbe(gdb.Command):
    """tony-camera-collision-probe [COUNT] -- sample camera world queries."""

    def __init__(self):
        super().__init__("tony-camera-collision-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-camera-collision-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-camera-collision-probe [COUNT]")
        count = _integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = CameraCollisionProbe(count, writer=_trace_writer)
        _runtime_breakpoints.append(probe)
        result_probe = CameraCollisionResultProbe(count, writer=_trace_writer)
        _runtime_breakpoints.append(result_probe)
        limit = "until disabled" if count is None else f"for {count} observations"
        _write(
            f"camera collision probes armed {limit} at "
            f"0x{probe.address:08x}/0x{result_probe.address:08x}")


class TonyActorSubmissionProbe(gdb.Command):
    """tony-actor-probe [COUNT] -- sample the actor/model submission pointer."""

    def __init__(self):
        super().__init__("tony-actor-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-actor-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-actor-probe [COUNT]")
        count = _integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = ActorSubmissionProbe(count, writer=_trace_writer)
        _runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} observations"
        _write(f"actor submission probe armed {limit} at 0x{probe.address:08x}")


class TonyGeometrySubmissionProbe(gdb.Command):
    """tony-geometry-probe [COUNT] -- sample the raw geometry handoff."""

    def __init__(self):
        super().__init__("tony-geometry-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-geometry-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-geometry-probe [COUNT]")
        count = _integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = GeometrySubmissionProbe(count, writer=_trace_writer)
        _runtime_breakpoints.append(probe)
        raster_probe = GeometryRasterReturnProbe(count, writer=_trace_writer)
        _runtime_breakpoints.append(raster_probe)
        limit = "until disabled" if count is None else f"for {count} observations"
        _write(
            f"geometry submission/raster probes armed {limit} at "
            f"0x{probe.address:08x}/0x{raster_probe.address:08x}"
        )


class TonyTransformedVertexProbe(gdb.Command):
    """tony-transformed-vertices [COUNT] -- sample common projected vertices."""

    def __init__(self):
        super().__init__("tony-transformed-vertices", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-transformed-vertices [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-transformed-vertices [COUNT]")
        count = _integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = TransformedVertexProbe(count, writer=_trace_writer)
        _runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} observations"
        _write(f"transformed vertex probe armed {limit} at 0x{probe.address:08x}")


class TonyPlayerDiff(gdb.Command):
    """tony-player-diff [COUNT] -- log changed player words at physics dispatch."""

    def __init__(self):
        super().__init__("tony-player-diff", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-player-diff [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-player-diff [COUNT]")
        count = _integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = PlayerDiffProbe(count, writer=_trace_writer)
        _runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} observations"
        _write(f"player diff probe armed {limit} at 0x{probe.address:08x}")


class TonyPositionCommitProbe(gdb.Command):
    """tony-position-commit [COUNT] -- log stable callers of position commit."""

    DEFAULT_COUNT = 16

    def __init__(self):
        super().__init__("tony-position-commit", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-position-commit [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-position-commit [COUNT]")
        count = _integer(values[0]) if values else self.DEFAULT_COUNT
        if count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        for address, label in POSITION_COMMIT_CALLS:
            breakpoint = PositionCommitBreakpoint(address, label, count, writer=_trace_writer)
            _runtime_breakpoints.append(breakpoint)
        _write(
            f"position-commit probe armed for {count} observations per caller "
            f"({len(POSITION_COMMIT_CALLS)} callsites)"
        )


class TonyCollisionProbe(gdb.Command):
    """tony-collision-probe [COUNT] -- log shared collision query inputs/results."""

    DEFAULT_COUNT = 32

    def __init__(self):
        super().__init__("tony-collision-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-collision-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-collision-probe [COUNT]")
        count = _integer(values[0]) if values else self.DEFAULT_COUNT
        if count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = CollisionQueryProbe(count, writer=_trace_writer)
        _runtime_breakpoints.extend((probe.entry, probe.return_breakpoint))
        _write(f"collision query probe armed for {count} completed calls")


class TonyCollisionInitBoundaryProbe(gdb.Command):
    """tony-collision-init-boundaries [COUNT] -- probe setup basis boundaries."""

    def __init__(self):
        super().__init__("tony-collision-init-boundaries", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(
            arg,
            "tony-collision-init-boundaries [COUNT]",
        ) if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-collision-init-boundaries [COUNT]")
        count = _integer(values[0]) if values else len(COLLISION_INIT_BOUNDARY_CASES)
        if count <= 0 or count > len(COLLISION_INIT_BOUNDARY_CASES):
            raise gdb.GdbError(
                "COUNT must be between 1 and "
                f"{len(COLLISION_INIT_BOUNDARY_CASES)}"
            )
        probe = CollisionQueryInitBoundaryProbe(count, writer=_trace_writer)
        _runtime_breakpoints.extend(probe.breakpoints)
        _write(
            f"collision init boundary probe armed for {count} completed calls"
        )


class TonyCollisionLoaderProbe(gdb.Command):
    """tony-collision-loader-probe [COUNT] -- log zone-loader handoffs."""

    DEFAULT_COUNT = 4

    def __init__(self):
        super().__init__("tony-collision-loader-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-collision-loader-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-collision-loader-probe [COUNT]")
        count = _integer(values[0]) if values else self.DEFAULT_COUNT
        if count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = CollisionLoaderProbe(count, writer=_trace_writer)
        _runtime_breakpoints.extend(probe.breakpoints)
        _write(f"collision loader probe armed for {count} completed calls")


class TonyCollisionModelKindProbe(gdb.Command):
    """tony-collision-model-kind-probe [COUNT] -- log model/cache setup."""

    DEFAULT_COUNT = 32

    def __init__(self):
        super().__init__("tony-collision-model-kind-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-collision-model-kind-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-collision-model-kind-probe [COUNT]")
        count = _integer(values[0]) if values else self.DEFAULT_COUNT
        if count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = CollisionModelKindProbe(count, writer=_trace_writer)
        _runtime_breakpoints.extend(probe.breakpoints)
        _write(f"collision model-kind probe armed for {count} completed calls")


class TonyCollisionFlagsProbe(gdb.Command):
    """tony-collision-flags-probe [COUNT] -- log face flag decoding."""

    DEFAULT_COUNT = 32

    def __init__(self):
        super().__init__("tony-collision-flags-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-collision-flags-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-collision-flags-probe [COUNT]")
        count = _integer(values[0]) if values else self.DEFAULT_COUNT
        if count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = CollisionFlagProbe(count, writer=_trace_writer)
        _runtime_breakpoints.extend(probe.breakpoints)
        _write(f"collision flag probe armed for {count} completed calls")


class TonyCollisionDynamicProbe(gdb.Command):
    """tony-collision-dynamic-probe [COUNT] -- log linked-object face tests."""

    DEFAULT_COUNT = 32

    def __init__(self):
        super().__init__("tony-collision-dynamic-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-collision-dynamic-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-collision-dynamic-probe [COUNT]")
        count = _integer(values[0]) if values else self.DEFAULT_COUNT
        if count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = CollisionDynamicProbe(count, writer=_trace_writer)
        _runtime_breakpoints.extend(probe.breakpoints)
        _write(f"collision dynamic probe armed for {count} completed calls")


class TonyCollisionDynamicCullProbe(gdb.Command):
    """tony-collision-dynamic-cull-probe [COUNT] -- log linked broad-phase survivors."""

    DEFAULT_COUNT = 32

    def __init__(self):
        super().__init__("tony-collision-dynamic-cull-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-collision-dynamic-cull-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-collision-dynamic-cull-probe [COUNT]")
        count = _integer(values[0]) if values else self.DEFAULT_COUNT
        if count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = CollisionDynamicCullProbe(count, writer=_trace_writer)
        _runtime_breakpoints.extend(probe.breakpoints)
        _write(f"collision dynamic cull probe armed for {count} completed calls")


class TonyCollisionDynamicTransformProbe(gdb.Command):
    """tony-collision-transform-probe [COUNT] -- log the 0x0200 matrix tail."""

    DEFAULT_COUNT = 16

    def __init__(self):
        super().__init__("tony-collision-transform-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-collision-transform-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-collision-transform-probe [COUNT]")
        count = _integer(values[0]) if values else self.DEFAULT_COUNT
        if count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = CollisionDynamicTransformProbe(count, writer=_trace_writer)
        _runtime_breakpoints.extend(probe.breakpoints)
        _write(f"collision transform probe armed for {count} completed calls")


class TonyCollisionDynamicTransformMutationProbe(gdb.Command):
    """tony-collision-transform-mutate X Y Z [COUNT] -- calibrate Q12 scale."""

    DEFAULT_COUNT = 16

    def __init__(self):
        super().__init__("tony-collision-transform-mutate", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(
            arg,
            "tony-collision-transform-mutate X Y Z [COUNT]",
        )
        if len(values) not in (3, 4):
            raise gdb.GdbError(
                "usage: tony-collision-transform-mutate X Y Z [COUNT]"
            )
        scale = tuple(_integer(value) for value in values[:3])
        if any(value < -0x8000 or value > 0x7FFF for value in scale):
            raise gdb.GdbError("X, Y, and Z must fit signed 16-bit Q12 words")
        count = _integer(values[3]) if len(values) == 4 else self.DEFAULT_COUNT
        if count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = CollisionDynamicTransformMutationProbe(
            scale, count=count, writer=_trace_writer
        )
        _runtime_breakpoints.extend(probe.breakpoints)
        _write(
            "collision transform mutation probe armed for "
            f"{count} completed calls with scale {scale}"
        )


class TonyType192CommandProbe(gdb.Command):
    """tony-trg-type192-probe [COUNT] -- trace type-192 command effects."""

    DEFAULT_COUNT = 32

    def __init__(self):
        super().__init__("tony-trg-type192-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-trg-type192-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-trg-type192-probe [COUNT]")
        count = _integer(values[0]) if values else self.DEFAULT_COUNT
        if count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = Type192CommandProbe(count, writer=_trace_writer)
        _runtime_breakpoints.extend(probe.breakpoints)
        _write(f"TRG type-192 command probe armed for {count} completed calls")


def _install_recording_instrumentation() -> None:
    global _recording_controller
    if _recording_controller is not None:
        return
    session_dir = os.environ.get("TONY_SESSION_DIR")
    _recording_controller = RecordingController(session_dir=session_dir)
    sink = _RecordingEventSink(_recording_controller)

    _runtime_breakpoints.extend(
        (
            TonyRecordingInputBreakpoint(_recording_controller),
            TonyRecordingFrameEntryBreakpoint(_recording_controller),
        )
    )

    collision_probe = CollisionQueryProbe(writer=sink)
    _runtime_breakpoints.extend(
        (collision_probe.entry, collision_probe.return_breakpoint)
    )
    _runtime_breakpoints.extend(
        (
            AirCollisionQueryProbe(writer=sink),
            PhysicsStateRequestProbe(writer=sink),
            PhysicsStateWriterProbe(writer=sink),
            TonyAnimationRequestBreakpoint(None, None, writer=sink),
        )
    )
    for address, label in POSITION_COMMIT_CALLS:
        _runtime_breakpoints.append(
            PositionCommitBreakpoint(address, label, None, writer=sink)
        )


_registered = False


def register_commands() -> None:
    global _registered
    if _registered:
        return
    TonyReadInteger("tony-read8", mem.u8, "uint8")
    TonyReadInteger("tony-read16", mem.u16, "uint16")
    TonyReadInteger("tony-read32", mem.u32, "uint32")
    TonyReadFloat()
    TonyHexdump()
    TonyDump()
    TonySnapshot()
    TonyDiff()
    TonyModules()
    TonyBreakpointCommand()
    TonyAddresses()
    TonyTHPS2Breakpoint()
    TonySkipMovies()
    TonyForceLevel()
    TonyFrontendPlay()
    TonyFrontendConfirm()
    TonyRecordingStart()
    TonyRecordingStop()
    TonyRecordingToggle()
    TonyRecordingStatus()
    TonyRetailReplay()
    TonyPlayerSample()
    TonyInputSample()
    TonyActionEdge()
    TonyJumpEdge()
    TonyKeyLoop()
    TonyKeyClear()
    TonyAnimationSample()
    TonyAnimationRequestSample()
    TonyAnimationSelectorSample()
    TonyActionSequence()
    TonyWatch()
    TonyWatchOnce()
    TonyWatchBatch()
    TonyWatchLog()
    TonyWatchClear()
    TonyTraceOpen()
    TonyTraceClose()
    TonyFrameClock()
    TonyPhysicsProbe()
    TonyForcePhysicsState()
    TonyCameraProbe()
    TonyCameraForceMode()
    TonyCameraViewportProbe()
    TonyCameraTimingProbe()
    TonyCameraPointSelectProbe()
    TonyCameraPointStateProbe()
    TonyCameraEffectsProbe()
    TonyViewProjectionProbe()
    TonyMovementPhysicsProbe()
    TonyGroundMotionProbe()
    TonyGroundMotionWriters()
    TonyGroundMotionProfileProbe()
    TonyInAirProbe()
    TonySpecialPhysicsProbe()
    TonyAirCollisionProbe()
    TonyPhysicsStateRequestProbe()
    TonyPhysicsStateWriterProbe()
    TonyOllieLatchProbe()
    TonyViewProjectionPerturb()
    TonyCameraPositionProbe()
    TonyCameraCollisionProbe()
    TonyActorSubmissionProbe()
    TonyGeometrySubmissionProbe()
    TonyTransformedVertexProbe()
    TonyPlayerDiff()
    TonyPositionCommitProbe()
    TonyCollisionProbe()
    TonyCollisionInitBoundaryProbe()
    TonyCollisionLoaderProbe()
    TonyCollisionModelKindProbe()
    TonyCollisionFlagsProbe()
    TonyCollisionDynamicProbe()
    TonyCollisionDynamicCullProbe()
    TonyCollisionDynamicTransformProbe()
    TonyCollisionDynamicTransformMutationProbe()
    TonyType192CommandProbe()
    _install_recording_instrumentation()
    _registered = True
    _write(
        "OpenTony GDB helpers loaded: tony-read8, tony-read16, tony-read32, tony-readf, "
        "tony-hexdump, tony-dump, tony-snapshot, tony-diff, tony-modules, tony-bp, "
        "tony-thps2, tony-bp-thps2, "
        "tony-skip-movies, tony-force-level, tony-player-sample, tony-input-sample, "
        "tony-record-start, tony-record-stop, tony-record-toggle, tony-record-status, "
        "tony-replay-retail, "
        "tony-action-edge, tony-jump-edge, "
        "tony-key-loop, tony-key-clear, tony-animation-sample, tony-animation-request-sample, "
        "tony-animation-selector-sample, "
        "tony-watch, tony-watch-once, tony-watch-batch, tony-watch-log, tony-watch-clear, "
        "tony-trace-open, tony-trace-close, tony-frame-clock, tony-physics-probe, "
        "tony-force-physics-state, "
        "tony-camera-probe, tony-camera-effects-probe, tony-view-probe, tony-view-perturb, "
        "tony-camera-position-probe, tony-actor-probe, tony-geometry-probe, "
        "tony-player-diff, tony-position-commit, "
        "tony-collision-probe, "
        "tony-collision-init-boundaries, "
        "tony-movement-physics-probe, "
        "tony-ground-motion-probe, tony-ground-motion-writers, "
        "tony-ground-motion-profile-probe, "
        "tony-in-air-probe, tony-air-collision-probe, tony-physics-state-requests, "
        "tony-special-physics-probe, "
        "tony-physics-state-writers, "
        "tony-ollie-latch-probe, "
        "tony-player-diff, tony-position-commit, "
        "tony-collision-loader-probe, tony-collision-model-kind-probe, "
        "tony-collision-flags-probe, "
        "tony-collision-dynamic-probe, tony-collision-dynamic-cull-probe, "
        "tony-collision-transform-probe, tony-collision-transform-mutate, "
        "tony-trg-type192-probe, "
        "tony-skip-movies, tony-force-level, tony-frontend-play, tony-frontend-confirm, "
        "tony-player-sample, tony-input-sample, "
        "tony-action-sequence, "
        "tony-watch, tony-watch-once, tony-watch-batch, tony-watch-log, tony-watch-clear, "
        "tony-trace-open, tony-trace-close, tony-frame-clock, tony-physics-probe, "
        "tony-camera-probe, tony-camera-force-mode, tony-camera-timing-probe, "
        "tony-camera-viewport-probe, "
        "tony-camera-point-probe, "
        "tony-camera-point-state-probe, "
        "tony-camera-effects-probe, "
        "tony-view-probe, tony-view-perturb, "
        "tony-camera-position-probe, tony-camera-collision-probe, "
        "tony-actor-probe, tony-geometry-probe, tony-transformed-vertices, "
        "tony-player-diff, tony-position-commit"
    )
