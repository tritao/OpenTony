#pragma once

// Native collision scene boundary.
//
// The original PC executable receives a heap-built zone/cache graph.  The
// packaged PSX scene is the stable source asset available to the
// reconstruction, so this module decodes its model/object/blockmap records
// and feeds the evidence-backed query primitive.  Native handles are scene
// IDs or source offsets; they are intentionally not fake PC pointers.

#include "../../re/evidence/collision_reference.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace opentony::collision {

namespace reference = collision_reference;

using Raw = reference::Raw;
using RawVec3 = reference::RawVec3;
using QueryRecord = reference::QueryRecord;
using CollisionFaceAabbRecord = reference::CollisionFaceAabbRecord;
using CollisionFaceFilter = reference::CollisionFaceFilter;

struct PsxFace {
    std::uint16_t base_flags = 0;
    std::uint16_t length_bytes = 0;
    std::array<std::uint8_t, 4> vertex_indices{};
    std::uint16_t normal_index = 0;
    std::uint16_t surface_flags = 0;
    std::size_t source_offset = 0;
};

struct PsxModel {
    std::uint16_t flags = 0;
    std::uint16_t vertex_count = 0;
    std::uint16_t normal_count = 0;
    std::uint16_t face_count = 0;
    std::uint32_t radius = 0;
    std::array<std::int16_t, 6> bounds{};
    std::size_t source_offset = 0;
    std::vector<RawVec3> vertices;
    std::vector<std::array<std::int16_t, 3>> normals;
    std::vector<PsxFace> faces;
};

struct PsxObject {
    std::uint32_t flags = 0;
    RawVec3 position{};
    // The PSX loader copies disk +0x10 low/high halves and disk +0x14 into
    // runtime object +0x14/+0x16/+0x18.  Collision treats those words as
    // signed rotation components, although their original asset names are
    // not recovered yet.
    std::array<std::int16_t, 3> collision_angles{};
    std::uint16_t model_index = 0;
    std::size_t source_index = 0;
};

// Collision-facing view of the heap node walked by 0x004628f0.  This is
// intentionally separate from PsxObject: the PC node is a live linked-list
// element with a 0x24-byte collision prefix, while the PSX scene object table
// has a different on-disk record and no recovered angle fields.
struct PsxLinkedCollisionObject {
    std::uint32_t body_id = 0;
    // Stable source-side identity when this node came from the parsed PSX
    // object table. This is separate from the caller-owned span index used
    // by PsxCollisionResult::object_index and from the retail heap pointer
    // carried in q+0x68.
    std::size_t source_object_index = std::numeric_limits<std::size_t>::max();
    std::uint16_t flags = 0;
    RawVec3 position{};
    std::array<std::int16_t, 3> angles{};
    std::uint16_t model_index = 0;  // caller-resolved scene model index
    std::uint8_t model_kind = 0;    // original type-table selector, retained as metadata
    // +0x28/+0x2a/+0x2c in the PC 0x4c-byte element. These are signed Q12
    // column factors consumed by 0x004f5540 when flags has 0x0200.
    std::array<std::int16_t, 3> matrix_scale_q12 =
        reference::identity_q12_scale();
};

struct PsxBlockmapCell {
    std::uint32_t unknown_1 = 0;
    std::uint32_t unknown_2 = 0;
    std::vector<std::uint32_t> object_indices;
};

struct PsxBlockmap {
    Raw min_x = 0;
    Raw min_z = 0;
    Raw max_x = 0;
    Raw max_z = 0;
    std::uint16_t cell_count_x = 0;
    std::uint16_t cell_count_z = 0;
    std::vector<PsxBlockmapCell> cells;
};

struct PsxCollisionResult {
    QueryRecord query{};
    std::size_t object_index = std::numeric_limits<std::size_t>::max();
    // Set for a dynamic hit when the linked node carries parsed-PSX
    // provenance. Static hits continue to use object_index as their scene
    // object identity.
    std::size_t source_object_index = std::numeric_limits<std::size_t>::max();
    std::size_t face_index = std::numeric_limits<std::size_t>::max();
    std::uint16_t base_flags = 0;
    std::uint16_t surface_flags = 0;
    std::uint32_t surface_word = 0;
    // The dynamic face walker treats surface bit 0x20000 as a query-mask
    // sideband: with q+0x88 set it stores body+0x1a in DAT_00567a64 and does
    // not publish a physical q+0x68 hit. Preserve that observable result
    // without pretending the executable's global is part of QueryRecord.
    std::optional<std::uint16_t> query_mask_model_index;

    bool hit() const { return query.hit_body != 0; }

    // Keep the executable's raw face words authoritative while exposing the
    // currently evidence-backed bit view for callers that need to classify a
    // ground/wall result. The names in FaceFlagView remain provisional.
    reference::FaceFlagView decoded_flags() const {
        return reference::decode_face_flags(base_flags, surface_word);
    }
};

struct PsxDynamicModelVertices {
    std::vector<reference::DynamicVertexRecord> vertices;
    std::uint16_t clip_mask = 0x60f;
};

struct PsxDynamicObjectPreprocess {
    PsxDynamicModelVertices vertices;
    std::array<std::int16_t, 9> query_object_basis =
        reference::identity_q12_basis();
    std::array<std::int16_t, 9> final_object_basis =
        reference::identity_q12_basis();
};

