#pragma once

// Evidence-backed C++ boundary for the recovered PC collision query.
//
// This is deliberately not the engine's complete scene implementation.  The
// level-file loader and cache ownership are still engine-dependent; this
// header captures the query record, model/face primitive, and zone traversal
// contracts without inventing a complete file parser.

#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <limits>
#include <optional>
#include <span>

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
    // q+0x48..0x58.  The rows are consumed by the linked/oriented-object
    // path; the static face path only needs them when it enters that branch.
    std::array<std::int16_t, 9> line_basis{};
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

// The PC code addresses this record as a byte-oriented x86 structure.  Keep
// the unknown gaps explicit so a native port cannot accidentally shift the
// fields that the recovered callees access.  Pointer fields are uint32_t on
// purpose: this is the 32-bit game ABI, not the host process ABI.
#pragma pack(push, 1)
struct CollisionQueryLayout {
    Raw start[3]{};                 // 0x00
    Raw end[3]{};                   // 0x0c
    Raw bounds_min[3]{};            // 0x18
    Raw bounds_max[3]{};            // 0x24
    std::uint8_t unknown_30[0x10]{};
    Raw hit_distance = kUnhit;      // 0x40
    Raw line_length = 0;            // 0x44
    std::int16_t line_basis[9]{};   // 0x48
    std::uint8_t unknown_5a[0x0e]{};
    std::uint32_t hit_body = 0;     // 0x68
    Raw hit_position[3]{};          // 0x6c
    std::int16_t hit_normal[3]{};   // 0x78
    std::uint16_t unknown_7e = 0;
    std::uint32_t hit_face_record = 0;  // 0x80
    std::uint16_t hit_model_index = 0xffff;  // 0x84
    std::uint16_t unknown_86 = 0;
    std::uint8_t query_mask_mode = 0;  // 0x88
    std::uint8_t direction_flag = 0;   // 0x89
    std::uint16_t query_stamp = 0;      // 0x8a
    Raw hit_parameter = kUnhit;         // 0x8c
};
#pragma pack(pop)

static_assert(sizeof(CollisionQueryLayout) == 0x90);
static_assert(offsetof(CollisionQueryLayout, hit_body) == 0x68);
static_assert(offsetof(CollisionQueryLayout, hit_position) == 0x6c);
static_assert(offsetof(CollisionQueryLayout, hit_normal) == 0x78);
static_assert(offsetof(CollisionQueryLayout, hit_face_record) == 0x80);
static_assert(offsetof(CollisionQueryLayout, hit_parameter) == 0x8c);

inline CollisionQueryLayout to_query_layout(const QueryRecord& query) {
    CollisionQueryLayout raw{};
    std::copy(query.start.begin(), query.start.end(), std::begin(raw.start));
    std::copy(query.end.begin(), query.end.end(), std::begin(raw.end));
    std::copy(query.bounds_min.begin(), query.bounds_min.end(),
              std::begin(raw.bounds_min));
    std::copy(query.bounds_max.begin(), query.bounds_max.end(),
              std::begin(raw.bounds_max));
    raw.hit_distance = query.hit_distance;
    raw.line_length = query.line_length;
    std::copy(query.line_basis.begin(), query.line_basis.end(),
              std::begin(raw.line_basis));
    raw.hit_body = query.hit_body;
    std::copy(query.hit_position.begin(), query.hit_position.end(),
              std::begin(raw.hit_position));
    std::copy(query.hit_normal.begin(), query.hit_normal.end(),
              std::begin(raw.hit_normal));
    raw.hit_face_record = query.hit_face_record;
    raw.hit_model_index = query.hit_model_index;
    raw.query_mask_mode = query.query_mask_mode;
    raw.direction_flag = query.direction_flag;
    raw.query_stamp = query.query_stamp;
    raw.hit_parameter = query.hit_parameter;
    return raw;
}

inline QueryRecord from_query_layout(const CollisionQueryLayout& raw) {
    QueryRecord query{};
    std::copy(std::begin(raw.start), std::end(raw.start), query.start.begin());
    std::copy(std::begin(raw.end), std::end(raw.end), query.end.begin());
    std::copy(std::begin(raw.bounds_min), std::end(raw.bounds_min),
              query.bounds_min.begin());
    std::copy(std::begin(raw.bounds_max), std::end(raw.bounds_max),
              query.bounds_max.begin());
    query.hit_distance = raw.hit_distance;
    query.line_length = raw.line_length;
    std::copy(std::begin(raw.line_basis), std::end(raw.line_basis),
              query.line_basis.begin());
    query.hit_body = raw.hit_body;
    std::copy(std::begin(raw.hit_position), std::end(raw.hit_position),
              query.hit_position.begin());
    std::copy(std::begin(raw.hit_normal), std::end(raw.hit_normal),
              query.hit_normal.begin());
    query.hit_face_record = raw.hit_face_record;
    query.hit_model_index = raw.hit_model_index;
    query.query_mask_mode = raw.query_mask_mode;
    query.direction_flag = raw.direction_flag;
    query.query_stamp = raw.query_stamp;
    query.hit_parameter = raw.hit_parameter;
    return query;
}

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

// 0x004660b0 uses two related strides.  A zone record is addressed as
// base + zone_index*0x660 bytes.  The candidate-pointer table is indexed as
// zone_index*0x198 + cell_x*0x14 + cell_z, and each element is a 32-bit
// pointer; its zone group therefore also occupies 0x660 bytes.  The 0x198
// quantity is an element stride, not a byte stride.
inline constexpr std::size_t kZoneRecordStrideBytes = 0x660;
inline constexpr std::size_t kZoneCandidateGroupStride = 0x198;
inline constexpr std::size_t kZoneCandidateCellXStride = 0x14;

