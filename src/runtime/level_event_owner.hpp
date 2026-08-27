#pragma once

#include "physics_replay.hpp"
#include "player_state.hpp"
#include "../camera/camera_runtime.hpp"
#include "../trg/level_trigger_state.hpp"

#include <cstddef>
#include <cstdint>

namespace opentony::runtime {

// Gameplay-side owner for the side effects returned by the TRG level-event
// service. LevelTriggerState owns the recovered global state machine; this
// object owns the concrete player, replay-slot, and camera writes that the
// retail callers perform after 0x00469a30/0x00469de0.
class LevelEventGameplayOwner final {
public:
    LevelEventGameplayOwner(
        PlayerState& primary,
        camera::CameraRuntime& primary_camera,
        PlayerReplayResetOwner& replay_owner) noexcept
        : primary_(primary),
          primary_camera_(primary_camera),
          replay_owner_(replay_owner) {}

    void bind_secondary(
        PlayerState* secondary,
        camera::CameraRuntime* secondary_camera) noexcept {
        secondary_ = secondary;
        secondary_camera_ = secondary_camera;
    }

    void set_score_input_active(bool primary, bool secondary) noexcept {
        primary_score_input_active_ = primary;
        secondary_score_input_active_ = secondary;
    }

    // Build the player-owned input portion for LevelTriggerState without
    // assigning names to the raw skater offsets that remain unresolved.
    [[nodiscard]] trg::TriggerLevelEventFrameInput frame_input(
        const trg::TriggerLevelEventInputs& inputs,
        bool mode7_input_active) const noexcept;

    // Apply every concrete side effect represented by one level-event result.
    // The completion/menu helper remains outside this owner because its
    // retail destination is not proven by the current static evidence.
    void apply(const trg::TriggerLevelEventFrameResult& result) noexcept;

private:
    PlayerState& primary_;
    camera::CameraRuntime& primary_camera_;
    PlayerReplayResetOwner& replay_owner_;
    PlayerState* secondary_{};
    camera::CameraRuntime* secondary_camera_{};
    bool primary_score_input_active_{};
    bool secondary_score_input_active_{};
};

} // namespace opentony::runtime
