"""Shared debugger mechanisms and runtime state for OpenTony commands."""

from __future__ import annotations

import shlex

import gdb

from ..breakpoint import TonyBreakpoint

# Breakpoint ownership is process-local, while the breakpoint objects belong
# to the command that created them.  A single list gives lifecycle commands a
# mechanism-level view without importing game-specific probe modules.
runtime_breakpoints: list[TonyBreakpoint] = []
key_loop_breakpoints = []

_trace_writer = None


def trace_writer():
    """Return the active trace sink for commands that emit optional traces."""

    return _trace_writer


def set_trace_writer(writer) -> None:
    global _trace_writer
    _trace_writer = writer


def argv(arg: str, usage: str) -> list[str]:
    try:
        values = shlex.split(arg)
    except ValueError as exc:
        raise gdb.GdbError(f"invalid arguments: {exc}") from exc
    if not values:
        raise gdb.GdbError(f"usage: {usage}")
    return values


def integer(value: str) -> int:
    try:
        # parse_and_eval also accepts useful GDB expressions such as $eip.
        return int(gdb.parse_and_eval(value))
    except (gdb.error, TypeError, ValueError) as exc:
        raise gdb.GdbError(f"expected an address or integer expression, got {value!r}") from exc


def write(text: str) -> None:
    gdb.write(text + "\n")