class PsxScene {
public:
    // Parse a version-4 PSX scene.  The reader intentionally accepts only
    // fields needed by collision and leaves texture/tag payloads opaque.
    static std::optional<PsxScene> parse(std::span<const std::uint8_t> bytes,
                                          std::string* error = nullptr) {
        PsxScene scene;
        Reader reader(bytes, error);
        const auto version = reader.u16("version");
        const auto marker = reader.u16("marker");
        const auto tag_offset = reader.u32("tag offset");
        const auto object_count = reader.u32("object count");
        if (!version.has_value() || !marker.has_value() ||
            !tag_offset.has_value() || !object_count.has_value()) {
            return std::nullopt;
        }
        if (*version != 4 || *marker != 2) {
            reader.fail("only PSX version 4 / marker 2 is supported");
            return std::nullopt;
        }
        if (*tag_offset > bytes.size()) {
            reader.fail("tag offset is outside the scene");
            return std::nullopt;
        }
        if (*object_count > bytes.size() / 36u) {
            reader.fail("object table is unreasonably large");
            return std::nullopt;
        }

        scene.objects_.reserve(*object_count);
        for (std::uint32_t index = 0; index < *object_count; ++index) {
            PsxObject object;
            const auto flags = reader.u32("object flags");
            const auto x = reader.i32("object x");
            const auto y = reader.i32("object y");
            const auto z = reader.i32("object z");
            const auto unknown_1 = reader.u32("object unknown 1");
            const auto unknown_2 = reader.u16("object unknown 2");
            const auto model_index = reader.u16("object model index");
            const auto unknown_x = reader.i16("object unknown x");
            const auto unknown_y = reader.i16("object unknown y");
            const auto unknown_3 = reader.u32("object unknown 3");
            const auto unknown_rgbx = reader.u32("object unknown rgbx");
            if (!flags.has_value() || !x.has_value() || !y.has_value() ||
                !z.has_value() || !unknown_1.has_value() ||
                !unknown_2.has_value() || !model_index.has_value() ||
                !unknown_x.has_value() || !unknown_y.has_value() ||
                !unknown_3.has_value() || !unknown_rgbx.has_value()) {
                return std::nullopt;
            }
            object.flags = *flags;
            object.position = {*x, *y, *z};
            object.collision_angles = {
                static_cast<std::int16_t>(*unknown_1 & 0xffffu),
                static_cast<std::int16_t>((*unknown_1 >> 16u) & 0xffffu),
                static_cast<std::int16_t>(*unknown_2)};
            object.model_index = *model_index;
            object.source_index = index;
            scene.objects_.push_back(object);
        }

        const auto model_count = reader.u32("model count");
        if (!model_count.has_value() || *model_count > bytes.size() / 4u) {
            reader.fail("model table is unreasonably large");
            return std::nullopt;
        }
        std::vector<std::uint32_t> model_offsets;
        model_offsets.reserve(*model_count);
        for (std::uint32_t index = 0; index < *model_count; ++index) {
            const auto offset = reader.u32("model offset");
            if (!offset.has_value()) {
                return std::nullopt;
            }
            model_offsets.push_back(*offset);
        }
        if (!std::is_sorted(model_offsets.begin(), model_offsets.end()) ||
            std::adjacent_find(model_offsets.begin(), model_offsets.end()) !=
                model_offsets.end()) {
            reader.fail("model offsets are not strictly increasing");
            return std::nullopt;
        }
        for (const auto& object : scene.objects_) {
            if (object.model_index >= *model_count) {
                reader.fail("object references a missing model");
                return std::nullopt;
            }
        }

        scene.models_.reserve(*model_count);
        for (std::size_t index = 0; index < model_offsets.size(); ++index) {
            const auto model_start = static_cast<std::size_t>(model_offsets[index]);
            const auto model_end = index + 1 < model_offsets.size()
                                       ? static_cast<std::size_t>(model_offsets[index + 1])
                                       : static_cast<std::size_t>(*tag_offset);
            if (model_start < reader.position() || model_end <= model_start ||
                model_end > bytes.size()) {
                reader.fail("model boundary is invalid");
                return std::nullopt;
            }
            Reader model_reader(bytes.subspan(model_start, model_end - model_start), error);
            PsxModel model;
            model.source_offset = model_start;
            const auto flags = model_reader.u16("model flags");
            const auto vertex_count = model_reader.u16("vertex count");
            const auto normal_count = model_reader.u16("normal count");
            const auto face_count = model_reader.u16("face count");
            const auto radius = model_reader.u32("model radius");
            if (!flags.has_value() || !vertex_count.has_value() ||
                !normal_count.has_value() || !face_count.has_value() ||
                !radius.has_value()) {
                return std::nullopt;
            }
            model.flags = *flags;
            model.vertex_count = *vertex_count;
            model.normal_count = *normal_count;
            model.face_count = *face_count;
            model.radius = *radius;
            for (auto& bound : model.bounds) {
                const auto value = model_reader.i16("model bound");
                if (!value.has_value()) {
                    return std::nullopt;
                }
                bound = *value;
            }
            if (!model_reader.u32("model unknown header").has_value()) {
                return std::nullopt;
            }

            model.vertices.reserve(model.vertex_count);
            for (std::uint16_t vertex_index = 0; vertex_index < model.vertex_count;
                 ++vertex_index) {
                const auto x = model_reader.i16("vertex x");
                const auto y = model_reader.i16("vertex y");
                const auto z = model_reader.i16("vertex z");
                if (!x.has_value() || !y.has_value() || !z.has_value() ||
                    !model_reader.skip(2, "vertex padding")) {
                    return std::nullopt;
                }
                model.vertices.push_back({*x, *y, *z});
            }
            model.normals.reserve(model.normal_count);
            for (std::uint16_t normal_index = 0; normal_index < model.normal_count;
                 ++normal_index) {
                const auto x = model_reader.i16("normal x");
                const auto y = model_reader.i16("normal y");
                const auto z = model_reader.i16("normal z");
                if (!x.has_value() || !y.has_value() || !z.has_value() ||
                    !model_reader.skip(2, "normal padding")) {
                    return std::nullopt;
                }
                model.normals.push_back({*x, *y, *z});
            }

            model.faces.reserve(model.face_count);
            for (std::uint16_t face_index = 0; face_index < model.face_count;
                 ++face_index) {
                const auto face_start = model_reader.position();
                const auto base_flags = model_reader.u16("face flags");
                const auto length_bytes = model_reader.u16("face length");
                if (!base_flags.has_value() || !length_bytes.has_value() ||
                    *length_bytes < 0x10u ||
                    face_start + *length_bytes > model_reader.size()) {
                    model_reader.fail("face record is invalid");
                    return std::nullopt;
                }
                PsxFace face;
                face.base_flags = *base_flags;
                face.length_bytes = *length_bytes;
                face.source_offset = model_start + face_start;
                for (auto& vertex_index : face.vertex_indices) {
                    const auto value = model_reader.u8("face vertex index");
                    if (!value.has_value() || *value >= model.vertex_count) {
                        model_reader.fail("face references a missing vertex");
                        return std::nullopt;
                    }
                    vertex_index = *value;
                }
                if (!model_reader.skip(4, "face render payload")) {
                    return std::nullopt;
                }
                const auto normal_index = model_reader.u16("face normal index");
                const auto surface_flags = model_reader.u16("face surface flags");
                if (!normal_index.has_value() || !surface_flags.has_value() ||
                    *normal_index >= model.normal_count) {
                    model_reader.fail("face references a missing normal");
                    return std::nullopt;
                }
                face.normal_index = *normal_index;
                face.surface_flags = *surface_flags;
                if (!model_reader.seek(face_start + *length_bytes, "face end")) {
                    return std::nullopt;
                }
                model.faces.push_back(face);
            }
            scene.models_.push_back(std::move(model));
        }

        if (!scene.read_tags(reader, *tag_offset)) {
            return std::nullopt;
        }
        return scene;
    }

