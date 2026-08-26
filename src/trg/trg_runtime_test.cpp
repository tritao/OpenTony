#include "trg_runtime.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <tuple>
#include <utility>
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

std::vector<std::byte> trg_file(const std::vector<std::vector<std::byte>>& nodes) {
    const std::size_t first_node = 12 + nodes.size() * 4;
    std::vector<std::byte> result;
    result.reserve(first_node);
    result.insert(result.end(), {
        std::byte{'_'}, std::byte{'T'}, std::byte{'R'}, std::byte{'G'},
    });
    u32(result, 2);
    u32(result, static_cast<std::uint32_t>(nodes.size()));
    std::size_t offset = first_node;
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
    // The absolute-node alignment is tested by the runtime using the final
    // file offset. This helper is used with an even first-node offset, so a
    // two-byte pad is required only for an odd number of link words.
    if (((20 + node.size()) & 2) != 0) {
        u16(node, 0);
    }
    u32(node, checksum);
    node.insert(node.end(), stream.begin(), stream.end());
    return node;
}

std::vector<std::byte> object_node() {
    std::vector<std::byte> node;
    u16(node, 1);
    u16(node, 0);
    u16(node, 0);
    u16(node, 0);
    node.push_back(std::byte{2});
    node.push_back(std::byte{4});
    node.push_back(std::byte{0xff});
    node.insert(node.end(), 24, std::byte{0});
    return node;
}

struct Recorder final : TriggerServices {
    std::size_t objects{};
    std::size_t pickups{};
    std::vector<std::pair<std::size_t, std::string>> restarts;
    std::vector<std::uint16_t> visible_values;
    std::vector<std::vector<std::uint16_t>> visible_links;
    std::vector<std::pair<std::uint32_t, std::uint16_t>> gaps;
    std::vector<std::uint32_t> timers;
    std::vector<std::string> resources;
    std::vector<std::string> diagnostics;
    std::vector<std::uint16_t> flags;
    std::vector<std::uint16_t> initial_states;
    std::vector<std::size_t> node_pulses;
    std::vector<std::pair<std::uint16_t, std::uint16_t>> global_words;
    std::vector<std::vector<std::uint8_t>> spawn_options;
    std::vector<std::size_t> selected_restarts;
    std::vector<std::size_t> applied_restarts;
    std::vector<std::pair<std::uint32_t, std::uint16_t>> restart_data;
    std::vector<std::pair<std::uint32_t, std::uint16_t>> applied_restart_data;
    std::vector<std::tuple<std::size_t, std::uint32_t, std::array<std::uint16_t, 3>>> script_objects;
    std::vector<std::tuple<std::size_t, std::uint16_t, std::uint16_t, std::array<std::int32_t, 3>>> special_states;
    std::vector<std::tuple<std::uint16_t, std::uint32_t, std::size_t>> unknown_commands;

    void on_object_node(std::size_t) override { ++objects; }
    void on_pickup_node(std::size_t) override { ++pickups; }
    void on_spawn_node_options(
        std::size_t,
        std::uint16_t,
        std::span<const std::uint8_t> options) override {
        spawn_options.emplace_back(options.begin(), options.end());
    }
    void on_script_object(
        std::size_t source,
        std::uint32_t key,
        std::array<std::uint16_t, 3> parameters) override {
        script_objects.emplace_back(source, key, parameters);
    }
    void on_special_node_state(
        std::size_t node,
        std::uint16_t type,
        std::uint16_t flags,
        std::array<std::int32_t, 3> position) override {
        special_states.emplace_back(node, type, flags, position);
    }
    void on_restart_node(std::size_t index, std::string_view name, std::array<std::int32_t, 3>) override {
        restarts.emplace_back(index, std::string(name));
    }
    void on_restart_node_data(std::size_t, std::uint32_t auxiliary, std::uint16_t word) override {
        restart_data.emplace_back(auxiliary, word);
    }
    void on_visible(std::size_t, std::uint16_t value, std::span<const std::uint16_t> links) override {
        visible_values.push_back(value);
        visible_links.emplace_back(links.begin(), links.end());
    }
    void on_gap(std::size_t, std::uint32_t checksum, std::uint16_t argument) override {
        gaps.emplace_back(checksum, argument);
    }
    void on_timer(std::uint32_t milliseconds) override { timers.push_back(milliseconds); }
    void on_resource(std::uint16_t, std::string_view name) override { resources.emplace_back(name); }
    void on_initial_state(std::uint16_t value) override { initial_states.push_back(value); }
    void set_career_flag(std::uint16_t flag) override { flags.push_back(flag); }
    void on_node_pulse(std::size_t node) override { node_pulses.push_back(node); }
    void on_global_word(std::uint16_t opcode, std::uint16_t value) override {
        global_words.emplace_back(opcode, value);
    }
    void on_restart_selected(std::uint16_t, std::size_t node, std::string_view) override {
        selected_restarts.push_back(node);
    }
    void on_apply_restart(std::size_t node, std::array<std::int32_t, 3>) override {
        applied_restarts.push_back(node);
    }
    void on_apply_restart_data(std::size_t, std::uint32_t auxiliary, std::uint16_t word) override {
        applied_restart_data.emplace_back(auxiliary, word);
    }
    void on_unknown_command(
        std::uint16_t opcode,
        std::uint32_t offset,
        std::size_t node,
        std::span<const std::byte>) override {
        unknown_commands.emplace_back(opcode, offset, node);
    }
    void on_diagnostic(std::string_view message) override { diagnostics.emplace_back(message); }
};

