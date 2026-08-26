#include "action_sequence.hpp"
#include "action_profile.hpp"
#include "physics_frame.hpp"
#include "player_state.hpp"
#include "tricks_bin.hpp"

#include <array>
#include "tests/test_check.hpp"
#include <cstdint>
#include <vector>

namespace {

void put_u16(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint16_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value & 0xffU);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8U);
}

void put_i16(std::vector<std::uint8_t>& bytes, std::size_t offset, std::int16_t value) {
    put_u16(bytes, offset, static_cast<std::uint16_t>(value));
}

} // namespace

int main() {
    using namespace opentony::runtime;

    RetailActionHistory history;
    CHECK(history.publish(1, true, 10));
    CHECK(history.publish(12, true, 10));
    CHECK(!history.publish(12, true, 11));
    const int newest = history.find_pressed(1, 0, 0, 1, false, 10);
    CHECK(newest == 1);
    CHECK(history.find_pressed(1, 1, 0, 1, false, 10) == 0);

    std::vector<std::uint8_t> table(16, 0);
    put_i16(table, 0, 2);
    put_i16(table, 2, 1);
    put_i16(table, 4, 12);
    put_i16(table, 6, 0x20);
    put_u16(table, 8, 10);
    put_i16(table, 10, 0); // terminator
    const auto parsed = read_action_sequence_record(table, 0);
    CHECK(parsed.has_value());
    CHECK(parsed->length == 2);
    CHECK(parsed->actions[0] == 1);
    CHECK(parsed->actions[1] == 12);
    CHECK(parsed->stream_relative == 0x20);
    CHECK(parsed->flags == 10);
    CHECK(!read_action_sequence_record(table, 100).has_value());

    const auto match = match_action_sequence(
        table,
        history,
        ActionSequenceMatcherInput{0, 10, 0, 1});
    CHECK(match.matched);
    CHECK(match.stream_relative == 0x20);
    CHECK(match.trigger_action == 12);
    CHECK(history.record(0).action == 0);
    CHECK(history.record(1).action == 0);

    std::vector<std::uint8_t> image(0x50, 0);
    for (std::size_t index = 0; index < 8; ++index) {
        put_i16(image, index * 2, static_cast<std::int16_t>(0x10 + index * 4));
    }
    image[0x20] = 0x07;
    const auto tricks = opentony::assets::TricksBinArchive::parse(image);

    PlayerState player;
    const ActionProfileState profile = map_action_profile(0x1010);
    player.publish_action_profile(profile, 10);
    const auto execution = player.run_action_sequences(
        tricks.view(),
        table,
        ActionSequenceMatcherInput{profile.selected_action, 10, 0, 1});
    CHECK(execution.match.matched);
    CHECK(execution.stream_resolved);
    CHECK(execution.stream_started);
    CHECK(execution.stream.completed);

    // The frame can use the shipped source table explicitly when the retail
    // heap-generated per-player table has not been synthesized yet.
    std::vector<std::uint8_t> source_image(0x60, 0);
    put_i16(source_image, 0x00, 0x10);
    put_i16(source_image, 0x02, 0x20);
    put_i16(source_image, 0x04, 0x28);
    put_i16(source_image, 0x06, 0x40);
    put_i16(source_image, 0x08, 0x50);
    put_i16(source_image, 0x0a, 0x30);
    put_i16(source_image, 0x0c, 0x38);
    put_i16(source_image, 0x0e, 0x3c);
    put_i16(source_image, 0x30, 2);
    put_i16(source_image, 0x32, 1);
    put_i16(source_image, 0x34, 12);
    put_i16(source_image, 0x36, 0x20);
    put_i16(source_image, 0x38, 0x2000);
    source_image[0x20] = 0x07;
    const auto source_archive =
        opentony::assets::TricksBinArchive::parse(source_image);
    const auto source_view = source_archive.view();

    PlayerState frame_player;
    InputState frame_input;
    frame_input.begin_frame(0x1010);
    PlayerPhysicsFrameHooks frame_hooks{};
    frame_hooks.apply_ground_turn = false;
    frame_hooks.integrate_position = false;
    frame_hooks.integrate_motion_correction = false;
    frame_hooks.action_sequence_source = ActionSequenceSource{
        &source_view,
        {},
        {},
        true,
    };
    const auto frame = PlayerPhysicsFrame::step(
        frame_player,
        frame_input,
        frame_hooks);
    CHECK(frame.action_sequence.has_value());
    CHECK(frame.action_sequence->match.matched);
    CHECK(frame.action_sequence->stream.completed);

    // FUN_00492ea0 keeps the stream active after a 0x2c queue barrier. The
    // next fixed frame drains the queued local motion first, then resumes at
    // the saved cursor rather than matching a new history record.
    std::vector<std::uint8_t> yielding_image(0x50, 0);
    for (std::size_t index = 0; index < 8; ++index) {
        put_i16(
            yielding_image,
            index * 2,
            static_cast<std::int16_t>(0x10 + index * 4));
    }
    yielding_image[0x20] = kSetQueuedMotionOpcode;
    put_i16(yielding_image, 0x21, 0);
    put_i16(yielding_image, 0x23, 1);
    put_i16(yielding_image, 0x25, 2);
    yielding_image[0x27] = kWaitQueuedMotionOpcode;
    yielding_image[0x28] = kEndActionStreamOpcode;
    const auto yielding_archive =
        opentony::assets::TricksBinArchive::parse(yielding_image);
    const auto yielding_view = yielding_archive.view();

    PlayerState yielding_player;
    PlayerPhysicsFrameHooks yielding_hooks{};
    yielding_hooks.apply_ground_turn = false;
    yielding_hooks.integrate_position = false;
    yielding_hooks.integrate_motion_correction = false;
    yielding_hooks.action_sequence_source = ActionSequenceSource{
        &yielding_view,
        table,
        ActionSequenceMatcherInput{0, 0, 0, 1},
        false,
    };
    InputState yielding_input;
    yielding_input.begin_frame(0x1010);
    const auto yielding_frame = PlayerPhysicsFrame::step(
        yielding_player,
        yielding_input,
        yielding_hooks);
    CHECK(yielding_frame.action_sequence.has_value());
    CHECK(yielding_frame.action_sequence->match.matched);
    CHECK(yielding_frame.action_sequence->stream.yielded);
    CHECK(yielding_frame.action_sequence->stream_active);
    CHECK(yielding_player.action_stream_active());
    // The archive view is a span beginning at image offset 0x20, so the
    // native cursor is the equivalent stream-local byte offset 7.
    CHECK(yielding_player.action_stream_cursor() == 7);
    CHECK(yielding_player.queued_motion().pending[0] == 1);

    const auto resumed_frame = PlayerPhysicsFrame::step(
        yielding_player,
        yielding_input,
        yielding_hooks);
    CHECK(resumed_frame.action_sequence.has_value());
    CHECK(!resumed_frame.action_sequence->match.matched);
    CHECK(resumed_frame.action_sequence->stream_resumed);
    CHECK(resumed_frame.action_sequence->stream.completed);
    CHECK(!resumed_frame.action_sequence->stream_active);
    CHECK(!yielding_player.action_stream_active());
    CHECK(yielding_player.queued_motion().pending[0] == 0);
    CHECK(resumed_frame.queued_motion.local_delta[0] == 1);
    return 0;
}
