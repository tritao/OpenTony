"""Retail replay command registration."""

from __future__ import annotations

import gdb

from ..replay import create_retail_replay
from .common import argv

retail_replay = None


class TonyRetailReplay(gdb.Command):
    """tony-replay-retail FILE -- replay one retail recording strictly."""

    def __init__(self):
        super().__init__("tony-replay-retail", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        del from_tty
        values = argv(arg, "tony-replay-retail FILE")
        if len(values) != 1:
            raise gdb.GdbError("usage: tony-replay-retail FILE")
        global retail_replay
        if retail_replay is not None:
            raise gdb.GdbError("a retail replay is already armed")
        try:
            retail_replay = create_retail_replay(values[0])
        except (OSError, TypeError, ValueError, gdb.GdbError) as exc:
            raise gdb.GdbError(str(exc)) from exc
        retail_replay.install()
