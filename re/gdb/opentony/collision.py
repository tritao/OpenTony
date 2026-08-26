"""Bounded runtime observations for the shared level collision query."""

from __future__ import annotations

import gdb

from .breakpoint import Context, TonyBreakpoint
from .knowledge import function_name_at
from .player import PlayerView

QUERY_WRAPPER = 0x00466090
QUERY_RETURN = 0x0046609F
ACTION_MASK = 0x006A3F1C
COLLISION_MODEL_TABLE = 0x0056D43C
COLLISION_LINKED_ROOT = 0x0056AF40
COLLISION_LINKED_ROOT_AUX = 0x0056AF44
COLLISION_ZONE_TABLE = 0x00567F80
COLLISION_CANDIDATE_TABLE = 0x00567FA0
COLLISION_FACE_CACHE = 0x005643B0
COLLISION_MODEL_CACHE = 0x00567A70
ZONE_LOADER = 0x004667E0
ZONE_LOADER_AFTER_ARGS = 0x0046682E
ZONE_LOADER_FIRST_CELL = 0x0046697A
ZONE_LOADER_RETURN_SITES = (0x0043E041, 0x004B29EB)
COLLISION_FLAG_CONSUMER = 0x0048EA80
COLLISION_FLAG_RETURN = 0x0048EB02
COLLISION_DYNAMIC_QUERY = 0x00463E50
COLLISION_DYNAMIC_RETURN = 0x004641F2
COLLISION_DYNAMIC_TRANSFORM = 0x004F5540
# Direct return PCs for every observed call to the reusable matrix helper.
# The final two are the linked-object collision path; the earlier callers are
# other model/object transform users and are retained for reuse discovery.
COLLISION_DYNAMIC_TRANSFORM_RETURNS = (
    0x0045FCC1,
    0x0045FDA1,
    0x00460DD3,
    0x00460EB4,
    0x00461C64,
    0x00464067,
    0x0046409A,
)
COLLISION_DYNAMIC_CULL = 0x004F43E0
COLLISION_DYNAMIC_CULL_RETURN = 0x004F492C
COLLISION_FLAG_GLOBALS = {
    "surface_bit_40": 0x0056B768,
    "inverse_bit_24": 0x0056B7B8,
    "inverse_bit_23": 0x0056B7A8,
    "face_bit_80": 0x0056B7AC,
    "surface_class": 0x0056B7E8,
    "face_pointer": 0x0056B77C,
}


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


def _collision_flag_snapshot(memory) -> dict[str, int | None]:
    return {
        name: _global_u32(address, memory)
        for name, address in COLLISION_FLAG_GLOBALS.items()
    }


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
                "matrix_scale_q12": (
                    [_signed16(memory.u16(address + offset))
                     for offset in (0x28, 0x2A, 0x2C)]
                    if memory.readable(address + 0x2C, 2)
                    else None
                ),
                "previous": (
                    f"0x{memory.u32(address + 0x34):08x}"
                    if memory.readable(address + 0x34, 4)
                    and memory.u32(address + 0x34)
                    else None
                ),
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
    if not memory.valid(address + 0x1F):
        return None
    return {
        "address": f"0x{address:08x}",
        "present_word": memory.u32(address),
        "min_x": _signed32(memory.u32(address + 0x04)),
        "min_z": _signed32(memory.u32(address + 0x08)),
        "max_x": _signed32(memory.u32(address + 0x0C)),
        "max_z": _signed32(memory.u32(address + 0x10)),
        "cell_divisor": _signed32(memory.u32(address + 0x14)),
        "cell_count_x": _signed16(memory.u16(address + 0x1C)),
        "cell_count_z": _signed16(memory.u16(address + 0x1E)),
    }


