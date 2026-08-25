from __future__ import annotations

from pathlib import Path

from .common import load_yaml, resolve, sha256


def recorded_executable() -> Path:
    """Return the recorded executable only after verifying its content hash."""

    spec = load_yaml("re/config/binaries.yml")["executables"]["thps2_pc"]
    path = spec.get("path")
    if not path:
        raise SystemExit("No executable recorded. Run: tony exe identify <path> --record")

    executable = resolve(path)
    if not executable.is_file():
        raise SystemExit(f"recorded executable missing: {executable}")

    expected_sha256 = spec.get("sha256")
    if not expected_sha256:
        raise SystemExit(
            "recorded executable identity is incomplete; run: tony exe identify <path> --record"
        )

    actual_sha256 = sha256(executable)
    if actual_sha256.lower() != str(expected_sha256).lower():
        raise SystemExit(
            "recorded executable SHA-256 mismatch:\n"
            f"  expected {expected_sha256}\n"
            f"  actual   {actual_sha256}"
        )
    return executable
