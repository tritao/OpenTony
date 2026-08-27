#pragma once

#include "trg_runtime.hpp"
#include "gap_table.hpp"
#include "../assets/psx_asset.hpp"
#include "../assets/psx_catalog.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
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

// The retail factory inserts these object families into different intrusive
// lists.  Keep the ownership boundary as a semantic value; the retail list
// head addresses are process-local and are not useful native pointers.
enum class TriggerFactoryList : std::uint8_t {
    None,
    CommonObject, // FUN_00403000 and FUN_00412640 -> 0x55f6bc
    Object192,    // FUN_0049f250 -> 0x56af40
    Pickup,       // FUN_004a7c50 -> 0x56b830
};

enum class TriggerSpecialRuntimeList : std::uint8_t {
    None,
    Type10Type11, // FUN_004aa8c0 -> DAT_0056b860
    Type12Type14, // FUN_004bd760 -> DAT_0056db90
};

enum class TriggerSpecialAnimationMode : std::uint8_t {
    Disabled,
    PaletteTriangle, // FUN_004bdd00 when DAT_0056db8c != 0
    CompactTriangle, // FUN_004bdd00's game-mode-8 branch
};

struct TriggerSpatialBounds {
    // Coordinates are the Q12 values returned by FUN_004c8650.  The retail
    // type-10/type-11 constructor expands these with the selected eligible
    // alias target; larger geometry/update behavior remains separate.
    std::array<std::int32_t, 3> minimum{};
    std::array<std::int32_t, 3> maximum{};
    bool valid{};
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
    // Relative offset of the post-position/orientation cursor saved by the
    // retail object at +0x17c. It is meaningful only with factory_node_bytes.
    std::uint32_t factory_cursor_offset{};
    bool has_factory_cursor_offset{};
    bool has_spawn_option_2{};
    bool has_spawn_option_4{};
    bool factory_requires_environment_registration{};
    bool factory_clears_object_flag_2{};
    bool factory_sets_object_flag_4{};
    TriggerObjectKind kind{TriggerObjectKind::Object};
    TriggerSpawnFamily spawn_family{TriggerSpawnFamily::Unknown};
    // Factory allocation/vtable/list facts recovered at the runtime-object
    // boundary. These are metadata for faithful recreation, not native heap
    // layout claims.
    std::uint32_t factory_allocation_size{};
    std::uint32_t factory_vtable{};
    TriggerFactoryList factory_list{TriggerFactoryList::None};
    std::uint8_t factory_initial_activation_byte{};
    bool has_factory_initial_activation_byte{};
    std::string factory_resource;
    std::uint32_t factory_model_selector{};
    bool has_factory_model_selector{};
    // Type-5 pickup constructor selection. These fields correspond to the
    // retail resource region and model-name lookup, not to a guessed render
    // object or collision shape.
    std::string pickup_resource;
    std::uint32_t pickup_model_checksum{};
    std::size_t pickup_model_index{CommandPointRuntime::npos};
    bool pickup_model_resolved{};
    // Confirmed powerup object lifecycle bytes from FUN_004a7c50 /
    // FUN_004a8ac0. Motion remains raw because its frame/random producers are
    // not yet owned by this service.
    std::uint8_t pickup_visual_state_d1{};
    std::uint8_t pickup_motion_state_d2{};
    std::uint8_t pickup_motion_substate_d3{};
    std::array<std::int16_t, 3> pickup_motion_words_14_18{};
    std::array<std::int16_t, 3> pickup_motion_words_70_74{};
    std::int32_t pickup_motion_speed_scale_q8{};
    bool has_pickup_motion_inputs{};
    // Explicit raw lifecycle inputs from FUN_004a7c50/FUN_004a8620. The
    // constructor's producer for +0xf0 is not implied by a TRG subtype, so
    // these stay opt-in until that caller is recovered.
    std::uint16_t pickup_timer_f0{0xffff};
    std::uint16_t pickup_phase_ea{0x0032};
    std::uint16_t pickup_phase_ec{0x0032};
    std::uint8_t pickup_global_fade_flags{};
    bool has_pickup_lifecycle_inputs{};
    bool pickup_glow_present{};
    std::uint64_t pickup_update_calls{};
    std::uint32_t link_key{};
    // Type-10/type-11 runtime-list fields recovered from FUN_004aa8c0 and
    // FUN_004aa3c0.  The raw flag word is retained; state is the retail
    // object byte at +0x04, not a renamed gameplay assumption.
    std::uint16_t trigger_flags{};
    std::uint8_t trigger_state{};
    std::uint8_t trigger_mode{};
    bool has_trigger_runtime{};
    std::vector<std::uint16_t> trigger_links;
    std::vector<std::size_t> trigger_alias_nodes;
    // FUN_004aa4b0 stores a 16-bit group index beside each eligible
    // type-10/type-11 node. This is the semantic value written into the
    // retail alias table, not a native pointer or a gameplay label.
    std::uint16_t trigger_alias_group{};
    bool has_trigger_alias_group{};
    TriggerSpatialBounds trigger_bounds;
    std::uint32_t special_runtime_allocation_size{};
    std::uint32_t special_runtime_vtable{};
    TriggerSpecialRuntimeList special_runtime_list{
        TriggerSpecialRuntimeList::None};
    // Type-12/type-14 FUN_004bd760/FUN_004bdc40 record boundary. The native
    // state deliberately does not pretend to know the +0x14 asset pointer or
    // the player-owner byte yet.
    bool has_special_runtime{};
    bool special_runtime_active{};
    std::uint8_t special_runtime_owner{};
    std::uint32_t special_runtime_control{};
    bool has_special_runtime_context{};
    // Raw node IDs traversed by FUN_004bdbd0 after the type-12/type-14
    // record's live asset is resolved. The native policy layer consumes this
    // list later; it is not folded into the link key.
    std::vector<std::uint16_t> special_runtime_links;
    // FUN_004bdc40's verified writes to the resolved live asset. The native
    // model join still does not claim to be the retail heap pointer, so the
    // mutation is retained as a separate asset-side record.
    std::uint8_t special_asset_flags_or{};
    std::uint32_t special_asset_marker{};
    bool has_special_asset_state{};
    // FUN_004bdd00 writes the live asset's +0x24 field. The mask is kept as
    // an explicit input because retail derives it from the heap record's
    // address; a native pointer must not be invented from a TRG node index.
    std::uint32_t special_asset_palette_mask{};
    bool has_special_asset_palette_mask{};
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
    // Type-12/type-14 nodes allocate a distinct 0x18-byte trick-object
    // record. Process-local vtable/list pointers are left zero; the raw
    // checksum, source index, active byte, and owner byte are synchronized
    // at their proven offsets.
    std::array<std::byte, 0x18> special_runtime_record{};

