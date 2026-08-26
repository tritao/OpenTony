from __future__ import annotations

import hashlib
import os
import tempfile
import urllib.request
from pathlib import Path

from .common import load_yaml, resolve, sha256

PROGRESS_INTERVAL = 16 * 1024 * 1024


def _print_progress(downloaded: int, total: int | None) -> None:
    downloaded_mib = downloaded / (1024 * 1024)
    if total:
        percent = min(downloaded * 100 / total, 100)
        total_mib = total / (1024 * 1024)
        print(f"Downloaded {downloaded_mib:.1f} MiB / {total_mib:.1f} MiB ({percent:.0f}%)", flush=True)
    else:
        print(f"Downloaded {downloaded_mib:.1f} MiB", flush=True)


def install_media() -> Path:
    """Download and verify the recorded THPS2 disc image."""

    spec = load_yaml("re/config/binaries.yml")["media"]["thps2_pc_disc"]
    target = resolve(spec["path"])
    source_url = spec["source_url"]
    expected_size = int(spec["size"])
    expected_sha256 = spec["sha256"]

    if target.is_file():
        if target.stat().st_size == expected_size and sha256(target) == expected_sha256:
            print(f"Media already provisioned: {target}")
            return target
        raise SystemExit(f"refusing to replace existing media with unexpected contents: {target}")
    if target.exists():
        raise SystemExit(f"media destination exists but is not a regular file: {target}")

    target.parent.mkdir(parents=True, exist_ok=True)
    partial: Path | None = None
    digest = hashlib.sha256()
    size = 0
    print(f"Downloading {source_url}\n       -> {target}")
    try:
        with tempfile.NamedTemporaryFile(
            mode="wb",
            dir=target.parent,
            prefix=f".{target.name}.",
            suffix=".part",
            delete=False,
        ) as output:
            partial = Path(output.name)
            with urllib.request.urlopen(source_url, timeout=30) as response:
                content_length = response.headers.get("Content-Length")
                total = int(content_length) if content_length else None
                next_progress = PROGRESS_INTERVAL
                while chunk := response.read(1024 * 1024):
                    output.write(chunk)
                    digest.update(chunk)
                    size += len(chunk)
                    if size >= next_progress:
                        _print_progress(size, total)
                        next_progress = size + PROGRESS_INTERVAL

        _print_progress(size, total)

        actual_sha256 = digest.hexdigest()
        if size != expected_size:
            raise SystemExit(f"download size mismatch: expected {expected_size}, got {size}")
        if actual_sha256 != expected_sha256:
            raise SystemExit(f"download SHA-256 mismatch: expected {expected_sha256}, got {actual_sha256}")
        os.replace(partial, target)
        partial = None
    except BaseException:
        if partial is not None:
            partial.unlink(missing_ok=True)
        raise

    print(f"Media provisioned: {target}")
    return target
