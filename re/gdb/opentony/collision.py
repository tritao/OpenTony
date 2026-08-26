"""Bounded runtime observations for the shared level collision query."""

from __future__ import annotations

from .breakpoint import Context, TonyBreakpoint
from .knowledge import function_name_at
from .player import PlayerView

QUERY_WRAPPER = 0x00466090
QUERY_RETURN = 0x0046609F
COLLISION_MODEL_TABLE = 0x0056D43C
COLLISION_LINKED_ROOT = 0x0056AF40
COLLISION_ZONE_TABLE = 0x00567F80
COLLISION_CANDIDATE_TABLE = 0x00567FA0
COLLISION_FACE_CACHE = 0x005643B0
COLLISION_MODEL_CACHE = 0x00567A70


def _signed16(value: int) -> int:
    return value - 0x10000 if value & 0x8000 else value


def _signed32(value: int) -> int:
    return value - 0x100000000 if value & 0x80000000 else value


def _vec_words(address: int, memory) -> list[int]:
    return [memory.u32(address + offset) for offset in (0, 4, 8)]


def _short_vec(address: int, memory) -> list[int]:
    return [_signed16(memory.u16(address + offset)) for offset in (0, 2, 4)]


def _short_words(address: int, count: int, memory) -> list[int]:
    return [_signed16(memory.u16(address + offset * 2)) for offset in range(count)]


def _u32_words(address: int, count: int, memory) -> list[int] | None:
    if not address or not memory.valid(address + count * 4 - 1):
        return None
    return [memory.u32(address + offset * 4) for offset in range(count)]


def _global_u32(address: int, memory) -> int | None:
    return memory.u32(address) if memory.valid(address + 3) else None


def _linked_object_snapshots(root: int | None, memory, limit: int = 32) -> dict:
    """Read a bounded view of the dynamic collision-object linked list."""

    nodes = []
    address = root or 0
    visited = set()
    termination = "null"
    while address and len(nodes) < limit:
        if address in visited:
            termination = "cycle"
            break
        if not memory.readable(address, 0x24):
            termination = "unreadable"
            break
        visited.add(address)
        next_address = memory.u32(address + 0x20)
        nodes.append(
            {
                "address": f"0x{address:08x}",
                "flags": memory.u16(address + 0x04),
                "query_stamp": _signed16(memory.u16(address + 0x06)),
                "position_raw": _vec_words(address + 0x08, memory),
                "position_s32": [
                    _signed32(value) for value in _vec_words(address + 0x08, memory)
                ],
                "angles_s16": _short_vec(address + 0x14, memory),
                "model_index": memory.u16(address + 0x1A),
                "model_kind": memory.u8(address + 0x1F),
                "next": f"0x{next_address:08x}" if next_address else None,
            }
        )
        address = next_address
    if address and len(nodes) >= limit:
        termination = "limit"
    return {
        "nodes": nodes,
        "termination": termination,
        "next_unread": f"0x{address:08x}" if address else None,
    }


def _zone_entry_snapshot(address: int, memory) -> dict | None:
    if not memory.valid(address + 0x9F):
        return None
    return {
        "address": f"0x{address:08x}",
        "present_word": memory.u32(address),
        "min_x": memory.u32(address + 0x84),
        "min_z": memory.u32(address + 0x88),
        "max_x": memory.u32(address + 0x8C),
        "max_z": memory.u32(address + 0x90),
        "cell_divisor": memory.u32(address + 0x94),
        "cell_count_x": memory.u16(address + 0x9C),
        "cell_count_z": memory.u16(address + 0x9E),
    }


