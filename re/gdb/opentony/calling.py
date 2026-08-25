"""Calling-convention helpers for entry breakpoints in the 32-bit x86 game."""

from __future__ import annotations

import gdb

from .memory import Memory, mem


class CallContext:
    """A snapshot of the x86 registers and stack at a breakpoint.

    This models a breakpoint at a function entry, before the callee changes
    ESP.  Cdecl/stdcall arguments are therefore at ``ESP + 4`` and ECX is the
    object pointer for the game's thiscall functions.
    """

    def __init__(self, memory: Memory | None = None, registers: dict[str, int] | None = None):
        self.memory = memory or mem
        self._registers = {key.lstrip("$").lower(): int(value) for key, value in (registers or {}).items()}
        self.esp = self.register("esp")

    def register(self, name: str) -> int:
        name = name.lstrip("$").lower()
        if name in self._registers:
            return self._registers[name]
        try:
            return int(gdb.parse_and_eval(f"${name}"))
        except (gdb.error, TypeError, ValueError) as exc:
            raise gdb.GdbError(f"could not read register ${name}") from exc

    def arg(self, index: int) -> int:
        if index < 0:
            raise gdb.GdbError("argument index must be non-negative")
        return self.memory.ptr(self.esp + 4 + (index * 4))

    def callsite_arg(self, index: int) -> int:
        """Read an argument at a breakpoint on the caller's ``call`` instruction.

        At a callsite the caller has already pushed the arguments, but the
        return address has not been pushed yet.  The first argument is thus at
        ``ESP`` rather than ``ESP + 4``.
        """

        if index < 0:
            raise gdb.GdbError("argument index must be non-negative")
        return self.memory.ptr(self.esp + (index * 4))

    def return_address(self) -> int:
        return self.memory.ptr(self.esp)

    def caller(self) -> int:
        """Return the caller instruction address saved on the entry stack."""

        return self.return_address()

    def this_ptr(self) -> int:
        return self.register("ecx")

    def return_value(self) -> int:
        return self.register("eax")


def capture(memory: Memory | None = None) -> CallContext:
    """Capture the current GDB register/stack state."""

    return CallContext(memory=memory)


class _CurrentCall:
    """Convenience facade for probes that need one current register snapshot."""

    def _capture(self) -> CallContext:
        return capture()

    def arg(self, index: int) -> int:
        return self._capture().arg(index)

    def return_address(self) -> int:
        return self._capture().return_address()

    def this_ptr(self) -> int:
        return self._capture().this_ptr()

    def caller(self) -> int:
        return self._capture().caller()


call = _CurrentCall()


Call = CallContext