void test_visible_pulse_and_record_layout() {
    std::vector<std::byte> stream;
    u16(stream, 0x000d);
    u16(stream, 1);
    u16(stream, 0xffff);
    const std::array<std::uint16_t, 1> links{1};
    const std::vector<std::byte> file_bytes = trg_file({
        type6_node(links, 0, stream),
        object_node(),
        std::vector<std::byte>{std::byte{0xff}, std::byte{0}},
    });
    TrgFile file = TrgFile::parse(file_bytes);
    assert(file.nodes().size() == 3);
    assert(file.nodes()[0].offset == 24);
    assert(file.node_bytes(0).size() == type6_node(links, 0, stream).size());
    assert(file.script(0).offset == 36);
    assert(file.links(0) == std::vector<std::uint16_t>{1});
    assert(file.node_spawn_options(1) == std::vector<std::uint8_t>({2, 4}));

    Recorder recorder;
    TriggerRuntime runtime(std::move(file), recorder);
    runtime.build();
    runtime.pulse_node(0);
    assert(recorder.objects == 1);
    const std::vector<std::vector<std::uint8_t>> expected_spawn_options{{2, 4}};
    assert(recorder.spawn_options == expected_spawn_options);
    assert(recorder.visible_values == std::vector<std::uint16_t>{1});
    assert(recorder.visible_links == std::vector<std::vector<std::uint16_t>>{{1}});
    const CommandPointRuntime* point = runtime.command_point(0);
    assert(point != nullptr);
    assert(point->source_node == 0);
    assert(point->pulse_count == 1);
}

void test_c9_alignment_and_gap_dispatch() {
    std::vector<std::byte> node;
    u16(node, 6);
    u16(node, 0);
    u32(node, 0xf3abdf8e);
    u16(node, 0x00c9);
    u16(node, 0);
    u32(node, 0xf3abdf8e);
    u16(node, 10);
    u16(node, 0xffff);
    const std::vector<std::byte> file_bytes = trg_file({node, {std::byte{0xff}, std::byte{0}}});

    Recorder recorder;
    TriggerRuntime runtime(TrgFile::parse(file_bytes), recorder);
    runtime.build();
    runtime.pulse_node(0);
    const std::vector<std::pair<std::uint32_t, std::uint16_t>> expected_gaps{{0xf3abdf8e, 10}};
    assert(recorder.gaps == expected_gaps);
    assert(recorder.diagnostics.empty());
}

void test_initial_pulses_and_timer() {
    std::vector<std::byte> stream;
    u16(stream, 0x00a6);
    u16(stream, 512);
    u16(stream, 0x00a9);
    u16(stream, 1000);
    u16(stream, 0x0086);
    u16(stream, 1);
    u16(stream, 0x0003);
    u16(stream, 0xffff);
    const std::array<std::uint16_t, 1> links{1};
    const std::vector<std::byte> file_bytes = trg_file({
        type6_node(links, 0, stream),
        object_node(),
        {std::byte{0xff}, std::byte{0}},
    });
    Recorder recorder;
    TriggerRuntime runtime(TrgFile::parse(file_bytes), recorder);
    runtime.build();
    runtime.pulse_node(0);
    const CommandPointRuntime* point = runtime.command_point(0);
    assert(point != nullptr);
    assert(point->initialized == 1);
    assert(point->state == 0);
    assert(point->pulse_count == 1);
    const std::vector<std::pair<std::uint16_t, std::uint16_t>> expected_globals{
        {0x00a6, 512},
        {0x00a9, 1000},
    };
    assert(recorder.global_words == expected_globals);
    runtime.pulse_node(0);
    assert(recorder.node_pulses == std::vector<std::size_t>{1});
    assert(point->pulse_count == 2);
}

