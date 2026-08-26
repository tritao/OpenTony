from __future__ import annotations

import hashlib
import json
import shutil
import struct
from collections import Counter
from dataclasses import dataclass
from pathlib import Path, PurePosixPath

from . import __version__
from .common import ROOT, relative_to_root, resolve, sha256

_PKR_HEADER = struct.Struct("<4sIII")
_PKR_DIRECTORY = struct.Struct("<32sII")
_PKR_FILE = struct.Struct("<32sIIII")
_PKR_MAGIC = b"PKR2"
_PKR_FILE_MARKER = 0xFFFFFFFE
_PRE_HEADER = struct.Struct("<I")
_TRG_HEADER = struct.Struct("<4sII")
_TRG_MAGIC = b"_TRG"
_HED_RECORD = struct.Struct("<III")
_NAMED_TABLE_RECORD = struct.Struct("<II")
_NAMED_TABLE_TERMINATOR = b"\xff\xff\xff\xff"
_HED_TERMINATOR = b"\0\0\0\0"
_TRG_NODE_NAMES = {
    1: "baddy",
    2: "crate",
    3: "point",
    4: "autoexec",
    5: "powerup",
    6: "command_point",
    8: "restart",
    10: "rail_point",
    11: "rail_def",
    12: "trick_object",
    13: "camera_point",
    14: "goal_object",
    15: "autoexec2",
    255: "terminator",
    501: "off_light",
    1000: "script_point",
}

# These names follow the retail PC dispatcher at 0x004c5dc0.  They are
# deliberately limited to operations whose branch, operand shape, or embedded
# diagnostic gives us a useful name; an unknown command remains visible as a
# raw opcode instead of being guessed.
_TRG_COMMAND_NAMES = {
    0x02: "set_cheat_restart_strings",
    0x03: "send_pulse",
    0x04: "send_suspend",
    0x05: "send_activate",
    0x0A: "send_signal",
    0x0B: "send_kill",
    0x0C: "send_kill_visible",
    0x0D: "send_visible",
    0x68: "set_fog_range",
    0x69: "play_music_track",
    0x6A: "play_sound",
    0x7E: "load_resource",
    0x7F: "load_resource_aux",
    0x80: "load_resource_mode1",
    0x81: "flush_resource_state",
    0x8D: "set_path_segment",
    0x8E: "set_competition_name",
    0xB2: "select_two_player_restart",
    0x83: "clear_object_flag_by_id",
    0x84: "set_object_flag_by_id",
    0x93: "set_initial_state",
    0x94: "if_pulse_count",
    0x95: "endif",
    0x97: "set_timer",
    0x98: "kill_bruce_restart",
    0xAB: "create_script_object",
    0x9D: "set_reverb_type",
    0x9E: "update_level_event_state",
    0xC8: "set_script_value",
    0xC9: "complete_gap",
    0xCA: "set_level_value",
    0xCB: "set_career_flag",
    0xCC: "if_career_flag",
    0xCD: "if_goal",
}


class PkrFormatError(ValueError):
    """The input does not satisfy the observed PKR2 archive layout."""


class HedFormatError(ValueError):
    """The input does not satisfy the observed HET/HED/WAD layout."""


class PsxFormatError(ValueError):
    """The input does not satisfy the observed THPS2 PSX asset layout."""


@dataclass(frozen=True)
class PkrDirectory:
    name: str
    entries_offset: int
    file_count: int


@dataclass(frozen=True)
class PkrEntry:
    directory: str
    name: str
    data_offset: int
    size: int

    @property
    def archive_path(self) -> str:
        directory = self.directory.rstrip("/")
        return f"{directory}/{self.name}" if directory else self.name


@dataclass(frozen=True)
class PreEntry:
    name: str
    data_offset: int
    size: int


@dataclass(frozen=True)
class TrgNode:
    index: int
    offset: int
    size: int
    node_type: int

    @property
    def type_name(self) -> str:
        return _TRG_NODE_NAMES.get(self.node_type, "unknown")


@dataclass(frozen=True)
class HedEntry:
    name_hash: int
    offset: int
    size: int
    name: str | None

    @property
    def output_name(self) -> str:
        return self.name or f"hash_{self.name_hash:08x}.bin"


@dataclass(frozen=True)
class NamedTableEntry:
    name: str
    offset: int
    size: int


@dataclass(frozen=True)
class PsxFace:
    base_flags: int
    vertex_indices: tuple[int, int, int, int]
    normal_index: int
    surface_flags: int
    texture_index: int | None
    uvs: tuple[tuple[int, int], ...]


@dataclass(frozen=True)
class PsxObject:
    index: int
    flags: int
    position: tuple[int, int, int]
    unknown_1: int
    unknown_2: int
    model_index: int
    unknown_x: int
    unknown_y: int
    unknown_3: int
    unknown_rgbx: int


@dataclass(frozen=True)
class PsxBlockmapCell:
    index: int
    x: int
    z: int
    unknown_1: int
    unknown_2: int
    object_indices: tuple[int, ...]


@dataclass(frozen=True)
class PsxBlockmap:
    tag_offset: int
    bounds: tuple[int, int, int, int]
    cell_counts: tuple[int, int]
    cells: tuple[PsxBlockmapCell, ...]


@dataclass(frozen=True)
class PsxModel:
    index: int
    offset: int
    size: int
    flags: int
    vertex_count: int
    normal_count: int
    face_count: int
    bounds: tuple[int, int, int, int, int, int]
    face_flags: tuple[int, ...]
    vertices: tuple[tuple[int, int, int], ...] = ()
    normals: tuple[tuple[int, int, int], ...] = ()
    faces: tuple[PsxFace, ...] = ()


@dataclass(frozen=True)
class PsxTag:
    offset: int
    tag_type: int
    size: int

    @property
    def type_name(self) -> str:
        return {0x0000000A: "blockmap", 0x73424752: "rgbs"}.get(self.tag_type, "unknown")


@dataclass(frozen=True)
class PsxTexture:
    index: int
    offset: int
    flags: int
    color_count: int
    palette_name: int
    name_index: int
    width: int
    height: int
    data_offset: int
    available_size: int

    @property
    def aligned_width(self) -> int:
        alignment = {16: 3, 256: 1, 65536: 0}[self.color_count]
        return (self.width + alignment) & ~alignment

    @property
    def aligned_height(self) -> int:
        alignment = {16: 3, 256: 1, 65536: 0}[self.color_count]
        return (self.height + alignment) & ~alignment

    @property
    def expected_data_size(self) -> int:
        if self.color_count == 16:
            return self.aligned_width * self.aligned_height // 2
        if self.color_count == 256:
            return self.aligned_width * self.aligned_height
        return self.aligned_width * self.aligned_height * 2


class _PsxCursor:
    def __init__(self, data: bytes, position: int = 0):
        self.data = data
        self.position = position

    def take(self, size: int, label: str) -> bytes:
        end = self.position + size
        if size < 0 or end > len(self.data):
            raise PsxFormatError(
                f"truncated {label}: need 0x{size:x} bytes at 0x{self.position:x}"
            )
        result = self.data[self.position:end]
        self.position = end
        return result

    def u16(self, label: str) -> int:
        return struct.unpack("<H", self.take(2, label))[0]

    def i16(self, label: str) -> int:
        return struct.unpack("<h", self.take(2, label))[0]

    def u32(self, label: str) -> int:
        return struct.unpack("<I", self.take(4, label))[0]

    def i32(self, label: str) -> int:
        return struct.unpack("<i", self.take(4, label))[0]

    def seek(self, position: int, label: str) -> None:
        if position < 0 or position > len(self.data):
            raise PsxFormatError(f"{label} seeks outside the file: 0x{position:x}")
        self.position = position


def _decode_name(raw: bytes, label: str) -> str:
    value = raw.split(b"\0", 1)[0]
    if not value:
        raise PkrFormatError(f"empty {label}")
    try:
        return value.decode("ascii")
    except UnicodeDecodeError:
        return value.decode("latin-1")


def _read_exact(stream, size: int, label: str) -> bytes:
    data = stream.read(size)
    if len(data) != size:
        raise PkrFormatError(f"truncated {label}: expected {size} bytes, got {len(data)}")
    return data


def _safe_archive_path(directory: str, name: str) -> tuple[str, ...]:
    archive_path = f"{directory.rstrip('/')}/{name}" if directory else name
    if "\\" in archive_path or "\0" in archive_path:
        raise PkrFormatError(f"unsafe archive path: {archive_path!r}")

    path = PurePosixPath(archive_path)
    if path.is_absolute() or any(part in {"", ".", ".."} for part in path.parts):
        raise PkrFormatError(f"unsafe archive path: {archive_path!r}")
    return path.parts


def _filename_crc32(data: bytes, start: int = 0xFFFFFFFF) -> int:
    """Match the non-reflected CRC used by the Neversoft filename tables."""

    result = start
    for byte in data:
        mask = result ^ byte
        for _ in range(8):
            result = ((result << 1) | (result >> 31)) & 0xFFFFFFFF
            if mask & 1:
                result ^= 0xEDB88320
            mask >>= 1
    return result


