#pragma once

#include "air_motion.hpp"
#include "fixed_step_driver.hpp"
#include "player_state.hpp"
#include "psx_collision_probe.hpp"
#include "action_sequence_builder.hpp"
#include "../camera/camera_runtime.hpp"
#include "../assets/tricks_bin.hpp"
#include "../trg/render_packet_builder.hpp"
#include "../trg/level_runtime.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace opentony::runtime {

// Configuration for the portable, renderer-independent gameplay shell. The
// values which are already recovered are enabled here; caller-specific
// surface policies remain explicit seams instead of being hidden in the
// application wrapper.
struct GameplaySessionConfig {
    FixedStepConfig fixed_step{};
    AirGravityConfig air_gravity{};
    bool apply_air_gravity{true};
    // Optional PC TRICKS.BIN image. When set, the session exposes the
    // archive-backed action source to PlayerPhysicsFrame. The source-table
    // fallback is intentionally opt-in because the retail per-player table
    // is synthesized in heap memory by FUN_004bd1e0.
    std::string tricks_path{};
    bool use_tricks_source_sequence_fallback{false};
    // Port the deterministic retail builder when the selected player-input
    // table and section-5 resource database are available. The normal player
    // slot supplies the selection view at +0xcc; its mapped ID/index pairs
    // are view-relative +0x2b/+0x30 (physical slot +0xf7/+0xfc). These bytes
    // are populated by the retail skater/resource setup path, so they remain
    // explicit until that path is connected.
    bool use_tricks_retail_builder{false};
    std::size_t tricks_player_index{0};
    std::array<std::uint8_t, 4> tricks_direct_resource_ids{
        0xff, 0xff, 0xff, 0xff};
    std::array<std::uint8_t, 5> tricks_mapped_resource_ids{
        0xff, 0xff, 0xff, 0xff, 0xff};
    std::array<std::uint8_t, 5> tricks_mapped_mapping_indices{
        0xff, 0xff, 0xff, 0xff, 0xff};
    assets::PsxCollisionQueryOptions collision_query_options{};
    bool apply_collision_response_bias{false};
    std::int32_t collision_response_bias_q12{0xcd};
    // Camera_Update is opt-in because the remaining tripod/collision
    // producers are caller-owned. When enabled, the session refreshes the
    // target position from PlayerState after each fixed gameplay step while
    // preserving the supplied raw camera inputs and hooks.
    bool update_camera{false};
    camera::CameraTargetRaw camera_target{};
    camera::CameraFollowInput camera_follow_input{};
    camera::Q16Vec3 camera_look_target_offset{};
    camera::CameraUpdateHooks camera_hooks{};
    camera::CameraModeInputRaw camera_mode_input{};
    camera::CameraMode25ProducerInputRaw camera_mode25_input{};
};

// Stable end-to-end observation for parity traces. It deliberately contains
// semantic counts and raw fixed-point player fields rather than pointers,
// addresses, or renderer-owned objects.
struct GameplaySessionObservation {
    GameplayFrameResult frame{};
    std::uint64_t level_time_ms{};
    std::size_t trigger_object_count{};
    std::size_t trigger_timer_count{};
    std::size_t trigger_gap_count{};
    std::size_t trigger_event_count{};
    std::size_t trigger_resource_count{};
    std::size_t scene_entity_count{};
    std::size_t scene_bound_trigger_count{};
    FixedPosition position{};
    FixedPosition previous_position{};
    FixedPosition collision_response{};
    FixedPosition motion_correction{};
    FixedPosition air_motion{};
    Q12Matrix3 orientation{};
    RetailBasis retail_basis{};
    std::int32_t physics_state{};
    std::int32_t ground_update_state{};
    std::int32_t ground_physics_mode{};
    ActionCommandRuntimeState action_command_state{};
    bool action_stream_active{};
    std::int16_t action_stream_relative{};
    std::size_t action_stream_cursor{};
    std::uint32_t restart_auxiliary{};
    std::uint16_t restart_auxiliary_word{};
    std::uint32_t camera_update_tick{};
    std::uint32_t camera_mode{};
    camera::Q4Vec3 camera_rendered_position{};
};

