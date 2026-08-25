"""First conservative runtime probe for the skater physics dispatcher."""

from __future__ import annotations

import struct

from .breakpoint import Context, CountingBreakpoint
from .knowledge import GLOBALS, function_address
from .player import PlayerView

PLAYER_DIFF_SIZE = 0x3208


def _changed_words(previous: bytes, current: bytes) -> list[dict[str, int | str]]:
    """Return raw 32-bit changes while preserving their player-relative offsets."""

    changes = []
    for index, (before_word, after_word) in enumerate(
        zip(
            struct.iter_unpack("<I", previous),
            struct.iter_unpack("<I", current),
            strict=True,
        )
    ):
        before = before_word[0]
        after = after_word[0]
        if before != after:
            changes.append(
                {
                    "offset": f"0x{index * 4:04x}",
                    "before": before,
                    "after": after,
                }
            )
    return changes


class PhysicsProbe(CountingBreakpoint):
    """Observe the dispatcher without assigning meanings to its state enum."""

    def __init__(self, count: int | None = None, writer=None):
        super().__init__(function_address("physics_dispatch"), count=count, internal=True)
        self.writer = writer

    def on_count(self, ctx: Context) -> bool:
        player = ctx.arg(0)
        if not ctx.memory.valid(player):
            return False
        view = PlayerView(player, ctx.memory)
        position = view.position
        position_history = view.position_history
        record = {
            "type": "physics",
            "frame": ctx.frame,
            "function": "Skater_PhysicsDispatcher",
            "eip": f"0x{ctx.eip:08x}",
            "caller": f"0x{ctx.caller():08x}",
            "player": f"0x{player:08x}",
            "physics_state": view.physics_state,
            "unknown_state": view.unknown_state,
            "position_raw": list(view.position_raw),
            "position_fixed": list(position.values),
            "position_history_raw": list(view.position_history_raw),
            "position_history_fixed": list(position_history.values),
        }
        if self.writer is None:
            self.emit(record)
        else:
            self.writer.event(record)
        return True


class PlayerDiffProbe(CountingBreakpoint):
    """Record per-dispatch player-word changes and the active action mask."""

    def __init__(self, count: int | None = None, writer=None):
        super().__init__(function_address("physics_dispatch"), count=count, internal=True)
        self.writer = writer
        self._previous_player = None
        self._previous_bytes = None

    def on_count(self, ctx: Context) -> bool:
        player = ctx.this_ptr()
        if not ctx.memory.valid(player):
            return False
        current = ctx.memory.bytes(player, PLAYER_DIFF_SIZE)
        previous = self._previous_bytes if self._previous_player == player else None
        action_mask_address = GLOBALS.get("ActionMask")
        record = {
            "type": "player_diff",
            "frame": ctx.frame,
            "function": "Skater_PhysicsDispatcher",
            "eip": f"0x{ctx.eip:08x}",
            "player": f"0x{player:08x}",
            "physics_state": ctx.memory.u32(player + PlayerView.PHYSICS_STATE_OFFSET),
            "action_mask": (
                ctx.memory.u16(action_mask_address) if action_mask_address is not None else None
            ),
            "snapshot_size": PLAYER_DIFF_SIZE,
            "previous_available": previous is not None,
            "changed_words": _changed_words(previous, current) if previous is not None else [],
        }
        self._previous_player = player
        self._previous_bytes = current
        if self.writer is None:
            self.emit(record)
        else:
            self.writer.event(record)
        return True
