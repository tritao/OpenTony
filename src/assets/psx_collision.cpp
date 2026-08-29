#include "psx_collision.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace opentony::assets {
namespace {

constexpr double kEpsilon = 1.0e-9;

[[nodiscard]] std::array<double, 3> to_double(
    std::array<std::int32_t, 3> value) noexcept {
    return {
        static_cast<double>(value[0]),
        static_cast<double>(value[1]),
        static_cast<double>(value[2]),
    };
}

[[nodiscard]] std::int32_t arithmetic_shift_12(
    std::int32_t value) noexcept {
    if (value >= 0) {
        return value >> 12;
    }
    const std::int64_t magnitude = -static_cast<std::int64_t>(value);
    return static_cast<std::int32_t>(
        -((magnitude + 0xfff) >> 12));
}

[[nodiscard]] std::array<std::int32_t, 3> to_integer_world(
    std::array<std::int32_t, 3> value) noexcept {
    return {
        arithmetic_shift_12(value[0]),
        arithmetic_shift_12(value[1]),
        arithmetic_shift_12(value[2]),
    };
}

[[nodiscard]] bool crosses_retail_collision_plane(
    const PsxCollisionFace& face,
    const std::array<std::int32_t, 3>& integer_start,
    const std::array<std::int32_t, 3>& integer_end) noexcept {
    const std::array<std::int32_t, 3> plane_point =
        to_integer_world(face.vertices[0]);
    const auto plane_value = [&](const std::array<std::int32_t, 3>& point) {
        const std::int64_t x = static_cast<std::int64_t>(point[0]) - plane_point[0];
        const std::int64_t y = static_cast<std::int64_t>(point[1]) - plane_point[1];
        const std::int64_t z = static_cast<std::int64_t>(point[2]) - plane_point[2];
        return x * face.normal[0] + y * face.normal[1] + z * face.normal[2];
    };
    const std::int64_t end_value = plane_value(integer_end);
    const std::int64_t start_value = plane_value(integer_start);
    // FUN_00462a20 accepts the oriented crossing used by the retail query:
    // reject plane_start < 0 or plane_end > 0, reject coplanar endpoints,
    // and require a quantized separation of 0x800 when the start is close to
    // the plane. This orientation matters for a floor whose normal is -Y:
    // the player starts below the plane and crosses toward positive Y.
    return start_value >= 0
        && end_value < 1
        && (start_value >= 0x800
            || (end_value != 0 && end_value < -0x7ff));
}

[[nodiscard]] std::array<double, 3> subtract(
    const std::array<double, 3>& left,
    const std::array<double, 3>& right) noexcept {
    return {left[0] - right[0], left[1] - right[1], left[2] - right[2]};
}

[[nodiscard]] std::array<double, 3> cross(
    const std::array<double, 3>& left,
    const std::array<double, 3>& right) noexcept {
    return {
        left[1] * right[2] - left[2] * right[1],
        left[2] * right[0] - left[0] * right[2],
        left[0] * right[1] - left[1] * right[0],
    };
}

[[nodiscard]] double dot(
    const std::array<double, 3>& left,
    const std::array<double, 3>& right) noexcept {
    return left[0] * right[0] + left[1] * right[1] + left[2] * right[2];
}

[[nodiscard]] std::int32_t checked_fixed_vertex_add(
    std::int32_t left,
    std::int16_t right) {
    const std::int64_t result = static_cast<std::int64_t>(left)
        + static_cast<std::int64_t>(right) * 0x1000;
    if (result < std::numeric_limits<std::int32_t>::min()
        || result > std::numeric_limits<std::int32_t>::max()) {
        throw PsxFormatError("PSX collision vertex overflows fixed-point world coordinates");
    }
    return static_cast<std::int32_t>(result);
}

[[nodiscard]] std::int64_t trunc_div(
    std::int64_t numerator,
    std::int64_t denominator) noexcept {
    if (numerator >= 0) {
        return numerator / denominator;
    }
    return -((-numerator) / denominator);
}

[[nodiscard]] std::array<std::int32_t, 3> world_vertex(
    const PsxObject& object,
    const std::array<std::int16_t, 3>& vertex) {
    return {
        checked_fixed_vertex_add(object.position[0], vertex[0]),
        checked_fixed_vertex_add(object.position[1], vertex[1]),
        checked_fixed_vertex_add(object.position[2], vertex[2]),
    };
}

} // namespace

