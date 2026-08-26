import json
import struct
import sys
import types
from pathlib import Path

if "gdb" not in sys.modules:
    class FakeBreakpoint:
        def __init__(self, *args, **kwargs):
            self.enabled = True

        def is_valid(self):
            return True

    gdb_stub = types.ModuleType("gdb")
    gdb_stub.error = RuntimeError
    gdb_stub.GdbError = RuntimeError
    gdb_stub.Breakpoint = FakeBreakpoint
    gdb_stub.BP_BREAKPOINT = 1
    gdb_stub.BP_WATCHPOINT = 2
    gdb_stub.WP_WRITE = 1
    gdb_stub.write = lambda _text: None
    sys.modules["gdb"] = gdb_stub

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "re/gdb"))

generated_knowledge = types.ModuleType("knowledge")
generated_knowledge.BUILD_SHA256 = "test"
generated_knowledge.FUNCTIONS = {"Skater_PhysicsDispatcher": 0x300}
generated_knowledge.GLOBALS = {"Player": 0x200}
generated_knowledge.DATA = {}
generated_knowledge.STRINGS = {}
generated_knowledge.FUNCTIONS_METADATA = {}
generated_knowledge.GLOBALS_METADATA = {}
generated_knowledge.FUNCTIONS_ALIASES = {"physics_dispatch": "Skater_PhysicsDispatcher"}
generated_knowledge.GLOBALS_ALIASES = {}
sys.modules["knowledge"] = generated_knowledge

from opentony.breakpoint import Context, CountingBreakpoint
from opentony.calling import CallContext
from opentony.collision import (
    _candidate_source_snapshot,
    _linked_object_snapshots,
    _zone_entry_snapshot,
)
from opentony.frame import FrameClock
from opentony.memory import Memory
from opentony.physics import PhysicsProbe, PlayerDiffProbe
from opentony.player import PlayerView
from opentony.position import PositionCommitBreakpoint
from opentony.snapshot import SnapshotStore, format_diff
from opentony.trace import JsonlWriter
from opentony.watchpoint import TonyWatchpoint, WatchpointManager


class FakeInferior:
    def __init__(self):
        self.data = bytearray(0x4000)

    def read_memory(self, address, size):
        return self.data[address:address + size]

    def write_memory(self, address, data):
        self.data[address:address + len(data)] = data


def test_typed_memory_preserves_word_views_and_float_bits():
    inferior = FakeInferior()
    inferior.data[0x10:0x14] = struct.pack("<I", 0x41460000)
    inferior.data[0x20:0x2C] = struct.pack("<3f", 1.0, -2.0, 3.5)
    inferior.data[0x40:0x4C] = struct.pack("<3I", 0xFFFF0000, 0, 0)
    memory = Memory(inferior)

    bits = memory.f32_bits(0x10)
    assert bits.value == 12.375
    assert bits.bits == 0x41460000
    assert memory.fixed16(0x10) == 0x41460000 / 65536.0
    assert memory.u32_vec3(0x20) == (0x3F800000, 0xC0000000, 0x40600000)
    fixed = memory.fixed_vec3(0x10)
    assert fixed.raw == (0x41460000, 0, 0)
    assert fixed.signed == (0x41460000, 0, 0)
    assert fixed.values == (0x41460000 / 65536.0, 0.0, 0.0)
    negative = memory.fixed_vec3(0x40)
    assert memory.word32(0x10)._asdict() == {
        "u32": 0x41460000,
        "s32": 0x41460000,
        "fixed16": 0x41460000 / 65536.0,
        "f32": 12.375,
    }
    assert memory.vec3(0x20) == (1.0, -2.0, 3.5)

    memory.write_u32(0x30, 0x12345678)
    memory.write_f32(0x34, 0.25)
    assert memory.u32(0x30) == 0x12345678
    assert memory.f32(0x34) == 0.25
    assert negative.x.signed == -65536
    assert negative.x.value == -1.0


def test_collision_probe_reads_bounded_linked_object_prefix():
    inferior = FakeInferior()
    first = 0x500
    second = 0x600
    struct.pack_into(
        "<IHH3i3hH3xBI",
        inferior.data,
        first,
        0,
        0x0025,
        0x1234,
        4096,
        -8192,
        12288,
        0x0100,
        -0x0100,
        0x0200,
        171,
        6,
        second,
    )
    struct.pack_into(
        "<IHH3i3hH3xBI",
        inferior.data,
        second,
        0,
        0x0020,
        0x1235,
        0,
        0,
        0,
        0,
        0,
        0,
        172,
        6,
        0,
    )
    snapshot = _linked_object_snapshots(first, Memory(inferior))

    assert snapshot["termination"] == "null"
    assert snapshot["next_unread"] is None
    assert len(snapshot["nodes"]) == 2
    assert snapshot["nodes"][0]["position_s32"] == [4096, -8192, 12288]
    assert snapshot["nodes"][0]["angles_s16"] == [0x100, -0x100, 0x200]
    assert snapshot["nodes"][0]["model_index"] == 171
    assert snapshot["nodes"][0]["previous"] is None
    assert snapshot["nodes"][1]["next"] is None


