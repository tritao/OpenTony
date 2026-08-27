#include "level_trigger_state.hpp"
#include "level_scene_registry.hpp"

#include <array>
#include "tests/test_check.hpp"
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

using namespace opentony::trg;

namespace {

void u16(std::vector<std::byte>& out, std::uint16_t value) {
    out.push_back(static_cast<std::byte>(value & 0xff));
    out.push_back(static_cast<std::byte>((value >> 8) & 0xff));
}

void u32(std::vector<std::byte>& out, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8) {
        out.push_back(static_cast<std::byte>((value >> shift) & 0xff));
    }
}

void u32_at(std::vector<std::byte>& out, std::size_t offset, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8) {
        out[offset + shift / 8] = static_cast<std::byte>((value >> shift) & 0xff);
    }
}

std::vector<std::byte> make_file(const std::vector<std::vector<std::byte>>& nodes) {
    const std::size_t first = 12 + nodes.size() * 4;
    std::vector<std::byte> result{
        std::byte{'_'}, std::byte{'T'}, std::byte{'R'}, std::byte{'G'},
    };
    u32(result, 2);
    u32(result, static_cast<std::uint32_t>(nodes.size()));
    std::size_t offset = first;
    for (const auto& node : nodes) {
        u32(result, static_cast<std::uint32_t>(offset));
        offset += node.size();
    }
    for (const auto& node : nodes) {
        result.insert(result.end(), node.begin(), node.end());
    }
    return result;
}

std::vector<std::byte> type6_node(
    std::span<const std::uint16_t> links,
    std::uint32_t checksum,
    std::span<const std::byte> stream) {
    std::vector<std::byte> node;
    u16(node, 6);
    u16(node, static_cast<std::uint16_t>(links.size()));
    for (const std::uint16_t link : links) {
        u16(node, link);
    }
    // The test file begins at an even offset. This is the same conditional
    // two-byte padding used by the retail loader.
    if (((20 + node.size()) & 2) != 0) {
        u16(node, 0);
    }
    u32(node, checksum);
    node.insert(node.end(), stream.begin(), stream.end());
    return node;
}

std::vector<std::byte> synthetic_psx() {
    std::vector<std::byte> result;
    u16(result, 4);
    u16(result, 2);
    u32(result, 0);
    u32(result, 2);
    const auto object = [&result](std::uint16_t model, std::int32_t x) {
        u32(result, 0);
        u32(result, static_cast<std::uint32_t>(x));
        u32(result, 20);
        u32(result, 30);
        u32(result, 0);
        u16(result, 0);
        u16(result, model);
        u16(result, 0);
        u16(result, 0);
        u32(result, 0);
        u32(result, 0);
    };
    object(0, 10);
    object(1, 40);
    u32(result, 2);
    u32(result, 96);
    u32(result, 124);
    result.insert(result.end(), 56, std::byte{0});
    const std::size_t tag_offset = result.size();
    u32_at(result, 4, static_cast<std::uint32_t>(tag_offset));
    u32(result, 0xffffffffU);
    u32(result, 0x1111U);
    u32(result, 0x2222U);
    u32(result, 0);
    u32(result, 0);
    u32(result, 0);
    u32(result, 0);
    return result;
}

void test_stateful_dispatch() {
    std::vector<std::byte> stream;
    u16(stream, 0x000d);
    u16(stream, 1);
    u16(stream, 0x0084);
    u16(stream, 7);
    u16(stream, 0x0086);
    u16(stream, 1);
    u16(stream, 0x0003);
    u16(stream, 0xffff);
    const std::array<std::uint16_t, 1> links{1};

    std::vector<std::byte> special;
    u16(special, 12);
    u16(special, 0);
    u32(special, 0x12345678);
    u16(special, 0xffff);

    const std::vector<std::byte> bytes = make_file({
        type6_node(links, 0xabcdef01, stream),
        special,
        {std::byte{0xff}, std::byte{0}},
    });

    LevelTriggerState state;
    TriggerRuntime runtime(TrgFile::parse(bytes), state);
    runtime.build();
    state.set_object_identifier(1, 7);
    runtime.pulse_node(0);

    const TriggerObjectState* target = state.object(1);
    CHECK(target != nullptr);
    CHECK(target->node_type == 12);
    CHECK(target->link_key == 0x12345678);
    CHECK(target->visible_commanded);
    CHECK((target->flags & 1U) != 0);
    CHECK(target->pulses == 1);
    CHECK(state.events().size() == 3);
}