PsxCollisionQueryOptions make_retail_collision_query_options(
    RetailCollisionFilterInputs inputs,
    bool apply_retail_plane_test) noexcept {
    PsxCollisionQueryOptions options{};
    options.apply_retail_face_filter = true;
    options.apply_retail_plane_test = apply_retail_plane_test;

    // FUN_004660b0 initializes DAT_00567a60 to zero, conditionally assigns
    // 0x400000, toggles that bit, and toggles 0x200000 when DAT_00567c78 is
    // clear. Preserve that order rather than assigning semantic names to the
    // flags before their producers are recovered.
    if (inputs.dat_00567c84) {
        options.reject_mask = 0x00400000U;
    }
    if (inputs.dat_00567c7c) {
        options.reject_mask ^= 0x00400000U;
    }
    if (!inputs.dat_00567c78) {
        options.reject_mask ^= 0x00200000U;
    }

    // The retail OR-mask starts at all ones. A clear bit in this value is a
    // required set bit in the packed source face word.
    options.accept_mask = 0xffffffffU;
    if (inputs.dat_00567c74) {
        options.accept_mask = 0xffefffffU;
    }
    if (inputs.dat_00567c80) {
        options.accept_mask ^= 0x00020000U;
    }
    return options;
}

PsxCollisionMaskView decode_collision_mask(
    std::uint32_t raw_collision_word,
    std::uint16_t face_flags) noexcept {
    const std::uint16_t surface_flags = static_cast<std::uint16_t>(
        raw_collision_word >> 16U);
    return PsxCollisionMaskView{
        static_cast<std::uint16_t>(raw_collision_word & 0xffffU),
        surface_flags,
        (surface_flags & 0x0040U) != 0,
        (surface_flags & 0x0080U) == 0,
        (surface_flags & 0x0100U) == 0,
        static_cast<std::uint8_t>((surface_flags >> 9U) & 0x0fU),
        (face_flags & 0x0080U) != 0,
    };
}

bool accepts_retail_collision_face(
    std::uint32_t raw_collision_word,
    const PsxCollisionQueryOptions& options) noexcept {
    if (!options.apply_retail_face_filter) {
        return true;
    }
    // FUN_00462a20's early face-word predicates, preserving the original
    // decompiler spelling for the two global masks:
    //   (word & DAT_00567a60) == 0
    //   (word | DAT_00567a68) == 0xffffffff
    if ((raw_collision_word & options.reject_mask) != 0
        || (raw_collision_word | options.accept_mask) != 0xffffffffU) {
        return false;
    }
    if (((raw_collision_word ^ 0x00010000U) & 0x00030000U) == 0) {
        return false;
    }
    if (!options.include_trigger_faces
        && (raw_collision_word & 0x00020000U) != 0) {
        return false;
    }
    return true;
}

