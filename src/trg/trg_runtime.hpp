#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace opentony::trg {

class FormatError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

struct NodeView {
    std::size_t index{};
    std::uint16_t type{};
    std::uint32_t offset{};
    std::uint32_t size{};
};

struct ScriptView {
    std::span<const std::byte> bytes{};
    std::uint32_t offset{};
};

struct FixedPathRecord {
    std::array<std::int32_t, 6> raw{};

    std::array<std::int32_t, 6> fixed12() const;
};

struct ScriptObjectCommand {
    // FUN_00401060 receives the aligned u32 key and a pointer to the three
    // following u16 values. The constructor uses the key; the PC build does
    // not read the three values, so keep them raw until another caller gives
    // them a stronger meaning.
    std::uint32_t key{};
    std::array<std::uint16_t, 3> parameters{};
};

struct DispatcherFieldWrite {
    std::size_t source_node{};
    std::uint16_t opcode{};
    std::array<std::uint16_t, 2> operands{};
    std::uint8_t operand_count{};
};

class TrgFile {
public:
    static TrgFile parse(std::span<const std::byte> bytes);
    static TrgFile load(const std::string& path);

    [[nodiscard]] std::uint32_t version() const noexcept { return version_; }
    [[nodiscard]] std::span<const std::byte> bytes() const noexcept { return backing_; }
    [[nodiscard]] const std::vector<NodeView>& nodes() const noexcept { return nodes_; }
    [[nodiscard]] const NodeView& node(std::size_t index) const;
    [[nodiscard]] std::span<const std::byte> node_bytes(std::size_t index) const;
    [[nodiscard]] std::uint16_t node_subtype(std::size_t index) const;
    // Type-1/type-7 object factories scan the byte list after the link words
    // for option values before the fixed-point position. Preserve that list
    // exactly; FUN_004c5460 gives values 2 and 4 distinct flag semantics.
    [[nodiscard]] std::vector<std::uint8_t> node_spawn_options(std::size_t index) const;
    [[nodiscard]] std::array<std::int32_t, 3> node_position(std::size_t index) const;
    // Type-10/type-11 constructors read the u16 immediately after their
    // fixed-point position triplet through FUN_004a9f70.  Keep that raw word
    // available instead of folding it into a guessed gameplay meaning.
    [[nodiscard]] std::uint16_t node_trigger_flags(std::size_t index) const;
    [[nodiscard]] std::array<std::uint16_t, 3> node_orientation(std::size_t index) const;

    // The retail loader uses this conditional 2-byte alignment for type-6
    // command-point payloads. The result is an offset within the file.
    [[nodiscard]] std::uint32_t node_link_key_offset(std::size_t index) const;
    [[nodiscard]] std::uint32_t command_point_key_offset(std::size_t index) const;
    [[nodiscard]] std::uint32_t command_point_checksum(std::size_t index) const;
    [[nodiscard]] ScriptView script(std::size_t index) const;
    [[nodiscard]] std::vector<std::uint16_t> links(std::size_t index) const;
    [[nodiscard]] std::string restart_name(std::size_t index) const;
    [[nodiscard]] std::array<std::int32_t, 3> restart_position(std::size_t index) const;
    [[nodiscard]] std::uint32_t restart_auxiliary(std::size_t index) const;
    [[nodiscard]] std::uint16_t restart_auxiliary_word(std::size_t index) const;

private:
    std::vector<std::byte> backing_;
    std::uint32_t version_{};
    std::vector<NodeView> nodes_;

    TrgFile(std::vector<std::byte> backing, std::uint32_t version, std::vector<NodeView> nodes);
};

class CommandCursor {
public:
    CommandCursor(std::span<const std::byte> bytes, std::uint32_t base_offset, std::size_t begin, std::size_t end);

    [[nodiscard]] bool at_end() const noexcept { return position_ >= end_; }
    [[nodiscard]] std::size_t position() const noexcept { return position_; }
    [[nodiscard]] std::uint32_t absolute_position() const noexcept;
    [[nodiscard]] std::uint16_t peek_u16() const;
    [[nodiscard]] std::uint16_t read_u16();
    [[nodiscard]] std::uint32_t read_u32();
    [[nodiscard]] std::string read_string();
    [[nodiscard]] std::array<std::uint32_t, 3> read_u16_triplet();
    [[nodiscard]] ScriptObjectCommand read_script_object_operands(std::size_t opcode_offset);
    [[nodiscard]] std::vector<FixedPathRecord> read_fixed_path_records(std::size_t first_record);
    [[nodiscard]] std::pair<std::uint32_t, std::uint16_t> read_gap_operands(std::size_t opcode_offset);
    [[nodiscard]] std::span<const std::byte> raw(std::size_t begin) const;

