from types import SimpleNamespace

from tony import native_progress


def test_native_progress_validation_accepts_tested_mapping(tmp_path, monkeypatch):
    source = tmp_path / "source.cpp"
    test = tmp_path / "test.cpp"
    evidence = tmp_path / "evidence.md"
    for path in (source, test, evidence):
        path.write_text("tracked\n")

    documents = {
        "progress.yml": {
            "version": 1,
            "functions": [{
                "address": 0x401000,
                "name": "Example",
                "status": "tested",
                "sources": [str(source)],
                "tests": [str(test)],
                "evidence": [str(evidence)],
            }],
        },
        "re/symbols/functions.yml": {"functions": [{"address": 0x401000, "name": "Example"}]},
    }
    monkeypatch.setattr(native_progress, "load_yaml", lambda path: documents[str(path)])

    errors, counts = native_progress.validate_native_progress("progress.yml")

    assert errors == []
    assert counts == {"functions": 1}


def test_native_progress_validation_rejects_overclaim(monkeypatch):
    documents = {
        "progress.yml": {
            "version": 1,
            "functions": [{
                "address": 0x401000,
                "name": "WrongName",
                "status": "trace-validated",
                "sources": [],
                "tests": [],
                "evidence": [],
            }],
        },
        "re/symbols/functions.yml": {"functions": [{"address": 0x401000, "name": "Example"}]},
    }
    monkeypatch.setattr(native_progress, "load_yaml", lambda path: documents[str(path)])

    errors, _counts = native_progress.validate_native_progress("progress.yml")

    assert any("name does not match" in error for error in errors)
    assert any("requires at least one test" in error for error in errors)
    assert any("requires trace/runtime evidence" in error for error in errors)


def test_native_verify_reports_summary(monkeypatch, capsys):
    monkeypatch.setattr(native_progress, "validate_native_progress", lambda: ([], {"functions": 3}))

    assert native_progress.native_verify(SimpleNamespace()) == 0
    assert "3 function mappings" in capsys.readouterr().out
