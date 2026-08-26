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

    // 0x83/0x84 search the same retail object-list identity space.
    state.on_object_flag_by_id(0, true);
    assert((state.script_objects()[0].flags & 1U) != 0);
    state.on_object_flag_by_id(0, false);
    assert((state.script_objects()[0].flags & 1U) == 0);
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
    assert(state.timers().size() == 1);
    assert(!state.timers()[0].fired);
    state.advance_time(1);
    assert(state.timers()[0].fired);

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

    state.on_spawn_node(10, 1, 0x00cb, {1, 2, 3}, {});
    assert(state.object(10)->spawn_family == TriggerSpawnFamily::ObjectCb);
    state.on_spawn_node(11, 5, 4, {4, 5, 6}, {});
    assert(state.object(11)->spawn_family == TriggerSpawnFamily::Pickup);
    state.on_spawn_node(12, 1, 0x00d7, {7, 8, 9}, {});
    assert(state.object(12)->factory_resource == "c_bus");
    assert(state.object(12)->has_factory_model_selector);
    assert(state.object(12)->factory_model_selector == 0x121);

    state.set_career_flag(3);
    state.mark_goal_complete(5);
    assert(state.career_flag(3));
    assert(state.goal_complete(5));
}

void test_scene_registry() {
    LevelTriggerState state;
    state.on_linked_node(7, 2, 0x1111, {});
    state.on_spawn_node(8, 1, 0x00cb, {100, 200, 300}, {});
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
    test_objectives_and_timers();
    test_scene_registry();
    std::cout << "Level trigger state tests passed\n";
}
