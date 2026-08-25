from __future__ import annotations

from pathlib import Path

from .identity import recorded_executable

# These offsets and bytes belong to the recorded THPS2 PC executable build.
# FUN_004bb240 loads the disc TOC and returns a nonzero error when the CD
# cannot be opened. Returning zero bypasses only that startup/recheck gate.
_PATCH_FILE_OFFSET = 0xBB240
_EXPECTED_BYTES = bytes.fromhex("81 ec 20 02 00 00")
_PATCH_BYTES = bytes.fromhex("31 c0 c3 90 90 90")


def _patched_bytes(source: bytes) -> bytes:
    end = _PATCH_FILE_OFFSET + len(_EXPECTED_BYTES)
    if len(source) < end:
        raise SystemExit("recorded executable is too small for the known no-CD patch")

    current = source[_PATCH_FILE_OFFSET:end]
    if current == _PATCH_BYTES:
        return source
    if current != _EXPECTED_BYTES:
        raise SystemExit(
            "recorded executable does not match the supported THPS2 build at "
            f"file offset 0x{_PATCH_FILE_OFFSET:x}; refusing to patch"
        )

    result = bytearray(source)
    result[_PATCH_FILE_OFFSET:end] = _PATCH_BYTES
    return bytes(result)


def patch_nocd_executable(output: Path | None = None) -> Path:
    """Create an adjacent no-CD executable without changing the canonical file."""

    original = recorded_executable()
    target = output or original.with_name(f"{original.stem}.nocd{original.suffix}")
    target = Path(target)
    if target.resolve() == original.resolve():
        raise SystemExit("refusing to overwrite the canonical executable")

    patched = _patched_bytes(original.read_bytes())
    if target.exists():
        if target.read_bytes() != patched:
            raise SystemExit(f"refusing to overwrite an existing file: {target}")
        print(f"No-CD executable already current: {target}")
        return target

    target.parent.mkdir(parents=True, exist_ok=True)
    # xorriso preserves read-only permissions from the source disc. The
    # extracted build tree is disposable, so make only this asset directory
    # writable when the default adjacent output needs to be created.
    if target.parent == original.parent:
        target.parent.chmod(target.parent.stat().st_mode | 0o200)
    target.write_bytes(patched)
    target.chmod((original.stat().st_mode & 0o777) | 0o200)
    print(f"Created no-CD executable: {target}")
    print(f"Patch: file offset 0x{_PATCH_FILE_OFFSET:x}, FUN_004bb240 returns success")
    return target


def nocd_executable() -> Path:
    """Return the generated no-CD executable, creating it when needed."""

    return patch_nocd_executable()
