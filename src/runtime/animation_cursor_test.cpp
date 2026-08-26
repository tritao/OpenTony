#include "animation_cursor.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>

namespace {

opentony::runtime::AnimationTableView test_table() {
    static constexpr auto counts = [] {
        std::array<std::uint8_t, 0x2f> result{};
        result[0] = 12;
        result[1] = 10;
        result[2] = 29;
        result[3] = 31;
        result[6] = 23;
        result[7] = 23;
        result[8] = 27;
        result[9] = 29;
        result[10] = 28;
        result[0x2e] = 17;
        return result;
    }();
    return {counts};
}

void test_approach_helper() {
    using opentony::runtime::approach_animation_frame;
    assert(approach_animation_frame(0, 22) == 5);
    assert(approach_animation_frame(0, 13) == 5);
    assert(approach_animation_frame(0, 12) == 3);
    assert(approach_animation_frame(0, 4) == 3);
    assert(approach_animation_frame(0, 3) == 1);
    assert(approach_animation_frame(19, 22) == 20);
    assert(approach_animation_frame(22, 0) == 17);
    assert(approach_animation_frame(2, 0) == 1);
}

void test_request_and_invalid_fallback() {
    using opentony::runtime::AnimationCursor;
    const auto table = test_table();
    AnimationCursor cursor;
    cursor.rate = 0x14000;

    const auto idle = cursor.request(table, 0);
    assert(idle.applied);
    assert(!idle.invalid_id);
    assert(cursor.id == 0);
    assert(cursor.frame_count == 12);
    assert(cursor.frame == 11);
    assert(cursor.endpoint == 11);
    assert(cursor.direction == 0);
    assert(cursor.finished);
    assert(cursor.rate == 0x14000);

    const auto turn = cursor.request(table, 6, -4, 80, -1);
    assert(turn.applied);
    assert(cursor.id == 6);
    assert(cursor.frame == 0);
    assert(cursor.endpoint == 22);
    assert(cursor.alternate_endpoint == -1);
    assert(cursor.direction == 1);
    assert(!cursor.finished);
    assert(cursor.fraction == 0);

    const auto invalid = cursor.request(table, 0xffff, 0, 0, -1);
    assert(invalid.applied);
    assert(invalid.invalid_id);
    assert(invalid.effective_id == 0x2e);
    assert(invalid.frame_count == 17);
    assert(cursor.id == 0x2e);

    const auto missing_fallback = cursor.request(
        opentony::runtime::AnimationTableView{
            std::span<const std::uint8_t>{},
        },
        0xffff);
    assert(!missing_fallback.applied);
}

void test_fixed_point_and_modes() {
    using opentony::runtime::AnimationCursor;
    using opentony::runtime::AnimationPlaybackMode;
    const auto table = test_table();
    AnimationCursor cursor;

    (void)cursor.request(table, 0, 0, 5, -1);
    assert(cursor.advance(0x100) == 0x00010005);
    assert(cursor.frame == 1 && cursor.fraction == 0);
    cursor.rate = 0x14000;
    assert(cursor.advance(0x100) == 0x00020005);
    assert(cursor.frame == 2 && cursor.fraction == 0x4000);

    (void)cursor.request(table, 0, 0, 5, -1);
    cursor.rate = 0x10000;
    assert(cursor.advance(0x80) == 0x00000005);
    assert(cursor.frame == 0 && cursor.fraction == 0x8000);

    (void)cursor.request(table, 0, 0, 5, 1);
    cursor.rate = 0x10000;
    for (int index = 0; index < 5; ++index) {
        (void)cursor.advance(0x100);
    }
    assert(cursor.frame == 5);
    (void)cursor.advance(0x100);
    assert(cursor.frame == 4);
    assert(cursor.endpoint == 1);
    assert(cursor.alternate_endpoint == 5);
    assert(cursor.direction == -1);

    (void)cursor.request(table, 0, 0, 2, -1);
    (void)cursor.advance(0x100);
    (void)cursor.advance(0x100);
    assert(!cursor.finished);
    (void)cursor.advance(0x100);
    assert(cursor.finished);
    assert(cursor.frame == 2);

    (void)cursor.request(table, 0, 0, 2, -2);
    cursor.rate = 0x18000;
    (void)cursor.advance(0x100);
    assert(cursor.frame == 1 && cursor.fraction == 0x8000);
    (void)cursor.advance(0x100);
    assert(cursor.frame == 2 && !cursor.finished);
    (void)cursor.advance(0x100);
    assert(cursor.frame == 2 && cursor.finished);

    (void)cursor.cycle(table, 0, 1);
    cursor.rate = 0x10000;
    for (int index = 0; index < 11; ++index) {
        (void)cursor.advance(0x100);
    }
    assert(cursor.frame == 11);
    (void)cursor.advance(0x100);
    assert(cursor.frame == 0);

    (void)cursor.cycle(table, 0, -1);
    (void)cursor.advance(0x100);
    assert(cursor.frame == 11);

    (void)cursor.request(table, 0, 0, 5, -1);
    cursor.mode = static_cast<std::uint8_t>(AnimationPlaybackMode::Hold);
    (void)cursor.advance(0x100);
    assert(cursor.frame == 0 && cursor.fraction == 0);

    (void)cursor.request(table, 0, 0, 2, -1);
    cursor.mode = static_cast<std::uint8_t>(AnimationPlaybackMode::Reverse);
    (void)cursor.advance(0x100);
    (void)cursor.advance(0x100);
    assert(cursor.frame == 2 && cursor.direction == -1);
    assert(cursor.endpoint == 0 && cursor.request_start == 2);
    (void)cursor.advance(0x100);
    assert(cursor.frame == 1);

    (void)cursor.request(table, 0, 0, 2, 1);
    cursor.rate = 0x10000;
    (void)cursor.advance(0x100);
    (void)cursor.advance(0x100);
    cursor.mode = static_cast<std::uint8_t>(AnimationPlaybackMode::Hold);
    (void)cursor.advance(0x180);
    assert(cursor.frame == 2);
    assert(cursor.endpoint == 1);
    assert(cursor.alternate_endpoint == 2);
    assert(cursor.direction == -1);
}

void test_clock_ping_pong() {
    using opentony::runtime::AnimationCursor;
    using opentony::runtime::AnimationPlaybackMode;
    AnimationCursor cursor;
    cursor.mode = static_cast<std::uint8_t>(
        AnimationPlaybackMode::ClockPingPong);
    cursor.rate = 0x10000;
    cursor.target_frame = 0;
    cursor.target_frame2 = 3;
    cursor.mode3_clock = 0;

    (void)cursor.advance(0x100, 0);
    assert(cursor.frame == 0);
    (void)cursor.advance(0x100, 2);
    assert(cursor.frame == 1);
    (void)cursor.advance(0x100, 4);
    assert(cursor.frame == 2);
    (void)cursor.advance(0x100, 6);
    assert(cursor.frame == 3);
    (void)cursor.advance(0x100, 8);
    assert(cursor.frame == 2);

    cursor.target_frame = 3;
    cursor.target_frame2 = 0;
    cursor.mode3_clock = 0;
    (void)cursor.advance(0x100, 2);
    assert(cursor.frame == 2);
    (void)cursor.advance(0x100, 4);
    assert(cursor.frame == 1);

    // UpdateFrame uses signed IDIV remainder semantics. A clock before the
    // captured origin therefore produces a negative phase instead of a
    // modulo-normalized phase.
    cursor.target_frame = 2;
    cursor.target_frame2 = 4;
    cursor.mode3_clock = 0;
    (void)cursor.advance(0x100, -2);
    assert(cursor.frame == 1);

    cursor.frame = 2;
    cursor.fraction = 0;
    const auto positive_cycle = cursor.advance(0x100, 8);
    assert(cursor.frame == 2 && positive_cycle == 1);

    cursor.target_frame = 4;
    cursor.target_frame2 = 2;
    cursor.frame = 4;
    cursor.fraction = 0;
    const auto negative_cycle = cursor.advance(0x100, 8);
    assert(cursor.frame == 4 && negative_cycle == 0x00000004);

    cursor.frame = 4;
    const auto large_negative_cycle = cursor.advance(0x100, 0x80000);
    assert(cursor.frame == 4 && large_negative_cycle == 0x00010004);

    // A same-frame ping-pong range leaves the common 16.16 accumulator alone.
    cursor.frame = 7;
    cursor.fraction = 0x8000;
    cursor.target_frame = 4;
    cursor.target_frame2 = 4;
    (void)cursor.advance(0x100, 100);
    assert(cursor.frame == 7 && cursor.fraction == 0x8000);
}

void test_frame_reached() {
    using opentony::runtime::AnimationCursor;
    AnimationCursor cursor;
    cursor.old_anim = 6;
    cursor.old_frame = 5;
    cursor.new_frame = 10;
    cursor.old_anim_dir = 1;
    assert(!cursor.frame_reached(7, 7));
    assert(!cursor.frame_reached(6, 4));
    assert(cursor.frame_reached(6, 5));
    assert(cursor.frame_reached(6, 10));

    cursor.old_anim_dir = -1;
    assert(cursor.frame_reached(6, 4));
    assert(cursor.frame_reached(6, 11));
    assert(!cursor.frame_reached(6, 7));

    cursor.old_frame = 10;
    cursor.new_frame = 5;
    cursor.old_anim_dir = 1;
    assert(cursor.frame_reached(6, 4));
    assert(cursor.frame_reached(6, 11));
    assert(!cursor.frame_reached(6, 7));

    cursor.old_anim_dir = -1;
    assert(cursor.frame_reached(6, 5));
    assert(cursor.frame_reached(6, 10));
    assert(!cursor.frame_reached(6, 4));
}

} // namespace

int main() {
    test_approach_helper();
    test_request_and_invalid_fallback();
    test_fixed_point_and_modes();
    test_clock_ping_pong();
    test_frame_reached();
    std::cout << "Animation cursor tests passed\n";
}