PsxCollisionWorld PsxCollisionWorld::build(const PsxArchive& archive) {
    PsxCollisionWorld result;
    std::vector<std::vector<std::size_t>> object_faces(archive.objects().size());
    std::vector<bool> referenced(archive.objects().size(), false);

    for (std::size_t blockmap_index = 0;
         blockmap_index < archive.blockmaps().size();
         ++blockmap_index) {
        const PsxBlockmap& source = archive.blockmaps()[blockmap_index];
        PsxCollisionGrid grid{};
        grid.source_blockmap = blockmap_index;
        grid.bounds = source.bounds;
        grid.cell_counts = source.cell_counts;
        grid.cells.resize(source.cells.size());
        for (std::size_t cell_index = 0; cell_index < source.cells.size(); ++cell_index) {
            const PsxBlockmapCell& source_cell = source.cells[cell_index];
            PsxCollisionCell& cell = grid.cells[cell_index];
            cell.unknown_1 = source_cell.unknown_1;
            cell.unknown_2 = source_cell.unknown_2;
            for (const std::uint32_t object_index : source_cell.object_indices) {
                if (object_index >= archive.objects().size()) {
                    throw PsxFormatError("PSX collision blockmap references a missing object");
                }
                referenced[object_index] = true;
            }
        }
        result.grids_.push_back(std::move(grid));
    }

    for (std::size_t object_index = 0;
         object_index < archive.objects().size();
         ++object_index) {
        if (!referenced[object_index]) {
            continue;
        }
        ++result.referenced_object_count_;
        const PsxObject& object = archive.objects()[object_index];
        const std::size_t model_index = object.model_index;
        const PsxModel& model = archive.models()[model_index];
        for (std::size_t face_index = 0; face_index < model.faces.size(); ++face_index) {
            const PsxFace& source_face = model.faces[face_index];
            PsxCollisionFace face{};
            face.object_index = object_index;
            face.model_index = model_index;
            face.model_face_index = face_index;
            face.normal = model.normals[source_face.normal_index];
            face.face_flags = source_face.flags;
            face.surface_flags = source_face.surface_flags;
            face.raw_collision_word = source_face.raw_collision_word;
            face.vertex_count = (source_face.flags & 0x0010U) != 0 ? 3 : 4;
            const std::array<std::size_t, 4> corner_order =
                face.vertex_count == 3
                // FUN_00462a20 anchors its quantized plane equation at the
                // first cached triangle vertex. Preserve the source order
                // here: reversing the triangle makes the approximate stored
                // normal use a different anchor and shifts the Q14 hit on
                // sloped faces, even though the geometric triangle is the
                // same.
                ? std::array<std::size_t, 4>{0, 1, 2, 3}
                : std::array<std::size_t, 4>{0, 2, 3, 1};
            for (std::size_t corner = 0; corner < face.vertex_count; ++corner) {
                face.vertices[corner] = world_vertex(
                    object,
                    model.vertices[source_face.vertex_indices[corner_order[corner]]]);
            }
            object_faces[object_index].push_back(result.faces_.size());
            result.faces_.push_back(face);
        }
    }

    for (std::size_t blockmap_index = 0;
         blockmap_index < archive.blockmaps().size();
         ++blockmap_index) {
        const PsxBlockmap& source = archive.blockmaps()[blockmap_index];
        PsxCollisionGrid& grid = result.grids_[blockmap_index];
        for (std::size_t cell_index = 0; cell_index < source.cells.size(); ++cell_index) {
            PsxCollisionCell& cell = grid.cells[cell_index];
            for (const std::uint32_t object_index : source.cells[cell_index].object_indices) {
                for (const std::size_t face_index : object_faces[object_index]) {
                    if (std::find(cell.faces.begin(), cell.faces.end(), face_index)
                        == cell.faces.end()) {
                        cell.faces.push_back(face_index);
                    }
                }
            }
        }
    }
    return result;
}

std::optional<std::size_t> PsxCollisionWorld::cell_axis(
    std::int32_t value,
    std::int32_t minimum,
    std::int32_t maximum,
    std::uint16_t count) noexcept {
    if (count == 0 || value < minimum || value > maximum || maximum <= minimum) {
        return std::nullopt;
    }
    if (value == maximum) {
        return static_cast<std::size_t>(count - 1U);
    }
    const std::int64_t local = static_cast<std::int64_t>(value)
        - static_cast<std::int64_t>(minimum);
    const std::int64_t range = static_cast<std::int64_t>(maximum)
        - static_cast<std::int64_t>(minimum);
    const std::int64_t index = (local * static_cast<std::int64_t>(count)) / range;
    return static_cast<std::size_t>(std::clamp<std::int64_t>(
        index,
        0,
        static_cast<std::int64_t>(count) - 1));
}

