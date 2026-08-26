#pragma once

#include "physics_frame.hpp"

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
    std::optional<OlliePrePhysicsResult> ollie;
    OllieBookkeeping ollie_state{};
    PhysicsStateRequest state_request{};

    friend bool operator==(
        const PlayerReplaySnapshot&,
        const PlayerReplaySnapshot&) = default;
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