class PkrArchive:
    """Reader for the PKR2 archive found in the THPS2 PC installer.

    The observed layout is a little-endian header, a 40-byte directory table,
    and 48-byte file records. File payload offsets point to absolute positions
    in the archive rather than positions relative to a data section.
    """

    def __init__(self, path: Path, version: int, directories: list[PkrDirectory], entries: list[PkrEntry]):
        self.path = path
        self.version = version
        self.directories = directories
        self.entries = entries

    @classmethod
    def read(cls, path: str | Path) -> PkrArchive:
        source = resolve(path)
        if not source.is_file():
            raise FileNotFoundError(source)
        file_size = source.stat().st_size

        with source.open("rb") as stream:
            magic, version, directory_count, file_count = _PKR_HEADER.unpack(
                _read_exact(stream, _PKR_HEADER.size, "PKR header")
            )
            if magic != _PKR_MAGIC:
                raise PkrFormatError(f"unsupported PKR magic: {magic!r}")

            directory_table_end = _PKR_HEADER.size + directory_count * _PKR_DIRECTORY.size
            if directory_table_end > file_size:
                raise PkrFormatError("directory table extends beyond the archive")

            directories: list[PkrDirectory] = []
            for index in range(directory_count):
                raw_name, entries_offset, entry_count = _PKR_DIRECTORY.unpack(
                    _read_exact(stream, _PKR_DIRECTORY.size, f"directory record {index}")
                )
                name = _decode_name(raw_name, f"directory name {index}")
                entries_end = entries_offset + entry_count * _PKR_FILE.size
                if entries_offset < directory_table_end or entries_end > file_size:
                    raise PkrFormatError(
                        f"directory {name!r} file table is outside the archive: "
                        f"0x{entries_offset:x}-0x{entries_end:x}"
                    )
                directories.append(PkrDirectory(name, entries_offset, entry_count))

            entries: list[PkrEntry] = []
            seen_paths: set[str] = set()
            for directory in directories:
                stream.seek(directory.entries_offset)
                for index in range(directory.file_count):
                    raw_name, marker, data_offset, size, size_again = _PKR_FILE.unpack(
                        _read_exact(
                            stream,
                            _PKR_FILE.size,
                            f"file record {directory.name}{index}",
                        )
                    )
                    if marker != _PKR_FILE_MARKER:
                        raise PkrFormatError(
                            f"unexpected marker in {directory.name}{index}: 0x{marker:08x}"
                        )
                    if size != size_again:
                        raise PkrFormatError(
                            f"size mismatch in {directory.name}{index}: {size} != {size_again}"
                        )
                    name = _decode_name(raw_name, f"file name {directory.name}{index}")
                    archive_parts = _safe_archive_path(directory.name, name)
                    archive_path = "/".join(archive_parts)
                    if archive_path in seen_paths:
                        raise PkrFormatError(f"duplicate archive path: {archive_path}")
                    seen_paths.add(archive_path)
                    data_end = data_offset + size
                    if data_offset > file_size or data_end > file_size:
                        raise PkrFormatError(
                            f"payload for {archive_path!r} is outside the archive: "
                            f"0x{data_offset:x}-0x{data_end:x}"
                        )
                    entries.append(PkrEntry(directory.name, name, data_offset, size))

            if len(entries) != file_count:
                raise PkrFormatError(
                    f"header declares {file_count} files but directory tables contain {len(entries)}"
                )

        return cls(source, version, directories, entries)

    def summary(self) -> dict:
        return {
            "format": "PKR2",
            "version": self.version,
            "directory_count": len(self.directories),
            "file_count": len(self.entries),
            "directory_record_size": _PKR_DIRECTORY.size,
            "file_record_size": _PKR_FILE.size,
            "payload_offsets": "absolute",
            "directories": [
                {
                    "path": directory.name,
                    "entries_offset": directory.entries_offset,
                    "file_count": directory.file_count,
                }
                for directory in self.directories
            ],
        }


def _read_cstring(stream, file_size: int, label: str) -> str:
    value = bytearray()
    while stream.tell() < file_size:
        character = stream.read(1)
        if character == b"\0":
            if not value:
                raise PkrFormatError(f"empty {label}")
            return _decode_name(bytes(value), label)
        value.extend(character)
        if len(value) > 4096:
            raise PkrFormatError(f"{label} exceeds 4096 bytes")
    raise PkrFormatError(f"unterminated {label}")


class PreArchive:
    """Reader for the inline resource container used by THPS2 ``.PRE`` files."""

    def __init__(self, path: Path, entries: list[PreEntry]):
        self.path = path
        self.entries = entries

    @classmethod
    def read(cls, path: str | Path) -> PreArchive:
        source = resolve(path)
        if not source.is_file():
            raise FileNotFoundError(source)
        file_size = source.stat().st_size
        with source.open("rb") as stream:
            (file_count,) = _PRE_HEADER.unpack(_read_exact(stream, _PRE_HEADER.size, "PRE header"))
            entries: list[PreEntry] = []
            seen_names: set[str] = set()
            for index in range(file_count):
                name = _read_cstring(stream, file_size, f"PRE name {index}")
                aligned_offset = (stream.tell() + 3) & ~3
                if aligned_offset > file_size:
                    raise PkrFormatError(f"PRE record {name!r} aligns beyond the file")
                stream.seek(aligned_offset)
                (size,) = _PRE_HEADER.unpack(_read_exact(stream, _PRE_HEADER.size, f"PRE size {name!r}"))
                data_offset = stream.tell()
                data_end = data_offset + size
                if data_end > file_size:
                    raise PkrFormatError(
                        f"payload for {name!r} is outside the PRE file: "
                        f"0x{data_offset:x}-0x{data_end:x}"
                    )
                _safe_archive_path("", name)
                if name in seen_names:
                    raise PkrFormatError(f"duplicate PRE path: {name}")
                seen_names.add(name)
                entries.append(PreEntry(name, data_offset, size))
                stream.seek((data_end + 3) & ~3)

        return cls(source, entries)

    def summary(self) -> dict:
        return {
            "format": "PRE",
            "file_count": len(self.entries),
            "name_encoding": "ASCII with Latin-1 fallback",
            "record_alignment": 4,
            "payloads": "inline",
        }


class TrgArchive:
    """Reader for the common TRG2 node table, without decoding node payloads."""

    def __init__(self, path: Path, version: int, nodes: list[TrgNode]):
        self.path = path
        self.version = version
        self.nodes = nodes

    @classmethod
    def read(cls, path: str | Path) -> TrgArchive:
        source = resolve(path)
        if not source.is_file():
            raise FileNotFoundError(source)
        file_size = source.stat().st_size
        with source.open("rb") as stream:
            magic, version, node_count = _TRG_HEADER.unpack(
                _read_exact(stream, _TRG_HEADER.size, "TRG header")
            )
            if magic != _TRG_MAGIC:
                raise PkrFormatError(f"unsupported TRG magic: {magic!r}")
            table_end = _TRG_HEADER.size + node_count * 4
            if table_end > file_size:
                raise PkrFormatError("TRG node-offset table extends beyond the file")
            offsets = [
                struct.unpack("<I", _read_exact(stream, 4, f"TRG node offset {index}"))[0]
                for index in range(node_count)
            ]
            if offsets != sorted(set(offsets)):
                raise PkrFormatError("TRG node offsets must be strictly increasing")
            if any(offset < table_end or offset + 2 > file_size for offset in offsets):
                raise PkrFormatError("TRG node offset is outside the file")

            nodes = []
            for index, offset in enumerate(offsets):
                next_offset = offsets[index + 1] if index + 1 < len(offsets) else file_size
                if next_offset <= offset:
                    raise PkrFormatError(f"TRG node {index} has a non-positive size")
                stream.seek(offset)
                node_type = struct.unpack("<H", _read_exact(stream, 2, f"TRG node {index} type"))[0]
                nodes.append(TrgNode(index, offset, next_offset - offset, node_type))

        return cls(source, version, nodes)

    def summary(self) -> dict:
        counts = Counter(node.node_type for node in self.nodes)
        return {
            "format": "TRG",
            "version": self.version,
            "node_count": len(self.nodes),
            "node_types": [
                {
                    "type": node_type,
                    "name": _TRG_NODE_NAMES.get(node_type, "unknown"),
                    "count": count,
                }
                for node_type, count in sorted(counts.items())
            ],
            "unknown_node_types": sorted(node_type for node_type in counts if node_type not in _TRG_NODE_NAMES),
        }


def _trg_align_down4(offset: int) -> int:
    """Match the retail ``(pointer + bias) & ~3`` expressions."""

    return offset & ~3


def _trg_command_point_key(offset: int, link_count: int) -> int:
    """Return the key position used by the type-6 loader.

    The original advances a ushort pointer past the links and adds one
    ushort only when the resulting pointer has bit 1 set.  Expressing that
    directly is safer than applying a generic round-up operation to an
    absolute file offset.
    """

    key = offset + 4 + link_count * 2
    if key & 2:
        key += 2
    return key


def _trg_string(data: bytes, offset: int, end: int) -> tuple[str, int]:
    terminator = data.find(b"\0", offset, end)
    if terminator < 0:
        raise ValueError("unterminated string operand")
    raw = data[offset:terminator]
    try:
        value = raw.decode("ascii")
    except UnicodeDecodeError:
        value = raw.decode("latin-1")
    # This is the byte-pointer alignment used by FUN_004c5d70.
    return value, (terminator + 2) & ~1


def _trg_script_start(data: bytes, node: TrgNode) -> int | None:
    """Return the command stream start for the node layouts used by retail PC."""

    offset = node.offset
    end = offset + node.size
    if node.node_type in (4, 15):
        return offset + 2
    if node.node_type == 6:
        if offset + 4 > end:
            raise ValueError("truncated command-point header")
        link_count = struct.unpack_from("<H", data, offset + 2)[0]
        key = _trg_command_point_key(offset, link_count)
        if key + 4 > end:
            raise ValueError("truncated command-point key")
        return key + 4
    if node.node_type == 8:
        if offset + 4 > end:
            raise ValueError("truncated restart header")
        link_count = struct.unpack_from("<H", data, offset + 2)[0]
        # FUN_004c8650 aligns the position triplet from the absolute node
        # pointer.  The returned position pointer is +0x0c, and the restart
        # name begins six bytes after that return value.
        position = (offset + link_count * 2 + 7) & ~3
        name = position + 18
        _, stream = _trg_string(data, name, end)
        return stream
    return None


