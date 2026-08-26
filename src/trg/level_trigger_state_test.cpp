#include "level_trigger_state.hpp"
#include "level_scene_registry.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
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
    assert(target != nullptr);
    assert(target->node_type == 12);
    assert(target->link_key == 0x12345678);
    assert(target->visible_commanded);
    assert((target->flags & 1U) != 0);
    assert(target->pulses == 1);
    assert(state.events().size() == 3);
}

void test_retail_link_target_filters() {
    LevelTriggerState state;
    state.on_spawn_node(1, 1, 0xcb, {0, 0, 0}, {});
    assert(state.object(1)->flags == 0x0041);
    state.on_spawn_node(2, 5, 0, {0, 0, 0}, {});
    state.on_linked_node(3, 2, 0x1234, {});
    state.on_special_node(4, 12, {});

    const std::array<std::uint16_t, 4> links{1, 2, 3, 4};
    state.on_signal(0, links);
    state.on_suspend_activate(0, 4, links);
    state.on_visible(0, 1, links);

    // Retail signal/suspend only traverse the type-1/type-7 object lists.
    assert(state.object(1)->signals == 1);
    assert(state.object(1)->suspend_activate_calls == 1);
    assert(state.object(2)->signals == 0);
    assert(state.object(3)->suspend_activate_calls == 0);
    // SendVisible resolves crates, pickups, type-12 and type-14 links, not
    // the type-1 baddy object.
    assert(!state.object(1)->visible_commanded);
    assert(state.object(2)->visible_commanded);
    assert(state.object(3)->visible_commanded);
    assert(state.object(4)->visible_commanded);
}

void test_script_object_state() {
    LevelTriggerState state;
    state.on_script_object(27, 0xf6e32d0e, {1, 2, 3});
    assert(state.script_objects().size() == 1);
    assert(state.script_objects()[0].source_node == 27);
    assert(state.script_objects()[0].script_key == 0xf6e32d0e);
    const std::array<std::uint16_t, 3> expected_parameters{1, 2, 3};
    assert(state.script_objects()[0].parameters == expected_parameters);
    assert(state.script_objects()[0].identifier == 0);
    assert(state.script_objects()[0].raw_record().size() == 0xcc);
    assert(state.script_objects()[0].raw_record()[0xc8] == std::byte{0});
    assert(state.script_objects()[0].raw_record()[0xc9] == std::byte{0});

    // 0x83/0x84 search the same retail object-list identity space.
    state.on_object_flag_by_id(0, true);
    assert((state.script_objects()[0].flags & 1U) != 0);
    assert(state.script_objects()[0].raw_record()[0x04] == std::byte{1});
    state.on_object_flag_by_id(0, false);
    assert((state.script_objects()[0].flags & 1U) == 0);
    assert(state.script_objects()[0].raw_record()[0x04] == std::byte{0});
}

void test_type10_type11_pulse_and_kill_state() {
    LevelTriggerState state;
    const std::array<std::int32_t, 3> position{10, 20, 30};
    state.on_special_node(4, 10, {});
    state.on_special_node_state(4, 10, 0x0042, position);
    const TriggerObjectState* object = state.object(4);
    assert(object != nullptr);
    assert(object->has_trigger_runtime);
    assert(object->trigger_flags == 0x0042);
    assert(object->trigger_mode == 2);
    assert(object->trigger_state == 0);
    assert(!object->active);
    assert(object->position == position);

    state.on_node_pulse(4);
    assert(state.object(4)->trigger_state == 1);
    assert(state.object(4)->active);
    const std::array<std::uint16_t, 1> links{4};
    state.on_kill(0, 0x000c, links);
    assert(state.object(4)->trigger_state == 0);
    assert(!state.object(4)->active);
    assert(state.object(4)->alive);
    assert(!state.object(4)->killed);
}