def _scene_roots(memory) -> dict:
    """Capture addresses/ownership markers without walking unbounded memory."""

    linked_root = _global_u32(COLLISION_LINKED_ROOT, memory)
    linked_root_aux = _global_u32(COLLISION_LINKED_ROOT_AUX, memory)
    return {
        "linked_root_value": linked_root,
        "linked_objects": _linked_object_snapshots(linked_root, memory),
        "linked_root_aux_value": linked_root_aux,
        "linked_objects_aux": _linked_object_snapshots(linked_root_aux, memory),
        "zone_table_base": f"0x{COLLISION_ZONE_TABLE:08x}",
        "zone0": _zone_entry_snapshot(COLLISION_ZONE_TABLE, memory),
        "candidate_table_base": f"0x{COLLISION_CANDIDATE_TABLE:08x}",
        "candidate0": _global_u32(COLLISION_CANDIDATE_TABLE, memory),
        "model_table_base": f"0x{COLLISION_MODEL_TABLE:08x}",
        "model_table_kind0": _global_u32(COLLISION_MODEL_TABLE, memory),
        "face_cache_base": f"0x{COLLISION_FACE_CACHE:08x}",
        "model_cache_base": f"0x{COLLISION_MODEL_CACHE:08x}",
    }


def _safe_entry_args(ctx: Context, count: int = 3) -> list[int | None]:
    values: list[int | None] = []
    for index in range(count):
        try:
            values.append(ctx.arg(index))
        except (gdb.error, gdb.GdbError):  # GDB may reject a partial entry stack.
            values.append(None)
    return values


def _candidate_source_snapshot(address: int | None, memory, preview: int = 8) -> dict | None:
    """Read a bounded view of one loader source-cell block."""

    if not address or not memory.readable(address, 0x0C):
        return None
    count = memory.u32(address + 0x08)
    result = {
        "address": f"0x{address:08x}",
        "header_words": _u32_words(address, 3, memory),
        "entry_count": count,
        "entries": [],
        "entries_truncated": count > preview,
        "terminator": None,
    }
    readable_count = min(count, preview)
    if readable_count and not memory.readable(address + 0x0C, readable_count * 4):
        result["entries_unreadable"] = True
        return result
    result["entries"] = [memory.u32(address + 0x0C + index * 4)
                          for index in range(readable_count)]
    terminator_address = address + 0x0C + count * 4
    if count <= 0x1000 and memory.readable(terminator_address, 4):
        result["terminator"] = memory.u32(terminator_address)
    return result


def _loader_input_snapshot(ctx: Context) -> dict | None:
    """Capture the bounded serialized-zone prefix passed to 0x004667e0."""

    args = _safe_entry_args(ctx)
    if len(args) < 2 or not args[1]:
        return None
    address = args[1]
    return {
        "address": f"0x{address:08x}",
        "header_words": _u32_words(address, 5, ctx.memory),
        "first_cell_base": f"0x{address + 0x14:08x}",
        "first_cell_words": _u32_words(address + 0x14, 8, ctx.memory),
        "first_candidate_base": f"0x{address + 0x20:08x}",
        "first_candidate_words": _u32_words(address + 0x20, 8, ctx.memory),
    }


def _candidate_table_snapshot(memory, count: int = 8) -> dict:
    entries = []
    for index in range(count):
        address = COLLISION_CANDIDATE_TABLE + index * 4
        if not memory.readable(address, 4):
            break
        pointer = memory.u32(address)
        item = {"index": index, "pointer": f"0x{pointer:08x}" if pointer else None}
        if pointer:
            item["heads"] = _u32_words(pointer, 8, memory)
        entries.append(item)
    return {"address": f"0x{COLLISION_CANDIDATE_TABLE:08x}", "entries": entries}