def _trg_skip_command(data: bytes, offset: int, end: int, opcode: int) -> int:
    """Apply the retail dispatcher operand widths and return the next command."""

    if opcode == 2:
        cursor = offset + 2
        while cursor < end and data[cursor] != 0:
            _, cursor = _trg_string(data, cursor, end)
        if cursor >= end:
            raise ValueError("unterminated string-list operand")
        return cursor + 2

    one_word = {
        3, 4, 5, 10, 11, 12, 0x66, 0x67, 0x69, 0x6A, 0x79, 0x7A,
        0x81, 0x88, 0x89, 0x95, 0x98, 0x9E, 0xAD, 0xAF,
    }
    two_words = {
        0x0D, 0x83, 0x84, 0x86, 0x8A, 0x93, 0x94, 0x96, 0x97,
        0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0xA0, 0xA1, 0xA3,
        0xA4, 0xA5, 0xA6, 0xA8, 0xA9, 0xAA, 0xAC, 0xB1, 0xCB,
        0xCC, 0xCD,
    }
    strings = {0x73, 0x77, 0x7E, 0x7F, 0x80, 0x8C, 0x8E, 0x9F, 0xA2, 0xB0, 0xB2}
    three_words = {0x82, 0x87, 0x8B, 0x8F, 0x90, 0x91, 0x92, 0xA7, 0xAE, 0xC8, 0xCA}

    if opcode in one_word:
        next_offset = offset + 2
    elif opcode in two_words:
        next_offset = offset + 4
    elif opcode == 0x68:
        next_offset = offset + 8
    elif opcode in strings:
        _, next_offset = _trg_string(data, offset + 2, end)
    elif opcode in three_words:
        next_offset = offset + 6
    elif opcode == 0x8D:
        # The table's terminator is tested as a byte by the original code,
        # despite Ghidra rendering the temporary as ushort.
        cursor = offset + 6
        while cursor < end and data[cursor] != 0xFF:
            record = _trg_align_down4(cursor + 3)
            if record + 0x18 > end:
                raise ValueError("truncated 0x8d path record")
            cursor = record + 0x18
        if cursor >= end:
            raise ValueError("unterminated 0x8d path table")
        next_offset = cursor + 2
    elif opcode == 0x85:
        cursor = offset + 2
        while cursor < end and data[cursor] != 0xFF:
            record = _trg_align_down4(cursor + 3)
            if record + 0x18 > end:
                raise ValueError("truncated 0x85 path record")
            cursor = record + 0x18
        if cursor >= end:
            raise ValueError("unterminated 0x85 path table")
        next_offset = cursor + 2
    elif opcode == 0xAB:
        next_offset = _trg_align_down4(offset + 5) + 10
    elif opcode == 0xC9:
        next_offset = _trg_align_down4(offset + 5) + 6
    else:
        raise ValueError(f"unknown opcode 0x{opcode:04x}")

    if next_offset > end:
        raise ValueError("command operand extends beyond node")
    return next_offset


def _trg_command_record(data: bytes, offset: int, next_offset: int, opcode: int) -> dict:
    record = {
        "offset": offset,
        "opcode": opcode,
        "name": _TRG_COMMAND_NAMES.get(opcode, "unknown"),
        "size": next_offset - offset,
        "raw": data[offset:next_offset].hex(),
    }
    if opcode == 0xC9:
        aligned = _trg_align_down4(offset + 5)
        record["checksum"] = struct.unpack_from("<I", data, aligned)[0]
        record["argument"] = struct.unpack_from("<H", data, aligned + 4)[0]
    elif opcode == 0xAB:
        aligned = _trg_align_down4(offset + 5)
        record["key"] = struct.unpack_from("<I", data, aligned)[0]
        record["arguments"] = [
            struct.unpack_from("<H", data, aligned + cursor)[0]
            for cursor in (4, 6, 8)
        ]
    elif opcode == 0x8D:
        record["arguments"] = [
            struct.unpack_from("<H", data, offset + 2)[0],
            struct.unpack_from("<H", data, offset + 4)[0],
        ]
    elif opcode in {0x73, 0x77, 0x7E, 0x7F, 0x80, 0x8C, 0x8E, 0x9F, 0xA2, 0xB0, 0xB2}:
        value, _ = _trg_string(data, offset + 2, next_offset)
        record["string"] = value
    elif next_offset - offset >= 4:
        record["arguments"] = [
            struct.unpack_from("<H", data, cursor)[0]
            for cursor in range(offset + 2, next_offset, 2)
        ]
    return record


def _trg_decode_script(data: bytes, node: TrgNode) -> dict | None:
    start = _trg_script_start(data, node)
    if start is None:
        return None
    end = node.offset + node.size
    result = {"stream_offset": start, "commands": []}
    cursor = start
    while cursor + 2 <= end:
        opcode = struct.unpack_from("<H", data, cursor)[0]
        if opcode == 0xFFFF:
            result["terminator_offset"] = cursor
            result["terminated"] = True
            return result
        try:
            next_offset = _trg_skip_command(data, cursor, end, opcode)
        except ValueError as error:
            result["commands"].append(
                {
                    "offset": cursor,
                    "opcode": opcode,
                    "name": _TRG_COMMAND_NAMES.get(opcode, "unknown"),
                    "size": end - cursor,
                    "raw": data[cursor:end].hex(),
                }
            )
            result["terminated"] = False
            result["decode_error"] = str(error)
            return result
        result["commands"].append(_trg_command_record(data, cursor, next_offset, opcode))
        cursor = next_offset
    result["terminated"] = False
    result["decode_error"] = "command stream ends without a terminator"
    return result


_PSX_TAG_NAMES = {0x0000000A: "blockmap", 0x73424752: "rgbs"}


