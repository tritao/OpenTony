#pragma once

#include "camera_system.hpp"

#include <cstdint>
#include <stdexcept>

namespace opentony::camera {

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

class CameraRuntimeError final : public std::runtime_error {
public:
    explicit CameraRuntimeError(const char* message)
        : std::runtime_error(message) {}
};

// Value-owned runtime counterpart of the 0x674 camera object. The raw state
// remains semantic rather than ABI-packed, while update() preserves the
// recovered Camera_Update stage order and returns the viewport handoff used
// by the renderer.
class CameraRuntime final {
public:
    CameraRuntime() noexcept { reset(); }

    void reset() noexcept;
    void reset(const CameraTargetRaw& target, std::uint32_t mode = 1) noexcept;

    [[nodiscard]] bool configured() const noexcept { return configured_; }
    [[nodiscard]] bool has_commit() const noexcept { return has_commit_; }
    [[nodiscard]] const CameraStateRaw& state() const noexcept { return state_; }
    [[nodiscard]] CameraStateRaw& state() noexcept { return state_; }
    [[nodiscard]] const CameraViewportCommitRaw& last_commit() const noexcept {
        return last_commit_;
    }
    [[nodiscard]] bool prepare_viewport_projection(
        ViewportInputRaw input,
        std::uint16_t state_selector,
        std::uint32_t scale_x,
        std::uint32_t scale_y) noexcept;
    [[nodiscard]] const ViewportProjectionRaw& viewport_projection() const noexcept {
        return viewport_projection_;
    }

    [[nodiscard]] CameraViewportCommitRaw update(
        const CameraRuntimeUpdateInput& input) noexcept;
    [[nodiscard]] CameraViewportCommitRaw update(
        const CameraTargetRaw& target,
        const CameraFollowInput& follow_input,
        const Q16Vec3& look_target_offset = {},
        const CameraUpdateHooks& hooks = {},
        const CameraModeInputRaw& mode_input = {},
        const CameraMode25ProducerInputRaw& mode25_input = {});

private:
    CameraStateRaw state_{};
    CameraViewportCommitRaw last_commit_{};
    ViewportProjectionRaw viewport_projection_{};
    bool configured_{};
    bool has_commit_{};
};

} // namespace opentony::camera