    static std::optional<PsxScene> parse(std::span<const std::byte> bytes,
                                          std::string* error = nullptr) {
        return parse(
            std::span<const std::uint8_t>(
                reinterpret_cast<const std::uint8_t*>(bytes.data()),
                bytes.size()),
            error);
    }

    const std::vector<PsxObject>& objects() const { return objects_; }
    const std::vector<PsxModel>& models() const { return models_; }
    const std::vector<PsxBlockmap>& blockmaps() const { return blockmaps_; }

    // Construct the collision-facing portion of a linked runtime object from
    // one parsed PSX object.  The finalizer at 0x004647c0 supplies the model
    // region slot and augments flags from model metadata; the attachment path
    // supplies the live body identity.  Keeping both explicit prevents this
    // source-level constructor from inventing those unresolved values.
    std::optional<PsxLinkedCollisionObject>
    linked_collision_object_from_source(std::size_t object_index,
                                        std::uint8_t model_region_slot,
                                        std::uint32_t body_id,
                                        std::uint16_t finalized_flags) const {
        if (object_index >= objects_.size()) {
            return std::nullopt;
        }
        const auto& source = objects_[object_index];
        PsxLinkedCollisionObject result;
        result.body_id = body_id;
        result.source_object_index = object_index;
        result.flags = finalized_flags;
        result.position = source.position;
        result.angles = source.collision_angles;
        result.model_index = source.model_index;
        result.model_kind = model_region_slot;
        return result;
    }

    // Query using the recovered static level path.  QueryRecord fields are
    // prepared here so this is the native equivalent of InitLineInfo plus
    // the scene wrapper; hit_body is object_index+1 and hit_face_record is
    // the source offset of the winning PSX face.
    QueryRecord query(RawVec3 start, RawVec3 end,
                      std::uint16_t query_stamp = 0,
                      CollisionFaceFilter filter = {}) const {
        QueryRecord result;
        result.start = start;
        result.end = end;
        reference::prepare(result, query_stamp);
        result.query_mask_mode = filter.query_mask_mode ? 1 : 0;
        query(result, filter);
        return result;
    }

    PsxCollisionResult query_with_metadata(
        RawVec3 start, RawVec3 end, std::uint16_t query_stamp = 0,
        CollisionFaceFilter filter = {}) const {
        PsxCollisionResult result;
        result.query.start = start;
        result.query.end = end;
        reference::prepare(result.query, query_stamp);
        result.query.query_mask_mode = filter.query_mask_mode ? 1 : 0;
        query(result.query, filter, &result);
        return result;
    }

    // Native equivalent of the cdecl wrapper at 0x00466090. The caller owns
    // the already-prepared query record; scene traversal publishes its result
    // into that record and the wrapper's return value is always zero. A
    // nonzero mode admits the caller-supplied linked-object pass before the
    // static blockmap pass, matching 0x004660b0's mode gate. Empty linked
    // input naturally leaves this as the static query path.
    int execute_query_wrapper(
        QueryRecord& query_record, int mode,
        std::span<const PsxLinkedCollisionObject> linked_objects = {},
        CollisionFaceFilter filter = {}) const {
        PsxCollisionResult linked_result;
        if (mode != 0 && !linked_objects.empty()) {
            linked_result = query_linked_objects(
                query_record.start, query_record.end, linked_objects,
                query_record.query_stamp, filter);
        }

        PsxCollisionResult static_result;
        static_result.query = query_record;
        static_result.query.query_mask_mode = filter.query_mask_mode ? 1 : 0;
        query(static_result.query, filter, &static_result);

        if (mode != 0 && !linked_objects.empty() && linked_result.hit() &&
            (!static_result.hit() ||
             linked_result.query.hit_distance <
                 static_result.query.hit_distance)) {
            query_record = linked_result.query;
        } else {
            query_record = static_result.query;
        }
        return 0;
    }

