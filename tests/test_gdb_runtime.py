import struct
import sys
import types
from pathlib import Path

if "gdb" not in sys.modules:
    class FakeBreakpoint:
        def __init__(self, *args, **kwargs):
            self.enabled = True

    gdb_stub = types.ModuleType("gdb")
    gdb_stub.error = RuntimeError
    gdb_stub.GdbError = RuntimeError
    gdb_stub.Breakpoint = FakeBreakpoint
    gdb_stub.BP_BREAKPOINT = 1
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
from opentony.frame import FrameClock
from opentony.memory import Memory
from opentony.physics import PhysicsProbe
from opentony.player import PlayerView


class FakeInferior:
    def __init__(self):
        self.data = bytearray(0x4000)

    def read_memory(self, address, size):
        return self.data[address:address + size]

    def write_memory(self, address, data):
        self.data[address:address + len(data)] = data


def test_typed_memory_preserves_float_bits_and_supports_writes():
    inferior = FakeInferior()
    inferior.data[0x10:0x14] = struct.pack("<I", 0x41460000)
    inferior.data[0x20:0x2C] = struct.pack("<3f", 1.0, -2.0, 3.5)
    memory = Memory(inferior)

    bits = memory.f32_bits(0x10)
    assert bits.value == 12.375
    assert bits.bits == 0x41460000
    assert memory.vec3(0x20) == (1.0, -2.0, 3.5)

    memory.write_u32(0x30, 0x12345678)
    memory.write_f32(0x34, 0.25)
    assert memory.u32(0x30) == 0x12345678
    assert memory.f32(0x34) == 0.25


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


def test_player_view_and_physics_probe_keep_candidate_names_and_raw_bits():
    inferior = FakeInferior()
    inferior.data[0x200:0x204] = struct.pack("<I", 0x100)
    inferior.data[0x108:0x114] = struct.pack("<3f", 1.0, -2.0, 3.5)
    inferior.data[0x1BC:0x1C8] = struct.pack("<3f", 4.0, 5.0, 6.0)
    inferior.data[0x31B8:0x31BC] = struct.pack("<I", 1)
    inferior.data[0x31C4:0x31C8] = struct.pack("<I", 2)
    memory = Memory(inferior)
    view = PlayerView.current(memory)

    assert view is not None
    assert view.candidate_position == (1.0, -2.0, 3.5)
    assert view.candidate_velocity == (4.0, 5.0, 6.0)
    assert view.physics_state == 1

    inferior.data[0x100:0x108] = struct.pack("<2I", 0x2222, 0x100)
    context = Context(CallContext(memory, registers={"esp": 0x100, "eip": 0x300}), memory)
    probe = PhysicsProbe(count=1)
    probe.on_hit(context)
    assert probe.hits == 1
    assert probe.remaining == 0