def test_collision_loader_source_snapshot_reads_counted_entries_and_terminator():
    inferior = FakeInferior()
    source = 0x700
    struct.pack_into("<3I", inferior.data, source, 0x10, 0x20, 2)
    struct.pack_into("<3I", inferior.data, source + 0x0C, 0x1111, 0x2222, 0)

    snapshot = _candidate_source_snapshot(source, Memory(inferior))

    assert snapshot is not None
    assert snapshot["entry_count"] == 2
    assert snapshot["entries"] == [0x1111, 0x2222]
    assert snapshot["terminator"] == 0


def test_collision_loader_zone_snapshot_uses_live_prefix_offsets():
    inferior = FakeInferior()
    zone = 0x1000
    struct.pack_into(
        "<I4iI4x2h",
        inferior.data,
        zone,
        1,
        -10,
        -20,
        30,
        40,
        5,
        20,
        20,
    )

    snapshot = _zone_entry_snapshot(zone, Memory(inferior))

    assert snapshot == {
        "address": "0x00001000",
        "present_word": 1,
        "min_x": -10,
        "min_z": -20,
        "max_x": 30,
        "max_z": 40,
        "cell_divisor": 5,
        "cell_count_x": 20,
        "cell_count_z": 20,
    }


def test_entry_call_context_reads_stack_arguments_and_this_pointer():
    inferior = FakeInferior()
    inferior.data[0x100:0x10C] = struct.pack("<3I", 0x2222, 12, 3)
    memory = Memory(inferior)
    context = CallContext(memory, registers={"esp": 0x100, "ecx": 0x05F39530, "eax": 7})

    assert context.return_address() == 0x2222
    assert context.caller() == 0x2222
    assert context.arg(0) == 12
    assert context.arg(1) == 3
    assert context.this_ptr() == 0x05F39530
    assert context.return_value() == 7


def test_callsite_call_context_reads_pushed_arguments_without_skipping_first():
    inferior = FakeInferior()
    inferior.data[0x100:0x10C] = struct.pack("<3I", 11, 22, 33)
    memory = Memory(inferior)
    context = CallContext(memory, registers={"esp": 0x100, "ecx": 0x05F39530})

    assert context.callsite_arg(0) == 11
    assert context.callsite_arg(1) == 22
    assert context.callsite_arg(2) == 33


def test_counting_breakpoint_and_frame_clock_share_context_state():
    inferior = FakeInferior()
    memory = Memory(inferior)
    context = Context(CallContext(memory, registers={"esp": 0x100, "eip": 0x400}), memory)
    clock = FrameClock()
    seen = []

    class Probe(CountingBreakpoint):
        def on_count(self, ctx):
            seen.append(ctx.eip)
            return True

    probe = Probe(0x1234, count=2)
    clock.tick()
    probe.on_hit(context)
    clock.tick()
    probe.on_hit(context)

    assert seen == [0x400, 0x400]
    assert probe.hits == 2
    assert probe.remaining == 0
    assert probe.enabled is False


def test_position_commit_probe_records_arguments_and_player_state():
    inferior = FakeInferior()
    memory = Memory(inferior)
    player = 0x500
    inferior.data[0x100:0x10C] = struct.pack("<3I", 11, 22, 33)
    inferior.data[player + 0x08:player + 0x14] = struct.pack("<3I", 101, 102, 103)
    inferior.data[player + 0x4C:player + 0x58] = struct.pack("<3I", 401, 402, 403)
    inferior.data[player + 0xBC:player + 0xC8] = struct.pack("<3I", 201, 202, 203)
    inferior.data[player + 0x30B8:player + 0x30C0] = struct.pack("<2I", 4, 9)
    events = []

    class Writer:
        def event(self, record):
            events.append(record)

    probe = PositionCommitBreakpoint(0x450, "test-callsite", 1, writer=Writer())
    context = Context(
        CallContext(memory, registers={"esp": 0x100, "ecx": player, "eip": 0x450}),
        memory,
    )

    probe.on_count(context)

    assert events[0]["type"] == "position_commit"
    assert events[0]["player"] == "0x00000500"
    assert events[0]["argument_values"] == ["0x0000000b", "0x00000016", "0x00000021"]
    assert events[0]["arguments"][0]["address"] == "0x00000100"
    assert events[0]["caller_return"] is None
    assert events[0]["position_before"]["raw"] == [101, 102, 103]
    assert events[0]["position_before"]["fixed"] == [101 / 65536.0, 102 / 65536.0, 103 / 65536.0]
    assert events[0]["vector_4c"]["raw"] == [401, 402, 403]
    assert events[0]["position_history"]["raw"] == [201, 202, 203]
    assert events[0]["physics_state"] == 4