    // Native boundary for the recovered 0x004f4b00 dynamic preprocessing
    // pass.  The caller supplies the already-composed query/object Q12
    // basis.  For an unrotated object, model_origin_units is the object
    // origin minus query start after the original >>12 conversion.
    static PsxDynamicModelVertices transform_dynamic_model(
        const PsxModel& model, const RawVec3& model_origin_units,
        const std::array<std::int16_t, 9>& query_basis, Raw line_length,
        const RawVec3& transformed_translation = {}) {
        PsxDynamicModelVertices result;
        result.vertices.resize(model.vertices.size());
        for (std::size_t index = 0; index < model.vertices.size(); ++index) {
            const auto& vertex = model.vertices[index];
            const Raw x = reference::wrapping_add(vertex[0], model_origin_units[0]);
            const Raw y = reference::wrapping_add(vertex[1], model_origin_units[1]);
            const Raw z = reference::wrapping_add(vertex[2], model_origin_units[2]);
            const Raw transformed_x = reference::q12_transform_component_raw(
                query_basis, 0, x, y, z);
            const Raw transformed_y = reference::q12_transform_component_raw(
                query_basis, 1, x, y, z);
            const Raw transformed_z = reference::q12_transform_component_raw(
                query_basis, 2, x, y, z);
            auto& output = result.vertices[index];
            output.x = reference::clamp_to_s16(reference::wrapping_add(
                transformed_x, transformed_translation[0]));
            output.y = reference::clamp_to_s16(reference::wrapping_add(
                transformed_y, transformed_translation[1]));
            output.z = reference::clamp_to_s16(reference::wrapping_add(
                transformed_z, transformed_translation[2]));
            output.clip_mask = reference::dynamic_clip_mask(
                output.x, output.y, output.z, line_length);
            result.clip_mask = static_cast<std::uint16_t>(
                result.clip_mask & output.clip_mask);
        }
        return result;
    }

    // Compose the recovered fast/oriented branch selection from 0x00463e50.
    // The query must already have been passed through reference::prepare so
    // its line basis, line length, and vertical direction flag are available.
    // The full PC 0x0200 path scales this temporary matrix from the signed
    // Q12 heap-tail words at +0x28/+0x2a/+0x2c, then left-composes it with
    // the query basis. The native caller can supply those factors directly;
    // the PC loader's values remain unresolved.
    static PsxDynamicObjectPreprocess preprocess_dynamic_object(
        const PsxModel& model, const QueryRecord& prepared_query,
        const RawVec3& object_position_raw,
        const std::array<std::int16_t, 3>& object_angles,
        bool force_oriented_path = false,
        const std::array<std::int16_t, 3>& matrix_scale_q12 =
            reference::identity_q12_scale()) {
        const auto transform = reference::build_dynamic_object_transform(
            prepared_query, object_position_raw, object_angles,
            force_oriented_path, matrix_scale_q12);
        PsxDynamicObjectPreprocess result;
        result.query_object_basis = transform.normal_basis;
        result.final_object_basis = transform.final_basis;
        result.vertices = transform_dynamic_model(
            model, transform.model_origin_units, transform.vertex_basis,
            prepared_query.line_length, transform.transformed_translation);
        return result;
    }

    // Run the recovered 0x004f4c50 face pass over a caller-owned transformed
    // vertex stream. query_object_basis is the basis used by 0x004e24b0 for
    // the candidate normal: query_basis * object_rotation * scale on the
    // oriented linked path, or the line basis on the fast unrotated path.
    // The final basis is the unscaled object rotation consumed by
    // 0x00463d50.
    PsxCollisionResult query_dynamic_object(
        RawVec3 start, RawVec3 end, std::size_t object_index,
        const PsxDynamicModelVertices& transformed_vertices,
        const std::array<std::int16_t, 9>& query_object_basis,
        const std::array<std::int16_t, 9>& final_object_basis,
        std::uint16_t query_stamp = 0,
        CollisionFaceFilter filter = {}) const {
        PsxCollisionResult result;
        result.query.start = start;
        result.query.end = end;
        reference::prepare(result.query, query_stamp);
        if (object_index >= objects_.size() ||
            (transformed_vertices.clip_mask & 0x60fu) != 0) {
            return result;
        }
        const auto& object = objects_[object_index];
        if (object.model_index >= models_.size()) {
            return result;
        }
        return query_dynamic_model(
            models_[object.model_index], start, end, object_index,
            static_cast<std::uint32_t>(object_index + 1), object.model_index,
            transformed_vertices, query_object_basis, final_object_basis,
            query_stamp, filter);
    }

    PsxCollisionResult query_dynamic_object(
        RawVec3 start, RawVec3 end, std::size_t object_index,
        const PsxDynamicObjectPreprocess& preprocess,
        std::uint16_t query_stamp = 0,
        CollisionFaceFilter filter = {}) const {
        return query_dynamic_object(
            start, end, object_index, preprocess.vertices,
            preprocess.query_object_basis, preprocess.final_object_basis,
            query_stamp, filter);
    }

