#pragma once

#include "trg_runtime.hpp"
#include "gap_table.hpp"
#include "../assets/psx_asset.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace opentony::trg {

enum class TriggerObjectKind : std::uint8_t {
    Object,
    Pickup,
    LinkedObject,
    Special,
    CommandPoint,
};

enum class TriggerSpawnFamily : std::uint8_t {
    Unknown,
    ObjectCb,
    Object192,
    SpecialVehicle,
    Pickup,
};

struct TriggerObjectState {
    std::size_t node{};
    std::uint16_t node_type{};
    std::uint16_t subtype{};
    std::array<std::int32_t, 3> position{};
    std::array<std::uint16_t, 3> orientation{};
    bool has_position{};
    bool has_orientation{};
    // Raw option bytes scanned by FUN_004c5460 before factory creation.
    std::vector<std::uint8_t> spawn_options;
    // Exact node bytes handed to the retail object constructor. The type-192
    // constructor stores a cursor into this stream at +0x17c; retaining the
    // bytes keeps later model-selection replay possible without guessing a
    // second payload schema here.
    std::vector<std::byte> factory_node_bytes;
    bool has_spawn_option_2{};
    bool has_spawn_option_4{};
    bool factory_requires_environment_registration{};
    bool factory_clears_object_flag_2{};
    bool factory_sets_object_flag_4{};
    TriggerObjectKind kind{TriggerObjectKind::Object};
    TriggerSpawnFamily spawn_family{TriggerSpawnFamily::Unknown};
    std::string factory_resource;
    std::uint32_t factory_model_selector{};
    bool has_factory_model_selector{};
    std::uint32_t link_key{};
    // Type-10/type-11 runtime-list fields recovered from FUN_004aa8c0 and
    // FUN_004aa3c0.  The raw flag word is retained; state is the retail
    // object byte at +0x04, not a renamed gameplay assumption.
    std::uint16_t trigger_flags{};
    std::uint8_t trigger_state{};
    std::uint8_t trigger_mode{};
    bool has_trigger_runtime{};
    // Type-12/type-14 FUN_004bd760/FUN_004bdc40 record boundary. The native
    // state deliberately does not pretend to know the +0x14 asset pointer or
    // the player-owner byte yet.
    bool has_special_runtime{};
    bool special_runtime_active{};
    std::uint8_t special_runtime_owner{};
    std::uint32_t special_runtime_control{};
    bool has_special_runtime_context{};
    // FUN_004bdc40's verified writes to the resolved live asset. The native
    // model join still does not claim to be the retail heap pointer, so the
    // mutation is retained as a separate asset-side record.
    std::uint8_t special_asset_flags_or{};
    std::uint32_t special_asset_marker{};
    bool has_special_asset_state{};
    std::size_t asset_model_index{CommandPointRuntime::npos};
    std::uint32_t asset_model_name{};
    std::size_t asset_scene_instance_count{};
    std::size_t asset_scene_first_instance{CommandPointRuntime::npos};
    std::array<std::int32_t, 3> asset_scene_position{};
    bool has_asset_scene_position{};
    std::uint16_t identifier{0xffff};
    std::uint16_t flags{};
    bool active{true};
    bool suspended{};
    bool alive{true};
    bool killed{};
    bool visible_commanded{};
    std::uint64_t pulses{};
    std::uint64_t signals{};
    std::uint64_t suspend_activate_calls{};
    std::uint64_t kills{};
};

struct TriggerScriptObjectState {
    // This is the bounded representation of retail FUN_00401060's object:
    // the source command, the resolved/raw script key, the three raw u16
    // operands, and the object-list identifier used by 0x83/0x84.
    std::size_t source_node{CommandPointRuntime::npos};
    std::uint32_t script_key{};
    std::array<std::uint16_t, 3> parameters{};
    std::uint16_t identifier{0xffff};
    std::uint16_t flags{};
};

struct TriggerRestartState {
    std::size_t node{};
    std::string name;
    std::array<std::int32_t, 3> position{};
    std::uint32_t auxiliary{};
    std::uint16_t auxiliary_word{};
};

