from pathlib import Path

from tony.gdb_knowledge import load_symbol_database, render_knowledge


def test_gdb_knowledge_covers_runtime_addresses():
    database = load_symbol_database()

    assert database["FUNCTIONS"]["Skater_PhysicsDispatcher"] == 0x0049DB80
    assert database["FUNCTIONS_ALIASES"]["physics_dispatch"] == "Skater_PhysicsDispatcher"
    assert database["GLOBALS"]["Player"] == 0x0056A858
    assert database["GLOBALS"]["CurrentLevel"] == 0x0056A898
    assert database["GLOBALS"]["KeyboardDevice"] == 0x006A43E0
    assert database["GLOBALS"]["KeyboardState"] == 0x006A43E4
    assert "RawKeyboardMask" not in database["GLOBALS"]


def test_gdb_knowledge_is_a_dependency_free_python_module(tmp_path: Path):
    database = load_symbol_database()
    output = tmp_path / "knowledge.py"
    output.write_text(render_knowledge(database), encoding="utf-8")

    namespace = {}
    exec(output.read_text(encoding="utf-8"), namespace)  # noqa: S102 - generated module smoke test
    assert namespace["FUNCTIONS"]["Game_LevelLoop"] == 0x0046A3A0
    assert namespace["GLOBALS"]["Player"] == 0x0056A858
