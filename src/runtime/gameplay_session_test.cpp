#include "gameplay_session.hpp"

#include <algorithm>
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
    config.update_camera = true;
    config.camera_target.follow_offset = {0, 0, -0x4000};
    config.camera_target.tripod_state = 1;
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
    assert(session.camera().configured());
    assert(session.level().texture_runtime() != nullptr);

    opentony::runtime::GameplaySessionConfig recovered_config = config;
    recovered_config.use_recovered_collision_scene = true;
    opentony::runtime::GameplaySession recovered_session(
        trg,
        psx,
        asset_path(""),
        opentony::runtime::PlayerState{},
        recovered_config);
    assert(recovered_session.physics_hooks().collision_query);

    const std::string hangar_trg = asset_path("SKHAN_T.TRG");
    const std::string hangar_psx = asset_path("SKHAN.PSX");
    if (std::filesystem::is_regular_file(hangar_trg) &&
        std::filesystem::is_regular_file(hangar_psx)) {
        opentony::runtime::GameplaySession hangar_session(
            hangar_trg,
            hangar_psx,
            asset_path(""),
            opentony::runtime::PlayerState{},
            recovered_config);
        const auto hangar_hit = hangar_session.physics_hooks().collision_query(
            {-4100096, -8822784, 11472896},
            {-4100096, 23945216, 11472896});
        assert(hangar_hit.has_value());
        assert(hangar_hit->model_index == 171);
        assert(hangar_hit->hit_parameter_q14 == 61);
        assert(hangar_hit->position == (opentony::runtime::FixedPosition{
            -4100096, -8700784, 11472896}));

        constexpr std::int32_t dynamic_offset = 100000000;
        hangar_session.set_recovered_linked_collision_objects({
            opentony::collision::PsxLinkedCollisionObject{
                .body_id = 0x05f26c84,
                .source_object_index = 170,
                .flags = 0x0110,
                .position = {
                    -4100096 + dynamic_offset,
                    -6782976,
                    9408512 + dynamic_offset,
                },
                .angles = {0, 0, 0},
                .model_index = 171,
                .model_kind = 6,
            },
        });
        const auto dynamic_hit = hangar_session.physics_hooks().collision_query(
            {-4100096 + dynamic_offset, -8822784,
             11472896 + dynamic_offset},
            {-4100096 + dynamic_offset, 23945216,
             11472896 + dynamic_offset});
        assert(dynamic_hit.has_value());
        assert(dynamic_hit->object_index == 0);
        assert(dynamic_hit->source_object_index == 170);
        assert(dynamic_hit->model_index == 171);
        assert(dynamic_hit->hit_parameter_q14 == 0x7fffffffU);
        assert(dynamic_hit->position == (opentony::runtime::FixedPosition{
            -4100096 + dynamic_offset, -8710784,
            11472896 + dynamic_offset}));
        assert(dynamic_hit->normal == (opentony::runtime::FixedPosition{
            1, -4093, -160}));
    }

    opentony::runtime::GameplaySessionConfig tricks_config{};
    tricks_config.tricks_path = asset_path("TRICKS.BIN");
    tricks_config.use_tricks_retail_builder = true;
    tricks_config.auto_select_tricks_retail_resources = false;
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

    // Exercise the mapped/static pass against the real Warehouse-era archive.
    // Resource ID 0 is a real section-5 record and mapping index 0 is the
    // first DAT_00540e30 entry; the pair is deliberately supplied through the
    // same selection-view arrays used by the retail builder.
    opentony::runtime::GameplaySessionConfig mapped_tricks_config = tricks_config;
    mapped_tricks_config.tricks_mapped_resource_ids[0] = 0;
    mapped_tricks_config.tricks_mapped_mapping_indices[0] = 0;
    opentony::runtime::GameplaySession mapped_tricks_session(
        trg,
        psx,
        asset_path(""),
        opentony::runtime::PlayerState{},
        mapped_tricks_config);
    assert(mapped_tricks_session.physics_hooks().action_sequence_source.has_value());
    assert(mapped_tricks_session.physics_hooks().action_sequence_source
        ->sequence_table.size()
        > tricks_session.physics_hooks().action_sequence_source
            ->sequence_table.size());

    const auto snapshot = session.render_snapshot();
    assert(snapshot.entities().size() == session.level().scene().entities().size());
    assert(!snapshot.faces().empty());
    // The first static entities retain the offline PSX object ordinal. Keep
    // the concrete object-17 bridge executable rather than testing only
    // aggregate counts.
    assert(snapshot.entities().size() > 17);
    assert(snapshot.entities()[17].psx_object_index == 17);
    assert(snapshot.entities()[17].model_index
        == session.level().scene_asset().objects()[17].model_index);
    const auto pickup_17 = std::find_if(
        snapshot.entities().begin(),
        snapshot.entities().end(),
        [](const opentony::trg::LevelRenderEntitySnapshot& entity) {
            return entity.kind == opentony::trg::LevelSceneEntityKind::Pickup
                && entity.source_node == 17;
        });
    assert(pickup_17 != snapshot.entities().end());
    assert(pickup_17->model_index == 5);
    assert(pickup_17->face_count > 0);

    const opentony::trg::RenderProjector projector =
        [](const opentony::trg::RenderViewVertexInput& input) {
            return opentony::trg::RenderProjectedVertex{
                static_cast<float>(input.position_q16[0]),
                static_cast<float>(input.position_q16[1]),
                static_cast<float>(input.position_q16[2]),
                1.0F,
                input.source_flags,
                0.0F,
            };
        };
    const auto packets = session.render_packets(projector);
    assert(packets.polygons.size() == snapshot.faces().size());
    assert(packets.working_vertices.size() >= packets.polygons.front().vertex_count);
    assert(packets.polygons.front().format
        == opentony::trg::kRenderPolygonPacketFormat);
    bool saw_object_17_packet = false;
    bool saw_pickup_17_packet = false;
    bool saw_external_warehouse_texture = false;
    for (const auto& polygon : packets.polygons) {
        if (polygon.object_index == 17) {
            saw_object_17_packet = true;
            assert(polygon.model_index
                == snapshot.entities()[17].model_index);
        }
        if (polygon.object_index == opentony::trg::CommandPointRuntime::npos
            && polygon.model_index == pickup_17->model_index) {
            saw_pickup_17_packet = true;
        }
        if (polygon.material_checksum == 0x032bbb26U) {
            saw_external_warehouse_texture = true;
            assert(polygon.textured);
            for (const auto& vertex : polygon.vertices) {
                assert(vertex.uv_normalized);
            }
        }
    }
    assert(saw_object_17_packet);
    assert(saw_pickup_17_packet);
    assert(saw_external_warehouse_texture);

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
    assert(observation.camera_update_tick == 1);
    assert(observation.camera_mode == 1);

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
