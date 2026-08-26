#include "gameplay_session.hpp"

#include <algorithm>
#include <cstddef>
#include <stdexcept>

namespace opentony::runtime {

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
      hooks_() {
    // The query is deliberately created at the frame boundary because the
    // probe's start point is the player's live position for that update.
    hooks_.collision_query = [this](
        const FixedPosition& start,
        const FixedPosition& end) {
        return PsxPositionCollisionProbe(level_.collision(), start).query(end);
    };

    if (config_.apply_air_gravity) {
        hooks_.apply_air_motion_basis = true;
        hooks_.air_gravity_input = [this](
            const PlayerState&,
            const InputState&) -> std::optional<AirGravityConfig> {
            return config_.air_gravity;
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
    level_.initialize(two_player, gap_table);
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

void GameplaySession::pulse_checksum(std::uint32_t checksum) {
    if (!initialized()) {
        throw std::logic_error("gameplay session pulsed before initialize");
    }
    level_.pulse_checksum(checksum);
}

FixedStepAdvanceResult GameplaySession::advance(
    std::uint32_t elapsed_ms,
    const DirectInputKeyboardState& keyboard,
    const InputBindings& bindings,
    LevelFrameObserver* observer) {
    if (!initialized()) {
        throw std::logic_error("gameplay session advanced before initialize");
    }
    FixedStepAdvanceResult result = driver_.advance(
        elapsed_ms,
        keyboard,
        bindings,
        hooks_,
        observer);
    if (result.stepped) {
        last_frame_ = result.last;
    }
    return result;
}

FixedStepAdvanceResult GameplaySession::advance(
    std::uint32_t elapsed_ms,
    std::uint16_t action_mask,
    std::int8_t horizontal_axis,
    std::int8_t vertical_axis,
    LevelFrameObserver* observer) {
    if (!initialized()) {
        throw std::logic_error("gameplay session advanced before initialize");
    }
    FixedStepAdvanceResult result = driver_.advance(
        elapsed_ms,
        action_mask,
        horizontal_axis,
        vertical_axis,
        hooks_,
        observer);
    if (result.stepped) {
        last_frame_ = result.last;
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
        level_.scene_asset());
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
    result.restart_auxiliary = player_.restart_auxiliary();
    result.restart_auxiliary_word = player_.restart_auxiliary_word();
    return result;
}

} // namespace opentony::runtime
