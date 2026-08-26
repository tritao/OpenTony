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

} // namespace opentony::runtime