class PsxArchive:
    """Conservative reader for the THPS2 PSX model/texture container."""

    def __init__(
        self,
        path: Path,
        version: int,
        marker: int,
        tag_offset: int,
        objects: list[PsxObject],
        models: list[PsxModel],
        model_names: list[int],
        tags: list[PsxTag],
        blockmaps: list[PsxBlockmap],
        tag_terminated: bool,
        texture_name_count: int,
        texture_names: list[int],
        palette4_count: int,
        palettes4: dict[int, tuple[int, ...]],
        palette8_count: int,
        palettes8: dict[int, tuple[int, ...]],
        textures: list[PsxTexture],
        texture_table_offset: int | None,
        metadata_end: int,
    ):
        self.path = path
        self.version = version
        self.marker = marker
        self.tag_offset = tag_offset
        self.objects = objects
        self.object_model_indices = [object_.model_index for object_ in objects]
        self.models = models
        self.model_names = model_names
        self.tags = tags
        self.blockmaps = blockmaps
        self.tag_terminated = tag_terminated
        self.texture_name_count = texture_name_count
        self.texture_names = texture_names
        self.palette4_count = palette4_count
        self.palettes4 = palettes4
        self.palette8_count = palette8_count
        self.palettes8 = palettes8
        self.textures = textures
        self.texture_table_offset = texture_table_offset
        self.metadata_end = metadata_end

    @classmethod
    def read(cls, path: str | Path) -> PsxArchive:
        source = resolve(path)
        if not source.is_file():
            raise FileNotFoundError(source)
        data = source.read_bytes()
        if len(data) < 12:
            raise PsxFormatError(f"PSX file is shorter than its 12-byte header: {source}")

        cursor = _PsxCursor(data)
        version = cursor.u16("PSX version")
        marker = cursor.u16("PSX marker")
        tag_offset = cursor.u32("PSX tag offset")
        object_count = cursor.u32("PSX object count")
        if version not in {3, 4, 6}:
            raise PsxFormatError(f"unsupported PSX version {version} in {source.name}")
        if marker != 2:
            raise PsxFormatError(f"unsupported PSX marker 0x{marker:04x} in {source.name}")
        if tag_offset > len(data):
            raise PsxFormatError(f"PSX tag offset is outside the file: 0x{tag_offset:x}")
        if object_count > len(data) // 36:
            raise PsxFormatError(f"PSX object table is unreasonably large: {object_count}")

        objects: list[PsxObject] = []
        for index in range(object_count):
            flags = cursor.u32(f"PSX object {index} flags")
            position = tuple(
                cursor.i32(f"PSX object {index} {axis} position") for axis in ("x", "y", "z")
            )
            unknown_1 = cursor.u32(f"PSX object {index} unknown 1")
            unknown_2 = cursor.u16(f"PSX object {index} unknown 2")
            model_index = cursor.u16(f"PSX object {index} model index")
            unknown_x = cursor.i16(f"PSX object {index} unknown x")
            unknown_y = cursor.i16(f"PSX object {index} unknown y")
            unknown_3 = cursor.u32(f"PSX object {index} unknown 3")
            unknown_rgbx = cursor.u32(f"PSX object {index} unknown RGBX")
            objects.append(
                PsxObject(
                    index,
                    flags,
                    position,
                    unknown_1,
                    unknown_2,
                    model_index,
                    unknown_x,
                    unknown_y,
                    unknown_3,
                    unknown_rgbx,
                )
            )

        model_count = cursor.u32("PSX model count")
        if model_count > len(data) // 4:
            raise PsxFormatError(f"PSX model table is unreasonably large: {model_count}")
        model_offsets = [cursor.u32(f"PSX model offset {index}") for index in range(model_count)]
        if model_offsets != sorted(set(model_offsets)):
            raise PsxFormatError("PSX model offsets must be strictly increasing")
        if any(offset < cursor.position or offset >= len(data) for offset in model_offsets):
            raise PsxFormatError("PSX model offset is outside the file")
        if any(object_.model_index >= model_count for object_ in objects):
            raise PsxFormatError("PSX object references a model outside the model table")

        models: list[PsxModel] = []
        for index, offset in enumerate(model_offsets):
            model_end = model_offsets[index + 1] if index + 1 < len(model_offsets) else tag_offset
            if model_end <= offset:
                raise PsxFormatError(f"PSX model {index} has an invalid boundary")
            cursor.seek(offset, f"PSX model {index}")
            if version >= 4:
                flags = cursor.u16(f"PSX model {index} flags")
                vertex_count = cursor.u16(f"PSX model {index} vertex count")
                normal_count = cursor.u16(f"PSX model {index} normal count")
                face_count = cursor.u16(f"PSX model {index} face count")
            else:
                flags = cursor.u32(f"PSX model {index} flags")
                vertex_count = cursor.u32(f"PSX model {index} vertex count")
                normal_count = cursor.u32(f"PSX model {index} normal count")
                face_count = cursor.u32(f"PSX model {index} face count")
            cursor.u32(f"PSX model {index} radius")
            bounds = tuple(cursor.i16(f"PSX model {index} bound {bound}") for bound in range(6))
            cursor.u32(f"PSX model {index} unknown header")

            vertices = []
            for vertex_index in range(vertex_count):
                vertices.append(
                    (
                        cursor.i16(f"PSX model {index} vertex {vertex_index} x"),
                        cursor.i16(f"PSX model {index} vertex {vertex_index} y"),
                        cursor.i16(f"PSX model {index} vertex {vertex_index} z"),
                    )
                )
                cursor.take(2, f"PSX model {index} vertex {vertex_index} padding")
            normals = []
            for normal_index in range(normal_count):
                normals.append(
                    (
                        cursor.i16(f"PSX model {index} normal {normal_index} x"),
                        cursor.i16(f"PSX model {index} normal {normal_index} y"),
                        cursor.i16(f"PSX model {index} normal {normal_index} z"),
                    )
                )
                cursor.take(2, f"PSX model {index} normal {normal_index} padding")

            face_flags: list[int] = []
            faces: list[PsxFace] = []
            for face_index in range(face_count):
                face_start = cursor.position
                base_flags = cursor.u16(f"PSX model {index} face {face_index} flags")
                face_length = cursor.u16(f"PSX model {index} face {face_index} length")
                face_end = face_start + face_length
                if face_length < 4 or face_end > model_end:
                    raise PsxFormatError(f"PSX model {index} face {face_index} has an invalid length")

                if version >= 4:
                    vertex_indices = tuple(
                        cursor.take(1, f"PSX model {index} face {face_index} vertex index")[0]
                        for _ in range(4)
                    )
                else:
                    vertex_indices = tuple(
                        cursor.u16(f"PSX model {index} face {face_index} vertex index")
                        for _ in range(4)
                    )
                if any(vertex_index >= vertex_count for vertex_index in vertex_indices):
                    raise PsxFormatError(f"PSX model {index} face {face_index} references a missing vertex")
                cursor.take(4, f"PSX model {index} face {face_index} GPU command")
                normal_index = cursor.u16(f"PSX model {index} face {face_index} normal index")
                if normal_index >= normal_count:
                    raise PsxFormatError(f"PSX model {index} face {face_index} references a missing normal")
                surface_flags = cursor.u16(f"PSX model {index} face {face_index} surface flags")
                texture_index = None
                if flags & 1 == 0 and base_flags & 2:
                    texture_index = cursor.u32(f"PSX model {index} face {face_index} texture index")
                uvs: tuple[tuple[int, int], ...] = ()
                if base_flags & 1:
                    if version >= 6:
                        u_values = tuple(
                            cursor.u16(f"PSX model {index} face {face_index} U coordinate")
                            for _ in range(4)
                        )
                        v_values = tuple(
                            cursor.u16(f"PSX model {index} face {face_index} V coordinate")
                            for _ in range(4)
                        )
                    else:
                        u_values = tuple(
                            cursor.take(1, f"PSX model {index} face {face_index} U coordinate")[0]
                            for _ in range(4)
                        )
                        v_values = tuple(
                            cursor.take(1, f"PSX model {index} face {face_index} V coordinate")[0]
                            for _ in range(4)
                        )
                    uvs = tuple(zip(u_values, v_values))
                if base_flags & 8:
                    cursor.take(8, f"PSX model {index} face {face_index} extra data")
                if flags & 1 == 0 and base_flags & 0x20:
                    cursor.take(4, f"PSX model {index} face {face_index} trailing data")
                if cursor.position > face_end:
                    raise PsxFormatError(f"PSX model {index} face {face_index} fields exceed its length")
                cursor.seek(face_end, f"PSX model {index} face {face_index} end")
                face_flags.append(base_flags)
                faces.append(PsxFace(base_flags, vertex_indices, normal_index, surface_flags, texture_index, uvs))

            if cursor.position > model_end:
                raise PsxFormatError(f"PSX model {index} exceeds its boundary")
            models.append(
                PsxModel(
                    index,
                    offset,
                    model_end - offset,
                    flags,
                    vertex_count,
                    normal_count,
                    face_count,
                    bounds,
                    tuple(face_flags),
                    tuple(vertices),
                    tuple(normals),
                    tuple(faces),
                )
            )

        tags, tag_terminated, post_tables_offset = cls._read_tags(data, tag_offset)
        texture_name_count = 0
        texture_names: list[int] = []
        palette4_count = 0
        palettes4: dict[int, tuple[int, ...]] = {}
        palette8_count = 0
        palettes8: dict[int, tuple[int, ...]] = {}
        textures: list[PsxTexture] = []
        texture_table_offset: int | None = None
        metadata_end = post_tables_offset
        model_names: list[int] = []
        if post_tables_offset < len(data) and model_count:
            post = _PsxCursor(data, post_tables_offset)
            model_names = [post.u32(f"PSX model name {index}") for index in range(model_count)]
            texture_name_count = post.u32("PSX texture name count")
            texture_names = [post.u32(f"PSX texture name {index}") for index in range(texture_name_count)]
            palette4_count = post.u32("PSX 4bpp palette count")
            for index in range(palette4_count):
                name = post.u32(f"PSX 4bpp palette {index} name")
                palettes4[name] = tuple(
                    post.u16(f"PSX 4bpp palette {index} color {color}") for color in range(16)
                )
            palette8_count = post.u32("PSX 8bpp palette count")
            for index in range(palette8_count):
                name = post.u32(f"PSX 8bpp palette {index} name")
                palettes8[name] = tuple(
                    post.u16(f"PSX 8bpp palette {index} color {color}") for color in range(256)
                )
            texture_count = post.u32("PSX texture count")
            if version >= 6 and texture_count == 0xFFFFFFFF:
                reference_count = post.u32("PSX texture reference count")
                post.take(reference_count * 36, "PSX texture references")
                cubemap_count = post.u32("PSX cubemap reference count")
                post.take(cubemap_count * 36, "PSX cubemap references")
                texture_count = post.u32("PSX texture count after references")
            if texture_count > len(data) // 4:
                raise PsxFormatError(f"PSX texture table is unreasonably large: {texture_count}")
            texture_table_offset = post.position
            texture_offsets = [post.u32(f"PSX texture offset {index}") for index in range(texture_count)]
            if texture_offsets != sorted(set(texture_offsets)):
                raise PsxFormatError("PSX texture offsets must be strictly increasing")
            if any(offset + 20 > len(data) for offset in texture_offsets):
                raise PsxFormatError("PSX texture offset is outside the file")
            for index, offset in enumerate(texture_offsets):
                texture_cursor = _PsxCursor(data, offset)
                flags = texture_cursor.u32(f"PSX texture {index} flags")
                color_count = texture_cursor.u32(f"PSX texture {index} color count")
                palette_name = texture_cursor.u32(f"PSX texture {index} palette")
                name_index = texture_cursor.u32(f"PSX texture {index} name index")
                width = texture_cursor.u16(f"PSX texture {index} width")
                height = texture_cursor.u16(f"PSX texture {index} height")
                if color_count not in {16, 256, 65536}:
                    raise PsxFormatError(f"PSX texture {index} has unsupported color count {color_count}")
                if name_index >= texture_name_count:
                    raise PsxFormatError(f"PSX texture {index} references a missing texture name")
                next_offset = texture_offsets[index + 1] if index + 1 < len(texture_offsets) else len(data)
                textures.append(
                    PsxTexture(
                        index,
                        offset,
                        flags,
                        color_count,
                        palette_name,
                        name_index,
                        width,
                        height,
                        offset + 20,
                        max(0, next_offset - (offset + 20)),
                    )
                )
            metadata_end = max(
                [post.position, *(texture.offset + 20 for texture in textures)],
                default=post.position,
            )

        return cls(
            source,
            version,
            marker,
            tag_offset,
            objects,
            models,
            model_names,
            tags,
            [
                cls._read_blockmap(data, tag, len(objects))
                for tag in tags
                if tag.type_name == "blockmap" and tag.size >= 36
            ],
            tag_terminated,
            texture_name_count,
            texture_names,
            palette4_count,
            palettes4,
            palette8_count,
            palettes8,
            textures,
            texture_table_offset,
            metadata_end,
        )

    @staticmethod
    def _read_tags(data: bytes, offset: int) -> tuple[list[PsxTag], bool, int]:
        if offset == len(data):
            return [], False, offset
        cursor = _PsxCursor(data, offset)
        tags: list[PsxTag] = []
        while cursor.position < len(data):
            tag_offset = cursor.position
            tag_type = cursor.u32("PSX tag type")
            if tag_type == 0xFFFFFFFF:
                return tags, True, cursor.position
            tag_size = cursor.u32("PSX tag size")
            tag_end = cursor.position + tag_size
            if tag_end > len(data):
                raise PsxFormatError(f"PSX tag at 0x{tag_offset:x} exceeds the file")
            cursor.seek(tag_end, "PSX tag end")
            tags.append(PsxTag(tag_offset, tag_type, tag_size))
        return tags, False, cursor.position

    @staticmethod
    def _read_blockmap(data: bytes, tag: PsxTag, object_count: int) -> PsxBlockmap:
        payload_start = tag.offset + 8
        payload_end = payload_start + tag.size
        payload = data[payload_start:payload_end]
        cursor = _PsxCursor(payload)
        bounds = tuple(cursor.i32(f"PSX blockmap bound {index}") for index in range(4))
        xcell_count = cursor.u16("PSX blockmap X cell count")
        zcell_count = cursor.u16("PSX blockmap Z cell count")
        if not xcell_count or not zcell_count:
            raise PsxFormatError(f"PSX blockmap at 0x{tag.offset:x} has an empty grid")
        cell_count = xcell_count * zcell_count
        if cell_count > len(payload) // 16:
            raise PsxFormatError(f"PSX blockmap at 0x{tag.offset:x} has an unreasonable cell count")

        cells: list[PsxBlockmapCell] = []
        for index in range(cell_count):
            unknown_1 = cursor.u32(f"PSX blockmap cell {index} unknown 1")
            unknown_2 = cursor.u32(f"PSX blockmap cell {index} unknown 2")
            reference_count = cursor.u32(f"PSX blockmap cell {index} object count")
            if reference_count > (len(payload) - cursor.position) // 4:
                raise PsxFormatError(f"PSX blockmap cell {index} has an unreasonable object count")
            object_indices = tuple(
                cursor.u32(f"PSX blockmap cell {index} object {object_index}")
                for object_index in range(reference_count)
            )
            terminator = cursor.u32(f"PSX blockmap cell {index} terminator")
            if terminator != 0:
                raise PsxFormatError(f"PSX blockmap cell {index} has a non-zero terminator")
            if any(object_index >= object_count for object_index in object_indices):
                raise PsxFormatError(f"PSX blockmap cell {index} references a missing object")
            cells.append(
                PsxBlockmapCell(
                    index,
                    index % xcell_count,
                    index // xcell_count,
                    unknown_1,
                    unknown_2,
                    object_indices,
                )
            )
        if cursor.position != len(payload):
            raise PsxFormatError(
                f"PSX blockmap at 0x{tag.offset:x} has {len(payload) - cursor.position} trailing bytes"
            )
        return PsxBlockmap(tag.offset, bounds, (xcell_count, zcell_count), tuple(cells))

    def summary(self) -> dict:
        face_counts = Counter(flag for model in self.models for flag in model.face_flags)
        texture_colors = Counter(texture.color_count for texture in self.textures)
        return {
            "format": "PSX",
            "version": self.version,
            "marker": self.marker,
            "tag_offset": self.tag_offset,
            "tag_count": len(self.tags),
            "tag_terminated": self.tag_terminated,
            "unknown_tag_types": sorted(tag.tag_type for tag in self.tags if tag.type_name == "unknown"),
            "object_count": len(self.object_model_indices),
            "model_count": len(self.models),
            "model_name_count": len(self.model_names),
            "blockmap_count": len(self.blockmaps),
            "blockmap_cell_count": sum(
                blockmap.cell_counts[0] * blockmap.cell_counts[1] for blockmap in self.blockmaps
            ),
            "blockmap_object_references": sum(
                len(cell.object_indices) for blockmap in self.blockmaps for cell in blockmap.cells
            ),
            "vertex_count": sum(model.vertex_count for model in self.models),
            "normal_count": sum(model.normal_count for model in self.models),
            "face_count": sum(model.face_count for model in self.models),
            "face_flags": [
                {"flags": flags, "count": count} for flags, count in sorted(face_counts.items())
            ],
            "texture_name_count": self.texture_name_count,
            "palette4_count": self.palette4_count,
            "palette8_count": self.palette8_count,
            "texture_count": len(self.textures),
            "texture_color_counts": [
                {"colors": colors, "count": count}
                for colors, count in sorted(texture_colors.items())
            ],
            "texture_table_offset": self.texture_table_offset,
            "metadata_end": self.metadata_end,
            "metadata_trailing_bytes": max(0, self.path.stat().st_size - self.metadata_end),
            "texture_payloads": "opaque",
        }


