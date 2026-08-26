from __future__ import annotations

import json
import os
import shutil
import stat
import subprocess
import tempfile
from pathlib import Path

from . import __version__
from .common import ROOT, capture, load_yaml, relative_to_root, resolve, save_yaml, sha256

_RAW_CD_SECTOR_SIZE = 2352
_ISO_SECTOR_SIZE = 2048
_RAW_CD_SYNC = b"\x00" + b"\xff" * 10 + b"\x00"
_ISO_PVD_SIGNATURE = b"\x01CD001"


def _read_at(path: Path, offset: int, size: int) -> bytes:
    with path.open("rb") as stream:
        stream.seek(offset)
        return stream.read(size)


def _pvd_data_sectors(pvd: bytes) -> int | None:
    if len(pvd) < 88 or pvd[:6] != _ISO_PVD_SIGNATURE:
        return None
    little = int.from_bytes(pvd[80:84], "little")
    big = int.from_bytes(pvd[84:88], "big")
    return little if little == big and little > 0 else None


def _detect_media_format(path: Path) -> dict[str, int | str | None]:
    """Identify an ISO, raw CD image, or opaque archive/container."""

    standard_pvd_offset = 16 * _ISO_SECTOR_SIZE
    standard_pvd = _read_at(path, standard_pvd_offset, _ISO_SECTOR_SIZE)
    if standard_pvd[:len(_ISO_PVD_SIGNATURE)] == _ISO_PVD_SIGNATURE:
        return {
            "format": "iso9660",
            "sector_size": _ISO_SECTOR_SIZE,
            "user_data_offset": 0,
            "user_data_size": _ISO_SECTOR_SIZE,
            "pvd_sector": 16,
            "data_sectors": _pvd_data_sectors(standard_pvd),
        }

    if path.stat().st_size % _RAW_CD_SECTOR_SIZE == 0:
        raw_sector = _read_at(path, 16 * _RAW_CD_SECTOR_SIZE, _RAW_CD_SECTOR_SIZE)
        if raw_sector[:12] == _RAW_CD_SYNC:
            mode = raw_sector[15]
            if mode == 1 and raw_sector[16:22] == _ISO_PVD_SIGNATURE:
                pvd = raw_sector[16:16 + _ISO_SECTOR_SIZE]
                return {
                    "format": "raw-cd-mode1",
                    "sector_size": _RAW_CD_SECTOR_SIZE,
                    "user_data_offset": 16,
                    "user_data_size": _ISO_SECTOR_SIZE,
                    "pvd_sector": 16,
                    "data_sectors": _pvd_data_sectors(pvd),
                }
            if mode == 2 and raw_sector[24:30] == _ISO_PVD_SIGNATURE:
                pvd = raw_sector[24:24 + _ISO_SECTOR_SIZE]
                return {
                    "format": "raw-cd-mode2-form1",
                    "sector_size": _RAW_CD_SECTOR_SIZE,
                    "user_data_offset": 24,
                    "user_data_size": _ISO_SECTOR_SIZE,
                    "pvd_sector": 16,
                    "data_sectors": _pvd_data_sectors(pvd),
                }

    return {
        "format": "container-or-archive",
        "sector_size": None,
        "user_data_offset": None,
        "user_data_size": None,
        "pvd_sector": None,
        "data_sectors": None,
    }


def _hash_range(path: Path, offset: int, size: int) -> str:
    import hashlib

    digest = hashlib.sha256()
    with path.open("rb") as stream:
        stream.seek(offset)
        remaining = size
        while remaining:
            chunk = stream.read(min(1024 * 1024, remaining))
            if not chunk:
                raise RuntimeError(f"short read while hashing {path}")
            digest.update(chunk)
            remaining -= len(chunk)
    return digest.hexdigest()


def _media_layout(path: Path, media_format: dict[str, int | str | None]) -> dict:
    """Describe raw sectors beyond the filesystem volume without classifying them."""

    if not str(media_format["format"]).startswith("raw-cd-"):
        return {"raw_sector_count": None, "raw_tail": None}

    sector_size = int(media_format["sector_size"])
    raw_sector_count, remainder = divmod(path.stat().st_size, sector_size)
    if remainder:
        raise RuntimeError(f"raw image size is not divisible by {sector_size}: {path}")

    data_sectors = media_format.get("data_sectors")
    if data_sectors is None:
        return {"raw_sector_count": raw_sector_count, "raw_tail": None}
    data_sector_count = int(data_sectors)
    if data_sector_count > raw_sector_count:
        raise RuntimeError(
            f"ISO volume exceeds raw image: {data_sector_count} > {raw_sector_count} sectors"
        )

    tail_sector_count = raw_sector_count - data_sector_count
    tail_offset = data_sector_count * sector_size
    tail_size = tail_sector_count * sector_size
    return {
        "raw_sector_count": raw_sector_count,
        "raw_tail": {
            "start_sector": data_sector_count,
            "sector_count": tail_sector_count,
            "offset": tail_offset,
            "size": tail_size,
            "sha256": _hash_range(path, tail_offset, tail_size),
            "classification": "unclassified",
        },
    }