struct CollisionZoneGrid {
    Raw min_x = 0;             // zone +0x84
    Raw min_z = 0;             // zone +0x88
    Raw max_x = 0;             // zone +0x8c
    Raw max_z = 0;             // zone +0x90
    Raw cell_divisor = 0;      // zone +0x94
    std::int16_t cell_count_x = 0;  // zone +0x9c
    std::int16_t cell_count_z = 0;  // zone +0x9e
};

inline bool zone_overlaps_query(const CollisionZoneGrid& zone,
                                const QueryRecord& query) {
    // This preserves the inclusive overlap form in 0x004660b0.  It is
    // intentionally a 2-D X/Z test; Y is handled by face AABBs.
    return ((zone.min_x <= query.start[0] || zone.min_x <= query.end[0]) &&
            (query.start[0] <= zone.max_x || query.end[0] <= zone.max_x) &&
            (zone.min_z <= query.start[2] || zone.min_z <= query.end[2]) &&
            (query.start[2] <= zone.max_z || query.end[2] <= zone.max_z));
}

inline std::optional<std::size_t> zone_candidate_index(
    std::size_t zone_index, std::int32_t cell_x, std::int32_t cell_z,
    const CollisionZoneGrid& zone) {
    if (zone.cell_divisor <= 0 || cell_x < 0 || cell_z < 0 ||
        cell_x >= zone.cell_count_x || cell_z >= zone.cell_count_z) {
        return std::nullopt;
    }
    return zone_index * kZoneCandidateGroupStride +
           static_cast<std::size_t>(cell_x) * kZoneCandidateCellXStride +
           static_cast<std::size_t>(cell_z);
}

inline Raw zone_mul_div(Raw lhs, Raw rhs, Raw divisor) {
    if (divisor == 0) {
        return 0;
    }
    return static_cast<Raw>(static_cast<std::uint32_t>(
        static_cast<std::int64_t>(lhs) * rhs / divisor));
}