void test_type12_type14_runtime_activation() {
    LevelTriggerState state;
    state.set_special_runtime_context(2, 0x13579bdf);
    state.on_linked_node(8, 12, 0xfeedcafe, {});
    const TriggerObjectState* object = state.object(8);
    assert(object != nullptr);
    assert(object->has_special_runtime);
    assert(!object->special_runtime_active);
    assert(object->link_key == 0xfeedcafe);
    assert(object->raw_special_record().size() == 0x18);
    assert(object->raw_special_record()[0x04] == std::byte{0xfe});
    assert(object->raw_special_record()[0x05] == std::byte{0xca});
    assert(object->raw_special_record()[0x08] == std::byte{8});
    assert(object->raw_special_record()[0x0a] == std::byte{0});
    state.on_node_pulse(8);
    assert(state.object(8)->special_runtime_active);
    assert(state.object(8)->has_special_runtime_context);
    assert(state.object(8)->special_runtime_owner == 2);
    assert(state.object(8)->raw_special_record()[0x0a] == std::byte{1});
    assert(state.object(8)->raw_special_record()[0x0b] == std::byte{2});
    assert(state.object(8)->special_runtime_control == 0x13579bdf);
    assert(state.object(8)->has_special_asset_state);
    assert(state.object(8)->special_asset_flags_or == 4);
    assert(state.object(8)->special_asset_marker == 0x202020);
    assert(state.object(8)->active);
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
    assert(object.has_4d4 && object.field_4d4 == -1);
    assert(object.has_4d8 && object.field_4d8 == static_cast<std::int16_t>(0x8000));
    assert(object.has_504 && object.field_504 == 0x1234);
    assert(state.global_word(0x00aa) == 0x0bad);
    assert(object.has_4dc && object.field_4dc == 0x55);
    assert(object.has_4de && object.field_4de == 0x66);
    assert(object.has_434 && object.field_434 == 0x77);
    assert(object.has_436 && object.field_436 == 0x88);
    assert(object.has_40c && object.field_40c == 0x1000);
    assert(object.has_410 && object.field_410 == 4);
    assert(object.has_414 && object.field_414 == 0x80);
    assert(object.copied_3dc_from_3a4);
    assert(state.current_skater_fields().field_3198 == 0x99);
    assert(state.current_skater_fields().field_319c == 0xaa);
    assert(state.dispatcher_field_writes().size() == 12);
}

void test_objectives_and_timers() {
    const GapTable& retail = retail_warehouse_gap_table();
    assert(retail.definitions().size() == 132);
    const TriggerGapDefinition* transfer = retail.find(1001);
    assert(transfer != nullptr);
    assert(transfer->score == 200);

    LevelTriggerState state;
    const GapTable table = GapTable::from_definitions({
        TriggerGapDefinition{0x0013, 1001, 200, "[TRANSFER]"},
        TriggerGapDefinition{0x0040, 1002, 500, "[DEFERRED]"},
    });
    state.set_gap_table(&table);
    state.on_timer(100);
    state.advance_time(99);
    assert(state.timers().empty());
    assert(state.timer_reset_requests() == 1);
    assert(state.last_timer_request_ms() == 100);
    state.advance_time(1);
    assert(state.events().back().kind == TriggerEvent::Kind::TimerReset);

    state.on_level_event_state();
    assert(state.level_event_updates() == 1);
    assert(state.level_event_initialized());
    assert(state.level_event_timer_value() == 0x50);
    assert(state.level_event_mode_value() == 0x40);
    assert(state.secondary_turn_reset());
    state.on_level_event_state();
    assert(state.level_event_updates() == 2);
    assert(state.level_event_timer_value() == 0x50);

    state.on_gap(4, 0x1234, 1001);
    assert(state.gaps().size() == 1);
    assert(state.gaps()[0].definition_found);
    assert(state.gaps()[0].name == "[TRANSFER]");
    assert(state.gaps()[0].score == 200);
    assert(state.gaps()[0].completed);
    assert(state.gaps()[0].awarded);
    assert(state.take_gap_pulse(0x1234, 1001));
    assert(!state.take_gap_pulse(0x1234, 1001));
    state.on_gap(4, 0x1234, 1001);
    assert(!state.take_gap_pulse(0x1234, 1001));
    assert(state.gaps()[0].seen == 2);
    state.mark_gap_complete(0x1234);
    assert(state.gaps()[0].completed);

    state.on_gap(4, 0x5678, 1002);
    assert(state.gaps()[1].definition_found);
    assert(state.gaps()[1].deferred);
    assert(!state.gaps()[1].completed);
    state.mark_gap_complete(0x5678);
    assert(state.gaps()[1].completed);
    assert(state.gaps()[1].awarded);
    assert(state.take_gap_pulse(0x5678, 1002));
    assert(state.events().back().kind == TriggerEvent::Kind::GapCompleted);

    state.on_spawn_node(10, 1, 0x00cb, {1, 2, 3}, {});
    assert(state.object(10)->spawn_family == TriggerSpawnFamily::ObjectCb);
    assert(state.object(10)->flags == 0x0041);
    const std::array<std::uint8_t, 3> object_options{2, 4, 7};
    state.on_spawn_node_options(10, 1, object_options);
    assert(state.object(10)->spawn_options
        == std::vector<std::uint8_t>({2, 4, 7}));
    assert(state.object(10)->has_spawn_option_2);
    assert(state.object(10)->has_spawn_option_4);
    assert(state.object(10)->factory_requires_environment_registration == false);
    assert(state.object(10)->factory_clears_object_flag_2);
    state.on_spawn_node(11, 5, 4, {4, 5, 6}, {});
    assert(state.object(11)->spawn_family == TriggerSpawnFamily::Pickup);
    state.on_spawn_node(12, 1, 0x00d7, {7, 8, 9}, {});
    assert(state.object(12)->factory_resource == "c_bus");
    assert(state.object(12)->has_factory_model_selector);
    assert(state.object(12)->factory_model_selector == 0x121);
    state.on_spawn_node_options(12, 7, {});
    assert(state.object(12)->factory_requires_environment_registration);
    assert(state.object(12)->factory_sets_object_flag_4);

    state.on_spawn_node(13, 1, 0x0192, {0, 0, 0}, {});
    assert(state.object(13)->spawn_family == TriggerSpawnFamily::Object192);
    assert(state.object(13)->flags == 0x0111);

    const std::array<std::byte, 4> constructor_bytes{
        std::byte{0x11}, std::byte{0x22}, std::byte{0x33}, std::byte{0x44}};
    state.on_spawn_node(13, 1, 0x0192, {0, 0, 0}, constructor_bytes);
    assert(state.object(13)->factory_node_bytes
        == std::vector<std::byte>(constructor_bytes.begin(), constructor_bytes.end()));
    state.on_spawn_factory_cursor(13, 30);
    assert(state.object(13)->has_factory_cursor_offset);
    assert(state.object(13)->factory_cursor_offset == 30);

    state.set_career_flag(3);
    state.mark_goal_complete(5);
    assert(state.career_flag(3));
    assert(state.goal_complete(5));
}