    // Consume the operands for an opcode whose opcode word has already been
    // read. Unknown opcodes throw instead of guessing a width and desyncing.
    void skip_operands(std::uint16_t opcode, std::size_t opcode_offset);

private:
    std::span<const std::byte> bytes_;
    std::uint32_t base_offset_{};
    std::size_t position_{};
    std::size_t end_{};

    void require(std::size_t size, std::string_view what) const;
    void set_position(std::size_t position);
    void skip_fixed_records(std::size_t first_record);
    [[nodiscard]] std::size_t align_down4_absolute(std::size_t relative) const;
};

struct CommandPointRuntime {
    static constexpr std::size_t npos = static_cast<std::size_t>(-1);

    std::span<const std::byte> stream{};
    std::uint8_t state0{};
    std::uint8_t state1{};
    std::uint8_t initialized{};
    std::uint8_t pulse_count{};
    std::uint16_t state{};
    std::uint16_t source_node{};
    std::uint32_t checksum{};
    std::size_t bucket_next{npos};
    std::size_t all_next{npos};
};

class TriggerServices {
public:
    virtual ~TriggerServices() = default;

    virtual void on_object_node(std::size_t) {}
    virtual void on_object_node_data(std::size_t, std::span<const std::byte>) {}
    virtual void on_script_object(
        std::size_t,
        std::uint32_t,
        std::array<std::uint16_t, 3>) {}
    virtual void on_pickup_node(std::size_t) {}
    virtual void on_pickup_node_data(std::size_t, std::span<const std::byte>) {}
    virtual void on_spawn_node(
        std::size_t,
        std::uint16_t,
        std::uint16_t,
        std::array<std::int32_t, 3>,
        std::span<const std::byte>) {}
    virtual void on_spawn_node_options(
        std::size_t,
        std::uint16_t,
        std::span<const std::uint8_t>) {}
    virtual void on_spawn_orientation(std::size_t, std::array<std::uint16_t, 3>) {}
    virtual void on_special_node(
        std::size_t,
        std::uint16_t,
        std::span<const std::byte>) {}
    virtual void on_special_node_state(
        std::size_t,
        std::uint16_t,
        std::uint16_t,
        std::array<std::int32_t, 3>) {}
    virtual void on_special_node_links(
        std::size_t,
        std::span<const std::uint16_t>) {}
    virtual void on_special_node_aliases_complete() {}
    virtual void on_special_runtime_links(
        std::size_t,
        std::uint16_t,
        std::span<const std::uint16_t>) {}
    virtual void on_unhandled_node(
        std::size_t,
        std::uint16_t,
        std::span<const std::byte>) {}
    virtual void on_linked_node(
        std::size_t,
        std::uint16_t,
        std::uint32_t,
        std::span<const std::byte>) {}
    virtual void on_restart_node(std::size_t, std::string_view, std::array<std::int32_t, 3>) {}
    virtual void on_restart_node_data(std::size_t, std::uint32_t, std::uint16_t) {}
    virtual void on_node_pulse(std::size_t) {}
    // FUN_004bdbd0 traverses a type-12/type-14 record's links only when its
    // game-mode/policy gate is open. The runtime owns the traversal and asks
    // the level service for that recovered policy.
    [[nodiscard]] virtual bool should_traverse_special_runtime_links(std::size_t) const {
        return false;
    }

    virtual void on_suspend_activate(
        std::size_t,
        std::uint16_t,
        std::span<const std::uint16_t>) {}
    virtual void on_signal(std::size_t, std::span<const std::uint16_t>) {}
    virtual void on_kill(std::size_t, std::uint16_t, std::span<const std::uint16_t>) {}
    virtual void on_visible(
        std::size_t,
        std::uint16_t,
        std::span<const std::uint16_t>) {}
    virtual void on_object_flag_by_id(std::uint16_t, bool) {}
    virtual void on_global_word(std::uint16_t, std::uint16_t) {}
    // Preserve dispatcher helpers that write fields on the current level
    // object/skater. The receiver owns the current-object identity; these
    // callbacks carry only verified raw operands and source ordering.
    virtual void on_current_object_word(std::size_t, std::uint16_t, std::uint16_t) {}
    virtual void on_current_object_pair(std::size_t, std::uint16_t, std::uint16_t, std::uint16_t) {}
    virtual void on_current_object_copy(std::size_t, std::uint16_t) {}
    virtual void on_current_skater_word(std::size_t, std::uint16_t, std::uint16_t) {}