    [[nodiscard]] std::span<const std::byte> raw_special_record() const noexcept {
        return special_runtime_record;
    }
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
    // Byte-preserving native image of the retail 0xcc-byte allocation. The
    // vtable and intrusive links remain zero because they are process-local
    // retail pointers; the proven flag and list-identifier offsets are kept
    // synchronized with the semantic fields above.
    std::array<std::byte, 0xcc> runtime_record{};

    [[nodiscard]] std::span<const std::byte> raw_record() const noexcept {
        return runtime_record;
    }
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
    // The two bits are retained separately because the retail player
    // dispatcher consumes +0x3014 and +0x3018 in different physics states.
    bool deferred_field_3014{};
    bool deferred_field_3018{};
    bool awarded{};
    bool pulse_pending{};
};

enum class TriggerDeferredGapSlot : std::uint16_t {
    field_3014 = 0x3014,
    field_3018 = 0x3018,
};

struct TriggerDeferredGapHandoff {
    std::size_t source_node{CommandPointRuntime::npos};
    std::uint32_t checksum{};
    std::uint16_t argument{};
    std::uint16_t flags{};
    std::int16_t score{};
    TriggerDeferredGapSlot slot{TriggerDeferredGapSlot::field_3018};
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

// Inputs consumed by LevelEvent_InitializeAndDispatch (0x00466c10).  These
// names intentionally retain the observed global/field role rather than
// assigning a higher-level versus or score meaning to the values.
struct TriggerLevelEventInputs {
    std::uint32_t game_mode{}; // DAT_00533f38
    std::uint32_t versus_state{}; // DAT_0056db64
    bool mode9_side_flag{}; // DAT_006a3d48
    std::int32_t primary_compare_value{}; // primary player +0x2cdc
    std::int32_t secondary_compare_value{}; // secondary player +0x2cdc
};

// The gameplay update supplies these player-owned fields to the level-event
// service.  The trigger runtime does not manufacture player or camera
// pointers; it returns the writes/requests that the owning runtime must apply.
struct TriggerLevelEventFrameInput {
    bool players_eligible{};
    bool secondary_present{};
    bool secondary_eligible{};
    bool primary_state7{};
    bool secondary_state7{};
    std::uint16_t primary_animation_state{}; // skater +0xf6
    std::uint16_t secondary_animation_state{}; // secondary skater +0xf6
    bool primary_animation_flag_107{}; // skater +0x107
    bool secondary_animation_flag_107{}; // secondary skater +0x107
    bool mode7_input_active{};
    std::int32_t primary_pending_score{}; // skater +0x2a8
    std::int32_t secondary_pending_score{}; // secondary skater +0x2a8
    bool primary_score_input_active{};
    bool secondary_score_input_active{};
};

struct TriggerLevelEventFrameResult {
    bool primary_animation_started{};
    bool secondary_animation_started{};
    std::uint32_t primary_animation{};
    std::uint32_t secondary_animation{};
    std::size_t replay_reset_requests{};
    bool completion_reset_requested{};
    std::int32_t primary_score_committed{};
    std::int32_t secondary_score_committed{};
    std::int32_t primary_camera_delta{};
    std::int32_t secondary_camera_delta{};
};

// Raw mode-8/mode-9 writes from 0x00466c10.  The addresses are part of the
// evidence contract; semantic names for the counters remain unresolved.
struct TriggerLevelEventRawStats {
    std::int32_t word_0056b798{};
    std::int32_t word_0056b79c{};
    std::int32_t word_0056b7a0{};
    std::int32_t word_0056b7a4{};
    std::int32_t word_0056b7b0{};
    std::int32_t word_0056b7b4{};
    std::int32_t word_0056b7dc{};
    std::int32_t word_0056b7e0{};
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
    void resolve_pickup_assets(const assets::PsxAssetCatalog& catalog);
    void set_special_runtime_context(
        std::uint8_t owner,
        std::uint32_t control) noexcept {
        special_runtime_context_ = TriggerSpecialRuntimeContext{owner, control, true};
    }
    void set_special_runtime_animation_mode(
        TriggerSpecialAnimationMode mode) noexcept {
        special_runtime_animation_mode_ = mode;
    }
    // FUN_004bdbd0's linked traversal is gated by DAT_00533f38 == 8. Keep
    // the mode as an explicit level-service input instead of inferring it
    // from a TRG node or from the animation branch.
    void set_special_runtime_game_mode(std::uint32_t mode) noexcept {
        special_runtime_game_mode_ = mode;
    }
    void set_special_alias_mode_mask(std::uint32_t mask) noexcept {
        special_alias_mode_mask_ = mask;
    }
    void set_special_runtime_palette_mask(
        std::size_t node,
        std::uint32_t mask);
    void set_pickup_motion_inputs(
        std::size_t node,
        std::array<std::int16_t, 3> words_14_18,
        std::array<std::int16_t, 3> words_70_74,
        std::int32_t speed_scale_q8 = 0x100);
    void set_pickup_lifecycle_inputs(
        std::size_t node,
        std::uint16_t timer_f0,
        std::uint16_t phase_ea = 0x0032,
        std::uint16_t phase_ec = 0x0032,
        std::uint8_t global_fade_flags = 0);
    void mark_gap_complete(std::uint32_t checksum);
    // Completes the deferred definition selected by the live player physics
    // state. This is the native handoff for retail +0x3014/+0x3018; the
    // caller owns the source-node pulse after receiving the record.
    [[nodiscard]] std::optional<TriggerDeferredGapHandoff>
    complete_deferred_gap_for_physics_state(std::int32_t physics_state);
    void mark_goal_complete(std::uint16_t goal, bool complete = true);
    void set_level_event_inputs(TriggerLevelEventInputs inputs) noexcept {
        level_event_inputs_ = inputs;
    }
    void set_level_event_frame_input(TriggerLevelEventFrameInput input) noexcept {
        level_event_frame_input_ = input;
        has_level_event_frame_input_ = true;
    }
    [[nodiscard]] TriggerLevelEventFrameResult advance_level_event_frame();
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
    // LevelRuntime::initialize/reset is the proven native release boundary
    // for the 0xcc script-object records. The retail destructor internals are
    // not claimed; this count makes the teardown visible to deterministic
    // lifecycle tests.
    [[nodiscard]] std::size_t script_object_teardown_count() const noexcept {
        return script_object_teardown_count_;
    }
    [[nodiscard]] std::size_t teardown_script_objects() noexcept;
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
    [[nodiscard]] TriggerSpecialAnimationMode special_runtime_animation_mode() const noexcept {
        return special_runtime_animation_mode_;
    }
    [[nodiscard]] std::uint32_t special_runtime_game_mode() const noexcept {
        return special_runtime_game_mode_;
    }
    // DAT_0056e320 is the retail clock after conversion from elapsed time to
    // 60 Hz-style ticks. This is exposed for parity traces and tests.
    [[nodiscard]] std::uint32_t special_runtime_clock() const noexcept {
        return static_cast<std::uint32_t>((time_ms_ * 3U) / 50U);
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
    [[nodiscard]] bool level_event_flag() const noexcept {
        return level_event_flag_;
    }
    [[nodiscard]] const TriggerLevelEventRawStats& level_event_raw_stats() const noexcept {
        return level_event_raw_stats_;
    }
    [[nodiscard]] const TriggerLevelEventInputs& level_event_inputs() const noexcept {
        return level_event_inputs_;
    }
    [[nodiscard]] const TriggerLevelEventFrameResult& last_level_event_frame() const noexcept {
        return last_level_event_frame_;
    }
    [[nodiscard]] std::size_t level_event_camera_updates() const noexcept {
        return level_event_camera_updates_;
    }
    [[nodiscard]] std::int64_t level_event_primary_camera_delta() const noexcept {
        return level_event_primary_camera_delta_;
    }
    [[nodiscard]] std::int64_t level_event_secondary_camera_delta() const noexcept {
        return level_event_secondary_camera_delta_;
    }
    [[nodiscard]] std::size_t level_event_replay_reset_requests() const noexcept {
        return level_event_replay_reset_requests_;
    }
    [[nodiscard]] std::size_t level_event_completion_reset_requests() const noexcept {
        return level_event_completion_reset_requests_;
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
    void on_spawn_factory_cursor(std::size_t node, std::uint32_t offset) override;
    void on_spawn_orientation(std::size_t node, std::array<std::uint16_t, 3> orientation) override;
    void on_special_node(std::size_t node, std::uint16_t type, std::span<const std::byte>) override;
    void on_special_node_state(
        std::size_t node,
        std::uint16_t type,
        std::uint16_t flags,
        std::array<std::int32_t, 3> position) override;
    void on_special_node_links(
        std::size_t node,
        std::span<const std::uint16_t> links) override;
    void on_special_node_aliases_complete() override;
    void on_special_runtime_links(
        std::size_t node,
        std::uint16_t type,
        std::span<const std::uint16_t> links) override;
    [[nodiscard]] bool should_traverse_special_runtime_links(
        std::size_t node) const override;
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
        std::span<const std::uint16_t> targets) override;
    void on_signal(std::size_t source, std::span<const std::uint16_t> targets) override;
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
    std::size_t script_object_teardown_count_{};
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
    TriggerSpecialAnimationMode special_runtime_animation_mode_{
        TriggerSpecialAnimationMode::Disabled};
    std::uint32_t special_runtime_game_mode_{};
    std::uint32_t special_alias_mode_mask_{};
    std::size_t selected_restart_{CommandPointRuntime::npos};
    std::size_t resource_flushes_{};
    std::size_t timer_reset_requests_{};
    std::uint32_t last_timer_request_ms_{};
    std::size_t bound_model_count_{};
    std::size_t bound_scene_instance_count_{};
    std::size_t bound_scene_position_count_{};
    std::size_t level_event_updates_{};
    bool level_event_initialized_{};
    bool level_event_flag_{};
    std::uint32_t level_event_timer_value_{};
    std::uint32_t level_event_mode_value_{};
    bool secondary_turn_reset_{};
    TriggerLevelEventInputs level_event_inputs_{};
    TriggerLevelEventFrameInput level_event_frame_input_{};
    bool has_level_event_frame_input_{};
    TriggerLevelEventRawStats level_event_raw_stats_{};
    TriggerLevelEventFrameResult last_level_event_frame_{};
    std::size_t level_event_camera_updates_{};
    std::int64_t level_event_primary_camera_delta_{};
    std::int64_t level_event_secondary_camera_delta_{};
    std::size_t level_event_replay_reset_requests_{};
    std::size_t level_event_completion_reset_requests_{};
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
