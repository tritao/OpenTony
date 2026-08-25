"""Access generated symbol knowledge from inside GDB."""

from __future__ import annotations

try:
    import knowledge as _generated
except ImportError as exc:  # pragma: no cover - exercised by a clean GDB checkout
    raise ImportError(
        "generated GDB knowledge is missing; run `tony gdb generate` before loading re/gdb/bootstrap.gdb"
    ) from exc

BUILD_SHA256 = _generated.BUILD_SHA256
FUNCTIONS = _generated.FUNCTIONS
GLOBALS = _generated.GLOBALS
DATA = _generated.DATA
STRINGS = _generated.STRINGS
FUNCTION_METADATA = _generated.FUNCTIONS_METADATA
GLOBAL_METADATA = _generated.GLOBALS_METADATA
FUNCTION_ALIASES = _generated.FUNCTIONS_ALIASES
GLOBAL_ALIASES = _generated.GLOBALS_ALIASES


def _lookup(name: str, values: dict[str, int], aliases: dict[str, str]) -> int:
    canonical = aliases.get(name, name)
    try:
        return values[canonical]
    except KeyError as exc:
        raise KeyError(f"unknown symbol {name!r}") from exc


def function_address(name: str) -> int:
    return _lookup(name, FUNCTIONS, FUNCTION_ALIASES)


def global_address(name: str) -> int:
    return _lookup(name, GLOBALS, GLOBAL_ALIASES)


def known_function_addresses():
    """Yield legacy GDB aliases and their generated metadata in source order."""

    for alias, canonical in FUNCTION_ALIASES.items():
        metadata = FUNCTION_METADATA.get(canonical, {})
        evidence = metadata.get("evidence", [])
        yield alias, (
            FUNCTIONS[canonical],
            metadata.get("description", canonical),
            evidence[0] if evidence else "",
        )