    virtual void on_fog_range(std::uint16_t, std::uint16_t, std::uint16_t) {}
    virtual void on_music(std::int16_t) {}
    virtual void on_sound(std::int16_t) {}
    virtual void on_resource(std::uint16_t, std::string_view) {}
    virtual void on_flush_resources() {}
    virtual void on_fixed_path(
        std::uint16_t,
        std::uint16_t,
        std::span<const FixedPathRecord>) {}
    virtual void on_fixed_path_records(std::span<const FixedPathRecord>) {}
    virtual void on_competition_name(std::string_view) {}
    virtual void on_restart_selected(std::uint16_t, std::size_t, std::string_view) {}
    virtual void on_apply_restart(std::size_t, std::array<std::int32_t, 3>) {}
    virtual void on_apply_restart_data(std::size_t, std::uint32_t, std::uint16_t) {}
    virtual void on_initial_state(std::uint16_t) {}
    virtual void on_timer(std::uint32_t) {}
    // Opcode 0x98 announces KILLBRUCE; TriggerRuntime separately forwards
    // its first linked type-8 restart through on_apply_restart/data after
    // validating the retail link shape.
    virtual void on_kill_bruce_restart(std::size_t) {}
    virtual void on_reverb_type(std::uint8_t) {}
    virtual void on_level_event_state() {}
    virtual void on_script_value(std::uint32_t) {}
    virtual void on_level_value(std::uint32_t) {}
    virtual void on_cheat_restart_strings(std::span<const std::string>) {}
    virtual void on_legacy_command(
        std::uint16_t,
        std::span<const std::byte>,
        std::size_t) {}
    virtual void on_unknown_command(
        std::uint16_t,
        std::uint32_t,
        std::size_t,
        std::span<const std::byte>) {}

    virtual void on_gap(
        std::size_t,
        std::uint32_t,
        std::uint16_t) {}
    // A normal retail gap completion sends the source command point's links
    // immediately. Stateful services can consume that one-shot pulse here.
    [[nodiscard]] virtual bool take_gap_pulse(std::uint32_t, std::uint16_t) { return false; }
    virtual void set_career_flag(std::uint16_t) {}
    [[nodiscard]] virtual bool career_flag(std::uint16_t) const { return false; }
    [[nodiscard]] virtual bool goal_complete(std::uint16_t) const { return false; }

    virtual void on_diagnostic(std::string_view) {}
};

class TriggerRuntime {
public:
    explicit TriggerRuntime(TrgFile file, TriggerServices& services);

    // Matches the retail load order: execute autoexec before constructing
    // object/command-point runtime records.
    void initialize(bool two_player = false);
    void build();
    void run_autoexec(bool two_player = false);
    void pulse_node(std::size_t node_index);
    void execute_restart(std::size_t node_index);

    [[nodiscard]] const TrgFile& file() const noexcept { return file_; }
    [[nodiscard]] const std::vector<CommandPointRuntime>& command_points() const noexcept {
        return command_points_;
    }
    [[nodiscard]] const CommandPointRuntime* command_point(std::size_t node_index) const;
    [[nodiscard]] const CommandPointRuntime* command_point_by_checksum(std::uint32_t checksum) const;
    [[nodiscard]] std::size_t find_restart_by_name(std::string_view name) const;
    [[nodiscard]] std::size_t selected_restart() const noexcept { return selected_restart_; }

private:
    TrgFile file_;
    TriggerServices& services_;
    std::vector<CommandPointRuntime> command_points_;
    std::vector<std::size_t> command_point_by_node_;
    std::size_t all_command_points_{CommandPointRuntime::npos};
    std::size_t selected_restart_{CommandPointRuntime::npos};
    std::array<std::size_t, 256> bucket_heads_{};
    bool two_player_mode_{};
    bool special_runtime_pulse_guard_{};

    void create_command_point(std::size_t node_index, std::span<const std::byte> stream);
    void dispatch(
        std::span<const std::byte> stream,
        std::uint32_t stream_offset,
        std::size_t source_node,
        CommandPointRuntime* command_point,
        bool resource_flush);
    void pulse_links(std::size_t node_index);
    void skip_to_endif(CommandCursor& cursor);
    [[nodiscard]] std::size_t find_autoexec(bool two_player) const;
};

} // namespace opentony::trg