def _convert_raw_cd(source: Path, destination: Path, media_format: dict[str, int | str | None]) -> None:
    sector_size = int(media_format["sector_size"])
    user_data_size = int(media_format["user_data_size"])
    total_sector_count, remainder = divmod(source.stat().st_size, sector_size)
    if remainder:
        raise RuntimeError(f"raw image size is not divisible by {sector_size}: {source}")
    sector_count = int(media_format.get("data_sectors") or total_sector_count)
    if sector_count > total_sector_count:
        raise RuntimeError(f"ISO volume exceeds raw image: {sector_count} > {total_sector_count} sectors")
    destination.parent.mkdir(parents=True, exist_ok=True)
    with source.open("rb") as input_stream, destination.open("wb") as output_stream:
        for index in range(sector_count):
            sector = input_stream.read(sector_size)
            if len(sector) != sector_size:
                raise RuntimeError(f"short raw sector {index} in {source}")
            if sector[:12] != _RAW_CD_SYNC:
                raise RuntimeError(f"invalid raw CD sync header at sector {index}")
            mode = sector[15]
            if mode == 1:
                user_data_offset = 16
            elif mode == 2:
                # The duplicated XA subheader's submode byte marks Form 2 with
                # bit 5. Form 2 carries 2324 bytes and cannot be normalized as
                # an ISO-9660 2048-byte logical sector.
                if sector[18] & 0x20 or sector[22] & 0x20:
                    raise RuntimeError(f"unexpected raw CD Mode 2/Form 2 sector at {index}")
                user_data_offset = 24
            else:
                raise RuntimeError(f"unexpected raw CD mode at sector {index}: {mode}")
            payload = sector[user_data_offset:user_data_offset + user_data_size]
            if len(payload) != user_data_size:
                raise RuntimeError(f"short user-data payload at sector {index}")
            output_stream.write(payload)

    pvd_offset = 16 * _ISO_SECTOR_SIZE
    if _read_at(destination, pvd_offset, len(_ISO_PVD_SIGNATURE)) != _ISO_PVD_SIGNATURE:
        raise RuntimeError(f"converted image has no valid ISO-9660 volume descriptor: {destination}")


def _build_output(path: Path) -> Path:
    output = path.resolve()
    build_root = (ROOT / "build").resolve()
    try:
        output.relative_to(build_root)
    except ValueError as exc:
        raise SystemExit(f"media extraction output must be under build/: {output}") from exc
    return output


def _extract_with_7z(source: Path, output: Path) -> int:
    output.mkdir(parents=True, exist_ok=True)
    return subprocess.run(["7z", "x", str(source), f"-o{output}", "-y"], cwd=ROOT, check=False).returncode


def _extract_with_xorriso(source: Path, output: Path) -> int:
    output.mkdir(parents=True, exist_ok=True)
    return subprocess.run(
        ["xorriso", "-osirrox", "on", "-indev", str(source), "-extract", "/", str(output)],
        cwd=ROOT,
        check=False,
    ).returncode


def _write_media_manifest(output: Path, manifest: dict) -> None:
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


def _remove_generated_tree(path: Path) -> None:
    def make_writable(function, target, _error):
        target_path = Path(target)
        for candidate in (target_path, target_path.parent):
            mode = os.stat(candidate).st_mode | stat.S_IWUSR
            if candidate.is_dir():
                mode |= stat.S_IXUSR
            os.chmod(candidate, mode)
        function(target)

    shutil.rmtree(path, onexc=make_writable)


def _media_spec_and_path(args) -> tuple[dict, Path]:
    config = load_yaml("re/config/binaries.yml")
    spec = config["media"]["thps2_pc_disc"]
    path = resolve(args.path or spec["path"])
    if not path.is_file():
        raise SystemExit(f"media not found: {path}")
    return config, path


