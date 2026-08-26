from pathlib import Path
from types import SimpleNamespace

import yaml

from tony import recovered_types


def write_types(root: Path, document: dict) -> None:
    root.mkdir(parents=True, exist_ok=True)
    (root / "test.yml").write_text(yaml.safe_dump(document, sort_keys=False))


def test_recovered_type_validation_accepts_partial_fixed_layout(tmp_path: Path):
    evidence = tmp_path / "evidence.md"
    evidence.write_text("observed\n")
    write_types(
        tmp_path / "types",
        {
            "version": 1,
            "types": [
                {
                    "name": "Example",
                    "kind": "fixed_layout",
                    "size": 16,
                    "confidence": "observed",
                    "fields": [
                        {
                            "offset": 4,
                            "name": "values",
                            "type": "array<i32,3>",
                            "confidence": "confirmed",
                            "evidence": [str(evidence)],
                        }
                    ],
                }
            ],
        },
    )

    errors, counts = recovered_types.validate_type_documents(tmp_path / "types")

    assert errors == []
    assert counts == {"files": 1, "types": 1, "fields": 1}


def test_recovered_type_validation_rejects_noncanonical_and_overlapping_fields(tmp_path: Path):
    evidence = tmp_path / "evidence.md"
    evidence.write_text("observed\n")
    write_types(
        tmp_path / "types",
        {
            "version": 1,
            "types": [
                {
                    "name": "Broken",
                    "kind": "fixed_layout",
                    "size": 8,
                    "confidence": "observed",
                    "fields": [
                        {
                            "offset": 0,
                            "name": "word",
                            "type": "u32",
                            "confidence": "observed",
                            "evidence": [str(evidence)],
                        },
                        {
                            "offset": 6,
                            "name": "legacy_spelling",
                            "type": "int32",
                            "confidence": "observed",
                            "evidence": [str(evidence)],
                        },
                        {
                            "offset": 2,
                            "name": "overlap",
                            "type": "u32",
                            "confidence": "observed",
                            "evidence": [str(evidence)],
                        },
                    ],
                }
            ],
        },
    )

    errors, _counts = recovered_types.validate_type_documents(tmp_path / "types")

    assert any("unknown referenced type 'int32'" in error for error in errors)
    assert any("overlaps word" in error for error in errors)


def test_types_verify_reports_repository_summary(monkeypatch, capsys):
    monkeypatch.setattr(
        recovered_types,
        "validate_type_documents",
        lambda: ([], {"files": 2, "types": 3, "fields": 4}),
    )

    assert recovered_types.types_verify(SimpleNamespace()) == 0
    assert "3 types, 4 fields across 2 files" in capsys.readouterr().out


def test_ghidra_plan_is_conservative_and_preserves_declared_extent(tmp_path: Path):
    evidence = tmp_path / "evidence.md"
    evidence.write_text("observed\n")
    write_types(
        tmp_path / "types",
        {
            "version": 1,
            "types": [
                {
                    "name": "Example",
                    "kind": "fixed_layout",
                    "size": 16,
                    "confidence": "provisional",
                    "aliases": ["OldExample"],
                    "fields": [
                        {"offset": 0, "name": "known", "type": "u32", "confidence": "observed", "evidence": [str(evidence)]},
                        {"offset": 4, "name": "guess", "type": "u32", "confidence": "inferred", "evidence": [str(evidence)]},
                    ],
                },
                {"name": "Stream", "kind": "variable_record", "confidence": "observed", "fields": []},
            ],
        },
    )

    plan = recovered_types.ghidra_type_plan(tmp_path / "types")

    assert plan["types"] == [
        {"name": "Example", "kind": "fixed_layout", "size": 16, "description": "", "fields": [
            {"name": "known", "offset": 0, "type": "u32", "size": 4, "comment": ""}
        ]}
    ]
    assert plan["aliases"] == [{"name": "OldExample", "target": "Example"}]
    assert {item["name"] for item in plan["skipped"]} == {"Example.guess", "Stream"}