void test_counted_link_commands_update_only_inline_targets() {
    std::vector<std::byte> stream;
    u16(stream, 0x0004);
    u16(stream, 1);
    u16(stream, 1);
    u16(stream, 0x000a);
    u16(stream, 1);
    u16(stream, 1);
    u16(stream, 0x0005);
    u16(stream, 1);
    u16(stream, 1);
    u16(stream, 0xffff);

    std::vector<std::byte> object;
    u16(object, 1);
    u16(object, 0);
    u16(object, 0);
    u16(object, 0);
    object.push_back(std::byte{2});
    object.push_back(std::byte{4});
    object.push_back(std::byte{0xff});
    object.insert(object.end(), 24, std::byte{0});

    LevelTriggerState state;
    TriggerRuntime runtime(TrgFile::parse(make_file({
        // There are no serialized type-6 links. Every target below must
        // come from the command payload itself.
        type6_node({}, 0, stream),
        object,
        {std::byte{0xff}, std::byte{0}},
    })), state);
    runtime.build();
    runtime.pulse_node(0);

    const TriggerObjectState* target = state.object(1);
    CHECK(target != nullptr);
    CHECK(target->signals == 1);
    CHECK(target->suspend_activate_calls == 2);
    CHECK(!target->suspended);
    CHECK(target->active);
    CHECK(state.events().size() == 3);
    CHECK(state.events()[0].target_node == 1);
    CHECK(state.events()[1].target_node == 1);
    CHECK(state.events()[2].target_node == 1);
}

void test_retail_link_target_filters() {
    LevelTriggerState state;
    state.on_spawn_node(1, 1, 0xcb, {0, 0, 0}, {});
    CHECK(state.object(1)->flags == 0x0041);
    CHECK(state.object(1)->factory_allocation_size == 0x1f4);
    CHECK(state.object(1)->factory_vtable == 0x005183b0);
    CHECK(state.object(1)->factory_list == TriggerFactoryList::CommonObject);
    CHECK(state.object(1)->factory_initial_activation_byte == 1);
    state.on_spawn_node(2, 5, 0, {0, 0, 0}, {});
    state.on_linked_node(3, 2, 0x1234, {});
    state.on_special_node(4, 12, {});

    const std::array<std::uint16_t, 4> links{1, 2, 3, 4};
    state.on_signal(0, links);
    state.on_suspend_activate(0, 4, links);
    state.on_visible(0, 1, links);

    // Retail signal/suspend only traverse the type-1/type-7 object lists.
    CHECK(state.object(1)->signals == 1);
    CHECK(state.object(1)->suspend_activate_calls == 1);
    CHECK(state.object(2)->signals == 0);
    CHECK(state.object(3)->suspend_activate_calls == 0);
    // SendVisible resolves crates, pickups, type-12 and type-14 links, not
    // the type-1 baddy object.
    CHECK(!state.object(1)->visible_commanded);
    CHECK(state.object(2)->visible_commanded);
    CHECK(state.object(3)->visible_commanded);
    CHECK(state.object(4)->visible_commanded);
}

void test_script_object_state() {
    LevelTriggerState state;
    state.on_script_object(27, 0xf6e32d0e, {1, 2, 3});
    CHECK(state.script_objects().size() == 1);
    CHECK(state.script_objects()[0].source_node == 27);
    CHECK(state.script_objects()[0].script_key == 0xf6e32d0e);
    const std::array<std::uint16_t, 3> expected_parameters{1, 2, 3};
    CHECK(state.script_objects()[0].parameters == expected_parameters);
    CHECK(state.script_objects()[0].identifier == 0);
    CHECK(state.script_objects()[0].raw_record().size() == 0xcc);
    CHECK(state.script_objects()[0].raw_record()[0xc8] == std::byte{0});
    CHECK(state.script_objects()[0].raw_record()[0xc9] == std::byte{0});

    // 0x83/0x84 search the same retail object-list identity space.
    state.on_object_flag_by_id(0, true);
    CHECK((state.script_objects()[0].flags & 1U) != 0);
    CHECK(state.script_objects()[0].raw_record()[0x04] == std::byte{1});
    state.on_object_flag_by_id(0, false);
    CHECK((state.script_objects()[0].flags & 1U) == 0);
    CHECK(state.script_objects()[0].raw_record()[0x04] == std::byte{0});
}