// Visit the same 2-D grid cells as 0x004660b0.  The callback receives the
// candidate-table index followed by the integer X/Z cell coordinates.  It is
// deliberately a callback rather than a pointer-table reader: loading the
// zone file and resolving each candidate-list pointer are separate concerns.
template <typename Visitor>
inline void visit_zone_cells(const QueryRecord& query,
                             std::size_t zone_index,
                             const CollisionZoneGrid& zone,
                             Visitor&& visitor) {
    if (zone.cell_divisor <= 0 || zone.cell_count_x <= 0 ||
        zone.cell_count_z <= 0 || !zone_overlaps_query(zone, query)) {
        return;
    }

    Raw x0 = query.start[0];
    Raw z0 = query.start[2];
    Raw x1 = query.end[0];
    Raw z1 = query.end[2];
    if (x0 == x1 && z0 == z1) {
        auto cell_x = (x0 - zone.min_x) / zone.cell_divisor;
        auto cell_z = (z0 - zone.min_z) / zone.cell_divisor;
        if (cell_x == zone.cell_count_x) {
            --cell_x;
        }
        if (cell_z == zone.cell_count_z) {
            --cell_z;
        }
        const auto index = zone_candidate_index(zone_index, cell_x, cell_z, zone);
        if (index) {
            visitor(*index, cell_x, cell_z);
        }
        return;
    }

    // Liang-Barsky-like endpoint clipping, written in the branch order used
    // by the original hand-coded grid walker.  The helper at 0x004f5f10 is
    // a floating multiply/divide converted with x87 truncation, equivalent
    // to zone_mul_div for these integer operands.
    if (x0 < zone.min_x) {
        Raw clipped_z = 0;
        if (z0 < z1) {
            clipped_z = zone_mul_div(zone.min_x - x0, z1 - z0,
                                     x1 - x0) + z0;
        } else {
            clipped_z = zone_mul_div(x1 - zone.min_x, z0 - z1,
                                     x1 - x0) + z1;
        }
        x0 = zone.min_x;
        z0 = clipped_z;
    }
    if (x1 < zone.min_x) {
        Raw clipped_z = 0;
        if (z1 < z0) {
            clipped_z = zone_mul_div(zone.min_x - x1, z0 - z1,
                                     x0 - x1) + z1;
        } else {
            clipped_z = zone_mul_div(x0 - zone.min_x, z1 - z0,
                                     x0 - x1) + z0;
        }
        x1 = zone.min_x;
        z1 = clipped_z;
    }
    if (x0 > zone.max_x) {
        Raw clipped_z = 0;
        if (z1 < z0) {
            clipped_z = zone_mul_div(zone.max_x - x1, z0 - z1,
                                     x0 - x1) + z1;
        } else {
            clipped_z = zone_mul_div(x0 - zone.max_x, z1 - z0,
                                     x0 - x1) + z0;
        }
        x0 = zone.max_x;
        z0 = clipped_z;
    }
    if (x1 > zone.max_x) {
        Raw clipped_z = 0;
        if (z0 < z1) {
            clipped_z = zone_mul_div(zone.max_x - x0, z1 - z0,
                                     x1 - x0) + z0;
        } else {
            clipped_z = zone_mul_div(x0 - zone.max_x, z0 - z1,
                                     x1 - x0) + z1;
        }
        x1 = zone.max_x;
        z1 = clipped_z;
    }
    if (z0 < zone.min_z) {
        Raw clipped_x = 0;
        if (x0 < x1) {
            clipped_x = zone_mul_div(zone.min_z - z0, x1 - x0,
                                     z1 - z0) + x0;
        } else {
            clipped_x = zone_mul_div(z1 - zone.min_z, x0 - x1,
                                     z1 - z0) + x1;
        }
        z0 = zone.min_z;
        x0 = clipped_x;
    }
    if (z1 < zone.min_z) {
        Raw clipped_x = 0;
        if (x1 < x0) {
            clipped_x = zone_mul_div(zone.min_z - z1, x0 - x1,
                                     z0 - z1) + x1;
        } else {
            clipped_x = zone_mul_div(z0 - zone.min_z, x1 - x0,
                                     z0 - z1) + x0;
        }
        z1 = zone.min_z;
        x1 = clipped_x;
    }
    if (z0 > zone.max_z) {
        Raw clipped_x = 0;
        if (x1 < x0) {
            clipped_x = zone_mul_div(zone.max_z - z1, x0 - x1,
                                     z0 - z1) + x1;
        } else {
            clipped_x = zone_mul_div(z0 - zone.max_z, x1 - x0,
                                     z0 - z1) + x0;
        }
        z0 = zone.max_z;
        x0 = clipped_x;
    }
    if (z1 > zone.max_z) {
        Raw clipped_x = 0;
        if (x0 < x1) {
            clipped_x = zone_mul_div(zone.max_z - z0, x1 - x0,
                                     z1 - z0) + x0;
        } else {
            clipped_x = zone_mul_div(z0 - zone.max_z, x0 - x1,
                                     z0 - z1) + x1;
        }
        z1 = zone.max_z;
        x1 = clipped_x;
    }

    std::int32_t cell_x0 = (x0 - zone.min_x) / zone.cell_divisor;
    std::int32_t cell_x1 = (x1 - zone.min_x) / zone.cell_divisor;
    std::int32_t cell_z0 = (z0 - zone.min_z) / zone.cell_divisor;
    std::int32_t cell_z1 = (z1 - zone.min_z) / zone.cell_divisor;
    Raw rem_x = (x0 - zone.min_x) % zone.cell_divisor;
    Raw rem_z = (z0 - zone.min_z) % zone.cell_divisor;
    if (cell_x0 == zone.cell_count_x) {
        --cell_x0;
        rem_x += zone.cell_divisor;
    }
    if (cell_z0 == zone.cell_count_z) {
        --cell_z0;
        rem_z += zone.cell_divisor;
    }
    if (cell_x1 == zone.cell_count_x) {
        --cell_x1;
    }
    if (cell_z1 == zone.cell_count_z) {
        --cell_z1;
    }

    const Raw abs_dx = x1 >= x0 ? x1 - x0 : x0 - x1;
    const Raw abs_dz = z1 >= z0 ? z1 - z0 : z0 - z1;
    const std::int32_t step_x = x1 >= x0 ? 1 : -1;
    const std::int32_t step_z = z1 >= z0 ? 1 : -1;
    Raw decision = zone_mul_div(abs_dx, rem_z, zone.cell_divisor);
    Raw z_boundary = zone_mul_div(abs_dz, rem_x, zone.cell_divisor);
    if (x0 < x1) {
        z_boundary -= abs_dz;
    } else {
        z_boundary = -z_boundary;
    }
    if (z0 < z1) {
        decision = abs_dx - decision;
    }
    decision += z_boundary;

    auto emit = [&](std::int32_t cell_x, std::int32_t cell_z) {
        const auto index = zone_candidate_index(zone_index, cell_x, cell_z, zone);
        if (index) {
            visitor(*index, cell_x, cell_z);
        }
    };
    if (cell_x0 != cell_x1 || cell_z0 != cell_z1) {
        emit(cell_x0, cell_z0);
        while (cell_x0 != cell_x1 || cell_z0 != cell_z1) {
            if (decision < 0) {
                decision += abs_dx;
                cell_z0 += step_z;
            } else {
                cell_x0 += step_x;
                decision -= abs_dz;
            }
            emit(cell_x0, cell_z0);
        }
    } else {
        emit(cell_x0, cell_z0);
    }
}

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
    // The static walker advances a uint32_t pointer by word0 >> 18 words;
    // the dynamic walker adds word0 >> 16 bytes.  The upper halfword is the
    // encoded byte length, normally divisible by four, so both paths agree.
    return static_cast<std::size_t>(face_word_zero >> 16u);
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

inline Raw x86_shift_left(Raw value, unsigned count) {
    return static_cast<Raw>(static_cast<std::uint32_t>(value) << (count & 31u));
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

struct CollisionModelFace {
    FaceGeometry geometry{};
    std::uint32_t base_word = 0;
    std::uint32_t surface_word = 0;
    std::size_t record_offset = 0;
};

inline std::optional<std::uint16_t> read_le_u16(std::span<const std::uint8_t> bytes,
                                                 std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 2) {
        return std::nullopt;
    }
    return static_cast<std::uint16_t>(bytes[offset]) |
           (static_cast<std::uint16_t>(bytes[offset + 1]) << 8u);
}

inline std::optional<std::int16_t> read_le_i16(std::span<const std::uint8_t> bytes,
                                                std::size_t offset) {
    const auto value = read_le_u16(bytes, offset);
    return value ? std::optional<std::int16_t>(static_cast<std::int16_t>(*value))
                 : std::nullopt;
}

inline std::optional<std::uint32_t> read_le_u32(std::span<const std::uint8_t> bytes,
                                                 std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 4) {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(bytes[offset]) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 8u) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 16u) |
           (static_cast<std::uint32_t>(bytes[offset + 3]) << 24u);
}