def _scene_roots(memory) -> dict:
    """Capture addresses/ownership markers without walking unbounded memory."""

    linked_root = _global_u32(COLLISION_LINKED_ROOT, memory)
    return {
        "linked_root_value": linked_root,
        "linked_objects": _linked_object_snapshots(linked_root, memory),
        "zone_table_base": f"0x{COLLISION_ZONE_TABLE:08x}",
        "zone0": _zone_entry_snapshot(COLLISION_ZONE_TABLE, memory),
        "candidate_table_base": f"0x{COLLISION_CANDIDATE_TABLE:08x}",
        "candidate0": _global_u32(COLLISION_CANDIDATE_TABLE, memory),
        "model_table_base": f"0x{COLLISION_MODEL_TABLE:08x}",
        "model_table_kind0": _global_u32(COLLISION_MODEL_TABLE, memory),
        "face_cache_base": f"0x{COLLISION_FACE_CACHE:08x}",
        "model_cache_base": f"0x{COLLISION_MODEL_CACHE:08x}",
    }


def _collision_model_geometry(model: int, face: int, model_index: int, memory) -> dict | None:
    """Read only the model/face fields used by 0x00462a20 for one hit."""

    if not model or not memory.valid(model + 0x1F):
        return None
    kind = memory.u8(model + 0x1F)
    slot = COLLISION_MODEL_TABLE + kind * 0x44
    if not memory.valid(slot + 3):
        return None
    table = memory.u32(slot)
    model_data = (
        memory.u32(table + model_index * 4)
        if table and memory.valid(table + model_index * 4 + 3)
        else 0
    )
    if not model_data or not memory.valid(model_data + 7):
        return None
    vertex_count = memory.u16(model_data + 2)
    normal_count = memory.u16(model_data + 4)
    face_count = memory.u16(model_data + 6)
    vertex_base = model_data + 0x1C
    normal_base = vertex_base + vertex_count * 8
    face_base = normal_base + normal_count * 8
    result = {
        "model_table_kind": kind,
        "model_table": f"0x{table:08x}" if table else None,
        "model_data": f"0x{model_data:08x}",
        "model_data_header_words": _u32_words(model_data, 7, memory),
        "model_counts": [vertex_count, normal_count, face_count],
        "model_face_base": f"0x{face_base:08x}",
        "face_record_delta": face - face_base if face else None,
    }
    if not face or not memory.valid(face + 0x0F):
        return result
    face_word_zero = memory.u32(face)
    result["face_base_flags"] = face_word_zero & 0xFFFF
    result["face_length_bytes"] = (face_word_zero >> 16) & 0xFFFF
    result["face_stride_bytes"] = face_word_zero >> 16
    # 0x00462a20 uses cache bytes +4,+5,+6,+7 as vertex indices.  The
    # cache word at +0 is flags; +1 is not the first vertex index.
    vertex_indices = [memory.u8(face + offset) for offset in (4, 5, 6, 7)]
    result["face_normal_index"] = (memory.u16(face + 0x0C) & 0xFFFF) >> 3
    result["face_vertices"] = []
    for index in vertex_indices:
        address = vertex_base + index * 8
        if not memory.valid(address + 5):
            result["face_vertices"] = None
            break
        result["face_vertices"].append([memory.s16(address + offset) for offset in (0, 2, 4)])
    normal_index = result["face_normal_index"]
    normal_address = normal_base + normal_index * 8
    result["face_normal"] = (
        [memory.s16(normal_address + offset) for offset in (0, 2, 4)]
        if normal_index < normal_count and memory.valid(normal_address + 5)
        else None
    )
    return result