void test_type10_type11_pulse_and_kill_state() {
    LevelTriggerState state;
    const std::array<std::int32_t, 3> position{10, 20, 30};
    state.on_special_node(4, 10, {});
    state.on_special_node_state(4, 10, 0x0042, position);
    const TriggerObjectState* object = state.object(4);
    CHECK(object != nullptr);
    CHECK(object->has_trigger_runtime);
    CHECK(object->trigger_flags == 0x0042);
    CHECK(object->trigger_mode == 2);
    CHECK(object->trigger_state == 0);
    CHECK(object->special_runtime_allocation_size == 0x28);
    CHECK(object->special_runtime_vtable == 0x005196a4);
    CHECK(object->special_runtime_list == TriggerSpecialRuntimeList::Type10Type11);
    CHECK(!object->active);
    CHECK(object->position == position);
    CHECK(object->trigger_bounds.valid);
    CHECK(object->trigger_bounds.minimum == position);
    CHECK(object->trigger_bounds.maximum == position);

    state.on_node_pulse(4);
    CHECK(state.object(4)->trigger_state == 1);
    CHECK(state.object(4)->active);
    const std::array<std::uint16_t, 1> links{4};
    state.on_kill(0, 0x000c, links);
    CHECK(state.object(4)->trigger_state == 0);
    CHECK(!state.object(4)->active);
    CHECK(state.object(4)->alive);
    CHECK(!state.object(4)->killed);
}

void test_type10_type11_alias_bounds() {
    std::vector<std::byte> source;
    u16(source, 10);
    u16(source, 1);
    u16(source, 1);
    u16(source, 0);
    u32(source, 1);
    u32(source, 2);
    u32(source, 3);
    u16(source, 0);

    std::vector<std::byte> alias;
    u16(alias, 11);
    u16(alias, 0);
    // With the source node above ending at absolute offset 46, the
    // type-10/type-11 position rule aligns this node's coordinates to 52.
    alias.insert(alias.end(), 2, std::byte{0});
    u32(alias, static_cast<std::uint32_t>(-4));
    u32(alias, static_cast<std::uint32_t>(-5));
    u32(alias, static_cast<std::uint32_t>(-6));
    u16(alias, 0);

    LevelTriggerState state;
    TriggerRuntime runtime(TrgFile::parse(make_file({
        source,
        alias,
        {std::byte{0xff}, std::byte{0}},
    })), state);
    runtime.build();

    const TriggerObjectState* object = state.object(0);
    CHECK(object != nullptr);
    CHECK(object->trigger_links == std::vector<std::uint16_t>{1});
    CHECK(object->trigger_alias_nodes == std::vector<std::size_t>{1});
    CHECK(object->has_trigger_alias_group);
    CHECK(object->trigger_alias_group == 0);
    CHECK(state.object(1)->has_trigger_alias_group);
    CHECK(state.object(1)->trigger_alias_group == 1);
    const std::array<std::int32_t, 3> expected_min{
        -4 * 0x1000,
        -5 * 0x1000,
        -6 * 0x1000,
    };
    const std::array<std::int32_t, 3> expected_max{
        1 << 12,
        2 << 12,
        3 << 12,
    };
    CHECK(object->trigger_bounds.minimum == expected_min);
    CHECK(object->trigger_bounds.maximum == expected_max);
}

void test_type12_type14_runtime_activation() {
    LevelTriggerState state;
    state.set_special_runtime_context(2, 0x13579bdf);
    state.on_linked_node(8, 12, 0xfeedcafe, {});
    const TriggerObjectState* object = state.object(8);
    CHECK(object != nullptr);
    CHECK(object->has_special_runtime);
    CHECK(!object->special_runtime_active);
    CHECK(object->special_runtime_allocation_size == 0x18);
    CHECK(object->special_runtime_vtable == 0x0051982c);
    CHECK(object->special_runtime_list == TriggerSpecialRuntimeList::Type12Type14);
    CHECK(object->link_key == 0xfeedcafe);
    CHECK(object->raw_special_record().size() == 0x18);
    CHECK(object->raw_special_record()[0x04] == std::byte{0xfe});
    CHECK(object->raw_special_record()[0x05] == std::byte{0xca});
    CHECK(object->raw_special_record()[0x08] == std::byte{8});
    CHECK(object->raw_special_record()[0x0a] == std::byte{0});
    state.on_node_pulse(8);
    CHECK(state.object(8)->special_runtime_active);
    CHECK(state.object(8)->has_special_runtime_context);
    CHECK(state.object(8)->special_runtime_owner == 2);
    CHECK(state.object(8)->raw_special_record()[0x0a] == std::byte{1});
    CHECK(state.object(8)->raw_special_record()[0x0b] == std::byte{2});
    CHECK(state.object(8)->special_runtime_control == 0x13579bdf);
    CHECK(state.object(8)->has_special_asset_state);
    CHECK(state.object(8)->special_asset_flags_or == 4);
    CHECK(state.object(8)->special_asset_marker == 0x202020);
    CHECK(state.object(8)->active);
}

