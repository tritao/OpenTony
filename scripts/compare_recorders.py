#!/usr/bin/env python3
"""Compare two recorder outputs at input, timer/event, and player boundaries."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tony.capture import compare_recordings


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("left")
    parser.add_argument("right")
    parser.add_argument(
        "--scope",
        choices=("all", "snapshots", "qualification"),
        default="all",
    )
    args = parser.parse_args()
    result = compare_recordings(args.left, args.right, scope=args.scope)
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0 if result["equal"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
