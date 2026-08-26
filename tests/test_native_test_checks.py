from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
NATIVE_TEST_ROOTS = (ROOT / "src", ROOT / "tests", ROOT / "re")


def test_native_tests_use_always_on_checks() -> None:
    offenders: list[str] = []
    for root in NATIVE_TEST_ROOTS:
        for path in root.rglob("*test.cpp"):
            source = path.read_text(encoding="utf-8")
            if "#include <cassert>" in source or "assert(" in source:
                offenders.append(str(path.relative_to(ROOT)))

    assert offenders == [], (
        "native tests must use tests/test_check.hpp and CHECK(...), because "
        f"standard assert is disabled by NDEBUG: {offenders}"
    )