std::vector<std::size_t> PsxCollisionWorld::candidate_faces(
    std::array<std::int32_t, 3> center,
    std::int32_t radius) const {
    if (radius < 0) {
        throw PsxFormatError("PSX collision query radius is negative");
    }
    const std::int64_t low_x = static_cast<std::int64_t>(center[0]) - radius;
    const std::int64_t high_x = static_cast<std::int64_t>(center[0]) + radius;
    const std::int64_t low_z = static_cast<std::int64_t>(center[2]) - radius;
    const std::int64_t high_z = static_cast<std::int64_t>(center[2]) + radius;
    std::vector<bool> seen(faces_.size(), false);
    std::vector<std::size_t> result;

    for (const PsxCollisionGrid& grid : grids_) {
        const std::int64_t minimum_x = grid.bounds[0];
        const std::int64_t minimum_z = grid.bounds[1];
        const std::int64_t maximum_x = grid.bounds[2];
        const std::int64_t maximum_z = grid.bounds[3];
        if (high_x < minimum_x || low_x > maximum_x
            || high_z < minimum_z || low_z > maximum_z) {
            continue;
        }
        const auto clamp_axis = [](std::int64_t value, std::int64_t minimum, std::int64_t maximum) {
            return std::clamp(value, minimum, maximum);
        };
        const std::int32_t clamped_low_x = static_cast<std::int32_t>(clamp_axis(low_x, minimum_x, maximum_x));
        const std::int32_t clamped_high_x = static_cast<std::int32_t>(clamp_axis(high_x, minimum_x, maximum_x));
        const std::int32_t clamped_low_z = static_cast<std::int32_t>(clamp_axis(low_z, minimum_z, maximum_z));
        const std::int32_t clamped_high_z = static_cast<std::int32_t>(clamp_axis(high_z, minimum_z, maximum_z));
        const auto first_x = cell_axis(clamped_low_x, grid.bounds[0], grid.bounds[2], grid.cell_counts[0]);
        const auto last_x = cell_axis(clamped_high_x, grid.bounds[0], grid.bounds[2], grid.cell_counts[0]);
        const auto first_z = cell_axis(clamped_low_z, grid.bounds[1], grid.bounds[3], grid.cell_counts[1]);
        const auto last_z = cell_axis(clamped_high_z, grid.bounds[1], grid.bounds[3], grid.cell_counts[1]);
        if (!first_x.has_value() || !last_x.has_value()
            || !first_z.has_value() || !last_z.has_value()) {
            continue;
        }
        for (std::size_t z = *first_z; z <= *last_z; ++z) {
            for (std::size_t x = *first_x; x <= *last_x; ++x) {
                const std::size_t cell_index = z * grid.cell_counts[0] + x;
                if (cell_index >= grid.cells.size()) {
                    continue;
                }
                for (const std::size_t face_index : grid.cells[cell_index].faces) {
                    if (!seen[face_index]) {
                        seen[face_index] = true;
                        result.push_back(face_index);
                    }
                }
            }
        }
    }
    return result;
}

bool PsxCollisionWorld::segment_triangle(
    const std::array<double, 3>& start,
    const std::array<double, 3>& direction,
    const std::array<double, 3>& a,
    const std::array<double, 3>& b,
    const std::array<double, 3>& c,
    double& fraction) noexcept {
    const std::array<double, 3> edge_1 = subtract(b, a);
    const std::array<double, 3> edge_2 = subtract(c, a);
    const std::array<double, 3> perpendicular = cross(direction, edge_2);
    const double determinant = dot(edge_1, perpendicular);
    if (std::abs(determinant) < kEpsilon) {
        return false;
    }
    const double inverse = 1.0 / determinant;
    const std::array<double, 3> origin = subtract(start, a);
    const double u = dot(origin, perpendicular) * inverse;
    if (u < -kEpsilon || u > 1.0 + kEpsilon) {
        return false;
    }
    const std::array<double, 3> cross_origin = cross(origin, edge_1);
    const double v = dot(direction, cross_origin) * inverse;
    if (v < -kEpsilon || u + v > 1.0 + kEpsilon) {
        return false;
    }
    const double candidate = dot(edge_2, cross_origin) * inverse;
    if (candidate < -kEpsilon || candidate > 1.0 + kEpsilon) {
        return false;
    }
    fraction = std::clamp(candidate, 0.0, 1.0);
    return true;
}

