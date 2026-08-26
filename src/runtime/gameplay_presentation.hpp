#pragma once

#include "gameplay_session.hpp"
#include "../camera/camera_runtime.hpp"

namespace opentony::runtime {

// Renderer-facing copy of the player fields that are already established at
// the gameplay boundary. No animation asset or model lookup is implied here.
struct PlayerPresentationSnapshot final {
    FixedPosition position{};
    FixedPosition previous_position{};
    FixedPosition collision_response{};
    Q12Matrix3 orientation{};
    std::uint16_t animation_state{};
    std::int16_t animation_frame{};
    std::int32_t physics_state{};
    PlayerScriptSkaterFields script_skater_fields{};
};

struct GameplayPresentationSnapshot final {
    std::uint64_t frame_index{};
    std::uint32_t elapsed_ms{};
    PlayerPresentationSnapshot player{};
    trg::LevelRenderSnapshot level{};
    camera::CameraStateRaw camera{};
    camera::CameraViewportCommitRaw camera_commit{};
    bool has_camera_commit{};
};

// Camera target positions use the same raw world words as the retail player
// boundary. No host-unit conversion is performed until the scale boundary is
// independently promoted by the renderer evidence.
[[nodiscard]] camera::CameraTargetRaw make_camera_target(
    const PlayerState& player,
    camera::Q16Vec3 follow_offset = {}) noexcept;

// Joins one gameplay/session snapshot with one camera update. This preserves
// the frame-local ordering: gameplay first, camera preparation second, then
// renderer/backend consumption.
class GameplayPresentation final {
public:
    explicit GameplayPresentation(GameplaySession& session) noexcept
        : session_(session) {}

    [[nodiscard]] camera::CameraViewportCommitRaw update_camera(
        const camera::CameraRuntimeUpdateInput& input) noexcept {
        return camera_.update(input);
    }

    [[nodiscard]] GameplayPresentationSnapshot snapshot() const;

    [[nodiscard]] const camera::CameraRuntime& camera() const noexcept {
        return camera_;
    }
    [[nodiscard]] camera::CameraRuntime& camera() noexcept { return camera_; }

private:
    GameplaySession& session_;
    camera::CameraRuntime camera_{};
};

} // namespace opentony::runtime