struct CollisionModelView {
    std::span<const std::uint8_t> bytes{};
    // The original writes a model pointer into q+0x68 and a face pointer into
    // q+0x80.  These are caller-supplied 32-bit identities when the model is
    // represented by a file/cache view rather than live game memory.
    std::uint32_t body_id = 0;
    std::uint32_t face_address_base = 0;

    std::optional<CollisionModelHeader> header() const {
        if (bytes.size() < sizeof(CollisionModelHeader)) {
            return std::nullopt;
        }
        CollisionModelHeader result{};
        result.unknown_flags = *read_le_u16(bytes, 0x00);
        result.vertex_count = *read_le_u16(bytes, 0x02);
        result.normal_count = *read_le_u16(bytes, 0x04);
        result.face_count = *read_le_u16(bytes, 0x06);
        result.radius = *read_le_u32(bytes, 0x08);
        result.x_max = *read_le_i16(bytes, 0x0c);
        result.x_min = *read_le_i16(bytes, 0x0e);
        result.y_max = *read_le_i16(bytes, 0x10);
        result.y_min = *read_le_i16(bytes, 0x12);
        result.z_max = *read_le_i16(bytes, 0x14);
        result.z_min = *read_le_i16(bytes, 0x16);
        result.unknown_value = *read_le_u32(bytes, 0x18);
        return result;
    }

    std::optional<RawVec3> vertex(std::uint16_t index) const {
        const auto model = header();
        if (!model || index >= model->vertex_count) {
            return std::nullopt;
        }
        const auto offset = kModelVertexBase + model_record_offset(index);
        const auto x = read_le_i16(bytes, offset);
        const auto y = read_le_i16(bytes, offset + 2);
        const auto z = read_le_i16(bytes, offset + 4);
        if (!x || !y || !z) {
            return std::nullopt;
        }
        return RawVec3{*x, *y, *z};
    }

    std::optional<std::array<std::int16_t, 3>> normal(std::uint16_t index) const {
        const auto model = header();
        if (!model || index >= model->normal_count) {
            return std::nullopt;
        }
        const auto offset = model_normal_offset(*model) + model_record_offset(index);
        const auto x = read_le_i16(bytes, offset);
        const auto y = read_le_i16(bytes, offset + 2);
        const auto z = read_le_i16(bytes, offset + 4);
        if (!x || !y || !z) {
            return std::nullopt;
        }
        return std::array<std::int16_t, 3>{*x, *y, *z};
    }

    std::optional<CollisionModelFace> face(std::uint16_t index) const {
        const auto model = header();
        if (!model || index >= model->face_count) {
            return std::nullopt;
        }
        std::size_t offset = model_face_offset(*model);
        for (std::uint16_t current = 0; current < index; ++current) {
            const auto word = read_le_u32(bytes, offset);
            if (!word) {
                return std::nullopt;
            }
            offset += face_record_stride_bytes(*word);
        }
        const auto word = read_le_u32(bytes, offset);
        if (!word || offset > bytes.size() || bytes.size() - offset < 0x10) {
            return std::nullopt;
        }
        const auto normal_word = read_le_u16(bytes, offset + 0x0c);
        const auto surface_flags = read_le_u16(bytes, offset + 0x0e);
        if (!normal_word || !surface_flags) {
            return std::nullopt;
        }
        const auto v0 = vertex(bytes[offset + 4]);
        const auto v1 = vertex(bytes[offset + 5]);
        const auto v2 = vertex(bytes[offset + 6]);
        const bool is_triangle = ((*word & 0x10u) != 0);
        const auto v3 = is_triangle ? std::optional<RawVec3>(RawVec3{})
                                    : vertex(bytes[offset + 7]);
        const auto plane_normal = normal(face_normal_index(*normal_word));
        if (!v0 || !v1 || !v2 || !plane_normal || (!is_triangle && !v3)) {
            return std::nullopt;
        }
        return CollisionModelFace{
            .geometry = FaceGeometry{
                .vertex0 = *v0,
                .vertex1 = *v1,
                .vertex2 = *v2,
                .vertex3 = *v3,
                .plane_normal = *plane_normal,
                .is_triangle = is_triangle,
            },
            .base_word = *word,
            .surface_word = (static_cast<std::uint32_t>(*surface_flags) << 16u) |
                            static_cast<std::uint32_t>(*normal_word),
            .record_offset = offset,
        };
    }
};

// 0x004f4b00 writes a temporary transformed-vertex stream with the same
// eight-byte record size as model vertices: three signed shorts followed by
// a six-bit clipping mask.  The stream is q-oriented, so the query segment
// runs along its positive Z axis and the dynamic face walker can reject a
// whole face with a shared clip-code AND.
#pragma pack(push, 1)
struct DynamicVertexRecord {
    std::int16_t x = 0;
    std::int16_t y = 0;
    std::int16_t z = 0;
    std::uint16_t clip_mask = 0;
};
#pragma pack(pop)

static_assert(sizeof(DynamicVertexRecord) == 0x08);

struct DynamicFaceIndices {
    std::uint8_t vertex0 = 0;
    std::uint8_t vertex1 = 0;
    std::uint8_t vertex2 = 0;
    std::uint8_t vertex3 = 0;
};

inline constexpr DynamicFaceIndices dynamic_face_indices(
    std::uint32_t packed_indices) {
    // 0x004f4c50 consumes the bytes in face order +4,+5,+6,+7.  Its
    // decompiler names the accesses by significance because the word is
    // loaded as a uint32_t.
    return {
        static_cast<std::uint8_t>(packed_indices),
        static_cast<std::uint8_t>(packed_indices >> 8u),
        static_cast<std::uint8_t>(packed_indices >> 16u),
        static_cast<std::uint8_t>(packed_indices >> 24u),
    };
}