def _decode_hed_name(raw: bytes, label: str) -> str:
    if not raw:
        raise HedFormatError(f"empty {label}")
    try:
        return raw.decode("ascii")
    except UnicodeDecodeError:
        return raw.decode("latin-1")


def _read_named_table(path: str | Path) -> list[NamedTableEntry]:
    """Read the variable-length filename/offset/size table used by HET/HEP."""

    source = resolve(path)
    if not source.is_file():
        raise FileNotFoundError(source)
    data = source.read_bytes()
    entries: list[NamedTableEntry] = []
    position = 0
    while position < len(data):
        if data[position : position + 4] == _NAMED_TABLE_TERMINATOR:
            if position + 4 != len(data):
                raise HedFormatError(f"trailing bytes after {source.name} terminator")
            return entries

        name_end = data.find(b"\0", position)
        if name_end < 0:
            raise HedFormatError(f"unterminated filename in {source.name} at 0x{position:x}")
        name = _decode_hed_name(data[position:name_end], f"{source.name} filename")
        record_position = (name_end + 1 + 3) & ~3
        if record_position > len(data) or record_position + _NAMED_TABLE_RECORD.size > len(data):
            raise HedFormatError(f"truncated {source.name} record for {name!r}")
        if any(data[name_end + 1 : record_position]):
            raise HedFormatError(f"non-zero alignment padding after {name!r} in {source.name}")
        offset, size = _NAMED_TABLE_RECORD.unpack_from(data, record_position)
        entries.append(NamedTableEntry(name, offset, size))
        position = record_position + _NAMED_TABLE_RECORD.size

    raise HedFormatError(f"missing {source.name} terminator")


def _read_hashed_table(path: str | Path) -> list[tuple[int, int, int]]:
    source = resolve(path)
    if not source.is_file():
        raise FileNotFoundError(source)
    data = source.read_bytes()
    if len(data) < len(_HED_TERMINATOR) or data[-4:] != _HED_TERMINATOR:
        raise HedFormatError(f"{source.name} is missing its zero terminator")
    table = data[:-4]
    if len(table) % _HED_RECORD.size:
        raise HedFormatError(f"{source.name} has a partial 12-byte record")
    return [_HED_RECORD.unpack_from(table, offset) for offset in range(0, len(table), _HED_RECORD.size)]


def _optional_sidecar(source: Path, requested: str | Path | None, suffix: str) -> Path | None:
    if requested is not None:
        candidate = resolve(requested)
        if not candidate.is_file():
            raise FileNotFoundError(candidate)
        return candidate
    candidate = source.with_suffix(suffix)
    return candidate if candidate.is_file() else None


def _nonzero_bytes(path: Path) -> int:
    count = 0
    with path.open("rb") as stream:
        while chunk := stream.read(1024 * 1024):
            count += sum(byte != 0 for byte in chunk)
    return count


class HedArchive:
    """Reader for the PC CD.HED/CD.HET/CD.HEP/CD.WAD asset family."""

    def __init__(
        self,
        path: Path,
        entries: list[HedEntry],
        named_entries: list[NamedTableEntry],
        secondary_named_entries: list[NamedTableEntry],
        names_path: Path | None,
        secondary_names_path: Path | None,
        wad_path: Path | None,
        wad_size: int,
        wad_nonzero_bytes: int,
        name_hash_matches: int,
        name_matches: int,
    ):
        self.path = path
        self.entries = entries
        self.named_entries = named_entries
        self.secondary_named_entries = secondary_named_entries
        self.names_path = names_path
        self.secondary_names_path = secondary_names_path
        self.wad_path = wad_path
        self.wad_size = wad_size
        self.wad_nonzero_bytes = wad_nonzero_bytes
        self.name_hash_matches = name_hash_matches
        self.name_matches = name_matches

    @classmethod
    def read(
        cls,
        path: str | Path,
        *,
        names_path: str | Path | None = None,
        wad_path: str | Path | None = None,
    ) -> HedArchive:
        source = resolve(path)
        if not source.is_file():
            raise FileNotFoundError(source)

        raw_entries = _read_hashed_table(source)
        resolved_names = _optional_sidecar(source, names_path, ".HET")
        resolved_secondary_names = _optional_sidecar(source, None, ".HEP")
        named_entries = _read_named_table(resolved_names) if resolved_names else []
        secondary_named_entries = (
            _read_named_table(resolved_secondary_names) if resolved_secondary_names else []
        )

        by_hash: dict[int, list[NamedTableEntry]] = {}
        for named_entry in named_entries:
            name_hash = _filename_crc32(named_entry.name.encode("latin-1"))
            by_hash.setdefault(name_hash, []).append(named_entry)

        entries: list[HedEntry] = []
        name_hash_matches = 0
        name_matches = 0
        for name_hash, offset, size in raw_entries:
            candidates = by_hash.get(name_hash, [])
            if candidates:
                name_hash_matches += 1
            exact = [candidate for candidate in candidates if candidate.offset == offset and candidate.size == size]
            name = exact[0].name if len(exact) == 1 else None
            if name is not None:
                name_matches += 1
            entries.append(HedEntry(name_hash, offset, size, name))

        resolved_wad = _optional_sidecar(source, wad_path, ".WAD")
        wad_size = resolved_wad.stat().st_size if resolved_wad else 0
        wad_nonzero_bytes = _nonzero_bytes(resolved_wad) if resolved_wad else 0
        return cls(
            source,
            entries,
            named_entries,
            secondary_named_entries,
            resolved_names,
            resolved_secondary_names,
            resolved_wad,
            wad_size,
            wad_nonzero_bytes,
            name_hash_matches,
            name_matches,
        )

    @property
    def max_referenced_end(self) -> int:
        return max((entry.offset + entry.size for entry in self.entries), default=0)

    @property
    def out_of_bounds_count(self) -> int:
        return sum(entry.offset + entry.size > self.wad_size for entry in self.entries)

    @property
    def range_overlap_count(self) -> int:
        active_end = 0
        overlaps = 0
        for entry in sorted(self.entries, key=lambda item: item.offset):
            if entry.offset < active_end:
                overlaps += 1
            active_end = max(active_end, entry.offset + entry.size)
        return overlaps

    @property
    def offsets_monotonic(self) -> bool:
        return all(left.offset <= right.offset for left, right in zip(self.entries, self.entries[1:]))

    @property
    def unmatched_hashes(self) -> int:
        return len(self.entries) - self.name_hash_matches

    @property
    def metadata_mismatches(self) -> int:
        return self.name_hash_matches - self.name_matches

    @property
    def wad_status(self) -> str:
        if self.wad_path is None:
            return "unavailable"
        if self.wad_size == 0:
            return "empty"
        return "all_zero" if self.wad_nonzero_bytes == 0 else "data"

    def summary(self) -> dict:
        return {
            "format": "HED/WAD",
            "file_count": len(self.entries),
            "hed_record_size": _HED_RECORD.size,
            "named_entry_count": len(self.named_entries),
            "secondary_named_entry_count": len(self.secondary_named_entries),
            "name_hash_matches": self.name_hash_matches,
            "name_matches": self.name_matches,
            "metadata_mismatches": self.metadata_mismatches,
            "unmatched_hashes": self.unmatched_hashes,
            "wad_size": self.wad_size,
            "wad_nonzero_bytes": self.wad_nonzero_bytes,
            "wad_status": self.wad_status,
            "max_referenced_end": self.max_referenced_end,
            "out_of_bounds_count": self.out_of_bounds_count,
            "offsets_monotonic": self.offsets_monotonic,
            "range_overlap_count": self.range_overlap_count,
        }


