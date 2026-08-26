#include "gameplay_presentation.hpp"

namespace opentony::runtime {

camera::CameraTargetRaw make_camera_target(
    const PlayerState& player,
    camera::Q16Vec3 follow_offset) noexcept {
    const FixedPosition& position = player.position();
    return camera::CameraTargetRaw{
        {position[0], position[1], position[2]},
        follow_offset,
        {},
        0,
        0,
        false,
    };
}

GameplayPresentationSnapshot GameplayPresentation::snapshot() const {
    const GameplaySessionObservation observation = session_.observation();
    GameplayPresentationSnapshot result{};
    result.frame_index = observation.frame.frame_index;
    result.elapsed_ms = observation.frame.elapsed_ms;
    result.player.position = observation.position;
    result.player.previous_position = observation.previous_position;
    result.player.collision_response = observation.collision_response;
    result.player.orientation = observation.orientation;
    result.player.animation_state = session_.player().animation_state();
    result.player.animation_frame = session_.player().animation_frame();
    result.player.physics_state = observation.physics_state;
    result.player.script_skater_fields = observation.script_skater_fields;
    result.level = session_.render_snapshot();
    result.camera = camera_.state();
    result.has_camera_commit = camera_.has_commit();
    if (result.has_camera_commit) {
        result.camera_commit = camera_.last_commit();
    }
    return result;
}

} // namespace opentony::runtime
