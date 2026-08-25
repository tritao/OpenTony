"""Runtime probes for the shared skater position-commit path."""

from __future__ import annotations

from .breakpoint import Context, CountingBreakpoint
from .knowledge import GLOBALS, function_name_at
from .memory import mem
from .player import PlayerView

POSITION_COMMIT_CALLS = (
    (0x00498CF5, "in-air-position-1"),
    (0x0049905C, "in-air-position-2"),
    (0x0049917B, "in-air-position-3"),
    (0x0049F0E5, "physics-dispatch-position"),
)


def _word_record(address: int, memory=None) -> dict:
    memory = memory or mem
    raw = memory.bytes(address, 4)
    word = memory.word32(address)
    return {"address": f"0x{address:08x}", "raw": raw.hex(), **word._asdict()}


def _vector_record(address: int, memory=None) -> list[dict]:
    return [_word_record(address + index * 4, memory) for index in range(3)]


class PositionCommitBreakpoint(CountingBreakpoint):
    """Log arguments and state before a call into the shared commit routine."""

    def __init__(self, address: int, label: str, count: int, writer=None):
        self.label = label
        self.writer = writer
        super().__init__(address, count=count, internal=True)

    def on_count(self, ctx: Context) -> bool:
        player = ctx.this_ptr()
        if not ctx.memory.valid(player):
            return False
        view = PlayerView(player, ctx.memory)
        arguments = [ctx.arg(index) for index in range(3)]
        record = {
            "type": "position_commit",
            "frame": ctx.frame,
            "callsite": self.label,
            "eip": f"0x{ctx.eip:08x}",
            "function": function_name_at(ctx.eip),
            "caller_return": f"0x{ctx.caller():08x}",
            "player": f"0x{player:08x}",
            "arguments": [_word_record(ctx.esp + 4 + index * 4, ctx.memory) for index in range(3)],
            "argument_values": [f"0x{value:08x}" for value in arguments],
            "position_before": _vector_record(player + view.POSITION_OFFSET, ctx.memory),
            "history_vector": _vector_record(player + view.VELOCITY_OFFSET, ctx.memory),
            "physics_state": _word_record(player + view.PHYSICS_STATE_OFFSET, ctx.memory),
            "unknown_state": _word_record(player + view.UNKNOWN_STATE_OFFSET, ctx.memory),
        }
        action_mask_address = GLOBALS.get("ActionMask")
        keyboard_address = GLOBALS.get("KeyboardState")
        if action_mask_address is not None:
            record["action_mask"] = ctx.memory.u16(action_mask_address)
        if keyboard_address is not None:
            keyboard = ctx.memory.bytes(keyboard_address, 0x100)
            record["held_keys"] = [code for code, value in enumerate(keyboard) if value & 0x80]
        if self.writer is None:
            self.emit(record)
        else:
            self.writer.event(record)
        return True
