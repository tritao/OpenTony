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
        record = {
            "type": "physics",
            "frame": ctx.frame,
            "function": "Skater_PhysicsDispatcher",
            "eip": f"0x{ctx.eip:08x}",
            "caller": f"0x{ctx.caller():08x}",
            "player": f"0x{player:08x}",
            "physics_state_raw": view.physics_state_raw,
            "physics_state_signed": view.physics_state_signed,
            "physics_state_fixed16": view.physics_state_fixed16,
            "physics_state_float": view.physics_state_float,
            "unknown_state_raw": view.unknown_state_raw,
            "unknown_state_signed": view.unknown_state_signed,
            "unknown_state_fixed16": view.unknown_state_fixed16,
            "unknown_state_float": view.unknown_state_float,
            "position_raw": list(view.position_raw),
            "position_signed": list(view.position_signed),
            "position_fixed16": list(view.position_fixed16),
            "position_float": list(view.position_float),
            "velocity_raw": list(view.velocity_raw),
            "velocity_signed": list(view.velocity_signed),
            "velocity_fixed16": list(view.velocity_fixed16),
            "velocity_float": list(view.velocity_float),
        }
        if self.writer is None:
            self.emit(record)
        else:
            self.writer.event(record)
        return True
