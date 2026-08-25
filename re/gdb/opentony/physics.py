"""First conservative runtime probe for the skater physics dispatcher."""

from __future__ import annotations

from .breakpoint import Context, CountingBreakpoint
from .knowledge import function_address
from .player import PlayerView


def _vec3_bits(view: PlayerView, offset: int) -> list[str]:
    return [f"0x{view.memory.f32_bits(view.address + offset + index * 4).bits:08x}" for index in range(3)]


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
        position = view.candidate_position
        velocity = view.candidate_velocity
        record = {
            "type": "physics",
            "frame": ctx.frame,
            "function": "Skater_PhysicsDispatcher",
            "eip": f"0x{ctx.eip:08x}",
            "caller": f"0x{ctx.caller():08x}",
            "player": f"0x{player:08x}",
            "physics_state": view.physics_state,
            "unknown_state": view.unknown_state,
            "candidate_position": list(position),
            "candidate_position_raw": _vec3_bits(view, view.POSITION_OFFSET),
            "candidate_velocity": list(velocity),
            "candidate_velocity_raw": _vec3_bits(view, view.VELOCITY_OFFSET),
        }
        if self.writer is None:
            self.emit(record)
        else:
            self.writer.event(record)
        return True