std::optional<PsxCollisionHit> PsxCollisionWorld::trace_segment(
    std::array<std::int32_t, 3> start,
    std::array<std::int32_t, 3> end,
    const PsxCollisionQueryOptions& options) const {
    const std::array<std::int64_t, 3> delta{
        static_cast<std::int64_t>(end[0]) - start[0],
        static_cast<std::int64_t>(end[1]) - start[1],
        static_cast<std::int64_t>(end[2]) - start[2],
    };
    const std::int64_t span = std::max(
        std::llabs(delta[0]),
        std::max(std::llabs(delta[1]), std::llabs(delta[2])));
    const std::array<std::int32_t, 3> center{
        static_cast<std::int32_t>(static_cast<std::int64_t>(start[0]) + delta[0] / 2),
        static_cast<std::int32_t>(static_cast<std::int64_t>(start[1]) + delta[1] / 2),
        static_cast<std::int32_t>(static_cast<std::int64_t>(start[2]) + delta[2] / 2),
    };
    const std::int32_t radius = span > std::numeric_limits<std::int32_t>::max()
        ? std::numeric_limits<std::int32_t>::max()
        : static_cast<std::int32_t>(span / 2 + 1);
    std::vector<std::size_t> candidates = candidate_faces(center, radius);
    if (grids_.empty()) {
        candidates.resize(faces_.size());
        for (std::size_t index = 0; index < faces_.size(); ++index) {
            candidates[index] = index;
        }
    }
    // FUN_00462a20 performs its plane/side tests after arithmetic SAR 12 on
    // both query endpoints and the object-relative face coordinates. The
    // winning parameter is then applied to the original fixed-point segment.
    const std::array<std::int32_t, 3> integer_start = to_integer_world(start);
    const std::array<std::int32_t, 3> integer_end = to_integer_world(end);
    const std::array<double, 3> integer_start_double = to_double(integer_start);
    const std::array<double, 3> direction = subtract(
        to_double(integer_end),
        integer_start_double);
    std::optional<PsxCollisionHit> best;
    for (const std::size_t face_index : candidates) {
        if (face_index >= faces_.size()) {
            continue;
        }
        const PsxCollisionFace& face = faces_[face_index];
        if (!accepts_retail_collision_face(face.raw_collision_word, options)) {
            continue;
        }
        if (options.apply_retail_plane_test
            && !crosses_retail_collision_plane(
                face,
                integer_start,
                integer_end)) {
            continue;
        }
        const auto test = [&](std::size_t first, std::size_t second, std::size_t third) {
            double fraction = 0.0;
            const auto integer_vertex = [&](std::size_t corner) {
                return to_integer_world(face.vertices[corner]);
            };
            if (!segment_triangle(
                integer_start_double,
                direction,
                    to_double(integer_vertex(first)),
                    to_double(integer_vertex(second)),
                to_double(integer_vertex(third)),
                fraction)) {
                return;
            }
            const auto plane_value = [&](
                const std::array<std::int32_t, 3>& point) {
                std::int64_t value = 0;
                for (std::size_t axis = 0; axis < point.size(); ++axis) {
                    value += static_cast<std::int64_t>(
                        point[axis] - integer_vertex(0)[axis])
                        * face.normal[axis];
                }
                return value;
            };
            const std::int64_t plane_start = plane_value(integer_start);
            const std::int64_t plane_end = plane_value(integer_end);
            if (plane_start == plane_end) {
                return;
            }
            // FUN_00462a20 publishes the q+8c parameter from the integer
            // plane crossing. The floating triangle test above remains the
            // face-side acceptance test, but its barycentric fraction is
            // not the retail hit parameter and can differ by several raw
            // units on a sloped landing face.
            const std::int64_t raw_parameter = trunc_div(
                plane_start * 0x4000,
                plane_start - plane_end);
            const std::uint32_t parameter = static_cast<std::uint32_t>(
                std::clamp<std::int64_t>(raw_parameter, 0, 0x4000));
            if (best.has_value() && parameter >= best->hit_parameter_q14) {
                return;
            }
            const std::array<std::int32_t, 3> point{
                static_cast<std::int32_t>(static_cast<std::int64_t>(start[0])
                    + trunc_div(delta[0] * parameter, 0x4000)),
                static_cast<std::int32_t>(static_cast<std::int64_t>(start[1])
                    + trunc_div(delta[1] * parameter, 0x4000)),
                static_cast<std::int32_t>(static_cast<std::int64_t>(start[2])
                    + trunc_div(delta[2] * parameter, 0x4000)),
            };
            PsxCollisionHit hit{
                face_index,
                face.object_index,
                face.model_index,
                face.model_face_index,
                parameter,
                static_cast<double>(parameter) / 0x4000,
                point,
                face.normal,
                face.face_flags,
                face.surface_flags,
            };
            hit.raw_collision_word = face.raw_collision_word;
            best = hit;
        };
        if (face.vertex_count == 3) {
            test(0, 1, 2);
        } else if (face.vertex_count == 4) {
            test(0, 1, 2);
            test(0, 2, 3);
        }
    }
    return best;
}

} // namespace opentony::assets