def _zone_table_snapshot(memory, indexes=(0, 1, 6)) -> list[dict]:
    result = []
    for index in indexes:
        address = COLLISION_ZONE_TABLE + index * 0x660
        snapshot = _zone_entry_snapshot(address, memory)
        if snapshot is not None:
            snapshot["index"] = index
            result.append(snapshot)
    return result


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
            "frame": ctx.frame,
            "action_mask": ctx.memory.u16(ACTION_MASK),
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
            "frame": active["frame"],
            "action_mask": active["action_mask"],
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
            # q+0x68 is the object/model pointer consumed by the final normal
            # pass. Capture a short node-shaped chain directly so a hit can
            # be compared with the linked-root walk even when it lies beyond
            # the walk's bounded prefix.
            "hit_object": _linked_object_snapshots(model, ctx.memory, limit=16)
            if model
            else None,
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


class CollisionLoaderProbe:
    """Capture the bounded level-loader handoff into zone/candidate globals."""

    def __init__(self, count: int | None = None, writer=None):
        self.remaining = count
        self.writer = writer
        self._entry = _CollisionLoaderEntry(self)
        self._stages = [
            _CollisionLoaderStage(self, ZONE_LOADER_AFTER_ARGS, "after_args"),
            _CollisionLoaderStage(self, ZONE_LOADER_FIRST_CELL, "first_cell"),
        ]
        self._returns = [_CollisionLoaderReturn(self, address)
                         for address in ZONE_LOADER_RETURN_SITES]
        self._pending: list[dict] = []

    @property
    def breakpoints(self):
        return (self._entry, *self._stages, *self._returns)

    def _emit(self, record: dict) -> None:
        if self.writer is None:
            TonyBreakpoint.emit(record)
        else:
            self.writer.event(record)

    def begin(self, ctx: Context) -> None:
        self._pending.append(
            {
                "caller": ctx.caller(),
                "caller_function": function_name_at(ctx.caller()),
                "this": f"0x{ctx.this_ptr():08x}" if ctx.this_ptr() else None,
                "args": _safe_entry_args(ctx),
                "registers": {
                    name: ctx.register(name)
                    for name in ("eax", "ebx", "ecx", "edx", "esi", "edi", "ebp")
                },
                "entry_stack_words": _u32_words(ctx.esp, 16, ctx.memory),
                "loader_input": _loader_input_snapshot(ctx),
                "zone_before": _zone_entry_snapshot(COLLISION_ZONE_TABLE, ctx.memory),
                "zone_table_before": _zone_table_snapshot(ctx.memory),
                "candidate_before": _candidate_table_snapshot(ctx.memory),
            }
        )

    def stage(self, ctx: Context, name: str) -> None:
        if not self._pending:
            return
        stages = self._pending[-1].setdefault("stages", [])
        if name == "first_cell" and sum(stage["name"] == name for stage in stages) >= 3:
            return
        stages.append(
            {
                "name": name,
                "eip": ctx.register("eip"),
                "registers": {
                    register: ctx.register(register)
                    for register in ("eax", "ebx", "ecx", "edx", "esi", "edi", "ebp")
                },
                "stack_words": _u32_words(ctx.esp, 20, ctx.memory),
                "esi_words": _u32_words(ctx.register("esi"), 16, ctx.memory),
                "ebx_words": _u32_words(ctx.register("ebx"), 16, ctx.memory),
            }
        )

    def finish(self, ctx: Context, return_site: int) -> None:
        if not self._pending:
            return
        active = self._pending.pop(0)
        record = {
            "type": "collision_loader",
            "function": "Collision_ZoneLoader",
            "address": f"0x{ZONE_LOADER:08x}",
            "return_site": f"0x{return_site:08x}",
            **active,
            "zone_after": _zone_entry_snapshot(COLLISION_ZONE_TABLE, ctx.memory),
            "zone_table_after": _zone_table_snapshot(ctx.memory),
            "candidate_after": _candidate_table_snapshot(ctx.memory),
            "model_table_kind0": _global_u32(COLLISION_MODEL_TABLE, ctx.memory),
        }
        self._emit(record)
        if self.remaining is not None:
            self.remaining -= 1
            if self.remaining <= 0:
                for breakpoint in self.breakpoints:
                    breakpoint.enabled = False


