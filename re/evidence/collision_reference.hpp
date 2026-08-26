#pragma once

// Evidence-backed C++ boundary for the recovered PC collision query.
//
// This is deliberately not the engine's scene implementation.  The level
// traversal/cache ownership is still engine-dependent; this header captures
// the query-record math and the recovered model/face byte layout without
// inventing a complete level-file parser.

#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

namespace opentony::collision_reference {

using Raw = std::int32_t;
using RawVec3 = std::array<Raw, 3>;

inline constexpr Raw kParameterScale = 0x4000;
inline constexpr Raw kUnhit = std::numeric_limits<Raw>::max();

struct QueryRecord {
    RawVec3 start{};
    RawVec3 end{};
    RawVec3 bounds_min{};
    RawVec3 bounds_max{};

    Raw hit_distance = kUnhit;
    Raw line_length = 0;
    Raw hit_parameter = kUnhit;

    std::uint32_t hit_body = 0;
    RawVec3 hit_position{};
    std::array<std::int16_t, 3> hit_normal{};
    std::uint32_t hit_face_record = 0;
    std::uint16_t hit_model_index = 0xffff;
    std::uint8_t query_mask_mode = 0;
    std::uint8_t direction_flag = 0;
    std::uint16_t query_stamp = 0;
};

// These packed views match the model block and face prefix consumed by
// 0x00462a20/0x004638d0.  They are byte-layout views, not ownership or
// lifetime claims about the PC loader.
#pragma pack(push, 1)
struct CollisionModelHeader {
    std::uint16_t unknown_flags = 0;
    std::uint16_t vertex_count = 0;
    std::uint16_t normal_count = 0;
    std::uint16_t face_count = 0;
    std::uint32_t radius = 0;
    std::int16_t x_max = 0;
    std::int16_t x_min = 0;
    std::int16_t y_max = 0;
    std::int16_t y_min = 0;
    std::int16_t z_max = 0;
    std::int16_t z_min = 0;
    std::uint32_t unknown_value = 0;
};

struct CollisionVertexRecord {
    std::int16_t x = 0;
    std::int16_t y = 0;
    std::int16_t z = 0;
    std::uint16_t padding = 0;
};

struct CollisionFacePrefix {
    std::uint16_t base_flags = 0;
    std::uint16_t length_bytes = 0;
    std::uint8_t vertex_indices[4]{};
    std::uint8_t render_payload[4]{};
    std::uint16_t normal_index_shifted = 0;
    std::uint16_t surface_flags = 0;
};

struct CollisionModelCacheEntry {
    std::uint32_t model = 0;
    std::uint32_t model_data = 0;
    std::uint32_t face_count = 0;
    std::uint32_t face_aabb_start = 0;
};

struct CollisionFaceAabbRecord {
    std::uint32_t base_flags = 0;
    Raw min_x = 0;
    Raw min_y = 0;
    Raw min_z = 0;
    Raw max_x = 0;
    Raw max_y = 0;
    Raw max_z = 0;
};
#pragma pack(pop)

static_assert(sizeof(CollisionModelHeader) == 0x1c);
static_assert(sizeof(CollisionVertexRecord) == 0x08);
static_assert(sizeof(CollisionFacePrefix) == 0x10);
static_assert(sizeof(CollisionModelCacheEntry) == 0x10);
static_assert(sizeof(CollisionFaceAabbRecord) == 0x1c);

inline constexpr std::size_t kModelVertexBase = 0x1c;
inline constexpr std::size_t kModelRecordSize = sizeof(CollisionVertexRecord);

inline constexpr std::size_t model_normal_offset(
    const CollisionModelHeader& model) {
    return kModelVertexBase +
           static_cast<std::size_t>(model.vertex_count) * kModelRecordSize;
}

inline constexpr std::size_t model_face_offset(
    const CollisionModelHeader& model) {
    return model_normal_offset(model) +
           static_cast<std::size_t>(model.normal_count) * kModelRecordSize;
}

inline constexpr std::size_t model_record_offset(std::uint16_t index) {
    return static_cast<std::size_t>(index) * kModelRecordSize;
}

inline constexpr std::size_t face_record_stride_bytes(
    std::uint32_t face_word_zero) {
    // The face walker advances by 4 + 4*(word0 >> 18).  The upper halfword
    // is therefore a byte length in this build, normally divisible by four.
    return 4u + (static_cast<std::size_t>(face_word_zero >> 18u) * 4u);
}

inline constexpr std::uint16_t face_normal_index(
    std::uint16_t shifted_normal_index) {
    return static_cast<std::uint16_t>(shifted_normal_index >> 3u);
}

struct FaceFlagView {
    std::uint16_t base_flags = 0;
    std::uint16_t surface_flags = 0;
    bool is_triangle = false;
    bool base_nonphysical = false;
    bool surface_bit_40 = false;
    // Cross-build aliases from the packaged model/debug artifacts; raw flags
    // above remain authoritative until a PC material probe falsifies them.
    bool surface_wallrideable = false;
    bool surface_large_polygon = false;
    bool surface_skateable = false;
    bool inverse_bit_23 = false;
    bool inverse_bit_24 = false;
    bool face_bit_80 = false;
    std::uint8_t surface_class = 0;
};

// The PC helper uses signed integer truncation after the floating-point
// divide has been converted back to an integer.  C++ signed division has the
// same truncation-toward-zero behavior for the non-overflow, nonzero cases.
inline std::optional<Raw> trunc_div_checked(std::int64_t numerator,
                                             std::int64_t denominator) {
    if (denominator == 0) {
        return std::nullopt;
    }
    const auto quotient = numerator / denominator;
    if (quotient < std::numeric_limits<Raw>::min() ||
        quotient > std::numeric_limits<Raw>::max()) {
        return std::nullopt;
    }
    return static_cast<Raw>(quotient);
}

inline Raw arithmetic_shift_right_12(Raw value) {
    // Spell out x86 SAR semantics instead of relying on a compiler's choice
    // for right-shifting a negative signed integer.
    auto shifted = static_cast<std::uint32_t>(value) >> 12;
    if (value < 0) {
        // SAR 12 fills only the upper 12 bits; bits 0..19 remain the
        // logical-shifted magnitude.  0xfffff000 would incorrectly destroy
        // bits 12..19 for large negative displacements.
        shifted |= 0xfff00000u;
    }
    return static_cast<Raw>(shifted);
}

inline Raw wrapping_sub(Raw lhs, Raw rhs) {
    return static_cast<Raw>(static_cast<std::uint32_t>(lhs) -
                            static_cast<std::uint32_t>(rhs));
}

inline Raw wrapping_add(Raw lhs, Raw rhs) {
    return static_cast<Raw>(static_cast<std::uint32_t>(lhs) +
                            static_cast<std::uint32_t>(rhs));
}

inline std::int16_t shifted_component(Raw value) {
    return static_cast<std::int16_t>(arithmetic_shift_right_12(value));
}

inline Raw wrapping_from_i64(std::int64_t value) {
    return static_cast<Raw>(static_cast<std::uint32_t>(value));
}

struct FaceGeometry {
    // These vertices and the plane normal are in the model-local integer
    // units used by 0x00462a20 after the query coordinates are >> 12.
    RawVec3 vertex0{};
    RawVec3 vertex1{};
    RawVec3 vertex2{};
    RawVec3 vertex3{};
    std::array<std::int16_t, 3> plane_normal{};
    bool is_triangle = true;  // face flag bit 0x10; clear means quad path
};

inline std::uint32_t integer_sqrt_floor(std::uint64_t value) {
    std::uint64_t result = 0;
    std::uint64_t bit = std::uint64_t{1} << 62;
    while (bit > value) {
        bit >>= 2;
    }
    while (bit != 0) {
        if (value >= result + bit) {
            value -= result + bit;
            result = (result >> 1) + bit;
        } else {
            result >>= 1;
        }
        bit >>= 2;
    }
    return static_cast<std::uint32_t>(result);
}

// Reconstruct the fields whose setup is established by 0x004624d0.  The
// short direction/basis matrix is intentionally left to the future model
// path: its consumers are known, but its complete construction is not.
inline void prepare(QueryRecord& query, std::uint16_t query_stamp = 0) {
    query.bounds_min = {
        std::min(query.start[0], query.end[0]),
        std::min(query.start[1], query.end[1]),
        std::min(query.start[2], query.end[2]),
    };
    query.bounds_max = {
        std::max(query.start[0], query.end[0]),
        std::max(query.start[1], query.end[1]),
        std::max(query.start[2], query.end[2]),
    };

    const Raw dx = wrapping_sub(query.end[0], query.start[0]);
    const Raw dy = wrapping_sub(query.end[1], query.start[1]);
    const Raw dz = wrapping_sub(query.end[2], query.start[2]);
    const auto sx = shifted_component(dx);
    const auto sy = shifted_component(dy);
    const auto sz = shifted_component(dz);
    const auto square = [](std::int16_t value) {
        const auto widened = static_cast<std::int64_t>(value);
        return static_cast<std::uint64_t>(widened * widened);
    };
    const auto length_squared = square(sx) + square(sy) + square(sz);
    query.line_length = static_cast<Raw>(integer_sqrt_floor(length_squared));

    query.hit_distance = kUnhit;
    query.hit_parameter = kUnhit;
    query.hit_body = 0;
    query.hit_face_record = 0;
    query.hit_model_index = 0xffff;
    query.query_mask_mode = 0;
    query.direction_flag = 0;
    query.query_stamp = query_stamp;
}

inline std::optional<Raw> segment_parameter(Raw plane_start, Raw plane_end) {
    // 0x00462a20 evaluates plane_start at q+0 and plane_end at q+0xc, then
    // uses plane_end/(plane_end-plane_start) for the q+0 -> q+0xc sweep.
    return trunc_div_checked(static_cast<std::int64_t>(plane_end) *
                                 kParameterScale,
                             static_cast<std::int64_t>(plane_end) - plane_start);
}

inline std::optional<Raw> interpolate_component(Raw start, Raw end, Raw parameter) {
    // Retain the exact multiply-before-divide order, including negative
    // truncation.
    const auto scaled = trunc_div_checked(
        (static_cast<std::int64_t>(end) - start) * parameter,
        kParameterScale);
    if (!scaled) {
        return std::nullopt;
    }
    return wrapping_add(start, *scaled);
}

inline std::optional<Raw> distance_at_parameter(Raw line_length, Raw parameter) {
    return trunc_div_checked(static_cast<std::int64_t>(line_length) * parameter,
                             kParameterScale);
}

inline RawVec3 to_model_local_units(const RawVec3& raw_position,
                                    const RawVec3& model_origin_raw) {
    RawVec3 result{};
    for (std::size_t axis = 0; axis < result.size(); ++axis) {
        result[axis] = wrapping_sub(arithmetic_shift_right_12(raw_position[axis]),
                                    arithmetic_shift_right_12(model_origin_raw[axis]));
    }
    return result;
}

inline Raw plane_value(const RawVec3& point,
                       const FaceGeometry& face,
                       const RawVec3& model_local_vertex0) {
    Raw value = 0;
    for (std::size_t axis = 0; axis < point.size(); ++axis) {
        const auto product =
            (static_cast<std::int64_t>(point[axis]) - model_local_vertex0[axis]) *
            face.plane_normal[axis];
        value = wrapping_from_i64(static_cast<std::int64_t>(value) + product);
    }
    return value;
}

inline bool plane_crossing_accepted(Raw plane_start, Raw plane_end) {
    // 0x00462a20's integer gate is asymmetric and includes a small
    // penetration threshold before the double-precision side tests.
    if (plane_start < 0 || plane_end > 0) {
        return false;
    }
    if (plane_start == 0) {
        return plane_end != 0 && plane_end < -0x7ff;
    }
    if (plane_start < 0x800) {
        return plane_end < -0x7ff;
    }
    return true;
}

inline double oriented_triple_product(const RawVec3& lhs,
                                      const RawVec3& rhs,
                                      const RawVec3& direction) {
    // This is the exact algebra emitted by the x87 block at 0x4630ae:
    // (lhs x rhs) dot direction.  The assembly writes the terms in z/y/x
    // order, but the result is the ordinary right-handed expansion below.
    return (static_cast<double>(lhs[1]) * rhs[2] -
            static_cast<double>(lhs[2]) * rhs[1]) * direction[0] +
           (static_cast<double>(lhs[2]) * rhs[0] -
            static_cast<double>(lhs[0]) * rhs[2]) * direction[1] +
           (static_cast<double>(lhs[0]) * rhs[1] -
            static_cast<double>(lhs[1]) * rhs[0]) * direction[2];
}

inline bool triangle_side_accepted(const RawVec3& start,
                                   const RawVec3& end,
                                   const FaceGeometry& face) {
    const RawVec3 direction{
        wrapping_sub(end[0], start[0]),
        wrapping_sub(end[1], start[1]),
        wrapping_sub(end[2], start[2]),
    };
    const auto side_ok = [](double value) { return value >= 0.0; };

    if (!side_ok(oriented_triple_product(face.vertex2, face.vertex0,
                                         direction)) ||
        !side_ok(oriented_triple_product(face.vertex1, face.vertex0,
                                         direction))) {
        return false;
    }
    if (face.is_triangle) {
        return side_ok(oriented_triple_product(face.vertex2, face.vertex1,
                                               direction));
    }
    return side_ok(oriented_triple_product(face.vertex3, face.vertex1,
                                           direction)) &&
           side_ok(oriented_triple_product(face.vertex3, face.vertex2,
                                           direction));
}

inline std::optional<RawVec3> contact_at_parameter(const QueryRecord& query,
                                                    Raw parameter) {
    RawVec3 result{};
    for (std::size_t axis = 0; axis < result.size(); ++axis) {
        const auto component = interpolate_component(query.start[axis],
                                                     query.end[axis], parameter);
        if (!component) {
            return std::nullopt;
        }
        result[axis] = *component;
    }
    return result;
}

// 0x00462a20 performs the model-local plane and triangle-side predicates
// before this record update. The plane arguments are evaluated at q+0 and
// q+0xc respectively, matching the endpoint ordering above.
inline bool record_nearest_plane_candidate(QueryRecord& query,
                                            Raw plane_start,
                                            Raw plane_end,
                                            std::uint32_t body,
                                            std::uint32_t face_record,
                                            std::uint16_t model_index) {
    const auto parameter = segment_parameter(plane_start, plane_end);
    if (!parameter || *parameter >= query.hit_parameter) {
        return false;
    }
    const auto contact = contact_at_parameter(query, *parameter);
    const auto distance = distance_at_parameter(query.line_length, *parameter);
    if (!contact || !distance) {
        return false;
    }

    query.hit_parameter = *parameter;
    query.hit_distance = *distance;
    query.hit_body = body;
    query.hit_position = *contact;
    query.hit_face_record = face_record;
    query.hit_model_index = model_index;
    return true;
}

inline bool record_nearest_face_candidate(QueryRecord& query,
                                           const FaceGeometry& face,
                                           const RawVec3& model_origin_raw,
                                           std::uint32_t body,
                                           std::uint32_t face_record,
                                           std::uint16_t model_index) {
    const auto start = to_model_local_units(query.start, model_origin_raw);
    const auto end = to_model_local_units(query.end, model_origin_raw);
    const auto plane_start = plane_value(start, face, face.vertex0);
    const auto plane_end = plane_value(end, face, face.vertex0);
    if (!plane_crossing_accepted(plane_start, plane_end) ||
        !triangle_side_accepted(start, end, face)) {
        return false;
    }
    return record_nearest_plane_candidate(query, plane_start, plane_end,
                                          body, face_record, model_index);
}

// 0x00463d50 supplies the finalized normal only after q+0x68 identifies a
// winning body.  The model-specific transform that produces the three short
// values remains outside this reference layer.
inline bool finalize_hit(QueryRecord& query,
                         std::array<std::int16_t, 3> finalized_normal) {
    if (query.hit_body == 0) {
        return false;
    }
    query.hit_normal = finalized_normal;
    return true;
}

inline FaceFlagView decode_face_flags(std::uint32_t face_word_zero,
                                      std::uint32_t face_word_c) {
    const auto base_flags = static_cast<std::uint16_t>(face_word_zero);
    const auto surface_flags = static_cast<std::uint16_t>(face_word_c >> 16u);
    return FaceFlagView{
        .base_flags = base_flags,
        .surface_flags = surface_flags,
        .is_triangle = (base_flags & 0x10u) != 0,
        .base_nonphysical = (base_flags & 0x80u) != 0,
        .surface_bit_40 = (surface_flags & 0x40u) != 0,
        .surface_wallrideable = (surface_flags & 0x10u) != 0,
        .surface_large_polygon = (surface_flags & 0x40u) != 0,
        .surface_skateable = (surface_flags & 0x100u) == 0,
        .inverse_bit_23 = ((~face_word_c >> 23u) & 1u) != 0,
        .inverse_bit_24 = ((~face_word_c >> 24u) & 1u) != 0,
        .face_bit_80 = (base_flags & 0x80u) != 0,
        .surface_class = static_cast<std::uint8_t>((face_word_c >> 25u) & 0xfu),
    };
}

}  // namespace opentony::collision_reference