inline constexpr std::uint16_t dynamic_clip_mask(Raw x, Raw y, Raw z,
                                                  Raw line_length) {
    return static_cast<std::uint16_t>(
        (x < 0 ? 0x004u : 0u) |
        (y < 0 ? 0x002u : 0u) |
        (z < 0 ? 0x001u : 0u) |
        (line_length < z ? 0x008u : 0u) |
        (x > 0 ? 0x400u : 0u) |
        (y > 0 ? 0x200u : 0u));
}

inline bool dynamic_face_clip_accepts(const DynamicVertexRecord& vertex0,
                                      const DynamicVertexRecord& vertex1,
                                      const DynamicVertexRecord& vertex2,
                                      const DynamicVertexRecord& vertex3) {
    return (vertex0.clip_mask & vertex1.clip_mask & vertex2.clip_mask &
            vertex3.clip_mask & 0x60fu) == 0;
}

struct DynamicProjectedFace {
    // 0x004f4c50 uses v0 for the first triangle, or v3 for the alternate
    // quad triangle when the v1/v2 determinant has the opposite winding.
    std::uint8_t first_vertex = 0;
    std::array<Raw, 3> determinants{};
    bool accepted = false;
};

inline Raw dynamic_projected_determinant(const DynamicVertexRecord& lhs,
                                         const DynamicVertexRecord& rhs) {
    // The dynamic walker works in the transformed X/Y projection.  Products
    // of signed 16-bit records fit in a signed 32-bit result, but widen the
    // C++ expression before reproducing the original subtraction.
    return wrapping_from_i64(
        static_cast<std::int64_t>(lhs.x) * rhs.y -
        static_cast<std::int64_t>(lhs.y) * rhs.x);
}

inline DynamicProjectedFace dynamic_projected_face(
    const DynamicVertexRecord& vertex0,
    const DynamicVertexRecord& vertex1,
    const DynamicVertexRecord& vertex2,
    const DynamicVertexRecord& vertex3,
    bool is_triangle) {
    DynamicProjectedFace result;
    result.determinants = {
        dynamic_projected_determinant(vertex1, vertex2),
        dynamic_projected_determinant(vertex2, vertex0),
        dynamic_projected_determinant(vertex0, vertex1),
    };
    if (result.determinants[0] < 0) {
        if (is_triangle) {
            return result;
        }
        result.first_vertex = 3;
        result.determinants[1] = wrapping_from_i64(
            -static_cast<std::int64_t>(
                dynamic_projected_determinant(vertex2, vertex3)));
        result.determinants[2] = wrapping_from_i64(
            -static_cast<std::int64_t>(
                dynamic_projected_determinant(vertex3, vertex1)));
    }
    // The original condition is `-1 < (det2 | det1)`, not two independent
    // signed comparisons. Preserve that bitwise behavior at the boundary.
    const auto combined = static_cast<std::uint32_t>(result.determinants[1]) |
                          static_cast<std::uint32_t>(result.determinants[2]);
    result.accepted = (combined & 0x80000000u) == 0;
    return result;
}

inline Raw q12_transform_component_raw(const std::array<std::int16_t, 9>& basis,
                                       std::size_t row,
                                       Raw x,
                                       Raw y,
                                       Raw z) {
    const auto offset = row * 3;
    const auto sum = static_cast<std::int64_t>(basis[offset]) * x +
                     static_cast<std::int64_t>(basis[offset + 1]) * y +
                     static_cast<std::int64_t>(basis[offset + 2]) * z;
    return wrapping_from_i64(sum >> 12u);
}

// Reference reconstruction of 0x004f4b00.  model_origin_units is the
// object-origin minus query-start displacement after the original >>12
// conversion (the fast non-rotated path in 0x00463e50 computes exactly this
// value).  The returned AND mask is the value tested against 0x60f by the
// caller before 0x004f4c50 scans faces.
inline std::uint16_t transform_model_vertices(
    const CollisionModelView& model,
    const RawVec3& model_origin_units,
    const std::array<std::int16_t, 9>& query_basis,
    Raw line_length,
    std::span<DynamicVertexRecord> output) {
    const auto model_header = model.header();
    if (!model_header || output.size() < model_header->vertex_count) {
        return 0x60f;
    }
    std::uint16_t mask = 0x60f;
    for (std::uint16_t index = 0; index < model_header->vertex_count; ++index) {
        const auto vertex = model.vertex(index);
        if (!vertex) {
            return 0x60f;
        }
        const Raw x = wrapping_add((*vertex)[0], model_origin_units[0]);
        const Raw y = wrapping_add((*vertex)[1], model_origin_units[1]);
        const Raw z = wrapping_add((*vertex)[2], model_origin_units[2]);
        const Raw transformed_x = wrapping_add(
            q12_transform_component_raw(query_basis, 0, x, y, z), 0);
        const Raw transformed_y = wrapping_add(
            q12_transform_component_raw(query_basis, 1, x, y, z), 0);
        const Raw transformed_z = wrapping_add(
            q12_transform_component_raw(query_basis, 2, x, y, z), 0);
        auto& record = output[index];
        record.x = static_cast<std::int16_t>(transformed_x);
        record.y = static_cast<std::int16_t>(transformed_y);
        record.z = static_cast<std::int16_t>(transformed_z);
        record.clip_mask = dynamic_clip_mask(transformed_x, transformed_y,
                                              transformed_z, line_length);
        mask = static_cast<std::uint16_t>(mask & record.clip_mask);
    }
    return mask;
}

