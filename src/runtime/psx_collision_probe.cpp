#include "psx_collision_probe.hpp"

namespace opentony::runtime {

bool PsxPositionCollisionProbe::operator()(
    const FixedPosition& candidate) const {
    return hit(candidate).has_value();
}

std::optional<assets::PsxCollisionHit> PsxPositionCollisionProbe::hit(
    const FixedPosition& candidate) const {
    return world_.trace_segment(start_, candidate, options_);
}

std::optional<PositionCollisionHit> PsxPositionCollisionProbe::query(
    const FixedPosition& candidate) const {
    const std::optional<assets::PsxCollisionHit> result = hit(candidate);
    if (!result.has_value()) {
        return std::nullopt;
    }
    const assets::PsxCollisionMaskView mask = result->collision_mask();
    PositionCollisionHit mapped{
        result->face_index,
        result->object_index,
        result->model_index,
        result->model_face_index,
        result->hit_parameter_q14,
        result->position,
        FixedPosition{
            result->normal[0],
            result->normal[1],
            result->normal[2],
        },
        result->face_flags,
        result->surface_flags,
        result->raw_collision_word,
        mask.surface_bit_6,
        mask.surface_bit_7_clear,
        mask.surface_bit_8_clear,
        mask.raw_type_bits_9_12,
        mask.face_flag_80,
    };
    return mapped;
}

bool PsxScenePositionCollisionProbe::operator()(
    const FixedPosition& candidate) const {
    return query(candidate).has_value();
}

std::optional<PositionCollisionHit> PsxScenePositionCollisionProbe::query(
    const FixedPosition& candidate) const {
    const collision::PsxCollisionResult result = linked_objects_.empty()
        ? scene_.query_with_metadata(start_, candidate, 0, filter_)
        : scene_.query_with_linked_objects(
              start_, candidate, linked_objects_, 0, filter_);
    if (!result.hit()) {
        return std::nullopt;
    }
    const collision::reference::FaceFlagView flags = result.decoded_flags();
    const FixedPosition normal{
        result.query.hit_normal[0],
        result.query.hit_normal[1],
        result.query.hit_normal[2],
    };
    return PositionCollisionHit{
        result.face_index,
        result.object_index,
        result.query.hit_model_index,
        result.face_index,
        static_cast<std::uint32_t>(result.query.hit_parameter),
        result.query.hit_position,
        normal,
        result.base_flags,
        result.surface_flags,
        result.surface_word,
        flags.surface_large_polygon,
        (result.surface_flags & 0x0080u) == 0,
        flags.surface_skateable,
        static_cast<std::uint8_t>((result.surface_flags >> 9u) & 0x0fu),
        flags.face_bit_80,
    };
}

} // namespace opentony::runtime
