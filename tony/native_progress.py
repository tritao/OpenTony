from __future__ import annotations

from types import SimpleNamespace

from .common import load_yaml, resolve

PROGRESS_PATH = "re/native/functions.yml"
STATUSES = ("unmodeled", "modeled", "tested", "trace-validated", "integrated")


def load_native_progress(path: str = PROGRESS_PATH) -> dict[int, dict]:
    document = load_yaml(path)
    return {int(item["address"]): item for item in document.get("functions", [])}


def validate_native_progress(path: str = PROGRESS_PATH) -> tuple[list[str], dict[str, int]]:
    errors: list[str] = []
    document = load_yaml(path)
    if document.get("version") != 1:
        errors.append(f"{path}: version must be 1")
    symbols = {
        int(item["address"]): item["name"]
        for item in load_yaml("re/symbols/functions.yml").get("functions", [])
    }
    seen = set()
    entries = document.get("functions", [])
    for item in entries:
        address = item.get("address")
        context = f"{path}:{item.get('name', address)}"
        if not isinstance(address, int) or address not in symbols:
            errors.append(f"{context}: address is not a tracked function")
            continue
        if address in seen:
            errors.append(f"{context}: duplicate address 0x{address:08x}")
        seen.add(address)
        if item.get("name") != symbols[address]:
            errors.append(f"{context}: name does not match {symbols[address]!r}")
        status = item.get("status")
        if status not in STATUSES:
            errors.append(f"{context}: invalid status {status!r}")
        for key in ("sources", "tests", "evidence"):
            values = item.get(key, [])
            if not isinstance(values, list):
                errors.append(f"{context}: {key} must be a list")
                continue
            for value in values:
                if not isinstance(value, str) or not resolve(value).is_file():
                    errors.append(f"{context}: missing {key} file {value!r}")
        if status in STATUSES[STATUSES.index("tested") :] and not item.get("tests"):
            errors.append(f"{context}: {status} requires at least one test")
        if status in {"trace-validated", "integrated"} and not item.get("evidence"):
            errors.append(f"{context}: {status} requires trace/runtime evidence")
    return errors, {"functions": len(entries)}


def native_verify(_args: SimpleNamespace | None = None) -> int:
    errors, counts = validate_native_progress()
    print(f"native progress: {counts['functions']} function mappings")
    for error in errors:
        print(f"FAIL {error}")
    if errors:
        return 1
    print("native progress: VALID")
    return 0
