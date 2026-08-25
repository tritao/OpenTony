"""Interactive GDB helpers for the exact THPS2 PC executable build.

This file runs inside GDB's embedded Python. Keep it dependency-free: the
OpenTony virtualenv is not available to GDB. Promote repeatable discoveries
into the main ``tony`` package and evidence files.
"""

import shlex
import struct
from pathlib import Path

import gdb


THPS2_BUILD_SHA256 = "f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c"

# These addresses belong to the recorded retail executable identified above.
# Keep new entries tied to an evidence file before using them in experiments.
THPS2_ADDRESSES = {
    "entry": (0x00502F74, "PE entry point", "re/evidence/startup-runtime.md"),
    "startup_return": (0x00503054, "startup return path", "re/evidence/startup-runtime.md"),
    "cd_check": (0x004BB240, "CD audio TOC authentication helper", "re/evidence/cd-check.md"),
    "cd_mci_helper": (0x004BB2D0, "MCI/CD media helper", "re/evidence/cd-check.md"),
    "drive_scan": (0x004F6730, "CD drive and CDPATH scan", "re/evidence/cd-check.md"),
    "startup": (0x004F7E30, "startup CD-check caller", "re/evidence/cd-check.md"),
    "cd_recheck": (0x004F6510, "later CD-check caller", "re/evidence/cd-check.md"),
}


def _argv(arg: str, usage: str) -> list[str]:
    try:
        values = shlex.split(arg)
    except ValueError as exc:
        raise gdb.GdbError(f"invalid arguments: {exc}") from exc
    if not values:
        raise gdb.GdbError(f"usage: {usage}")
    return values


def _integer(value: str) -> int:
    try:
        # parse_and_eval also accepts useful GDB expressions such as $eip.
        return int(gdb.parse_and_eval(value))
    except (gdb.error, TypeError, ValueError) as exc:
        raise gdb.GdbError(f"expected an address or integer expression, got {value!r}") from exc


def _read(address: int, size: int) -> bytes:
    if size < 0:
        raise gdb.GdbError("size must be non-negative")
    try:
        return bytes(gdb.selected_inferior().read_memory(address, size))
    except gdb.error as exc:
        raise gdb.GdbError(f"could not read {size} bytes at 0x{address:08x}: {exc}") from exc


def _write(text: str) -> None:
    gdb.write(text + "\n")


class TonyReadInteger(gdb.Command):
    def __init__(self, name: str, size: int, fmt: str, label: str):
        super().__init__(name, gdb.COMMAND_DATA)
        self.command_name = name
        self.size = size
        self.fmt = fmt
        self.label = label

    def invoke(self, arg, from_tty):
        values = _argv(arg, f"{self.command_name} ADDRESS")
        if len(values) != 1:
            raise gdb.GdbError(f"usage: {self.command_name} ADDRESS")
        address = _integer(values[0])
        value = struct.unpack(self.fmt, _read(address, self.size))[0]
        _write(f"0x{address:08x}: {self.label} {value} (0x{value:x})")