class _CollisionLoaderEntry(TonyBreakpoint):
    def __init__(self, owner: CollisionLoaderProbe):
        self.owner = owner
        super().__init__(ZONE_LOADER, internal=True)

    def on_hit(self, ctx: Context) -> None:
        self.owner.begin(ctx)


class _CollisionLoaderStage(TonyBreakpoint):
    def __init__(self, owner: CollisionLoaderProbe, address: int, name: str):
        self.owner = owner
        self.name = name
        super().__init__(address, internal=True)

    def on_hit(self, ctx: Context) -> None:
        self.owner.stage(ctx, self.name)


class _CollisionLoaderReturn(TonyBreakpoint):
    def __init__(self, owner: CollisionLoaderProbe, address: int):
        self.owner = owner
        self.return_site = address
        super().__init__(address, internal=True)

    def on_hit(self, ctx: Context) -> None:
        self.owner.finish(ctx, self.return_site)


class CollisionFlagProbe:
    """Pair the face-flag consumer with its exact derived global outputs."""

    def __init__(self, count: int | None = None, writer=None):
        self.remaining = count
        self.writer = writer
        self._entry = _CollisionFlagEntry(self)
        self._return = _CollisionFlagReturn(self)
        self._entry.writer = writer
        self._return.writer = writer
        self._active: dict[str, object] | None = None

    @property
    def breakpoints(self):
        return (self._entry, self._return)

    def _emit(self, record: dict) -> None:
        if self.writer is None:
            TonyBreakpoint.emit(record)
        else:
            self.writer.event(record)

    def begin(self, ctx: Context) -> None:
        query = ctx.arg(0)
        if not query or not ctx.memory.valid(query + 0x80 + 3):
            return
        face = ctx.memory.u32(query + 0x80)
        self._active = {
            "query": query,
            "caller": ctx.caller(),
            "caller_function": function_name_at(ctx.caller()),
            "query_hit_body": ctx.memory.u32(query + 0x68)
            if ctx.memory.valid(query + 0x68 + 3)
            else None,
            "face_before": f"0x{face:08x}" if face else None,
            "flags_before": _collision_flag_snapshot(ctx.memory),
        }

    def finish(self, ctx: Context) -> None:
        active = self._active
        self._active = None
        if active is None:
            return
        query = int(active["query"])
        if not ctx.memory.valid(query + 0x80 + 3):
            return
        face = ctx.memory.u32(query + 0x80)
        face_words = _u32_words(face, 4, ctx.memory) if face else None
        record = {
            "type": "collision_flag_consumer",
            "function": "Collision_ConsumeHitFlags",
            "address": f"0x{COLLISION_FLAG_CONSUMER:08x}",
            "return_pc": f"0x{ctx.eip:08x}",
            "caller": f"0x{int(active['caller']):08x}",
            "caller_function": active["caller_function"],
            "query": f"0x{query:08x}",
            "query_hit_body": (
                f"0x{int(active['query_hit_body']):08x}"
                if active["query_hit_body"]
                else None
            ),
            "face": f"0x{face:08x}" if face else None,
            "face_words": face_words,
            "face_base_flags": face_words[0] & 0xffff if face_words else None,
            "face_surface_flags": (face_words[3] >> 16) & 0xffff
            if face_words
            else None,
            "flags_before": active["flags_before"],
            "flags_after": _collision_flag_snapshot(ctx.memory),
        }
        self._emit(record)
        if self.remaining is not None:
            self.remaining -= 1
            if self.remaining <= 0:
                self._entry.enabled = False
                self._return.enabled = False


class _CollisionFlagEntry(TonyBreakpoint):
    def __init__(self, owner: CollisionFlagProbe):
        self.owner = owner
        super().__init__(COLLISION_FLAG_CONSUMER, internal=True)

    def on_hit(self, ctx: Context) -> None:
        self.owner.begin(ctx)


