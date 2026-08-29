"""Host-side level selectors used by the launch orchestration commands."""

from __future__ import annotations

import argparse

LEVEL_NAMES = {
    "hangar": 0,
    "warehouse": 12,
}
LEVEL_COUNT = 13


def parse_level(value: str) -> int:
    """Resolve a known level name or a THPS2 level index."""

    normalized = value.strip().casefold()
    if normalized in LEVEL_NAMES:
        return LEVEL_NAMES[normalized]
    try:
        level = int(normalized, 0)
    except ValueError as exc:
        raise argparse.ArgumentTypeError(
            "level must be a known name or an integer from 0 through 12"
        ) from exc
    if not 0 <= level < LEVEL_COUNT:
        raise argparse.ArgumentTypeError("level index must be between 0 and 12")
    return level