    // Run the recovered linked-object branch over caller-owned node data.
    // The caller supplies a native view of the live PC list; no PC pointer is
    // fabricated. Objects are first admitted by 0x004f43e0's flag gate and
    // object-space broad phase, then the exact transformed-vertex/face pass
    // is run. The returned query uses q+0x40 distance ordering, matching the
    // dynamic routine rather than the static q+0x8c parameter ordering.
    PsxCollisionResult query_linked_objects(
        RawVec3 start, RawVec3 end,
        std::span<const PsxLinkedCollisionObject> linked_objects,
        std::uint16_t query_stamp = 0,
        CollisionFaceFilter filter = {}) const {
        PsxCollisionResult best;
        best.query.start = start;
        best.query.end = end;
        reference::prepare(best.query, query_stamp);
        best.query.query_mask_mode = filter.query_mask_mode ? 1 : 0;

        const RawVec3 relative_start{0, 0, 0};
        RawVec3 relative_end{};
        for (std::size_t axis = 0; axis < relative_end.size(); ++axis) {
            relative_end[axis] = reference::arithmetic_shift_right_12(
                reference::wrapping_sub(end[axis], start[axis]));
        }

        RawVec3 ordered_start = relative_start;
        RawVec3 ordered_end = relative_end;
        std::uint8_t reflection_mask = 0;
        for (std::size_t axis = 0; axis < ordered_start.size(); ++axis) {
            if (ordered_start[axis] > ordered_end[axis]) {
                std::swap(ordered_start[axis], ordered_end[axis]);
                reflection_mask = static_cast<std::uint8_t>(
                    reflection_mask | (1u << axis));
            }
        }

        std::optional<std::uint16_t> query_mask_model_index;
        for (std::size_t object_index = 0; object_index < linked_objects.size();
             ++object_index) {
            const auto& object = linked_objects[object_index];
            if (!reference::linked_object_flag_gate(object.flags) ||
                object.model_index >= models_.size()) {
                continue;
            }
            const auto& model = models_[object.model_index];
            const auto model_header = collision_model_header(model);
            const auto object_offset = relative_position(
                object.position, start);
            const auto matrix_scale =
                reference::linked_object_uses_matrix_transform(object.flags)
                    ? object.matrix_scale_q12
                    : reference::identity_q12_scale();
            const auto bounds = reference::build_object_bounds(
                model_header, ordered_start, ordered_end, object_offset,
                reflection_mask, matrix_scale);
            if (!reference::object_broadphase_test(
                    ordered_start, ordered_end, bounds)) {
                continue;
            }

            const auto preprocess = preprocess_dynamic_object(
                model, best.query, object.position, object.angles,
                reference::linked_object_uses_matrix_transform(object.flags),
                matrix_scale);
            const auto body_id = object.body_id != 0
                                     ? object.body_id
                                     : static_cast<std::uint32_t>(object_index + 1);
            auto candidate = query_dynamic_model(
                model, start, end, object_index, body_id, object.model_index,
                preprocess.vertices, preprocess.query_object_basis,
                preprocess.final_object_basis, query_stamp, filter);
            candidate.source_object_index = object.source_object_index;
            if (candidate.query_mask_model_index) {
                query_mask_model_index = candidate.query_mask_model_index;
            }
            if (candidate.hit() &&
                candidate.query.hit_distance < best.query.hit_distance) {
                best = candidate;
            }
        }
        best.query.query_mask_mode = filter.query_mask_mode ? 1 : 0;
        best.query_mask_model_index = query_mask_model_index;
        return best;
    }

    // Compose the two branches reached by 0x004660b0. The executable runs
    // linked objects and zone candidates through different nearest fields,
    // but both paths leave the comparable traveled distance at q+0x40. A
    // strict comparison preserves the static result on an exact tie, which
    // matches the dynamic-before-static traversal order plus the original
    // strict-nearer updates.
    PsxCollisionResult query_with_linked_objects(
        RawVec3 start, RawVec3 end,
        std::span<const PsxLinkedCollisionObject> linked_objects,
        std::uint16_t query_stamp = 0,
        CollisionFaceFilter filter = {}) const {
        const auto static_result = query_with_metadata(
            start, end, query_stamp, filter);
        const auto linked_result = query_linked_objects(
            start, end, linked_objects, query_stamp, filter);
        if (!linked_result.hit() ||
            (static_result.hit() &&
             static_result.query.hit_distance <=
                 linked_result.query.hit_distance)) {
            auto result = static_result;
            result.query_mask_model_index =
                linked_result.query_mask_model_index;
            return result;
        }
        return linked_result;
    }

    bool query(QueryRecord& query_record,
               CollisionFaceFilter filter = {}) const {
        return query(query_record, filter, nullptr);
    }

private:
    static reference::CollisionModelHeader collision_model_header(
        const PsxModel& model) {
        return reference::CollisionModelHeader{
            .unknown_flags = model.flags,
            .vertex_count = model.vertex_count,
            .normal_count = model.normal_count,
            .face_count = model.face_count,
            .radius = model.radius,
            .x_max = model.bounds[0],
            .x_min = model.bounds[1],
            .y_max = model.bounds[2],
            .y_min = model.bounds[3],
            .z_max = model.bounds[4],
            .z_min = model.bounds[5],
            .unknown_value = 0,
        };
    }

    static RawVec3 relative_position(const RawVec3& position,
                                     const RawVec3& query_start) {
        RawVec3 result{};
        for (std::size_t axis = 0; axis < result.size(); ++axis) {
            result[axis] = reference::arithmetic_shift_right_12(
                reference::wrapping_sub(position[axis], query_start[axis]));
        }
        return result;
    }

    static PsxCollisionResult query_dynamic_model(
        const PsxModel& model, RawVec3 start, RawVec3 end,
        std::size_t object_index, std::uint32_t body_id,
        std::uint16_t model_index,
        const PsxDynamicModelVertices& transformed_vertices,
        const std::array<std::int16_t, 9>& query_object_basis,
        const std::array<std::int16_t, 9>& final_object_basis,
        std::uint16_t query_stamp, CollisionFaceFilter filter) {
        PsxCollisionResult result;
        result.query.start = start;
        result.query.end = end;
        reference::prepare(result.query, query_stamp);
        result.query.query_mask_mode = filter.query_mask_mode ? 1 : 0;
        if ((transformed_vertices.clip_mask & 0x60fu) != 0 ||
            transformed_vertices.vertices.size() < model.vertices.size()) {
            return result;
        }
        for (std::size_t face_index = 0; face_index < model.faces.size();
             ++face_index) {
            const auto& face = model.faces[face_index];
            const auto surface_word =
                (static_cast<std::uint32_t>(face.surface_flags) << 16u) |
                (static_cast<std::uint32_t>(face.normal_index) << 3u);
            if (!filter.accepts(surface_word) ||
                face.normal_index >= model.normals.size()) {
                continue;
            }
            const auto& indices = face.vertex_indices;
            if (indices[0] >= transformed_vertices.vertices.size() ||
                indices[1] >= transformed_vertices.vertices.size() ||
                indices[2] >= transformed_vertices.vertices.size() ||
                indices[3] >= transformed_vertices.vertices.size()) {
                continue;
            }
            const auto& vertex0 = transformed_vertices.vertices[indices[0]];
            const auto& vertex1 = transformed_vertices.vertices[indices[1]];
            const auto& vertex2 = transformed_vertices.vertices[indices[2]];
            const auto& vertex3 = transformed_vertices.vertices[indices[3]];
            if (!reference::dynamic_face_clip_accepts(
                    vertex0, vertex1, vertex2, vertex3)) {
                continue;
            }
            const auto candidate = reference::dynamic_face_candidate(
                vertex0, vertex1, vertex2, vertex3,
                (face.base_flags & 0x10u) != 0,
                model.normals[face.normal_index], query_object_basis,
                result.query.line_length);
            if (!candidate) {
                continue;
            }
            if ((surface_word & 0x20000u) != 0) {
                // 0x004f4c50's special surface branch is not a collision hit.
                // It only records the model selector when q+0x88 is true;
                // CollisionFaceFilter has already rejected this face in the
                // ordinary mode.
                if (filter.query_mask_mode) {
                    result.query_mask_model_index = model_index;
                }
                continue;
            }
            if (!reference::record_nearest_dynamic_candidate(
                                   result.query, *candidate, body_id,
                                   static_cast<std::uint32_t>(face.source_offset),
                                   model_index)) {
                continue;
            }
            result.query.hit_normal = reference::transform_normal_q12(
                final_object_basis, model.normals[face.normal_index]);
            result.object_index = object_index;
            result.face_index = face_index;
            result.base_flags = face.base_flags;
            result.surface_flags = face.surface_flags;
            result.surface_word = surface_word;
        }
        if (result.hit()) {
            const auto contact = reference::dynamic_contact_at_distance(result.query);
            if (contact) {
                result.query.hit_position = *contact;
            }
        }
        return result;
    }

