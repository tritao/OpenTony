"""First conservative runtime probe for the skater physics dispatcher."""

from __future__ import annotations

from .breakpoint import Context, CountingBreakpoint
from .knowledge import function_address
from .player import PlayerView


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