class CollisionQueryProbe:
    """Pair wrapper entry/return breakpoints and record one query result."""

    def __init__(self, count: int | None = None, writer=None):
        self.remaining = count
        self.writer = writer
        self.hits = 0
        self._entry = _CollisionQueryEntry(self)
        self._return = _CollisionQueryReturn(self)
        self._entry.writer = writer
        self._return.writer = writer
        self._active: dict[str, object] | None = None

    @property
    def entry(self):
        return self._entry

    @property
    def return_breakpoint(self):
        return self._return

    def _emit(self, record: dict) -> None:
        if self.writer is None:
            TonyBreakpoint.emit(record)
        else:
            self.writer.event(record)

    def begin(self, ctx: Context) -> None:
        query = ctx.arg(0)
        if not ctx.memory.valid(query):
            return
        player = PlayerView.current(ctx.memory)
        caller = ctx.caller()
        self._active = {
            "query": query,
            "mode": ctx.arg(1),
            "caller": caller,
            "caller_name": function_name_at(caller),
            "start_raw": _vec_words(query, ctx.memory),
            "end_raw": _vec_words(query + 0x0C, ctx.memory),
            "query_flags": ctx.memory.u8(query + 0x88),
            "scene_roots": _scene_roots(ctx.memory),
            "player": f"0x{player.address:08x}" if player is not None else None,
            "physics_state": player.physics_state if player is not None else None,
        }

    def finish(self, ctx: Context) -> None:
        active = self._active
        self._active = None
        if active is None:
            return
        query = int(active["query"])
        if not ctx.memory.valid(query):
            return
        model = ctx.memory.u32(query + 0x68)
        face = ctx.memory.u32(query + 0x80)
        model_index = ctx.memory.u16(query + 0x84)
        record = {
            "type": "collision_query",
            "function": "Collision_QueryWrapper",
            "wrapper": f"0x{QUERY_WRAPPER:08x}",
            "return_pc": f"0x{ctx.eip:08x}",
            "caller": f"0x{int(active['caller']):08x}",
            "caller_function": active["caller_name"],
            "query": f"0x{query:08x}",
            "mode": active["mode"],
            "player": active["player"],
            "physics_state": active["physics_state"],
            "start_raw": active["start_raw"],
            "end_raw": active["end_raw"],
            "query_flags": active["query_flags"],
            "scene_roots": active["scene_roots"],
            "direction_flag": ctx.memory.u8(query + 0x89),
            "query_stamp": ctx.memory.u16(query + 0x8A),
            "line_basis_s16": _short_words(query + 0x48, 9, ctx.memory),
            "hit": bool(model),
            "model": f"0x{model:08x}" if model else None,
            "contact_raw": _vec_words(query + 0x6C, ctx.memory),
            "normal_s16": _short_vec(query + 0x78, ctx.memory),
            "face": f"0x{face:08x}" if face else None,
            "model_index": model_index,
            "face_flags": ctx.memory.u32(face + 0x0C) if face and ctx.memory.valid(face + 0x0C) else None,
            "face_words": _u32_words(face, 4, ctx.memory),
            "face_stride_bytes": (
                ctx.memory.u32(face) >> 16
                if face and ctx.memory.valid(face + 3)
                else None
            ),
            "face_vertex_bytes": [ctx.memory.u8(face + offset) for offset in (4, 5, 6, 7)]
            if face and ctx.memory.valid(face + 7)
            else None,
            "model_header_words": _u32_words(model, 8, ctx.memory),
            "model_index_word": ctx.memory.u16(model + 0x1A)
            if model and ctx.memory.valid(model + 0x1B)
            else None,
            "model_kind": ctx.memory.u8(model + 0x1F)
            if model and ctx.memory.valid(model + 0x1F)
            else None,
            "model_geometry": _collision_model_geometry(model, face, model_index, ctx.memory),
            "distance_raw": ctx.memory.u32(query + 0x8C),
            "hit_parameter": _signed32(ctx.memory.u32(query + 0x8C)),
            "distance_limit_raw": ctx.memory.u32(query + 0x40),
            "hit_distance": _signed32(ctx.memory.u32(query + 0x40)),
            "line_length": _signed32(ctx.memory.u32(query + 0x44)),
        }
        self._emit(record)
        self.hits += 1
        if self.remaining is not None:
            self.remaining -= 1
            if self.remaining <= 0:
                self._entry.enabled = False
                self._return.enabled = False


class _CollisionQueryEntry(TonyBreakpoint):
    def __init__(self, owner: CollisionQueryProbe):
        self.owner = owner
        super().__init__(QUERY_WRAPPER, internal=True)

    def on_hit(self, ctx: Context) -> None:
        self.owner.begin(ctx)


class _CollisionQueryReturn(TonyBreakpoint):
    def __init__(self, owner: CollisionQueryProbe):
        self.owner = owner
        super().__init__(QUERY_RETURN, internal=True)

    def on_hit(self, ctx: Context) -> None:
        self.owner.finish(ctx)