class _CollisionFlagReturn(TonyBreakpoint):
    def __init__(self, owner: CollisionFlagProbe):
        self.owner = owner
        super().__init__(COLLISION_FLAG_RETURN, internal=True)

    def on_hit(self, ctx: Context) -> None:
        self.owner.finish(ctx)


def _dynamic_object_snapshot(address: int | None, memory) -> dict | None:
    if not address or not memory.readable(address, 0x24):
        return None
    return {
        "address": f"0x{address:08x}",
        "flags": memory.u16(address + 0x04),
        "query_stamp": memory.u16(address + 0x06),
        "position_raw": _vec_words(address + 0x08, memory),
        "position_s32": [
            _signed32(value) for value in _vec_words(address + 0x08, memory)
        ],
        "angles_s16": _short_vec(address + 0x14, memory),
        "model_index": memory.u16(address + 0x1A),
        "model_kind": memory.u8(address + 0x1F),
        "next": (
            f"0x{memory.u32(address + 0x20):08x}"
            if memory.u32(address + 0x20)
            else None
        ),
        "matrix_scale_q12": (
            [_signed16(memory.u16(address + offset))
             for offset in (0x28, 0x2A, 0x2C)]
            if memory.readable(address + 0x2C, 2)
            else None
        ),
    }


def _dynamic_query_snapshot(query: int | None, memory) -> dict | None:
    if not query or not memory.readable(query, 0x90):
        return None
    body = memory.u32(query + 0x68)
    face = memory.u32(query + 0x80)
    return {
        "address": f"0x{query:08x}",
        "start_raw": _vec_words(query, memory),
        "end_raw": _vec_words(query + 0x0C, memory),
        "hit_body": f"0x{body:08x}" if body else None,
        "contact_raw": _vec_words(query + 0x6C, memory),
        "normal_s16": _short_vec(query + 0x78, memory),
        "face": f"0x{face:08x}" if face else None,
        "model_index": memory.u16(query + 0x84),
        "query_flags": memory.u8(query + 0x88),
        "direction_flag": memory.u8(query + 0x89),
        "query_stamp": memory.u16(query + 0x8A),
        "hit_parameter": _signed32(memory.u32(query + 0x8C)),
        "hit_distance": _signed32(memory.u32(query + 0x40)),
        "line_length": _signed32(memory.u32(query + 0x44)),
    }


class CollisionDynamicProbe:
    """Capture the linked-object face tester and its shared query record."""

    def __init__(self, count: int | None = None, writer=None):
        self.remaining = count
        self.writer = writer
        self._entry = _CollisionDynamicEntry(self)
        self._return = _CollisionDynamicReturn(self)
        self._entry.writer = writer
        self._return.writer = writer
        self._active: dict[str, object] | None = None

    @property
    def breakpoints(self):
        return (self._entry, self._return)

    def _emit(self, record: dict) -> None:
        if self.writer is None:
            TonyBreakpoint.emit(record)
        else:
            self.writer.event(record)

    def begin(self, ctx: Context) -> None:
        object_address = ctx.arg(0)
        query = ctx.arg(1)
        if not object_address or not query:
            return
        self._active = {
            "object": object_address,
            "query": query,
            "caller": ctx.caller(),
            "caller_function": function_name_at(ctx.caller()),
            "object_before": _dynamic_object_snapshot(object_address, ctx.memory),
            "query_before": _dynamic_query_snapshot(query, ctx.memory),
        }

    def finish(self, ctx: Context) -> None:
        active = self._active
        self._active = None
        if active is None:
            return
        object_address = int(active["object"])
        query = int(active["query"])
        record = {
            "type": "collision_dynamic_object_query",
            "function": "Collision_TestLinkedObjectFaces",
            "address": f"0x{COLLISION_DYNAMIC_QUERY:08x}",
            "return_pc": f"0x{ctx.eip:08x}",
            "return_value": ctx.register("eax"),
            "caller": f"0x{int(active['caller']):08x}",
            "caller_function": active["caller_function"],
            "object": f"0x{object_address:08x}",
            "object_before": active["object_before"],
            "object_after": _dynamic_object_snapshot(object_address, ctx.memory),
            "query": f"0x{query:08x}",
            "query_before": active["query_before"],
            "query_after": _dynamic_query_snapshot(query, ctx.memory),
        }
        self._emit(record)
        if self.remaining is not None:
            self.remaining -= 1
            if self.remaining <= 0:
                self._entry.enabled = False
                self._return.enabled = False


