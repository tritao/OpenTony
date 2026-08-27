#include "gameplay_session.hpp"

#include <algorithm>
#include <cstddef>
#include <stdexcept>

namespace opentony::runtime {

class GameplaySession::FrameObserver final : public LevelFrameObserver {
public:
    FrameObserver(
        GameplaySession& session,
        LevelFrameObserver* downstream) noexcept
        : session_(session), downstream_(downstream) {}

    void on_input_frame(
        std::uint64_t frame,
        const InputState& input) override {
        session_.prepare_level_event_frame(input);
        if (downstream_ != nullptr) {
            downstream_->on_input_frame(frame, input);
        }
    }

    void on_level_tick(
        std::uint64_t frame,
        std::uint32_t milliseconds,
        const InputState& input,
        const trg::LevelRuntime& level) override {
        // LevelRuntime::tick has already advanced the TRG event state. Apply
        // its player/camera/replay result before GameplayFrame enters the
        // physics step, matching the recovered consumer ordering.
        session_.apply_level_event_frame();
        if (downstream_ != nullptr) {
            downstream_->on_level_tick(frame, milliseconds, input, level);
        }
    }

    void on_player_physics(
        std::uint64_t frame,
        const PlayerPhysicsFrameResult& result,
        const PlayerState& player) override {
        session_.apply_deferred_gap_handoff();
        if (downstream_ != nullptr) {
            downstream_->on_player_physics(frame, result, player);
        }
    }

private:
    GameplaySession& session_;
    LevelFrameObserver* downstream_{};
};

GameplaySession::GameplaySession(
    const std::string& trg_path,
    const std::string& psx_path,
    const std::string& asset_root,
    PlayerState initial_player,
    GameplaySessionConfig config)
    : level_(trg_path, psx_path, asset_root),
      player_(initial_player),
      gameplay_(level_, player_),
      config_(config),
      driver_(gameplay_, config_.fixed_step),
      camera_(),
      replay_reset_owner_(),
      level_event_owner_(player_, camera_, replay_reset_owner_),
      hooks_() {
    if (config_.use_recovered_collision_scene) {
        std::string error;
        const auto scene = collision::PsxScene::parse(
            level_.scene_asset().bytes(), &error);
        if (!scene.has_value()) {
            throw std::logic_error(
                "recovered PSX collision scene parse failed: " + error);
        }
        collision_scene_ = *scene;
    }
    if (!config_.tricks_path.empty()) {
        tricks_archive_ = assets::TricksBinArchive::load(config_.tricks_path);
        tricks_view_ = tricks_archive_->view();

        std::span<const std::uint8_t> sequence_table{};
        if (config_.use_tricks_retail_builder) {
            const auto input_table = tricks_view_->player_input_sequence_table(
                config_.tricks_player_index);
            const auto source_table = tricks_view_->source_sequence_table();
            if (input_table.has_value() && source_table.has_value()) {
                const auto resources = parse_retail_action_resources(
                    *source_table,
                    tricks_view_->bytes);
                if (resources.has_value()) {
                    RetailActionResourceSelection selection{
                        config_.tricks_direct_resource_ids,
                        config_.tricks_mapped_resource_ids,
                        config_.tricks_mapped_mapping_indices};
                    const bool explicit_selection = std::any_of(
                        selection.direct_resource_ids.begin(),
                        selection.direct_resource_ids.end(),
                        [](std::uint8_t id) { return id != 0xff; })
                        || std::any_of(
                            selection.mapped_resource_ids.begin(),
                            selection.mapped_resource_ids.end(),
                            [](std::uint8_t id) { return id != 0xff; })
                        || std::any_of(
                            selection.mapped_mapping_indices.begin(),
                            selection.mapped_mapping_indices.end(),
                            [](std::uint8_t id) { return id != 0xff; });
                    if (config_.auto_select_tricks_retail_resources
                        && !explicit_selection) {
                        const auto automatic_selection =
                            build_retail_action_resource_selection(
                                *input_table,
                                *resources);
                        if (automatic_selection.has_value()) {
                            selection = *automatic_selection;
                        }
                    }
                    const RetailActionSequenceBuilderInput builder_input{
                        *input_table,
                        *resources,
                        selection.direct_resource_ids,
                        selection.mapped_resource_ids,
                        selection.mapped_mapping_indices,
                    };
                    const auto built = build_retail_action_sequence_table(
                        builder_input);
                    if (built.has_value()) {
                        tricks_sequence_table_ = built->table;
                        sequence_table = tricks_sequence_table_;
                    }
                }
            }
        }
        hooks_.action_sequence_source = ActionSequenceSource{
            &*tricks_view_,
            sequence_table,
            {},
            sequence_table.empty() &&
                config_.use_tricks_source_sequence_fallback,
        };
    }
    // The query is deliberately created at the frame boundary because the
    // probe's start point is the player's live position for that update.
    hooks_.collision_query = [this](
        const FixedPosition& start,
        const FixedPosition& end) {
        if (config_.use_recovered_collision_scene && collision_scene_.has_value()) {
            collision::CollisionFaceFilter filter{};
            filter.apply_retail_face_filter =
                config_.collision_query_options.apply_retail_face_filter;
            filter.reject_mask = config_.collision_query_options.reject_mask;
            filter.required_bits = config_.collision_query_options.accept_mask;
            filter.query_mask_mode =
                config_.collision_query_options.include_trigger_faces;
            return PsxScenePositionCollisionProbe(
                *collision_scene_, start, filter,
                std::span<const collision::PsxLinkedCollisionObject>(
                    recovered_linked_collision_objects_.data(),
                    recovered_linked_collision_objects_.size())).query(end);
        }
        return PsxPositionCollisionProbe(
            level_.collision(),
            start,
            config_.collision_query_options).query(end);
    };

    if (config_.apply_air_gravity) {
        hooks_.apply_air_motion_basis = true;
        hooks_.air_gravity_input = [this](
            const PlayerState&,
            const InputState&) -> std::optional<AirGravityConfig> {
            return config_.air_gravity;
        };
    }

    if (config_.apply_ground_motion) {
        hooks_.ground_motion_input = [this](
            const PlayerState&,
            const InputState&,
            const ActionProfileState&) {
            GroundMotionInput input{};
            input.producer_enabled = true;
            if (config_.ground_motion_profile_records.has_value()) {
                const GroundMotionProfileTable table =
                    materialize_ground_motion_profile_table(
                        *config_.ground_motion_profile_records);
                input.profile_table_value_nonzero =
                    table.selected_value_nonzero();
            } else {
                input.profile_table_value_nonzero =
                    config_.ground_motion_profile_table_value_nonzero ||
                    config_.ground_motion_profile_table.selected_value_nonzero();
            }
            input.apply_control_side_effects =
                config_.apply_ground_motion_control;
            input.surface_response_metric =
                config_.ground_motion_surface_response_metric;
            input.rearm_random_available =
                config_.ground_motion_rearm_random_available;
            input.rearm_random_roll = config_.ground_motion_rearm_random_roll;
            input.animation_event_enabled =
                config_.ground_motion_animation_event_enabled;
            // Negative values request the frame to fill the verified live
            // response metric and threshold from PlayerState.
            input.response_speed_metric = -1;
            input.response_speed_threshold = -1;
            return std::optional<GroundMotionInput>{input};
        };
    }

    if (config_.classify_retail_air_contacts) {
        hooks_.on_air_contact = [](
            PlayerState&,
            const PositionCollisionHit& hit,
            const PositionCommitResult&) {
            return accepts_retail_ground_contact(hit);
        };
    }

    if (config_.apply_collision_response_bias) {
        hooks_.collision_response_bias_q12 = [this](
            const PlayerState&,
            const PositionCollisionHit&,
            PhysicsDispatchStage) -> std::optional<std::int32_t> {
            return config_.collision_response_bias_q12;
        };
    }
}

void GameplaySession::initialize(
    bool two_player,
    const trg::GapTable* gap_table) {
    // Configure this before LevelRuntime's reset/autoexec/build sequence so
    // an autoexec pulse observes the same mode gate as retail.
    level_.state().set_special_runtime_game_mode(
        config_.special_runtime_game_mode);
    if (config_.level_event_inputs.has_value()) {
        level_.state().set_level_event_inputs(*config_.level_event_inputs);
    }
    level_event_owner_.set_score_input_active(
        config_.level_event_primary_score_input_active,
        config_.level_event_secondary_score_input_active);
    replay_reset_owner_.reset();
    level_.initialize(two_player, gap_table);
    // Front_LoadGame calls FUN_004c4e30 after the TRG autoexec/object pass.
    // That routine applies the restart selected by 0x8c/0xb0, including its
    // post-name command stream, before the first gameplay frame.
    const std::size_t selected_restart = level_.triggers().selected_restart();
    if (selected_restart != trg::CommandPointRuntime::npos) {
        level_.execute_restart(selected_restart);
        const trg::TriggerRestartApplication& restart =
            level_.state().last_restart();
        if (restart.set) {
            player_.apply_restart(
                restart.position,
                restart.auxiliary,
                restart.auxiliary_word);
        }
    }
    if (config_.update_camera) {
        camera_.reset(config_.camera_target);
    }
    sync_script_skater_fields();
    driver_.reset();
    last_frame_ = {};
}

void GameplaySession::execute_restart(std::size_t node) {
    if (!initialized()) {
        throw std::logic_error("gameplay session restarted before initialize");
    }
    level_.execute_restart(node);
    const trg::TriggerRestartApplication& restart = level_.state().last_restart();
    if (!restart.set) {
        throw std::logic_error(
            "level restart did not produce an applied restart record");
    }
    player_.apply_restart(
        restart.position,
        restart.auxiliary,
        restart.auxiliary_word);
    sync_script_skater_fields();
    driver_.reset();
}

void GameplaySession::execute_restart(std::string_view name) {
    if (!initialized()) {
        throw std::logic_error("gameplay session restarted before initialize");
    }
    const std::size_t node = level_.triggers().find_restart_by_name(name);
    if (node == trg::CommandPointRuntime::npos) {
        throw std::logic_error("gameplay restart name was not found");
    }
    execute_restart(node);
}

void GameplaySession::pulse_node(std::size_t node) {
    if (!initialized()) {
        throw std::logic_error("gameplay session pulsed before initialize");
    }
    const std::size_t event_start = level_.state().events().size();
    level_.pulse_node(node);
    apply_restart_events(event_start);
    sync_script_skater_fields();
}

void GameplaySession::pulse_checksum(std::uint32_t checksum) {
    if (!initialized()) {
        throw std::logic_error("gameplay session pulsed before initialize");
    }
    const std::size_t event_start = level_.state().events().size();
    level_.pulse_checksum(checksum);
    apply_restart_events(event_start);
    sync_script_skater_fields();
}

void GameplaySession::set_recovered_linked_collision_objects(
    std::vector<collision::PsxLinkedCollisionObject> objects) {
    recovered_linked_collision_objects_ = std::move(objects);
}

void GameplaySession::apply_restart_events(std::size_t event_start) {
    const auto& events = level_.state().events();
    const bool restart_applied = std::any_of(
        events.begin() + static_cast<std::ptrdiff_t>(event_start),
        events.end(),
        [](const trg::TriggerEvent& event) {
            return event.kind == trg::TriggerEvent::Kind::RestartApplied;
        });
    if (restart_applied) {
        const trg::TriggerRestartApplication& restart = level_.state().last_restart();
        player_.apply_restart(
            restart.position,
            restart.auxiliary,
            restart.auxiliary_word);
        driver_.reset();
    }
}

void GameplaySession::prepare_level_event_frame(
    const InputState& input) noexcept {
    level_event_owner_.set_score_input_active(
        config_.level_event_primary_score_input_active,
        config_.level_event_secondary_score_input_active);
    level_.state().set_level_event_frame_input(
        level_event_owner_.frame_input(
            level_.state().level_event_inputs(),
            input.action_mask() != 0));
}

void GameplaySession::apply_level_event_frame() noexcept {
    level_event_owner_.apply(level_.state().last_level_event_frame());
}

void GameplaySession::apply_deferred_gap_handoff() {
    const auto handoff = level_.state().complete_deferred_gap_for_physics_state(
        player_.physics_state());
    if (!handoff.has_value()
        || handoff->source_node == trg::CommandPointRuntime::npos) {
        return;
    }
    // The player physics branch selects the deferred slot; the source node is
    // then pulsed through the normal level owner, preserving script ordering.
    pulse_node(handoff->source_node);
}

void GameplaySession::update_camera_after_step() {
    if (!config_.update_camera) {
        return;
    }
    if (!camera_.configured()) {
        camera_.reset(config_.camera_target);
    }
    camera::CameraTargetRaw target = config_.camera_target;
    target.position = {
        player_.position()[0],
        player_.position()[1],
        player_.position()[2],
    };
    (void)camera_.update(
        target,
        config_.camera_follow_input,
        config_.camera_look_target_offset,
        config_.camera_hooks,
        config_.camera_mode_input,
        config_.camera_mode25_input);
}

FixedStepAdvanceResult GameplaySession::advance(
    std::uint32_t elapsed_ms,
    const DirectInputKeyboardState& keyboard,
    const InputBindings& bindings,
    LevelFrameObserver* observer,
    std::optional<std::int32_t> frame_scale_override) {
    if (!initialized()) {
        throw std::logic_error("gameplay session advanced before initialize");
    }
    FrameObserver session_observer(*this, observer);
    FixedStepAdvanceResult result = driver_.advance(
        elapsed_ms,
        keyboard,
        bindings,
        hooks_,
        &session_observer,
        frame_scale_override);
    if (result.stepped) {
        last_frame_ = result.last;
        update_camera_after_step();
    }
    return result;
}

FixedStepAdvanceResult GameplaySession::advance(
    std::uint32_t elapsed_ms,
    std::uint16_t action_mask,
    std::int8_t horizontal_axis,
    std::int8_t vertical_axis,
    LevelFrameObserver* observer,
    std::optional<std::int32_t> frame_scale_override) {
    if (!initialized()) {
        throw std::logic_error("gameplay session advanced before initialize");
    }
    FrameObserver session_observer(*this, observer);
    FixedStepAdvanceResult result = driver_.advance(
        elapsed_ms,
        action_mask,
        horizontal_axis,
        vertical_axis,
        hooks_,
        &session_observer,
        frame_scale_override);
    if (result.stepped) {
        last_frame_ = result.last;
        update_camera_after_step();
    }
    return result;
}

trg::LevelRenderSnapshot GameplaySession::render_snapshot() const {
    if (!initialized()) {
        throw std::logic_error(
            "gameplay session rendered before initialize");
    }
    return trg::LevelRenderSnapshot::build(
        level_.scene(),
        level_.scene_runtime(),
        &level_.powerups(),
        level_.item_runtime(),
        level_.medal_runtime());
}

trg::RenderPacketBuildResult GameplaySession::render_packets(
    const trg::RenderProjector& projector,
    const trg::RenderPacketBuildOptions& options) const {
    if (!initialized()) {
        throw std::logic_error(
            "gameplay session rendered before initialize");
    }
    trg::RenderPacketBuildOptions effective = options;
    if (!effective.texture_dimensions) {
        effective.texture_dimensions = [this](
            std::size_t material_index,
            std::uint32_t material_checksum)
            -> std::optional<trg::RenderTextureDimensions> {
            const std::array<const assets::PcTextureRuntime*, 3> texture_regions{
                level_.texture_runtime(),
                level_.item_texture_runtime(),
                level_.medal_texture_runtime(),
            };
            for (const assets::PcTextureRuntime* textures : texture_regions) {
                if (textures == nullptr) {
                    continue;
                }
                const auto dimensions = textures->dimensions_for_material(
                    material_index, material_checksum);
                if (dimensions.has_value()) {
                    return trg::RenderTextureDimensions{
                        dimensions->at(0), dimensions->at(1)};
                }
            }
            const auto dimensions =
                level_.scene_runtime().texture_dimensions_for_material(
                    material_index, material_checksum);
            if (!dimensions.has_value()) {
                return std::nullopt;
            }
            return trg::RenderTextureDimensions{
                dimensions->at(0), dimensions->at(1)};
        };
    }
    return trg::RenderPacketBuilder::build(
        render_snapshot(), camera_.state(), projector, effective);
}

const GameplaySessionObservation GameplaySession::observation() const noexcept {
    const trg::LevelTriggerState& state = level_.state();
    GameplaySessionObservation result{};
    result.frame = last_frame_;
    result.level_time_ms = state.time_ms();
    result.trigger_object_count = state.objects().size();
    result.trigger_timer_count = state.timers().size();
    result.trigger_gap_count = state.gaps().size();
    result.trigger_event_count = state.events().size();
    result.trigger_resource_count = state.resources().size();
    result.scene_entity_count = level_.scene().entities().size();
    result.scene_bound_trigger_count = level_.scene().bound_trigger_count();
    result.position = player_.position();
    result.previous_position = player_.previous_position();
    result.collision_response = player_.collision_response();
    result.motion_correction = player_.motion_correction();
    result.air_motion = player_.air_motion();
    result.orientation = player_.orientation();
    result.retail_basis = player_.retail_basis();
    result.physics_state = player_.physics_state();
    result.ground_update_state = player_.ground_update_state();
    result.ground_physics_mode = player_.ground_physics_mode();
    result.action_command_state = player_.action_command_state();
    result.action_stream_active = player_.action_stream_active();
    result.action_stream_relative = player_.action_stream_relative();
    result.action_stream_cursor = player_.action_stream_cursor();
    result.restart_auxiliary = player_.restart_auxiliary();
    result.restart_auxiliary_word = player_.restart_auxiliary_word();
    result.camera_update_tick = camera_.state().update_tick;
    result.camera_mode = camera_.state().mode;
    result.camera_rendered_position = camera_.last_commit().rendered_position;
    result.script_skater_fields = player_.script_skater_fields();
    return result;
}

void GameplaySession::sync_script_skater_fields() noexcept {
    const trg::TriggerCurrentSkaterFields& source =
        level_.state().current_skater_fields();
    PlayerScriptSkaterFields fields = player_.script_skater_fields();
    if (source.has_3198) {
        fields.has_3198 = true;
        fields.field_3198 = source.field_3198;
    }
    if (source.has_319c) {
        fields.has_319c = true;
        fields.field_319c = source.field_319c;
    }
    player_.set_script_skater_fields(fields);
}

} // namespace opentony::runtime
