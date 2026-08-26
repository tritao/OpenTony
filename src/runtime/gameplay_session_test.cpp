#include "gameplay_session.hpp"

#include <cassert>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

[[nodiscard]] std::string asset_path(const char* relative) {
    const std::filesystem::path root =
        "/home/joao/dev/OpenTony/build/assets/all-pkr/files/data";
    return (root / relative).string();
}

} // namespace

int main() {
    const std::string trg = asset_path("SKWARE_T.TRG");
    const std::string psx = asset_path("SKWARE.PSX");
    if (!std::filesystem::is_regular_file(trg)
        || !std::filesystem::is_regular_file(psx)) {
        std::cout << "fixture unavailable\n";
        return 0;
    }

    opentony::runtime::GameplaySessionConfig config{};
    config.fixed_step.max_catch_up_steps = 2;
    config.fixed_step.frame_scale_q8 = 0x80;
    opentony::runtime::GameplaySession session(
        trg,
        psx,
        asset_path(""),
        opentony::runtime::PlayerState{},
        config);
    assert(!session.initialized());
    session.initialize();
    assert(session.initialized());
    assert(session.level().scene().static_entity_count() == 252);
    assert(session.physics_hooks().collision_query);
    assert(session.physics_hooks().air_gravity_input);

    const auto snapshot = session.render_snapshot();
    assert(snapshot.entities().size() == session.level().scene().entities().size());
    assert(!snapshot.faces().empty());

    const auto& first_face = session.level().collision().faces().front();
    const opentony::runtime::FixedPosition face_center{
        static_cast<std::int32_t>((
            static_cast<std::int64_t>(first_face.vertices[0][0])
            + first_face.vertices[1][0]
            + first_face.vertices[2][0]) / 3),
        static_cast<std::int32_t>((
            static_cast<std::int64_t>(first_face.vertices[0][1])
            + first_face.vertices[1][1]
            + first_face.vertices[2][1]) / 3),
        static_cast<std::int32_t>((
            static_cast<std::int64_t>(first_face.vertices[0][2])
            + first_face.vertices[1][2]
            + first_face.vertices[2][2]) / 3),
    };
    const auto session_hit = session.physics_hooks().collision_query(
        {face_center[0] + first_face.normal[0],
         face_center[1] + first_face.normal[1],
         face_center[2] + first_face.normal[2]},
        {face_center[0] - first_face.normal[0],
         face_center[1] - first_face.normal[1],
         face_center[2] - first_face.normal[2]});
    assert(session_hit.has_value());
    assert(session_hit->object_index == first_face.object_index);

    const auto bindings =
        opentony::runtime::InputBindings::movement_defaults();
    opentony::runtime::DirectInputKeyboardState keyboard{};
    keyboard[opentony::runtime::kDikLeft] = 0x80;

    const auto partial = session.advance(15, keyboard, bindings);
    assert(!partial.stepped);
    assert(partial.steps == 0);
    assert(session.clock().accumulated_ms() == 15);

    const auto stepped = session.advance(1, keyboard, bindings);
    assert(stepped.steps == 1);
    assert(stepped.last.frame_index == 1);
    assert(stepped.last.frame_scale_q8 == 0x80);
    assert(stepped.last.input.held(
        opentony::runtime::movement_bit(
            opentony::runtime::MovementAction::Left)));
    assert(session.player().frame_counter() == 1);
    assert(session.level().state().time_ms() == 16);

    const auto observation = session.observation();
    assert(observation.frame.frame_index == 1);
    assert(observation.level_time_ms == 16);
    assert(observation.trigger_object_count == session.level().state().objects().size());
    assert(observation.scene_entity_count == session.level().scene().entities().size());
    assert(observation.position == session.player().position());
    assert(observation.orientation == session.player().orientation());

    const std::size_t events_before_pulse = session.level().state().events().size();
    session.pulse_node(141);
    assert(session.level().state().events().size() > events_before_pulse);
    const auto* visible_target = session.level().state().object(0x00cf);
    assert(visible_target != nullptr);
    assert(visible_target->visible_commanded);
    const auto* command_point = session.level().triggers().command_point(141);
    assert(command_point != nullptr);
    session.pulse_checksum(command_point->checksum);

    session.execute_restart("Ho_SkWare_HPGap");
    const auto& applied_restart = session.level().state().last_restart();
    assert(applied_restart.set);
    assert(session.player().position() == applied_restart.position);
    assert(session.player().previous_position() == applied_restart.position);
    assert(session.player().restart_auxiliary() == applied_restart.auxiliary);
    assert(session.player().restart_auxiliary_word()
        == applied_restart.auxiliary_word);
    assert(session.clock().accumulated_ms() == 0);

    session.reset_clock();
    assert(session.clock().accumulated_ms() == 0);

    // SKB1 node 11 is a retail KILLBRUCE command point linked to restart
    // node 2. The command must apply the restart through the same session
    // player boundary as the explicit restart API.
    const std::string b1_trg = asset_path("SKB1_T.TRG");
    const std::string b1_psx = asset_path("SKB1.PSX");
    if (std::filesystem::is_regular_file(b1_trg)
        && std::filesystem::is_regular_file(b1_psx)) {
        opentony::runtime::GameplaySession b1_session(
            b1_trg,
            b1_psx,
            asset_path(""));
        b1_session.initialize();
        b1_session.pulse_node(11);
        const auto& b1_restart = b1_session.level().state().last_restart();
        assert(b1_restart.set);
        assert(b1_restart.node == 2);
        assert(b1_session.player().position() == b1_restart.position);
        assert(b1_session.player().previous_position() == b1_restart.position);
        assert(b1_session.player().restart_auxiliary() == b1_restart.auxiliary);
        assert(b1_session.player().restart_auxiliary_word()
            == b1_restart.auxiliary_word);
    }
    std::cout << "Gameplay session tests passed\n";
}
