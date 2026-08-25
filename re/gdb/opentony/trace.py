"""Dependency-free JSONL runtime trace writer."""

from __future__ import annotations

import json
from pathlib import Path
from typing import Self

from .frame import frame_clock
from .knowledge import BUILD_SHA256


class JsonlWriter:
    FORMAT = "opentony-runtime-trace-v1"

    def __init__(self, path: str | Path, experiment: str, *, overwrite: bool = False):
        self.path = Path(path).expanduser()
        self.experiment = experiment
        self.overwrite = overwrite
        self._stream = None
        self._closed = False

    def open(self) -> None:
        if self._stream is not None:
            return
        if self.path.exists() and not self.overwrite:
            raise OSError(f"refusing to overwrite {self.path}; use --force if intended")
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self._stream = self.path.open("w", encoding="utf-8")
        self.write(
            {
                "type": "header",
                "format": self.FORMAT,
                "binary_sha256": BUILD_SHA256,
                "experiment": self.experiment,
            }
        )

    def write(self, record: dict) -> None:
        if self._closed:
            raise OSError(f"trace writer is closed: {self.path}")
        if self._stream is None:
            self.open()
        self._stream.write(json.dumps(record, sort_keys=True, allow_nan=True) + "\n")
        self._stream.flush()

    def event(self, record: dict) -> None:
        event = dict(record)
        event.setdefault("type", "event")
        self.write(event)

    def close(self, *, frames: int | None = None) -> None:
        if self._closed:
            return
        if self._stream is None:
            self.open()
        self.write({"type": "end", "frames": frame_clock.value if frames is None else frames})
        self._stream.close()
        self._closed = True

    def __enter__(self) -> Self:
        self.open()
        return self

    def __exit__(self, _exception_type, _exception, _traceback) -> None:
        self.close()
