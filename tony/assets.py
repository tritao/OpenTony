from __future__ import annotations

import hashlib
import json
import shutil
import struct
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


class PkrFormatError(ValueError):
    """The input does not satisfy the observed PKR2 archive layout."""


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


def assets_inspect_pkr(args) -> int:
    result = inspect_pkr(args.path, include_entries=args.entries)
    print(json.dumps(result, indent=2))
    return 0


def assets_inspect_pre(args) -> int:
    result = inspect_pre(args.path, include_entries=args.entries)
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
