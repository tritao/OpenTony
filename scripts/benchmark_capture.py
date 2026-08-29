#!/usr/bin/env python3
"""Measure scenario capture wall time for one or more recorder backends."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("scenario")
    parser.add_argument("--backend", action="append", choices=("gdb", "inproc"), dest="backends")
    parser.add_argument("--runs", type=int, default=1)
    parser.add_argument("--output-root", type=Path, default=Path("build/benchmarks/capture"))
    args = parser.parse_args()
    if args.runs <= 0:
        parser.error("--runs must be positive")
    backends = args.backends or ["gdb", "inproc"]
    results = []
    for backend in backends:
        for run in range(args.runs):
            output = args.output_root / backend / f"{args.scenario}-{run}.otrec"
            command = [
                sys.executable,
                "-m",
                "tony",
                "scenario",
                "capture",
                args.scenario,
                "--backend",
                backend,
                "--output",
                str(output),
                "--force",
            ]
            started = time.monotonic()
            completed = subprocess.run(command, cwd=ROOT, check=False)
            elapsed = time.monotonic() - started
            results.append({"backend": backend, "run": run, "seconds": elapsed, "returncode": completed.returncode})
    print(json.dumps(results, indent=2, sort_keys=True))
    return 0 if all(item["returncode"] == 0 for item in results) else 1


if __name__ == "__main__":
    raise SystemExit(main())