template <typename Visitor>
inline void visit_model_face_aabbs(const CollisionModelView& model,
                                   const RawVec3& model_origin_raw,
                                   Visitor&& visitor) {
    const auto model_header = model.header();
    if (!model_header) {
        return;
    }
    for (std::uint16_t index = 0; index < model_header->face_count; ++index) {
        const auto face = model.face(index);
        if (!face) {
            continue;
        }
        const auto world_vertex = [&model_origin_raw](const RawVec3& vertex) {
            RawVec3 result{};
            for (std::size_t axis = 0; axis < result.size(); ++axis) {
                const auto offset = static_cast<std::int64_t>(vertex[axis]) * 0x1000;
                result[axis] = static_cast<Raw>(static_cast<std::uint32_t>(
                    static_cast<std::int64_t>(model_origin_raw[axis]) + offset));
            }
            return result;
        };
        const auto first = world_vertex(face->geometry.vertex0);
        RawVec3 min_corner = first;
        RawVec3 max_corner = first;
        const std::array<RawVec3, 3> remaining{
            world_vertex(face->geometry.vertex1),
            world_vertex(face->geometry.vertex2),
            world_vertex(face->geometry.vertex3),
        };
        const std::size_t vertex_count = face->geometry.is_triangle ? 2 : 3;
        for (std::size_t vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
            for (std::size_t axis = 0; axis < min_corner.size(); ++axis) {
                min_corner[axis] = std::min(min_corner[axis], remaining[vertex_index][axis]);
                max_corner[axis] = std::max(max_corner[axis], remaining[vertex_index][axis]);
            }
        }
        visitor(index, CollisionFaceAabbRecord{
            .base_flags = face->base_word & 0xffffu,
            .min_x = min_corner[0],
            .min_y = min_corner[1],
            .min_z = min_corner[2],
            .max_x = max_corner[0],
            .max_y = max_corner[1],
            .max_z = max_corner[2],
        });
    }
}

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

struct NormalizedSqrt {
    // The original helper first shifts the integer until its sign bit is set,
    // computes sqrt(value << (lead_minus_one & 0x1e)), then shifts the result
    // back by lead_minus_one >> 1.  Keeping both forms is necessary because
    // the normalized root is reused while constructing the line basis.
    Raw normalized_root = 0;
    Raw unscaled_root = 0;
    unsigned lead_minus_one = 0;
};

inline NormalizedSqrt normalized_sqrt(Raw value) {
    if (value <= 0) {
        return {};
    }
    std::uint32_t shifted = static_cast<std::uint32_t>(value);
    unsigned leading_shift = 0;
    while ((shifted & 0x80000000u) == 0) {
        shifted <<= 1;
        ++leading_shift;
    }
    const unsigned lead_minus_one = leading_shift - 1;
    const unsigned root_shift = lead_minus_one & 0x1eu;
    const auto root = integer_sqrt_floor(
        static_cast<std::uint64_t>(static_cast<std::uint32_t>(value)) << root_shift);
    return {
        .normalized_root = static_cast<Raw>(root),
        .unscaled_root = static_cast<Raw>(root >> (lead_minus_one >> 1u)),
        .lead_minus_one = lead_minus_one,
    };
}

inline std::int16_t q12_matrix_component(std::int16_t a,
                                         std::int16_t b,
                                         std::int16_t c,
                                         Raw x,
                                         Raw y,
                                         Raw z) {
    const auto sum = static_cast<std::int64_t>(a) * x +
                     static_cast<std::int64_t>(b) * y +
                     static_cast<std::int64_t>(c) * z;
    return static_cast<std::int16_t>(arithmetic_shift_right_12(
        wrapping_from_i64(sum)));
}

inline std::int16_t object_angle_cos_q12(std::int16_t angle) {
    constexpr double kFullTurn = 6.283185307179586476925286766559;
    return static_cast<std::int16_t>(static_cast<Raw>(
        std::cos(static_cast<double>(angle) * kFullTurn / 4096.0) * 4096.0));
}

inline std::int16_t object_angle_sin_q12(std::int16_t angle) {
    constexpr double kFullTurn = 6.283185307179586476925286766559;
    return static_cast<std::int16_t>(static_cast<Raw>(
        std::sin(static_cast<double>(angle) * kFullTurn / 4096.0) * 4096.0));
}

inline std::array<std::int16_t, 9> compose_q12_basis(
    const std::array<std::int16_t, 9>& lhs,
    const std::array<std::int16_t, 9>& rhs) {
    std::array<std::int16_t, 9> result{};
    for (std::size_t column = 0; column < 3; ++column) {
        const auto x = rhs[column];
        const auto y = rhs[3 + column];
        const auto z = rhs[6 + column];
        result[column] = q12_matrix_component(lhs[0], lhs[1], lhs[2], x, y, z);
        result[3 + column] = q12_matrix_component(lhs[3], lhs[4], lhs[5], x, y, z);
        result[6 + column] = q12_matrix_component(lhs[6], lhs[7], lhs[8], x, y, z);
    }
    return result;
}