    bool query(QueryRecord& query_record, CollisionFaceFilter filter,
               PsxCollisionResult* metadata) const {
        const auto candidates = candidate_objects(query_record);
        for (const auto object_index : candidates) {
            if (object_index >= objects_.size()) {
                continue;
            }
            const auto& object = objects_[object_index];
            if (object.model_index >= models_.size()) {
                continue;
            }
            const auto& model = models_[object.model_index];
            for (std::size_t face_index = 0; face_index < model.faces.size(); ++face_index) {
                const auto& face = model.faces[face_index];
                const auto surface_word =
                    (static_cast<std::uint32_t>(face.surface_flags) << 16u) |
                    (static_cast<std::uint32_t>(face.normal_index) << 3u);
                if (!filter.accepts(surface_word)) {
                    continue;
                }
                const auto geometry = face_geometry(model, face);
                if (!geometry) {
                    continue;
                }
                if (!reference::face_aabb_overlaps_query(
                        face_aabb(model, face, object.position), query_record)) {
                    continue;
                }
                const auto old_parameter = query_record.hit_parameter;
                if (reference::record_nearest_face_candidate(
                        query_record, *geometry, object.position,
                        static_cast<std::uint32_t>(object_index + 1),
                        static_cast<std::uint32_t>(face.source_offset),
                        object.model_index) &&
                    query_record.hit_parameter != old_parameter) {
                    query_record.hit_normal = model.normals[face.normal_index];
                    if (metadata != nullptr) {
                        metadata->object_index = object_index;
                        metadata->face_index = face_index;
                        metadata->base_flags = face.base_flags;
                        metadata->surface_flags = face.surface_flags;
                        metadata->surface_word = surface_word;
                    }
                }
            }
        }
        return query_record.hit_body != 0;
    }

    class Reader {
    public:
        Reader(std::span<const std::uint8_t> bytes, std::string* error)
            : bytes_(bytes), error_(error) {}

        std::size_t position() const { return position_; }
        std::size_t size() const { return bytes_.size(); }
        std::span<const std::uint8_t> bytes() const { return bytes_; }
        std::string* error() const { return error_; }

        std::optional<std::uint8_t> u8(const char* label) {
            if (!require(1, label)) {
                return std::nullopt;
            }
            return bytes_[position_++];
        }

        std::optional<std::uint16_t> u16(const char* label) {
            if (!require(2, label)) {
                return std::nullopt;
            }
            const auto value = static_cast<std::uint16_t>(bytes_[position_]) |
                               (static_cast<std::uint16_t>(bytes_[position_ + 1]) << 8u);
            position_ += 2;
            return value;
        }

        std::optional<std::uint32_t> u32(const char* label) {
            if (!require(4, label)) {
                return std::nullopt;
            }
            const auto value = static_cast<std::uint32_t>(bytes_[position_]) |
                               (static_cast<std::uint32_t>(bytes_[position_ + 1]) << 8u) |
                               (static_cast<std::uint32_t>(bytes_[position_ + 2]) << 16u) |
                               (static_cast<std::uint32_t>(bytes_[position_ + 3]) << 24u);
            position_ += 4;
            return value;
        }

        std::optional<std::int16_t> i16(const char* label) {
            const auto value = u16(label);
            return value ? std::optional<std::int16_t>(static_cast<std::int16_t>(*value))
                         : std::nullopt;
        }

        std::optional<std::int32_t> i32(const char* label) {
            const auto value = u32(label);
            return value ? std::optional<std::int32_t>(static_cast<std::int32_t>(*value))
                         : std::nullopt;
        }

        bool skip(std::size_t count, const char* label) {
            return require(count, label) ? ((position_ += count), true) : false;
        }

        bool seek(std::size_t position, const char* label) {
            if (position > bytes_.size()) {
                fail(label);
                return false;
            }
            position_ = position;
            return true;
        }

        void fail(const char* message) {
            if (error_ && error_->empty()) {
                *error_ = std::string(message) + " at 0x" +
                          to_hex(position_);
            }
        }

    private:
        bool require(std::size_t count, const char* label) {
            if (count <= bytes_.size() - std::min(position_, bytes_.size())) {
                return true;
            }
            fail(label);
            return false;
        }

        static std::string to_hex(std::size_t value) {
            constexpr char digits[] = "0123456789abcdef";
            std::string result(2 * sizeof(std::size_t), '0');
            for (std::size_t index = result.size(); index != 0; --index) {
                result[index - 1] = digits[value & 0xfu];
                value >>= 4u;
            }
            return result;
        }

