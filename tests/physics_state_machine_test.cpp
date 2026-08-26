#include "physics_state_machine.hpp"

#include <cassert>
#include <cstdint>
#include <vector>

using namespace opentony::physics;

namespace {

struct CallbackLog {
    int launch_calls = 0;
    int dispatch_calls = 0;
    int position_calls = 0;
    std::int32_t callback_state_at_position = -1;
    DispatchResult last_dispatch{};
    std::vector<int> frame_stages;
};

void test_retail_fixed12_math_and_orientation_layout() {
    assert(fixed12_dot(FixedVec3{2048, 0, 0}, FixedVec3{1, 0, 0}) == 0);
    assert(fixed12_dot(FixedVec3{2048, 0, 0}, FixedVec3{3, 0, 0}) == 1);
    assert(fixed12_scalar_multiply(3, 2048) == 1);

    const PackedSurfaceNormal packed{0x80001000u, 0x0000c000u};
    assert((decode_surface_normal(packed) == FixedVec3{0x1000, -0x8000, -0x4000}));
    const auto projected = project_vector_off_surface(
        FixedVec3{0x2000, 0x1000, 0}, PackedSurfaceNormal{0x00001000u, 0});
    assert((projected == FixedVec3{0, 0x1000, 0}));

    const auto preserved = project_velocity_preserving_speed(
        FixedVec3{0x4000, 0x1000, 0}, PackedSurfaceNormal{0x00001000u, 0});
    assert(preserved.original_speed_metric == 16832);
    assert(preserved.projected_speed_metric == 4096);
    assert(preserved.speed_rescaled);
    assert((preserved.velocity == FixedVec3{0, 0x4100, 0}));

    const auto basis = copy_orientation_basis(
        std::array<std::int16_t, 9>{1, 2, 3, 4, 5, 6, 7, 8, 9});
    assert((basis.basis_30f4 == FixedVec3{3, 6, 9}));
    assert((basis.basis_3100 == FixedVec3{1, 4, 7}));
    assert((basis.basis_310c == FixedVec3{2, 5, 8}));
}

void test_in_air_orientation_preparation() {
    assert((normalize_retail_fixed12(FixedVec3{}) ==
            FixedVec3{0x1000, 0, 0}));
    assert((normalize_retail_fixed12(FixedVec3{0x10000, 0, 0}) ==
            FixedVec3{0x1000, 0, 0}));
    assert((fixed12_cross(FixedVec3{0x1000, 0, 0},
                         FixedVec3{0, 0x1000, 0}) ==
            FixedVec3{0, 0, 0x1000}));
    assert((clamp_cross_scratch_to_int16(
                FixedVec3{0x9000, -0x9000, 0x1234}) ==
            FixedVec3{0x7fff, -0x8000, 0x1234}));

    PhysicsStateMachine machine;
    machine.state().basis_30f4 = FixedVec3{0x1000, 0, 0};
    machine.state().basis_310c = FixedVec3{0, 1000, 0};
    const auto prepared = machine.prepare_in_air_orientation();
    assert(prepared.handled);
    assert(!prepared.rolling_axis_reset);
    assert((prepared.basis.basis_310c == FixedVec3{0, 0x1000, 0}));
    assert((prepared.basis.basis_3100 == FixedVec3{0, 0, 0x1000}));
    assert((prepared.basis.basis_30f4 == FixedVec3{0x1000, 0, 0}));
    assert((machine.state().orientation_shorts ==
            std::array<std::int16_t, 9>{0, 0, 0x1000, 0, 0x1000,
                                        0, 0x1000, 0, 0}));
    const auto republished =
        copy_orientation_basis(machine.state().orientation_shorts);
    assert(republished.basis_30f4 == prepared.basis.basis_30f4);
    assert(republished.basis_3100 == prepared.basis.basis_3100);
    assert(republished.basis_310c == prepared.basis.basis_310c);

    machine.state().basis_310c = FixedVec3{123, -4000, 456};
    const auto reset = machine.prepare_in_air_orientation();
    assert(reset.rolling_axis_reset);
    assert((reset.basis.basis_310c == FixedVec3{0, -0x1000, 0}));
}

void test_orientation_recovery() {
    PhysicsStateMachine machine;
    // Current object +0x2e5c/+0x2e62/+0x2e68 axis is +X. The target is +Y;
    // the two cross products therefore rebuild +Z/+X/+Y in the retail field
    // order used by FUN_0049d080.
    machine.state().orientation_shorts = {
        0, 0, 0x1000,
        0, 0, 0,
        0, 0, 0,
    };
    machine.state().orientation_target_shorts = {0, 0x1000, 0};
    machine.state().orientation_recovery_progress = 0x18000;
    const auto recovered = machine.apply_orientation_recovery();
    assert(recovered.handled);
    assert(!recovered.skipped);
    assert((recovered.target == FixedVec3{0, 0x1000, 0}));
    assert((recovered.basis.basis_30f4 == FixedVec3{0x1000, 0, 0}));
    assert((recovered.basis.basis_3100 == FixedVec3{0, 0, 0x1000}));
    assert((recovered.basis.basis_310c == FixedVec3{0, 0x1000, 0}));
    assert((machine.state().orientation_shorts ==
            std::array<std::int16_t, 9>{0, 0, 0x1000, 0, 0x1000,
                                        0, 0x1000, 0, 0}));
    assert(machine.state().orientation_recovery_base == recovered.target);

    machine.state().orientation_recovery_progress = 0x18001;
    machine.state().orientation_target_shorts = {0x1000, 0, 0};
    const auto skipped = machine.apply_orientation_recovery();
    assert(!skipped.handled);
    assert(skipped.skipped);
    assert(machine.state().basis_310c == recovered.basis.basis_310c);
}

void test_upright_correction() {
    PhysicsStateMachine machine;
    machine.state().raw_state = 1;
    machine.state().basis_30f4 = FixedVec3{0x1000, 0, 0};
    machine.state().basis_310c = FixedVec3{0, 0x1000, 0};
    machine.state().orientation_shorts = {
        0x1000, 0, 0,
        0, 0x1000, 0,
        0, 0, 0x1000,
    };

    // +X cross -Z points along +Y, so the +0x29 threshold takes the +0xb
    // rotation branch in FUN_0049c330.
    const auto rotated = machine.apply_upright_correction(
        FixedVec3{0, 0, -0x1000});
    assert(rotated.handled);
    assert(rotated.rotation_applied);
    assert(rotated.cross_dot == 0x1000);
    assert(rotated.rotation_angle == 0xb);
    assert((machine.state().orientation_shorts ==
            std::array<std::int16_t, 9>{4095, -69, 0,
                                        69, 4095, 0,
                                        0, 0, 4096}));
    assert((rotated.basis.basis_3100 == FixedVec3{4095, 69, 0}));
    assert((rotated.basis.basis_310c == FixedVec3{-69, 4095, 0}));
    assert((rotated.basis.basis_30f4 == FixedVec3{0, 0, 4096}));

    PhysicsStateMachine negative;
    negative.state().raw_state = 1;
    negative.state().basis_30f4 = FixedVec3{0x1000, 0, 0};
    negative.state().basis_310c = FixedVec3{0, 0x1000, 0};
    negative.state().orientation_shorts = {
        0x1000, 0, 0,
        0, 0x1000, 0,
        0, 0, 0x1000,
    };
    const auto negative_result = negative.apply_upright_correction(
        FixedVec3{0, 0, 0x1000});
    assert(negative_result.rotation_angle == -0xb);
    assert((negative.state().orientation_shorts ==
            std::array<std::int16_t, 9>{4095, 69, 0,
                                        -69, 4095, 0,
                                        0, 0, 4096}));

    negative.state().raw_state = 2;
    const auto skipped = negative.apply_upright_correction(
        FixedVec3{0, 0, -0x1000});
    assert(!skipped.handled);
}

void test_air_collision_recovery() {
    PhysicsStateMachine transient;
    transient.state().raw_state = 1;
    transient.state().ollie.mode_latched = 1;
    const auto transient_result = transient.handle_air_collision_recovery(
        AirCollisionRecoveryInput{10, 0, false, FixedVec3{0x1000, 0, 0}});
    assert(transient_result.handled);
    assert(transient_result.transient_requested);
    assert(!transient_result.recovery_requested);
    assert(transient_result.request.from == 1);
    assert(transient_result.request.to == 2);
    assert(transient_result.request.reason ==
           retail::kAirCollisionTransientReason);
    assert(transient_result.request.request_callsite ==
           retail::kAirCollisionTransientRequestCallsite);

    PhysicsStateMachine recovery;
    recovery.state().raw_state = 1;
    recovery.state().velocity = FixedVec3{0x4000, 0, 0};
    recovery.state().ollie.mode_latched = 1;
    recovery.state().ollie.recovery_latch = 1;
    recovery.state().ollie.in_progress = 1;
    recovery.update_action_mask(retail::kUpActionBit);
    const auto recovery_result = recovery.handle_air_collision_recovery(
        AirCollisionRecoveryInput{20, 10, false, FixedVec3{0x1000, 0, 0x0800}});
    assert(recovery_result.handled);
    assert(recovery_result.recovery_requested);
    assert(recovery_result.velocity_adjusted);
    assert(recovery_result.request.from == 1);
    assert(recovery_result.request.to == 1);
    assert(recovery_result.request.reason == retail::kAirCollisionRecoveryReason);
    assert(recovery_result.request.request_callsite ==
           retail::kAirCollisionRecoveryRequestCallsite);
    assert((recovery.state().velocity == FixedVec3{0x3000, 0, -0x0800}));
    assert(recovery.state().ollie.mode == 0);
    assert(!recovery_result.in_progress_set);
}

void test_blocked_physics_reset() {
    PhysicsStateMachine machine;
    machine.state().prephysics_blocked = 1;
    machine.state().acceleration = FixedVec3{11, 22, 33};
    machine.state().velocity = FixedVec3{1000, -500, 3000};
    machine.state().ollie.latched = 1;
    machine.state().ollie.in_progress = 1;

    const auto reset = machine.apply_blocked_physics_reset(
        BlockedPhysicsInput{0x100, 10, true});
    assert(reset.handled);
    assert(reset.acceleration_xz_cleared);
    assert(reset.launch_bookkeeping_cleared);
    assert(reset.vertical_velocity_clamped);
    assert(reset.velocity_decayed);
    assert((machine.state().acceleration == FixedVec3{0, 22, 0}));
    assert((machine.state().velocity == FixedVec3{900, 0, 2700}));
    assert(machine.state().ollie.latched == 0);
    assert(machine.state().ollie.in_progress == 0);

    PhysicsStateMachine no_op;
    no_op.state().velocity = FixedVec3{100, 200, 300};
    assert(!no_op.apply_blocked_physics_reset(
                       BlockedPhysicsInput{0x100, 1, true})
                .handled);
    assert((no_op.state().velocity == FixedVec3{100, 200, 300}));
}

void test_collision_recovery_window() {
    PhysicsStateMachine machine;
    machine.state().heading_deadband = -0x29;
    machine.state().heading_input = 0;
    machine.update_action_mask(retail::kUpActionBit);
    assert(machine.update_collision_recovery_window(12));
    assert(machine.state().collision_recovery_frame == 12);
    assert(machine.update_collision_recovery_window(15));
    assert(machine.state().collision_recovery_frame == 12);

    machine.update_action_mask(retail::kLeftActionBit);
    assert(!machine.update_collision_recovery_window(16));
    assert(machine.state().collision_recovery_frame == 0);

    machine.update_action_mask(retail::kUpActionBit);
    machine.state().heading_input = 0x32;
    assert(!machine.update_collision_recovery_window(17));
    assert(machine.state().collision_recovery_frame == 0);
}

void test_cross_build_physics_state_labels() {
    assert(classify_physics_state(retail::kPhysicsOnGround) ==
           PhysicsStateSemantic::OnGround);
    assert(classify_physics_state(retail::kPhysicsInAir) ==
           PhysicsStateSemantic::InAir);
    assert(classify_physics_state(retail::kPhysicsOnInvisible) ==
           PhysicsStateSemantic::OnInvisible);
    assert(classify_physics_state(retail::kPhysicsInAirStickTo) ==
           PhysicsStateSemantic::InAirStickTo);
    assert(classify_physics_state(retail::kPhysicsOnRail) ==
           PhysicsStateSemantic::OnRail);
    assert(classify_physics_state(retail::kPhysicsInWallride) ==
           PhysicsStateSemantic::InWallride);
    assert(classify_physics_state(retail::kPhysicsInFootplant) ==
           PhysicsStateSemantic::InFootplant);
    assert(classify_physics_state(retail::kPhysicsStopped) ==
           PhysicsStateSemantic::Stopped);
    assert(classify_physics_state(retail::kPhysicsInHandplant) ==
           PhysicsStateSemantic::InHandplant);
    assert(classify_physics_state(-1) == PhysicsStateSemantic::Unknown);

    PhysicsStateMachine machine;
    machine.state().raw_state = retail::kPhysicsOnRail;
    const auto dispatch = machine.dispatch();
    assert(dispatch.semantic_state == PhysicsStateSemantic::OnRail);
}

void test_observed_state4_transition_path() {
    PhysicsStateMachine machine;
    machine.state().raw_state = retail::kPhysicsOnGround;

    const auto transient = machine.enter_collision_transient();
    assert(transient.from == retail::kPhysicsOnGround);
    assert(transient.to == retail::kPhysicsOnInvisible);
    assert(transient.reason == retail::kCollisionTransientEnterReason);
    assert(transient.request_callsite ==
           retail::kCollisionTransientEnterCallsite);

    const auto transient_dispatch = machine.dispatch();
    assert(transient_dispatch.kind == DispatchKind::CollisionTransient);
    assert(transient_dispatch.handler_count == 1);
    assert(transient_dispatch.handler_pcs[0] == 0x00496550);
    assert(machine.state().auxiliary == 1);

    const auto entered = machine.enter_state4_from_collision();
    assert(entered.from == retail::kPhysicsOnInvisible);
    assert(entered.to == retail::kPhysicsOnRail);
    assert(entered.reason == retail::kState4EnterReason);
    assert(entered.request_callsite == retail::kState4EnterRequestCallsite);
    assert(entered.writer_pc == retail::kStateWriter);
    assert(machine.state().phase_state == retail::kPhysicsOnInvisible);

    const auto dispatch = machine.dispatch();
    assert(dispatch.raw_state == retail::kPhysicsOnRail);
    assert(dispatch.kind == DispatchKind::State4);
    assert(dispatch.handler_count == 1);
    assert(dispatch.handler_pcs[0] == 0x00494210);

    const auto exited = machine.leave_state4_to_air();
    assert(exited.from == retail::kPhysicsOnRail);
    assert(exited.to == retail::kPhysicsInAir);
    assert(exited.reason == retail::kState4ExitReason);
    assert(exited.request_callsite == retail::kState4ExitRequestCallsite);
    assert(machine.state().raw_state == retail::kPhysicsInAir);
    assert(machine.state().prephysics_blocked == 1);
    assert(machine.state().off_ground.transition_count_302c == 1);
    assert(machine.state().off_ground.gravity_percent_29f4 == 100);

    const auto air = machine.dispatch();
    assert(air.kind == DispatchKind::InAir);
    assert(air.handler_pcs[0] == retail::kInAirHandler);

    assert(machine.accept_air_contact(true, FixedVec3{11, 22, 33}));
    assert(machine.state().raw_state == retail::kPhysicsOnGround);
    assert(machine.requests().size() == 4);
    assert(machine.requests()[3].from == retail::kPhysicsInAir);
    assert(machine.requests()[3].to == retail::kPhysicsOnGround);
    assert(machine.requests()[3].reason == retail::kLandingReason);
    assert(machine.requests()[3].request_callsite ==
           retail::kLandingRequestCallsite);
}

void test_ground_to_air_leave_ground_transition() {
    PhysicsStateMachine machine;
    machine.state().raw_state = retail::kPhysicsOnGround;
    machine.state().velocity.y = -1234;

    const auto transitioned = machine.try_ground_to_air(
        GroundAirTransitionInput{100, 0x100, 1001, 0x5001, 0});
    assert(transitioned.eligible);
    assert(transitioned.transitioned);
    assert(transitioned.vertical_velocity_clamped);
    assert(transitioned.request.from == retail::kPhysicsOnGround);
    assert(transitioned.request.to == retail::kPhysicsInAir);
    assert(transitioned.request.reason == retail::kGroundLeaveAirReason);
    assert(transitioned.request.request_callsite ==
           retail::kGroundLeaveAirRequestCallsite);
    assert(transitioned.off_ground.handled);
    assert(transitioned.off_ground.mode == 0x14);
    assert(transitioned.off_ground.invocation_callsite ==
           retail::kGroundLeaveAirOffGroundCallsite);
    assert(transitioned.off_ground.request.from == retail::kPhysicsInAir);
    assert(transitioned.off_ground.request.to == retail::kPhysicsInAir);
    assert(transitioned.off_ground.request.reason == retail::kState4ExitReason);
    assert(transitioned.off_ground.request.request_callsite ==
           retail::kState4ExitRequestCallsite);
    assert(machine.state().off_ground.marker_3204 == 0x28);
    assert(machine.state().phase_state == retail::kPhysicsInAir);
    assert(machine.requests().size() == 2);
    assert(machine.state().velocity.y == 0);

    PhysicsStateMachine recent;
    recent.state().raw_state = retail::kPhysicsOnGround;
    recent.state().velocity.y = 4;
    const auto recent_result = recent.try_ground_to_air(
        GroundAirTransitionInput{100, 0x100, 1001, 0x5000, 99});
    assert(recent_result.eligible);
    assert(recent_result.transitioned);
    assert(!recent_result.vertical_velocity_clamped);

    PhysicsStateMachine rejected;
    rejected.state().raw_state = retail::kPhysicsOnGround;
    const auto rejected_result = rejected.try_ground_to_air(
        GroundAirTransitionInput{100, 0x100, 1000, 0x5001, 0});
    assert(!rejected_result.eligible);
    assert(rejected.state().raw_state == retail::kPhysicsOnGround);
}

void test_off_ground_reset_boundary() {
    PhysicsStateMachine machine;
    machine.state().raw_state = retail::kPhysicsOnRail;
    machine.state().ollie.landing_contact_auxiliary = 1;
    machine.state().ollie.special_mode = 2;
    machine.state().ollie.action_context = 3;
    machine.state().ollie.latched = 4;
    machine.state().ollie.charge = 5;
    machine.state().ollie.recovery_latch = 6;
    machine.state().off_ground.word_2c64 = 7;
    machine.state().off_ground.word_2c8c = 8;
    machine.state().off_ground.word_29e0 = 9;
    machine.state().off_ground.word_2bd8 = 10;
    machine.state().off_ground.word_2ec8 = 11;
    machine.state().off_ground.word_2e90 = 12;
    machine.state().off_ground.word_29d4 = 13;
    machine.state().off_ground.word_29d0 = 14;
    machine.state().off_ground.word_29f0 = 15;
    machine.state().off_ground.gravity_percent_29f4 = 16;
    machine.state().off_ground.word_2dd0 = 17;
    machine.state().off_ground.word_2f68 = 18;
    machine.state().off_ground.word_2e94 = 19;

    const auto result = machine.apply_off_ground_transition(
        OffGroundTransitionInput{0x14, 23, 0x00491234});
    assert(result.handled);
    assert(result.bookkeeping_reset);
    assert(result.mode == 0x14);
    assert(result.invocation_callsite == 0x00491234);
    assert(result.request.from == retail::kPhysicsOnRail);
    assert(result.request.to == retail::kPhysicsInAir);
    assert(result.request.reason == retail::kState4ExitReason);
    assert(result.request.request_callsite == retail::kState4ExitRequestCallsite);
    assert(machine.state().prephysics_blocked == 1);
    assert(machine.state().off_ground.control_argument_2f60 == 23);
    assert(machine.state().off_ground.transition_count_302c == 1);
    assert(machine.state().off_ground.gravity_percent_29f4 == 100);
    assert(machine.state().ollie.landing_contact_auxiliary == 0);
    assert(machine.state().ollie.special_mode == 0);
    assert(machine.state().ollie.action_context == 0);
    assert(machine.state().ollie.latched == 0);
    assert(machine.state().ollie.charge == 0);
    assert(machine.state().ollie.recovery_latch == 0);
    assert(machine.state().off_ground.word_2c64 == 0);
    assert(machine.state().off_ground.word_2c8c == 0);
    assert(machine.state().off_ground.word_29e0 == 0);
    assert(machine.state().off_ground.word_2bd8 == 0);
    assert(machine.state().off_ground.word_2ec8 == 0);
    assert(machine.state().off_ground.word_2e90 == 0);
    assert(machine.state().off_ground.word_29d4 == 0);
    assert(machine.state().off_ground.word_29d0 == 0);
    assert(machine.state().off_ground.word_29f0 == 0);
    assert(machine.state().off_ground.word_2dd0 == 0);
    assert(machine.state().off_ground.word_2f68 == 0);
    assert(machine.state().off_ground.word_2e94 == 0);
}

void test_landing_cleanup_boundary() {
    PhysicsStateMachine machine;
    machine.state().raw_state = retail::kPhysicsInAir;
    machine.state().jump.held = 0;
    machine.state().ollie.animation_gate = 7;
    machine.state().ollie.landing_effect_state = 1;
    machine.state().off_ground.word_2e90 = 1;
    machine.state().off_ground.word_2e94 = 1;
    machine.state().off_ground.word_2ec4 = 2;

    const auto landing = machine.apply_landing_cleanup();
    assert(landing.handled);
    assert(landing.animation_gate_cleared);
    assert(landing.timer_decremented);
    assert(landing.landing_marker_consumed);
    assert(landing.recovery_marker_cleared);
    assert(landing.returned_early);
    assert(machine.state().ollie.animation_gate == 0);
    assert(machine.state().ollie.landing_effect_state == 0);
    assert(machine.state().off_ground.word_2e90 == 0);
    assert(machine.state().off_ground.word_2e94 == 0);
    assert(machine.state().off_ground.word_2ec4 == 1);

    machine.state().off_ground.word_2e34 = 9;
    const auto no_jump_collision_gate = machine.apply_landing_cleanup();
    assert(no_jump_collision_gate.collision_gate_cleared);
    assert(machine.state().off_ground.word_2e34 == 0);

    // Once the landing marker has drained, the ordinary no-jump gate still
    // clears a pending recovery marker and returns before deeper animation
    // selection.
    machine.state().off_ground.word_2e94 = 1;
    const auto recovery = machine.apply_landing_cleanup();
    assert(recovery.handled);
    assert(!recovery.landing_marker_consumed);
    assert(recovery.recovery_marker_cleared);
    assert(recovery.returned_early);
    assert(machine.state().off_ground.word_2e94 == 0);

    PhysicsStateMachine recovery_reset;
    recovery_reset.state().raw_state = retail::kPhysicsOnGround;
    recovery_reset.state().jump.held = 1;
    recovery_reset.state().off_ground.word_2e94 = 1;
    recovery_reset.state().off_ground.word_29f2 = 5;
    recovery_reset.state().off_ground.word_2e2c = 0;
    recovery_reset.state().ollie.recovery_latch = 1;
    const auto reset = recovery_reset.apply_landing_cleanup();
    assert(reset.recovery_reset_requested);
    assert(reset.off_ground.handled);
    assert(reset.off_ground.mode == 5);
    assert(reset.off_ground.invocation_callsite ==
           retail::kLandingRecoveryOffGroundCallsite);
    assert(reset.off_ground.request.from == retail::kPhysicsOnGround);
    assert(reset.off_ground.request.to == retail::kPhysicsInAir);
    assert(reset.off_ground.request.reason == retail::kOffGroundReason);
    assert(reset.off_ground.request.request_callsite ==
           retail::kOffGroundStateRequestCallsite);
    assert(recovery_reset.state().off_ground.word_29f2 == 0);
    assert(recovery_reset.state().off_ground.counter_3034 == 1);

    PhysicsStateMachine trick_scan;
    trick_scan.state().raw_state = retail::kPhysicsOnGround;
    trick_scan.state().jump.held = 1;
    trick_scan.state().off_ground.word_2e94 = 1;
    trick_scan.state().off_ground.word_2c88 = 1;
    trick_scan.state().off_ground.word_2c90 = 0;
    trick_scan.state().off_ground.word_2c8c = 500;
    trick_scan.state().off_ground.word_2c6c = 1;
    trick_scan.state().ollie.recovery_latch = 1;
    trick_scan.state().off_ground.word_2e2c = 1;
    trick_scan.state().off_ground.byte_100 = 1;
    trick_scan.state().off_ground.byte_101 = 0;
    trick_scan.state().velocity.y = -1;
    trick_scan.state().basis_310c.y = 100;
    trick_scan.state().basis_30f4.y = 0x1000;
    trick_scan.state().off_ground.word_82 = 0;
    trick_scan.state().basis_3100.y = 0;
    const auto scan = trick_scan.apply_landing_cleanup();
    assert(!scan.recovery_reset_requested);
    assert(scan.trick_scan_requested);
    assert(!scan.trick_dispatch_requested);
    assert(scan.trick_direction_hint);
    assert(trick_scan.state().off_ground.word_2c70 == 1);
}

void test_ground_surface_acceleration_contract() {
    GroundSurfaceAccelerationInput input;
    input.velocity = FixedVec3{0x2000, 0x1000, 0};
    input.basis_30f4 = FixedVec3{0x1000, 0, 0};
    input.basis_3100 = FixedVec3{0, 0x1000, 0};
    input.frame_delta_fixed = 0x100;
    input.surface_acceleration_enabled = true;
    input.contact_identity = 5;

    const auto result = apply_ground_surface_acceleration(input);
    assert((result.velocity == FixedVec3{0x2000, 0, 0}));
    assert((result.acceleration == FixedVec3{-0x100, 0, 0}));
    assert(result.velocity_projected);

    input.speed_gate = true;
    const auto gated = apply_ground_surface_acceleration(input);
    assert((gated.acceleration == FixedVec3{-0xf0, 0, 0}));

    PhysicsStateMachine machine;
    machine.state().velocity = input.velocity;
    machine.state().acceleration = input.acceleration;
    const auto applied = machine.apply_ground_surface_acceleration(input);
    assert(machine.state().velocity == applied.velocity);
    assert(machine.state().acceleration == applied.acceleration);
}

void test_ground_collision_handoff_contract() {
    GroundCollisionInput input;
    input.velocity = FixedVec3{0x4000, 0x1000, 0};
    input.reference_surface_vector = FixedVec3{0, 0x1964, 0};
    input.surface_normal = PackedSurfaceNormal{0x00001000u, 0};
    input.collision_result = true;

    const auto result = apply_ground_collision_handoff(input);
    assert((result.projected_surface_vector == FixedVec3{0, 0x1964, 0}));
    assert((result.acceleration == FixedVec3{0, 0x1964, 0}));
    assert((result.velocity == FixedVec3{0, 0x4100, 0}));
    assert(result.surface_vector_published);
    assert(result.velocity_projection.speed_rescaled);

    input.transient_state = true;
    const auto transient = apply_ground_collision_handoff(input);
    assert(!transient.surface_vector_published);

    PhysicsStateMachine machine;
    const auto applied = machine.apply_ground_collision_handoff(input);
    assert(machine.state().velocity == applied.velocity);
    assert(machine.state().acceleration == applied.acceleration);
    assert(!machine.state().contact_surface_vector_valid);
    assert(machine.state().contact_surface_normal == input.surface_normal);
}

void test_air_landing_predicate_contract() {
    AirCollisionObservation observation;
    observation.result = 0x05f365f8;
    observation.material_flags_contact = 0x80;
    observation.material_flags_secondary = 1;
    observation.material_flags_transient = 1;
    observation.material_type = 14;

    AirLandingPredicateInput input;
    input.raw_state = 1;
    input.current_frame = 10347;
    input.last_surface_frame = 10320;
    assert(standard_air_landing_accepted(observation, input));

    observation.result = 0;
    assert(!standard_air_landing_accepted(observation, input));

    observation.result = 1;
    observation.material_flags_transient = 0;
    input.current_frame = 100;
    input.last_surface_frame = 0;
    input.raw_state = 1;
    assert(!standard_air_landing_accepted(observation, input));

    observation.material_flags = 0x40;
    input.raw_state = 3;
    input.current_frame = 120;
    input.last_surface_frame = 100;
    assert(standard_air_landing_accepted(observation, input));

    observation.material_flags_transient = 1;
    PhysicsStateMachine machine;
    machine.state().raw_state = 1;
    machine.state().frame_counter = 10347;
    machine.state().jump.inactive_counter = 0;
    const AirContactInput contact{false, FixedVec3{7, 8, 9}};
    assert(machine.accept_standard_air_collision(observation, 10320, contact));
    assert(machine.state().position == contact.position);
    assert(machine.state().raw_state == 0);
    assert(machine.state().ollie.landing_contact_identity == 14);

    PhysicsStateMachine rejected;
    rejected.state().raw_state = 1;
    rejected.state().frame_counter = 100;
    assert(!rejected.accept_standard_air_collision(observation, 0, contact));
    assert(rejected.state().raw_state == 1);
}

void test_gravity_and_velocity_damping_contracts() {
    GravityInput gravity;
    assert(compute_gravity_acceleration(gravity) == 13000);
    gravity.raw_state = 2;
    gravity.transient_random = 100;
    assert(compute_gravity_acceleration(gravity) == 10400);
    gravity.modifier_c = true;
    gravity.modifier_e = true;
    gravity.global_half_modifier = true;
    assert(compute_gravity_acceleration(gravity) == 1950);

    PhysicsStateMachine machine;
    assert(machine.initialize_gravity_acceleration(gravity) == 1950);
    assert(machine.state().gravity_acceleration == 1950);

    VelocityDampingRandom no_drag;
    no_drag.cap = 1000;
    no_drag.threshold = 1000;
    const auto low = apply_velocity_damping(
        FixedVec3{0x1000, -0x1000, 0}, 0x100, no_drag);
    assert(low.initial_speed_metric == 0x1680);
    assert(!low.cap_applied);
    assert(!low.drag_applied);
    assert(low.low_speed_damped);
    assert((low.velocity == FixedVec3{0xba0, -0xba0, 0}));

    VelocityDampingRandom cap;
    cap.cap = -500;
    cap.cap_x = -500;
    cap.cap_y = -500;
    cap.cap_z = -500;
    cap.threshold = 1000;
    const auto capped = apply_velocity_damping(
        FixedVec3{0x100000, 0, 0}, 0x100, cap, false);
    assert(capped.cap_applied);
    assert(capped.cap_component_targets == FixedVec3{});
    assert(capped.velocity == FixedVec3{});
    assert(!capped.low_speed_damped);

    // The three cap-rescale draws are independent in FUN_0049d480. A native
    // recreation must not reuse the initial cap target for every component.
    VelocityDampingRandom independent_caps;
    independent_caps.cap = -500;
    independent_caps.cap_x = -500;
    independent_caps.cap_y = 0;
    independent_caps.cap_z = 1000;
    independent_caps.threshold = 1000;
    const auto independent = apply_velocity_damping(
        FixedVec3{0x100000, 0x100000, 0x100000}, 0x100,
        independent_caps, false);
    assert(independent.cap_applied);
    assert(independent.cap_component_targets.x == 0);
    assert(independent.cap_component_targets.y != 0);
    assert(independent.cap_component_targets.z !=
           independent.cap_component_targets.y);
    assert(independent.velocity.x == 0);
    assert(independent.velocity.y != independent.velocity.x);
    assert(independent.velocity.z != independent.velocity.y);

    machine.state().velocity = FixedVec3{0x1000, -0x1000, 0};
    const auto applied = machine.apply_postphysics_velocity_damping(
        0x100, no_drag);
    assert(machine.state().velocity == applied.velocity);
}

void on_launch(PhysicsState& state, std::int32_t launch_state,
               std::uint32_t, void* opaque) {
    auto& log = *static_cast<CallbackLog*>(opaque);
    ++log.launch_calls;
    assert(state.raw_state == 0);
    // The retail prephysics routine owns the actual impulse. This sentinel
    // only proves the callback runs before the state request.
    state.velocity.y = launch_state == 1 ? 0x100 : 0x80;
}

void on_dispatch(PhysicsState& state, const DispatchResult& result, void* opaque) {
    auto& log = *static_cast<CallbackLog*>(opaque);
    ++log.dispatch_calls;
    log.last_dispatch = result;
    assert(result.raw_state == state.raw_state);
}

void on_position(PhysicsState& state, const FixedVec3&, const FixedVec3&, void* opaque) {
    auto& log = *static_cast<CallbackLog*>(opaque);
    ++log.position_calls;
    log.callback_state_at_position = state.raw_state;
}

void frame_prephysics(PhysicsStateMachine& machine, void* opaque) {
    auto& log = *static_cast<CallbackLog*>(opaque);
    log.frame_stages.push_back(1);
    assert(machine.state().frame_counter == 0);
    assert(machine.state().kick.press_edge);

    OllieImpulseInput impulse;
    impulse.charge = 7;
    machine.apply_ollie_impulse(impulse);
    const auto request = machine.begin_ollie(LaunchPath::Ordinary);
    assert(request.from == 0 && request.to == 1);
}

void frame_movement_action_step(PhysicsStateMachine& machine, void* opaque) {
    auto& log = *static_cast<CallbackLog*>(opaque);
    log.frame_stages.push_back(0);
    assert(machine.state().action_mask == retail::kLeftActionBit);
    assert(machine.state().left.held == 1);
    assert(machine.state().left.press_edge);
    const auto step = machine.apply_ground_action_step(0x200);
    assert(step.grounded_path);
    assert(step.target_step == 0x7800);
    assert(machine.state().movement_target_x == -0x7800);
    assert(machine.state().movement_target_z == -0x7800);
    assert(machine.state().steering_active == 1);
}

void frame_gravity_initialization(PhysicsStateMachine& machine, void* opaque) {
    auto& log = *static_cast<CallbackLog*>(opaque);
    log.frame_stages.push_back(-1);
    assert(machine.state().frame_counter == 0);
    assert(machine.initialize_gravity_acceleration(GravityInput{}) == 13000);
}

void frame_gravity_observer(PhysicsStateMachine& machine, void* opaque) {
    auto& log = *static_cast<CallbackLog*>(opaque);
    log.frame_stages.push_back(-2);
    assert(machine.state().gravity_acceleration == 13000);
}

void frame_ground_preparation(PhysicsStateMachine& machine, void* opaque) {
    auto& log = *static_cast<CallbackLog*>(opaque);
    log.frame_stages.push_back(2);
    assert(machine.state().raw_state == 1);
}

void frame_action_history(PhysicsStateMachine& machine, void* opaque) {
    auto& log = *static_cast<CallbackLog*>(opaque);
    log.frame_stages.push_back(17);
    assert(machine.state().frame_counter == 0);
    assert(machine.update_action_history(0, 99) == 1);
}

void frame_ground_after_action_history(PhysicsStateMachine&, void* opaque) {
    auto& log = *static_cast<CallbackLog*>(opaque);
    log.frame_stages.push_back(18);
}

void frame_collision_preparation(PhysicsStateMachine& machine, void* opaque) {
    auto& log = *static_cast<CallbackLog*>(opaque);
    log.frame_stages.push_back(3);
    assert(machine.state().phase_state == 1);
    assert((machine.state().position_history == FixedVec3{10, 20, 30}));
}

void frame_dispatch(PhysicsState&, const DispatchResult& result, void* opaque) {
    auto& log = *static_cast<CallbackLog*>(opaque);
    log.frame_stages.push_back(4);
    assert(result.kind == DispatchKind::InAir);
}

void frame_air_motion(PhysicsStateMachine& machine, void* opaque) {
    auto& log = *static_cast<CallbackLog*>(opaque);
    log.frame_stages.push_back(5);
    assert(machine.state().raw_state == 1);
    machine.integrate_in_air_position(0x100);
}

void frame_air_preparation(PhysicsStateMachine& machine, void* opaque) {
    auto& log = *static_cast<CallbackLog*>(opaque);
    log.frame_stages.push_back(11);
    assert(machine.state().raw_state == 1);
    machine.state().basis_30f4 = FixedVec3{0x1000, 0, 0};
    machine.state().basis_310c = FixedVec3{0, 1000, 0};
    assert(machine.prepare_in_air_orientation().handled);
}

void frame_state6_air_motion(PhysicsStateMachine& machine, void* opaque) {
    auto& log = *static_cast<CallbackLog*>(opaque);
    log.frame_stages.push_back(9);
    assert(machine.state().raw_state == 6);
}

void frame_state6_preair_setup(PhysicsStateMachine& machine, void*) {
    assert(machine.state().raw_state == 6);
    const auto result = machine.run_state6_preair_setup(
        State6PreAirInput{100, 100, 0, 0, 0});
    assert(result.handled);
    assert(result.request.from == 6);
    assert(result.request.to == 1);
}

void frame_state6_air_motion_after_setup(PhysicsStateMachine& machine,
                                         void* opaque) {
    auto& log = *static_cast<CallbackLog*>(opaque);
    log.frame_stages.push_back(10);
    assert(machine.state().raw_state == 1);
}

void frame_state3_air_motion(PhysicsStateMachine& machine, void* opaque) {
    auto& log = *static_cast<CallbackLog*>(opaque);
    log.frame_stages.push_back(15);
    // The state-3 timeout is a dispatcher post-handler check. The common
    // in-air handler must still observe raw state 3 first.
    assert(machine.state().raw_state == 3);
}

void frame_set_acceleration(PhysicsStateMachine& machine, void*) {
    machine.state().acceleration = FixedVec3{7, -8, 9};
}

void frame_blocked_physics_reset(PhysicsStateMachine& machine, void* opaque) {
    auto& log = *static_cast<CallbackLog*>(opaque);
    log.frame_stages.push_back(13);
    assert(machine.state().raw_state == 1);
    const auto result = machine.apply_blocked_physics_reset(
        BlockedPhysicsInput{0x100, 10, true});
    assert(result.handled);
    assert(machine.state().acceleration.x == 0);
    assert(machine.state().acceleration.z == 0);
}

void frame_blocked_velocity_observer(PhysicsStateMachine& machine,
                                     void* opaque) {
    auto& log = *static_cast<CallbackLog*>(opaque);
    log.frame_stages.push_back(14);
    assert((machine.state().velocity == FixedVec3{900, 0, 2700}));
    assert((machine.state().acceleration == FixedVec3{0, 22, 0}));
}

void frame_landing_cleanup(PhysicsStateMachine& machine, void* opaque) {
    auto& log = *static_cast<CallbackLog*>(opaque);
    log.frame_stages.push_back(20);
    assert(machine.state().raw_state == retail::kPhysicsOnGround);
    const auto result = machine.apply_landing_cleanup();
    assert(result.handled);
}

void frame_landing_collision_preparation(PhysicsStateMachine& machine,
                                         void* opaque) {
    auto& log = *static_cast<CallbackLog*>(opaque);
    log.frame_stages.push_back(19);
    assert(machine.state().raw_state == retail::kPhysicsOnGround);
}

bool frame_air_contact(PhysicsStateMachine& machine, FixedVec3& contact, void* opaque) {
    auto& log = *static_cast<CallbackLog*>(opaque);
    log.frame_stages.push_back(6);
    assert(machine.state().raw_state == 1);
    contact = FixedVec3{40, 50, 60};
    return true;
}

void frame_position_commit(PhysicsState&, const FixedVec3&, const FixedVec3&, void* opaque) {
    auto& log = *static_cast<CallbackLog*>(opaque);
    log.frame_stages.push_back(7);
}

void frame_postphysics(PhysicsStateMachine& machine, void* opaque) {
    auto& log = *static_cast<CallbackLog*>(opaque);
    log.frame_stages.push_back(8);
    assert(machine.state().raw_state == 0);
    assert(machine.state().phase_state == 1);
}

void frame_velocity_integration(PhysicsStateMachine& machine, void* opaque) {
    auto& log = *static_cast<CallbackLog*>(opaque);
    log.frame_stages.push_back(12);
    assert(machine.state().raw_state == 0);
    machine.integrate_velocity(0x100);
}

void test_velocity_integration_contract() {
    const auto delta = compute_velocity_delta(FixedVec3{0x100, -0x200, 3}, 0x100);
    assert((delta == FixedVec3{0x100, -0x200, 3}));

    PhysicsStateMachine machine;
    machine.state().velocity = FixedVec3{0x1000, -0x2000, 3};
    machine.state().acceleration = FixedVec3{0x100, -0x200, 3};
    assert((machine.integrate_velocity(0x100) == delta));
    assert((machine.state().velocity == FixedVec3{0x1100, -0x2200, 6}));
}

void test_jump_action_record() {
    JumpActionState action;
    action.update(false);
    assert(action.held == 0);
    assert(action.held_counter == 0);
    assert(action.inactive_counter == 1);

    action.update(true);
    assert(action.held == 1);
    assert(action.press_edge);
    assert(action.edge_latch == 1);
    assert(action.held_counter == 1);
    assert(action.inactive_counter == 0);

    action.update(true);
    assert(!action.press_edge);
    assert(action.edge_latch == 1);
    assert(action.held_counter == 2);

    assert(action.consume_press_edge());
    assert(!action.consume_press_edge());

    action.update(false);
    assert(action.held == 0);
    assert(action.held_counter == 0);
    assert(action.inactive_counter == 1);
}

void test_in_air_jump_hold_effect() {
    PhysicsStateMachine machine;

    machine.state().velocity.y = 0x1234;
    machine.state().acceleration.y = -0x2345;
    machine.update_action_mask(retail::kJumpActionBit);
    assert(!machine.apply_in_air_jump_hold_effect());
    assert(machine.state().velocity.y == 0x1234);
    assert(machine.state().acceleration.y == -0x2345);

    machine.update_action_mask(retail::kJumpActionBit);
    machine.update_action_mask(retail::kJumpActionBit);
    assert(machine.state().jump.held_counter == 3);
    assert(machine.apply_in_air_jump_hold_effect());
    assert(machine.state().velocity.y == 0);
    assert(machine.state().acceleration.y == 0);

    machine.state().velocity.y = 0x3456;
    machine.state().acceleration.y = -0x4567;
    machine.update_action_mask(0);
    assert(!machine.apply_in_air_jump_hold_effect());
    assert(machine.state().velocity.y == 0x3456);
    assert(machine.state().acceleration.y == -0x4567);
}

void test_action_record_bank() {
    PhysicsStateMachine machine;
    const std::uint32_t directional_mask =
        retail::kLeftActionBit | retail::kUpActionBit | retail::kDownActionBit;
    machine.update_action_mask(directional_mask);

    assert(machine.state().left.held == 1);
    assert(machine.state().left.press_edge);
    assert(machine.state().left.edge_latch == 1);
    assert(machine.state().left.held_counter == 1);
    assert(machine.state().up.held == 1);
    assert(machine.state().down.held == 1);
    assert(machine.state().right.held == 0);
    assert(machine.state().right.inactive_counter == 1);

    machine.update_action_mask(directional_mask);
    assert(!machine.state().left.press_edge);
    assert(machine.state().left.edge_latch == 1);
    assert(machine.state().left.held_counter == 2);
    assert(machine.state().up.held_counter == 2);

    assert(machine.state().left.consume_press_edge());
    assert(!machine.state().left.consume_press_edge());

    machine.update_action_mask(retail::kRightActionBit | retail::kKickActionBit);
    assert(machine.state().left.held == 0);
    assert(machine.state().left.inactive_counter == 1);
    assert(machine.state().right.press_edge);
    assert(machine.state().right.held_counter == 1);
    assert(machine.state().kick.press_edge);
    assert(machine.state().up.inactive_counter == 1);
    assert(machine.state().down.inactive_counter == 1);
}

void test_action_history_ring() {
    PhysicsStateMachine machine;
    machine.update_action_mask(retail::kKickActionBit);

    // FUN_00492190 calls indices 1..8 first, then KICK at index 0xb. Only
    // changed values reach the FUN_00491c90 ring writer.
    assert(machine.update_action_history(3, 50) == 2);
    assert(machine.state().action_history_write_index == 2);
    assert((machine.state().action_history_events[0] ==
            ActionHistoryEvent{3, 1, 50}));
    assert((machine.state().action_history_events[1] ==
            ActionHistoryEvent{11, 1, 50}));
    assert(machine.state().action_history_previous[12] == 0);

    // No edge means no new ring entry, even though the producer is called
    // again every ground-preparation pass.
    assert(machine.update_action_history(3, 51) == 0);
    assert(machine.state().action_history_write_index == 2);

    machine.update_action_mask(retail::kJumpActionBit);
    assert(machine.update_action_history(0, 52) == 3);
    assert((machine.state().action_history_events[2] ==
            ActionHistoryEvent{3, 0, 52}));
    assert((machine.state().action_history_events[3] ==
            ActionHistoryEvent{11, 0, 52}));
    assert((machine.state().action_history_events[4] ==
            ActionHistoryEvent{12, 1, 52}));
    assert(machine.state().action_history_write_index == 5);

    // The writer wraps exactly at 0x20 entries. Starting at slot five, 27
    // transient physics-action changes land on slot 31 and wrap the index to
    // zero; the next release overwrites slot zero.
    for (int step = 0; step != 13; ++step) {
        assert(machine.update_action_history(1, 60 + step * 2) == 1);
        assert(machine.update_action_history(0, 61 + step * 2) == 1);
    }
    assert(machine.update_action_history(1, 86) == 1);
    assert(machine.state().action_history_write_index == 0);
    assert((machine.state().action_history_events[31] ==
            ActionHistoryEvent{1, 1, 86}));
    assert(machine.update_action_history(0, 87) == 1);
    assert((machine.state().action_history_events[0] ==
            ActionHistoryEvent{1, 0, 87}));
    assert(machine.state().action_history_write_index == 1);
}

void test_clean_ground_air_ground_trace() {
    PhysicsStateMachine machine;
    CallbackLog log;

    machine.update_action_mask(0);
    machine.begin_dispatcher_phase();
    const auto ground = machine.dispatch(on_dispatch, &log);
    assert(ground.kind == DispatchKind::Ground);
    assert(ground.handler_count == 4);

    machine.update_action_mask(retail::kKickActionBit);
    assert(machine.state().kick.press_edge);
    assert(machine.state().kick.consume_press_edge());

    const auto launch = machine.begin_ollie(LaunchPath::Ordinary, on_launch, &log);
    assert(launch.changed);
    assert(launch.from == 0 && launch.to == 1);
    assert(launch.reason == retail::kOrdinaryLaunchReason);
    assert(launch.request_callsite == retail::kOrdinaryLaunchRequestCallsite);
    assert(machine.state().phase_state == 0);
    assert(machine.state().velocity.y == 0x100);

    machine.begin_dispatcher_phase();
    const auto air = machine.dispatch(on_dispatch, &log);
    assert(air.kind == DispatchKind::InAir);
    assert(air.handler_count == 1);
    assert(air.handler_pcs[0] == retail::kInAirHandler);
    assert(machine.state().phase_state == 1);

    machine.update_action_mask(0);
    assert(machine.state().jump.held == 0);
    const FixedVec3 contact{0x100, 0x200, 0x300};
    assert(machine.accept_air_contact(true, contact, on_position, &log));
    assert(log.callback_state_at_position == 1);
    assert(machine.state().position == contact);
    assert(machine.state().old_position == FixedVec3{});
    assert(machine.state().raw_state == 0);
    assert(machine.state().phase_state == 1);
    assert(machine.requests().size() == 2);
    assert(machine.requests().back().reason == retail::kLandingReason);
    assert(machine.requests().back().request_callsite == retail::kLandingRequestCallsite);
    assert(log.launch_calls == 1);
    assert(log.position_calls == 1);
    assert(machine.state().ollie.landing_effect_state == 0);
    assert(machine.state().off_ground.word_2e90 == 0);
    assert(machine.state().off_ground.word_2e94 == 0);
}

void test_transient_routes_remain_distinct() {
    PhysicsStateMachine machine;

    const auto enter = machine.enter_collision_transient();
    assert(enter.from == 0 && enter.to == 2);
    assert(enter.reason == retail::kCollisionTransientEnterReason);
    assert(enter.request_callsite == retail::kCollisionTransientEnterCallsite);
    const auto state2 = machine.dispatch();
    assert(state2.kind == DispatchKind::CollisionTransient);
    assert(state2.handler_pcs[0] == 0x00496550);
    assert(machine.state().auxiliary == 1);

    const auto exit = machine.exit_collision_transient();
    assert(exit.from == 2 && exit.to == 0);
    assert(exit.reason == retail::kCollisionTransientExitReason);
    assert(exit.request_callsite == retail::kCollisionTransientExitCallsite);

    machine.request_state(4, 0);
    const auto state4 = machine.dispatch();
    assert(state4.kind == DispatchKind::State4);
    assert(state4.handler_pcs[0] == 0x00494210);
    assert(machine.state().auxiliary == 0);

    machine.request_state(3, retail::kAlternateLaunchReason);
    const auto state3 = machine.dispatch();
    assert(state3.kind == DispatchKind::InAir);
    assert(state3.handler_pcs[0] == retail::kInAirHandler);
    assert(state3.raw_state != 1);

    machine.request_state(6, 0);
    const auto state6 = machine.dispatch();
    assert(state6.kind == DispatchKind::State6PreAir);
    assert(state6.handler_count == 2);
    assert(state6.handler_pcs[0] == 0x004993f0);
    assert(state6.handler_pcs[1] == retail::kInAirHandler);

    PhysicsStateMachine state6_contact;
    state6_contact.state().raw_state = 6;
    state6_contact.state().position = FixedVec3{10, 20, 30};
    assert(state6_contact.accept_air_contact(true, FixedVec3{11, 21, 31}));
    assert(state6_contact.state().raw_state == 0);
    assert(state6_contact.requests().back().reason == retail::kLandingReason);

    machine.state().position = FixedVec3{90, 91, 92};
    machine.state().position_history = FixedVec3{12, 13, 14};
    machine.request_state(7, 0);
    const auto state7 = machine.dispatch();
    assert(state7.kind == DispatchKind::State7Ground);
    assert(state7.handler_count == 4);
    assert(state7.handler_pcs[0] == 0x0049dad0);
    assert(state7.handler_pcs[1] == 0x00496550);
    assert(state7.handler_pcs[2] == 0x00495cc0);
    assert(state7.handler_pcs[3] == 0x0049d9c0);
    assert(machine.state().position == machine.state().position_history);

    PhysicsStateMachine state6_frame;
    state6_frame.state().raw_state = 6;
    CallbackLog frame_log;
    PhysicsFrameCallbacks callbacks;
    callbacks.air_motion = frame_state6_air_motion;
    const auto frame = state6_frame.step_frame(0, callbacks, &frame_log);
    assert(frame.kind == DispatchKind::State6PreAir);
    assert((frame_log.frame_stages == std::vector<int>{9}));

    PhysicsStateMachine state6_integrated;
    state6_integrated.state().raw_state = 6;
    state6_integrated.state().basis_30f4 = FixedVec3{0x1000, 0, 0};
    state6_integrated.state().velocity = FixedVec3{0x1000, 0, 0};
    CallbackLog integrated_log;
    PhysicsFrameCallbacks integrated_callbacks;
    integrated_callbacks.state6_preair_setup = frame_state6_preair_setup;
    integrated_callbacks.air_preparation = frame_air_preparation;
    integrated_callbacks.air_motion = frame_state6_air_motion_after_setup;
    const auto integrated = state6_integrated.step_frame(
        0, integrated_callbacks, &integrated_log);
    assert(integrated.kind == DispatchKind::State6PreAir);
    assert(state6_integrated.state().raw_state == 1);
    assert(state6_integrated.state().ollie.charge == 5);
    assert(state6_integrated.requests().back().request_callsite ==
           retail::kState6PreAirRequestCallsite);
    assert((integrated_log.frame_stages == std::vector<int>{11, 10}));

    PhysicsStateMachine state6_setup;
    state6_setup.state().raw_state = 6;
    state6_setup.state().basis_30f4 = FixedVec3{0x1000, 0, 0};
    state6_setup.state().velocity = FixedVec3{0x1000, 0, 0};
    const auto setup = state6_setup.run_state6_preair_setup(
        State6PreAirInput{100, 100, 0, 0, 0});
    assert(setup.handled);
    assert(setup.charge == 5);
    assert(setup.request.from == 6);
    assert(setup.request.to == 1);
    assert(setup.request.reason == retail::kState6PreAirReason);
    assert(setup.request.request_callsite ==
           retail::kState6PreAirRequestCallsite);
    assert(state6_setup.state().ollie.in_progress == 1);
    assert(setup.velocity_shaped);
    assert(setup.velocity == state6_setup.state().velocity);
    assert((setup.velocity == FixedVec3{0x4000, -137625, 0}));
}

void test_ollie_impulse_formula() {
    OllieImpulseInput low_slope;
    low_slope.charge = 0;
    const auto low = compute_ollie_vertical_impulse(low_slope);
    assert(!low.high_slope_branch);
    assert(low.delta_y == -82944);

    OllieImpulseInput high_slope;
    high_slope.slope_metric = 0x9c4;
    const auto high = compute_ollie_vertical_impulse(high_slope);
    assert(high.high_slope_branch);
    assert(high.delta_y == -82944);

    OllieImpulseInput wallie;
    wallie.wallie = true;
    const auto wallie_result = compute_ollie_vertical_impulse(wallie);
    assert(wallie_result.delta_y == -82944 - 0xf000);
}

void test_ollie_charge_and_air_integration() {
    PhysicsStateMachine machine;
    const auto jump_only = machine.advance_ollie_charge(true, 0, 0);
    assert(jump_only.charge == 0);
    assert(!jump_only.latched);
    assert(!jump_only.pending);

    // The recovered prephysics gate reads the KICK subrecord (+0x30), not
    // the configured JUMP record (+0x00).
    machine.update_action_mask(retail::kKickActionBit);
    const auto charged = machine.advance_ollie_charge(true, 0, 0);
    assert(charged.charge == 1);
    assert(charged.cap == 0xf);
    assert(charged.latched);
    assert(charged.pending);
    assert(machine.state().ollie.latched == 1);
    assert(machine.state().ollie.pending == 1);

    machine.state().ollie.charge = 0xf;
    const auto capped = machine.advance_ollie_charge(true, 0, 0);
    assert(capped.capped);
    assert(capped.charge == 0xf);

    const auto not_charged = machine.advance_ollie_charge(false, 0, 0);
    assert(not_charged.charge == 0xf);

    const FixedVec3 delta = compute_in_air_position_delta(
        FixedVec3{0x1000, -0x1000, 0}, FixedVec3{0x200, -0x200, 0}, 0x100);
    assert((delta == FixedVec3{0x1100, -0x1100, 0}));

    machine.state().velocity = FixedVec3{0x1000, -0x1000, 0};
    machine.state().acceleration = FixedVec3{0x200, -0x200, 0};
    assert(machine.integrate_in_air_position(0x100) == delta);
    assert(machine.state().position == delta);

    machine.state().gravity_acceleration = 13000;
    assert((machine.apply_in_air_gravity() == FixedVec3{0x200, 12488, 0}));
    assert((machine.state().acceleration == FixedVec3{0x200, 12488, 0}));
}

void test_in_air_action_control_contract() {
    InAirActionControlInput input;
    input.velocity = FixedVec3{0, 0, 0};
    input.acceleration = FixedVec3{1, 2, 3};
    input.basis_30f4 = FixedVec3{0x1000, 0x2000, -0x1000};
    input.basis_3100 = FixedVec3{0x1000, 0, 0};
    input.basis_310c = FixedVec3{0x1000, -0x1000, 0x2000};
    input.gravity_acceleration = 100;
    input.control_enabled = true;
    input.kick_held = true;
    input.up_held = true;
    input.down_held = true;
    input.spin_left_held = true;
    input.spin_right_held = true;

    const auto result = apply_in_air_action_control(input);
    // KICK: +{180,-180,360}; UP: -{150,300,-150}; DOWN:
    // +{150,300,-150}; the opposing spin terms cancel.
    assert(result.applied);
    assert((result.acceleration == FixedVec3{181, -178, 363}));

    input.control_enabled = false;
    const auto disabled = apply_in_air_action_control(input);
    assert(!disabled.applied);
    assert(disabled.acceleration == input.acceleration);

    PhysicsStateMachine machine;
    machine.state().air_control_enabled = true;
    machine.state().gravity_acceleration = 100;
    machine.state().basis_310c = input.basis_310c;
    machine.state().acceleration = FixedVec3{};
    machine.update_action_mask(retail::kKickActionBit);
    const auto applied = machine.apply_in_air_action_control();
    assert(applied.applied);
    assert((machine.state().acceleration == FixedVec3{180, -180, 360}));

    input.velocity = FixedVec3{0x2000, 0, 0};
    input.acceleration = FixedVec3{};
    input.control_enabled = true;
    input.basis_30f4 = FixedVec3{};
    input.basis_310c = FixedVec3{};
    input.basis_3100 = FixedVec3{0x1000, 0, 0};
    input.kick_held = false;
    input.up_held = false;
    input.down_held = false;
    input.spin_left_held = false;
    input.spin_right_held = false;
    const auto stabilized = apply_in_air_action_control(input);
    assert((stabilized.acceleration == FixedVec3{-0x100, 0, 0}));
}

void test_ground_action_target_step() {
    PhysicsStateMachine ground;
    ground.state().raw_state = 0;
    ground.state().ground_speed_profile = 0;
    ground.update_action_mask(retail::kLeftActionBit);

    const auto first = ground.apply_ground_action_step(0x200);
    assert(first.grounded_path);
    assert(first.target_step == 0x7800);
    assert(first.target_limit == 0x2d000);
    assert(!first.brake_mode);
    assert(first.steering_active);
    assert(ground.state().movement_target_x == -0x7800);
    assert(ground.state().movement_target_z == -0x7800);

    for (int step = 0; step < 5; ++step) {
        ground.apply_ground_action_step(0x200);
    }
    assert(ground.state().movement_target_x == -0x2d000);
    assert(ground.state().movement_target_z == -0x2d000);

    ground.update_action_mask(0);
    const auto release = ground.apply_ground_action_step(0x200);
    assert(!release.steering_active);
    assert(ground.state().movement_target_x == -0x21c00);
    assert(ground.state().movement_target_z == -0x21c00);

    PhysicsStateMachine analog;
    analog.state().raw_state = 0;
    analog.state().heading_input = 64;
    const auto analog_step = analog.apply_ground_action_step(0x200);
    assert(analog_step.target_limit == 0x2d000);
    assert(analog_step.steering_active);
    assert(analog.state().movement_target_x == 0xf000);
    assert(analog.state().movement_target_z == 0xf000);

    PhysicsStateMachine braking;
    braking.state().raw_state = 0;
    braking.state().heading_deadband = 31;
    braking.state().velocity.x = 0x10000;
    braking.update_action_mask(retail::kDownActionBit);
    const auto brake = braking.apply_ground_action_step(0x100);
    assert(brake.brake_mode);
    assert(brake.target_step == 0x7800);
    // Retail dot/sqrt scale: 0x10000 velocity gives length 0x400 and the
    // grounded brake divisor is ((0x400 * 0x40) >> 12) == 0x10.
    assert(braking.state().acceleration.x == -0x1000);

    PhysicsStateMachine airborne;
    airborne.state().raw_state = 1;
    airborne.update_action_mask(retail::kLeftActionBit);
    const auto no_ground_step = airborne.apply_ground_action_step(0x200);
    assert(!no_ground_step.grounded_path);
    assert(airborne.state().movement_target_x == 0);
}

void test_kick_release_is_the_launch_boundary() {
    // The first held cap pair and the refresh pair are distinct from the
    // five draws consumed by the launch impulse formula.
    PhysicsStateMachine cap_stream;
    cap_stream.update_action_mask(retail::kKickActionBit);
    OlliePrePhysicsInput cap_input;
    cap_input.charge_cap_random.first = 100;
    cap_input.charge_cap_random.second = 100;
    const auto first_cap = cap_stream.run_ollie_prephysics(cap_input);
    assert(first_cap.charge == 1);
    assert(first_cap.cap == 5);
    assert(!first_cap.capped);
    cap_stream.update_action_mask(retail::kKickActionBit);
    cap_input.force_cap = true;
    cap_input.charge_cap_refresh_random.first = 80;
    cap_input.charge_cap_refresh_random.second = 80;
    const auto refreshed_cap = cap_stream.run_ollie_prephysics(cap_input);
    assert(refreshed_cap.capped);
    assert(refreshed_cap.charge == 7);
    assert(refreshed_cap.cap == 7);

    PhysicsStateMachine machine;
    machine.state().ollie.mode = 0;
    machine.state().movement_target_x = 0x100;
    machine.state().ollie.launch_auxiliary = 11;
    machine.state().ollie.launch_angle_accumulator = 22;
    machine.state().ollie.launch_angle_turns = 33;

    // The configured JUMP bit may be held alongside KICK, but the direct
    // prephysics charge gate is the KICK record at +0x30.
    machine.update_action_mask(retail::kJumpActionBit | retail::kKickActionBit);
    OlliePrePhysicsInput input;
    input.current_frame = 620;
    input.early_release_random.first = 280;
    const auto charging = machine.run_ollie_prephysics(input);
    assert(charging.event == OlliePrePhysicsEvent::Charging);
    assert(charging.charge == 1);
    assert(machine.state().movement_target_x == 0);
    assert(charging.latch_set);
    assert(charging.pending_set);
    assert(machine.state().raw_state == 0);

    // Release KICK while JUMP remains held. This is the observed launch edge
    // at 0x0049a751; it is not a JUMP press-edge transition.
    machine.update_action_mask(retail::kJumpActionBit);
    input.current_frame = 621;
    const auto launched = machine.run_ollie_prephysics(input);
    assert(launched.event == OlliePrePhysicsEvent::Launched);
    assert(launched.launch_consumed);
    assert(launched.state_requested);
    assert(launched.request.from == 0);
    assert(launched.request.to == 1);
    assert(launched.request.reason == retail::kOrdinaryLaunchReason);
    assert(launched.request.request_callsite ==
           retail::kOrdinaryLaunchRequestCallsite);
    assert(machine.state().raw_state == 1);
    assert(machine.state().ollie.charge == 0);
    assert(machine.state().ollie.launch_charge == 1);
    assert(machine.state().ollie.latched == 0);
    assert(machine.state().ollie.pending == 0);
    assert(machine.state().ollie.in_progress == 1);
    assert(machine.state().ollie.early_release_count == 0);
    assert(machine.state().ollie.launch_auxiliary == 0);
    assert(machine.state().ollie.launch_angle_accumulator == 0);
    assert(machine.state().ollie.launch_angle_turns == 0);
    assert(machine.state().ollie.launch_frame == 621);

    const auto air = machine.dispatch();
    assert(air.kind == DispatchKind::InAir);
    assert(air.handler_pcs[0] == retail::kInAirHandler);

    assert(machine.accept_air_contact(true, FixedVec3{1, 2, 3}));
    assert(machine.state().raw_state == 0);
    assert(machine.requests().back().reason == retail::kLandingReason);
    // The contact branch writes +0x2ec0=1, then immediately calls
    // FUN_004914d0, which consumes that one-frame marker.
    assert(machine.state().ollie.landing_effect_state == 0);
    assert(machine.state().ollie.landing_frame ==
           static_cast<std::int32_t>(machine.state().frame_counter));

    // The first grounded prephysics pass consumes the old phase word left by
    // the 1 -> 0 request and clears the launch/re-entry bookkeeping.
    machine.state().ollie.action_context = 9;
    machine.state().ollie.recovery_latch = 7;
    machine.state().phase_state = 1;
    OlliePrePhysicsInput recovery_input;
    recovery_input.current_frame = 682;
    const auto recovery = machine.run_ollie_prephysics(recovery_input);
    assert(recovery.recovery_cleared);
    assert(machine.state().ollie.in_progress == 0);
    assert(machine.state().ollie.mode_latched == 0);
    assert(machine.state().ollie.action_context == 0);
    assert(machine.state().ollie.recovery_latch == 0);

    PhysicsStateMachine projected_landing;
    projected_landing.state().raw_state = 1;
    projected_landing.state().velocity = FixedVec3{0x4000, 0x1000, 0};
    AirContactInput landing;
    landing.accepted = true;
    landing.position = FixedVec3{7, 8, 9};
    landing.surface_normal = PackedSurfaceNormal{0x00001000u, 0};
    landing.surface_normal_valid = true;
    landing.contact_identity = 6;
    assert(projected_landing.accept_air_contact(landing));
    assert((projected_landing.state().velocity == FixedVec3{0, 0x4100, 0}));
    assert(projected_landing.state().ollie.landing_contact_identity == 6);
    assert(projected_landing.state().contact_surface_normal == landing.surface_normal);

    // Retail's alternate branch is the complementary zero-state case: a
    // nonzero latched mode requests raw state 3 and records the alternate
    // launch reason.
    PhysicsStateMachine alternate;
    alternate.state().ollie.mode = 1;
    alternate.update_action_mask(retail::kKickActionBit);
    OlliePrePhysicsInput alternate_input;
    alternate_input.current_frame = 700;
    assert(alternate.run_ollie_prephysics(alternate_input).latch_set);
    alternate.update_action_mask(0);
    alternate_input.current_frame = 701;
    const auto alternate_launch = alternate.run_ollie_prephysics(alternate_input);
    assert(alternate_launch.state_requested);
    assert(alternate_launch.request.to == 3);
    assert(alternate_launch.request.reason == retail::kAlternateLaunchReason);
    assert(alternate_launch.request.request_callsite ==
           retail::kAlternateLaunchRequestCallsite);
    assert(alternate.state().ollie.alternate_state_frame == 701);
    alternate.state().frame_counter = 727;
    const auto alternate_dispatch = alternate.dispatch();
    assert(alternate_dispatch.raw_state == 3);
    assert(alternate_dispatch.kind == DispatchKind::InAir);
    assert(alternate.state().raw_state == 1);
    assert(alternate.requests().back().reason == retail::kAlternateStateTimeoutReason);
    assert(alternate.requests().back().request_callsite ==
           retail::kAlternateStateTimeoutRequestCallsite);

    // A raw-3 contact inside the launch grace commits position but remains in
    // the alternate air state; the landing request is deferred.
    PhysicsStateMachine grace;
    grace.state().raw_state = 3;
    grace.state().ollie.launch_frame = 100;
    grace.state().frame_counter = 120;
    assert(grace.accept_air_contact(true, FixedVec3{4, 5, 6}));
    assert((grace.state().position == FixedVec3{4, 5, 6}));
    assert(grace.state().raw_state == 3);
    assert(grace.requests().empty());

    // A JUMP-only release path has no KICK latch and therefore cannot launch.
    PhysicsStateMachine jump_only;
    jump_only.update_action_mask(retail::kJumpActionBit);
    OlliePrePhysicsInput jump_input;
    jump_input.current_frame = 1;
    const auto no_launch = jump_only.run_ollie_prephysics(jump_input);
    assert(no_launch.event == OlliePrePhysicsEvent::None);
    assert(!no_launch.state_requested);
    assert(jump_only.state().raw_state == 0);

    // The static release path rejects a latch older than twenty frames.
    PhysicsStateMachine stale;
    stale.state().ollie.mode = 1;
    stale.update_action_mask(retail::kKickActionBit);
    OlliePrePhysicsInput stale_input;
    stale_input.current_frame = 10;
    assert(stale.run_ollie_prephysics(stale_input).latch_set);
    stale.state().ollie.latch_timestamp = 10;
    stale.update_action_mask(0);
    stale_input.current_frame = 31;
    const auto stale_result = stale.run_ollie_prephysics(stale_input);
    assert(stale_result.event == OlliePrePhysicsEvent::StaleLatchCleared);
    assert(stale_result.stale_latch_cleared);
    assert(!stale_result.state_requested);
    assert(stale.state().raw_state == 0);

    // Raw state 5 uses the dedicated wallie cap pair before applying the
    // wallie impulse bias; it must not reuse the ordinary held charge.
    PhysicsStateMachine wallie;
    wallie.state().raw_state = 5;
    wallie.update_action_mask(retail::kKickActionBit);
    OlliePrePhysicsInput wallie_input;
    wallie_input.current_frame = 40;
    assert(wallie.run_ollie_prephysics(wallie_input).latch_set);
    wallie.update_action_mask(0);
    wallie_input.current_frame = 41;
    wallie_input.wallie_charge_random.first = 100;
    wallie_input.wallie_charge_random.second = 100;
    const auto wallie_launch = wallie.run_ollie_prephysics(wallie_input);
    assert(wallie_launch.launch_consumed);
    assert(wallie.state().ollie.wallie == 1);
    assert(wallie.state().ollie.launch_charge == 5);
}

void test_outer_frame_order_and_history() {
    PhysicsStateMachine machine;
    CallbackLog log;
    machine.state().position = FixedVec3{10, 20, 30};
    machine.state().gravity_acceleration = 13000;

    PhysicsFrameCallbacks callbacks;
    callbacks.prephysics = frame_prephysics;
    callbacks.ground_preparation = frame_ground_preparation;
    callbacks.collision_preparation = frame_collision_preparation;
    callbacks.dispatcher = frame_dispatch;
    callbacks.air_preparation = frame_air_preparation;
    callbacks.air_motion = frame_air_motion;
    callbacks.air_contact = frame_air_contact;
    callbacks.position_commit = frame_position_commit;
    callbacks.landing_collision_preparation = frame_landing_collision_preparation;
    callbacks.landing_cleanup = frame_landing_cleanup;
    callbacks.velocity_integration = frame_velocity_integration;
    callbacks.postphysics = frame_postphysics;

    const auto result = machine.step_frame(retail::kKickActionBit, callbacks, &log);
    assert(result.raw_state == 1);
    assert((log.frame_stages ==
            std::vector<int>{1, 2, 3, 4, 11, 5, 6, 7, 19, 20, 12, 8}));
    assert(machine.state().frame_counter == 1);
    assert(machine.state().older_position == FixedVec3{});
    assert((machine.state().position_history == FixedVec3{10, 20, 30}));
    assert((machine.state().position == FixedVec3{40, 50, 60}));
    assert((machine.state().old_position == FixedVec3{10, -100555, 30}));
    assert(machine.state().ollie.in_progress == 1);
    assert(machine.state().acceleration == FixedVec3{});

    PhysicsStateMachine handplant;
    handplant.state().raw_state = retail::kPhysicsInHandplant;
    CallbackLog handplant_log;
    PhysicsFrameCallbacks handplant_callbacks;
    handplant_callbacks.landing_collision_preparation =
        frame_landing_collision_preparation;
    handplant_callbacks.landing_cleanup = frame_landing_cleanup;
    handplant.step_frame(0, handplant_callbacks, &handplant_log);
    assert(handplant_log.frame_stages.empty());
}

void test_state3_timeout_follows_air_handler() {
    PhysicsStateMachine machine;
    machine.state().raw_state = retail::kPhysicsInAirStickTo;
    machine.state().frame_counter = 26;
    machine.state().ollie.alternate_state_frame = 0;

    CallbackLog log;
    PhysicsFrameCallbacks callbacks;
    callbacks.air_motion = frame_state3_air_motion;
    const auto result = machine.step_frame(0, callbacks, &log);

    assert(result.raw_state == retail::kPhysicsInAirStickTo);
    assert(result.kind == DispatchKind::InAir);
    assert((log.frame_stages == std::vector<int>{15}));
    assert(machine.state().raw_state == retail::kPhysicsInAir);
    assert(machine.requests().size() == 1);
    assert(machine.requests().back().from == retail::kPhysicsInAirStickTo);
    assert(machine.requests().back().to == retail::kPhysicsInAir);
    assert(machine.requests().back().reason ==
           retail::kAlternateStateTimeoutReason);
    assert(machine.requests().back().request_callsite ==
           retail::kAlternateStateTimeoutRequestCallsite);
}

void test_action_history_frame_boundary() {
    PhysicsStateMachine machine;
    CallbackLog log;
    PhysicsFrameCallbacks callbacks;
    callbacks.action_history = frame_action_history;
    callbacks.ground_preparation = frame_ground_after_action_history;

    const auto result = machine.step_frame(retail::kKickActionBit, callbacks, &log);
    assert(result.kind == DispatchKind::Ground);
    assert((log.frame_stages == std::vector<int>{17, 18}));
    assert((machine.state().action_history_events[0] ==
            ActionHistoryEvent{11, 1, 99}));
}

void test_non_air_acceleration_boundary() {
    PhysicsStateMachine machine;
    machine.state().acceleration = FixedVec3{1, -2, 3};
    assert(machine.reset_non_air_acceleration());
    assert(machine.state().acceleration == FixedVec3{});

    machine.state().raw_state = 1;
    machine.state().acceleration = FixedVec3{4, -5, 6};
    assert(!machine.reset_non_air_acceleration());
    assert((machine.state().acceleration == FixedVec3{4, -5, 6}));

    PhysicsStateMachine frame_ground;
    PhysicsFrameCallbacks callbacks;
    callbacks.movement_action_step = frame_set_acceleration;
    frame_ground.step_frame(0, callbacks);
    assert(frame_ground.state().acceleration == FixedVec3{});

    PhysicsStateMachine frame_air;
    frame_air.state().raw_state = 1;
    frame_air.step_frame(0, callbacks);
    assert((frame_air.state().acceleration == FixedVec3{7, -8, 9}));
}

void test_blocked_physics_frame_boundary() {
    PhysicsStateMachine machine;
    machine.state().raw_state = 1;
    machine.state().prephysics_blocked = 1;
    machine.state().acceleration = FixedVec3{11, 22, 33};
    machine.state().velocity = FixedVec3{1000, -500, 3000};

    CallbackLog log;
    PhysicsFrameCallbacks callbacks;
    callbacks.blocked_physics_reset = frame_blocked_physics_reset;
    callbacks.velocity_integration = frame_blocked_velocity_observer;
    const auto result = machine.step_frame(0, callbacks, &log);

    assert(result.kind == DispatchKind::InAir);
    assert((log.frame_stages == std::vector<int>{13, 14}));
}

void test_movement_action_step_boundary() {
    PhysicsStateMachine machine;
    CallbackLog log;
    PhysicsFrameCallbacks callbacks;
    callbacks.movement_action_step = frame_movement_action_step;

    const auto result = machine.step_frame(retail::kLeftActionBit, callbacks, &log);
    assert(result.kind == DispatchKind::Ground);
    assert((log.frame_stages == std::vector<int>{0}));
    assert(machine.state().movement_target_x == -0x7800);
    assert(machine.state().movement_target_z == -0x7800);
    assert(machine.state().steering_active == 1);
}

void test_gravity_frame_start_boundary() {
    PhysicsStateMachine machine;
    CallbackLog log;
    PhysicsFrameCallbacks callbacks;
    callbacks.gravity_initialization = frame_gravity_initialization;
    callbacks.movement_action_step = frame_gravity_observer;
    machine.step_frame(0, callbacks, &log);
    assert((log.frame_stages == std::vector<int>{-1, -2}));
    assert(machine.state().gravity_acceleration == 13000);
}

}  // namespace