void test_type12_type14_runtime_links() {
    LevelTriggerState state;
    state.on_linked_node(8, 12, 0xfeedcafe, {});
    const std::array<std::uint16_t, 3> links{12, 14, 21};
    state.on_special_runtime_links(8, 12, links);
    CHECK(state.object(8)->special_runtime_links
        == std::vector<std::uint16_t>({12, 14, 21}));
}

void test_type12_type14_runtime_link_policy() {
    std::vector<std::byte> special_root;
    u16(special_root, 12);
    u16(special_root, 1);
    u16(special_root, 1);
    u16(special_root, 0);
    u32(special_root, 0x11111111);
    u16(special_root, 0xffff);

    std::vector<std::byte> special_target;
    u16(special_target, 12);
    u16(special_target, 1);
    u16(special_target, 2);
    u32(special_target, 0x22222222);
    u16(special_target, 0xffff);

    std::vector<std::byte> object;
    u16(object, 1);
    u16(object, 0);
    u16(object, 0);
    u16(object, 0);
    object.push_back(std::byte{2});
    object.push_back(std::byte{4});
    object.push_back(std::byte{0xff});
    object.insert(object.end(), 24, std::byte{0});

    LevelTriggerState state;
    TriggerRuntime runtime(TrgFile::parse(make_file({
        special_root,
        special_target,
        object,
        {std::byte{0xff}, std::byte{0}},
    })), state);
    runtime.build();

    runtime.pulse_node(0);
    CHECK(state.object(0)->pulses == 1);
    CHECK(state.object(1)->pulses == 0);
    CHECK(state.object(2)->pulses == 0);

    state.set_special_runtime_game_mode(8);
    runtime.pulse_node(0);
    CHECK(state.object(0)->pulses == 2);
    // The root sends the direct special target through its activation helper;
    // the guard prevents that target's own links from recursively traversing.
    CHECK(state.object(1)->pulses == 1);
    CHECK(state.object(2)->pulses == 0);
}

void test_type12_type14_animation_modes() {
    const opentony::assets::PsxArchive archive =
        opentony::assets::PsxArchive::parse(synthetic_psx(), "animation.psx");

    LevelTriggerState palette_state;
    palette_state.on_linked_node(8, 12, 0x1111, {});
    palette_state.bind_psx_models(archive);
    CHECK(palette_state.object(8)->has_asset_scene_position);
    palette_state.set_special_runtime_palette_mask(8, 0x000000ffU);
    palette_state.set_special_runtime_animation_mode(
        TriggerSpecialAnimationMode::PaletteTriangle);
    palette_state.on_node_pulse(8);
    palette_state.advance_time(1000);
    CHECK(palette_state.special_runtime_clock() == 60);
    // x=10 raw PSX units floor to zero Q12 units; clock=60 produces phase 40.
    // The low byte is preserved by the
    // supplied 0xff mask, matching FUN_004bdd00's masked asset write.
    CHECK(palette_state.object(8)->special_asset_marker == 0x00282820U);

    LevelTriggerState compact_state;
    compact_state.on_linked_node(8, 12, 0x1111, {});
    compact_state.bind_psx_models(archive);
    compact_state.set_special_runtime_context(1, 2000);
    compact_state.set_special_runtime_animation_mode(
        TriggerSpecialAnimationMode::CompactTriangle);
    compact_state.on_node_pulse(8);
    compact_state.advance_time(1000);
    // control=2000 gives span=64, phase=20 at clock=60, then owner 1 writes
    // the compact value into the asset's high color byte.
    CHECK(compact_state.object(8)->special_asset_marker == 0x00542020U);
}