class _CollisionDynamicEntry(TonyBreakpoint):
    def __init__(self, owner: CollisionDynamicProbe):
        self.owner = owner
        super().__init__(COLLISION_DYNAMIC_QUERY, internal=True)

    def on_hit(self, ctx: Context) -> None:
        self.owner.begin(ctx)


class _CollisionDynamicReturn(TonyBreakpoint):
    def __init__(self, owner: CollisionDynamicProbe):
        self.owner = owner
        super().__init__(COLLISION_DYNAMIC_RETURN, internal=True)

    def on_hit(self, ctx: Context) -> None:
        self.owner.finish(ctx)


def _matrix_snapshot(address: int | None, memory) -> list[int] | None:
    if not address or not memory.readable(address, 0x12):
        return None
    return _short_words(address, 9, memory)


def _dynamic_transform_snapshot(address: int | None, memory) -> dict | None:
    if not address or not memory.readable(address, 0x2E):
        return None
    return {
        "address": f"0x{address:08x}",
        "flags": memory.u16(address + 0x04),
        "tail_24_u32": memory.u32(address + 0x24),
        "tail_28_u32": memory.u32(address + 0x28),
        "tail_28_s16": _signed16(memory.u16(address + 0x28)),
        "tail_2a_s16": _signed16(memory.u16(address + 0x2A)),
        "tail_2c_s16": _signed16(memory.u16(address + 0x2C)),
        "tail_30_u32": memory.u32(address + 0x30),
    }


class CollisionDynamicTransformProbe:
    """Capture the opaque 0x0200 matrix-transform tail boundary."""

    def __init__(self, count: int | None = None, writer=None):
        self.remaining = count
        self.writer = writer
        self._entry = _CollisionDynamicTransformEntry(self)
        self._returns = [
            _CollisionDynamicTransformReturn(self, address)
            for address in COLLISION_DYNAMIC_TRANSFORM_RETURNS
        ]
        self._active: dict[str, object] | None = None

    @property
    def breakpoints(self):
        return (self._entry, *self._returns)

    def _emit(self, record: dict) -> None:
        if self.writer is None:
            TonyBreakpoint.emit(record)
        else:
            self.writer.event(record)

    def begin(self, ctx: Context) -> None:
        object_address = ctx.arg(0)
        matrix_address = ctx.arg(1)
        if not object_address or not matrix_address:
            return
        self._active = {
            "object": object_address,
            "matrix": matrix_address,
            "caller": ctx.caller(),
            "return_address": ctx.memory.u32(ctx.esp),
            "object_before": _dynamic_transform_snapshot(
                object_address, ctx.memory
            ),
            "matrix_before": _matrix_snapshot(matrix_address, ctx.memory),
        }

    def finish(self, ctx: Context) -> None:
        active = self._active
        self._active = None
        if active is None:
            return
        matrix_address = int(active["matrix"])
        record = {
            "type": "collision_dynamic_transform",
            "function": "Collision_TransformLinkedModelMatrix",
            "address": f"0x{COLLISION_DYNAMIC_TRANSFORM:08x}",
            "return_pc": f"0x{ctx.eip:08x}",
            "entry_return_address": f"0x{int(active['return_address']):08x}",
            "caller": f"0x{int(active['caller']):08x}",
            "object": f"0x{int(active['object']):08x}",
            "object_before": active["object_before"],
            "matrix": f"0x{matrix_address:08x}",
            "matrix_before": active["matrix_before"],
            "matrix_after": _matrix_snapshot(matrix_address, ctx.memory),
        }
        self._emit(record)
        if self.remaining is not None:
            self.remaining -= 1
            if self.remaining <= 0:
                for breakpoint in self.breakpoints:
                    breakpoint.enabled = False