void test_conditional_skip_consumes_music_and_sound_operands() {
    std::vector<std::byte> stream;
    u16(stream, 0x0094);
    u16(stream, 0); // pulse count deliberately does not match the first pulse
    u16(stream, 0x0069);
    u16(stream, 7);
    u16(stream, 0x006a);
    u16(stream, 8);
    u16(stream, 0x0095);
    u16(stream, 0xffff);

    const std::array<std::uint16_t, 0> links{};
    Recorder recorder;
    TriggerRuntime runtime(TrgFile::parse(trg_file({
        type6_node(links, 0, stream),
        {std::byte{0xff}, std::byte{0}},
    })), recorder);
    runtime.build();
    runtime.pulse_node(0);
    assert(recorder.diagnostics.empty());
}

void test_conditional_skip_consumes_a7_pair() {
    std::vector<std::byte> stream;
    u16(stream, 0x0094);
    u16(stream, 0); // pulse count deliberately does not match the pair
    u16(stream, 0x00a7);
    u16(stream, 0x0123);
    u16(stream, 0x0045);
    u16(stream, 0x00ae);
    u16(stream, 0x0067);
    u16(stream, 0x0089);
    u16(stream, 0x00c8);
    u16(stream, 0x00ab);
    u16(stream, 0x00cd);
    u16(stream, 0x00ca);
    u16(stream, 0x00ef);
    u16(stream, 0x0012);
    u16(stream, 0x0095);
    u16(stream, 0x00a6);
    u16(stream, 9);
    u16(stream, 0xffff);

    const std::array<std::uint16_t, 0> links{};
    Recorder recorder;
    TriggerRuntime runtime(TrgFile::parse(trg_file({
        type6_node(links, 0, stream),
        {std::byte{0xff}, std::byte{0}},
    })), recorder);
    runtime.build();
    runtime.pulse_node(0);
    const std::vector<std::pair<std::uint16_t, std::uint16_t>> expected_globals{
        {0x00a6, 9}};
    assert(recorder.global_words == expected_globals);
    assert(recorder.diagnostics.empty());
}

void test_restart_selection_and_checksum_lookup() {
    std::vector<std::byte> autoexec;
    u16(autoexec, 4);
    u16(autoexec, 0x008c);
    autoexec.push_back(std::byte{'R'});
    autoexec.push_back(std::byte{0});
    u16(autoexec, 0xffff);

    std::vector<std::byte> stream;
    u16(stream, 0x008c);
    stream.push_back(std::byte{'R'});
    stream.push_back(std::byte{0});
    u16(stream, 0xffff);

    std::vector<std::byte> restart;
    u16(restart, 8);
    u16(restart, 0);
    // With four nodes, the restart starts at absolute offset 50; the
    // retail position helper therefore aligns its payload at offset 56.
    u16(restart, 0);
    u32(restart, 1);
    u32(restart, 2);
    u32(restart, 3);
    u32(restart, 0x44556677);
    u16(restart, 0x8899);
    restart.push_back(std::byte{'R'});
    restart.push_back(std::byte{0});
    u16(restart, 0xffff);

    const std::array<std::uint16_t, 0> links{};
    const std::vector<std::byte> file_bytes = trg_file({
        autoexec,
        type6_node(links, 0x12345678, stream),
        restart,
        {std::byte{0xff}, std::byte{0}},
    });
    Recorder recorder;
    TriggerRuntime runtime(TrgFile::parse(file_bytes), recorder);
    runtime.initialize();
    assert(runtime.selected_restart() == 2);
    assert(recorder.selected_restarts == std::vector<std::size_t>{2});
    runtime.pulse_node(1);
    const std::vector<std::size_t> expected_selected{2, 2};
    assert(recorder.selected_restarts == expected_selected);
    const CommandPointRuntime* point = runtime.command_point_by_checksum(0x12345678);
    assert(point != nullptr);
    assert(point->source_node == 1);
    runtime.execute_restart(2);
    assert(recorder.applied_restarts == std::vector<std::size_t>{2});
    const std::vector<std::pair<std::uint32_t, std::uint16_t>> expected_applied_data{{0x44556677, 0x8899}};
    assert(recorder.applied_restart_data == expected_applied_data);
}