int main() {
    test_retail_fixed12_math_and_orientation_layout();
    test_in_air_orientation_preparation();
    test_orientation_recovery();
    test_upright_correction();
    test_air_collision_recovery();
    test_blocked_physics_reset();
    test_collision_recovery_window();
    test_cross_build_physics_state_labels();
    test_observed_state4_transition_path();
    test_ground_to_air_leave_ground_transition();
    test_off_ground_reset_boundary();
    test_landing_cleanup_boundary();
    test_ground_surface_acceleration_contract();
    test_ground_collision_handoff_contract();
    test_air_landing_predicate_contract();
    test_gravity_and_velocity_damping_contracts();
    test_velocity_integration_contract();
    test_jump_action_record();
    test_in_air_jump_hold_effect();
    test_action_record_bank();
    test_action_history_ring();
    test_clean_ground_air_ground_trace();
    test_transient_routes_remain_distinct();
    test_ollie_impulse_formula();
    test_ollie_charge_and_air_integration();
    test_in_air_action_control_contract();
    test_ground_action_target_step();
    test_kick_release_is_the_launch_boundary();
    test_outer_frame_order_and_history();
    test_state3_timeout_follows_air_handler();
    test_action_history_frame_boundary();
    test_non_air_acceleration_boundary();
    test_blocked_physics_frame_boundary();
    test_movement_action_step_boundary();
    test_gravity_frame_start_boundary();
    return 0;
}