class _CollisionDynamicTransformEntry(TonyBreakpoint):
    def __init__(self, owner: CollisionDynamicTransformProbe):
        self.owner = owner
        super().__init__(COLLISION_DYNAMIC_TRANSFORM, internal=True)

    def on_hit(self, ctx: Context) -> None:
        self.owner.begin(ctx)


class _CollisionDynamicTransformReturn(TonyBreakpoint):
    def __init__(self, owner: CollisionDynamicTransformProbe, address: int):
        self.owner = owner
        super().__init__(address, internal=True)

    def on_hit(self, ctx: Context) -> None:
        self.owner.finish(ctx)


class CollisionDynamicCullProbe:
    """Capture linked-list broad-phase survivors before face testing."""

    def __init__(self, count: int | None = None, writer=None):
        self.remaining = count
        self.writer = writer
        self._entry = _CollisionDynamicCullEntry(self)
        self._return = _CollisionDynamicCullReturn(self)
        self._entry.writer = writer
        self._return.writer = writer
        self._pending: list[dict] = []

    @property
    def breakpoints(self):
        return (self._entry, self._return)

    def _emit(self, record: dict) -> None:
        if self.writer is None:
            TonyBreakpoint.emit(record)
        else:
            self.writer.event(record)

    def begin(self, ctx: Context) -> None:
        root = ctx.arg(0)
        query = ctx.arg(2)
        stamp = ctx.arg(3)
        if not root or not query:
            return
        self._pending.append(
            {
                "root": root,
                "query": query,
                "stamp": stamp & 0xffff,
                "caller": ctx.caller(),
                "caller_function": function_name_at(ctx.caller()),
                "objects_before": _linked_object_snapshots(root, ctx.memory),
            }
        )

    def finish(self, ctx: Context) -> None:
        if not self._pending:
            return
        active = self._pending.pop(0)
        root = int(active["root"])
        stamp = int(active["stamp"])
        objects_after = _linked_object_snapshots(root, ctx.memory)
        survivors = [
            node["address"]
            for node in objects_after["nodes"]
            if node["query_stamp"] != stamp
        ]
        self._emit(
            {
                "type": "collision_dynamic_cull",
                "function": "Collision_CullLinkedObjects",
                "address": f"0x{COLLISION_DYNAMIC_CULL:08x}",
                "return_pc": f"0x{ctx.eip:08x}",
                "caller": f"0x{int(active['caller']):08x}",
                "caller_function": active["caller_function"],
                "root": f"0x{root:08x}",
                "query": f"0x{int(active['query']):08x}",
                "stamp": stamp,
                "objects_before": active["objects_before"],
                "objects_after": objects_after,
                "face_test_survivors": survivors,
                "return_value": ctx.register("eax"),
            }
        )
        if self.remaining is not None:
            self.remaining -= 1
            if self.remaining <= 0:
                self._entry.enabled = False
                self._return.enabled = False


class _CollisionDynamicCullEntry(TonyBreakpoint):
    def __init__(self, owner: CollisionDynamicCullProbe):
        self.owner = owner
        super().__init__(COLLISION_DYNAMIC_CULL, internal=True)

    def on_hit(self, ctx: Context) -> None:
        self.owner.begin(ctx)


class _CollisionDynamicCullReturn(TonyBreakpoint):
    def __init__(self, owner: CollisionDynamicCullProbe):
        self.owner = owner
        super().__init__(COLLISION_DYNAMIC_CULL_RETURN, internal=True)

    def on_hit(self, ctx: Context) -> None:
        self.owner.finish(ctx)
