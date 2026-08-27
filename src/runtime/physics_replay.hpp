#pragma once

#include "physics_frame.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace opentony::runtime {

struct PlayerReplayInput {
    std::uint16_t action_mask{};
    std::int8_t horizontal_axis{};
    std::int8_t vertical_axis{};
    std::int32_t frame_scale_q8{0x100};
};

struct PlayerReplaySnapshot {
    std::uint16_t action_mask{};
    std::uint16_t effective_movement_mask{};
    FixedPosition position{};
    FixedPosition previous_position{};
    FixedPosition collision_response{};
    FixedPosition motion_correction{};
    FixedPosition air_motion{};
    Q12Matrix3 orientation{};
    RetailBasis retail_basis{};
    std::uint32_t restart_auxiliary{};
    std::uint16_t restart_auxiliary_word{};
    std::int32_t physics_state{};
    std::int32_t ground_update_state{};
    std::int32_t ground_physics_mode{};
    std::int32_t turn_accumulator{};
    bool dispatch_handled{};
    PositionCommitResult position_commit{};
    std::optional<PositionCollisionHit> collision_hit;
    std::optional<CollisionOrientationResult> collision_orientation;
    bool hit_normal_removed{};
    bool landed{};
    bool in_air_jump_hold_applied{};
    bool velocity_damped{};
    VelocityDampingResult velocity_damping{};
    std::optional<GroundBrakeResult> ground_brake;
    std::optional<GroundPhysicsResult> ground_physics;
    std::optional<OlliePrePhysicsResult> ollie;
    OllieBookkeeping ollie_state{};
    PhysicsStateRequest state_request{};

    friend bool operator==(
        const PlayerReplaySnapshot&,
        const PlayerReplaySnapshot&) = default;
};

// The level-event consumer resets both replay-input slots at countdown
// expiry. The replay stream itself is still a separate asset/service, so this
// owner records the slot-scoped reset requests without fabricating stream
// contents.
class PlayerReplayResetOwner final {
public:
    void reset() noexcept { reset_requests_ = {}; }

    void reset_slot(std::size_t slot) noexcept {
        if (slot >= reset_requests_.size()) {
            return;
        }
        ++reset_requests_[slot];
    }

    [[nodiscard]] std::size_t reset_requests(std::size_t slot) const noexcept {
        return slot < reset_requests_.size() ? reset_requests_[slot] : 0;
    }

    [[nodiscard]] std::size_t total_reset_requests() const noexcept {
        return reset_requests_[0] + reset_requests_[1];
    }

private:
    std::array<std::size_t, 2> reset_requests_{};
};

// Headless deterministic runner for native-vs-retail semantic traces. It
// records stable fields rather than pointers or allocator addresses and keeps
// input history in the same InputState instance for the whole replay.
class PlayerPhysicsReplay final {
public:
    [[nodiscard]] static std::vector<PlayerReplaySnapshot> run(
        PlayerState initial_player,
        const std::vector<PlayerReplayInput>& inputs,
        const PlayerPhysicsFrameHooks& hooks = {});
};

} // namespace opentony::runtime
