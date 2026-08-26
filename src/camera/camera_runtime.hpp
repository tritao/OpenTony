#pragma once

#include "camera_system.hpp"

namespace opentony::camera {

// Complete value-only input to the recovered Camera_Update ordering. The
// gameplay/world owners fill the target and mode-specific records; the camera
// keeps the raw PE32 state and does not reach into TRG or PSX data.
struct CameraRuntimeUpdateInput final {
    CameraTargetRaw target{};
    CameraFollowInput follow{};
    Q16Vec3 look_target_offset{};
    CameraUpdateHooks hooks{};
    CameraModeInputRaw mode{};
    CameraMode25ProducerInputRaw mode25{};
    CameraAlternateFollowInputRaw alternate_follow{};
    CameraViewportParameterControlRaw viewport_control{};
    CameraFramingInputControlRaw framing_control{};
    CameraSmoothingProducerInputRaw smoothing_producer{};
};

// Owns the camera state between gameplay frames. This is the native handoff
// that was missing from the renderer-independent gameplay shell: callers can
// update the camera after GameplayFrame and pass the resulting commit plus
// raw state to a renderer without exposing retail pointers.
class CameraRuntime final {
public:
    CameraRuntime() noexcept { reset(); }

    void reset() noexcept {
        state_ = {};
        // The ordinary level camera enters the mode-1 follow table. The
        // anchor flags make the first supplied target establish its anchor;
        // subsequent updates use the recovered history recurrence.
        state_.mode = 1;
        state_.anchor_update_flag = 1;
        state_.tripod_anchor_flag = 1;
        has_commit_ = false;
        last_commit_ = {};
    }

    [[nodiscard]] CameraViewportCommitRaw update(
        const CameraRuntimeUpdateInput& input) noexcept {
        last_commit_ = update_camera(
            state_,
            input.target,
            input.follow,
            input.look_target_offset,
            input.hooks,
            input.mode,
            input.mode25,
            input.alternate_follow,
            input.viewport_control,
            input.framing_control,
            input.smoothing_producer);
        has_commit_ = true;
        return last_commit_;
    }

    [[nodiscard]] const CameraStateRaw& state() const noexcept {
        return state_;
    }
    [[nodiscard]] CameraStateRaw& state() noexcept { return state_; }
    [[nodiscard]] bool has_commit() const noexcept { return has_commit_; }
    [[nodiscard]] const CameraViewportCommitRaw& last_commit() const noexcept {
        return last_commit_;
    }

private:
    CameraStateRaw state_{};
    CameraViewportCommitRaw last_commit_{};
    bool has_commit_{};
};

} // namespace opentony::camera
