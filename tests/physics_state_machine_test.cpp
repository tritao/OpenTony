#include "physics_state_machine.hpp"

#include "tests/test_check.hpp"
#include <cstdint>
#include <limits>
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

void frame_ground_dispatch(PhysicsState&, const DispatchResult&, void*);
bool frame_ground_leave_air_input(PhysicsStateMachine&,
                                  GroundAirTransitionInput&, void*);
bool frame_ground_leave_air_rejected(PhysicsStateMachine&,
                                     GroundAirTransitionInput&, void*);
bool frame_ground_leave_air_missing(PhysicsStateMachine&,
                                    GroundAirTransitionInput&, void*);

void test_retail_fixed12_math_and_orientation_layout() {
    CHECK(fixed12_dot(FixedVec3{2048, 0, 0}, FixedVec3{1, 0, 0}) == 0);
    CHECK(fixed12_dot(FixedVec3{2048, 0, 0}, FixedVec3{3, 0, 0}) == 1);
    CHECK(fixed12_scalar_multiply(3, 2048) == 1);

    const PackedSurfaceNormal packed{0x80001000u, 0x0000c000u};
    CHECK((decode_surface_normal(packed) == FixedVec3{0x1000, -0x8000, -0x4000}));
    const auto projected = project_vector_off_surface(
        FixedVec3{0x2000, 0x1000, 0}, PackedSurfaceNormal{0x00001000u, 0});
    CHECK((projected == FixedVec3{0, 0x1000, 0}));

    const auto preserved = project_velocity_preserving_speed(
        FixedVec3{0x4000, 0x1000, 0}, PackedSurfaceNormal{0x00001000u, 0});
    CHECK(preserved.original_speed_metric == 16832);
    CHECK(preserved.projected_speed_metric == 4096);
    CHECK(preserved.speed_rescaled);
    CHECK((preserved.velocity == FixedVec3{0, 0x4100, 0}));

    const auto basis = copy_orientation_basis(
        std::array<std::int16_t, 9>{1, 2, 3, 4, 5, 6, 7, 8, 9});
    CHECK((basis.basis_30f4 == FixedVec3{3, 6, 9}));
    CHECK((basis.basis_3100 == FixedVec3{1, 4, 7}));
    CHECK((basis.basis_310c == FixedVec3{2, 5, 8}));
}

void test_in_air_orientation_preparation() {
    CHECK((normalize_retail_fixed12(FixedVec3{}) ==
            FixedVec3{0x1000, 0, 0}));
    CHECK((normalize_retail_fixed12(FixedVec3{0x10000, 0, 0}) ==
            FixedVec3{0x1000, 0, 0}));
    CHECK((fixed12_cross(FixedVec3{0x1000, 0, 0},
                         FixedVec3{0, 0x1000, 0}) ==
            FixedVec3{0, 0, 0x1000}));
    CHECK((clamp_cross_scratch_to_int16(
                FixedVec3{0x9000, -0x9000, 0x1234}) ==
            FixedVec3{0x7fff, -0x8000, 0x1234}));

    PhysicsStateMachine machine;
    machine.state().basis_30f4 = FixedVec3{0x1000, 0, 0};
    machine.state().basis_310c = FixedVec3{0, 1000, 0};
    const auto prepared = machine.prepare_in_air_orientation();
    CHECK(prepared.handled);
    CHECK(!prepared.rolling_axis_reset);
    CHECK((prepared.basis.basis_310c == FixedVec3{0, 0x1000, 0}));
    CHECK((prepared.basis.basis_3100 == FixedVec3{0, 0, 0x1000}));
    CHECK((prepared.basis.basis_30f4 == FixedVec3{0x1000, 0, 0}));
    CHECK((machine.state().orientation_shorts ==
            std::array<std::int16_t, 9>{0, 0, 0x1000, 0, 0x1000,
                                        0, 0x1000, 0, 0}));
    const auto republished =
        copy_orientation_basis(machine.state().orientation_shorts);
    CHECK(republished.basis_30f4 == prepared.basis.basis_30f4);
    CHECK(republished.basis_3100 == prepared.basis.basis_3100);
    CHECK(republished.basis_310c == prepared.basis.basis_310c);

    machine.state().basis_310c = FixedVec3{123, -4000, 456};
    const auto reset = machine.prepare_in_air_orientation();
    CHECK(reset.rolling_axis_reset);
    CHECK((reset.basis.basis_310c == FixedVec3{0, -0x1000, 0}));
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
    CHECK(recovered.handled);
    CHECK(!recovered.skipped);
    CHECK((recovered.target == FixedVec3{0, 0x1000, 0}));
    CHECK((recovered.basis.basis_30f4 == FixedVec3{0x1000, 0, 0}));
    CHECK((recovered.basis.basis_3100 == FixedVec3{0, 0, 0x1000}));
    CHECK((recovered.basis.basis_310c == FixedVec3{0, 0x1000, 0}));
    CHECK((machine.state().orientation_shorts ==
            std::array<std::int16_t, 9>{0, 0, 0x1000, 0, 0x1000,
                                        0, 0x1000, 0, 0}));
    CHECK(machine.state().orientation_recovery_base == recovered.target);

    machine.state().orientation_recovery_progress = 0x18001;
    machine.state().orientation_target_shorts = {0x1000, 0, 0};
    const auto skipped = machine.apply_orientation_recovery();
    CHECK(!skipped.handled);
    CHECK(skipped.skipped);
    CHECK(machine.state().basis_310c == recovered.basis.basis_310c);
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
    CHECK(rotated.handled);
    CHECK(rotated.rotation_applied);
    CHECK(rotated.cross_dot == 0x1000);
    CHECK(rotated.rotation_angle == 0xb);
    CHECK((machine.state().orientation_shorts ==
            std::array<std::int16_t, 9>{4095, -69, 0,
                                        69, 4095, 0,
                                        0, 0, 4096}));
    CHECK((rotated.basis.basis_3100 == FixedVec3{4095, 69, 0}));
    CHECK((rotated.basis.basis_310c == FixedVec3{-69, 4095, 0}));
    CHECK((rotated.basis.basis_30f4 == FixedVec3{0, 0, 4096}));

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
    CHECK(negative_result.rotation_angle == -0xb);
    CHECK((negative.state().orientation_shorts ==
            std::array<std::int16_t, 9>{4095, 69, 0,
                                        -69, 4095, 0,
                                        0, 0, 4096}));

    negative.state().raw_state = 2;
    const auto skipped = negative.apply_upright_correction(
        FixedVec3{0, 0, -0x1000});
    CHECK(!skipped.handled);
}