// 0x004e7de0, 0x004e7c60, and 0x004e7f60 build Y, X, and Z rotations in
// that order from body+0x14 angle shorts.  The PC scale is one full turn per
// 0x1000 units and the resulting matrix is Q12.
inline std::array<std::int16_t, 9> build_object_rotation_basis(
    const std::array<std::int16_t, 3>& angles) {
    const auto cx = object_angle_cos_q12(angles[0]);
    const auto sx = object_angle_sin_q12(angles[0]);
    const auto cy = object_angle_cos_q12(angles[1]);
    const auto sy = object_angle_sin_q12(angles[1]);
    const auto cz = object_angle_cos_q12(angles[2]);
    const auto sz = object_angle_sin_q12(angles[2]);
    const std::array<std::int16_t, 9> identity{
        0x1000, 0, 0, 0, 0x1000, 0, 0, 0, 0x1000};
    const std::array<std::int16_t, 9> y_rotation{
        cy, 0, sy, 0, 0x1000, 0, static_cast<std::int16_t>(-sy), 0, cy};
    const std::array<std::int16_t, 9> x_rotation{
        0x1000, 0, 0, 0, cx, static_cast<std::int16_t>(-sx), 0, sx, cx};
    const std::array<std::int16_t, 9> z_rotation{
        cz, static_cast<std::int16_t>(-sz), 0, sz, cz, 0, 0, 0, 0x1000};
    return compose_q12_basis(
        compose_q12_basis(compose_q12_basis(identity, y_rotation), x_rotation),
        z_rotation);
}

inline std::array<std::int16_t, 9> build_line_basis(std::int16_t x_delta,
                                                     std::int16_t y_delta,
                                                     std::int16_t z_delta,
                                                     Raw horizontal_squared,
                                                     Raw total_squared) {
    if (horizontal_squared == 0) {
        const std::int16_t vertical_sign = y_delta < 0 ? -0x1000 : 0x1000;
        return {0x1000, 0, 0,
                0, 0, static_cast<std::int16_t>(-vertical_sign),
                0, vertical_sign, 0};
    }

    const auto horizontal = normalized_sqrt(horizontal_squared);
    const auto total = normalized_sqrt(total_squared);
    const auto half_horizontal = horizontal.lead_minus_one >> 1u;
    const auto ratio_shift = half_horizontal + 12u;
    const Raw z_normalized = static_cast<Raw>(
        static_cast<std::int64_t>(x86_shift_left(z_delta, ratio_shift)) /
        horizontal.normalized_root);
    const Raw x_normalized = static_cast<Raw>(
        static_cast<std::int64_t>(x86_shift_left(x_delta, ratio_shift)) /
        horizontal.normalized_root);

    // These are the exact three vectors seeded by 0x004624d0, before its
    // three calls to the Q12 matrix multiply at 0x004e3130.
    const auto half_total = total.lead_minus_one >> 1u;
    const auto vertical_ratio = static_cast<std::int16_t>(
        static_cast<std::int64_t>(x86_shift_left(y_delta,
                                                 (half_total + 24u))) /
        total.normalized_root);
    const Raw horizontal_ratio = static_cast<Raw>(
        static_cast<std::int64_t>(x86_shift_left(horizontal.normalized_root,
                                                 12u)) /
        x86_shift_left(total.normalized_root,
                       (half_horizontal - half_total) & 31u));
    const auto u3 = static_cast<std::int16_t>(horizontal_ratio);

    const std::array<std::int16_t, 9> seeded{
        0x1000, 0, 0,
        0, u3, static_cast<std::int16_t>(-vertical_ratio),
        0, vertical_ratio, u3,
    };
    std::array<std::int16_t, 9> result{};
    // The original writes each matrix column by multiplying the seeded
    // matrix rows by one of the three rotation vectors.
    for (std::size_t column = 0; column < 3; ++column) {
        const Raw vx = column == 0 ? z_normalized :
                       column == 1 ? 0 : -x_normalized;
        const Raw vy = column == 0 ? 0 : column == 1 ? 0x1000 : 0;
        const Raw vz = column == 0 ? x_normalized :
                       column == 1 ? 0 : z_normalized;
        result[column] = q12_matrix_component(seeded[0], seeded[1], seeded[2],
                                               vx, vy, vz);
        result[3 + column] = q12_matrix_component(seeded[3], seeded[4], seeded[5],
                                                   vx, vy, vz);
        result[6 + column] = q12_matrix_component(seeded[6], seeded[7], seeded[8],
                                                   vx, vy, vz);
    }
    return result;
}

// Reconstruct the fields whose setup is established by 0x004624d0, including
// the short basis consumed by the linked/oriented-object path.
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
    const auto horizontal_squared = wrapping_from_i64(
        static_cast<std::int64_t>(square(sx)) + square(sz));
    const auto total_squared = wrapping_add(
        horizontal_squared, wrapping_from_i64(square(sy)));
    query.line_length = normalized_sqrt(total_squared).unscaled_root;
    query.line_basis = build_line_basis(sx, sy, sz, horizontal_squared,
                                         total_squared);

    query.hit_distance = kUnhit;
    query.hit_parameter = kUnhit;
    query.hit_body = 0;
    query.hit_face_record = 0;
    query.hit_model_index = 0xffff;
    query.query_mask_mode = 0;
    query.direction_flag = 0;
    query.query_stamp = query_stamp;

    // The setup routine sets this byte only for a nonnegative vertical
    // degenerate sweep.  build_line_basis has already constructed the same
    // basis; restore the branch flag after the common reset above.
    if (horizontal_squared == 0 && sy >= 0) {
        query.direction_flag = 1;
    }
}