        std::span<const std::uint8_t> bytes_;
        std::string* error_ = nullptr;
        std::size_t position_ = 0;
    };

    static std::optional<reference::FaceGeometry> face_geometry(
        const PsxModel& model, const PsxFace& face) {
        const auto vertex = [&model](std::uint8_t index) -> std::optional<RawVec3> {
            if (index >= model.vertices.size()) {
                return std::nullopt;
            }
            return model.vertices[index];
        };
        const auto v0 = vertex(face.vertex_indices[0]);
        const auto v1 = vertex(face.vertex_indices[1]);
        const auto v2 = vertex(face.vertex_indices[2]);
        const auto v3 = vertex(face.vertex_indices[3]);
        if (!v0 || !v1 || !v2 || !v3 || face.normal_index >= model.normals.size()) {
            return std::nullopt;
        }
        return reference::FaceGeometry{
            .vertex0 = *v0,
            .vertex1 = *v1,
            .vertex2 = *v2,
            .vertex3 = *v3,
            .plane_normal = model.normals[face.normal_index],
            .is_triangle = (face.base_flags & 0x10u) != 0,
        };
    }

    static CollisionFaceAabbRecord face_aabb(const PsxModel& model,
                                             const PsxFace& face,
                                             const RawVec3& origin) {
        const auto geometry = face_geometry(model, face);
        const auto world = [&origin](const RawVec3& vertex) {
            return RawVec3{
                reference::wrapping_add(origin[0], reference::x86_shift_left(vertex[0], 12)),
                reference::wrapping_add(origin[1], reference::x86_shift_left(vertex[1], 12)),
                reference::wrapping_add(origin[2], reference::x86_shift_left(vertex[2], 12)),
            };
        };
        const auto first = world(geometry->vertex0);
        RawVec3 min_corner = first;
        RawVec3 max_corner = first;
        const std::array<RawVec3, 3> remaining{
            world(geometry->vertex1), world(geometry->vertex2), world(geometry->vertex3)};
        const auto count = geometry->is_triangle ? 2u : 3u;
        for (std::size_t index = 0; index < count; ++index) {
            for (std::size_t axis = 0; axis < 3; ++axis) {
                min_corner[axis] = std::min(min_corner[axis], remaining[index][axis]);
                max_corner[axis] = std::max(max_corner[axis], remaining[index][axis]);
            }
        }
        return CollisionFaceAabbRecord{
            .base_flags = face.base_flags,
            .min_x = min_corner[0],
            .min_y = min_corner[1],
            .min_z = min_corner[2],
            .max_x = max_corner[0],
            .max_y = max_corner[1],
            .max_z = max_corner[2],
        };
    }

    std::vector<std::size_t> candidate_objects(const QueryRecord& query_record) const {
        std::vector<std::size_t> result;
        std::vector<bool> seen(objects_.size(), false);
        const auto add = [&result, &seen, this](std::uint32_t object_index) {
            if (object_index < seen.size() && !seen[object_index]) {
                seen[object_index] = true;
                result.push_back(object_index);
            }
        };
        if (blockmaps_.empty()) {
            for (std::size_t index = 0; index < objects_.size(); ++index) {
                add(static_cast<std::uint32_t>(index));
            }
            return result;
        }
        for (const auto& blockmap : blockmaps_) {
            if (!((blockmap.min_x <= query_record.start[0] ||
                   blockmap.min_x <= query_record.end[0]) &&
                  (query_record.start[0] <= blockmap.max_x ||
                   query_record.end[0] <= blockmap.max_x) &&
                  (blockmap.min_z <= query_record.start[2] ||
                   blockmap.min_z <= query_record.end[2]) &&
                  (query_record.start[2] <= blockmap.max_z ||
                   query_record.end[2] <= blockmap.max_z))) {
                continue;
            }
            // The PC zone walker performs these subtractions in 32-bit
            // fixed-point registers. Keep the wrap explicit so a malformed
            // or extreme-but-representable asset cannot invoke signed
            // overflow in the native port.
            const auto width = blockmap.cell_count_x == 0
                                   ? 0
                                   : reference::wrapping_sub(
                                         blockmap.max_x, blockmap.min_x) /
                                         blockmap.cell_count_x;
            const auto depth = blockmap.cell_count_z == 0
                                   ? 0
                                   : reference::wrapping_sub(
                                         blockmap.max_z, blockmap.min_z) /
                                         blockmap.cell_count_z;
            if (width <= 0 || depth <= 0) {
                continue;
            }
            if (blockmap.cell_count_x <=
                    static_cast<std::uint16_t>(
                        std::numeric_limits<std::int16_t>::max()) &&
                blockmap.cell_count_z <=
                    static_cast<std::uint16_t>(
                        std::numeric_limits<std::int16_t>::max())) {
                // The PC table walk is a hand-written line/grid traversal,
                // not a rectangular scan of every cell in the query AABB.
                // Reuse the recovered DDA so cell visitation order (and
                // therefore strict equal-distance tie behavior) matches the
                // original. PSX blockmap storage is row-major x + z*width;
                // the PC candidate table uses a different address stride,
                // so only the emitted coordinates are used here.
                const reference::CollisionZoneGrid zone{
                    .min_x = blockmap.min_x,
                    .min_z = blockmap.min_z,
                    .max_x = blockmap.max_x,
                    .max_z = blockmap.max_z,
                    .cell_divisor = width,
                    .cell_count_x = static_cast<std::int16_t>(
                        blockmap.cell_count_x),
                    .cell_count_z = static_cast<std::int16_t>(
                        blockmap.cell_count_z),
                };
                reference::visit_zone_cells(
                    query_record, 0, zone,
                    [&add, &blockmap](std::size_t, std::int32_t cell_x,
                                      std::int32_t cell_z) {
                        if (cell_x < 0 || cell_z < 0 ||
                            cell_x >= blockmap.cell_count_x ||
                            cell_z >= blockmap.cell_count_z) {
                            return;
                        }
                        const auto index = static_cast<std::size_t>(cell_x) +
                                           static_cast<std::size_t>(cell_z) *
                                               blockmap.cell_count_x;
                        if (index >= blockmap.cells.size()) {
                            return;
                        }
                        for (const auto object_index :
                             blockmap.cells[index].object_indices) {
                            add(object_index);
                        }
                    });
                continue;
            }
            const auto clamp_cell = [](Raw value, Raw minimum, Raw,
                                       Raw cell_size, std::uint16_t count) {
                const auto raw = reference::wrapping_sub(value, minimum) /
                                 cell_size;
                return std::min<std::int32_t>(std::max<std::int32_t>(raw, 0), count - 1);
            };
            const auto min_x = std::min(query_record.start[0], query_record.end[0]);
            const auto max_x = std::max(query_record.start[0], query_record.end[0]);
            const auto min_z = std::min(query_record.start[2], query_record.end[2]);
            const auto max_z = std::max(query_record.start[2], query_record.end[2]);
            const auto cell_x0 = clamp_cell(min_x, blockmap.min_x, blockmap.max_x,
                                            width, blockmap.cell_count_x);
            const auto cell_x1 = clamp_cell(max_x, blockmap.min_x, blockmap.max_x,
                                            width, blockmap.cell_count_x);
            const auto cell_z0 = clamp_cell(min_z, blockmap.min_z, blockmap.max_z,
                                            depth, blockmap.cell_count_z);
            const auto cell_z1 = clamp_cell(max_z, blockmap.min_z, blockmap.max_z,
                                            depth, blockmap.cell_count_z);
            for (auto cell_x = cell_x0; cell_x <= cell_x1; ++cell_x) {
                for (auto cell_z = cell_z0; cell_z <= cell_z1; ++cell_z) {
                    // The PSX reader indexes cells as x + z*cell_count_x
                    // (x is the fast-changing coordinate).
                    const auto index = static_cast<std::size_t>(cell_x) +
                                       static_cast<std::size_t>(cell_z) *
                                           blockmap.cell_count_x;
                    if (index < blockmap.cells.size()) {
                        for (const auto object_index : blockmap.cells[index].object_indices) {
                            add(object_index);
                        }
                    }
                }
            }
        }
        return result;
    }

