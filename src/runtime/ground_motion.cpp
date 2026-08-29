#include "ground_motion.hpp"

#include <bit>
#include <cstdint>

namespace opentony::runtime {
namespace {

[[nodiscard]] std::int32_t wrap32(std::int64_t value) noexcept {
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value));
}

[[nodiscard]] std::int32_t scaled_basis(
    std::int32_t basis,
    std::int32_t scale) noexcept {
    // The retail write is a 32-bit product. Preserve its low word before
    // exposing it through the native signed fixed-point boundary.
    return wrap32(-static_cast<std::int64_t>(basis) * scale);
}

[[nodiscard]] GroundMotionResult write_basis_correction(
    FixedPosition& motion_correction,
    const FixedPosition& basis,
    std::int32_t scale,
    GroundMotionBranch branch,
    std::int32_t response_speed_metric) noexcept {
    GroundMotionResult result{};
    for (std::size_t index = 0; index < motion_correction.size(); ++index) {
        motion_correction[index] = scaled_basis(basis[index], scale);
    }
    result.applied = true;
    result.scale = scale;
    result.branch = branch;
    result.response_speed_metric = response_speed_metric;
    return result;
}

[[nodiscard]] std::int32_t sample_rearm_threshold(
    std::int32_t random_roll,
    std::int32_t offset) noexcept {
    return static_cast<std::int32_t>(
        (static_cast<std::int64_t>(random_roll) + offset) * 0x2d000 /
        0x118);
}

void write_rearm(
    GroundMotionResult& result,
    std::int32_t random_roll,
    std::uint32_t event_reason) noexcept {
    result.cooldown_written = true;
    result.cooldown_value = 0x14;
    result.threshold_written = true;
    result.threshold_value = sample_rearm_threshold(random_roll, event_reason == 0x2570
        ? 0xaa
        : 0xdc);
    result.pending_animation_event_written = true;
    result.pending_animation_event = true;
    result.event_reason = event_reason;
    result.animation_speed = 0x14000;
}

void copy_side_effects(
    GroundMotionResult& destination,
    const GroundMotionResult& source) noexcept {
    destination.cooldown_written = source.cooldown_written;
    destination.cooldown_value = source.cooldown_value;
    destination.threshold_written = source.threshold_written;
    destination.threshold_value = source.threshold_value;
    destination.pending_animation_event_written =
        source.pending_animation_event_written;
    destination.pending_animation_event = source.pending_animation_event;
    destination.animation_event_written = source.animation_event_written;
    destination.animation_event_parameter = source.animation_event_parameter;
    destination.event_reason = source.event_reason;
    destination.animation_speed = source.animation_speed;
}

void apply_control_side_effects(
    GroundMotionResult& result,
    const GroundMotionInput& input) noexcept {
    if (!input.apply_control_side_effects) {
        return;
    }

    const auto write_cooldown = [&result]() noexcept {
        result.cooldown_written = true;
        result.cooldown_value = 0x14;
    };

    // The first B010 branch is entered only when the turn gate is open. A
    // clear cooldown either rearms the threshold/event state or writes the
    // plain 20-frame cooldown before the correction block.
    if (input.correction_gate_open && !input.cooldown_active &&
        input.ordinary_ground_state) {
        const std::int32_t half_threshold =
            (input.response_speed_threshold -
             (input.response_speed_threshold < 0 ? -1 : 0)) / 2;
        const bool surface_allows_rearm =
            input.surface_response_metric < -0x4cc ||
            input.response_speed_metric <= half_threshold;
        const bool animation_allows_rearm =
            input.animation_state != 0x5e &&
            (input.animation_state == 0 ||
             (input.animation_state >= 5 && input.animation_state <= 7));
        // The binary's fallback cooldown is the else side of the outer
        // surface/threshold gate. An ineligible animation (for example the
        // seated animation-14 pose on the landing boundary) leaves that
        // gate untouched; it does not itself consume a cooldown frame.
        if (surface_allows_rearm) {
            if (!input.blocked_or_special && animation_allows_rearm &&
                input.rearm_random_available) {
                write_rearm(result, input.rearm_random_roll, 0x2570);
            }
        } else {
            write_cooldown();
        }
    }

    // The zero-profile-table late path has a separate rearm sequence. It is
    // intentionally independent of the first branch's cooldown write: the
    // retail B010 block can request animation 1 while that cooldown remains
    // active.
    const bool late_rearm =
        !input.profile_table_value_nonzero && input.strong_profile &&
        input.surface_response_metric >= -0x4cc &&
        !input.blocked_or_special && input.ordinary_ground_state &&
        (input.animation_state == 0 ||
         (input.animation_state >= 5 && input.animation_state <= 7)) &&
        input.rearm_random_available;
    if (late_rearm) {
        write_rearm(result, input.rearm_random_roll, 0x25e5);
    }
}

} // namespace

GroundMotionProfileTable materialize_ground_motion_profile_table(
    const GroundMotionProfileRecords& records) noexcept {
    GroundMotionProfileTable table{};
    for (std::size_t index = 0; index < table.values.size(); ++index) {
        table.values[index] = records.primary_field_10[index] == 1 ? 1 : 0;
        table.secondary_values[index] =
            records.secondary_field_10[index] == 1 ? 1 : 0;
    }
    table.player_index = records.player_index;
    table.mode = records.mode;
    table.mode7_selector = records.mode7_selector;
    return table;
}

