#include "camera_runtime.hpp"

#include <cassert>
#include <iostream>

namespace {

bool same_transform(const opentony::camera::TransformQ12& lhs,
                    const opentony::camera::TransformQ12& rhs) {
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z && lhs.w == rhs.w;
}

}  // namespace

int main() {
    opentony::camera::CameraRuntime camera;
    opentony::camera::CameraRuntimeUpdateInput input{};
    input.target.position = {0x1000, 0x2000, 0x3000};
    input.target.follow_offset = {0, -0x1000, 0};
    input.follow.tripod_state = 0;

    const auto commit = camera.update(input);
    assert(camera.state().mode == 1);
    assert(camera.state().update_tick == 1);
    assert(camera.has_commit());
    assert(same_transform(commit.current_transform, camera.state().current_transform));
    assert(commit.viewport_parameter_low_raw == 0);

    input.target.position.x += 0x1000;
    const auto second = camera.update(input);
    assert(camera.state().update_tick == 2);
    assert(same_transform(second.current_transform, camera.state().current_transform));

    std::cout << "camera runtime ok\n";
}