void test_two_player_restart_selection_command() {
    std::vector<std::byte> autoexec;
    u16(autoexec, 4);
    u16(autoexec, 0x00b2);
    autoexec.push_back(std::byte{'R'});
    autoexec.push_back(std::byte{0});
    u16(autoexec, 0xffff);

    std::vector<std::byte> restart;
    u16(restart, 8);
    u16(restart, 0);
    u32(restart, 1);
    u32(restart, 2);
    u32(restart, 3);
    u32(restart, 0);
    u16(restart, 0);
    restart.push_back(std::byte{'R'});
    restart.push_back(std::byte{0});
    u16(restart, 0xffff);

    Recorder recorder;
    TriggerRuntime runtime(TrgFile::parse(trg_file({
        autoexec,
        restart,
        {std::byte{0xff}, std::byte{0}},
    })), recorder);
    runtime.initialize(true);
    assert(runtime.selected_restart() == 1);
    assert(recorder.selected_restarts == std::vector<std::size_t>{1});

    // The same opcode is consumed but does not select a restart outside the
    // retail two-player mode.
    runtime.run_autoexec(false);
    assert(runtime.selected_restart() == 1);
    assert(recorder.selected_restarts == std::vector<std::size_t>{1});
}

void test_kill_bruce_applies_linked_restart() {
    std::vector<std::byte> stream;
    u16(stream, 0x0098);
    u16(stream, 0xffff);
    const std::array<std::uint16_t, 1> links{1};

    std::vector<std::byte> restart;
    u16(restart, 8);
    u16(restart, 0);
    u32(restart, 0x1111);
    u32(restart, 0x2222);
    u32(restart, 0x3333);
    u32(restart, 0x44556677);
    u16(restart, 0x8899);
    restart.push_back(std::byte{'K'});
    restart.push_back(std::byte{0});
    u16(restart, 0xffff);

    Recorder recorder;
    TriggerRuntime runtime(TrgFile::parse(trg_file({
        type6_node(links, 0, stream),
        restart,
        {std::byte{0xff}, std::byte{0}},
    })), recorder);
    runtime.build();
    runtime.pulse_node(0);
    assert(recorder.applied_restarts == std::vector<std::size_t>{1});
    const std::vector<std::pair<std::uint32_t, std::uint16_t>> expected_data{
        {0x44556677, 0x8899},
    };
    assert(recorder.applied_restart_data == expected_data);
}

void test_script_object_command() {
    std::vector<std::byte> restart;
    u16(restart, 8);
    u16(restart, 0);
    u32(restart, 1);
    u32(restart, 2);
    u32(restart, 3);
    u32(restart, 0);
    u16(restart, 0);
    restart.push_back(std::byte{'S'});
    restart.push_back(std::byte{0});
    u16(restart, 0x00ab);
    u16(restart, 0); // absolute alignment pad before the u32 operand
    u32(restart, 0xf6e32d0e);
    u16(restart, 0x1111);
    u16(restart, 0x2222);
    u16(restart, 0x3333);
    u16(restart, 0xffff);

    Recorder recorder;
    TriggerRuntime runtime(TrgFile::parse(trg_file({
        restart,
        {std::byte{0xff}, std::byte{0}},
    })), recorder);
    runtime.build();
    runtime.execute_restart(0);

    const std::vector<std::tuple<std::size_t, std::uint32_t, std::array<std::uint16_t, 3>>> expected{
        {0, 0xf6e32d0e, {0x1111, 0x2222, 0x3333}},
    };
    assert(recorder.script_objects == expected);
}

void test_load_ai_reports_retail_unsupported_diagnostic() {
    std::vector<std::byte> stream;
    u16(stream, 0x00a2);
    stream.push_back(std::byte{'A'});
    stream.push_back(std::byte{'I'});
    stream.push_back(std::byte{0});
    // read_string() advances to the next even absolute byte before the
    // following command word.
    stream.push_back(std::byte{0});
    u16(stream, 0xffff);

    Recorder recorder;
    TriggerRuntime runtime(TrgFile::parse(trg_file({
        type6_node({}, 0, stream),
        {std::byte{0xff}, std::byte{0}},
    })), recorder);
    runtime.build();
    runtime.pulse_node(0);

    const std::vector<std::string> expected_diagnostics{
        "LoadAI command not supported",
    };
    assert(recorder.diagnostics == expected_diagnostics);
}