// Owns the level, player, recovered frame ordering, and fixed-step clock in
// one usable native runtime object. This is intentionally not a renderer or
// platform class: a frontend can poll devices, call advance(), and consume
// render_snapshot() without reaching into TRG or PSX offsets.
class GameplaySession final {
public:
    GameplaySession(
        const std::string& trg_path,
        const std::string& psx_path,
        const std::string& asset_root = {},
        PlayerState initial_player = PlayerState{},
        GameplaySessionConfig config = {});

    GameplaySession(const GameplaySession&) = delete;
    GameplaySession& operator=(const GameplaySession&) = delete;
    GameplaySession(GameplaySession&&) = delete;
    GameplaySession& operator=(GameplaySession&&) = delete;

    void initialize(
        bool two_player = false,
        const trg::GapTable* gap_table = nullptr);

    // Executes the TRG restart stream and applies the same selected restart
    // record to the native player state before the next fixed step.
    void execute_restart(std::size_t node);
    void execute_restart(std::string_view name);

    // Sends a gameplay event through the recovered TRG command dispatcher and
    // refreshes the level scene bindings. These are the public event boundary
    // for gap/object/script callers; callers do not need to reach into the
    // level runtime's command-point tables directly.
    void pulse_node(std::size_t node);
    void pulse_checksum(std::uint32_t checksum);

    [[nodiscard]] FixedStepAdvanceResult advance(
        std::uint32_t elapsed_ms,
        const DirectInputKeyboardState& keyboard,
        const InputBindings& bindings,
        LevelFrameObserver* observer = nullptr);

    // Direct action/device path for analog controllers and deterministic
    // replay. The axes remain raw until InputState applies the retail
    // threshold-to-action mapping.
    [[nodiscard]] FixedStepAdvanceResult advance(
        std::uint32_t elapsed_ms,
        std::uint16_t action_mask,
        std::int8_t horizontal_axis,
        std::int8_t vertical_axis,
        LevelFrameObserver* observer = nullptr);

    void reset_clock() noexcept { driver_.reset(); }

    [[nodiscard]] bool initialized() const noexcept {
        return level_.initialized();
    }
    [[nodiscard]] const GameplaySessionConfig& config() const noexcept {
        return config_;
    }
    [[nodiscard]] GameplaySessionConfig& config() noexcept { return config_; }
    [[nodiscard]] const trg::LevelRuntime& level() const noexcept {
        return level_;
    }
    [[nodiscard]] trg::LevelRuntime& level() noexcept { return level_; }
    [[nodiscard]] const PlayerState& player() const noexcept { return player_; }
    [[nodiscard]] PlayerState& player() noexcept { return player_; }
    [[nodiscard]] const PlayerPhysicsFrameHooks& physics_hooks() const noexcept {
        return hooks_;
    }
    [[nodiscard]] PlayerPhysicsFrameHooks& physics_hooks() noexcept {
        return hooks_;
    }
    [[nodiscard]] const FixedStepDriver& clock() const noexcept { return driver_; }
    [[nodiscard]] const camera::CameraRuntime& camera() const noexcept {
        return camera_;
    }
    [[nodiscard]] camera::CameraRuntime& camera() noexcept { return camera_; }

    [[nodiscard]] const GameplaySessionObservation observation() const noexcept;

    // Copies the current level-owned scene/model data for a presentation
    // backend. The session must have completed initialize() first.
    [[nodiscard]] trg::LevelRenderSnapshot render_snapshot() const;

    // Builds the native pre-backend polygon packets from the same owned
    // level/runtime state. Projection/raster policy remains the supplied
    // callback and therefore cannot be mistaken for a recovered backend.
    [[nodiscard]] trg::RenderPacketBuildResult render_packets(
        const trg::RenderProjector& projector,
        const trg::RenderPacketBuildOptions& options = {}) const;

private:
    trg::LevelRuntime level_;
    PlayerState player_;
    GameplayFrame gameplay_;
    GameplaySessionConfig config_;
    std::optional<assets::TricksBinArchive> tricks_archive_;
    std::optional<assets::TricksBinView> tricks_view_;
    std::vector<std::uint8_t> tricks_sequence_table_{};
    FixedStepDriver driver_;
    camera::CameraRuntime camera_;
    PlayerPhysicsFrameHooks hooks_;
    GameplayFrameResult last_frame_{};

    void apply_restart_events(std::size_t event_start);
    void update_camera_after_step();
};

} // namespace opentony::runtime