void test_dispatcher_field_writes() {
    std::vector<std::byte> stream;
    u16(stream, 0x0099);
    u16(stream, 0xffff);
    u16(stream, 0x009a);
    u16(stream, 0x8000);
    u16(stream, 0x00a0);
    u16(stream, 0x1234);
    u16(stream, 0x00aa);
    u16(stream, 0x0bad);
    u16(stream, 0x00a4);
    u16(stream, 0x0055);
    u16(stream, 0x00a5);
    u16(stream, 0x0066);
    u16(stream, 0x00a8);
    u16(stream, 0x0077);
    u16(stream, 0x00ac);
    u16(stream, 0x0088);
    u16(stream, 0x00a3);
    u16(stream, 0x0099);
    u16(stream, 0x00b1);
    u16(stream, 0x00aa);
    u16(stream, 0x00a7);
    u16(stream, 0x1000);
    u16(stream, 0);
    u16(stream, 0x00a7);
    u16(stream, 0x1200);
    u16(stream, 4);
    u16(stream, 0x00ad);
    u16(stream, 0xffff);
    const std::array<std::uint16_t, 0> links{};

    LevelTriggerState state;
    TriggerRuntime runtime(TrgFile::parse(make_file({
        type6_node(links, 0x10203040, stream),
        {std::byte{0xff}, std::byte{0}},
    })), state);
    runtime.build();
    runtime.pulse_node(0);

    const TriggerCurrentObjectFields& object = state.current_object_fields();
    CHECK(object.has_4d4 && object.field_4d4 == -1);
    CHECK(object.has_4d8 && object.field_4d8 == static_cast<std::int16_t>(0x8000));
    CHECK(object.has_504 && object.field_504 == 0x1234);
    CHECK(state.global_word(0x00aa) == 0x0bad);
    CHECK(object.has_4dc && object.field_4dc == 0x55);
    CHECK(object.has_4de && object.field_4de == 0x66);
    CHECK(object.has_434 && object.field_434 == 0x77);
    CHECK(object.has_436 && object.field_436 == 0x88);
    CHECK(object.has_40c && object.field_40c == 0x1000);
    CHECK(object.has_410 && object.field_410 == 4);
    CHECK(object.has_414 && object.field_414 == 0x80);
    CHECK(object.copied_3dc_from_3a4);
    CHECK(state.current_skater_fields().field_3198 == 0x99);
    CHECK(state.current_skater_fields().field_319c == 0xaa);
    CHECK(state.dispatcher_field_writes().size() == 12);
}