GroundMotionResult apply_ground_motion(
    FixedPosition& motion_correction,
    const RetailBasis& basis,
    const GroundMotionInput& input) noexcept {
    GroundMotionResult result{};
    result.response_speed_metric = input.response_speed_metric;
    if (input.physics_locked) {
        return result;
    }

    // FUN_0049b010 performs this side effect before checking the local
    // profile table and before the ordinary ground correction branches.
    // +0xf6 values 3 and 0x5e emit 0x2537 with parameter zero; value 1 emits
    // 0x2531 with parameter three and also writes +0x108.
    std::int16_t effective_animation_state = input.animation_state;
    if (input.animation_event_enabled) {
        // The retail animation service seats the completed animation-1 pose
        // on animation 3 at its terminal frame. B010 is entered once more
        // with the old ID at that boundary; its later correction branch then
        // observes the new ID in the same call.
        if (input.animation_state == 1 && input.animation_finished) {
            result.animation_event_written = true;
            result.animation_event_parameter = 3;
            result.event_reason = 0x2531;
            result.animation_speed = 0x14000;
            effective_animation_state = 3;
        } else if (input.animation_finished
                   && (input.animation_state == 3
                       || input.animation_state == 0x5e)) {
            result.animation_event_written = true;
            result.animation_event_parameter = 0;
            result.event_reason = 0x2537;
            // The event is observed before the same B010 call reaches its
            // ordinary correction branch.  Model the newly selected idle
            // state locally; the animation service callback publishes it to
            // PlayerState after this result is returned.
            effective_animation_state = 0;
        }
    }

    if (!input.producer_enabled) {
        return result;
    }

    // At 0x0049b131 retail takes the later strong-profile branch only when
    // both the configured +0x2ccc+0x10 slot and the local table value are
    // zero. That branch cannot produce a correction without strong_profile,
    // so the whole producer is inactive in that case.
    const bool profile_branch_enabled =
        input.strong_profile || input.profile_table_value_nonzero;
    if (!profile_branch_enabled) {
        return result;
    }

    apply_control_side_effects(result, input);

    // The pending animation marker is consumed in the state-2/3 frame
    // window before the transient correction is written.
    if ((effective_animation_state == 2 || effective_animation_state == 3) &&
        input.animation_frame > 10 && input.animation_frame < 16 &&
        input.pending_animation_event) {
        result.pending_animation_event_written = true;
        result.pending_animation_event = false;
        result.event_reason = 0x22;
    }

    const std::int32_t effective_threshold = result.threshold_written
        ? result.threshold_value
        : input.response_speed_threshold;
    const bool below_threshold =
        input.response_speed_metric < effective_threshold;

    // This is the state-0 section of FUN_0049b010. The decompilation writes
    // the correction directly, so a caller should clear the transient field
    // once at frame start just as FUN_0049e680 does.
    if (input.ordinary_ground_state && input.correction_gate_open) {
        if ((effective_animation_state == 2 || effective_animation_state == 3) &&
            input.animation_frame > 10 && input.animation_frame < 0x10 &&
            below_threshold) {
            const std::int32_t scale = input.strong_profile ? 8 : 4;
            GroundMotionResult correction = write_basis_correction(
                motion_correction,
                basis.at_30f4,
                scale,
                GroundMotionBranch::Animation2Or3,
                input.response_speed_metric);
            copy_side_effects(correction, result);
            return correction;
        }

        if (effective_animation_state == 0x5e &&
            input.animation_frame > 0xf && input.animation_frame < 0x14 &&
            below_threshold) {
            GroundMotionResult correction = write_basis_correction(
                motion_correction,
                basis.at_30f4,
                8,
                GroundMotionBranch::Animation5e,
                input.response_speed_metric);
            copy_side_effects(correction, result);
            return correction;
        }

        // 0x0049b2c1: the uphill gate only suppresses the ordinary write
        // while the metric is at or below 0x4e20. Once the metric is above
        // that value, retail takes the write regardless of basis Y, subject
        // to the later threshold check.
        if (effective_animation_state != 2 &&
            effective_animation_state != 3 &&
            effective_animation_state != 0x5e &&
            (input.response_speed_metric > 0x4e20 ||
             input.forward_basis_y < 0x1f4) &&
            below_threshold) {
            GroundMotionResult correction = write_basis_correction(
                motion_correction,
                basis.at_30f4,
                1,
                GroundMotionBranch::Ordinary,
                input.response_speed_metric);
            copy_side_effects(correction, result);
            return correction;
        }
    }

    // The later profile branch is the path reached after the local table
    // value is zero. It is outside the explicit state-0 block in the binary,
    // but B010 has already returned unless the dispatcher state is zero.
    if (input.ordinary_ground_state && !input.profile_table_value_nonzero &&
        input.strong_profile && below_threshold &&
        (effective_animation_state == 2 || effective_animation_state == 3) &&
        input.animation_frame > 10 &&
        input.animation_frame < 0x10) {
        GroundMotionResult correction = write_basis_correction(
            motion_correction,
            basis.at_30f4,
            8,
            GroundMotionBranch::Animation2Or3,
            input.response_speed_metric);
        copy_side_effects(correction, result);
        return correction;
    }

    return result;
}

} // namespace opentony::runtime