inline std::optional<Raw> segment_parameter(Raw plane_start, Raw plane_end) {
    // 0x00462a20 calls 0x004f5f10(plane_start, 0x4000,
    // plane_start-plane_end).  The first endpoint is therefore the numerator
    // for the q+0 -> q+0xc sweep.
    return trunc_div_checked(static_cast<std::int64_t>(plane_start) *
                                 kParameterScale,
                             static_cast<std::int64_t>(plane_start) - plane_end);
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
    const auto relative_to_start = [&start](const RawVec3& vertex) {
        return RawVec3{
            wrapping_sub(vertex[0], start[0]),
            wrapping_sub(vertex[1], start[1]),
            wrapping_sub(vertex[2], start[2]),
        };
    };
    const auto vertex0 = relative_to_start(face.vertex0);
    const auto vertex1 = relative_to_start(face.vertex1);
    const auto vertex2 = relative_to_start(face.vertex2);
    const auto vertex3 = relative_to_start(face.vertex3);

    // The original expressions correspond to the consistently oriented
    // edges v2->v0, v0->v1, and v1->v2.  Treating all three as
    // oriented_triple_product(vertexN, vertex0, direction) changes the sign
    // of the latter two tests and rejects valid stored windings.
    if (!side_ok(oriented_triple_product(vertex2, vertex0, direction)) ||
        !side_ok(oriented_triple_product(vertex0, vertex1, direction))) {
        return false;
    }
    if (face.is_triangle) {
        return side_ok(oriented_triple_product(vertex1, vertex2, direction));
    }
    return side_ok(oriented_triple_product(vertex1, vertex3, direction)) &&
           side_ok(oriented_triple_product(vertex2, vertex3, direction));
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

inline bool face_aabb_overlaps_query(const CollisionFaceAabbRecord& face_aabb,
                                     const QueryRecord& query) {
    // 0x00462a20 floors query bounds to 1/4096-unit boundaries and expands
    // them by 0x4000 before comparing against the cached world-space AABB.
    const auto floor_fixed = [](Raw value) {
        return static_cast<Raw>(static_cast<std::uint32_t>(value) & 0xfffff000u);
    };
    const Raw min_x = wrapping_add(floor_fixed(query.bounds_min[0]), -0x4000);
    const Raw min_y = wrapping_add(floor_fixed(query.bounds_min[1]), -0x4000);
    const Raw min_z = wrapping_add(floor_fixed(query.bounds_min[2]), -0x4000);
    const Raw max_x = wrapping_add(floor_fixed(query.bounds_max[0]), 0x4000);
    const Raw max_y = wrapping_add(floor_fixed(query.bounds_max[1]), 0x4000);
    const Raw max_z = wrapping_add(floor_fixed(query.bounds_max[2]), 0x4000);
    return face_aabb.min_x <= max_x && min_x <= face_aabb.max_x &&
           face_aabb.min_y <= max_y && min_y <= face_aabb.max_y &&
           face_aabb.min_z <= max_z && min_z <= face_aabb.max_z;
}

struct CollisionFaceFilter {
    std::uint32_t reject_mask = 0;       // DAT_00567a60
    std::uint32_t required_bits = 0xffffffffu;  // DAT_00567a68
    bool query_mask_mode = false;        // q+0x88

    bool accepts(std::uint32_t surface_word) const {
        return (surface_word & reject_mask) == 0 &&
               (surface_word | required_bits) == 0xffffffffu &&
               ((surface_word ^ 0x10000u) & 0x30000u) != 0 &&
               (query_mask_mode || (surface_word & 0x20000u) == 0);
    }
};

// A complete level-zone loader is intentionally outside this evidence layer,
// but the model-face primitive is reusable once a zone/cache supplies a model
// byte span.  The returned count is the number of candidates accepted by the
// geometric test, not merely the number of records visited.
inline std::size_t query_model_faces(QueryRecord& query,
                                     const CollisionModelView& model,
                                     const RawVec3& model_origin_raw,
                                     std::uint16_t model_index,
                                     CollisionFaceFilter filter = {}) {
    const auto model_header = model.header();
    if (!model_header) {
        return 0;
    }
    std::size_t accepted = 0;
    for (std::uint16_t index = 0; index < model_header->face_count; ++index) {
        const auto face = model.face(index);
        if (!face || !filter.accepts(face->surface_word)) {
            continue;
        }
        const auto face_address = wrapping_from_i64(
            static_cast<std::int64_t>(model.face_address_base) + face->record_offset);
        if (record_nearest_face_candidate(query, face->geometry, model_origin_raw,
                                           model.body_id, face_address,
                                           model_index)) {
            ++accepted;
        }
    }
    return accepted;
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

inline std::array<std::int16_t, 3> transform_normal_q12(
    const std::array<std::int16_t, 9>& basis,
    const std::array<std::int16_t, 3>& model_normal) {
    return {
        q12_matrix_component(basis[0], basis[1], basis[2], model_normal[0],
                             model_normal[1], model_normal[2]),
        q12_matrix_component(basis[3], basis[4], basis[5], model_normal[0],
                             model_normal[1], model_normal[2]),
        q12_matrix_component(basis[6], basis[7], basis[8], model_normal[0],
                             model_normal[1], model_normal[2]),
    };
}

// The finalizer at 0x00463d50 supplies the object-rotation basis separately;
// this overload captures its exact Q12 matrix application once that basis is
// available from a dynamic-object view.
inline bool finalize_hit(QueryRecord& query,
                         const std::array<std::int16_t, 9>& object_basis,
                         const std::array<std::int16_t, 3>& model_normal) {
    if (query.hit_body == 0) {
        return false;
    }
    query.hit_normal = transform_normal_q12(object_basis, model_normal);
    return true;
}

inline bool finalize_hit(QueryRecord& query,
                         const std::array<std::int16_t, 3>& object_angles,
                         const std::array<std::int16_t, 3>& model_normal) {
    return finalize_hit(query, build_object_rotation_basis(object_angles),
                        model_normal);
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
