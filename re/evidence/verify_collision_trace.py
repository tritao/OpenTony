#!/usr/bin/env python3
"""Verify arithmetic invariants in a recorded collision runtime trace.

This is intentionally a small evidence checker rather than a second query
implementation.  It checks only relationships directly established by the
PC trace: static contact/distance interpolation and the dynamic fallback
contact calculation.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any


S32_MASK = 0xFFFFFFFF
SENTINEL = 0x7FFFFFFF
PARAMETER_SCALE = 0x4000


def signed32(value: int) -> int:
    value &= S32_MASK
    return value - 0x100000000 if value & 0x80000000 else value


def wrapped_add(lhs: int, rhs: int) -> int:
    return signed32(lhs + rhs)


def wrapped_sub(lhs: int, rhs: int) -> int:
    return signed32(lhs - rhs)


def trunc_div(numerator: int, denominator: int) -> int:
    if denominator == 0:
        raise ValueError("division by zero")
    magnitude = abs(numerator) // abs(denominator)
    return -magnitude if (numerator < 0) != (denominator < 0) else magnitude


def arithmetic_shift_right(value: int, shift: int) -> int:
    return signed32(value) >> shift


def fixed_vector(values: list[int]) -> list[int]:
    return [signed32(value) for value in values]


def fail(path: Path, line_number: int, message: str) -> None:
    raise SystemExit(f"{path}:{line_number}: {message}")


def verify_static(record: dict[str, Any], path: Path, line_number: int) -> None:
    start = fixed_vector(record["start_raw"])
    end = fixed_vector(record["end_raw"])
    actual_contact = fixed_vector(record["contact_raw"])
    parameter = int(record["hit_parameter"])
    line_length = int(record["line_length"])
    distance = int(record["hit_distance"])
    if parameter == SENTINEL:
        return
    if not 0 <= parameter <= PARAMETER_SCALE:
        fail(path, line_number, f"static parameter outside range: {parameter}")
    expected_contact = [
        wrapped_add(start[axis], trunc_div((end[axis] - start[axis]) * parameter,
                                           PARAMETER_SCALE))
        for axis in range(3)
    ]
    if actual_contact != expected_contact:
        fail(path, line_number,
             f"static contact mismatch: {actual_contact} != {expected_contact}")
    expected_distance = trunc_div(line_length * parameter, PARAMETER_SCALE)
    if distance != expected_distance:
        fail(path, line_number,
             f"static distance mismatch: {distance} != {expected_distance}")
    normal = [int(value) for value in record["normal_s16"]]
    magnitude_squared = sum(value * value for value in normal)
    if not 4094 * 4094 <= magnitude_squared <= 4098 * 4098:
        fail(path, line_number, f"normal magnitude outside observed range: {normal}")


def verify_dynamic(record: dict[str, Any], path: Path, line_number: int) -> None:
    query = record.get("query_after")
    if not query or not query.get("hit_body"):
        return
    parameter = int(query["hit_parameter"])
    if parameter != SENTINEL:
        return
    start = fixed_vector(query["start_raw"])
    end = fixed_vector(query["end_raw"])
    distance = int(query["hit_distance"])
    line_length = int(query["line_length"])
    interpolation = trunc_div(distance * 0x1000, line_length)
    expected_contact = [
        wrapped_add(
            start[axis],
            arithmetic_shift_right(wrapped_sub(end[axis], start[axis]), 12)
            * interpolation,
        )
        for axis in range(3)
    ]
    actual_contact = fixed_vector(query["contact_raw"])
    if actual_contact != expected_contact:
        fail(path, line_number,
             f"dynamic contact mismatch: {actual_contact} != {expected_contact}")


def verify(path: Path) -> tuple[int, int, int]:
    wrapper_queries = wrapper_hits = dynamic_hits = 0
    with path.open(encoding="utf-8") as stream:
        for line_number, line in enumerate(stream, 1):
            record = json.loads(line)
            kind = record.get("type")
            if kind == "collision_query":
                wrapper_queries += 1
                if record.get("hit"):
                    wrapper_hits += 1
                    verify_static(record, path, line_number)
            elif kind == "collision_dynamic_object_query":
                query = record.get("query_after")
                if query and query.get("hit_body"):
                    dynamic_hits += 1
                    verify_dynamic(record, path, line_number)
    return wrapper_queries, wrapper_hits, dynamic_hits


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("trace", type=Path)
    args = parser.parse_args()
    wrapper_queries, wrapper_hits, dynamic_hits = verify(args.trace)
    print(
        f"verified {wrapper_queries} wrapper queries, {wrapper_hits} wrapper hits, "
        f"and {dynamic_hits} dynamic hits: {args.trace}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