def _display_path(path: Path) -> str:
    try:
        return relative_to_root(path)
    except ValueError:
        return str(path.resolve())


def inspect_pkr(path: str | Path, *, include_entries: bool = False) -> dict:
    source = resolve(path)
    archive = PkrArchive.read(source)
    result = {
        "source": {
            "path": _display_path(source),
            "size": source.stat().st_size,
            "sha256": sha256(source),
        },
        **archive.summary(),
    }
    if include_entries:
        result["entries"] = [
            {
                "path": entry.archive_path,
                "offset": entry.data_offset,
                "size": entry.size,
            }
            for entry in archive.entries
        ]
    return result


def inspect_pre(path: str | Path, *, include_entries: bool = False) -> dict:
    source = resolve(path)
    archive = PreArchive.read(source)
    result = {
        "source": {
            "path": _display_path(source),
            "size": source.stat().st_size,
            "sha256": sha256(source),
        },
        **archive.summary(),
    }
    if include_entries:
        result["entries"] = [
            {
                "path": entry.name,
                "offset": entry.data_offset,
                "size": entry.size,
            }
            for entry in archive.entries
        ]
    return result


def inspect_trg(
    path: str | Path,
    *,
    include_nodes: bool = False,
    include_scripts: bool = False,
) -> dict:
    source = resolve(path)
    archive = TrgArchive.read(source)
    result = {
        "source": {
            "path": _display_path(source),
            "size": source.stat().st_size,
            "sha256": sha256(source),
        },
        **archive.summary(),
    }
    if include_nodes:
        result["nodes"] = [
            {
                "index": node.index,
                "offset": node.offset,
                "size": node.size,
                "type": node.node_type,
                "name": node.type_name,
            }
            for node in archive.nodes
        ]
    if include_scripts:
        data = source.read_bytes()
        scripts = []
        for node in archive.nodes:
            if node.node_type not in (4, 6, 8, 15):
                continue
            try:
                script = _trg_decode_script(data, node)
            except ValueError as error:
                script = {
                    "stream_offset": None,
                    "commands": [],
                    "terminated": False,
                    "decode_error": str(error),
                }
            scripts.append(
                {
                    "node": node.index,
                    "offset": node.offset,
                    "size": node.size,
                    "type": node.node_type,
                    "name": node.type_name,
                    **script,
                }
            )
        result["scripts"] = scripts
    return result


def inspect_psx(
    path: str | Path,
    *,
    include_models: bool = False,
    include_textures: bool = False,
    include_tags: bool = False,
    include_objects: bool = False,
) -> dict:
    source = resolve(path)
    archive = PsxArchive.read(source)
    result = {
        "source": {
            "path": _display_path(source),
            "size": source.stat().st_size,
            "sha256": sha256(source),
        },
        **archive.summary(),
    }
    if include_models:
        result["models"] = [
            {
                "index": model.index,
                "name": archive.model_names[model.index] if archive.model_names else None,
                "offset": model.offset,
                "size": model.size,
                "flags": model.flags,
                "vertex_count": model.vertex_count,
                "normal_count": model.normal_count,
                "face_count": model.face_count,
                "bounds": model.bounds,
                "face_flags": [
                    {"flags": flags, "count": count}
                    for flags, count in sorted(Counter(model.face_flags).items())
                ],
            }
            for model in archive.models
        ]
    if include_objects:
        result["objects"] = [
            {
                "index": object_.index,
                "flags": object_.flags,
                "model_index": object_.model_index,
                "model_name": archive.model_names[object_.model_index]
                if archive.model_names
                else None,
                "position_fixed": object_.position,
                "position": tuple(round(value / 4096, 6) for value in object_.position),
                "unknown_1": object_.unknown_1,
                "unknown_2": object_.unknown_2,
                "unknown_x": object_.unknown_x,
                "unknown_y": object_.unknown_y,
                "unknown_3": object_.unknown_3,
                "unknown_rgbx": object_.unknown_rgbx,
            }
            for object_ in archive.objects
        ]
    if include_textures:
        result["textures"] = [
            {
                "index": texture.index,
                "offset": texture.offset,
                "flags": texture.flags,
                "color_count": texture.color_count,
                "palette_name": texture.palette_name,
                "name_index": texture.name_index,
                "width": texture.width,
                "height": texture.height,
                "aligned_width": texture.aligned_width,
                "aligned_height": texture.aligned_height,
                "data_offset": texture.data_offset,
                "available_size": texture.available_size,
                "expected_data_size": texture.expected_data_size,
            }
            for texture in archive.textures
        ]
    if include_tags:
        result["tags"] = [
            {
                "offset": tag.offset,
                "type": tag.tag_type,
                "name": tag.type_name,
                "size": tag.size,
            }
            for tag in archive.tags
        ]
    return result


def _psx_color_to_rgb(color: int) -> tuple[int, int, int]:
    return tuple(round(((color >> shift) & 0x1F) * 255 / 31) for shift in (0, 5, 10))


def _psx_decode_texture(archive: PsxArchive, texture: PsxTexture, data: bytes) -> bytes:
    if texture.available_size < texture.expected_data_size:
        raise PsxFormatError(
            f"PSX texture {texture.index} payload is truncated: "
            f"need {texture.expected_data_size} bytes, have {texture.available_size}"
        )
    raw = data[texture.data_offset : texture.data_offset + texture.expected_data_size]
    palette = None
    if texture.color_count == 16:
        palette = archive.palettes4.get(texture.palette_name)
    elif texture.color_count == 256:
        palette = archive.palettes8.get(texture.palette_name)
    if palette is not None and len(palette) < texture.color_count:
        raise PsxFormatError(f"PSX texture {texture.index} palette is incomplete")
    pixels = bytearray()
    if texture.color_count == 16:
        assert palette is not None
        for byte in raw:
            pixels.extend(_psx_color_to_rgb(palette[byte & 0x0F]))
            pixels.extend(_psx_color_to_rgb(palette[(byte >> 4) & 0x0F]))
    elif texture.color_count == 256:
        assert palette is not None
        for index in raw:
            pixels.extend(_psx_color_to_rgb(palette[index]))
    else:
        for offset in range(0, len(raw), 2):
            pixels.extend(_psx_color_to_rgb(struct.unpack_from("<H", raw, offset)[0]))
    expected_pixels = texture.aligned_width * texture.aligned_height * 3
    if len(pixels) != expected_pixels:
        raise PsxFormatError(
            f"PSX texture {texture.index} decoded to {len(pixels)} bytes, expected {expected_pixels}"
        )
    return bytes(pixels)


def _write_ppm(path: Path, width: int, height: int, pixels: bytes) -> None:
    path.write_bytes(f"P6\n{width} {height}\n255\n".encode("ascii") + pixels)


def _psx_texture_filename(archive: PsxArchive, texture: PsxTexture) -> str:
    name = archive.texture_names[texture.name_index]
    return f"texture_{name:08x}_{texture.index:04d}.ppm"


@dataclass(frozen=True)
class _PsxMaterial:
    name: str
    texture_filename: str
    width: int
    height: int


def _psx_model_lines(
    model: PsxModel,
    texture_materials: dict[int, _PsxMaterial],
    *,
    translation: tuple[int, int, int] = (0, 0, 0),
    object_name: str | None = None,
) -> tuple[list[str], set[str]]:
    materials: set[str] = set()
    lines = [f"o {object_name}"] if object_name else []
    tx, ty, tz = translation
    lines.extend(
        f"v {(tx + x) / 4096:.6f} {(ty + y) / 4096:.6f} {(tz + z) / 4096:.6f}"
        for x, y, z in model.vertices
    )
    lines.extend(f"vn {x / 4096:.6f} {y / 4096:.6f} {z / 4096:.6f}" for x, y, z in model.normals)
    texture_coordinate_index = 0
    for face in model.faces:
        if face.base_flags & 0x80:
            continue
        material_info = texture_materials.get(face.texture_index) if face.texture_index is not None else None
        material = material_info.name if material_info else "untextured"
        materials.add(material)
        lines.append(f"usemtl {material}")
        order = (2, 1, 0) if face.base_flags & 0x10 else (0, 2, 3, 1)
        corners = []
        for corner in order:
            vertex_index = face.vertex_indices[corner] + 1
            normal_index = face.normal_index + 1
            if face.uvs:
                u, v = face.uvs[corner]
                width = material_info.width if material_info else 256
                height = material_info.height if material_info else 256
                texture_coordinate_index += 1
                lines.append(f"vt {u / width:.6f} {1 - v / height:.6f}")
                corners.append(f"{vertex_index}/{texture_coordinate_index}/{normal_index}")
            else:
                corners.append(f"{vertex_index}//{normal_index}")
        lines.append("f " + " ".join(corners))
    return lines, materials


def _write_psx_model(path: Path, model: PsxModel, texture_materials: dict[int, _PsxMaterial]) -> set[str]:
    lines, materials = _psx_model_lines(model, texture_materials)
    path.write_text("\n".join(["# OpenTony PSX model export", "mtllib materials.mtl", *lines]) + "\n", encoding="utf-8")
    return materials