void test_type10_type11_runtime_list_state() {
    // With two nodes the first node begins at absolute offset 20.  The
    // retail type-10/type-11 position rule aligns (node + link_words + 7)
    // down to absolute offset 24, then reads the trailing flag word after
    // the three u32 coordinates.
    std::vector<std::byte> trigger;
    u16(trigger, 10);
    u16(trigger, 0);
    u32(trigger, 0x1000);
    u32(trigger, 0x2000);
    u32(trigger, 0x3000);
    u16(trigger, 0x0042);

    Recorder recorder;
    TriggerRuntime runtime(TrgFile::parse(trg_file({trigger, {std::byte{0xff}, std::byte{0}}})), recorder);
    runtime.build();
    assert(recorder.special_states.size() == 1);
    assert(std::get<0>(recorder.special_states[0]) == 0);
    assert(std::get<1>(recorder.special_states[0]) == 10);
    assert(std::get<2>(recorder.special_states[0]) == 0x0042);
    const std::array<std::int32_t, 3> expected_position{
        0x1000 << 12,
        0x2000 << 12,
        0x3000 << 12,
    };
    assert(std::get<3>(recorder.special_states[0]) == expected_position);
    assert(runtime.file().node_trigger_flags(0) == 0x0042);

    runtime.pulse_node(0);
    assert(recorder.node_pulses == std::vector<std::size_t>{0});
}

void test_restart_view_and_autoexec() {
    std::vector<std::byte> restart;
    u16(restart, 8);
    u16(restart, 0);
    u32(restart, 1);
    u32(restart, 2);
    u32(restart, 3);
    u32(restart, 0x11223344);
    u16(restart, 0x5566);
    restart.push_back(std::byte{'R'});
    restart.push_back(std::byte{0});
    u16(restart, 0xffff);
    const std::vector<std::byte> file_bytes = trg_file({
        restart,
        {std::byte{0xff}, std::byte{0}},
    });
    Recorder recorder;
    TriggerRuntime runtime(TrgFile::parse(file_bytes), recorder);
    runtime.build();
    const std::vector<std::pair<std::size_t, std::string>> expected_restarts{{0, "R"}};
    assert(recorder.restarts == expected_restarts);
    const std::vector<std::pair<std::uint32_t, std::uint16_t>> expected_restart_data{{0x11223344, 0x5566}};
    assert(recorder.restart_data == expected_restart_data);
}

void test_unknown_opcode_matches_retail_fallthrough() {
    std::vector<std::byte> stream;
    u16(stream, 0x1234);
    u16(stream, 0x0086);
    u16(stream, 1);
    u16(stream, 0xffff);
    const std::vector<std::byte> file_bytes = trg_file({type6_node({}, 0, stream), {std::byte{0xff}, std::byte{0}}});
    Recorder recorder;
    TriggerRuntime runtime(TrgFile::parse(file_bytes), recorder);
    runtime.build();
    runtime.pulse_node(0);
    const std::vector<std::tuple<std::uint16_t, std::uint32_t, std::size_t>> expected_unknown{
        {0x1234, 28, 0},
    };
    assert(recorder.unknown_commands == expected_unknown);
    assert(!recorder.diagnostics.empty());
    assert(runtime.command_point(0)->initialized == 1);
}

} // namespace

int main() {
    test_visible_pulse_and_record_layout();
    test_c9_alignment_and_gap_dispatch();
    test_initial_pulses_and_timer();
    test_conditional_skip_consumes_music_and_sound_operands();
    test_conditional_skip_consumes_a7_pair();
    test_restart_selection_and_checksum_lookup();
    test_two_player_restart_selection_command();
    test_kill_bruce_applies_linked_restart();
    test_script_object_command();
    test_load_ai_reports_retail_unsupported_diagnostic();
    test_type10_type11_runtime_list_state();
    test_restart_view_and_autoexec();
    test_unknown_opcode_matches_retail_fallthrough();
    std::cout << "TRG runtime tests passed\n";
}