void test_objectives_and_timers() {
    const GapTable& retail = retail_warehouse_gap_table();
    CHECK(retail.definitions().size() == 132);
    const TriggerGapDefinition* transfer = retail.find(1001);
    CHECK(transfer != nullptr);
    CHECK(transfer->score == 200);

    LevelTriggerState state;
    const GapTable table = GapTable::from_definitions({
        TriggerGapDefinition{0x0013, 1001, 200, "[TRANSFER]"},
        TriggerGapDefinition{0x0040, 1002, 500, "[DEFERRED]"},
        TriggerGapDefinition{0x0008, 1003, 750, "[STATE4-DEFERRED]"},
    });
    state.set_gap_table(&table);
    state.on_timer(100);
    state.advance_time(99);
    CHECK(state.timers().empty());
    CHECK(state.timer_reset_requests() == 1);
    CHECK(state.last_timer_request_ms() == 100);
    state.advance_time(1);
    CHECK(state.events().back().kind == TriggerEvent::Kind::TimerReset);

    state.on_level_event_state();
    CHECK(state.level_event_updates() == 1);
    CHECK(state.level_event_initialized());
    CHECK(state.level_event_timer_value() == 0x50);
    CHECK(state.level_event_mode_value() == 0x40);
    CHECK(state.secondary_turn_reset());
    state.on_level_event_state();
    CHECK(state.level_event_updates() == 2);
    CHECK(state.level_event_timer_value() == 0x50);

    state.on_gap(4, 0x1234, 1001);
    CHECK(state.gaps().size() == 1);
    CHECK(state.gaps()[0].definition_found);
    CHECK(state.gaps()[0].name == "[TRANSFER]");
    CHECK(state.gaps()[0].score == 200);
    CHECK(state.gaps()[0].completed);
    CHECK(state.gaps()[0].awarded);
    CHECK(state.events().back().kind == TriggerEvent::Kind::GapCompleted);
    CHECK(state.events().back().source_node == 4);
    CHECK(state.events().back().value == 1001);
    CHECK(state.events().back().checksum == 0x1234);
    CHECK(state.take_gap_pulse(0x1234, 1001));
    CHECK(!state.take_gap_pulse(0x1234, 1001));
    state.on_gap(4, 0x1234, 1001);
    CHECK(!state.take_gap_pulse(0x1234, 1001));
    CHECK(state.gaps()[0].seen == 2);
    state.mark_gap_complete(0x1234);
    CHECK(state.gaps()[0].completed);

    state.on_gap(4, 0x5678, 1002);
    CHECK(state.gaps()[1].definition_found);
    CHECK(state.gaps()[1].deferred);
    CHECK(!state.gaps()[1].completed);
    state.mark_gap_complete(0x5678);
    CHECK(state.gaps()[1].completed);
    CHECK(state.gaps()[1].awarded);
    CHECK(state.take_gap_pulse(0x5678, 1002));
    CHECK(state.events().back().kind == TriggerEvent::Kind::GapCompleted);

    state.on_gap(4, 0x9abc, 1003);
    CHECK(state.gaps()[2].definition_found);
    CHECK(state.gaps()[2].deferred);
    CHECK(!state.gaps()[2].completed);
    CHECK(!state.take_gap_pulse(0x9abc, 1003));

    state.on_spawn_node(10, 1, 0x00cb, {1, 2, 3}, {});
    CHECK(state.object(10)->spawn_family == TriggerSpawnFamily::ObjectCb);
    CHECK(state.object(10)->flags == 0x0041);
    const std::array<std::uint8_t, 3> object_options{2, 4, 7};
    state.on_spawn_node_options(10, 1, object_options);
    CHECK(state.object(10)->spawn_options
        == std::vector<std::uint8_t>({2, 4, 7}));
    CHECK(state.object(10)->has_spawn_option_2);
    CHECK(state.object(10)->has_spawn_option_4);
    CHECK(state.object(10)->factory_requires_environment_registration == false);
    CHECK(state.object(10)->factory_clears_object_flag_2);
    state.on_spawn_node(11, 5, 4, {4, 5, 6}, {});
    CHECK(state.object(11)->spawn_family == TriggerSpawnFamily::Pickup);
    CHECK(state.object(11)->factory_allocation_size == 0x100);
    CHECK(state.object(11)->factory_vtable == 0x00519684);
    CHECK(state.object(11)->factory_list == TriggerFactoryList::Pickup);
    CHECK(state.object(11)->pickup_resource == "items");
    CHECK(state.object(11)->pickup_model_checksum == 0x2328a71c);
    state.set_pickup_motion_inputs(
        11,
        {std::numeric_limits<std::int16_t>::max(),
         std::numeric_limits<std::int16_t>::min(),
         0},
        {1, -2, 0x100},
        0x100);
    state.advance_time(16);
    const std::array<std::int16_t, 3> expected_pickup_motion{
        std::numeric_limits<std::int16_t>::min(),
        32766,
        0x100};
    CHECK(state.object(11)->pickup_motion_words_14_18
        == expected_pickup_motion);
    state.set_pickup_lifecycle_inputs(11, 0x003c, 0x0032, 0x0032, 0);
    state.advance_time(16);
    CHECK(state.object(11)->pickup_timer_f0 == 0x003b);
    CHECK(state.object(11)->pickup_phase_ec == 51);
    CHECK(state.object(11)->pickup_update_calls == 2);
    CHECK(state.object(11)->pickup_glow_present);
    CHECK((state.object(11)->flags & 0x0041U) == 0x0041U);
    state.set_pickup_lifecycle_inputs(11, 1, 0x0032, 0x0032, 0);
    state.advance_time(16);
    CHECK(state.object(11)->pickup_timer_f0 == 0);
    CHECK(state.object(11)->pickup_update_calls == 3);
    CHECK(!state.object(11)->alive);
    CHECK(!state.object(11)->active);
    state.on_spawn_node(14, 5, 0x664, {0, 0, 0}, {});
    CHECK(state.object(14)->pickup_resource == "skmedals");
    CHECK(state.object(14)->pickup_model_checksum == 0x54636518);
    state.set_pickup_lifecycle_inputs(14, 0x003c, 0x0032, 0x0032, 0x02);
    state.advance_time(16);
    CHECK(state.object(14)->pickup_timer_f0 == 0x003b);
    CHECK(state.object(14)->pickup_phase_ec == 50);
    CHECK(!state.object(14)->active);
    CHECK((state.object(14)->flags & 1U) == 0);
    CHECK(!state.object(14)->pickup_glow_present);
    state.on_spawn_node(15, 5, 33, {0, 0, 0}, {});
    CHECK(state.object(15)->pickup_resource.empty());
    CHECK(state.object(15)->pickup_model_checksum == 0);
    state.on_spawn_node(12, 1, 0x00d7, {7, 8, 9}, {});
    CHECK(state.object(12)->factory_resource == "c_bus");
    CHECK(state.object(12)->has_factory_model_selector);
    CHECK(state.object(12)->factory_model_selector == 0x121);
    CHECK(state.object(12)->factory_allocation_size == 0x1e8);
    CHECK(state.object(12)->factory_vtable == 0x005184e0);
    CHECK(state.object(12)->factory_list == TriggerFactoryList::CommonObject);
    state.on_spawn_node_options(12, 7, {});
    CHECK(state.object(12)->factory_requires_environment_registration);
    CHECK(state.object(12)->factory_sets_object_flag_4);

    state.on_spawn_node(13, 1, 0x0192, {0, 0, 0}, {});
    CHECK(state.object(13)->spawn_family == TriggerSpawnFamily::Object192);
    CHECK(state.object(13)->factory_allocation_size == 0x218);
    CHECK(state.object(13)->factory_vtable == 0x005194f8);
    CHECK(state.object(13)->factory_list == TriggerFactoryList::Object192);
    CHECK(state.object(13)->flags == 0x0111);

    const std::array<std::byte, 4> constructor_bytes{
        std::byte{0x11}, std::byte{0x22}, std::byte{0x33}, std::byte{0x44}};
    state.on_spawn_node(13, 1, 0x0192, {0, 0, 0}, constructor_bytes);
    CHECK(state.object(13)->factory_node_bytes
        == std::vector<std::byte>(constructor_bytes.begin(), constructor_bytes.end()));
    state.on_spawn_factory_cursor(13, 30);
    CHECK(state.object(13)->has_factory_cursor_offset);
    CHECK(state.object(13)->factory_cursor_offset == 30);

    state.on_spawn_node(16, 7, 0x00cb, {0, 0, 0}, {});
    state.on_spawn_node_options(16, 7, {});
    CHECK((state.object(16)->flags & 0x0004U) != 0);

    state.set_career_flag(3);
    state.mark_goal_complete(5);
    CHECK(state.career_flag(3));
    CHECK(state.goal_complete(5));
}