def _write_psx_scene(
    path: Path,
    archive: PsxArchive,
    texture_materials: dict[int, _PsxMaterial],
) -> set[str]:
    lines = ["# OpenTony PSX scene export", "mtllib models/materials.mtl"]
    materials: set[str] = set()
    for object_ in archive.objects:
        model = archive.models[object_.model_index]
        model_name = archive.model_names[object_.model_index] if archive.model_names else model.index
        object_name = f"object_{object_.index:04d}_model_{model_name:08x}"
        object_lines, object_materials = _psx_model_lines(
            model,
            texture_materials,
            translation=object_.position,
            object_name=object_name,
        )
        lines.extend(object_lines)
        materials.update(object_materials)
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return materials


def _psx_blockmap_object_indices(archive: PsxArchive) -> tuple[int, ...]:
    return tuple(
        sorted(
            {
                object_index
                for blockmap in archive.blockmaps
                for cell in blockmap.cells
                for object_index in cell.object_indices
            }
        )
    )


def _write_psx_collision(path: Path, archive: PsxArchive) -> dict:
    lines = ["# OpenTony PSX collision export", "mtllib collision.mtl"]
    surface_flags: Counter[int] = Counter()
    object_indices = _psx_blockmap_object_indices(archive)
    face_count = 0
    vertex_offset = 0
    normal_offset = 0
    for object_index in object_indices:
        object_ = archive.objects[object_index]
        model = archive.models[object_.model_index]
        model_name = archive.model_names[object_.model_index] if archive.model_names else model.index
        object_name = f"collision_object_{object_index:04d}_model_{model_name:08x}"
        lines.append(f"o {object_name}")
        tx, ty, tz = object_.position
        lines.extend(
            f"v {(tx + x) / 4096:.6f} {(ty + y) / 4096:.6f} {(tz + z) / 4096:.6f}"
            for x, y, z in model.vertices
        )
        lines.extend(f"vn {x / 4096:.6f} {y / 4096:.6f} {z / 4096:.6f}" for x, y, z in model.normals)
        for face in model.faces:
            if face.base_flags & 0x80:
                continue
            material = f"surface_{face.surface_flags:04x}"
            surface_flags[face.surface_flags] += 1
            lines.append(f"usemtl {material}")
            order = (2, 1, 0) if face.base_flags & 0x10 else (0, 2, 3, 1)
            corners = [
                f"{vertex_offset + face.vertex_indices[corner] + 1}//"
                f"{normal_offset + face.normal_index + 1}"
                for corner in order
            ]
            lines.append("f " + " ".join(corners))
            face_count += 1
        vertex_offset += len(model.vertices)
        normal_offset += len(model.normals)
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return {
        "object_indices": object_indices,
        "face_count": face_count,
        "surface_flags": {
            f"0x{flags:04x}": count for flags, count in sorted(surface_flags.items())
        },
        "materials": {f"surface_{flags:04x}" for flags in surface_flags},
    }


def _psx_blockmap_json(archive: PsxArchive) -> dict:
    return {
        "version": 1,
        "blockmaps": [
            {
                "tag_offset": blockmap.tag_offset,
                "bounds_fixed": list(blockmap.bounds),
                "bounds": [round(value / 4096, 6) for value in blockmap.bounds],
                "cell_counts": list(blockmap.cell_counts),
                "cells": [
                    {
                        "index": cell.index,
                        "x": cell.x,
                        "z": cell.z,
                        "unknown_1": cell.unknown_1,
                        "unknown_2": cell.unknown_2,
                        "objects": list(cell.object_indices),
                    }
                    for cell in blockmap.cells
                ],
            }
            for blockmap in archive.blockmaps
        ],
    }


