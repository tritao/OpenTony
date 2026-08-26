"""Shared runtime frame/sample clock."""

from __future__ import annotations

from .breakpoint import Context, FunctionBreakpoint


class FrameClock:
    def __init__(self):
        self.value = 0

    def reset(self) -> None:
        self.value = 0

    def tick(self) -> int:
        self.value += 1
        return self.value


frame_clock = FrameClock()


class FrameBreakpoint(FunctionBreakpoint):
    """Tick the shared clock at a selected candidate frame boundary."""

    def __init__(self, function: str, **kwargs):
        self.clock = kwargs.pop("clock", frame_clock)
        self.writer = kwargs.pop("writer", None)
        super().__init__(function, **kwargs)

    def on_hit(self, ctx: Context) -> None:
        frame = self.clock.tick()
        if self.writer is not None:
            self.writer.event(
                {
                    "type": "render_present",
                    "function": self.function,
                    "frame": frame,
                    "eip": f"0x{ctx.eip:08x}",
                    "caller": f"0x{ctx.caller():08x}",
                }
            )
