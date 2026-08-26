#pragma once

// Backend-neutral frame boundary for the recovered camera contract.
//
// This is intentionally not a game loop and it does not update gameplay
// objects.  The gameplay/scene layer supplies the raw target and producer
// values, this adapter runs the camera before view preparation, and the
// backend calls present_camera_frame only at its actual present boundary.
// Keeping those stages explicit prevents a message-pump tick or a render
// preparation call from being mistaken for a displayed frame.

#include "src/camera/camera_system.hpp"

#include <cstdint>

namespace opentony::camera {

struct CameraFrameStateRaw {
    SimulationClockStateRaw simulation_clock{};
    CameraTimingStateRaw camera_timing{};
    CameraStateRaw camera{};

    // This serial is advanced only by present_camera_frame.  It is therefore
    // the native-side equivalent of the confirmed DirectDraw Flip boundary at
    // 0x004d0ca4, rather than the message-pump/timing callsite.
    std::uint64_t presented_frame_serial{};
};

struct CameraFrameInputRaw {
    // The clock and timing producer are deliberately separate: the retail
    // level loop samples a millisecond source before gameplay, while
    // 0x00468b30 updates the Q8 camera rate in render preparation for the next
    // camera update.
    Raw timestamp_ms{};
    bool timer_disabled{};
    bool simulation_paused{};
    bool timing_paused{};
    bool quarter_rate{};
    bool slow_rate{};
    bool progress_paused{};

    // These values are assembled by the gameplay/object layer.  They are
    // passed through without inventing a player, tripod, collision, or asset
    // ownership model in the camera adapter.
    CameraTargetRaw target{};
    CameraFollowInput follow_input{};
    Q16Vec3 look_target_offset{};
    CameraModeInputRaw mode_input{};
    CameraMode25ProducerInputRaw mode25_input{};
    CameraAlternateFollowInputRaw alternate_follow_input{};
    CameraViewportParameterControlRaw viewport_control{};
    CameraFramingInputControlRaw framing_control{};
    CameraSmoothingProducerInputRaw smoothing_producer{};

    // Viewport/projection values are supplied by the display/render layer.
    // The camera transform is overwritten from CameraFrameStateRaw so a
    // stale caller-side transform cannot silently desynchronise the handoff.
    CameraRenderPreparationInputRaw render_input{};

    // A paused/menu render can still present the current camera state.  The
    // normal level path leaves this enabled.
    bool update_camera{true};
    bool update_camera_timing{true};
};

struct CameraFrameStepRaw {
    SimulationClockStepRaw simulation_clock{};
    CameraViewportCommitRaw camera_commit{};
    CameraRenderPreparationRaw render_preparation{};
    CameraTimingStepRaw camera_timing{};
    bool camera_updated{};
    bool render_prepared{};
    bool presented{};
    std::uint64_t presented_frame_serial{};
};

// Runs the camera/render-preparation half of one level-loop iteration.  The
// caller must invoke present_camera_frame after scene submission and backend
// setup; that split models the observed ordering:
//
//   clock -> camera update -> world/view preparation -> scene submission
//       -> backend -> present
//
// The camera timing producer is intentionally advanced after camera update,
// preserving its observed one-update latency.
inline CameraFrameStepRaw advance_camera_frame(
    CameraFrameStateRaw& state,
    const CameraFrameInputRaw& input,
    const CameraUpdateHooks& configured_hooks = {}) {
    CameraFrameStepRaw step;
    step.simulation_clock = advance_simulation_clock_ms(
        state.simulation_clock,
        input.timestamp_ms,
        input.timer_disabled,
        input.simulation_paused);

    if (input.update_camera) {
        CameraUpdateHooks hooks = configured_hooks;
        hooks.smoothing_delta_q8 = state.camera_timing.simulation_delta_q8;
        step.camera_commit = update_camera(
            state.camera,
            input.target,
            input.follow_input,
            input.look_target_offset,
            hooks,
            input.mode_input,
            input.mode25_input,
            input.alternate_follow_input,
            input.viewport_control,
            input.framing_control,
            input.smoothing_producer);
        step.camera_updated = true;
    }

    CameraRenderPreparationInputRaw render_input = input.render_input;
    render_input.camera_transform = state.camera.current_transform;
    step.render_prepared = prepare_camera_render_state_q12(
        render_input, step.render_preparation);

    if (input.update_camera_timing) {
        step.camera_timing = advance_camera_timing(
            state.camera_timing,
            state.simulation_clock.simulation_time,
            input.timing_paused,
            input.quarter_rate,
            input.slow_rate,
            input.progress_paused);
    }
    return step;
}

// Marks the point at which the backend has actually displayed the prepared
// frame.  This is deliberately separate from advance_camera_frame: the
// renderer may submit no frame, retry a surface, or use a different backend.
// The caller should invoke this exactly once after successful scene/backend
// submission.
inline bool present_camera_frame(
    CameraFrameStateRaw& state,
    CameraFrameStepRaw& step) {
    if (!step.render_prepared || step.presented) {
        return false;
    }
    step.presented = true;
    step.presented_frame_serial = ++state.presented_frame_serial;
    return true;
}

} // namespace opentony::camera
