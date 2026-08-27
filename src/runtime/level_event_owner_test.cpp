#include "level_event_owner.hpp"

#include "../trg/gap_table.hpp"
#include "../trg/level_trigger_state.hpp"
#include "../trg/trg_runtime.hpp"
#include "tests/test_check.hpp"

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace {

void append_u16(std::vector<std::byte>& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<std::byte>(value & 0xffU));
    bytes.push_back(static_cast<std::byte>(value >> 8U));
}

void append_u32(std::vector<std::byte>& bytes, std::uint32_t value) {
    append_u16(bytes, static_cast<std::uint16_t>(value));
    append_u16(bytes, static_cast<std::uint16_t>(value >> 16U));
}

[[nodiscard]] std::vector<std::byte> event_trg_file() {
    std::vector<std::byte> bytes{
        std::byte{'_'}, std::byte{'T'}, std::byte{'R'}, std::byte{'G'},
    };
    append_u32(bytes, 2);
    append_u32(bytes, 2);
    append_u32(bytes, 20);
    append_u32(bytes, 32);

    // Type-6 command point with the operand-free 0x009e event command.
    append_u16(bytes, 6);
    append_u16(bytes, 0);
    append_u32(bytes, 0x12345678);
    append_u16(bytes, 0x009e);
    append_u16(bytes, 0xffff);
    append_u16(bytes, 0xff);
    append_u16(bytes, 0);
    return bytes;
}

} // namespace

int main() {
    using namespace opentony::runtime;
    using namespace opentony::trg;

    // Deterministic deferred-gap fixture: the player physics state selects
    // the raw pending slot, the level state completes the checklist record,
    // and the source pulse remains available to the command-point owner.
    const GapTable gap_table = GapTable::from_definitions({
        TriggerGapDefinition{0x0040, 0x1001, 500, "[GROUND]"},
        TriggerGapDefinition{0x0008, 0x1002, 750, "[STATE4]"},
    });
    LevelTriggerState gaps;
    gaps.set_gap_table(&gap_table);
    gaps.on_gap(7, 0x1111, 0x1001);
    CHECK(gaps.gaps()[0].deferred_field_3018);
    CHECK(!gaps.gaps()[0].deferred_field_3014);
    const auto ground = gaps.complete_deferred_gap_for_physics_state(0);
    CHECK(ground.has_value());
    CHECK(ground->slot == TriggerDeferredGapSlot::field_3018);
    CHECK(ground->source_node == 7);
    CHECK(ground->score == 500);
    CHECK(gaps.gaps()[0].completed);
    CHECK(!gaps.gaps()[0].deferred);
    CHECK(gaps.take_gap_pulse(0x1111, 0x1001));

    gaps.on_gap(8, 0x2222, 0x1002);
    CHECK(gaps.gaps()[1].deferred_field_3014);
    CHECK(!gaps.gaps()[1].deferred_field_3018);
    const auto state4 = gaps.complete_deferred_gap_for_physics_state(4);
    CHECK(state4.has_value());
    CHECK(state4->slot == TriggerDeferredGapSlot::field_3014);
    CHECK(state4->source_node == 8);
    CHECK(state4->score == 750);

    // Full level-event side-effect chain: 0x009e initialization -> eligible
    // frame result -> animation request, camera +0x5b4 write, replay-slot
    // resets, and +0x2a8 -> +0x16c score transfer.
    LevelTriggerState event_state;
    const TriggerLevelEventInputs event_inputs{
        0, 0, false, 0, 0};
    event_state.set_level_event_inputs(event_inputs);
    TrgFile event_file = TrgFile::parse(event_trg_file());
    TriggerRuntime event_runtime(std::move(event_file), event_state);
    event_runtime.build();
    event_runtime.pulse_node(0);
    CHECK(event_state.level_event_updates() == 1);

    PlayerState player;
    player.set_physics_state(7);
    player.set_level_event_field_2dd4(0);
    player.set_level_event_field_2a8(123);
    opentony::camera::CameraRuntime camera;
    camera.reset();
    PlayerReplayResetOwner replay;
    LevelEventGameplayOwner owner(player, camera, replay);
    owner.set_score_input_active(true, false);

    TriggerLevelEventFrameResult final_result{};
    for (std::size_t frame = 0; frame < 80; ++frame) {
        event_state.set_level_event_frame_input(
            owner.frame_input(event_state.level_event_inputs(), false));
        final_result = event_state.advance_level_event_frame();
        owner.apply(final_result);
    }

    CHECK(final_result.primary_animation_started);
    CHECK(final_result.primary_animation == 0x5d);
    CHECK(player.level_event_animation_requests() == 80);
    CHECK(player.last_level_event_animation() == 0x5d);
    CHECK(replay.reset_requests(0) == 1);
    CHECK(replay.reset_requests(1) == 1);
    CHECK(player.level_event_field_16c() == 123);
    CHECK(player.level_event_field_2a8() == 0);
    CHECK(event_state.level_event_timer_value() == 1);
    CHECK(event_state.level_event_mode_value() == 0);
    // Frames 41..79 are the 0x28-exclusive camera window: 39 * 0x40.
    CHECK(camera.state().follow_rotation_raw == 2496);

    // The level reset is the native script-object release boundary; the
    // object payload remains raw and no retail destructor is invented.
    event_state.on_script_object(3, 0xabcdef01, {4, 5, 6});
    event_state.reset();
    CHECK(event_state.script_objects().empty());
    CHECK(event_state.script_object_teardown_count() == 1);
    event_state.on_script_object(4, 0xabcdef02, {7, 8, 9});
    CHECK(event_state.teardown_script_objects() == 1);
    CHECK(event_state.script_objects().empty());
    CHECK(event_state.script_object_teardown_count() == 2);
}