struct TriggerTimerState {
    std::uint64_t due_ms{};
    std::uint32_t duration_ms{};
    bool fired{};
};

struct TriggerGapState {
    std::uint32_t checksum{};
    std::uint16_t last_argument{};
    std::size_t last_source{CommandPointRuntime::npos};
    std::uint32_t seen{};
    bool completed{};
    bool definition_found{};
    std::uint16_t flags{};
    std::int16_t score{};
    std::string name;
    bool deferred{};
    bool awarded{};
    bool pulse_pending{};
};

struct TriggerResourceRequest {
    std::uint16_t mode{};
    std::string name;
};

struct TriggerPathState {
    std::uint16_t first{};
    std::uint16_t second{};
    std::vector<FixedPathRecord> records;
};

struct TriggerLegacyCommand {
    std::uint16_t opcode{};
    std::size_t source_node{};
    std::vector<std::byte> bytes;
};

struct TriggerUnknownCommand {
    std::uint16_t opcode{};
    std::uint32_t offset{};
    std::size_t source_node{};
    std::vector<std::byte> remaining;
};

struct TriggerCurrentObjectFields {
    bool has_4d4{};
    std::int32_t field_4d4{};
    bool has_4d8{};
    std::int32_t field_4d8{};
    bool has_4dc{};
    std::uint16_t field_4dc{};
    bool has_4de{};
    std::uint16_t field_4de{};
    bool has_434{};
    std::uint16_t field_434{};
    bool has_436{};
    std::uint16_t field_436{};
    bool has_504{};
    std::uint32_t field_504{};
    bool has_40c{};
    std::uint32_t field_40c{};
    bool has_410{};
    std::uint16_t field_410{};
    bool has_414{};
    std::int32_t field_414{};
    bool copied_3dc_from_3a4{};
};

struct TriggerCurrentSkaterFields {
    bool has_3198{};
    std::uint32_t field_3198{};
    bool has_319c{};
    std::uint32_t field_319c{};
};

struct TriggerSpecialRuntimeContext {
    // FUN_004bdc40 copies the retail player-selection globals into the
    // type-12/type-14 record at +0x0b and +0x0c. Their higher-level meaning is
    // still external to the trigger service, so retain the raw values.
    std::uint8_t owner{};
    std::uint32_t control{};
    bool configured{};
};

struct TriggerEvent {
    enum class Kind : std::uint8_t {
        Pulse,
        Suspend,
        Activate,
        Signal,
        Kill,
        Visible,
        ObjectFlag,
        RestartSelected,
        RestartApplied,
        TimerScheduled,
        TimerFired,
        TimerReset,
        LevelEventUpdated,
        GapSeen,
        GapCompleted,
    };

    Kind kind{};
    std::size_t source_node{CommandPointRuntime::npos};
    std::size_t target_node{CommandPointRuntime::npos};
    std::uint16_t opcode{};
    std::uint16_t value{};
    std::uint32_t checksum{};
};

struct TriggerFogState {
    std::uint16_t near_range{};
    std::uint16_t far_range{};
    std::uint16_t mode{};
    bool set{};
};

struct TriggerRestartApplication {
    std::size_t node{CommandPointRuntime::npos};
    std::array<std::int32_t, 3> position{};
    std::uint32_t auxiliary{};
    std::uint16_t auxiliary_word{};
    bool set{};
};

// A deterministic in-memory implementation of the script-facing gameplay
// boundary. It is intentionally not a renderer or physics object: it records
// the state that the recovered trigger helpers mutate, giving those systems a
// stable C++ contract and making event ordering testable before the remaining
// game systems exist.
class LevelTriggerState final : public TriggerServices {
public:
    LevelTriggerState() = default;

