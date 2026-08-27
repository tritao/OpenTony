#include "ground_animation.hpp"

#include "tests/test_check.hpp"
#include <array>
#include <iostream>

int main() {
    using opentony::runtime::GroundAnimationBranch;
    using opentony::runtime::GroundAnimationInput;
    using opentony::runtime::GroundAnimationRequestWrapper;
    using opentony::runtime::approach_animation_frame;
    using opentony::runtime::AnimationCursor;
    using opentony::runtime::AnimationTableView;
    using opentony::runtime::apply_ground_animation_request;
    using opentony::runtime::update_ground_animation;

    CHECK(approach_animation_frame(0, 22) == 5);
    CHECK(approach_animation_frame(10, 22) == 13);
    CHECK(approach_animation_frame(19, 22) == 20);
    CHECK(approach_animation_frame(21, 22) == 22);
    CHECK(approach_animation_frame(22, 0) == 17);

    GroundAnimationInput ground{};
    ground.turn_mirror = 0x2d000;
    const auto first = update_ground_animation(ground);
    CHECK(first.changed);
    CHECK(first.branch == GroundAnimationBranch::GroundTurn);
    CHECK(first.animation_state == 7);
    CHECK(first.target_frame == 22);
    CHECK(first.animation_frame == 5);
    CHECK(first.request.issued);
    CHECK(first.request.wrapper == GroundAnimationRequestWrapper::Range);
    CHECK(first.request.animation == 7);
    CHECK(first.request.start == 5);
    CHECK(first.request.end == 5);
    CHECK(first.request.alternate == -1);
    CHECK(first.request.resets_rate);
    CHECK(!first.request.completion_check);

    ground.animation_state = first.animation_state;
    ground.animation_frame = first.animation_frame;
    const auto second = update_ground_animation(ground);
    CHECK(second.animation_frame == 10);

    // The alternate positive branch increments the seated frame once before
    // entering the shared 5/3/1 approach helper.
    GroundAnimationInput alternate{};
    alternate.turn_mirror = 0x2d000;
    alternate.alternate_mode = true;
    const auto alternate_result = update_ground_animation(alternate);
    CHECK(alternate_result.animation_state == 6);
    CHECK(alternate_result.target_frame == 22);
    CHECK(alternate_result.animation_frame == 6);
    CHECK(alternate_result.request.start == 6);
    CHECK(alternate_result.request.end == 6);

    GroundAnimationInput wide{};
    wide.turn_mirror = 0x2d000;
    wide.wide_turn_profile = true;
    const auto wide_result = update_ground_animation(wide);
    CHECK(wide_result.target_frame == 11);

    GroundAnimationInput special{};
    special.turn_mirror = -0x2d000;
    special.blocked_or_special = true;
    special.animation_state = 6;
    const auto special_result = update_ground_animation(special);
    CHECK(special_result.branch == GroundAnimationBranch::SpecialTurn);
    CHECK(special_result.animation_state == 9);
    CHECK(special_result.target_frame == 15);
    CHECK(special_result.animation_frame == 15);
    CHECK(special_result.completed);
    CHECK(special_result.request.animation == 9);
    CHECK(special_result.request.start == 15);
    CHECK(special_result.request.end == 15);
    CHECK(special_result.request.completion_check);

    GroundAnimationInput special_other_source{};
    special_other_source.turn_mirror = 0x2d000;
    special_other_source.blocked_or_special = true;
    special_other_source.animation_state = 7;
    const auto special_other_result = update_ground_animation(
        special_other_source);
    CHECK(special_other_result.animation_state == 10);
    CHECK(special_other_result.animation_frame == 12);
    CHECK(special_other_result.completed);
    CHECK(special_other_result.request.completion_check);

    GroundAnimationInput special_easing{};
    special_easing.turn_mirror = -0x2d000;
    special_easing.blocked_or_special = true;
    special_easing.animation_state = 9;
    const auto special_easing_result = update_ground_animation(special_easing);
    CHECK(special_easing_result.animation_state == 9);
    CHECK(special_easing_result.animation_frame == 5);
    CHECK(!special_easing_result.request.completion_check);

    static constexpr std::array<std::uint8_t, 12> counts = [] {
        std::array<std::uint8_t, 12> result{};
        result[0] = 12;
        result[6] = 23;
        result[7] = 23;
        result[8] = 27;
        result[9] = 29;
        result[10] = 28;
        return result;
    }();

    GroundAnimationInput release_turn{};
    release_turn.animation_state = 6;
    release_turn.animation_frame = 7;
    const auto release_idle = update_ground_animation(release_turn);
    CHECK(release_idle.animation_state == 0);
    CHECK(release_idle.animation_frame == 0);
    CHECK(release_idle.request.wrapper == GroundAnimationRequestWrapper::Start);
    CHECK(release_idle.request.animation == 0);
    CHECK(release_idle.request.start == 0);
    CHECK(release_idle.request.end == -1);
    CHECK(release_idle.request.alternate == -1);
    AnimationCursor idle_cursor;
    idle_cursor.rate = 0x14000;
    const auto idle_request_result = apply_ground_animation_request(
        idle_cursor,
        AnimationTableView{counts},
        release_idle.request);
    CHECK(idle_request_result.applied);
    CHECK(idle_cursor.rate == 0x10000);
    CHECK(idle_cursor.id == 0);
    CHECK(idle_cursor.frame == 0);
    CHECK(idle_cursor.endpoint == 11);
    CHECK(idle_cursor.direction == 1);
    CHECK(!idle_cursor.finished);

    GroundAnimationInput release_crouch{};
    release_crouch.animation_state = 10;
    release_crouch.animation_frame = 12;
    const auto release_crouch_result = update_ground_animation(release_crouch);
    CHECK(release_crouch_result.animation_state == 8);
    CHECK(release_crouch_result.animation_frame == 0x13);
    CHECK(release_crouch_result.request.wrapper == GroundAnimationRequestWrapper::Full);
    CHECK(release_crouch_result.request.animation == 8);
    CHECK(release_crouch_result.request.start == 0x13);
    CHECK(release_crouch_result.request.end == 0x1a);
    CHECK(release_crouch_result.request.alternate == 0x13);

    GroundAnimationInput no_release{};
    no_release.animation_state = 0;
    no_release.animation_frame = 3;
    const auto no_release_result = update_ground_animation(no_release);
    CHECK(!no_release_result.request.issued);
    CHECK(no_release_result.animation_state == 0);
    CHECK(no_release_result.animation_frame == 3);

    // The request record is sufficient to cross the gameplay/playback seam:
    // the wrapper resets a previously accelerated rate before RunAnim applies
    // the exact endpoint fields.
    AnimationCursor cursor;
    cursor.rate = 0x14000;
    const auto request_result = apply_ground_animation_request(
        cursor,
        AnimationTableView{counts},
        release_crouch_result.request);
    CHECK(request_result.applied);
    CHECK(cursor.rate == 0x10000);
    CHECK(cursor.id == 8);
    CHECK(cursor.frame == 19);
    CHECK(cursor.endpoint == 26);
    CHECK(cursor.alternate_endpoint == 19);
    CHECK(cursor.direction == 1);
    CHECK(!cursor.finished);

    std::cout << "Ground animation tests passed\n";
}
