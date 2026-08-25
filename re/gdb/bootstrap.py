"""Small GDB helpers for OpenTony.

Keep this dependency-free: it runs inside GDB's embedded Python, not the
OpenTony venv. Promote repeatable discoveries into the main `tony` package.
"""

import gdb
import struct


def _read(address: int, size: int) -> bytes:
    return bytes(gdb.selected_inferior().read_memory(address, size))


class TonyRead32(gdb.Command):
    """tony-read32 ADDRESS -- read a little-endian uint32."""

    def __init__(self):
        super().__init__("tony-read32", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        address = int(arg.strip(), 0)
        value = struct.unpack("<I", _read(address, 4))[0]
        gdb.write(f"0x{address:08x}: 0x{value:08x} ({value})\\n")


class TonyReadFloat(gdb.Command):
    """tony-readf ADDRESS -- read a little-endian float32."""

    def __init__(self):
        super().__init__("tony-readf", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        address = int(arg.strip(), 0)
        value = struct.unpack("<f", _read(address, 4))[0]
        gdb.write(f"0x{address:08x}: {value!r}\\n")


TonyRead32()
TonyReadFloat()
gdb.write("OpenTony GDB helpers loaded: tony-read32, tony-readf\\n")