    void reset();
    void set_visible_mask(std::uint16_t mask) noexcept { visible_mask_ = mask; }
    void set_object_identifier(std::size_t node, std::uint16_t identifier);
    void set_gap_table(const GapTable* table) noexcept { gap_table_ = table; }
    // Bind the recovered TRG link-key namespace to PSX model-name hashes.
    // A scene file may place several instances of one model, so this records
    // the stable model identity and leaves instance selection to the scene
    // layer.
    void bind_psx_models(const assets::PsxArchive& archive);
    void set_special_runtime_context(
        std::uint8_t owner,
        std::uint32_t control) noexcept {
        special_runtime_context_ = TriggerSpecialRuntimeContext{owner, control, true};
    }
    void mark_gap_complete(std::uint32_t checksum);
    void mark_goal_complete(std::uint16_t goal, bool complete = true);
    void advance_time(std::uint32_t milliseconds);

    [[nodiscard]] std::uint64_t time_ms() const noexcept { return time_ms_; }
    [[nodiscard]] const TriggerObjectState* object(std::size_t node) const;
    [[nodiscard]] std::size_t bound_model_count() const noexcept {
        return bound_model_count_;
    }
    [[nodiscard]] std::size_t bound_scene_instance_count() const noexcept {
        return bound_scene_instance_count_;
    }
    [[nodiscard]] std::size_t bound_scene_position_count() const noexcept {
        return bound_scene_position_count_;
    }
    [[nodiscard]] const std::vector<TriggerObjectState>& objects() const noexcept { return objects_; }
    [[nodiscard]] const std::vector<TriggerScriptObjectState>& script_objects() const noexcept {
        return script_objects_;
    }
    [[nodiscard]] const std::vector<TriggerRestartState>& restarts() const noexcept { return restarts_; }
    [[nodiscard]] const std::vector<TriggerTimerState>& timers() const noexcept { return timers_; }
    [[nodiscard]] std::size_t timer_reset_requests() const noexcept {
        return timer_reset_requests_;
    }
    [[nodiscard]] std::uint32_t last_timer_request_ms() const noexcept {
        return last_timer_request_ms_;
    }
    [[nodiscard]] const std::vector<TriggerGapState>& gaps() const noexcept { return gaps_; }
    [[nodiscard]] const std::vector<TriggerResourceRequest>& resources() const noexcept { return resources_; }
    [[nodiscard]] const std::vector<TriggerPathState>& paths() const noexcept { return paths_; }
    [[nodiscard]] const std::vector<TriggerEvent>& events() const noexcept { return events_; }
    [[nodiscard]] const std::vector<TriggerLegacyCommand>& legacy_commands() const noexcept {
        return legacy_commands_;
    }
    [[nodiscard]] const std::vector<TriggerUnknownCommand>& unknown_commands() const noexcept {
        return unknown_commands_;
    }
    [[nodiscard]] const TriggerCurrentObjectFields& current_object_fields() const noexcept {
        return current_object_fields_;
    }
    [[nodiscard]] const TriggerCurrentSkaterFields& current_skater_fields() const noexcept {
        return current_skater_fields_;
    }
    [[nodiscard]] const TriggerSpecialRuntimeContext& special_runtime_context() const noexcept {
        return special_runtime_context_;
    }
    [[nodiscard]] const std::vector<DispatcherFieldWrite>& dispatcher_field_writes() const noexcept {
        return dispatcher_field_writes_;
    }
    [[nodiscard]] const std::vector<std::string>& diagnostics() const noexcept { return diagnostics_; }
    [[nodiscard]] const TriggerFogState& fog() const noexcept { return fog_; }
    [[nodiscard]] const TriggerRestartApplication& last_restart() const noexcept { return last_restart_; }
    [[nodiscard]] std::uint16_t music_track() const noexcept { return music_track_; }
    [[nodiscard]] std::int16_t sound_id() const noexcept { return sound_id_; }
    [[nodiscard]] std::uint8_t reverb_type() const noexcept { return reverb_type_; }
    [[nodiscard]] std::uint32_t script_value() const noexcept { return script_value_; }
    [[nodiscard]] std::uint32_t level_value() const noexcept { return level_value_; }
    [[nodiscard]] std::uint16_t initial_state() const noexcept { return initial_state_; }
    [[nodiscard]] std::string_view competition_name() const noexcept { return competition_name_; }
    [[nodiscard]] std::size_t selected_restart() const noexcept { return selected_restart_; }
    [[nodiscard]] std::size_t resource_flushes() const noexcept { return resource_flushes_; }
    [[nodiscard]] std::size_t level_event_updates() const noexcept { return level_event_updates_; }
    [[nodiscard]] bool level_event_initialized() const noexcept {
        return level_event_initialized_;
    }
    [[nodiscard]] std::uint32_t level_event_timer_value() const noexcept {
        return level_event_timer_value_;
    }
    [[nodiscard]] std::uint32_t level_event_mode_value() const noexcept {
        return level_event_mode_value_;
    }
    [[nodiscard]] bool secondary_turn_reset() const noexcept {
        return secondary_turn_reset_;
    }
    [[nodiscard]] std::uint16_t global_word(std::uint16_t opcode) const noexcept;
    [[nodiscard]] bool career_flag(std::uint16_t flag) const override;
    [[nodiscard]] bool goal_complete(std::uint16_t goal) const override;