class TonyReadFloat(gdb.Command):
    """Read a little-endian float32 from inferior memory."""

    def __init__(self):
        super().__init__("tony-readf", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-readf ADDRESS")
        if len(values) != 1:
            raise gdb.GdbError("usage: tony-readf ADDRESS")
        address = _integer(values[0])
        value = struct.unpack("<f", _read(address, 4))[0]
        _write(f"0x{address:08x}: float32 {value!r}")


def _hexdump(address: int, data: bytes, width: int = 16) -> str:
    lines = []
    for offset in range(0, len(data), width):
        chunk = data[offset:offset + width]
        hex_part = " ".join(f"{byte:02x}" for byte in chunk)
        hex_part = f"{hex_part:<{width * 3 - 1}}"
        ascii_part = "".join(chr(byte) if 32 <= byte < 127 else "." for byte in chunk)
        lines.append(f"0x{address + offset:08x}:  {hex_part}  |{ascii_part}|")
    return "\n".join(lines)


class TonyHexdump(gdb.Command):
    """tony-hexdump ADDRESS LENGTH [WIDTH] -- print inferior memory."""

    def __init__(self):
        super().__init__("tony-hexdump", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-hexdump ADDRESS LENGTH [WIDTH]")
        if len(values) not in (2, 3):
            raise gdb.GdbError("usage: tony-hexdump ADDRESS LENGTH [WIDTH]")
        address = _integer(values[0])
        length = _integer(values[1])
        width = _integer(values[2]) if len(values) == 3 else 16
        if length < 0 or width <= 0 or width > 64:
            raise gdb.GdbError("LENGTH must be non-negative and WIDTH must be between 1 and 64")
        output = _hexdump(address, _read(address, length), width)
        if output:
            gdb.write(output + "\n")


class TonyDump(gdb.Command):
    """tony-dump ADDRESS LENGTH FILE [--force] -- save raw inferior memory."""

    def __init__(self):
        super().__init__("tony-dump", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-dump ADDRESS LENGTH FILE [--force]")
        force = values[-1:] == ["--force"]
        if force:
            values.pop()
        if len(values) != 3:
            raise gdb.GdbError("usage: tony-dump ADDRESS LENGTH FILE [--force]")
        address = _integer(values[0])
        length = _integer(values[1])
        if length < 0:
            raise gdb.GdbError("LENGTH must be non-negative")
        path = Path(values[2]).expanduser()
        if path.exists() and not force:
            raise gdb.GdbError(f"refusing to overwrite {path}; add --force if intended")
        try:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(_read(address, length))
        except OSError as exc:
            raise gdb.GdbError(f"could not write {path}: {exc}") from exc
        _write(f"wrote {length} bytes from 0x{address:08x} to {path}")


class TonyModules(gdb.Command):
    """tony-modules -- show loaded module ranges from GDB/Wine."""

    def __init__(self):
        super().__init__("tony-modules", gdb.COMMAND_FILES)

    def invoke(self, arg, from_tty):
        if arg.strip():
            raise gdb.GdbError("usage: tony-modules")
        main = gdb.current_progspace().filename
        if main:
            _write(f"main: {main}")
        output = gdb.execute("info sharedlibrary", to_string=True)
        gdb.write(output)


class TonyBreakpoint(gdb.Breakpoint):
    def __init__(self, address: int, temporary: bool = False):
        super().__init__(f"*0x{address:x}", gdb.BP_BREAKPOINT, temporary=temporary)
        self.address = address


def _set_breakpoint(address: int, temporary: bool = False) -> None:
    breakpoint = TonyBreakpoint(address, temporary=temporary)
    kind = "temporary breakpoint" if temporary else "breakpoint"
    _write(f"set {kind} {breakpoint.number} at 0x{address:08x}")


class TonyBreakpointCommand(gdb.Command):
    """tony-bp ADDRESS [temporary] -- set an address breakpoint."""

    def __init__(self):
        super().__init__("tony-bp", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-bp ADDRESS [temporary]")
        if len(values) not in (1, 2) or (len(values) == 2 and values[1] != "temporary"):
            raise gdb.GdbError("usage: tony-bp ADDRESS [temporary]")
        _set_breakpoint(_integer(values[0]), len(values) == 2)


class TonyAddresses(gdb.Command):
    """tony-thps2 [NAME] -- show known addresses for this THPS2 build."""

    def __init__(self):
        super().__init__("tony-thps2", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        try:
            values = shlex.split(arg)
        except ValueError as exc:
            raise gdb.GdbError(f"invalid arguments: {exc}") from exc
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-thps2 [NAME]")
        if not values:
            _write(f"THPS2 build SHA-256: {THPS2_BUILD_SHA256}")
            for name, (address, description, evidence) in THPS2_ADDRESSES.items():
                _write(f"{name:<16} 0x{address:08x}  {description} [{evidence}]")
            return
        name = values[0]
        try:
            address, description, evidence = THPS2_ADDRESSES[name]
        except KeyError as exc:
            raise gdb.GdbError(f"unknown THPS2 address {name!r}; run tony-thps2") from exc
        _write(f"{name}: 0x{address:08x}  {description} [{evidence}]")


class TonyTHPS2Breakpoint(gdb.Command):
    """tony-bp-thps2 NAME [temporary] -- break at a known THPS2 address."""

    def __init__(self):
        super().__init__("tony-bp-thps2", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-bp-thps2 NAME [temporary]")
        if len(values) not in (1, 2) or (len(values) == 2 and values[1] != "temporary"):
            raise gdb.GdbError("usage: tony-bp-thps2 NAME [temporary]")
        try:
            address = THPS2_ADDRESSES[values[0]][0]
        except KeyError as exc:
            raise gdb.GdbError(f"unknown THPS2 address {values[0]!r}; run tony-thps2") from exc
        _set_breakpoint(address, len(values) == 2)


TonyReadInteger("tony-read8", 1, "<B", "uint8")
TonyReadInteger("tony-read16", 2, "<H", "uint16")
TonyReadInteger("tony-read32", 4, "<I", "uint32")
TonyReadFloat()
TonyHexdump()
TonyDump()
TonyModules()
TonyBreakpointCommand()
TonyAddresses()
TonyTHPS2Breakpoint()
_write("OpenTony GDB helpers loaded: tony-read8, tony-read16, tony-read32, tony-readf, tony-hexdump, tony-dump, tony-modules, tony-bp, tony-thps2, tony-bp-thps2")
