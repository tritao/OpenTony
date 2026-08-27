#include "physics_frame.hpp"
#include "physics_replay.hpp"

#include "tests/test_check.hpp"
#include <iostream>
#include <vector>

int main() {
    using opentony::runtime::FixedPosition;
    using opentony::runtime::InputState;
    using opentony::runtime::MovementAction;
    using opentony::runtime::PhysicsDispatchStage;
    using opentony::runtime::PlayerPhysicsFrame;
    using opentony::runtime::PlayerPhysicsFrameHooks;
    using opentony::runtime::PlayerState;
    using opentony::runtime::movement_bit;

    PlayerState player({0, 0, 0});
    player.set_physics_state(3);
    player.set_collision_response({10, 20, 30});
    InputState input;
    input.begin_frame(movement_bit(MovementAction::Left));

    std::vector<PhysicsDispatchStage> stages;
    PlayerPhysicsFrameHooks hooks{};
    hooks.collision_probe = [](const FixedPosition& candidate) {
        return candidate[0] > 5;
    };
    hooks.on_stage = [&stages](
        PhysicsDispatchStage stage,
        PlayerState&,
        const InputState&) {
        stages.push_back(stage);
    };

    const auto frame = PlayerPhysicsFrame::step(player, input, hooks);
    CHECK(frame.dispatch.handled);
    CHECK(frame.position_integrated);
    CHECK(frame.motion_correction_integrated);
    CHECK(stages.size() == 1);
    CHECK(stages.front() == PhysicsDispatchStage::InAir_97f40);
    // The desired x=10 is blocked; FUN_00496060 falls back to the old x.
    CHECK(frame.position_commit.collided);
    CHECK(frame.position_commit.probes == 2);
    CHECK(frame.position_commit.selected_candidate == 2);
    CHECK(player.position() == FixedPosition({0, 20, 30}));
    CHECK(player.previous_position() == FixedPosition({0, 0, 0}));
    CHECK(player.turn_accumulator() == 0);

    // FUN_00493370 applies the drained local delta through the live matrix
    // before the state dispatcher. A retail zero-angle restart installs the
    // negative-identity basis, so a local +X step becomes world -X.
    PlayerState queued_player({100, 200, 300});
    queued_player.apply_restart({100, 200, 300}, 0x08000000);
    CHECK(queued_player.set_queued_motion_command(0, 1, 4));
    const auto queued_motion = queued_player.drain_queued_motion();
    const FixedPosition queued_world_delta = queued_player.apply_queued_motion(
        queued_motion);
    CHECK(queued_world_delta == FixedPosition({-1, 0, 0}));
    CHECK(queued_player.position() == FixedPosition({99, 200, 300}));
    CHECK(queued_player.previous_position() == FixedPosition({100, 200, 300}));

    PlayerState queued_frame_player;
    queued_frame_player.set_physics_state(3);
    CHECK(queued_frame_player.set_queued_motion_command(1, 1, 2));
    InputState queued_frame_input;
    queued_frame_input.begin_frame(0);
    PlayerPhysicsFrameHooks queued_frame_hooks{};
    queued_frame_hooks.apply_ground_turn = false;
    queued_frame_hooks.integrate_position = false;
    queued_frame_hooks.integrate_motion_correction = false;
    const auto queued_frame = PlayerPhysicsFrame::step(
        queued_frame_player,
        queued_frame_input,
        queued_frame_hooks);
    CHECK(queued_frame.queued_motion.moved);
    CHECK(queued_frame.queued_motion_world_delta == FixedPosition({0, 1, 0}));
    CHECK(queued_frame_player.position() == FixedPosition({0, 1, 0}));

    PlayerState producer({100, 200, 300});
    producer.set_physics_state(0);
    producer.set_collision_response({100, 0, 0});
    InputState neutral;
    neutral.begin_frame(0);
    PlayerPhysicsFrameHooks producer_hooks{};
    producer_hooks.apply_ground_turn = false;
    producer_hooks.on_stage = [](
        PhysicsDispatchStage stage,
        PlayerState& current_player,
        const InputState&) {
        if (stage == PhysicsDispatchStage::GroundCollision_96550) {
            current_player.set_motion_correction({0x1000, 0, 0});
            current_player.prepare_ground_basis_correction(false);
        }
    };
    const auto produced = PlayerPhysicsFrame::step(
        producer,
        neutral,
        producer_hooks);
    CHECK(produced.position_integrated);
    CHECK(producer.position() == FixedPosition({207, 200, 300}));
    CHECK(producer.collision_response() == FixedPosition({4096, 0, 0}));

    // The unresolved retail ground-motion producer belongs before the shared
    // position integrator. Keep that source injectable until its writer is
    // identified; movement input must not silently become guessed velocity.
    PlayerState prephysics_source({0, 0, 0});
    prephysics_source.set_physics_state(0);
    InputState left_input;
    left_input.begin_frame(movement_bit(MovementAction::Left));
    PlayerPhysicsFrameHooks prephysics_hooks{};
    prephysics_hooks.apply_ground_turn = false;
    prephysics_hooks.on_prephysics = [](
        PlayerState& current_player,
        const InputState&) {
        current_player.set_collision_response({0x100, 0, 0});
    };
    const auto prephysics_frame = PlayerPhysicsFrame::step(
        prephysics_source,
        left_input,
        prephysics_hooks);
    CHECK(prephysics_frame.position_integrated);
    CHECK(prephysics_source.previous_position() == FixedPosition({0, 0, 0}));
    CHECK(prephysics_source.position() == FixedPosition({0x100, 0, 0}));

    // The outer retail wrapper publishes the completed transient correction
    // separately from the persistent response add. Both replay seams run
    // after dispatch and must remain independently observable.
    PlayerState correction_hooks_player;
    correction_hooks_player.set_physics_state(3);
    correction_hooks_player.set_collision_response({1, 2, 3});
    InputState correction_hooks_input;
    correction_hooks_input.begin_frame(0);
    PlayerPhysicsFrameHooks correction_hooks{};
    correction_hooks.integrate_position = false;
    correction_hooks.integrate_motion_correction = false;
    correction_hooks.motion_correction_input = [](
        const PlayerState&,
        const opentony::runtime::PhysicsDispatchResult&) {
        return std::optional<FixedPosition>{FixedPosition({4, 5, 6})};
    };
    correction_hooks.response_correction_input = [](
        const PlayerState&,
        const opentony::runtime::PhysicsDispatchResult&) {
        return std::optional<FixedPosition>{FixedPosition({7, 8, 9})};
    };
    const auto correction_hooks_frame = PlayerPhysicsFrame::step(
        correction_hooks_player,
        correction_hooks_input,
        correction_hooks);
    CHECK(!correction_hooks_frame.position_integrated);
    CHECK(correction_hooks_player.motion_correction() == FixedPosition({4, 5, 6}));
    CHECK(correction_hooks_player.collision_response() == FixedPosition({8, 10, 12}));

    PlayerState profile_player;
    profile_player.set_physics_state(0);
    InputState profile_input;
    profile_input.begin_frame(0x0080U, static_cast<std::int8_t>(-0x29), 0);
    PlayerPhysicsFrameHooks profile_hooks{};
    profile_hooks.apply_ground_turn = false;
    profile_hooks.integrate_position = false;
    profile_hooks.integrate_motion_correction = false;
    profile_hooks.ground_animation_input = [](
        const PlayerState&,
        const InputState&,
        const opentony::runtime::ActionProfileState&) {
        return opentony::runtime::GroundAnimationInput{
            0x2d000,
            0,
            false,
            false,
            false,
            0,
            0,
        };
    };
    profile_hooks.ground_motion_input = [](
        const PlayerState& player,
        const InputState&,
        const opentony::runtime::ActionProfileState& profile) {
        CHECK(profile.slot_at_offset(0x10));
        CHECK(profile.slot_at_offset(0x80));
        CHECK(!profile.slot_at_offset(0x90));
        CHECK(player.animation_state() == 7);
        CHECK(player.animation_frame() == 5);
        return std::optional<opentony::runtime::GroundMotionInput>{};
    };
    const auto profile_frame = PlayerPhysicsFrame::step(
        profile_player,
        profile_input,
        profile_hooks);
    CHECK(profile_frame.action_profile.slot_at_offset(0x10));
    CHECK(profile_frame.action_profile.slot_at_offset(0x80));

    PlayerState stateful_ground_player;
    stateful_ground_player.set_physics_state(0);
    stateful_ground_player.set_collision_response({0x80000, 0, 0});
    InputState stateful_ground_input;
    stateful_ground_input.begin_frame(0);
    PlayerPhysicsFrameHooks stateful_ground_hooks{};
    stateful_ground_hooks.integrate_position = false;
    stateful_ground_hooks.integrate_motion_correction = false;
    stateful_ground_hooks.ground_physics_input = [](
        const PlayerState&,
        const InputState&) {
        return std::optional<opentony::runtime::GroundPhysicsInput>{
            opentony::runtime::GroundPhysicsInput{
                {},
                0,
                0,
                -0x1000,
                0,
                0,
                0,
                0,
                true,
                false,
                false,
                false,
                false,
            },
        };
    };
    const auto stateful_ground_frame = PlayerPhysicsFrame::step(
        stateful_ground_player,
        stateful_ground_input,
        stateful_ground_hooks);
    CHECK(stateful_ground_frame.ground_physics.has_value());
    CHECK(stateful_ground_frame.ground_physics->ground_update_state == 1);
    CHECK(stateful_ground_player.ground_physics_mode() == 1);
    CHECK(stateful_ground_player.collision_response()
        == FixedPosition({0x78000, 0, 0}));
    CHECK(stateful_ground_player.ground_motion_cooldown() == 2);

    PlayerState wide_player;
    wide_player.set_physics_state(0);
    InputState wide_input;
    wide_input.begin_frame(
        opentony::runtime::movement_bit(MovementAction::Right),
        0,
        static_cast<std::int8_t>(0x20));
    PlayerPhysicsFrameHooks wide_hooks{};
    wide_hooks.integrate_position = false;
    wide_hooks.integrate_motion_correction = false;
    bool saw_closed_turn_gate = false;
    wide_hooks.ground_motion_input = [&saw_closed_turn_gate](
        const PlayerState& current_player,
        const InputState&,
        const opentony::runtime::ActionProfileState&) {
        saw_closed_turn_gate =
            current_player.ground_turn_wide_profile() &&
            current_player.ground_turn_policy_changed() &&
            !current_player.ground_motion_correction_gate_open();
        return std::optional<opentony::runtime::GroundMotionInput>{};
    };
    const auto wide_frame = PlayerPhysicsFrame::step(
        wide_player,
        wide_input,
        wide_hooks);
    CHECK(wide_frame.ground_turn.has_value());
    CHECK(wide_frame.ground_turn->wide_profile);
    CHECK(wide_frame.ground_turn->policy_changed);
    CHECK(saw_closed_turn_gate);

    PlayerState normalized_player;
    normalized_player.set_physics_state(0);
    normalized_player.set_collision_response({0x10000, 0, 0});
    InputState normalized_input;
    normalized_input.begin_frame(
        opentony::runtime::movement_bit(MovementAction::Right),
        0,
        static_cast<std::int8_t>(0x20));
    PlayerPhysicsFrameHooks normalized_hooks{};
    normalized_hooks.integrate_position = false;
    normalized_hooks.integrate_motion_correction = false;
    const auto normalized_frame = PlayerPhysicsFrame::step(
        normalized_player,
        normalized_input,
        normalized_hooks);
    CHECK(normalized_frame.ground_turn.has_value());
    CHECK(normalized_frame.ground_turn->response_normalized);
    // The response-normalized write is transient: retail clears +0x58/+0x5c/
    // +0x60 between FUN_00493370 and B010.
    CHECK(normalized_player.motion_correction() == FixedPosition({0, 0, 0}));

    // The live retail frame canonicalizes the captured, slightly-short Q12
    // basis before grounded turn/input processing. Preserve the short values
    // in InitialState, but expose the normalized values after frame 0.
    PlayerState basis_player;
    basis_player.set_physics_state(0);
    opentony::runtime::Q12Matrix3 short_basis{};
    short_basis.at(0, 0) = -4095;
    short_basis.at(0, 2) = -6;
    short_basis.at(1, 1) = -4096;
    short_basis.at(2, 0) = 6;
    short_basis.at(2, 2) = -4095;
    basis_player.set_orientation(short_basis);
    InputState basis_input;
    basis_input.begin_frame(0);
    PlayerPhysicsFrameHooks basis_hooks{};
    basis_hooks.integrate_position = false;
    basis_hooks.integrate_motion_correction = false;
    static_cast<void>(PlayerPhysicsFrame::step(
        basis_player,
        basis_input,
        basis_hooks));
    CHECK(basis_player.orientation().values
        == (std::array<std::int16_t, 9>{
            -4096, 0, -6,
            0, -4096, 0,
            6, 0, -4096,
        }));

    PlayerState cooldown_player;
    cooldown_player.set_physics_state(0);
    cooldown_player.set_ground_motion_cooldown(2);
    InputState cooldown_input;
    cooldown_input.begin_frame(0);
    PlayerPhysicsFrameHooks cooldown_hooks{};
    cooldown_hooks.apply_ground_turn = false;
    cooldown_hooks.integrate_position = false;
    cooldown_hooks.integrate_motion_correction = false;
    bool saw_cooldown = false;
    cooldown_hooks.ground_motion_input = [&saw_cooldown](
        const PlayerState& current_player,
        const InputState&,
        const opentony::runtime::ActionProfileState&) {
        saw_cooldown = current_player.ground_motion_cooldown() == 1;
        return std::optional<opentony::runtime::GroundMotionInput>{};
    };
    static_cast<void>(PlayerPhysicsFrame::step(
        cooldown_player,
        cooldown_input,
        cooldown_hooks));
    CHECK(saw_cooldown);

    PlayerState surface_player({0, 4096, 0});
    surface_player.set_physics_state(3);
    surface_player.set_collision_response({0, -8192, 0});
    InputState surface_input;
    surface_input.begin_frame(0);
    PlayerPhysicsFrameHooks surface_hooks{};
    surface_hooks.collision_query = [](
        const FixedPosition& start,
        const FixedPosition& end) -> std::optional<opentony::runtime::PositionCollisionHit> {
        if (start[1] >= 0 && end[1] < 0) {
            return opentony::runtime::PositionCollisionHit{
                7,
                3,
                2,
                11,
                0x2000,
                {0, 0, 0},
                {0, 0x1000, 0},
                0x20,
                0x1234,
            };
        }
        return std::nullopt;
    };
    bool saw_surface = false;
    surface_hooks.on_collision = [&saw_surface](
        PlayerState&,
        const opentony::runtime::PositionCollisionHit& hit,
        const opentony::runtime::PositionCommitResult&) {
        saw_surface = hit.surface_flags == 0x1234;
    };
    const auto surface_frame = PlayerPhysicsFrame::step(
        surface_player,
        surface_input,
        surface_hooks);
    CHECK(surface_frame.collision_hit.has_value());
    CHECK(surface_frame.hit_normal_removed);
    CHECK(saw_surface);
    CHECK(surface_player.position() == FixedPosition({0, 4096, 0}));
    CHECK(surface_player.collision_response() == FixedPosition({0, 0, 0}));

    PlayerState response_player({0, 100, 0});
    response_player.set_physics_state(0);
    response_player.set_collision_response({0, -0x1000, 0});
    PlayerPhysicsFrameHooks response_hooks{};
    response_hooks.remove_hit_normal_component = false;
    response_hooks.integrate_motion_correction = false;
    response_hooks.collision_query = [](
        const FixedPosition& start,
        const FixedPosition& end)
        -> std::optional<opentony::runtime::PositionCollisionHit> {
        if (start[1] > 0 && end[1] <= 0) {
            return opentony::runtime::PositionCollisionHit{
                1, 2, 3, 4, 0x2000, {0, 0, 0}, {0, 0x1000, 0}, 0, 0};
        }
        return std::nullopt;
    };
    response_hooks.collision_response_bias_q12 = [](
        const PlayerState&,
        const opentony::runtime::PositionCollisionHit&,
        PhysicsDispatchStage) {
        return std::optional<std::int32_t>{0xcd};
    };
    const auto response_frame = PlayerPhysicsFrame::step(
        response_player,
        surface_input,
        response_hooks);
    CHECK(response_frame.collision_response.has_value());
    CHECK(response_frame.collision_response->adjusted);
    CHECK(response_player.collision_response()[1] == 0xcd);

    PlayerState gravity_player;
    gravity_player.set_physics_state(3);
    gravity_player.set_air_motion({100, -1000, 300});
    PlayerPhysicsFrameHooks gravity_hooks{};
    gravity_hooks.integrate_position = false;
    gravity_hooks.air_gravity_input = [](
        const PlayerState&,
        const InputState&) {
        return std::optional<opentony::runtime::AirGravityConfig>{};
    };
    const auto gravity_disabled = PlayerPhysicsFrame::step(
        gravity_player,
        surface_input,
        gravity_hooks);
    CHECK(!gravity_disabled.air_gravity.has_value());
    CHECK(gravity_player.air_motion() == FixedPosition({100, -1000, 300}));

    gravity_hooks.air_gravity_input = [](
        const PlayerState&,
        const InputState&) {
        return std::optional<opentony::runtime::AirGravityConfig>{
            opentony::runtime::AirGravityConfig{500, -0x1000, -0xe0c, true}};
    };
    gravity_hooks.apply_air_motion_basis = true;
    const auto gravity_applied = PlayerPhysicsFrame::step(
        gravity_player,
        surface_input,
        gravity_hooks);
    CHECK(gravity_applied.air_gravity.has_value());
    CHECK(gravity_applied.air_motion_basis.has_value());
    CHECK(gravity_player.air_motion()
        == opentony::runtime::q12_normalize({100, -1500, 300}));

    PlayerState air_input_player;
    air_input_player.set_physics_state(3);
    InputState air_input;
    air_input.begin_frame(movement_bit(MovementAction::Up));
    PlayerPhysicsFrameHooks air_input_hooks{};
    air_input_hooks.integrate_position = false;
    air_input_hooks.integrate_motion_correction = false;
    air_input_hooks.air_direction_input = [](
        const PlayerState&,
        const InputState&) {
        return std::optional<opentony::runtime::AirDirectionInputConfig>{
            opentony::runtime::AirDirectionInputConfig{100, 150}};
    };
    const auto air_input_frame = PlayerPhysicsFrame::step(
        air_input_player,
        air_input,
        air_input_hooks);
    CHECK(air_input_frame.air_direction_input.has_value());
    CHECK(air_input_frame.air_direction_input->delta
        == FixedPosition({0, 0, 150}));
    CHECK(air_input_player.motion_correction()
        == FixedPosition({0, 0, -150}));

    PlayerState stat_air_player;
    stat_air_player.set_physics_state(3);
    InputState stat_air_input;
    stat_air_input.begin_frame(movement_bit(MovementAction::Up));
    PlayerPhysicsFrameHooks stat_air_hooks{};
    stat_air_hooks.integrate_position = false;
    stat_air_hooks.integrate_motion_correction = false;
    stat_air_hooks.air_speed_input = [](
        const PlayerState&,
        const InputState&) {
        return std::optional<opentony::runtime::AirSpeedConfig>{
            opentony::runtime::AirSpeedConfig{100, 0, 3, 0, false, false, false}};
    };
    const auto stat_air_frame = PlayerPhysicsFrame::step(
        stat_air_player,
        stat_air_input,
        stat_air_hooks);
    CHECK(stat_air_frame.air_direction_input.has_value());
    CHECK(stat_air_frame.air_direction_input->delta
        == FixedPosition({0, 0, 19500}));

    PlayerState air_control_player;
    air_control_player.set_physics_state(3);
    air_control_player.set_collision_response({3200, 0, 0});
    InputState air_control_input;
    air_control_input.begin_frame(opentony::runtime::kKickActionBit);
    PlayerPhysicsFrameHooks air_control_hooks{};
    air_control_hooks.integrate_motion_correction = false;
    air_control_hooks.air_action_control_input = [](
        const PlayerState&,
        const InputState&) {
        return std::optional<opentony::runtime::AirActionControlConfig>{
            opentony::runtime::AirActionControlConfig{
                1000, true, false, false, false, false, false}};
    };
    const auto air_control_frame = PlayerPhysicsFrame::step(
        air_control_player,
        air_control_input,
        air_control_hooks);
    CHECK(air_control_frame.air_action_control.has_value());
    CHECK(air_control_frame.air_action_control->applied);
    CHECK(air_control_player.motion_correction()
        == FixedPosition({-100, 1800, 0}));

    PlayerState replay_player({0, 0, 0});
    replay_player.set_physics_state(3);
    replay_player.set_collision_response({100, 0, 0});
    const std::vector<opentony::runtime::PlayerReplayInput> replay_inputs{
        {0, 0, 0, 0x100},
        {movement_bit(MovementAction::Right), 0, 0, 0x100},
        {0, 0, 0, 0x80},
    };
    PlayerPhysicsFrameHooks replay_hooks{};
    replay_hooks.apply_ground_turn = false;
    const auto replay_a = opentony::runtime::PlayerPhysicsReplay::run(
        replay_player,
        replay_inputs,
        replay_hooks);
    const auto replay_b = opentony::runtime::PlayerPhysicsReplay::run(
        replay_player,
        replay_inputs,
        replay_hooks);
    CHECK(replay_a == replay_b);
    CHECK(replay_a.size() == replay_inputs.size());
    CHECK(replay_a[0].action_mask == 0);
    CHECK(replay_a[1].effective_movement_mask
        == movement_bit(MovementAction::Right));
    CHECK(replay_a[0].position == FixedPosition({100, 0, 0}));
    CHECK(replay_a[2].previous_position == replay_a[1].position);

    PlayerState ollie_player;
    ollie_player.set_physics_state(0);
    PlayerPhysicsFrameHooks ollie_hooks{};
    ollie_hooks.ollie_input = [](
        const PlayerState&,
        const InputState&) {
        return opentony::runtime::OlliePrePhysicsInput{};
    };
    InputState kick_input;
    kick_input.begin_frame(opentony::runtime::kKickActionBit);
    const auto charge_frame = PlayerPhysicsFrame::step(
        ollie_player,
        kick_input,
        ollie_hooks);
    CHECK(charge_frame.ollie.has_value());
    CHECK(charge_frame.ollie->event
        == opentony::runtime::OlliePrePhysicsEvent::Charging);
    kick_input.begin_frame(0);
    const auto launch_frame = PlayerPhysicsFrame::step(
        ollie_player,
        kick_input,
        ollie_hooks);
    CHECK(launch_frame.ollie.has_value());
    CHECK(launch_frame.ollie->event
        == opentony::runtime::OlliePrePhysicsEvent::Launched);
    CHECK(ollie_player.physics_state() == 1);
    CHECK(ollie_player.last_state_request().reason
        == opentony::runtime::kOrdinaryLaunchReason);

    PlayerState landing_player({0, 100, 0});
    landing_player.set_physics_state(3);
    landing_player.set_collision_response({0, -200, 0});
    InputState landing_input;
    landing_input.begin_frame(0);
    PlayerPhysicsFrameHooks landing_hooks{};
    landing_hooks.collision_query = [](
        const FixedPosition& start,
        const FixedPosition& end)
        -> std::optional<opentony::runtime::PositionCollisionHit> {
        if (start[1] > 0 && end[1] <= 0) {
            return opentony::runtime::PositionCollisionHit{
                1, 2, 3, 4, 0x2000, {0, 0, 0}, {0, 0x1000, 0}, 0, 0};
        }
        return std::nullopt;
    };
    landing_hooks.on_air_contact = [](
        PlayerState&,
        const opentony::runtime::PositionCollisionHit& hit,
        const opentony::runtime::PositionCommitResult&) {
        return hit.normal[1] > 0;
    };
    const auto landed = PlayerPhysicsFrame::step(
        landing_player,
        landing_input,
        landing_hooks);
    CHECK(landed.landed);
    CHECK(landing_player.physics_state() == 0);
    CHECK(landing_player.last_state_request().reason
        == opentony::runtime::kLandingReason);

    PlayerState brake_player({0, 0, 0});
    brake_player.set_physics_state(0);
    PlayerPhysicsFrameHooks brake_hooks{};
    brake_hooks.apply_ground_turn = false;
    brake_hooks.integrate_position = false;
    brake_hooks.ground_brake_input = [](
        const PlayerState&,
        const InputState&) -> std::optional<opentony::runtime::GroundBrakeInput> {
        return opentony::runtime::GroundBrakeInput{
            {},
            0,
            -0x1000,
            0x100,
            0,
            0,
            true,
        };
    };
    const auto braked = PlayerPhysicsFrame::step(
        brake_player,
        landing_input,
        brake_hooks);
    CHECK(braked.ground_brake.has_value());
    CHECK(braked.ground_brake->requested_state7);
    CHECK(brake_player.physics_state() == 7);
    CHECK(brake_player.last_state_request().reason
        == opentony::runtime::kGroundStopReason);
    CHECK(braked.dispatch.state == 7);

    PlayerState damping_player;
    damping_player.set_collision_response({30, -30, 20});
    PlayerPhysicsFrameHooks damping_hooks{};
    damping_hooks.velocity_damping_input = [](
        const PlayerState&,
        const opentony::runtime::PhysicsDispatchResult&) {
        opentony::runtime::VelocityDampingInput config{};
        return std::optional<opentony::runtime::VelocityDampingInput>{config};
    };
    const auto damping_frame = PlayerPhysicsFrame::step(
        damping_player,
        landing_input,
        damping_hooks);
    CHECK(damping_frame.velocity_damped);
    CHECK(damping_player.collision_response()
        == FixedPosition({23, -23, 0}));

    std::cout << "Physics frame tests passed\n";
}