    void on_object_node(std::size_t node) override;
    void on_object_node_data(std::size_t node, std::span<const std::byte>) override;
    void on_script_object(
        std::size_t source,
        std::uint32_t key,
        std::array<std::uint16_t, 3> parameters) override;
    void on_pickup_node(std::size_t node) override;
    void on_pickup_node_data(std::size_t node, std::span<const std::byte>) override;
    void on_spawn_node(
        std::size_t node,
        std::uint16_t type,
        std::uint16_t subtype,
        std::array<std::int32_t, 3> position,
        std::span<const std::byte>) override;
    void on_spawn_node_options(
        std::size_t node,
        std::uint16_t type,
        std::span<const std::uint8_t> options) override;
    void on_spawn_orientation(std::size_t node, std::array<std::uint16_t, 3> orientation) override;
    void on_special_node(std::size_t node, std::uint16_t type, std::span<const std::byte>) override;
    void on_special_node_state(
        std::size_t node,
        std::uint16_t type,
        std::uint16_t flags,
        std::array<std::int32_t, 3> position) override;
    void on_linked_node(
        std::size_t node,
        std::uint16_t type,
        std::uint32_t key,
        std::span<const std::byte>) override;
    void on_restart_node(std::size_t node, std::string_view name, std::array<std::int32_t, 3> position) override;
    void on_restart_node_data(std::size_t node, std::uint32_t auxiliary, std::uint16_t auxiliary_word) override;
    void on_node_pulse(std::size_t node) override;

    void on_suspend_activate(
        std::size_t source,
        std::uint16_t opcode,
        std::span<const std::uint16_t> links) override;
    void on_signal(std::size_t source, std::span<const std::uint16_t> links) override;
    void on_kill(
        std::size_t source,
        std::uint16_t opcode,
        std::span<const std::uint16_t> links) override;
    void on_visible(
        std::size_t source,
        std::uint16_t value,
        std::span<const std::uint16_t> links) override;
    void on_object_flag_by_id(std::uint16_t identifier, bool set) override;
    void on_global_word(std::uint16_t opcode, std::uint16_t value) override;
    void on_current_object_word(std::size_t source, std::uint16_t opcode, std::uint16_t value) override;
    void on_current_object_pair(
        std::size_t source,
        std::uint16_t opcode,
        std::uint16_t first,
        std::uint16_t second) override;
    void on_current_object_copy(std::size_t source, std::uint16_t opcode) override;
    void on_current_skater_word(std::size_t source, std::uint16_t opcode, std::uint16_t value) override;

