#include "gameplay_session.hpp"
#include "gameplay_presentation.hpp"

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
    // This probe intentionally crosses the first cached face in both
    // directions. Use the broad compatibility policy here; ordinary session
    // defaults exercise the recovered retail face masks and plane test.
    config.collision_query_options = {};
    opentony::runtime::GameplaySession session(
        trg,
        psx,
        asset_path(""),
        opentony::runtime::PlayerState{},
        config);
    assert(!session.initialized());
    session.initialize();
    assert(session.initialized());
    assert(session.level().triggers().selected_restart()
        != opentony::trg::CommandPointRuntime::npos);
    assert(session.level().state().last_restart().set);
    assert(session.player().position()
        == session.level().state().last_restart().position);
    assert(session.player().previous_position()
        == session.level().state().last_restart().position);
    assert(session.level().scene().static_entity_count() == 252);
    assert(session.physics_hooks().collision_query);
    assert(session.physics_hooks().air_gravity_input);
    assert(session.physics_hooks().on_air_contact);

    // The trigger service owns the raw 0xa3/0xb1 writes. A session pulse is
    // the boundary that publishes those writes to the live player without
    // assigning their still-unresolved retail meanings.
    session.level().state().on_current_skater_word(141, 0x00a3, 0x1234);
    session.pulse_node(141);
    assert(session.player().script_skater_fields().has_3198);
    assert(session.player().script_skater_fields().field_3198 == 0x1234);
    assert(!session.player().script_skater_fields().has_319c);

    session.level().state().on_current_skater_word(141, 0x00b1, 0xabcd);
    session.pulse_node(141);
    assert(session.player().script_skater_fields().field_3198 == 0x1234);
    assert(session.player().script_skater_fields().has_319c);
    assert(session.player().script_skater_fields().field_319c == 0xabcd);

    // Exercise the recovered B010 correction through the real Warehouse
    // level/session boundary. The profile-table value is supplied explicitly
    // because its retail source is a character/profile configuration record,
    // not the directional action-table result.
    opentony::runtime::GameplaySessionConfig motion_config = config;
    motion_config.apply_ground_motion = true;
    opentony::runtime::GroundMotionProfileRecords profile_records{};
    profile_records.primary_field_10[0] = 1;
    motion_config.ground_motion_profile_records = profile_records;
    motion_config.apply_ground_motion_control = true;
    motion_config.ground_motion_rearm_random_available = true;
    motion_config.ground_motion_rearm_random_roll = 0;
    opentony::runtime::GameplaySession motion_session(
        trg,
        psx,
        asset_path(""),
        opentony::runtime::PlayerState{},
        motion_config);
    assert(motion_session.physics_hooks().ground_motion_input);
    motion_session.initialize();
    motion_session.player().set_collision_response({0x1000, 0, 0});
    const auto motion_step = motion_session.advance(16, 0, 0, 0);
    assert(motion_step.steps == 1);
    assert(motion_step.last.physics.ground_motion.has_value());
    assert(motion_step.last.physics.ground_motion->applied);
    assert(motion_step.last.physics.ground_motion->branch ==
        opentony::runtime::GroundMotionBranch::Ordinary);
    assert(motion_step.last.physics.ground_motion->cooldown_written);
    assert(motion_session.player().ground_motion_cooldown() == 0x14);

    opentony::runtime::GameplaySessionConfig tricks_config{};
    tricks_config.tricks_path = asset_path("TRICKS.BIN");
    tricks_config.use_tricks_retail_builder = true;
    opentony::runtime::GameplaySession tricks_session(
        trg,
        psx,
        asset_path(""),
        opentony::runtime::PlayerState{},
        tricks_config);
    assert(tricks_session.physics_hooks().action_sequence_source.has_value());
    assert(tricks_session.physics_hooks().action_sequence_source->tricks != nullptr);
    assert(!tricks_session.physics_hooks().action_sequence_source
        ->use_source_sequence_fallback);
    assert(!tricks_session.physics_hooks().action_sequence_source
        ->sequence_table.empty());
    assert(tricks_session.physics_hooks().action_sequence_source
        ->sequence_table.size() < 0x1000);
    assert(tricks_session.physics_hooks().action_sequence_source->tricks
        ->source_sequence_table().has_value());
    tricks_session.initialize();
    static_cast<void>(tricks_session.advance(16, 0x4000U, 0, 0));
    const auto trick_step = tricks_session.advance(16, 0x1000U, 0, 0);
    assert(trick_step.last.physics.action_sequence.has_value());
    assert(trick_step.last.physics.action_sequence->match.matched);
    assert(trick_step.last.physics.action_sequence->stream_resolved);

    // Exercise the explicit mapped/static pass against the real Warehouse-era
    // archive. Resource ID 0 is a real section-5 record and mapping index 0
    // is the first DAT_00540e30 entry; the pair is deliberately supplied
    // through the same selection-view arrays used by the retail builder.
    opentony::runtime::GameplaySessionConfig mapped_tricks_config = tricks_config;
    mapped_tricks_config.auto_select_tricks_retail_resources = false;
    mapped_tricks_config.tricks_mapped_resource_ids[0] = 0;
    mapped_tricks_config.tricks_mapped_mapping_indices[0] = 0;
    opentony::runtime::GameplaySession mapped_tricks_session(
        trg,
        psx,
        asset_path(""),
        opentony::runtime::PlayerState{},
        mapped_tricks_config);
    assert(mapped_tricks_session.physics_hooks().action_sequence_source.has_value());
    assert(!mapped_tricks_session.physics_hooks().action_sequence_source
        ->sequence_table.empty());
    // The default session above used the recovered automatic selection pass;
    // it should include the real static-combo resources discovered from the
    // section-0 records rather than only the explicit mapped fixture.
    assert(tricks_session.physics_hooks().action_sequence_source
        ->sequence_table.size()
        > mapped_tricks_session.physics_hooks().action_sequence_source
            ->sequence_table.size());

    const auto snapshot = session.render_snapshot();
    assert(snapshot.entities().size() == session.level().scene().entities().size());
    assert(!snapshot.faces().empty());

    // The presentation boundary consumes the live session without reaching
    // back into TRG/PSX internals. Camera preparation is intentionally raw;
    // renderer scale and backend policy remain outside this test.
    opentony::runtime::GameplayPresentation presentation(session);
    opentony::camera::CameraRuntimeUpdateInput camera_input{};
    camera_input.target = opentony::runtime::make_camera_target(
        session.player(),
        {0, -0x1000, 0});
    const auto camera_commit = presentation.update_camera(camera_input);
    assert(presentation.camera().has_commit());
    assert(camera_commit.viewport_parameter_low_raw == 0);
    const auto presentation_snapshot = presentation.snapshot();
    assert(presentation_snapshot.frame_index == session.observation().frame.frame_index);
    assert(presentation_snapshot.player.position == session.player().position());
    assert(presentation_snapshot.player.script_skater_fields
        == session.player().script_skater_fields());
    assert(presentation_snapshot.level.entities().size() == snapshot.entities().size());
    assert(presentation_snapshot.has_camera_commit);

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
    assert(observation.action_stream_active == session.player().action_stream_active());
    assert(observation.action_stream_relative == session.player().action_stream_relative());
    assert(observation.action_stream_cursor == session.player().action_stream_cursor());
    assert(observation.script_skater_fields
        == session.player().script_skater_fields());

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

        opentony::runtime::GameplaySession b1_checksum_session(
            b1_trg,
            b1_psx,
            asset_path(""));
        b1_checksum_session.initialize();
        const auto* b1_command_point =
            b1_checksum_session.level().triggers().command_point(11);
        assert(b1_command_point != nullptr);
        b1_checksum_session.pulse_checksum(b1_command_point->checksum);
        const auto& b1_checksum_restart =
            b1_checksum_session.level().state().last_restart();
        assert(b1_checksum_restart.set);
        assert(b1_checksum_session.player().position()
            == b1_checksum_restart.position);
        assert(b1_checksum_session.player().previous_position()
            == b1_checksum_restart.position);
        assert(b1_checksum_session.player().restart_auxiliary()
            == b1_checksum_restart.auxiliary);
        assert(b1_checksum_session.player().restart_auxiliary_word()
            == b1_checksum_restart.auxiliary_word);
    }
    std::cout << "Gameplay session tests passed\n";
}