def extract_psx(
    path: str | Path,
    output: str | Path,
    *,
    force: bool = False,
    include_models: bool = True,
    include_textures: bool = True,
) -> dict:
    source = resolve(path)
    archive = PsxArchive.read(source)
    if archive.version != 4:
        raise SystemExit("PSX export currently supports version 4 assets only")
    if not include_models and not include_textures:
        raise ValueError("at least one PSX export category must be enabled")

    destination = _prepare_output(output, force)
    models_output = destination / "models"
    textures_output = destination / "textures"
    if include_models:
        models_output.mkdir(parents=True, exist_ok=True)
    if include_textures:
        textures_output.mkdir(parents=True, exist_ok=True)

    source_data = source.read_bytes()
    texture_entries = []
    skipped_textures = []
    texture_files: dict[int, str] = {}
    if include_textures:
        for texture in archive.textures:
            filename = _psx_texture_filename(archive, texture)
            try:
                pixels = _psx_decode_texture(archive, texture, source_data)
            except PsxFormatError as exc:
                skipped_textures.append({"index": texture.index, "reason": str(exc)})
                continue
            target = textures_output / filename
            _write_ppm(target, texture.aligned_width, texture.aligned_height, pixels)
            texture_files.setdefault(texture.name_index, filename)
            texture_entries.append(
                {
                    "index": texture.index,
                    "path": f"textures/{filename}",
                    "name": archive.texture_names[texture.name_index],
                    "width": texture.aligned_width,
                    "height": texture.aligned_height,
                    "color_count": texture.color_count,
                    "source_offset": texture.data_offset,
                    "source_size": texture.expected_data_size,
                    "sha256": hashlib.sha256(pixels).hexdigest(),
                }
            )

    texture_materials = {
        texture.name_index: _PsxMaterial(
            name=f"texture_{archive.texture_names[texture.name_index]:08x}_{texture.index:04d}",
            texture_filename=texture_files[texture.name_index],
            width=texture.width,
            height=texture.height,
        )
        for texture in archive.textures
        if texture.name_index in texture_files
    }
    model_entries = []
    model_materials: set[str] = set()
    scene_entry = None
    blockmap_entry = None
    collision_entry = None
    if include_models:
        for model in archive.models:
            target = models_output / f"model_{model.index:04d}.obj"
            materials = _write_psx_model(target, model, texture_materials)
            model_materials.update(materials)
            model_entries.append(
                {
                    "index": model.index,
                    "name": archive.model_names[model.index] if archive.model_names else None,
                    "path": f"models/{target.name}",
                    "vertex_count": model.vertex_count,
                    "normal_count": model.normal_count,
                    "face_count": model.face_count,
                }
            )
        scene_materials = _write_psx_scene(destination / "scene.obj", archive, texture_materials)
        model_materials.update(scene_materials)
        scene_entry = {"path": "scene.obj", "object_count": len(archive.objects)}
        materials_path = models_output / "materials.mtl"
        material_lines = []
        for material in sorted(model_materials):
            material_lines.extend([f"newmtl {material}", "Kd 1.0 1.0 1.0"])
            texture_material = next(
                (item for item in texture_materials.values() if item.name == material),
                None,
            )
            if texture_material:
                material_lines.append(f"map_Kd ../textures/{texture_material.texture_filename}")
            material_lines.append("")
        materials_path.write_text("\n".join(material_lines), encoding="utf-8")
        if archive.blockmaps:
            blockmap_path = destination / "blockmap.json"
            blockmap_path.write_text(
                json.dumps(_psx_blockmap_json(archive), indent=2) + "\n",
                encoding="utf-8",
            )
            blockmap_entry = {
                "path": "blockmap.json",
                "count": len(archive.blockmaps),
                "cell_count": sum(
                    blockmap.cell_counts[0] * blockmap.cell_counts[1]
                    for blockmap in archive.blockmaps
                ),
                "object_references": sum(
                    len(cell.object_indices)
                    for blockmap in archive.blockmaps
                    for cell in blockmap.cells
                ),
            }
            collision_data = _write_psx_collision(destination / "collision.obj", archive)
            collision_material_lines = []
            for material in sorted(collision_data["materials"]):
                collision_material_lines.extend(
                    [f"newmtl {material}", "Kd 0.7 0.7 0.7", ""]
                )
            (destination / "collision.mtl").write_text(
                "\n".join(collision_material_lines), encoding="utf-8"
            )
            collision_entry = {
                "path": "collision.obj",
                "materials": "collision.mtl",
                "object_count": len(collision_data["object_indices"]),
                "face_count": collision_data["face_count"],
                "surface_flags": collision_data["surface_flags"],
            }

    object_entries = [
        {
            "index": object_.index,
            "flags": object_.flags,
            "model_index": object_.model_index,
            "model_name": archive.model_names[object_.model_index]
            if archive.model_names
            else None,
            "position_fixed": list(object_.position),
            "position": [round(value / 4096, 6) for value in object_.position],
        }
        for object_ in archive.objects
    ]
    manifest = {
        "version": 1,
        "tony_version": __version__,
        "source": {
            "path": _display_path(source),
            "size": source.stat().st_size,
            "sha256": sha256(source),
        },
        "format": archive.summary(),
        "extracted_path": _display_path(destination),
        "models": model_entries,
        "objects": object_entries,
        "scene": scene_entry,
        "blockmap": blockmap_entry,
        "collision": collision_entry,
        "textures": texture_entries,
        "skipped_textures": skipped_textures,
    }
    (destination / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    return manifest


def _asset_source_info(path: Path) -> dict:
    return {
        "path": _display_path(path),
        "size": path.stat().st_size,
        "sha256": sha256(path),
    }


def inspect_hed(
    path: str | Path,
    *,
    include_entries: bool = False,
    names_path: str | Path | None = None,
    wad_path: str | Path | None = None,
) -> dict:
    source = resolve(path)
    archive = HedArchive.read(source, names_path=names_path, wad_path=wad_path)
    result = {
        "source": _asset_source_info(source),
        "associated_files": {
            "het": _asset_source_info(archive.names_path) if archive.names_path else None,
            "hep": _asset_source_info(archive.secondary_names_path)
            if archive.secondary_names_path
            else None,
            "wad": _asset_source_info(archive.wad_path) if archive.wad_path else None,
        },
        **archive.summary(),
    }
    if include_entries:
        result["entries"] = [
            {
                "index": index,
                "name": entry.name,
                "output_name": entry.output_name,
                "name_hash": entry.name_hash,
                "name_hash_hex": f"0x{entry.name_hash:08x}",
                "offset": entry.offset,
                "size": entry.size,
            }
            for index, entry in enumerate(archive.entries)
        ]
    return result


def inventory_assets(root: str | Path, *, examples: int = 3) -> dict:
    source = resolve(root)
    if not source.is_dir():
        raise FileNotFoundError(source)
    if examples < 0:
        raise ValueError("examples must not be negative")

    buckets: dict[str, dict[str, int | list[str]]] = {}
    file_count = 0
    total_size = 0
    for path in sorted(candidate for candidate in source.rglob("*") if candidate.is_file()):
        extension = path.suffix.lower() or "<none>"
        bucket = buckets.setdefault(extension, {"file_count": 0, "total_size": 0, "examples": []})
        size = path.stat().st_size
        bucket["file_count"] = int(bucket["file_count"]) + 1
        bucket["total_size"] = int(bucket["total_size"]) + size
        bucket_examples = bucket["examples"]
        if isinstance(bucket_examples, list) and len(bucket_examples) < examples:
            bucket_examples.append(path.relative_to(source).as_posix())
        file_count += 1
        total_size += size

    return {
        "root": _display_path(source),
        "file_count": file_count,
        "total_size": total_size,
        "extensions": [
            {"extension": extension, **buckets[extension]}
            for extension in sorted(buckets)
        ],
    }


def _build_output(path: str | Path) -> Path:
    output = resolve(path).resolve()
    build_root = (ROOT / "build").resolve()
    try:
        output.relative_to(build_root)
    except ValueError as exc:
        raise SystemExit(f"asset extraction output must be under build/: {output}") from exc
    if output == build_root:
        raise SystemExit(f"asset extraction output must be a directory inside build/: {output}")
    return output


def _remove_output(path: Path) -> None:
    if path.is_symlink():
        raise SystemExit(f"refusing to remove symlink output: {path}")
    shutil.rmtree(path)


def _prepare_output(output: str | Path, force: bool) -> Path:
    destination = resolve(output).resolve()
    if destination.exists():
        if not destination.is_dir():
            raise SystemExit(f"asset extraction output is not a directory: {destination}")
        if any(destination.iterdir()):
            if not force:
                raise SystemExit(f"output already exists; use --force to replace generated output: {destination}")
            _remove_output(destination)
    return destination


def extract_pkr(path: str | Path, output: str | Path, *, force: bool = False) -> dict:
    source = resolve(path)
    archive = PkrArchive.read(source)
    destination = _prepare_output(output, force)

    files_output = destination / "files"
    files_output.mkdir(parents=True, exist_ok=True)
    manifest_entries = []
    with source.open("rb") as stream:
        for entry in archive.entries:
            parts = _safe_archive_path(entry.directory, entry.name)
            target = files_output.joinpath(*parts)
            target.parent.mkdir(parents=True, exist_ok=True)
            stream.seek(entry.data_offset)
            data = _read_exact(stream, entry.size, f"payload {entry.archive_path}")
            target.write_bytes(data)
            manifest_entries.append(
                {
                    "path": entry.archive_path,
                    "offset": entry.data_offset,
                    "size": entry.size,
                    "sha256": hashlib.sha256(data).hexdigest(),
                }
            )

    manifest = {
        "version": 1,
        "tony_version": __version__,
        "source": {
            "path": _display_path(source),
            "size": source.stat().st_size,
            "sha256": sha256(source),
        },
        "format": archive.summary(),
        "extracted_path": _display_path(files_output),
        "entries": manifest_entries,
    }
    (destination / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    return manifest


def extract_pre(path: str | Path, output: str | Path, *, force: bool = False) -> dict:
    source = resolve(path)
    archive = PreArchive.read(source)
    destination = _prepare_output(output, force)
    files_output = destination / "files"
    files_output.mkdir(parents=True, exist_ok=True)
    manifest_entries = []
    with source.open("rb") as stream:
        for entry in archive.entries:
            target = files_output.joinpath(*_safe_archive_path("", entry.name))
            target.parent.mkdir(parents=True, exist_ok=True)
            stream.seek(entry.data_offset)
            data = _read_exact(stream, entry.size, f"payload {entry.name}")
            target.write_bytes(data)
            manifest_entries.append(
                {
                    "path": entry.name,
                    "offset": entry.data_offset,
                    "size": entry.size,
                    "sha256": hashlib.sha256(data).hexdigest(),
                }
            )

    manifest = {
        "version": 1,
        "tony_version": __version__,
        "source": {
            "path": _display_path(source),
            "size": source.stat().st_size,
            "sha256": sha256(source),
        },
        "format": archive.summary(),
        "extracted_path": _display_path(files_output),
        "entries": manifest_entries,
    }
    (destination / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    return manifest


def extract_hed(
    path: str | Path,
    output: str | Path,
    *,
    force: bool = False,
    names_path: str | Path | None = None,
    wad_path: str | Path | None = None,
    allow_zero_wad: bool = False,
) -> dict:
    source = resolve(path)
    archive = HedArchive.read(source, names_path=names_path, wad_path=wad_path)
    if archive.wad_path is None:
        raise SystemExit("cannot extract HED entries: associated WAD file was not found")
    if archive.wad_status == "all_zero" and not allow_zero_wad:
        raise SystemExit(
            "refusing to extract an all-zero WAD; use --allow-zero-wad only for forensic output"
        )
    if archive.range_overlap_count:
        raise SystemExit(
            "cannot extract HED entries: table ranges overlap; direct raw WAD extraction is unsupported"
        )
    if archive.out_of_bounds_count:
        raise SystemExit(
            f"cannot extract HED entries: {archive.out_of_bounds_count} payloads exceed the WAD size"
        )

    destination = _prepare_output(output, force)
    files_output = destination / "files"
    files_output.mkdir(parents=True, exist_ok=True)
    manifest_entries = []
    seen_paths: set[str] = set()
    with archive.wad_path.open("rb") as stream:
        for entry in archive.entries:
            parts = _safe_archive_path("", entry.output_name)
            archive_path = "/".join(parts)
            if archive_path in seen_paths:
                raise HedFormatError(f"duplicate HED output path: {archive_path}")
            seen_paths.add(archive_path)
            target = files_output.joinpath(*parts)
            target.parent.mkdir(parents=True, exist_ok=True)
            stream.seek(entry.offset)
            data = _read_exact(stream, entry.size, f"WAD payload {archive_path}")
            target.write_bytes(data)
            manifest_entries.append(
                {
                    "path": archive_path,
                    "name_hash": entry.name_hash,
                    "offset": entry.offset,
                    "size": entry.size,
                    "sha256": hashlib.sha256(data).hexdigest(),
                }
            )

    manifest = {
        "version": 1,
        "tony_version": __version__,
        "source": {
            "path": _display_path(source),
            "size": source.stat().st_size,
            "sha256": sha256(source),
        },
        "associated_files": {
            "het": _asset_source_info(archive.names_path) if archive.names_path else None,
            "hep": _asset_source_info(archive.secondary_names_path)
            if archive.secondary_names_path
            else None,
            "wad": _asset_source_info(archive.wad_path),
        },
        "format": archive.summary(),
        "extracted_path": _display_path(files_output),
        "entries": manifest_entries,
    }
    (destination / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    return manifest


def assets_inspect_pkr(args) -> int:
    result = inspect_pkr(args.path, include_entries=args.entries)
    print(json.dumps(result, indent=2))
    return 0


def assets_inspect_pre(args) -> int:
    result = inspect_pre(args.path, include_entries=args.entries)
    print(json.dumps(result, indent=2))
    return 0


def assets_inspect_trg(args) -> int:
    result = inspect_trg(args.path, include_nodes=args.nodes, include_scripts=args.scripts)
    print(json.dumps(result, indent=2))
    return 0


def assets_inspect_psx(args) -> int:
    result = inspect_psx(
        args.path,
        include_models=args.models,
        include_textures=args.textures,
        include_tags=args.tags,
    )
    print(json.dumps(result, indent=2))
    return 0


def assets_extract_psx(args) -> int:
    destination = _build_output(args.output)
    manifest = extract_psx(
        args.path,
        destination,
        force=args.force,
        include_models=not args.textures_only,
        include_textures=not args.models_only,
    )
    print(
        f"Exported {len(manifest['models'])} models and {len(manifest['textures'])} textures into {destination}"
    )
    if manifest["skipped_textures"]:
        print(f"Skipped textures: {len(manifest['skipped_textures'])}")
    print(f"Manifest: {destination / 'manifest.json'}")
    return 0


def assets_inspect_hed(args) -> int:
    result = inspect_hed(
        args.path,
        include_entries=args.entries,
        names_path=args.names,
        wad_path=args.wad,
    )
    print(json.dumps(result, indent=2))
    return 0


def assets_inventory(args) -> int:
    result = inventory_assets(args.path, examples=args.examples)
    print(json.dumps(result, indent=2))
    return 0


def assets_extract_pkr(args) -> int:
    destination = _build_output(args.output)
    manifest = extract_pkr(args.path, destination, force=args.force)
    print(f"Extracted {manifest['format']['file_count']} PKR files into {destination}")
    print(f"Manifest: {destination / 'manifest.json'}")
    return 0


def assets_extract_pre(args) -> int:
    destination = _build_output(args.output)
    manifest = extract_pre(args.path, destination, force=args.force)
    print(f"Extracted {manifest['format']['file_count']} PRE files into {destination}")
    print(f"Manifest: {destination / 'manifest.json'}")
    return 0


def assets_extract_hed(args) -> int:
    destination = _build_output(args.output)
    manifest = extract_hed(
        args.path,
        destination,
        force=args.force,
        names_path=args.names,
        wad_path=args.wad,
        allow_zero_wad=args.allow_zero_wad,
    )
    print(f"Extracted {manifest['format']['file_count']} HED/WAD files into {destination}")
    print(f"Manifest: {destination / 'manifest.json'}")
    return 0