void test_scene_registry() {
    LevelTriggerState state;
    state.on_linked_node(7, 2, 0x1111, {});
    state.on_spawn_node(8, 1, 0x00cb, {100, 200, 300}, {});
    const std::array<std::uint8_t, 2> scene_options{2, 4};
    state.on_spawn_node_options(8, 1, scene_options);
    const opentony::assets::PsxArchive archive =
        opentony::assets::PsxArchive::parse(synthetic_psx(), "scene.psx");
    state.bind_psx_models(archive);

    LevelSceneRegistry scene;
    scene.build(state, archive);
    assert(scene.static_entity_count() == 2);
    assert(scene.entities().size() == 3);
    assert(scene.bound_trigger_count() == 1);
    assert(scene.unresolved_trigger_count() == 1);
    const LevelSceneBinding* linked = scene.binding(7);
    assert(linked != nullptr);
    assert(linked->bound_to_psx);
    assert(linked->entities == std::vector<std::size_t>{0});
    const LevelSceneBinding* spawned = scene.binding(8);
    assert(spawned != nullptr);
    assert(!spawned->bound_to_psx);
    assert(scene.entity(spawned->entities[0])->spawn_family == TriggerSpawnFamily::ObjectCb);
    assert(scene.entity(spawned->entities[0])->spawn_options
        == std::vector<std::uint8_t>({2, 4}));
    assert(scene.entity(spawned->entities[0])->factory_clears_object_flag_2);
    assert(scene.entity(spawned->entities[0])->has_spawn_option_2);

    const std::array<std::uint16_t, 1> links{7};
    state.on_visible(0, 1, links);
    scene.sync(state);
    assert((scene.entity(0)->gameplay_flags & 0x41U) == 0);
}

} // namespace

int main() {
    test_stateful_dispatch();
    test_retail_link_target_filters();
    test_script_object_state();
    test_type10_type11_pulse_and_kill_state();
    test_type12_type14_runtime_activation();
    test_dispatcher_field_writes();
    test_objectives_and_timers();
    test_scene_registry();
    std::cout << "Level trigger state tests passed\n";
}
