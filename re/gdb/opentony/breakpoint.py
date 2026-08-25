"""Structured breakpoint primitives for small runtime experiments."""

from __future__ import annotations

import json
from collections.abc import Callable

import gdb

from .calling import CallContext, capture
from .memory import Memory, mem


class Context:
    """A consistent view of registers, the entry stack, memory, and frame."""

    def __init__(self, call: CallContext, memory: Memory):
        self.call = call
        self.memory = memory

    @classmethod
    def capture(cls, memory: Memory | None = None) -> Context:
        memory = memory or mem
        return cls(capture(memory), memory)

    @property
    def eip(self) -> int:
        return self.call.register("eip")

    @property
    def esp(self) -> int:
        return self.call.esp

    @property
    def frame(self) -> int:
        # Lazy import avoids a frame/breakpoint module cycle.
        from .frame import frame_clock

        return frame_clock.value

    def register(self, name: str) -> int:
        return self.call.register(name)

    def arg(self, index: int) -> int:
        return self.call.arg(index)

    def callsite_arg(self, index: int) -> int:
        return self.call.callsite_arg(index)

    def this_ptr(self) -> int:
        return self.call.this_ptr()

    def return_address(self) -> int:
        return self.call.return_address()

    def caller(self) -> int:
        return self.call.caller()


class TonyBreakpoint(gdb.Breakpoint):
    """Base class that turns a raw GDB hit into a structured context."""

    def __init__(
        self,
        address: int,
        *,
        temporary: bool = False,
        internal: bool = False,
        should_stop: bool = False,
    ):
        super().__init__(f"*0x{address:x}", gdb.BP_BREAKPOINT, temporary=temporary, internal=internal)
        self.address = address
        self.should_stop = should_stop

    def on_hit(self, ctx: Context) -> None:
        """Override in a probe; return from the debugger unless configured otherwise."""

    def stop(self):
        self.on_hit(Context.capture())
        return self.should_stop

    @staticmethod
    def emit(record: dict) -> None:
        gdb.write(json.dumps(record, sort_keys=True) + "\n")


class FunctionBreakpoint(TonyBreakpoint):
    """Breakpoint addressed by a generated canonical function name or alias."""

    def __init__(self, function: str, **kwargs):
        from .knowledge import function_address

        self.function = function
        super().__init__(function_address(function), **kwargs)


class LoggingBreakpoint(TonyBreakpoint):
    """Call a callback for every hit and auto-continue by default."""

    def __init__(self, address: int, logger: Callable[[Context], dict | None], **kwargs):
        self.logger = logger
        super().__init__(address, **kwargs)

    def on_hit(self, ctx: Context) -> None:
        record = self.logger(ctx)
        if record is not None:
            self.emit(record)


class CountingBreakpoint(TonyBreakpoint):
    """Run a probe for a bounded number of accepted observations."""

    def __init__(self, address: int, count: int | None = None, **kwargs):
        self.remaining = count
        self.hits = 0
        super().__init__(address, **kwargs)

    def on_count(self, ctx: Context) -> bool:
        """Return false to ignore this hit without consuming the count."""

        return True

    def on_complete(self) -> None:
        """Hook for probes that need to report completion."""

    def on_hit(self, ctx: Context) -> None:
        if not self.on_count(ctx):
            return
        self.hits += 1
        if self.remaining is not None:
            self.remaining -= 1
            if self.remaining <= 0:
                self.enabled = False
                self.on_complete()


class OneShotBreakpoint(CountingBreakpoint):
    """A bounded breakpoint that accepts exactly one observation."""

    def __init__(self, address: int, **kwargs):
        super().__init__(address, count=1, temporary=True, **kwargs)