    bool read_tags(Reader& reader, std::size_t tag_offset) {
        if (!reader.seek(tag_offset, "tag table")) {
            return false;
        }
        while (reader.position() < reader.size()) {
            const auto type = reader.u32("tag type");
            if (!type.has_value()) {
                return false;
            }
            if (*type == 0xffffffffu) {
                return true;
            }
            const auto size = reader.u32("tag size");
            const auto payload_start = reader.position();
            if (!size.has_value() || payload_start > reader.size() ||
                *size > reader.size() - payload_start) {
                reader.fail("tag payload");
                return false;
            }
            const auto payload = reader.bytes().subspan(payload_start, *size);
            if (!reader.skip(*size, "tag payload")) {
                return false;
            }
            if (*type == 0x0000000au &&
                !read_blockmap(payload, reader.error())) {
                return false;
            }
        }
        return true;
    }

    bool read_blockmap(std::span<const std::uint8_t> payload,
                       std::string* error) {
        Reader reader(payload, error);
        PsxBlockmap blockmap;
        const auto min_x = reader.i32("blockmap min x");
        const auto min_z = reader.i32("blockmap min z");
        const auto max_x = reader.i32("blockmap max x");
        const auto max_z = reader.i32("blockmap max z");
        const auto cell_count_x = reader.u16("blockmap cell count x");
        const auto cell_count_z = reader.u16("blockmap cell count z");
        if (!min_x.has_value() || !min_z.has_value() || !max_x.has_value() ||
            !max_z.has_value() || !cell_count_x.has_value() ||
            !cell_count_z.has_value() || *cell_count_x == 0 ||
            *cell_count_z == 0) {
            return false;
        }
        blockmap.min_x = *min_x;
        blockmap.min_z = *min_z;
        blockmap.max_x = *max_x;
        blockmap.max_z = *max_z;
        blockmap.cell_count_x = *cell_count_x;
        blockmap.cell_count_z = *cell_count_z;
        const auto cell_count = static_cast<std::size_t>(*cell_count_x) *
                                *cell_count_z;
        blockmap.cells.reserve(cell_count);
        for (std::size_t index = 0; index < cell_count; ++index) {
            PsxBlockmapCell cell;
            const auto unknown_1 = reader.u32("blockmap cell unknown 1");
            const auto unknown_2 = reader.u32("blockmap cell unknown 2");
            const auto reference_count = reader.u32("blockmap cell reference count");
            if (!unknown_1.has_value() || !unknown_2.has_value() ||
                !reference_count.has_value() ||
                *reference_count > reader.size() / 4u) {
                reader.fail("blockmap cell");
                return false;
            }
            cell.unknown_1 = *unknown_1;
            cell.unknown_2 = *unknown_2;
            cell.object_indices.reserve(*reference_count);
            for (std::uint32_t reference = 0; reference < *reference_count;
                 ++reference) {
                const auto object_index = reader.u32("blockmap object index");
                if (!object_index.has_value() ||
                    *object_index >= objects_.size()) {
                    reader.fail("blockmap references a missing object");
                    return false;
                }
                cell.object_indices.push_back(*object_index);
            }
            const auto terminator = reader.u32("blockmap cell terminator");
            if (!terminator.has_value() || *terminator != 0) {
                reader.fail("blockmap cell terminator");
                return false;
            }
            blockmap.cells.push_back(std::move(cell));
        }
        if (reader.position() != reader.size()) {
            reader.fail("blockmap trailing bytes");
            return false;
        }
        blockmaps_.push_back(std::move(blockmap));
        return true;
    }

    std::vector<PsxObject> objects_;
    std::vector<PsxModel> models_;
    std::vector<PsxBlockmap> blockmaps_;
};

}  // namespace opentony::collision