void test_level_event_mode_and_frame_contract() {
    LevelTriggerState mode8_less;
    mode8_less.set_level_event_inputs(
        TriggerLevelEventInputs{8, 0, false, 10, 20});
    mode8_less.on_level_event_state();
    const TriggerLevelEventRawStats& less_stats =
        mode8_less.level_event_raw_stats();
    CHECK(less_stats.word_0056b7e0 == 1);
    CHECK(less_stats.word_0056b79c == 1);
    CHECK(less_stats.word_0056b7b4 == 1);
    CHECK(less_stats.word_0056b7b0 == -1);
    CHECK(less_stats.word_0056b7a4 == 10);
    CHECK(less_stats.word_0056b7a0 == 20);
    mode8_less.on_level_event_state();
    CHECK(mode8_less.level_event_raw_stats().word_0056b7e0 == 1);

    LevelTriggerState mode8_greater;
    mode8_greater.set_level_event_inputs(
        TriggerLevelEventInputs{8, 0, false, 30, 20});
    mode8_greater.on_level_event_state();
    CHECK(mode8_greater.level_event_raw_stats().word_0056b7dc == 1);
    CHECK(mode8_greater.level_event_raw_stats().word_0056b798 == 1);
    CHECK(mode8_greater.level_event_raw_stats().word_0056b7b0 == 1);
    CHECK(mode8_greater.level_event_raw_stats().word_0056b7b4 == -1);

    LevelTriggerState mode9;
    mode9.set_level_event_inputs(
        TriggerLevelEventInputs{9, 2, false, 0, 0});
    mode9.on_level_event_state();
    CHECK(mode9.level_event_raw_stats().word_0056b7dc == 1);
    CHECK(mode9.level_event_raw_stats().word_0056b798 == 1);
    CHECK(mode9.level_event_raw_stats().word_0056b7b0 == 1);
    CHECK(mode9.level_event_raw_stats().word_0056b7b4 == -1);

    LevelTriggerState state;
    state.set_level_event_inputs(TriggerLevelEventInputs{1, 0, false, 0, 0});
    state.on_level_event_state();
    TriggerLevelEventFrameInput frame{};
    frame.players_eligible = true;
    frame.secondary_present = true;
    frame.secondary_eligible = true;
    frame.primary_state7 = true;
    frame.secondary_state7 = true;
    frame.primary_animation_state = 0;
    frame.secondary_animation_state = 0x5d;
    frame.secondary_animation_flag_107 = true;
    frame.primary_pending_score = 100;
    frame.secondary_pending_score = 200;
    frame.primary_score_input_active = true;
    frame.secondary_score_input_active = true;
    state.set_level_event_frame_input(frame);

    LevelTriggerState tick_bridge;
    tick_bridge.on_level_event_state();
    TriggerLevelEventFrameInput tick_frame{};
    tick_frame.players_eligible = true;
    tick_bridge.set_level_event_frame_input(tick_frame);
    tick_bridge.advance_time(0);
    CHECK(tick_bridge.level_event_timer_value() == 0x4f);

    for (int frame_index = 0; frame_index < 79; ++frame_index) {
        const TriggerLevelEventFrameResult result =
            state.advance_level_event_frame();
        CHECK(result.primary_animation == 0x5d);
        CHECK(result.secondary_animation == 0x5f);
        CHECK(result.primary_animation_started);
        CHECK(result.secondary_animation_started);
    }
    CHECK(state.level_event_timer_value() == 1);
    CHECK(state.level_event_camera_updates() == 78);
    CHECK(state.level_event_primary_camera_delta() == 39 * 0x40);
    CHECK(state.level_event_secondary_camera_delta() == 39 * 0x40);

    const TriggerLevelEventFrameResult committed =
        state.advance_level_event_frame();
    CHECK(committed.primary_score_committed == 100);
    CHECK(committed.secondary_score_committed == 200);
    CHECK(!committed.completion_reset_requested);
    CHECK(state.level_event_initialized());
    CHECK(state.level_event_mode_value() == 0);
    CHECK(state.level_event_timer_value() == 1);
    CHECK(state.level_event_replay_reset_requests() == 2);

    frame.primary_pending_score = 0;
    frame.secondary_pending_score = 0;
    state.set_level_event_frame_input(frame);
    const TriggerLevelEventFrameResult completed =
        state.advance_level_event_frame();
    CHECK(completed.replay_reset_requests == 2);
    CHECK(completed.completion_reset_requested);
    CHECK(!state.level_event_initialized());
    CHECK(state.level_event_completion_reset_requests() == 1);

    LevelTriggerState mode7;
    mode7.set_level_event_inputs(TriggerLevelEventInputs{7, 0, false, 0, 0});
    mode7.on_level_event_state();
    TriggerLevelEventFrameInput mode7_frame{};
    mode7_frame.players_eligible = true;
    mode7_frame.mode7_input_active = false;
    mode7.set_level_event_frame_input(mode7_frame);
    for (int frame_index = 0; frame_index < 21; ++frame_index) {
        (void)mode7.advance_level_event_frame();
    }
    mode7_frame.mode7_input_active = true;
    mode7.set_level_event_frame_input(mode7_frame);
    const TriggerLevelEventFrameResult mode7_result =
        mode7.advance_level_event_frame();
    CHECK(mode7_result.completion_reset_requested);
    CHECK(!mode7.level_event_initialized());
}

