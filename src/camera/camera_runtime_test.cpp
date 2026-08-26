#include "camera_runtime.hpp"

#include <cassert>

int main() {
    using namespace opentony::camera;

    CameraRuntime runtime;
    bool rejected = false;
    try {
        (void)runtime.update({}, {});
    } catch (const CameraRuntimeError&) {
        rejected = true;
    }
    assert(rejected);

    CameraTargetRaw target{};
    target.position = {0x10000, 0, 0};
    target.follow_offset = {0, 0, -0x4000};
    target.tripod_state = 1;
    runtime.reset(target);
    assert(runtime.configured());
    assert(runtime.state().mode == 1);
    assert(runtime.state().anchor_target.x == 0x10000);

    const auto first = runtime.update(target, {});
    assert(runtime.state().update_tick == 1);
    assert(runtime.last_commit().current_transform.x == first.current_transform.x);
    assert(runtime.last_commit().current_transform.y == first.current_transform.y);
    assert(runtime.last_commit().current_transform.z == first.current_transform.z);
    assert(runtime.last_commit().current_transform.w == first.current_transform.w);
    assert(runtime.state().look_angles.third == 0);
    assert(runtime.last_commit().rendered_position.y == 0);
    ViewportInputRaw viewport{};
    viewport.words = {640, 480, 0, 0, 10, 0, 3413, 12, 320, 240, 0, 0, 320, 480};
    assert(runtime.prepare_viewport_projection(viewport, 20512, 3413, 3413));
    assert(runtime.viewport_projection().viewport.words[5] == 20512);
    assert(runtime.viewport_projection().viewport.words[8] == 320);
    assert(runtime.viewport_projection().viewport.words[9] == 240);

    CameraMode25ProducerInputRaw mode25{};
    mode25.tripod_present = true;
    mode25.tripod_follow_offset_y_raw = 0x65;
    mode25.tripod_physics_state = 1;
    const auto second = runtime.update(target, {}, {}, {}, {}, mode25);
    assert(runtime.state().mode == 25);
    assert(second.current_transform.w != 0);

    CameraRuntime point_runtime;
    point_runtime.reset(target, 23);
    CameraModeInputRaw point{};
    point.point_target_valid = true;
    point.point_target_position = {0x20000, 0, 0};
    point.point_start_valid = true;
    point.point_start_position = target.position;
    const auto point_commit = point_runtime.update(target, {}, {}, {}, point);
    assert(point_runtime.state().point_camera_tick == 1);
    assert(point_commit.rendered_position.x >= 0);
    return 0;
}