    void on_fog_range(std::uint16_t near_range, std::uint16_t far_range, std::uint16_t mode) override;
    void on_music(std::int16_t track) override;
    void on_sound(std::int16_t sound) override;
    void on_resource(std::uint16_t mode, std::string_view name) override;
    void on_flush_resources() override;
    void on_fixed_path(
        std::uint16_t first,
        std::uint16_t second,
        std::span<const FixedPathRecord> records) override;
    void on_fixed_path_records(std::span<const FixedPathRecord> records) override;
    void on_competition_name(std::string_view name) override;
    void on_restart_selected(std::uint16_t opcode, std::size_t node, std::string_view) override;
    void on_apply_restart(std::size_t node, std::array<std::int32_t, 3> position) override;
    void on_apply_restart_data(std::size_t node, std::uint32_t auxiliary, std::uint16_t auxiliary_word) override;
    void on_initial_state(std::uint16_t state) override;
    void on_timer(std::uint32_t milliseconds) override;
    void on_reverb_type(std::uint8_t type) override;
    void on_level_event_state() override;
    void on_script_value(std::uint32_t value) override;
    void on_level_value(std::uint32_t value) override;
    void on_gap(std::size_t source, std::uint32_t checksum, std::uint16_t argument) override;
    [[nodiscard]] bool take_gap_pulse(
        std::uint32_t checksum,
        std::uint16_t argument) override;
    void set_career_flag(std::uint16_t flag) override;
    void on_legacy_command(
        std::uint16_t opcode,
        std::span<const std::byte> bytes,
        std::size_t source) override;
    void on_unknown_command(
        std::uint16_t opcode,
        std::uint32_t offset,
        std::size_t source,
        std::span<const std::byte> remaining) override;
    void on_diagnostic(std::string_view message) override;

private:
    std::uint64_t time_ms_{};
    std::uint16_t visible_mask_{0x41};
    std::vector<TriggerObjectState> objects_;
    std::vector<TriggerScriptObjectState> script_objects_;
    std::vector<TriggerRestartState> restarts_;
    std::vector<TriggerTimerState> timers_;
    std::vector<TriggerGapState> gaps_;
    std::vector<TriggerResourceRequest> resources_;
    std::vector<TriggerPathState> paths_;
    std::vector<FixedPathRecord> legacy_path_records_;
    std::vector<TriggerEvent> events_;
    std::vector<TriggerLegacyCommand> legacy_commands_;
    std::vector<TriggerUnknownCommand> unknown_commands_;
    std::vector<DispatcherFieldWrite> dispatcher_field_writes_;
    std::vector<std::string> diagnostics_;
    std::array<std::uint16_t, 256> global_words_{};
    std::array<bool, 256> has_global_word_{};
    std::array<bool, 8> career_flags_{};
    std::array<bool, 11> goals_{};
    TriggerFogState fog_{};
    TriggerRestartApplication last_restart_{};
    std::uint16_t music_track_{};
    std::int16_t sound_id_{};
    std::uint8_t reverb_type_{};
    std::uint32_t script_value_{};
    std::uint32_t level_value_{};
    std::uint16_t initial_state_{};
    std::string competition_name_;
    TriggerCurrentObjectFields current_object_fields_{};
    TriggerCurrentSkaterFields current_skater_fields_{};
    TriggerSpecialRuntimeContext special_runtime_context_{};
    std::size_t selected_restart_{CommandPointRuntime::npos};
    std::size_t resource_flushes_{};
    std::size_t timer_reset_requests_{};
    std::uint32_t last_timer_request_ms_{};
    std::size_t bound_model_count_{};
    std::size_t bound_scene_instance_count_{};
    std::size_t bound_scene_position_count_{};
    std::size_t level_event_updates_{};
    bool level_event_initialized_{};
    std::uint32_t level_event_timer_value_{};
    std::uint32_t level_event_mode_value_{};
    bool secondary_turn_reset_{};
    const GapTable* gap_table_{};

    TriggerObjectState* find_object(std::size_t node);
    const TriggerObjectState* find_object(std::size_t node) const;
    TriggerObjectState& ensure_object(
        std::size_t node,
        std::uint16_t type,
        TriggerObjectKind kind,
        std::uint32_t key = 0);
    TriggerObjectState* find_identifier(std::uint16_t identifier);
    TriggerScriptObjectState* find_script_identifier(std::uint16_t identifier);
    void record_target_event(
        TriggerEvent::Kind kind,
        std::size_t source,
        std::size_t target,
        std::uint16_t opcode = 0,
        std::uint16_t value = 0);
    void copy_path_records(std::vector<FixedPathRecord>& destination, std::span<const FixedPathRecord> records);
};

} // namespace opentony::trg
