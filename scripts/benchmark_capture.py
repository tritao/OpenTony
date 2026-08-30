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
    parser.add_argument(
        "--backend",
        action="append",
        choices=("gdb", "inproc", "hybrid"),
        dest="backends",
        help="backend to measure (repeat; defaults to gdb and inproc)",
    )
    parser.add_argument("--runs", type=int, default=1)
    parser.add_argument("--output-root", type=Path, default=Path("build/benchmarks/capture"))
    parser.add_argument(
        "--no-forensics",
        action="store_true",
        help="omit diagnostic GDB probes so backend hot paths are comparable",
    )
    args = parser.parse_args()
    if args.runs <= 0:
        parser.error("--runs must be positive")
    backends = args.backends or ["gdb", "inproc"]
    results = []
    for backend in backends:
        for run in range(args.runs):
            output = args.output_root / backend / f"{args.scenario}-{run}.otrec"
            prefix = args.output_root / ".prefixes" / f"{backend}-{run}"
            prefix.parent.mkdir(parents=True, exist_ok=True)
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
                "--headless-prefix",
                str(prefix),
            ]
            if args.no_forensics:
                command.append("--no-forensics")
            started = time.monotonic()
            completed = subprocess.run(command, cwd=ROOT, check=False)
            elapsed = time.monotonic() - started
            results.append({"backend": backend, "run": run, "seconds": elapsed, "returncode": completed.returncode})
    summary = {}
    for backend in backends:
        samples = [item["seconds"] for item in results if item["backend"] == backend]
        successful = [item for item in results if item["backend"] == backend and item["returncode"] == 0]
        if samples:
            summary[backend] = {
                "runs": len(samples),
                "successful_runs": len(successful),
                "failed_runs": len(samples) - len(successful),
                "min_seconds": min(samples),
                "max_seconds": max(samples),
                "mean_seconds": sum(samples) / len(samples),
            }
    print(json.dumps({"results": results, "summary": summary}, indent=2, sort_keys=True))
    return 0 if all(item["returncode"] == 0 for item in results) else 1


if __name__ == "__main__":
    raise SystemExit(main())