def media_identify(args) -> int:
    config, path = _media_spec_and_path(args)
    status, description = capture(["file", "-b", str(path)])
    media_format = _detect_media_format(path)
    record = {
        "path": relative_to_root(path),
        "size": path.stat().st_size,
        "sha256": sha256(path),
        "file_description": description if status == 0 else None,
        **media_format,
        **_media_layout(path, media_format),
    }
    print(json.dumps(record, indent=2))
    if args.record:
        config["media"]["thps2_pc_disc"].update(record)
        save_yaml("re/config/binaries.yml", config)
        print("Recorded identity in re/config/binaries.yml")
    return 0


def media_tracks(args) -> int:
    _config, path = _media_spec_and_path(args)
    media_format = _detect_media_format(path)
    layout = _media_layout(path, media_format)
    data_sectors = media_format.get("data_sectors")
    record = {
        "path": relative_to_root(path),
        "format": media_format["format"],
        "raw_sector_count": layout["raw_sector_count"],
        "filesystem_volume": (
            {"start_sector": 0, "sector_count": int(data_sectors)} if data_sectors is not None else None
        ),
        "unclassified_regions": (
            [layout["raw_tail"]] if layout["raw_tail"] and layout["raw_tail"]["sector_count"] else []
        ),
    }
    print(json.dumps(record, indent=2))
    return 0


def media_list(args) -> int:
    _config, path = _media_spec_and_path(args)
    media_format = _detect_media_format(path)
    if str(media_format["format"]).startswith("raw-cd-"):
        with tempfile.TemporaryDirectory(prefix="opentony-media-") as temporary:
            normalized = Path(temporary) / "image.iso"
            _convert_raw_cd(path, normalized, media_format)
            return subprocess.run(["7z", "l", str(normalized)], cwd=ROOT, check=False).returncode
    return subprocess.run(["7z", "l", str(path)], cwd=ROOT, check=False).returncode


def media_extract(args) -> int:
    config = load_yaml("re/config/binaries.yml")
    spec = config["media"]["thps2_pc_disc"]
    path = resolve(args.path or spec["path"])
    if not path.is_file():
        raise SystemExit(f"media not found: {path}")
    expected_sha256 = spec.get("sha256")
    if not expected_sha256:
        raise SystemExit("media identity is not recorded; run: tony media identify --record")
    actual_sha256 = sha256(path)
    if actual_sha256 != expected_sha256:
        raise SystemExit(
            f"canonical media SHA-256 mismatch:\n  expected {expected_sha256}\n  actual   {actual_sha256}"
        )

    output = _build_output(resolve(args.output))
    if output == ROOT.resolve() or output == (ROOT / "game").resolve():
        raise SystemExit("refusing to extract into the repository or game directory; use build/")
    if output.exists() and any(output.iterdir()):
        if not args.force:
            raise SystemExit(f"output already exists; use --force to replace generated output: {output}")
        _remove_generated_tree(output)
    output.mkdir(parents=True, exist_ok=True)

    media_format = _detect_media_format(path)
    layout = _media_layout(path, media_format)
    files_output = output / "files"
    normalized_iso = None
    if str(media_format["format"]).startswith("raw-cd-"):
        normalized_iso = output / f"{path.stem}.iso"
        _convert_raw_cd(path, normalized_iso, media_format)
        status = _extract_with_xorriso(normalized_iso, files_output)
    elif media_format["format"] == "iso9660":
        normalized_iso = output / f"{path.stem}.iso"
        shutil.copyfile(path, normalized_iso)
        status = _extract_with_xorriso(normalized_iso, files_output)
    else:
        status = _extract_with_7z(path, files_output)

    if status != 0:
        return status

    extractor = "xorriso" if normalized_iso is not None else "7z"
    _, version_output = capture([extractor, "--version"] if extractor == "xorriso" else [extractor])
    manifest = {
        "version": 1,
        "tony_version": __version__,
        "source": {
            "path": relative_to_root(path),
            "size": path.stat().st_size,
            "sha256": actual_sha256,
            "layout": layout,
        },
        "format": media_format,
        "normalized_iso": (
            {
                "path": relative_to_root(normalized_iso),
                "size": normalized_iso.stat().st_size,
                "sha256": sha256(normalized_iso),
            }
            if normalized_iso is not None
            else None
        ),
        "extracted_path": relative_to_root(files_output),
        "extractor": {
            "name": extractor,
            "version": version_output.splitlines()[0] if version_output else None,
        },
    }
    _write_media_manifest(output, manifest)
    print(f"Extracted media into {output}")
    print(f"Manifest: {output / 'manifest.json'}")
    return 0
