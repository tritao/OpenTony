#include "camera_runtime.hpp"

namespace opentony::camera {

void CameraRuntime::reset(const CameraTargetRaw& target, std::uint32_t mode) noexcept {
    state_ = CameraStateRaw{};
    state_.mode = mode;
    state_.anchor_update_flag = 1;
    state_.tripod_anchor_flag = 1;
    state_.mirrored_anchor = target.position;
    state_.anchor_target = target.position;
    state_.look_target = target.position;
    state_.viewport_parameter_raw = 0x100;
    last_commit_ = CameraViewportCommitRaw{};
    viewport_projection_ = ViewportProjectionRaw{};
    configured_ = true;
}

bool CameraRuntime::prepare_viewport_projection(
    ViewportInputRaw input,
    std::uint16_t state_selector,
    std::uint32_t scale_x,
    std::uint32_t scale_y) noexcept {
    if (!configured_) {
        return false;
    }
    return build_viewport_projection(
        input, state_selector, scale_x, scale_y, viewport_projection_);
}

CameraViewportCommitRaw CameraRuntime::update(
    const CameraTargetRaw& target,
    const CameraFollowInput& follow_input,
    const Q16Vec3& look_target_offset,
    const CameraUpdateHooks& hooks,
    const CameraModeInputRaw& mode_input,
    const CameraMode25ProducerInputRaw& mode25_input) {
    if (!configured_) {
        throw CameraRuntimeError("camera runtime updated before reset");
    }
    last_commit_ = update_camera(
        state_, target, follow_input, look_target_offset,
        hooks, mode_input, mode25_input);
    return last_commit_;
}

} // namespace opentony::camera