def test_player_view_exposes_fixed_position_and_integer_state_fields():
    inferior = FakeInferior()
    inferior.data[0x200:0x204] = struct.pack("<I", 0x100)
    inferior.data[0x108:0x114] = struct.pack("<3I", 0x00018000, 0x00008000, 0x3F800000)
    inferior.data[0x1BC:0x1C8] = struct.pack("<3I", 0x00004000, 0x0000C000, 0x41200000)
    inferior.data[0x31B8:0x31BC] = struct.pack("<I", 1)
    inferior.data[0x31C4:0x31C8] = struct.pack("<I", 2)
    memory = Memory(inferior)
    view = PlayerView.current(memory)

    assert view is not None
    assert view.position_raw == (0x00018000, 0x00008000, 0x3F800000)
    assert view.position.values == (1.5, 0.5, 16256.0)
    assert view.position.x.raw == 0x00018000
    assert view.position_history_raw == (0x00004000, 0x0000C000, 0x41200000)
    assert view.position_history.values == (0.25, 0.75, 16672.0)
    assert view.physics_state == 1
    assert view.unknown_state == 2

    inferior.data[0x100:0x108] = struct.pack("<2I", 0x2222, 0x100)
    context = Context(CallContext(memory, registers={"esp": 0x100, "eip": 0x300}), memory)
    events = []

    class Writer:
        def event(self, record):
            events.append(record)

    probe = PhysicsProbe(count=1, writer=Writer())
    probe.on_hit(context)
    assert probe.hits == 1
    assert probe.remaining == 0
    assert events[0]["position_raw"] == [0x00018000, 0x00008000, 0x3F800000]
    assert events[0]["position_fixed"] == [1.5, 0.5, 16256.0]
    assert events[0]["position_history_raw"] == [0x00004000, 0x0000C000, 0x41200000]
    assert events[0]["position_history_fixed"] == [0.25, 0.75, 16672.0]
    assert events[0]["physics_state"] == 1
    assert "candidate_position" not in events[0]


def test_player_diff_probe_records_changed_relative_words():
    inferior = FakeInferior()
    memory = Memory(inferior)
    player = 0x500
    inferior.data[player + 0x30B8:player + 0x30BC] = struct.pack("<I", 0)
    events = []

    class Writer:
        def event(self, record):
            events.append(record)

    probe = PlayerDiffProbe(count=2, writer=Writer())
    context = Context(
        CallContext(memory, registers={"esp": 0x100, "ecx": player, "eip": 0x300}),
        memory,
    )
    probe.on_count(context)
    inferior.data[player + 0x104:player + 0x108] = struct.pack("<I", 0x12345678)
    probe.on_count(context)

    assert events[0]["previous_available"] is False
    assert events[0]["changed_words"] == []
    assert events[1]["previous_available"] is True
    assert {entry["offset"] for entry in events[1]["changed_words"]} == {"0x0104"}
    assert events[1]["changed_words"][0]["before"] == 0
    assert events[1]["changed_words"][0]["after"] == 0x12345678


def test_snapshot_diff_is_raw_first_with_all_word_heuristics():
    inferior = FakeInferior()
    inferior.data[0x300:0x308] = struct.pack("<2I", 0, 0)
    store = SnapshotStore()
    store.capture("idle", 0x300, 8, memory=Memory(inferior))

    inferior.data[0x300:0x308] = struct.pack("<2f", 1.0, 2.5)
    store.capture("moving", 0x300, 8, memory=Memory(inferior))
    entries = store.diff("idle", "moving")

    assert [entry.offset for entry in entries] == [0, 4]
    assert entries[0].before_u32 == 0
    assert entries[0].after_f32 == 1.0
    assert entries[0].after_s32 == 0x3F800000
    assert entries[0].after_fixed16 == 16256.0
    assert "u32 0 -> 1065353216" in format_diff(entries)[0]
    assert "s32 0 -> 1065353216" in format_diff(entries)[0]
    assert "fixed16 0.0 -> 16256.0" in format_diff(entries)[0]
    assert "f32 0.0 -> 2.5" in format_diff(entries)[1]