void test_air_collision_recovery() {
    PhysicsStateMachine transient;
    transient.state().raw_state = 1;
    transient.state().ollie.mode_latched = 1;
    const auto transient_result = transient.handle_air_collision_recovery(
        AirCollisionRecoveryInput{10, 0, false, FixedVec3{0x1000, 0, 0}});
    CHECK(transient_result.handled);
    CHECK(transient_result.transient_requested);
    CHECK(!transient_result.recovery_requested);
    CHECK(transient_result.request.from == 1);
    CHECK(transient_result.request.to == 2);
    CHECK(transient_result.request.reason ==
           retail::kAirCollisionTransientReason);
    CHECK(transient_result.request.request_callsite ==
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
    CHECK(recovery_result.handled);
    CHECK(recovery_result.recovery_requested);
    CHECK(recovery_result.velocity_adjusted);
    CHECK(recovery_result.request.from == 1);
    CHECK(recovery_result.request.to == 1);
    CHECK(recovery_result.request.reason == retail::kAirCollisionRecoveryReason);
    CHECK(recovery_result.request.request_callsite ==
           retail::kAirCollisionRecoveryRequestCallsite);
    CHECK((recovery.state().velocity == FixedVec3{0x3000, 0, -0x0800}));
    CHECK(recovery.state().ollie.mode == 0);
    CHECK(!recovery_result.in_progress_set);
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
    CHECK(reset.handled);
    CHECK(reset.acceleration_xz_cleared);
    CHECK(reset.launch_bookkeeping_cleared);
    CHECK(reset.vertical_velocity_clamped);
    CHECK(reset.velocity_decayed);
    CHECK((machine.state().acceleration == FixedVec3{0, 22, 0}));
    CHECK((machine.state().velocity == FixedVec3{900, 0, 2700}));
    CHECK(machine.state().ollie.latched == 0);
    CHECK(machine.state().ollie.in_progress == 0);

    PhysicsStateMachine no_op;
    no_op.state().velocity = FixedVec3{100, 200, 300};
    CHECK(!no_op.apply_blocked_physics_reset(
                       BlockedPhysicsInput{0x100, 1, true})
                .handled);
    CHECK((no_op.state().velocity == FixedVec3{100, 200, 300}));
}

void test_collision_recovery_window() {
    PhysicsStateMachine machine;
    machine.state().heading_deadband = -0x29;
    machine.state().heading_input = 0;
    machine.update_action_mask(retail::kUpActionBit);
    CHECK(machine.update_collision_recovery_window(12));
    CHECK(machine.state().collision_recovery_frame == 12);
    CHECK(machine.update_collision_recovery_window(15));
    CHECK(machine.state().collision_recovery_frame == 12);

    machine.update_action_mask(retail::kLeftActionBit);
    CHECK(!machine.update_collision_recovery_window(16));
    CHECK(machine.state().collision_recovery_frame == 0);

    machine.update_action_mask(retail::kUpActionBit);
    machine.state().heading_input = 0x32;
    CHECK(!machine.update_collision_recovery_window(17));
    CHECK(machine.state().collision_recovery_frame == 0);
}

void test_cross_build_physics_state_labels() {
    CHECK(classify_physics_state(retail::kPhysicsOnGround) ==
           PhysicsStateSemantic::OnGround);
    CHECK(classify_physics_state(retail::kPhysicsInAir) ==
           PhysicsStateSemantic::InAir);
    CHECK(classify_physics_state(retail::kPhysicsOnInvisible) ==
           PhysicsStateSemantic::OnInvisible);
    CHECK(classify_physics_state(retail::kPhysicsInAirStickTo) ==
           PhysicsStateSemantic::InAirStickTo);
    CHECK(classify_physics_state(retail::kPhysicsOnRail) ==
           PhysicsStateSemantic::OnRail);
    CHECK(classify_physics_state(retail::kPhysicsInWallride) ==
           PhysicsStateSemantic::InWallride);
    CHECK(classify_physics_state(retail::kPhysicsInFootplant) ==
           PhysicsStateSemantic::InFootplant);
    CHECK(classify_physics_state(retail::kPhysicsStopped) ==
           PhysicsStateSemantic::Stopped);
    CHECK(classify_physics_state(retail::kPhysicsInHandplant) ==
           PhysicsStateSemantic::InHandplant);
    CHECK(classify_physics_state(-1) == PhysicsStateSemantic::Unknown);

    PhysicsStateMachine machine;
    machine.state().raw_state = retail::kPhysicsOnRail;
    const auto dispatch = machine.dispatch();
    CHECK(dispatch.semantic_state == PhysicsStateSemantic::OnRail);
}

void test_observed_state4_transition_path() {
    PhysicsStateMachine machine;
    machine.state().raw_state = retail::kPhysicsOnGround;

    const auto transient = machine.enter_collision_transient();
    CHECK(transient.from == retail::kPhysicsOnGround);
    CHECK(transient.to == retail::kPhysicsOnInvisible);
    CHECK(transient.reason == retail::kCollisionTransientEnterReason);
    CHECK(transient.request_callsite ==
           retail::kCollisionTransientEnterCallsite);

    const auto transient_dispatch = machine.dispatch();
    CHECK(transient_dispatch.kind == DispatchKind::CollisionTransient);
    CHECK(transient_dispatch.handler_count == 1);
    CHECK(transient_dispatch.handler_pcs[0] == 0x00496550);
    CHECK(machine.state().auxiliary == 1);

    const auto entered = machine.enter_state4_from_collision();
    CHECK(entered.from == retail::kPhysicsOnInvisible);
    CHECK(entered.to == retail::kPhysicsOnRail);
    CHECK(entered.reason == retail::kState4EnterReason);
    CHECK(entered.request_callsite == retail::kState4EnterRequestCallsite);
    CHECK(entered.writer_pc == retail::kStateWriter);
    CHECK(machine.state().phase_state == retail::kPhysicsOnInvisible);

    const auto dispatch = machine.dispatch();
    CHECK(dispatch.raw_state == retail::kPhysicsOnRail);
    CHECK(dispatch.kind == DispatchKind::State4);
    CHECK(dispatch.handler_count == 1);
    CHECK(dispatch.handler_pcs[0] == 0x00494210);

    const auto exited = machine.leave_state4_to_air();
    CHECK(exited.from == retail::kPhysicsOnRail);
    CHECK(exited.to == retail::kPhysicsInAir);
    CHECK(exited.reason == retail::kState4ExitReason);
    CHECK(exited.request_callsite == retail::kState4ExitRequestCallsite);
    CHECK(machine.state().raw_state == retail::kPhysicsInAir);
    CHECK(machine.state().prephysics_blocked == 1);
    CHECK(machine.state().off_ground.transition_count_302c == 1);
    CHECK(machine.state().off_ground.gravity_percent_29f4 == 100);

    const auto air = machine.dispatch();
    CHECK(air.kind == DispatchKind::InAir);
    CHECK(air.handler_pcs[0] == retail::kInAirHandler);

    CHECK(machine.accept_air_contact(true, FixedVec3{11, 22, 33}));
    CHECK(machine.state().raw_state == retail::kPhysicsOnGround);
    CHECK(machine.requests().size() == 4);
    CHECK(machine.requests()[3].from == retail::kPhysicsInAir);
    CHECK(machine.requests()[3].to == retail::kPhysicsOnGround);
    CHECK(machine.requests()[3].reason == retail::kLandingReason);
    CHECK(machine.requests()[3].request_callsite ==
           retail::kLandingRequestCallsite);
}

void test_ground_to_air_leave_ground_transition() {
    PhysicsStateMachine machine;
    machine.state().raw_state = retail::kPhysicsOnGround;
    machine.state().velocity.y = -1234;

    const auto ground_dispatch = machine.dispatch();
    CHECK(ground_dispatch.kind == DispatchKind::Ground);
    CHECK(ground_dispatch.handler_count == 4);
    CHECK(ground_dispatch.handler_pcs[0] == 0x0049dad0);
    CHECK(ground_dispatch.handler_pcs[1] == 0x00496550);
    CHECK(ground_dispatch.handler_pcs[2] == 0x00495cc0);
    CHECK(ground_dispatch.handler_pcs[3] == 0x0049d9c0);

    const auto transitioned = machine.try_ground_to_air(
        GroundAirTransitionInput{100, 0x100, 1001, 0x5001, 0});
    CHECK(transitioned.predicate_evaluated);
    CHECK(transitioned.eligible);
    CHECK(transitioned.transitioned);
    CHECK(transitioned.vertical_velocity_clamped);
    CHECK(transitioned.request.from == retail::kPhysicsOnGround);
    CHECK(transitioned.request.to == retail::kPhysicsInAir);
    CHECK(transitioned.request.reason == retail::kGroundLeaveAirReason);
    CHECK(transitioned.request.request_callsite ==
           retail::kGroundLeaveAirRequestCallsite);
    CHECK(transitioned.off_ground.handled);
    CHECK(transitioned.off_ground.mode == 0x14);
    CHECK(transitioned.off_ground.invocation_callsite ==
           retail::kGroundLeaveAirOffGroundCallsite);
    CHECK(transitioned.off_ground.request.from == retail::kPhysicsInAir);
    CHECK(transitioned.off_ground.request.to == retail::kPhysicsInAir);
    CHECK(transitioned.off_ground.request.reason == retail::kState4ExitReason);
    CHECK(transitioned.off_ground.request.request_callsite ==
           retail::kState4ExitRequestCallsite);
    CHECK(machine.state().off_ground.marker_3204 == 0x28);
    CHECK(machine.state().phase_state == retail::kPhysicsInAir);
    CHECK(machine.requests().size() == 2);
    CHECK(machine.requests()[0].request_callsite ==
           retail::kGroundLeaveAirRequestCallsite);
    CHECK(machine.requests()[1].request_callsite ==
           retail::kOffGroundStateRequestCallsite);
    CHECK(machine.state().velocity.y == 0);

    PhysicsStateMachine recent;
    recent.state().raw_state = retail::kPhysicsOnGround;
    recent.state().velocity.y = 4;
    const auto recent_result = recent.try_ground_to_air(
        GroundAirTransitionInput{100, 0x100, 1001, 0x5000, 99});
    CHECK(recent_result.predicate_evaluated);
    CHECK(recent_result.eligible);
    CHECK(recent_result.transitioned);
    CHECK(!recent_result.vertical_velocity_clamped);

    PhysicsStateMachine rejected;
    rejected.state().raw_state = retail::kPhysicsOnGround;
    const auto rejected_result = rejected.try_ground_to_air(
        GroundAirTransitionInput{100, 0x100, 1000, 0x5001, 0});
    CHECK(rejected_result.predicate_evaluated);
    CHECK(!rejected_result.eligible);
    CHECK(rejected.state().raw_state == retail::kPhysicsOnGround);
}

void test_ground_to_air_predicate_boundaries() {
    GroundAirTransitionInput input{
        100, 0x100, retail::kGroundLeaveAirSlopeThreshold + 1,
        retail::kGroundLeaveAirRecoveryThreshold, 94};

    // (100 - 94) == ((0x100 * 6) >> 8), so the recent-window comparison is
    // false at equality. The recovery comparison is also false at 0x5000.
    CHECK(!ground_leave_air_predicate(input));

    input.last_landing_frame = 95;
    CHECK(ground_leave_air_predicate(input));

    input.last_landing_frame = 94;
    input.recovery_progress = retail::kGroundLeaveAirRecoveryThreshold + 1;
    CHECK(ground_leave_air_predicate(input));

    input.recovery_progress = retail::kGroundLeaveAirRecoveryThreshold;
    input.slope_metric = retail::kGroundLeaveAirSlopeThreshold;
    CHECK(!ground_leave_air_predicate(input));
    input.slope_metric = retail::kGroundLeaveAirSlopeThreshold + 1;

    // A genuinely negative signed age satisfies the strict '< recent_window'
    // branch before the frame counter wraps.
    input.current_frame = 99;
    input.last_landing_frame = 100;
    CHECK(ground_leave_air_predicate(input));

    // This also checks the 32-bit wrap at INT32_MAX -> INT32_MIN without
    // relying on host signed overflow; the wrapped age is one frame.
    input.current_frame = std::numeric_limits<std::int32_t>::min();
    input.last_landing_frame = std::numeric_limits<std::int32_t>::max();
    CHECK(ground_leave_air_predicate(input));
}

void test_ground_leave_air_frame_dispatch_boundary() {
    PhysicsStateMachine machine;
    machine.state().raw_state = retail::kPhysicsOnGround;
    machine.state().velocity.y = -1234;

    CallbackLog log;
    PhysicsFrameCallbacks callbacks;
    callbacks.dispatcher = frame_ground_dispatch;
    callbacks.ground_leave_air_input = frame_ground_leave_air_input;

    const auto result = machine.step_frame(0, callbacks, &log);
    CHECK(result.raw_state == retail::kPhysicsOnGround);
    CHECK(result.kind == DispatchKind::Ground);
    CHECK(result.ground_leave_air.predicate_evaluated);
    CHECK(result.ground_leave_air.eligible);
    CHECK(result.ground_leave_air.transitioned);
    CHECK(result.ground_leave_air.request.from == retail::kPhysicsOnGround);
    CHECK(result.ground_leave_air.request.to == retail::kPhysicsInAir);
    CHECK(result.ground_leave_air.request.reason ==
           retail::kGroundLeaveAirReason);
    CHECK(result.ground_leave_air.off_ground.handled);
    CHECK((log.frame_stages == std::vector<int>{30, 31}));
    CHECK(machine.state().raw_state == retail::kPhysicsInAir);
    CHECK(machine.state().phase_state == retail::kPhysicsInAir);
    CHECK(machine.state().velocity.y == 0);
    CHECK(machine.state().prephysics_blocked == 1);
    CHECK(machine.state().off_ground.marker_3204 ==
           retail::kGroundLeaveAirMarker);
    CHECK(machine.requests().size() == 2);
    CHECK(machine.requests()[0].request_callsite ==
           retail::kGroundLeaveAirRequestCallsite);
    CHECK(machine.requests()[1].request_callsite ==
           retail::kOffGroundStateRequestCallsite);

    PhysicsStateMachine rejected;
    rejected.state().raw_state = retail::kPhysicsOnGround;
    PhysicsFrameCallbacks rejected_callbacks;
    rejected_callbacks.ground_leave_air_input =
        frame_ground_leave_air_rejected;
    const auto rejected_result = rejected.step_frame(0, rejected_callbacks);
    CHECK(rejected_result.raw_state == retail::kPhysicsOnGround);
    CHECK(rejected_result.ground_leave_air.predicate_evaluated);
    CHECK(!rejected_result.ground_leave_air.eligible);
    CHECK(rejected.state().raw_state == retail::kPhysicsOnGround);
    CHECK(rejected.requests().empty());

    PhysicsStateMachine missing;
    missing.state().raw_state = retail::kPhysicsOnGround;
    PhysicsFrameCallbacks missing_callbacks;
    missing_callbacks.ground_leave_air_input =
        frame_ground_leave_air_missing;
    const auto missing_result = missing.step_frame(0, missing_callbacks);
    CHECK(!missing_result.ground_leave_air.predicate_evaluated);
    CHECK(missing.state().raw_state == retail::kPhysicsOnGround);
    CHECK(missing.requests().empty());
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
    CHECK(result.handled);
    CHECK(result.bookkeeping_reset);
    CHECK(result.mode == 0x14);
    CHECK(result.invocation_callsite == 0x00491234);
    CHECK(result.request.from == retail::kPhysicsOnRail);
    CHECK(result.request.to == retail::kPhysicsInAir);
    CHECK(result.request.reason == retail::kState4ExitReason);
    CHECK(result.request.request_callsite == retail::kState4ExitRequestCallsite);
    CHECK(machine.state().prephysics_blocked == 1);
    CHECK(machine.state().off_ground.control_argument_2f60 == 23);
    CHECK(machine.state().off_ground.transition_count_302c == 1);
    CHECK(machine.state().off_ground.gravity_percent_29f4 == 100);
    CHECK(machine.state().ollie.landing_contact_auxiliary == 0);
    CHECK(machine.state().ollie.special_mode == 0);
    CHECK(machine.state().ollie.action_context == 0);
    CHECK(machine.state().ollie.latched == 0);
    CHECK(machine.state().ollie.charge == 0);
    CHECK(machine.state().ollie.recovery_latch == 0);
    CHECK(machine.state().off_ground.word_2c64 == 0);
    CHECK(machine.state().off_ground.word_2c8c == 0);
    CHECK(machine.state().off_ground.word_29e0 == 0);
    CHECK(machine.state().off_ground.word_2bd8 == 0);
    CHECK(machine.state().off_ground.word_2ec8 == 0);
    CHECK(machine.state().off_ground.word_2e90 == 0);
    CHECK(machine.state().off_ground.word_29d4 == 0);
    CHECK(machine.state().off_ground.word_29d0 == 0);
    CHECK(machine.state().off_ground.word_29f0 == 0);
    CHECK(machine.state().off_ground.word_2dd0 == 0);
    CHECK(machine.state().off_ground.word_2f68 == 0);
    CHECK(machine.state().off_ground.word_2e94 == 0);
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
    CHECK(landing.handled);
    CHECK(landing.animation_gate_cleared);
    CHECK(landing.timer_decremented);
    CHECK(landing.landing_marker_consumed);
    CHECK(landing.recovery_marker_cleared);
    CHECK(landing.returned_early);
    CHECK(machine.state().ollie.animation_gate == 0);
    CHECK(machine.state().ollie.landing_effect_state == 0);
    CHECK(machine.state().off_ground.word_2e90 == 0);
    CHECK(machine.state().off_ground.word_2e94 == 0);
    CHECK(machine.state().off_ground.word_2ec4 == 1);

    machine.state().off_ground.word_2e34 = 9;
    const auto no_jump_collision_gate = machine.apply_landing_cleanup();
    CHECK(no_jump_collision_gate.collision_gate_cleared);
    CHECK(machine.state().off_ground.word_2e34 == 0);

    // Once the landing marker has drained, the ordinary no-jump gate still
    // clears a pending recovery marker and returns before deeper animation
    // selection.
    machine.state().off_ground.word_2e94 = 1;
    const auto recovery = machine.apply_landing_cleanup();
    CHECK(recovery.handled);
    CHECK(!recovery.landing_marker_consumed);
    CHECK(recovery.recovery_marker_cleared);
    CHECK(recovery.returned_early);
    CHECK(machine.state().off_ground.word_2e94 == 0);

    PhysicsStateMachine recovery_reset;
    recovery_reset.state().raw_state = retail::kPhysicsOnGround;
    recovery_reset.state().jump.held = 1;
    recovery_reset.state().off_ground.word_2e94 = 1;
    recovery_reset.state().off_ground.word_29f2 = 5;
    recovery_reset.state().off_ground.word_2e2c = 0;
    recovery_reset.state().ollie.recovery_latch = 1;
    const auto reset = recovery_reset.apply_landing_cleanup();
    CHECK(reset.recovery_reset_requested);
    CHECK(reset.off_ground.handled);
    CHECK(reset.off_ground.mode == 5);
    CHECK(reset.off_ground.invocation_callsite ==
           retail::kLandingRecoveryOffGroundCallsite);
    CHECK(reset.off_ground.request.from == retail::kPhysicsOnGround);
    CHECK(reset.off_ground.request.to == retail::kPhysicsInAir);
    CHECK(reset.off_ground.request.reason == retail::kOffGroundReason);
    CHECK(reset.off_ground.request.request_callsite ==
           retail::kOffGroundStateRequestCallsite);
    CHECK(recovery_reset.state().off_ground.word_29f2 == 0);
    CHECK(recovery_reset.state().off_ground.counter_3034 == 1);

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
    CHECK(!scan.recovery_reset_requested);
    CHECK(scan.trick_scan_requested);
    CHECK(!scan.trick_dispatch_requested);
    CHECK(scan.trick_direction_hint);
    CHECK(trick_scan.state().off_ground.word_2c70 == 1);
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
    CHECK((result.velocity == FixedVec3{0x2000, 0, 0}));
    CHECK((result.acceleration == FixedVec3{-0x100, 0, 0}));
    CHECK(result.velocity_projected);

    input.speed_gate = true;
    const auto gated = apply_ground_surface_acceleration(input);
    CHECK((gated.acceleration == FixedVec3{-0xf0, 0, 0}));

    PhysicsStateMachine machine;
    machine.state().velocity = input.velocity;
    machine.state().acceleration = input.acceleration;
    const auto applied = machine.apply_ground_surface_acceleration(input);
    CHECK(machine.state().velocity == applied.velocity);
    CHECK(machine.state().acceleration == applied.acceleration);
}

void test_ground_collision_handoff_contract() {
    GroundCollisionInput input;
    input.velocity = FixedVec3{0x4000, 0x1000, 0};
    input.reference_surface_vector = FixedVec3{0, 0x1964, 0};
    input.surface_normal = PackedSurfaceNormal{0x00001000u, 0};
    input.collision_result = true;

    const auto result = apply_ground_collision_handoff(input);
    CHECK((result.projected_surface_vector == FixedVec3{0, 0x1964, 0}));
    CHECK((result.acceleration == FixedVec3{0, 0x1964, 0}));
    CHECK((result.velocity == FixedVec3{0, 0x4100, 0}));
    CHECK(result.surface_vector_published);
    CHECK(result.velocity_projection.speed_rescaled);

    input.transient_state = true;
    const auto transient = apply_ground_collision_handoff(input);
    CHECK(!transient.surface_vector_published);

    PhysicsStateMachine machine;
    const auto applied = machine.apply_ground_collision_handoff(input);
    CHECK(machine.state().velocity == applied.velocity);
    CHECK(machine.state().acceleration == applied.acceleration);
    CHECK(!machine.state().contact_surface_vector_valid);
    CHECK(machine.state().contact_surface_normal == input.surface_normal);
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
    CHECK(standard_air_landing_accepted(observation, input));

    observation.result = 0;
    CHECK(!standard_air_landing_accepted(observation, input));

    observation.result = 1;
    observation.material_flags_transient = 0;
    input.current_frame = 100;
    input.last_surface_frame = 0;
    input.raw_state = 1;
    CHECK(!standard_air_landing_accepted(observation, input));

    observation.material_flags = 0x40;
    input.raw_state = 3;
    input.current_frame = 120;
    input.last_surface_frame = 100;
    CHECK(standard_air_landing_accepted(observation, input));

    observation.material_flags_transient = 1;
    PhysicsStateMachine machine;
    machine.state().raw_state = 1;
    machine.state().frame_counter = 10347;
    machine.state().jump.inactive_counter = 0;
    const AirContactInput contact{false, FixedVec3{7, 8, 9}};
    CHECK(machine.accept_standard_air_collision(observation, 10320, contact));
    CHECK(machine.state().position == contact.position);
    CHECK(machine.state().raw_state == 0);
    CHECK(machine.state().ollie.landing_contact_identity == 14);

    PhysicsStateMachine rejected;
    rejected.state().raw_state = 1;
    rejected.state().frame_counter = 100;
    CHECK(!rejected.accept_standard_air_collision(observation, 0, contact));
    CHECK(rejected.state().raw_state == 1);
}

void test_gravity_and_velocity_damping_contracts() {
    GravityInput gravity;
    CHECK(compute_gravity_acceleration(gravity) == 13000);
    gravity.raw_state = 2;
    gravity.transient_random = 100;
    CHECK(compute_gravity_acceleration(gravity) == 10400);
    gravity.modifier_c = true;
    gravity.modifier_e = true;
    gravity.global_half_modifier = true;
    CHECK(compute_gravity_acceleration(gravity) == 1950);

    PhysicsStateMachine machine;
    CHECK(machine.initialize_gravity_acceleration(gravity) == 1950);
    CHECK(machine.state().gravity_acceleration == 1950);

    VelocityDampingRandom no_drag;
    no_drag.cap = 1000;
    no_drag.threshold = 1000;
    const auto low = apply_velocity_damping(
        FixedVec3{0x1000, -0x1000, 0}, 0x100, no_drag);
    CHECK(low.initial_speed_metric == 0x1680);
    CHECK(!low.cap_applied);
    CHECK(!low.drag_applied);
    CHECK(low.low_speed_damped);
    CHECK((low.velocity == FixedVec3{0xba0, -0xba0, 0}));

    VelocityDampingRandom cap;
    cap.cap = -500;
    cap.cap_x = -500;
    cap.cap_y = -500;
    cap.cap_z = -500;
    cap.threshold = 1000;
    const auto capped = apply_velocity_damping(
        FixedVec3{0x100000, 0, 0}, 0x100, cap, false);
    CHECK(capped.cap_applied);
    CHECK(capped.cap_component_targets == FixedVec3{});
    CHECK(capped.velocity == FixedVec3{});
    CHECK(!capped.low_speed_damped);

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
    CHECK(independent.cap_applied);
    CHECK(independent.cap_component_targets.x == 0);
    CHECK(independent.cap_component_targets.y != 0);
    CHECK(independent.cap_component_targets.z !=
           independent.cap_component_targets.y);
    CHECK(independent.velocity.x == 0);
    CHECK(independent.velocity.y != independent.velocity.x);
    CHECK(independent.velocity.z != independent.velocity.y);

    machine.state().velocity = FixedVec3{0x1000, -0x1000, 0};
    const auto applied = machine.apply_postphysics_velocity_damping(
        0x100, no_drag);
    CHECK(machine.state().velocity == applied.velocity);
}

void on_launch(PhysicsState& state, std::int32_t launch_state,
               std::uint32_t, void* opaque) {
    auto& log = *static_cast<CallbackLog*>(opaque);
    ++log.launch_calls;
    CHECK(state.raw_state == 0);
    // The retail prephysics routine owns the actual impulse. This sentinel
    // only proves the callback runs before the state request.
    state.velocity.y = launch_state == 1 ? 0x100 : 0x80;
}

void on_dispatch(PhysicsState& state, const DispatchResult& result, void* opaque) {
    auto& log = *static_cast<CallbackLog*>(opaque);
    ++log.dispatch_calls;
    log.last_dispatch = result;
    CHECK(result.raw_state == state.raw_state);
}

void on_position(PhysicsState& state, const FixedVec3&, const FixedVec3&, void* opaque) {
    auto& log = *static_cast<CallbackLog*>(opaque);
    ++log.position_calls;
    log.callback_state_at_position = state.raw_state;
}

void frame_prephysics(PhysicsStateMachine& machine, void* opaque) {
    auto& log = *static_cast<CallbackLog*>(opaque);
    log.frame_stages.push_back(1);
    CHECK(machine.state().frame_counter == 0);
    CHECK(machine.state().kick.press_edge);

    OllieImpulseInput impulse;
    impulse.charge = 7;
    machine.apply_ollie_impulse(impulse);
    const auto request = machine.begin_ollie(LaunchPath::Ordinary);
    CHECK(request.from == 0 && request.to == 1);
}

void frame_movement_action_step(PhysicsStateMachine& machine, void* opaque) {
    auto& log = *static_cast<CallbackLog*>(opaque);
    log.frame_stages.push_back(0);
    CHECK(machine.state().action_mask == retail::kLeftActionBit);
    CHECK(machine.state().left.held == 1);
    CHECK(machine.state().left.press_edge);
    const auto step = machine.apply_ground_action_step(0x200);
    CHECK(step.grounded_path);
    CHECK(step.target_step == 0x7800);
    CHECK(machine.state().movement_target_x == -0x7800);
    CHECK(machine.state().movement_target_z == -0x7800);
    CHECK(machine.state().steering_active == 1);
}

void frame_gravity_initialization(PhysicsStateMachine& machine, void* opaque) {
    auto& log = *static_cast<CallbackLog*>(opaque);
    log.frame_stages.push_back(-1);
    CHECK(machine.state().frame_counter == 0);
    CHECK(machine.initialize_gravity_acceleration(GravityInput{}) == 13000);
}

void frame_gravity_observer(PhysicsStateMachine& machine, void* opaque) {
    auto& log = *static_cast<CallbackLog*>(opaque);
    log.frame_stages.push_back(-2);
    CHECK(machine.state().gravity_acceleration == 13000);
}

void frame_ground_preparation(PhysicsStateMachine& machine, void* opaque) {
    auto& log = *static_cast<CallbackLog*>(opaque);
    log.frame_stages.push_back(2);
    CHECK(machine.state().raw_state == 1);
}

void frame_action_history(PhysicsStateMachine& machine, void* opaque) {
    auto& log = *static_cast<CallbackLog*>(opaque);
    log.frame_stages.push_back(17);
    CHECK(machine.state().frame_counter == 0);
    CHECK(machine.update_action_history(0, 99) == 1);
}

void frame_ground_after_action_history(PhysicsStateMachine&, void* opaque) {
    auto& log = *static_cast<CallbackLog*>(opaque);
    log.frame_stages.push_back(18);
}

void frame_collision_preparation(PhysicsStateMachine& machine, void* opaque) {
    auto& log = *static_cast<CallbackLog*>(opaque);
    log.frame_stages.push_back(3);
    CHECK(machine.state().phase_state == 1);
    CHECK((machine.state().position_history == FixedVec3{10, 20, 30}));
}

void frame_dispatch(PhysicsState&, const DispatchResult& result, void* opaque) {
    auto& log = *static_cast<CallbackLog*>(opaque);
    log.frame_stages.push_back(4);
    CHECK(result.kind == DispatchKind::InAir);
}

void frame_ground_dispatch(PhysicsState&, const DispatchResult& result,
                           void* opaque) {
    auto& log = *static_cast<CallbackLog*>(opaque);
    log.frame_stages.push_back(30);
    CHECK(result.kind == DispatchKind::Ground);
    CHECK(result.handler_count == 4);
    CHECK(result.handler_pcs[0] == 0x0049dad0);
    CHECK(result.handler_pcs[1] == 0x00496550);
    CHECK(result.handler_pcs[2] == 0x00495cc0);
    CHECK(result.handler_pcs[3] == 0x0049d9c0);
}

bool frame_ground_leave_air_input(PhysicsStateMachine& machine,
                                  GroundAirTransitionInput& input,
                                  void* opaque) {
    auto& log = *static_cast<CallbackLog*>(opaque);
    log.frame_stages.push_back(31);
    CHECK(machine.state().raw_state == retail::kPhysicsOnGround);
    input = GroundAirTransitionInput{100, 0x100, 1001, 0x5001, 0};
    return true;
}

bool frame_ground_leave_air_rejected(PhysicsStateMachine& machine,
                                     GroundAirTransitionInput& input,
                                     void*) {
    CHECK(machine.state().raw_state == retail::kPhysicsOnGround);
    input = GroundAirTransitionInput{
        100, 0x100, retail::kGroundLeaveAirSlopeThreshold, 0x5001, 0};
    return true;
}

bool frame_ground_leave_air_missing(PhysicsStateMachine& machine,
                                    GroundAirTransitionInput&, void*) {
    CHECK(machine.state().raw_state == retail::kPhysicsOnGround);
    return false;
}

void frame_air_motion(PhysicsStateMachine& machine, void* opaque) {
    auto& log = *static_cast<CallbackLog*>(opaque);
    log.frame_stages.push_back(5);
    CHECK(machine.state().raw_state == 1);
    machine.integrate_in_air_position(0x100);
}

void frame_air_preparation(PhysicsStateMachine& machine, void* opaque) {
    auto& log = *static_cast<CallbackLog*>(opaque);
    log.frame_stages.push_back(11);
    CHECK(machine.state().raw_state == 1);
    machine.state().basis_30f4 = FixedVec3{0x1000, 0, 0};
    machine.state().basis_310c = FixedVec3{0, 1000, 0};
    CHECK(machine.prepare_in_air_orientation().handled);
}

void frame_state6_air_motion(PhysicsStateMachine& machine, void* opaque) {
    auto& log = *static_cast<CallbackLog*>(opaque);
    log.frame_stages.push_back(9);
    CHECK(machine.state().raw_state == 6);
}

void frame_state6_preair_setup(PhysicsStateMachine& machine, void*) {
    CHECK(machine.state().raw_state == 6);
    const auto result = machine.run_state6_preair_setup(
        State6PreAirInput{100, 100, 0, 0, 0});
    CHECK(result.handled);
    CHECK(result.request.from == 6);
    CHECK(result.request.to == 1);
}

void frame_state6_air_motion_after_setup(PhysicsStateMachine& machine,
                                         void* opaque) {
    auto& log = *static_cast<CallbackLog*>(opaque);
    log.frame_stages.push_back(10);
    CHECK(machine.state().raw_state == 1);
}

void frame_state3_air_motion(PhysicsStateMachine& machine, void* opaque) {
    auto& log = *static_cast<CallbackLog*>(opaque);
    log.frame_stages.push_back(15);
    // The state-3 timeout is a dispatcher post-handler check. The common
    // in-air handler must still observe raw state 3 first.
    CHECK(machine.state().raw_state == 3);
}

void frame_set_acceleration(PhysicsStateMachine& machine, void*) {
    machine.state().acceleration = FixedVec3{7, -8, 9};
}

void frame_blocked_physics_reset(PhysicsStateMachine& machine, void* opaque) {
    auto& log = *static_cast<CallbackLog*>(opaque);
    log.frame_stages.push_back(13);
    CHECK(machine.state().raw_state == 1);
    const auto result = machine.apply_blocked_physics_reset(
        BlockedPhysicsInput{0x100, 10, true});
    CHECK(result.handled);
    CHECK(machine.state().acceleration.x == 0);
    CHECK(machine.state().acceleration.z == 0);
}

void frame_blocked_velocity_observer(PhysicsStateMachine& machine,
                                     void* opaque) {
    auto& log = *static_cast<CallbackLog*>(opaque);
    log.frame_stages.push_back(14);
    CHECK((machine.state().velocity == FixedVec3{900, 0, 2700}));
    CHECK((machine.state().acceleration == FixedVec3{0, 22, 0}));
}

void frame_landing_cleanup(PhysicsStateMachine& machine, void* opaque) {
    auto& log = *static_cast<CallbackLog*>(opaque);
    log.frame_stages.push_back(20);
    CHECK(machine.state().raw_state == retail::kPhysicsOnGround);
    const auto result = machine.apply_landing_cleanup();
    CHECK(result.handled);
}

void frame_landing_collision_preparation(PhysicsStateMachine& machine,
                                         void* opaque) {
    auto& log = *static_cast<CallbackLog*>(opaque);
    log.frame_stages.push_back(19);
    CHECK(machine.state().raw_state == retail::kPhysicsOnGround);
}

bool frame_air_contact(PhysicsStateMachine& machine, FixedVec3& contact, void* opaque) {
    auto& log = *static_cast<CallbackLog*>(opaque);
    log.frame_stages.push_back(6);
    CHECK(machine.state().raw_state == 1);
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
    CHECK(machine.state().raw_state == 0);
    CHECK(machine.state().phase_state == 1);
}

void frame_velocity_integration(PhysicsStateMachine& machine, void* opaque) {
    auto& log = *static_cast<CallbackLog*>(opaque);
    log.frame_stages.push_back(12);
    CHECK(machine.state().raw_state == 0);
    machine.integrate_velocity(0x100);
}

void test_velocity_integration_contract() {
    const auto delta = compute_velocity_delta(FixedVec3{0x100, -0x200, 3}, 0x100);
    CHECK((delta == FixedVec3{0x100, -0x200, 3}));

    PhysicsStateMachine machine;
    machine.state().velocity = FixedVec3{0x1000, -0x2000, 3};
    machine.state().acceleration = FixedVec3{0x100, -0x200, 3};
    CHECK((machine.integrate_velocity(0x100) == delta));
    CHECK((machine.state().velocity == FixedVec3{0x1100, -0x2200, 6}));
}

void test_jump_action_record() {
    JumpActionState action;
    action.update(false);
    CHECK(action.held == 0);
    CHECK(action.held_counter == 0);
    CHECK(action.inactive_counter == 1);

    action.update(true);
    CHECK(action.held == 1);
    CHECK(action.press_edge);
    CHECK(action.edge_latch == 1);
    CHECK(action.held_counter == 1);
    CHECK(action.inactive_counter == 0);

    action.update(true);
    CHECK(!action.press_edge);
    CHECK(action.edge_latch == 1);
    CHECK(action.held_counter == 2);

    CHECK(action.consume_press_edge());
    CHECK(!action.consume_press_edge());

    action.update(false);
    CHECK(action.held == 0);
    CHECK(action.held_counter == 0);
    CHECK(action.inactive_counter == 1);
}

void test_in_air_jump_hold_effect() {
    PhysicsStateMachine machine;

    machine.state().velocity.y = 0x1234;
    machine.state().acceleration.y = -0x2345;
    machine.update_action_mask(retail::kJumpActionBit);
    CHECK(!machine.apply_in_air_jump_hold_effect());
    CHECK(machine.state().velocity.y == 0x1234);
    CHECK(machine.state().acceleration.y == -0x2345);

    machine.update_action_mask(retail::kJumpActionBit);
    machine.update_action_mask(retail::kJumpActionBit);
    CHECK(machine.state().jump.held_counter == 3);
    CHECK(machine.apply_in_air_jump_hold_effect());
    CHECK(machine.state().velocity.y == 0);
    CHECK(machine.state().acceleration.y == 0);

    machine.state().velocity.y = 0x3456;
    machine.state().acceleration.y = -0x4567;
    machine.update_action_mask(0);
    CHECK(!machine.apply_in_air_jump_hold_effect());
    CHECK(machine.state().velocity.y == 0x3456);
    CHECK(machine.state().acceleration.y == -0x4567);
}

void test_action_record_bank() {
    PhysicsStateMachine machine;
    const std::uint32_t directional_mask =
        retail::kLeftActionBit | retail::kUpActionBit | retail::kDownActionBit;
    machine.update_action_mask(directional_mask);

    CHECK(machine.state().left.held == 1);
    CHECK(machine.state().left.press_edge);
    CHECK(machine.state().left.edge_latch == 1);
    CHECK(machine.state().left.held_counter == 1);
    CHECK(machine.state().up.held == 1);
    CHECK(machine.state().down.held == 1);
    CHECK(machine.state().right.held == 0);
    CHECK(machine.state().right.inactive_counter == 1);

    machine.update_action_mask(directional_mask);
    CHECK(!machine.state().left.press_edge);
    CHECK(machine.state().left.edge_latch == 1);
    CHECK(machine.state().left.held_counter == 2);
    CHECK(machine.state().up.held_counter == 2);

    CHECK(machine.state().left.consume_press_edge());
    CHECK(!machine.state().left.consume_press_edge());

    machine.update_action_mask(retail::kRightActionBit | retail::kKickActionBit);
    CHECK(machine.state().left.held == 0);
    CHECK(machine.state().left.inactive_counter == 1);
    CHECK(machine.state().right.press_edge);
    CHECK(machine.state().right.held_counter == 1);
    CHECK(machine.state().kick.press_edge);
    CHECK(machine.state().up.inactive_counter == 1);
    CHECK(machine.state().down.inactive_counter == 1);
}

void test_action_history_ring() {
    PhysicsStateMachine machine;
    machine.update_action_mask(retail::kKickActionBit);

    // FUN_00492190 calls indices 1..8 first, then KICK at index 0xb. Only
    // changed values reach the FUN_00491c90 ring writer.
    CHECK(machine.update_action_history(3, 50) == 2);
    CHECK(machine.state().action_history_write_index == 2);
    CHECK((machine.state().action_history_events[0] ==
            ActionHistoryEvent{3, 1, 50}));
    CHECK((machine.state().action_history_events[1] ==
            ActionHistoryEvent{11, 1, 50}));
    CHECK(machine.state().action_history_previous[12] == 0);

    // No edge means no new ring entry, even though the producer is called
    // again every ground-preparation pass.
    CHECK(machine.update_action_history(3, 51) == 0);
    CHECK(machine.state().action_history_write_index == 2);

    machine.update_action_mask(retail::kJumpActionBit);
    CHECK(machine.update_action_history(0, 52) == 3);
    CHECK((machine.state().action_history_events[2] ==
            ActionHistoryEvent{3, 0, 52}));
    CHECK((machine.state().action_history_events[3] ==
            ActionHistoryEvent{11, 0, 52}));
    CHECK((machine.state().action_history_events[4] ==
            ActionHistoryEvent{12, 1, 52}));
    CHECK(machine.state().action_history_write_index == 5);

    // The writer wraps exactly at 0x20 entries. Starting at slot five, 27
    // transient physics-action changes land on slot 31 and wrap the index to
    // zero; the next release overwrites slot zero.
    for (int step = 0; step != 13; ++step) {
        CHECK(machine.update_action_history(1, 60 + step * 2) == 1);
        CHECK(machine.update_action_history(0, 61 + step * 2) == 1);
    }
    CHECK(machine.update_action_history(1, 86) == 1);
    CHECK(machine.state().action_history_write_index == 0);
    CHECK((machine.state().action_history_events[31] ==
            ActionHistoryEvent{1, 1, 86}));
    CHECK(machine.update_action_history(0, 87) == 1);
    CHECK((machine.state().action_history_events[0] ==
            ActionHistoryEvent{1, 0, 87}));
    CHECK(machine.state().action_history_write_index == 1);
}

void test_clean_ground_air_ground_trace() {
    PhysicsStateMachine machine;
    CallbackLog log;

    machine.update_action_mask(0);
    machine.begin_dispatcher_phase();
    const auto ground = machine.dispatch(on_dispatch, &log);
    CHECK(ground.kind == DispatchKind::Ground);
    CHECK(ground.handler_count == 4);

    machine.update_action_mask(retail::kKickActionBit);
    CHECK(machine.state().kick.press_edge);
    CHECK(machine.state().kick.consume_press_edge());

    const auto launch = machine.begin_ollie(LaunchPath::Ordinary, on_launch, &log);
    CHECK(launch.changed);
    CHECK(launch.from == 0 && launch.to == 1);
    CHECK(launch.reason == retail::kOrdinaryLaunchReason);
    CHECK(launch.request_callsite == retail::kOrdinaryLaunchRequestCallsite);
    CHECK(machine.state().phase_state == 0);
    CHECK(machine.state().velocity.y == 0x100);

    machine.begin_dispatcher_phase();
    const auto air = machine.dispatch(on_dispatch, &log);
    CHECK(air.kind == DispatchKind::InAir);
    CHECK(air.handler_count == 1);
    CHECK(air.handler_pcs[0] == retail::kInAirHandler);
    CHECK(machine.state().phase_state == 1);

    machine.update_action_mask(0);
    CHECK(machine.state().jump.held == 0);
    const FixedVec3 contact{0x100, 0x200, 0x300};
    CHECK(machine.accept_air_contact(true, contact, on_position, &log));
    CHECK(log.callback_state_at_position == 1);
    CHECK(machine.state().position == contact);
    CHECK(machine.state().old_position == FixedVec3{});
    CHECK(machine.state().raw_state == 0);
    CHECK(machine.state().phase_state == 1);
    CHECK(machine.requests().size() == 2);
    CHECK(machine.requests().back().reason == retail::kLandingReason);
    CHECK(machine.requests().back().request_callsite == retail::kLandingRequestCallsite);
    CHECK(log.launch_calls == 1);
    CHECK(log.position_calls == 1);
    CHECK(machine.state().ollie.landing_effect_state == 0);
    CHECK(machine.state().off_ground.word_2e90 == 0);
    CHECK(machine.state().off_ground.word_2e94 == 0);
}

void test_transient_routes_remain_distinct() {
    PhysicsStateMachine machine;

    const auto enter = machine.enter_collision_transient();
    CHECK(enter.from == 0 && enter.to == 2);
    CHECK(enter.reason == retail::kCollisionTransientEnterReason);
    CHECK(enter.request_callsite == retail::kCollisionTransientEnterCallsite);
    const auto state2 = machine.dispatch();
    CHECK(state2.kind == DispatchKind::CollisionTransient);
    CHECK(state2.handler_pcs[0] == 0x00496550);
    CHECK(machine.state().auxiliary == 1);

    const auto exit = machine.exit_collision_transient();
    CHECK(exit.from == 2 && exit.to == 0);
    CHECK(exit.reason == retail::kCollisionTransientExitReason);
    CHECK(exit.request_callsite == retail::kCollisionTransientExitCallsite);

    machine.request_state(4, 0);
    const auto state4 = machine.dispatch();
    CHECK(state4.kind == DispatchKind::State4);
    CHECK(state4.handler_pcs[0] == 0x00494210);
    CHECK(machine.state().auxiliary == 0);

    machine.request_state(3, retail::kAlternateLaunchReason);
    const auto state3 = machine.dispatch();
    CHECK(state3.kind == DispatchKind::InAir);
    CHECK(state3.handler_pcs[0] == retail::kInAirHandler);
    CHECK(state3.raw_state != 1);

    machine.request_state(6, 0);
    const auto state6 = machine.dispatch();
    CHECK(state6.kind == DispatchKind::State6PreAir);
    CHECK(state6.handler_count == 2);
    CHECK(state6.handler_pcs[0] == 0x004993f0);
    CHECK(state6.handler_pcs[1] == retail::kInAirHandler);

    PhysicsStateMachine state6_contact;
    state6_contact.state().raw_state = 6;
    state6_contact.state().position = FixedVec3{10, 20, 30};
    CHECK(state6_contact.accept_air_contact(true, FixedVec3{11, 21, 31}));
    CHECK(state6_contact.state().raw_state == 0);
    CHECK(state6_contact.requests().back().reason == retail::kLandingReason);

    machine.state().position = FixedVec3{90, 91, 92};
    machine.state().position_history = FixedVec3{12, 13, 14};
    machine.request_state(7, 0);
    const auto state7 = machine.dispatch();
    CHECK(state7.kind == DispatchKind::State7Ground);
    CHECK(state7.handler_count == 4);
    CHECK(state7.handler_pcs[0] == 0x0049dad0);
    CHECK(state7.handler_pcs[1] == 0x00496550);
    CHECK(state7.handler_pcs[2] == 0x00495cc0);
    CHECK(state7.handler_pcs[3] == 0x0049d9c0);
    CHECK(machine.state().position == machine.state().position_history);

    PhysicsStateMachine state6_frame;
    state6_frame.state().raw_state = 6;
    CallbackLog frame_log;
    PhysicsFrameCallbacks callbacks;
    callbacks.air_motion = frame_state6_air_motion;
    const auto frame = state6_frame.step_frame(0, callbacks, &frame_log);
    CHECK(frame.kind == DispatchKind::State6PreAir);
    CHECK((frame_log.frame_stages == std::vector<int>{9}));

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
    CHECK(integrated.kind == DispatchKind::State6PreAir);
    CHECK(state6_integrated.state().raw_state == 1);
    CHECK(state6_integrated.state().ollie.charge == 5);
    CHECK(state6_integrated.requests().back().request_callsite ==
           retail::kState6PreAirRequestCallsite);
    CHECK((integrated_log.frame_stages == std::vector<int>{11, 10}));

    PhysicsStateMachine state6_setup;
    state6_setup.state().raw_state = 6;
    state6_setup.state().basis_30f4 = FixedVec3{0x1000, 0, 0};
    state6_setup.state().velocity = FixedVec3{0x1000, 0, 0};
    const auto setup = state6_setup.run_state6_preair_setup(
        State6PreAirInput{100, 100, 0, 0, 0});
    CHECK(setup.handled);
    CHECK(setup.charge == 5);
    CHECK(setup.request.from == 6);
    CHECK(setup.request.to == 1);
    CHECK(setup.request.reason == retail::kState6PreAirReason);
    CHECK(setup.request.request_callsite ==
           retail::kState6PreAirRequestCallsite);
    CHECK(state6_setup.state().ollie.in_progress == 1);
    CHECK(setup.velocity_shaped);
    CHECK(setup.velocity == state6_setup.state().velocity);
    CHECK((setup.velocity == FixedVec3{0x4000, -137625, 0}));
}

void test_ollie_impulse_formula() {
    OllieImpulseInput low_slope;
    low_slope.charge = 0;
    const auto low = compute_ollie_vertical_impulse(low_slope);
    CHECK(!low.high_slope_branch);
    CHECK(low.delta_y == -82944);

    OllieImpulseInput high_slope;
    high_slope.slope_metric = 0x9c4;
    const auto high = compute_ollie_vertical_impulse(high_slope);
    CHECK(high.high_slope_branch);
    CHECK(high.delta_y == -82944);

    OllieImpulseInput wallie;
    wallie.wallie = true;
    const auto wallie_result = compute_ollie_vertical_impulse(wallie);
    CHECK(wallie_result.delta_y == -82944 - 0xf000);
}

void test_ollie_charge_and_air_integration() {
    PhysicsStateMachine machine;
    const auto jump_only = machine.advance_ollie_charge(true, 0, 0);
    CHECK(jump_only.charge == 0);
    CHECK(!jump_only.latched);
    CHECK(!jump_only.pending);

    // The recovered prephysics gate reads the KICK subrecord (+0x30), not
    // the configured JUMP record (+0x00).
    machine.update_action_mask(retail::kKickActionBit);
    const auto charged = machine.advance_ollie_charge(true, 0, 0);
    CHECK(charged.charge == 1);
    CHECK(charged.cap == 0xf);
    CHECK(charged.latched);
    CHECK(charged.pending);
    CHECK(machine.state().ollie.latched == 1);
    CHECK(machine.state().ollie.pending == 1);

    machine.state().ollie.charge = 0xf;
    const auto capped = machine.advance_ollie_charge(true, 0, 0);
    CHECK(capped.capped);
    CHECK(capped.charge == 0xf);

    const auto not_charged = machine.advance_ollie_charge(false, 0, 0);
    CHECK(not_charged.charge == 0xf);

    const FixedVec3 delta = compute_in_air_position_delta(
        FixedVec3{0x1000, -0x1000, 0}, FixedVec3{0x200, -0x200, 0}, 0x100);
    CHECK((delta == FixedVec3{0x1100, -0x1100, 0}));

    machine.state().velocity = FixedVec3{0x1000, -0x1000, 0};
    machine.state().acceleration = FixedVec3{0x200, -0x200, 0};
    CHECK(machine.integrate_in_air_position(0x100) == delta);
    CHECK(machine.state().position == delta);

    machine.state().gravity_acceleration = 13000;
    CHECK((machine.apply_in_air_gravity() == FixedVec3{0x200, 12488, 0}));
    CHECK((machine.state().acceleration == FixedVec3{0x200, 12488, 0}));
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
    CHECK(result.applied);
    CHECK((result.acceleration == FixedVec3{181, -178, 363}));

    input.control_enabled = false;
    const auto disabled = apply_in_air_action_control(input);
    CHECK(!disabled.applied);
    CHECK(disabled.acceleration == input.acceleration);

    PhysicsStateMachine machine;
    machine.state().air_control_enabled = true;
    machine.state().gravity_acceleration = 100;
    machine.state().basis_310c = input.basis_310c;
    machine.state().acceleration = FixedVec3{};
    machine.update_action_mask(retail::kKickActionBit);
    const auto applied = machine.apply_in_air_action_control();
    CHECK(applied.applied);
    CHECK((machine.state().acceleration == FixedVec3{180, -180, 360}));

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
    CHECK((stabilized.acceleration == FixedVec3{-0x100, 0, 0}));
}

void test_ground_action_target_step() {
    PhysicsStateMachine ground;
    ground.state().raw_state = 0;
    ground.state().ground_speed_profile = 0;
    ground.update_action_mask(retail::kLeftActionBit);

    const auto first = ground.apply_ground_action_step(0x200);
    CHECK(first.grounded_path);
    CHECK(first.target_step == 0x7800);
    CHECK(first.target_limit == 0x2d000);
    CHECK(!first.brake_mode);
    CHECK(first.steering_active);
    CHECK(ground.state().movement_target_x == -0x7800);
    CHECK(ground.state().movement_target_z == -0x7800);

    for (int step = 0; step < 5; ++step) {
        ground.apply_ground_action_step(0x200);
    }
    CHECK(ground.state().movement_target_x == -0x2d000);
    CHECK(ground.state().movement_target_z == -0x2d000);

    ground.update_action_mask(0);
    const auto release = ground.apply_ground_action_step(0x200);
    CHECK(!release.steering_active);
    CHECK(ground.state().movement_target_x == -0x21c00);
    CHECK(ground.state().movement_target_z == -0x21c00);

    PhysicsStateMachine analog;
    analog.state().raw_state = 0;
    analog.state().heading_input = 64;
    const auto analog_step = analog.apply_ground_action_step(0x200);
    CHECK(analog_step.target_limit == 0x2d000);
    CHECK(analog_step.steering_active);
    CHECK(analog.state().movement_target_x == 0xf000);
    CHECK(analog.state().movement_target_z == 0xf000);

    PhysicsStateMachine lean_boundary;
    lean_boundary.state().raw_state = 0;
    lean_boundary.state().heading_input = 0x1a;
    const auto analog_boundary = lean_boundary.apply_ground_action_step(0x100);
    CHECK(analog_boundary.steering_active);
    CHECK(lean_boundary.state().movement_target_x == 0x548b);

    PhysicsStateMachine release_boundary;
    release_boundary.state().raw_state = 0;
    release_boundary.state().heading_input = 0x19;
    release_boundary.state().movement_target_x = 0x4000;
    const auto release_boundary_result =
        release_boundary.apply_ground_action_step(0x100);
    CHECK(!release_boundary_result.steering_active);
    CHECK(release_boundary.state().movement_target_x == 0x3000);

    PhysicsStateMachine negative_lean;
    negative_lean.state().raw_state = 0;
    negative_lean.state().heading_input = -0x1a;
    const auto negative_lean_step =
        negative_lean.apply_ground_action_step(0x100);
    CHECK(negative_lean_step.steering_active);
    CHECK(negative_lean.state().movement_target_x == -0x548b);

    PhysicsStateMachine capped_turn;
    capped_turn.state().raw_state = 0;
    capped_turn.state().movement_target_x = 0x2d000;
    capped_turn.update_action_mask(retail::kRightActionBit);
    const auto capped_turn_result = capped_turn.apply_ground_action_step(0x100);
    CHECK(capped_turn_result.steering_active);
    CHECK(capped_turn.state().movement_target_x == 0x2d000);

    PhysicsStateMachine braking;
    braking.state().raw_state = 0;
    braking.state().heading_deadband = 31;
    braking.state().velocity.x = 0x10000;
    braking.update_action_mask(retail::kDownActionBit);
    const auto brake = braking.apply_ground_action_step(0x100);
    CHECK(brake.brake_mode);
    CHECK(brake.target_step == 0x7800);
    // Retail dot/sqrt scale: 0x10000 velocity gives length 0x400 and the
    // grounded brake divisor is ((0x400 * 0x40) >> 12) == 0x10.
    CHECK(braking.state().acceleration.x == -0x1000);

    PhysicsStateMachine airborne;
    airborne.state().raw_state = 1;
    airborne.update_action_mask(retail::kLeftActionBit);
    const auto no_ground_step = airborne.apply_ground_action_step(0x200);
    CHECK(!no_ground_step.grounded_path);
    CHECK(airborne.state().movement_target_x == 0);
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
    CHECK(first_cap.charge == 1);
    CHECK(first_cap.cap == 5);
    CHECK(!first_cap.capped);
    cap_stream.update_action_mask(retail::kKickActionBit);
    cap_input.force_cap = true;
    cap_input.charge_cap_refresh_random.first = 80;
    cap_input.charge_cap_refresh_random.second = 80;
    const auto refreshed_cap = cap_stream.run_ollie_prephysics(cap_input);
    CHECK(refreshed_cap.capped);
    CHECK(refreshed_cap.charge == 7);
    CHECK(refreshed_cap.cap == 7);

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
    CHECK(charging.event == OlliePrePhysicsEvent::Charging);
    CHECK(charging.charge == 1);
    CHECK(machine.state().movement_target_x == 0);
    CHECK(charging.latch_set);
    CHECK(charging.pending_set);
    CHECK(machine.state().raw_state == 0);

    // Release KICK while JUMP remains held. This is the observed launch edge
    // at 0x0049a751; it is not a JUMP press-edge transition.
    machine.update_action_mask(retail::kJumpActionBit);
    input.current_frame = 621;
    const auto launched = machine.run_ollie_prephysics(input);
    CHECK(launched.event == OlliePrePhysicsEvent::Launched);
    CHECK(launched.launch_consumed);
    CHECK(launched.state_requested);
    CHECK(launched.request.from == 0);
    CHECK(launched.request.to == 1);
    CHECK(launched.request.reason == retail::kOrdinaryLaunchReason);
    CHECK(launched.request.request_callsite ==
           retail::kOrdinaryLaunchRequestCallsite);
    CHECK(machine.state().raw_state == 1);
    CHECK(machine.state().ollie.charge == 0);
    CHECK(machine.state().ollie.launch_charge == 1);
    CHECK(machine.state().ollie.latched == 0);
    CHECK(machine.state().ollie.pending == 0);
    CHECK(machine.state().ollie.in_progress == 1);
    CHECK(machine.state().ollie.early_release_count == 0);
    CHECK(machine.state().ollie.launch_auxiliary == 0);
    CHECK(machine.state().ollie.launch_angle_accumulator == 0);
    CHECK(machine.state().ollie.launch_angle_turns == 0);
    CHECK(machine.state().ollie.launch_frame == 621);

    const auto air = machine.dispatch();
    CHECK(air.kind == DispatchKind::InAir);
    CHECK(air.handler_pcs[0] == retail::kInAirHandler);

    CHECK(machine.accept_air_contact(true, FixedVec3{1, 2, 3}));
    CHECK(machine.state().raw_state == 0);
    CHECK(machine.requests().back().reason == retail::kLandingReason);
    // The contact branch writes +0x2ec0=1, then immediately calls
    // FUN_004914d0, which consumes that one-frame marker.
    CHECK(machine.state().ollie.landing_effect_state == 0);
    CHECK(machine.state().ollie.landing_frame ==
           static_cast<std::int32_t>(machine.state().frame_counter));

    // The first grounded prephysics pass consumes the old phase word left by
    // the 1 -> 0 request and clears the launch/re-entry bookkeeping.
    machine.state().ollie.action_context = 9;
    machine.state().ollie.recovery_latch = 7;
    machine.state().phase_state = 1;
    OlliePrePhysicsInput recovery_input;
    recovery_input.current_frame = 682;
    const auto recovery = machine.run_ollie_prephysics(recovery_input);
    CHECK(recovery.recovery_cleared);
    CHECK(machine.state().ollie.in_progress == 0);
    CHECK(machine.state().ollie.mode_latched == 0);
    CHECK(machine.state().ollie.action_context == 0);
    CHECK(machine.state().ollie.recovery_latch == 0);

    PhysicsStateMachine projected_landing;
    projected_landing.state().raw_state = 1;
    projected_landing.state().velocity = FixedVec3{0x4000, 0x1000, 0};
    AirContactInput landing;
    landing.accepted = true;
    landing.position = FixedVec3{7, 8, 9};
    landing.surface_normal = PackedSurfaceNormal{0x00001000u, 0};
    landing.surface_normal_valid = true;
    landing.contact_identity = 6;
    CHECK(projected_landing.accept_air_contact(landing));
    CHECK((projected_landing.state().velocity == FixedVec3{0, 0x4100, 0}));
    CHECK(projected_landing.state().ollie.landing_contact_identity == 6);
    CHECK(projected_landing.state().contact_surface_normal == landing.surface_normal);

    // Retail's alternate branch is the complementary zero-state case: a
    // nonzero latched mode requests raw state 3 and records the alternate
    // launch reason.
    PhysicsStateMachine alternate;
    alternate.state().ollie.mode = 1;
    alternate.update_action_mask(retail::kKickActionBit);
    OlliePrePhysicsInput alternate_input;
    alternate_input.current_frame = 700;
    CHECK(alternate.run_ollie_prephysics(alternate_input).latch_set);
    alternate.update_action_mask(0);
    alternate_input.current_frame = 701;
    const auto alternate_launch = alternate.run_ollie_prephysics(alternate_input);
    CHECK(alternate_launch.state_requested);
    CHECK(alternate_launch.request.to == 3);
    CHECK(alternate_launch.request.reason == retail::kAlternateLaunchReason);
    CHECK(alternate_launch.request.request_callsite ==
           retail::kAlternateLaunchRequestCallsite);
    CHECK(alternate.state().ollie.alternate_state_frame == 701);
    alternate.state().frame_counter = 727;
    const auto alternate_dispatch = alternate.dispatch();
    CHECK(alternate_dispatch.raw_state == 3);
    CHECK(alternate_dispatch.kind == DispatchKind::InAir);
    CHECK(alternate.state().raw_state == 1);
    CHECK(alternate.requests().back().reason == retail::kAlternateStateTimeoutReason);
    CHECK(alternate.requests().back().request_callsite ==
           retail::kAlternateStateTimeoutRequestCallsite);

    // A raw-3 contact inside the launch grace commits position but remains in
    // the alternate air state; the landing request is deferred.
    PhysicsStateMachine grace;
    grace.state().raw_state = 3;
    grace.state().ollie.launch_frame = 100;
    grace.state().frame_counter = 120;
    CHECK(grace.accept_air_contact(true, FixedVec3{4, 5, 6}));
    CHECK((grace.state().position == FixedVec3{4, 5, 6}));
    CHECK(grace.state().raw_state == 3);
    CHECK(grace.requests().empty());

    // A JUMP-only release path has no KICK latch and therefore cannot launch.
    PhysicsStateMachine jump_only;
    jump_only.update_action_mask(retail::kJumpActionBit);
    OlliePrePhysicsInput jump_input;
    jump_input.current_frame = 1;
    const auto no_launch = jump_only.run_ollie_prephysics(jump_input);
    CHECK(no_launch.event == OlliePrePhysicsEvent::None);
    CHECK(!no_launch.state_requested);
    CHECK(jump_only.state().raw_state == 0);

    // The static release path rejects a latch older than twenty frames.
    PhysicsStateMachine stale;
    stale.state().ollie.mode = 1;
    stale.update_action_mask(retail::kKickActionBit);
    OlliePrePhysicsInput stale_input;
    stale_input.current_frame = 10;
    CHECK(stale.run_ollie_prephysics(stale_input).latch_set);
    stale.state().ollie.latch_timestamp = 10;
    stale.update_action_mask(0);
    stale_input.current_frame = 31;
    const auto stale_result = stale.run_ollie_prephysics(stale_input);
    CHECK(stale_result.event == OlliePrePhysicsEvent::StaleLatchCleared);
    CHECK(stale_result.stale_latch_cleared);
    CHECK(!stale_result.state_requested);
    CHECK(stale.state().raw_state == 0);

    // Raw state 5 uses the dedicated wallie cap pair before applying the
    // wallie impulse bias; it must not reuse the ordinary held charge.
    PhysicsStateMachine wallie;
    wallie.state().raw_state = 5;
    wallie.update_action_mask(retail::kKickActionBit);
    OlliePrePhysicsInput wallie_input;
    wallie_input.current_frame = 40;
    CHECK(wallie.run_ollie_prephysics(wallie_input).latch_set);
    wallie.update_action_mask(0);
    wallie_input.current_frame = 41;
    wallie_input.wallie_charge_random.first = 100;
    wallie_input.wallie_charge_random.second = 100;
    const auto wallie_launch = wallie.run_ollie_prephysics(wallie_input);
    CHECK(wallie_launch.launch_consumed);
    CHECK(wallie.state().ollie.wallie == 1);
    CHECK(wallie.state().ollie.launch_charge == 5);
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
    CHECK(result.raw_state == 1);
    CHECK((log.frame_stages ==
            std::vector<int>{1, 2, 3, 4, 11, 5, 6, 7, 19, 20, 12, 8}));
    CHECK(machine.state().frame_counter == 1);
    CHECK(machine.state().older_position == FixedVec3{});
    CHECK((machine.state().position_history == FixedVec3{10, 20, 30}));
    CHECK((machine.state().position == FixedVec3{40, 50, 60}));
    CHECK((machine.state().old_position == FixedVec3{10, -100555, 30}));
    CHECK(machine.state().ollie.in_progress == 1);
    CHECK(machine.state().acceleration == FixedVec3{});

    PhysicsStateMachine handplant;
    handplant.state().raw_state = retail::kPhysicsInHandplant;
    CallbackLog handplant_log;
    PhysicsFrameCallbacks handplant_callbacks;
    handplant_callbacks.landing_collision_preparation =
        frame_landing_collision_preparation;
    handplant_callbacks.landing_cleanup = frame_landing_cleanup;
    handplant.step_frame(0, handplant_callbacks, &handplant_log);
    CHECK(handplant_log.frame_stages.empty());
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

    CHECK(result.raw_state == retail::kPhysicsInAirStickTo);
    CHECK(result.kind == DispatchKind::InAir);
    CHECK((log.frame_stages == std::vector<int>{15}));
    CHECK(machine.state().raw_state == retail::kPhysicsInAir);
    CHECK(machine.requests().size() == 1);
    CHECK(machine.requests().back().from == retail::kPhysicsInAirStickTo);
    CHECK(machine.requests().back().to == retail::kPhysicsInAir);
    CHECK(machine.requests().back().reason ==
           retail::kAlternateStateTimeoutReason);
    CHECK(machine.requests().back().request_callsite ==
           retail::kAlternateStateTimeoutRequestCallsite);
}

void test_action_history_frame_boundary() {
    PhysicsStateMachine machine;
    CallbackLog log;
    PhysicsFrameCallbacks callbacks;
    callbacks.action_history = frame_action_history;
    callbacks.ground_preparation = frame_ground_after_action_history;

    const auto result = machine.step_frame(retail::kKickActionBit, callbacks, &log);
    CHECK(result.kind == DispatchKind::Ground);
    CHECK((log.frame_stages == std::vector<int>{17, 18}));
    CHECK((machine.state().action_history_events[0] ==
            ActionHistoryEvent{11, 1, 99}));
}

void test_non_air_acceleration_boundary() {
    PhysicsStateMachine machine;
    machine.state().acceleration = FixedVec3{1, -2, 3};
    CHECK(machine.reset_non_air_acceleration());
    CHECK(machine.state().acceleration == FixedVec3{});

    machine.state().raw_state = 1;
    machine.state().acceleration = FixedVec3{4, -5, 6};
    CHECK(!machine.reset_non_air_acceleration());
    CHECK((machine.state().acceleration == FixedVec3{4, -5, 6}));

    PhysicsStateMachine frame_ground;
    PhysicsFrameCallbacks callbacks;
    callbacks.movement_action_step = frame_set_acceleration;
    frame_ground.step_frame(0, callbacks);
    CHECK(frame_ground.state().acceleration == FixedVec3{});

    PhysicsStateMachine frame_air;
    frame_air.state().raw_state = 1;
    frame_air.step_frame(0, callbacks);
    CHECK((frame_air.state().acceleration == FixedVec3{7, -8, 9}));
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

    CHECK(result.kind == DispatchKind::InAir);
    CHECK((log.frame_stages == std::vector<int>{13, 14}));
}

void test_movement_action_step_boundary() {
    PhysicsStateMachine machine;
    CallbackLog log;
    PhysicsFrameCallbacks callbacks;
    callbacks.movement_action_step = frame_movement_action_step;

    const auto result = machine.step_frame(retail::kLeftActionBit, callbacks, &log);
    CHECK(result.kind == DispatchKind::Ground);
    CHECK((log.frame_stages == std::vector<int>{0}));
    CHECK(machine.state().movement_target_x == -0x7800);
    CHECK(machine.state().movement_target_z == -0x7800);
    CHECK(machine.state().steering_active == 1);
}

void test_gravity_frame_start_boundary() {
    PhysicsStateMachine machine;
    CallbackLog log;
    PhysicsFrameCallbacks callbacks;
    callbacks.gravity_initialization = frame_gravity_initialization;
    callbacks.movement_action_step = frame_gravity_observer;
    machine.step_frame(0, callbacks, &log);
    CHECK((log.frame_stages == std::vector<int>{-1, -2}));
    CHECK(machine.state().gravity_acceleration == 13000);
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
    test_ground_to_air_predicate_boundaries();
    test_ground_leave_air_frame_dispatch_boundary();
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
    test_state3_timeout_follows_air_handler();
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