void test_scene_registry() {
    LevelTriggerState state;
    state.on_linked_node(7, 2, 0x1111, {});
    state.on_spawn_node(8, 1, 0x00cb, {100, 200, 300}, {});
    state.on_special_node_state(9, 10, 0x0042, {400, 500, 600});
    const std::array<std::uint8_t, 2> scene_options{2, 4};
    state.on_spawn_node_options(8, 1, scene_options);
    const opentony::assets::PsxArchive archive =
        opentony::assets::PsxArchive::parse(synthetic_psx(), "scene.psx");
    state.bind_psx_models(archive);

    LevelSceneRegistry scene;
    scene.build(state, archive);
    CHECK(scene.static_entity_count() == 2);
    CHECK(scene.entities().size() == 4);
    CHECK(scene.bound_trigger_count() == 1);
    CHECK(scene.unresolved_trigger_count() == 2);
    const LevelSceneBinding* linked = scene.binding(7);
    CHECK(linked != nullptr);
    CHECK(linked->bound_to_psx);
    CHECK(linked->entities == std::vector<std::size_t>{0});
    const LevelSceneBinding* spawned = scene.binding(8);
    CHECK(spawned != nullptr);
    CHECK(!spawned->bound_to_psx);
    CHECK(scene.entity(spawned->entities[0])->spawn_family == TriggerSpawnFamily::ObjectCb);
    CHECK(scene.entity(spawned->entities[0])->spawn_options
        == std::vector<std::uint8_t>({2, 4}));
    CHECK(scene.entity(spawned->entities[0])->factory_clears_object_flag_2);
    CHECK(scene.entity(spawned->entities[0])->has_spawn_option_2);
    const LevelSceneBinding* special = scene.binding(9);
    CHECK(special != nullptr);
    CHECK(!special->bound_to_psx);
    const LevelSceneEntity* special_entity = scene.entity(special->entities[0]);
    CHECK(special_entity != nullptr);
    CHECK(special_entity->trigger_bounds.valid);
    const std::array<std::int32_t, 3> special_bounds{400, 500, 600};
    CHECK(special_entity->trigger_bounds.minimum == special_bounds);
    CHECK(special_entity->trigger_bounds.maximum == special_bounds);

    const std::array<std::uint16_t, 1> links{7};
    state.on_visible(0, 1, links);
    scene.sync(state);
    CHECK((scene.entity(0)->gameplay_flags & 0x41U) == 0);
}

} // namespace

int main() {
    test_stateful_dispatch();
    test_counted_link_commands_update_only_inline_targets();
    test_retail_link_target_filters();
    test_script_object_state();
    test_type10_type11_pulse_and_kill_state();
    test_type10_type11_alias_bounds();
    test_type12_type14_runtime_activation();
    test_type12_type14_runtime_links();
    test_type12_type14_runtime_link_policy();
    test_type12_type14_animation_modes();
    test_dispatcher_field_writes();
    test_objectives_and_timers();
    test_level_event_mode_and_frame_contract();
    test_scene_registry();
    std::cout << "Level trigger state tests passed\n";
}