def test_jsonl_writer_emits_header_events_and_footer(tmp_path):
    path = tmp_path / "trace.jsonl"
    writer = JsonlWriter(path, "warehouse-ollie")
    writer.open()
    writer.event({"type": "physics", "frame": 4, "player": "0x100"})
    writer.close(frames=8)

    records = [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines()]
    assert records[0] == {
        "binary_sha256": "test",
        "experiment": "warehouse-ollie",
        "format": "opentony-runtime-trace-v1",
        "type": "header",
    }
    assert records[1]["frame"] == 4
    assert records[-1] == {"frames": 8, "type": "end"}


def test_jsonl_writer_tracks_active_session_trace(tmp_path, monkeypatch):
    session_dir = tmp_path / "session"
    monkeypatch.setenv("TONY_SESSION_DIR", str(session_dir))
    writer = JsonlWriter(tmp_path / "trace.jsonl", "warehouse-ollie")
    writer.open()
    marker = session_dir / "trace.active"
    assert marker.is_file()
    writer.close(frames=1)
    assert not marker.exists()


def test_watchpoint_records_raw_and_all_heuristic_old_new_values():
    inferior = FakeInferior()
    inferior.data[0x350:0x354] = struct.pack("<I", 0)
    memory = Memory(inferior)
    watchpoint = TonyWatchpoint(0x350, label="player+0x08", memory=memory)

    inferior.data[0x350:0x354] = struct.pack("<I", 0x3D23A841)
    context = Context(CallContext(memory, registers={"esp": 0x100, "eip": 0x305}), memory)
    record = watchpoint.record(context)

    assert record["old_raw"] == "00000000"
    assert record["new_raw"] == "41a8233d"
    assert record["old"] == {"u32": 0, "s32": 0, "fixed16": 0.0, "f32": 0.0}
    assert record["new"]["u32"] == 0x3D23A841
    assert record["new"]["s32"] == 0x3D23A841
    assert record["new"]["fixed16"] == 0x3D23A841 / 65536.0
    assert record["function"] == "Skater_PhysicsDispatcher+0x5"
    assert record["address"] == "0x00000350"


def test_watchpoint_once_latches_after_recording_without_disabling(monkeypatch):
    inferior = FakeInferior()
    memory = Memory(inferior)
    watchpoint = TonyWatchpoint(0x350, memory=memory, once=True)
    inferior.data[0x350:0x354] = struct.pack("<I", 1)
    context = Context(CallContext(memory, registers={"esp": 0x100, "eip": 0x305}), memory)
    monkeypatch.setattr("opentony.watchpoint.Context.capture", lambda _memory: context)

    assert watchpoint.stop() is False
    assert watchpoint.enabled is True
    assert watchpoint.events == 1
    inferior.data[0x350:0x354] = struct.pack("<I", 2)
    assert watchpoint.stop() is False
    assert watchpoint.events == 1
    assert watchpoint.latched is True


def test_watchpoint_limit_latches_without_unbounded_logging(monkeypatch):
    inferior = FakeInferior()
    memory = Memory(inferior)
    watchpoint = TonyWatchpoint(0x350, memory=memory, limit=2)
    context = Context(CallContext(memory, registers={"esp": 0x100, "eip": 0x305}), memory)
    monkeypatch.setattr("opentony.watchpoint.Context.capture", lambda _memory: context)

    for value in (1, 2):
        inferior.data[0x350:0x354] = struct.pack("<I", value)
        assert watchpoint.stop() is False
    assert watchpoint.enabled is True
    assert watchpoint.events == 2
    assert watchpoint.latched is True
    inferior.data[0x350:0x354] = struct.pack("<I", 3)
    assert watchpoint.stop() is False
    assert watchpoint.events == 2


def test_watchpoint_manager_tracks_active_watches_and_writer_shutdown():
    manager = WatchpointManager()
    inferior = FakeInferior()
    memory = Memory(inferior)
    first = TonyWatchpoint(0x350, memory=memory, writer="trace-a")
    second = TonyWatchpoint(0x354, memory=memory, writer="trace-b")
    manager.watchpoints.extend((first, second))

    assert manager.active() == [first, second]
    assert manager.available() == 2
    manager.disable_writer("trace-a")
    assert manager.active() == [second]


def test_watchpoint_manager_rejects_a_fifth_active_hardware_watch():
    manager = WatchpointManager()
    inferior = FakeInferior()
    memory = Memory(inferior)
    for offset in range(4):
        manager.arm(0x350 + offset * 4, memory=memory)

    assert manager.available() == 0
    try:
        manager.arm(0x360, memory=memory)
    except RuntimeError as exc:
        assert "four OpenTony hardware watchpoint slots" in str(exc)
    else:
        raise AssertionError("expected the fifth hardware watchpoint to be rejected")
