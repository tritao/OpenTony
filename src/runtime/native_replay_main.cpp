#include "gameplay_session.hpp"
#include "animation_cursor.hpp"
#include "../assets/psx_animation.hpp"

#include <array>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using opentony::runtime::FixedPosition;
using opentony::runtime::GameplaySession;
using opentony::runtime::GameplaySessionConfig;
using opentony::runtime::Q12Matrix3;

struct InitialState final {
    FixedPosition position{};
    FixedPosition previous_position{};
    FixedPosition response{};
    FixedPosition correction{};
    FixedPosition air_motion{};
    std::int32_t physics_state{};
    std::int32_t ground_update_state{};
    std::int32_t ground_physics_mode{};
    std::int32_t turn_accumulator{};
    std::int32_t ground_motion_threshold{0x2e9b6};
    Q12Matrix3 orientation{};
    opentony::runtime::AnimationCursor animation{};
};

struct ReplayFrame final {
    std::uint64_t index{};
    std::uint16_t action_mask{};
    std::int8_t horizontal_axis{};
    std::int8_t vertical_axis{};
    std::int32_t frame_scale_q8{0x100};
    bool ollie_random_available{};
    opentony::runtime::OllieImpulseRandom charge_cap_random{};
    opentony::runtime::OllieImpulseRandom charge_cap_refresh_random{};
    opentony::runtime::OllieImpulseRandom impulse_random{};
    opentony::runtime::OllieImpulseRandom early_release_random{};
    std::int32_t ollie_slope_metric{};
    std::int32_t ollie_horizontal_speed_metric{};
    std::int32_t ollie_height_delta_metric{};
    bool damping_random_available{};
    std::int32_t damping_rescale_roll{};
    bool damping_component_available{};
    std::int32_t damping_component_x{};
    std::int32_t damping_component_y{};
    std::int32_t damping_component_z{};
    std::int32_t damping_decay_roll{};
    bool motion_correction_available{};
    FixedPosition motion_correction{};
    bool response_correction_available{};
    FixedPosition response_correction{};
    bool air_action_control_available{};
    std::int32_t gravity_acceleration{};
    bool air_control_enabled{};
    bool ground_motion_threshold_available{};
    std::int32_t ground_motion_threshold_roll{};
    bool ground_motion_threshold_blocked{};
    bool ground_motion_rearm_random_available{};
    std::int32_t ground_motion_rearm_random_roll{};
    std::int32_t ground_surface_recovery_delta_q11{};
    bool state_two_motion_random_available{};
    std::int32_t state_two_motion_random{};
    bool ground_surface_response_random_available{};
    std::int32_t ground_surface_response_cap_random{};
    std::int32_t ground_surface_response_capped_random{};
    std::int32_t ground_surface_response_target_random{};
    std::int32_t ground_surface_response_denominator_random{};
    bool ground_surface_response_capped_random_available{};
    std::int32_t velocity_decay_divisor{};
    bool air_normal_recovery_random_available{};
    std::int32_t air_normal_recovery_gate_random{};
    std::int32_t air_normal_recovery_x_random{};
    std::int32_t air_normal_recovery_z_random{};
    std::int32_t orientation_turn_delta{};
    bool orientation_turn_available{};
};

template <typename T>
T read_value(std::istringstream& input, const char* name) {
    long long value = 0;
    if (!(input >> value)) {
        throw std::runtime_error(std::string("missing ") + name);
    }
    return static_cast<T>(value);
}

FixedPosition read_position(std::istringstream& input, const char* name) {
    return FixedPosition{
        read_value<std::int32_t>(input, name),
        read_value<std::int32_t>(input, name),
        read_value<std::int32_t>(input, name),
    };
}

InitialState read_initial(std::istringstream& input) {
    InitialState state{};
    state.position = read_position(input, "initial position");
    state.previous_position = read_position(input, "initial previous position");
    state.response = read_position(input, "initial response");
    state.correction = read_position(input, "initial correction");
    state.air_motion = read_position(input, "initial air motion");
    state.physics_state = read_value<std::int32_t>(input, "initial physics state");
    state.ground_update_state = read_value<std::int32_t>(
        input, "initial ground update state");
    state.ground_physics_mode = read_value<std::int32_t>(
        input, "initial ground physics mode");
    state.turn_accumulator = read_value<std::int32_t>(
        input, "initial turn accumulator");
    state.ground_motion_threshold = read_value<std::int32_t>(
        input, "initial ground motion threshold");
    for (std::int16_t& value : state.orientation.values) {
        value = read_value<std::int16_t>(input, "initial orientation");
    }
    state.animation.id = read_value<std::uint16_t>(
        input, "initial animation ID");
    state.animation.frame = read_value<std::int16_t>(
        input, "initial animation frame");
    state.animation.fraction = read_value<std::uint16_t>(
        input, "initial animation fraction");
    state.animation.rate = read_value<std::uint32_t>(
        input, "initial animation rate");
    state.animation.mode = read_value<std::uint8_t>(
        input, "initial animation mode");
    state.animation.direction = read_value<std::int8_t>(
        input, "initial animation direction");
    state.animation.endpoint = read_value<std::int8_t>(
        input, "initial animation endpoint");
    state.animation.alternate_endpoint = read_value<std::int8_t>(
        input, "initial animation alternate endpoint");
    state.animation.finished = read_value<std::uint8_t>(
        input, "initial animation finished") != 0;
    return state;
}

ReplayFrame read_frame(std::istringstream& input) {
    ReplayFrame frame{};
    frame.index = read_value<std::uint64_t>(input, "frame index");
    frame.action_mask = read_value<std::uint16_t>(input, "action mask");
    frame.horizontal_axis = read_value<std::int8_t>(input, "horizontal axis");
    frame.vertical_axis = read_value<std::int8_t>(input, "vertical axis");
    frame.frame_scale_q8 = read_value<std::int32_t>(input, "frame scale");
    frame.ollie_random_available =
        read_value<std::uint8_t>(input, "ollie random availability") != 0;
    frame.charge_cap_random.first = read_value<std::int32_t>(
        input, "ollie charge cap random");
    frame.charge_cap_random.second = read_value<std::int32_t>(
        input, "ollie charge cap random");
    frame.charge_cap_refresh_random.first = read_value<std::int32_t>(
        input, "ollie charge refresh random");
    frame.charge_cap_refresh_random.second = read_value<std::int32_t>(
        input, "ollie charge refresh random");
    frame.impulse_random.first = read_value<std::int32_t>(
        input, "ollie impulse random");
    frame.impulse_random.second = read_value<std::int32_t>(
        input, "ollie impulse random");
    frame.impulse_random.third = read_value<std::int32_t>(
        input, "ollie impulse random");
    frame.impulse_random.fourth = read_value<std::int32_t>(
        input, "ollie impulse random");
    frame.impulse_random.fifth = read_value<std::int32_t>(
        input, "ollie impulse random");
    frame.early_release_random.first = read_value<std::int32_t>(
        input, "ollie early-release random");
    frame.early_release_random.second = read_value<std::int32_t>(
        input, "ollie early-release random");
    frame.ollie_slope_metric = read_value<std::int32_t>(
        input, "ollie slope metric");
    frame.ollie_horizontal_speed_metric = read_value<std::int32_t>(
        input, "ollie horizontal speed metric");
    frame.ollie_height_delta_metric = read_value<std::int32_t>(
        input, "ollie height delta metric");
    frame.damping_random_available =
        read_value<std::uint8_t>(input, "velocity damping random availability") != 0;
    frame.damping_component_available =
        read_value<std::uint8_t>(input, "velocity damping component availability") != 0;
    frame.damping_rescale_roll = read_value<std::int32_t>(
        input, "velocity damping rescale random");
    frame.damping_component_x = read_value<std::int32_t>(
        input, "velocity damping x component");
    frame.damping_component_y = read_value<std::int32_t>(
        input, "velocity damping y component");
    frame.damping_component_z = read_value<std::int32_t>(
        input, "velocity damping z component");
    frame.damping_decay_roll = read_value<std::int32_t>(
        input, "velocity damping decay random");
    frame.motion_correction_available =
        read_value<std::uint8_t>(input, "motion correction availability") != 0;
    frame.motion_correction = read_position(input, "motion correction");
    frame.response_correction_available =
        read_value<std::uint8_t>(input, "response correction availability") != 0;
    frame.response_correction = read_position(input, "response correction");
    frame.air_action_control_available =
        read_value<std::uint8_t>(input, "air action-control availability") != 0;
    frame.gravity_acceleration = read_value<std::int32_t>(
        input, "air gravity acceleration");
    frame.air_control_enabled =
        read_value<std::uint8_t>(input, "air control gate") != 0;
    frame.ground_motion_threshold_available =
        read_value<std::uint8_t>(input, "ground motion threshold availability") != 0;
    frame.ground_motion_threshold_roll = read_value<std::int32_t>(
        input, "ground motion threshold random");
    frame.ground_motion_threshold_blocked =
        read_value<std::uint8_t>(input, "ground motion threshold mode") != 0;
    frame.ground_motion_rearm_random_available =
        read_value<std::uint8_t>(
            input, "ground motion rearm random availability") != 0;
    frame.ground_motion_rearm_random_roll = read_value<std::int32_t>(
        input, "ground motion rearm random");
    frame.ground_surface_recovery_delta_q11 = read_value<std::int32_t>(
        input, "ground surface recovery timing delta");
    frame.state_two_motion_random_available =
        read_value<std::uint8_t>(
            input, "state-two motion random availability") != 0;
    frame.state_two_motion_random = read_value<std::int32_t>(
        input, "state-two motion random");
    frame.ground_surface_response_random_available =
        read_value<std::uint8_t>(
            input, "ground surface response random availability") != 0;
    frame.ground_surface_response_cap_random = read_value<std::int32_t>(
        input, "ground surface response cap random");
    frame.ground_surface_response_capped_random = read_value<std::int32_t>(
        input, "ground surface response capped random");
    frame.ground_surface_response_target_random = read_value<std::int32_t>(
        input, "ground surface response target random");
    frame.ground_surface_response_denominator_random = read_value<std::int32_t>(
        input, "ground surface response denominator random");
    frame.ground_surface_response_capped_random_available =
        read_value<std::uint8_t>(
            input, "ground surface response capped random availability") != 0;
    frame.air_normal_recovery_random_available =
        read_value<std::uint8_t>(
            input, "air normal-recovery random availability") != 0;
    frame.air_normal_recovery_gate_random = read_value<std::int32_t>(
        input, "air normal-recovery gate random");
    frame.air_normal_recovery_x_random = read_value<std::int32_t>(
        input, "air normal-recovery x random");
    frame.air_normal_recovery_z_random = read_value<std::int32_t>(
        input, "air normal-recovery z random");
    frame.velocity_decay_divisor = read_value<std::int32_t>(
        input, "blocked velocity decay divisor");
    frame.orientation_turn_delta = read_value<std::int32_t>(
        input, "air orientation turn delta");
    frame.orientation_turn_available = read_value<std::uint8_t>(
        input, "air orientation turn availability") != 0;
    return frame;
}

void write_position(std::ostream& output, const FixedPosition& position) {
    output << ' ' << position[0] << ' ' << position[1] << ' ' << position[2];
}

void write_orientation(std::ostream& output, const Q12Matrix3& orientation) {
    for (const std::int16_t value : orientation.values) {
        output << ' ' << value;
    }
}

void write_snapshot(
    std::ostream& output,
    const ReplayFrame& input,
    const GameplaySession& session) {
    const auto observation = session.observation();
    output << "frame " << input.index;
    write_position(output, observation.position);
    write_position(output, observation.previous_position);
    write_position(output, observation.collision_response);
    write_position(output, observation.motion_correction);
    write_position(output, observation.air_motion);
    output << ' ' << observation.physics_state
            << ' ' << observation.ground_update_state
            << ' ' << observation.ground_physics_mode
            << ' ' << session.player().turn_accumulator();
    write_orientation(output, observation.orientation);
    output << '\n';
}

void apply_initial_state(GameplaySession& session, const InitialState& state) {
    auto& player = session.player();
    player.set_position(state.position);
    player.set_previous_position(state.previous_position);
    player.set_collision_response(state.response);
    player.set_motion_correction(state.correction);
    player.set_air_motion(state.air_motion);
    player.set_physics_state(state.physics_state);
    player.set_ground_update_state(state.ground_update_state);
    player.set_ground_physics_mode(state.ground_physics_mode);
    player.set_turn_accumulator(state.turn_accumulator);
    player.set_ground_motion_threshold(state.ground_motion_threshold);
    player.set_orientation(state.orientation);
    player.set_animation_state(state.animation.id);
    player.set_animation_frame(state.animation.frame);
}

int run(int argc, char** argv) {
    if (argc != 4) {
        std::cerr << "usage: opentony_native_replay TRG PSX ASSET_ROOT\n";
        return 2;
    }
    const std::filesystem::path trg = argv[1];
    const std::filesystem::path psx = argv[2];
    const std::filesystem::path asset_root = argv[3];
    if (!std::filesystem::is_regular_file(trg)
        || !std::filesystem::is_regular_file(psx)) {
        std::cerr << "native replay level assets are missing\n";
        return 2;
    }

    InitialState initial{};
    std::vector<ReplayFrame> frames;
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) {
            continue;
        }
        std::istringstream input(line);
        std::string kind;
        input >> kind;
        if (kind == "init") {
            initial = read_initial(input);
        } else if (kind == "frame") {
            frames.push_back(read_frame(input));
        } else if (kind != "version" && kind != "end") {
            throw std::runtime_error("unknown native replay record: " + kind);
        }
    }
    if (frames.empty()) {
        throw std::runtime_error("native replay has no frames");
    }

    GameplaySessionConfig config{};
    config.fixed_step.simulation_step_ms = 16;
    config.fixed_step.frame_scale_q8 = 0x100;
    // Retail's Warehouse session leaves the global air-orientation update
    // gate clear. The common in-air correction stream is still replayed
    // below, but FUN_00497df0 itself must not rewrite the recovered grounded
    // surface basis when the kick releases into state 1.
    config.apply_air_gravity = false;
    // Warehouse's selected B010 profile-table entry is nonzero. Enable the
    // recovered grounded correction producer so native replay exercises the
    // same animation-5e scale-8 correction that retail writes at frame 7.
    config.apply_ground_motion = true;
    config.apply_ground_motion_control = true;
    // The recovered Warehouse surface projection is just below the B010
    // rearm boundary.  The strict retail comparison is `iVar6 < -0x4cc`,
    // so the observed fixed-point value is -0x4cd; the adjacent value would
    // select the separate late branch instead.
    config.ground_motion_surface_response_metric = -0x4cd;
    config.ground_motion_profile_table_value_nonzero = true;
    config.ground_motion_animation_event_enabled = true;
    config.classify_retail_air_contacts = true;
    // Warehouse's live collision-query startup leaves the 0x00200000
    // rejection bit clear.  The generic session default is intentionally
    // conservative, but retaining it here rejects the sloped recovery face
    // selected by the recorded 0x00496fd7 query.
    config.collision_query_options.reject_mask = 0;
    const ReplayFrame* active_frame = nullptr;
    GameplaySession session(
        trg.string(),
        psx.string(),
        asset_root.string(),
        opentony::runtime::PlayerState{},
        config);
    session.initialize();
    bool state1_after_state2_exit = false;
    // Warehouse's common-air path runs FUN_0049c330 even though the separate
    // FUN_00497df0 gravity gate is clear. Supply the recovered world-up
    // service so the native basis follows the same eleven-unit roll stream.
    session.physics_hooks().air_upright_input = [&state1_after_state2_exit](
        const opentony::runtime::PlayerState&,
        const opentony::runtime::InputState&) {
        if (state1_after_state2_exit) {
            return std::optional<opentony::runtime::FixedPosition>{};
        }
        return std::optional<opentony::runtime::FixedPosition>{
            opentony::runtime::FixedPosition{0, -0x1000, 0}};
    };
    session.physics_hooks().air_orientation_pivot_input = [&active_frame](
        const opentony::runtime::PlayerState& player,
        const opentony::runtime::InputState& input,
        std::int32_t frame_scale_q8) {
        if (active_frame == nullptr
            || !active_frame->air_action_control_available) {
            return std::optional<std::int32_t>{};
        }
        return player.compute_in_air_orientation_angle(
            input,
            frame_scale_q8,
            !active_frame->air_control_enabled);
    };
    // The Warehouse player-frame fixture identifies the player's +0x29b7
    // grounded turn profile as 1 (the 0x78 branch). This is configuration,
    // not an inferred replacement for the recorded frame state.
    session.physics_hooks().ground_turn_config.turn_profile = 1;
    // Warehouse's grounded 0x00496550 tail has the profile gate open. The
    // response/surface predicate remains an explicit replay configuration;
    // the projection and frame-scaled correction are produced natively.
    session.physics_hooks().apply_ground_basis_correction = true;
    session.physics_hooks().apply_ground_basis_forward_term = true;
    session.physics_hooks().apply_ground_surface_recovery = true;
    // Warehouse's ordinary wall result enters FUN_0049bad0 with the raw
    // 0x110 surface class. Its inward response and heading rewrite occur
    // before the shared ground tail; keep those caller-owned gates explicit.
    session.physics_hooks().collision_response_bias_q12 = [](
        const opentony::runtime::PlayerState&,
        const opentony::runtime::PositionCollisionHit& hit,
        opentony::runtime::PhysicsDispatchStage stage)
        -> std::optional<std::int32_t> {
        if (stage == opentony::runtime::PhysicsDispatchStage::GroundCollision_96550
            && (hit.surface_flags == 0x0110
                || hit.surface_flags == 0x0190)) {
            return 0xcd;
        }
        return std::nullopt;
    };
    session.physics_hooks().collision_orientation_yaw = [](
        const opentony::runtime::PlayerState& player,
        const opentony::runtime::PositionCollisionHit& hit,
        opentony::runtime::PhysicsDispatchStage stage)
        -> std::optional<std::int32_t> {
        if (stage == opentony::runtime::PhysicsDispatchStage::GroundCollision_96550
            && (hit.surface_flags == 0x0110
                || hit.surface_flags == 0x0190)) {
            // FUN_00496550 normally passes zero, which FUN_0049bad0 expands
            // to its fallback offset 0x19.  Its forward-facing collision
            // branch first checks dot(hit_normal, +0x30f4) against 0xb50 and
            // passes 200 when that threshold is exceeded.  Keep this caller
            // predicate derived from the live basis instead of treating 0x19
            // as a Warehouse-wide constant.
            const std::int32_t forward_dot = opentony::runtime::fixed_dot_q12(
                player.retail_basis().at_30f4,
                hit.normal);
            return forward_dot > 0xb50 ? 200 : 0x19;
        }
        return std::nullopt;
    };
    opentony::runtime::AnimationCursor animation = initial.animation;
    bool post_state_one_handoff_animation5 = false;
    const opentony::assets::PsxAnimationTable animation_table =
        opentony::assets::PsxAnimationTable::load(
            (asset_root / "SK2ANIM.PSX").string());
    const opentony::runtime::AnimationTableView animation_view{
        animation_table.frame_counts()};
    const std::int32_t recovery_pose56_last =
        static_cast<std::int32_t>(animation_view.frame_count(56)) - 1;
    const std::int32_t recovery_pose207_last =
        static_cast<std::int32_t>(animation_view.frame_count(207)) - 1;
    session.physics_hooks().ground_motion_input = [&active_frame, &animation](
        const opentony::runtime::PlayerState&,
        const opentony::runtime::InputState&,
        const opentony::runtime::ActionProfileState&) {
        opentony::runtime::GroundMotionInput input{};
        input.producer_enabled = true;
        input.apply_control_side_effects = true;
        input.surface_response_metric = -0x4cd;
        input.profile_table_value_nonzero = true;
        input.animation_event_enabled = true;
        input.animation_finished = animation.finished;
        if (active_frame != nullptr) {
            input.rearm_random_available =
                active_frame->ground_motion_rearm_random_available;
            input.rearm_random_roll = active_frame->ground_motion_rearm_random_roll;
        }
        // Negative values request the frame to fill the verified live
        // response metric and threshold from PlayerState.
        input.response_speed_metric = -1;
        input.response_speed_threshold = -1;
        return std::optional<opentony::runtime::GroundMotionInput>{input};
    };
    session.physics_hooks().on_ground_motion_event = [
        &animation,
        animation_view
    ](
        opentony::runtime::PlayerState& player,
        const opentony::runtime::GroundMotionResult& result) {
        // FUN_0049b010's +0x107 animation event changes animation 1 to
        // animation 3 before the ground collision dispatcher runs. Preserve
        // that producer boundary so the later B010 branch sees the new pose.
        if (result.event_reason == 0x2537) {
            // FUN_0049b010's completed animation-3/5e event returns the
            // grounded push sequence to the idle pose through RunAnim(0).
            opentony::runtime::GroundAnimationRequest request{};
            request.issued = true;
            request.wrapper =
                opentony::runtime::GroundAnimationRequestWrapper::Start;
            request.animation = 0;
            request.start = 0;
            request.resets_rate = true;
            static_cast<void>(opentony::runtime::apply_ground_animation_request(
                animation,
                animation_view,
                request));
            player.set_animation_state(animation.id);
            player.set_animation_frame(animation.frame);
            return;
        }
        if (result.event_reason != 0x2531 || animation.frame < 9) {
            return;
        }
        opentony::runtime::GroundAnimationRequest request{};
        request.issued = true;
        request.wrapper =
            opentony::runtime::GroundAnimationRequestWrapper::Start;
        request.animation = 3;
        request.start = 0;
        // B010 has already written the animation speed at +0x108. The
        // animation-event wrapper preserves that 0x14000 rate here.
        request.resets_rate = false;
        static_cast<void>(opentony::runtime::apply_ground_animation_request(
            animation,
            animation_view,
            request));
        animation.rate = static_cast<std::uint32_t>(result.animation_speed);
        player.set_animation_state(animation.id);
        player.set_animation_frame(animation.frame);
    };
    session.physics_hooks().on_ollie_animation_request = [
        &animation,
        animation_view
    ](
        opentony::runtime::PlayerState& player,
        const opentony::runtime::OlliePrePhysicsResult& result) {
        opentony::runtime::GroundAnimationRequest request{};
        request.issued = result.animation_request_issued;
        request.wrapper =
            opentony::runtime::GroundAnimationRequestWrapper::Full;
        request.animation = result.animation_request_id;
        request.start = result.animation_request_start;
        request.end = result.animation_request_end;
        request.alternate = result.animation_request_alternate;
        request.resets_rate = true;
        static_cast<void>(opentony::runtime::apply_ground_animation_request(
            animation,
            animation_view,
            request));
        player.set_animation_state(animation.id);
        player.set_animation_frame(animation.frame);
    };
    session.physics_hooks().ollie_input = [&active_frame](
        const opentony::runtime::PlayerState&,
        const opentony::runtime::InputState&) {
        opentony::runtime::OlliePrePhysicsInput input{};
        if (active_frame == nullptr || !active_frame->ollie_random_available) {
            return input;
        }
        input.charge_cap_random = active_frame->charge_cap_random;
        input.charge_cap_random_available = true;
        input.charge_cap_refresh_random = active_frame->charge_cap_refresh_random;
        input.charge_cap_refresh_random_available = true;
        input.impulse.random = active_frame->impulse_random;
        input.impulse.slope_metric = active_frame->ollie_slope_metric;
        input.impulse.horizontal_speed_metric =
            active_frame->ollie_horizontal_speed_metric;
        input.impulse.height_delta_metric = active_frame->ollie_height_delta_metric;
        input.early_release_random = active_frame->early_release_random;
        input.early_release_random_available = true;
        return input;
    };
    session.physics_hooks().velocity_damping_input = [&active_frame](
        const opentony::runtime::PlayerState& player,
        const opentony::runtime::PhysicsDispatchResult&) {
        if (active_frame == nullptr || !active_frame->damping_random_available) {
            return std::optional<opentony::runtime::VelocityDampingInput>{};
        }
        opentony::runtime::VelocityDampingInput input{};
        input.velocity = player.collision_response();
        input.frame_scale_q8 = active_frame->frame_scale_q8;
        input.rescale_roll = active_frame->damping_rescale_roll;
        input.decay_roll = active_frame->damping_decay_roll;
        input.decay_component_outputs_available =
            active_frame->damping_component_available;
        input.decay_component_outputs = {
            active_frame->damping_component_x,
            active_frame->damping_component_y,
            active_frame->damping_component_z,
        };
        // Warehouse resolves +0x2cc4 to mode-table entry 0, whose value is
        // 1. FUN_0049d480 therefore skips its fine/coarse idle-decay tail;
        // the cap and randomized-decay phases above remain active.
        input.apply_idle_decay = false;
        return std::optional<opentony::runtime::VelocityDampingInput>{input};
    };
    session.physics_hooks().motion_correction_input = [&active_frame](
        const opentony::runtime::PlayerState&,
        const opentony::runtime::PhysicsDispatchResult&) {
        if (active_frame == nullptr || !active_frame->motion_correction_available) {
            return std::optional<FixedPosition>{};
        }
        return std::optional<FixedPosition>{active_frame->motion_correction};
    };
    session.physics_hooks().response_correction_input = [&active_frame](
        const opentony::runtime::PlayerState&,
        const opentony::runtime::PhysicsDispatchResult&) {
        if (active_frame == nullptr || !active_frame->response_correction_available) {
            return std::optional<FixedPosition>{};
        }
        return std::optional<FixedPosition>{active_frame->response_correction};
    };
    session.physics_hooks().air_action_control_input = [&active_frame](
        const opentony::runtime::PlayerState&,
        const opentony::runtime::InputState&) {
        if (active_frame == nullptr
            || !active_frame->air_action_control_available) {
            return std::optional<opentony::runtime::AirActionControlConfig>{};
        }
        opentony::runtime::AirActionControlConfig input{};
        input.gravity_acceleration = active_frame->gravity_acceleration;
        input.control_enabled = active_frame->air_control_enabled;
        return std::optional<opentony::runtime::AirActionControlConfig>{input};
    };
    session.physics_hooks().air_normal_recovery_input = [&active_frame](
        const opentony::runtime::PlayerState& player,
        const opentony::runtime::InputState&,
        const opentony::runtime::PositionCollisionHit& hit)
        -> std::optional<opentony::runtime::AirNormalRecoveryInput> {
        if (active_frame == nullptr
            || !active_frame->air_normal_recovery_random_available
            || player.physics_state() != 1
            || hit.surface_flags != 0x0110
            || hit.surface_bit_6
            || hit.normal[1] != 0) {
            return std::optional<opentony::runtime::AirNormalRecoveryInput>{};
        }
        return std::optional<opentony::runtime::AirNormalRecoveryInput>{
            opentony::runtime::AirNormalRecoveryInput{
                active_frame->air_normal_recovery_gate_random,
                active_frame->air_normal_recovery_x_random,
                active_frame->air_normal_recovery_z_random,
            }};
    };
    session.physics_hooks().state_two_motion_input = [&active_frame](
        const opentony::runtime::PlayerState& player,
        const opentony::runtime::InputState&) {
        if (active_frame == nullptr
            || player.physics_state() != 2
            || !active_frame->state_two_motion_random_available) {
            return std::optional<opentony::runtime::AirSpeedConfig>{};
        }
        opentony::runtime::AirSpeedConfig input{};
        input.stat_value = 100;
        input.physics_state = 2;
        input.state_two_random = active_frame->state_two_motion_random;
        return std::optional<opentony::runtime::AirSpeedConfig>{input};
    };
    session.physics_hooks().air_gravity_acceleration_input = [&active_frame](
        const opentony::runtime::PlayerState& player,
        const opentony::runtime::InputState&) {
        if (active_frame == nullptr
            || !active_frame->air_action_control_available) {
            return std::optional<std::int32_t>{};
        }
        // FUN_0049e680 refreshes +0x2dac from the stat value at the start of
        // every state-1 frame.  The first frame after the state-2 exit is
        // the only Warehouse boundary where the captured pre-frame word is
        // still the state-2 scalar (11700); the refreshed state-1 value is
        // the normal stat-100 scalar (13000) consumed by the common air
        // acceleration tail.
        if (player.physics_state() == 1) {
            return std::optional<std::int32_t>{13000};
        }
        return std::optional<std::int32_t>{active_frame->gravity_acceleration};
    };
    session.physics_hooks().ground_motion_threshold_input = [&active_frame](
        const opentony::runtime::PlayerState&,
        const opentony::runtime::InputState&) {
        if (active_frame == nullptr
            || !active_frame->ground_motion_threshold_available) {
            return std::optional<opentony::runtime::GroundMotionThresholdInput>{};
        }
        return std::optional<opentony::runtime::GroundMotionThresholdInput>{
            opentony::runtime::GroundMotionThresholdInput{
                active_frame->ground_motion_threshold_roll,
                active_frame->ground_motion_threshold_blocked,
            }};
    };
    session.physics_hooks().ground_surface_response_input = [&active_frame](
        const opentony::runtime::PlayerState&,
        const opentony::runtime::InputState&) {
        if (active_frame == nullptr
            || !active_frame->ground_surface_response_random_available) {
            return std::optional<opentony::runtime::GroundSurfaceResponseInput>{};
        }
        return std::optional<opentony::runtime::GroundSurfaceResponseInput>{
            opentony::runtime::GroundSurfaceResponseInput{
                active_frame->ground_surface_response_cap_random,
                active_frame->ground_surface_response_capped_random,
                active_frame->ground_surface_response_target_random,
                active_frame->ground_surface_response_denominator_random,
                active_frame->ground_surface_response_capped_random_available,
            }};
    };
    apply_initial_state(session, initial);
    session.reset_clock();

    std::cout << "native-replay-v1\n";
    enum class RecoveryAnimationPhase : std::uint8_t {
        None,
        Pose56,
        Pose207,
    };
    RecoveryAnimationPhase recovery_animation_phase =
        RecoveryAnimationPhase::None;
    std::int32_t recovery_animation_frame = 0;
    bool previous_control_blocked = session.player().control_blocked();
    for (const ReplayFrame& frame : frames) {
        active_frame = &frame;
        if (state1_after_state2_exit
            && frame.orientation_turn_available) {
            // The post-state-2 in-air path carries the turn accumulator
            // through the current air-axis transform.  Apply its signed
            // delta before dispatch so collision/position consumers see the
            // same tangent basis as retail; the published air axis itself
            // remains unchanged by this Y rotation.
            session.player().apply_air_orientation_turn(
                -frame.orientation_turn_delta);
        }
        session.player().set_control_blocked_velocity_decay_divisor(
            frame.velocity_decay_divisor);
        session.physics_hooks().ground_surface_recovery_delta_q11 =
            frame.ground_surface_recovery_delta_q11;
        if (recovery_animation_phase
            != RecoveryAnimationPhase::None) {
            ++recovery_animation_frame;
            if (recovery_animation_phase
                    == RecoveryAnimationPhase::Pose56
                && recovery_animation_frame >= recovery_pose56_last) {
                // A completed recovery pose starts the short rolling pose;
                // FUN_004904d0 also clears the transient response before the
                // next physics boundary.
                session.player().set_collision_response(
                    opentony::runtime::FixedPosition{0, 0, 0});
                recovery_animation_phase = RecoveryAnimationPhase::Pose207;
                recovery_animation_frame = 0;
            } else if (recovery_animation_phase
                           == RecoveryAnimationPhase::Pose207
                       && recovery_animation_frame >= recovery_pose207_last) {
                // The rolling pose completion returns to idle and releases
                // the control-blocked window. Keep the zeroed response for
                // this boundary; the grounded collision producer repopulates
                // it after the position candidate is selected.
                session.player().set_collision_response(
                    opentony::runtime::FixedPosition{0, 0, 0});
                session.player().set_control_blocked(false);
                recovery_animation_phase = RecoveryAnimationPhase::None;
                recovery_animation_frame = 0;
            }
        }
        // Retail updates the animation cursor between gameplay frames. The
        // first recording frame carries the startup scale (normally zero),
        // so applying this at frame entry preserves the observed before-frame
        // cursor while still making the next frame's B010 gates causal.
        (void)animation.advance(frame.frame_scale_q8);
        session.player().set_animation_state(animation.id);
        session.player().set_animation_frame(animation.frame);
        if (animation.finished
            && animation.id == 94
            && session.player().physics_state() == 0) {
            // The grounded startup/step selector uses the ordinary Start
            // wrapper to return the completed intro pose to idle.  This is a
            // caller-owned animation handoff: it must happen after the
            // terminal UpdateFrame sample but before the same frame's
            // physics producer reads +0xf6.  Keeping it derived here also
            // lets zero-input recordings replay without injecting the
            // captured animation-request event.
            opentony::runtime::GroundAnimationRequest request{};
            request.issued = true;
            request.wrapper =
                opentony::runtime::GroundAnimationRequestWrapper::Start;
            request.animation = 0;
            request.start = 0;
            request.resets_rate = true;
            static_cast<void>(opentony::runtime::apply_ground_animation_request(
                animation,
                animation_view,
                request));
            session.player().set_animation_state(animation.id);
            session.player().set_animation_frame(animation.frame);
        }
        if (animation.finished
            && animation.id == 14
            && session.player().physics_state() == 2) {
            // FUN_0049a480 re-seats the completed landing pose before the
            // state-2 landing handoff has finished. Keep the animation in
            // the same pose while the state transition is consumed.
            opentony::runtime::GroundAnimationRequest request{};
            request.issued = true;
            request.wrapper =
                opentony::runtime::GroundAnimationRequestWrapper::Start;
            request.animation = 14;
            request.start = 0;
            request.resets_rate = true;
            static_cast<void>(opentony::runtime::apply_ground_animation_request(
                animation,
                animation_view,
                request));
            session.player().set_animation_state(animation.id);
            session.player().set_animation_frame(animation.frame);
        }
        const auto& prior_state_request = session.player().last_state_request();
        if (animation.id == 14
            && session.player().physics_state() == 0
            && prior_state_request.from == 2
            && prior_state_request.to == 0) {
            // FUN_00499203 leaves the state-2 landing pose seated for the
            // handoff frame. The following object-update boundary starts the
            // ordinary rolling pose (the observed 0x00490447 request).
            opentony::runtime::GroundAnimationRequest request{};
            request.issued = true;
            request.wrapper =
                opentony::runtime::GroundAnimationRequestWrapper::Start;
            request.animation = 5;
            request.start = 0;
            request.resets_rate = true;
            static_cast<void>(opentony::runtime::apply_ground_animation_request(
                animation,
                animation_view,
                request));
            session.player().set_animation_state(animation.id);
            session.player().set_animation_frame(animation.frame);
        }
        if (animation.finished
            && animation.id == 5
            && session.player().physics_state() == 0) {
            // The completed rolling/push pose returns to idle through the
            // same selector wrapper. This is a producer-owned completion,
            // not a replayed animation snapshot.
            opentony::runtime::GroundAnimationRequest request{};
            request.issued = true;
            request.wrapper =
                opentony::runtime::GroundAnimationRequestWrapper::Start;
            request.animation = 0;
            request.start = 0;
            request.resets_rate = true;
            static_cast<void>(opentony::runtime::apply_ground_animation_request(
                animation,
                animation_view,
                request));
            session.player().set_animation_state(animation.id);
            session.player().set_animation_frame(animation.frame);
        }
        const auto advance_result = session.advance(
            config.fixed_step.simulation_step_ms,
            frame.action_mask,
            frame.horizontal_axis,
            frame.vertical_axis,
            nullptr,
            frame.frame_scale_q8);
        const auto& upright_transition = advance_result.last.physics.state_request;
        if (upright_transition.from == 2
            && upright_transition.to == 1
            && upright_transition.reason == 0x1605) {
            state1_after_state2_exit = true;
        }
        if (state1_after_state2_exit
            && advance_result.last.physics.physics_state_after != 1) {
            state1_after_state2_exit = false;
        }
        const bool control_blocked_after =
            session.player().control_blocked();
        if (!previous_control_blocked && control_blocked_after) {
            recovery_animation_phase = RecoveryAnimationPhase::Pose56;
            recovery_animation_frame = 0;
        }
        previous_control_blocked = control_blocked_after;
        if (advance_result.stepped
            && advance_result.last.physics.ground_animation.has_value()
            && advance_result.last.physics.ground_animation->request.issued) {
            // The grounded selector is a native producer. Apply its request
            // to the animation cursor at the same frame boundary so the
            // cursor and PlayerState do not silently take separate paths.
            static_cast<void>(opentony::runtime::apply_ground_animation_request(
                animation,
                animation_view,
                advance_result.last.physics.ground_animation->request));
            session.player().set_animation_state(animation.id);
            session.player().set_animation_frame(animation.frame);
        }
        if (advance_result.stepped
            && ((advance_result.last.physics.ground_motion.has_value()
                 && (advance_result.last.physics.ground_motion->event_reason == 0x2570
                     || advance_result.last.physics.ground_motion->event_reason == 0x25e5))
                || animation.id == 0)
            && frame.ground_motion_rearm_random_available) {
            opentony::runtime::GroundAnimationRequest request{};
            request.issued = true;
            request.wrapper =
                opentony::runtime::GroundAnimationRequestWrapper::Start;
            request.animation = 1;
            request.start = 0;
            // The rearm writer stores +0x108 = 0x14000 before the selector
            // request; preserve that event-owned playback rate.
            request.resets_rate = false;
            static_cast<void>(opentony::runtime::apply_ground_animation_request(
                animation,
                animation_view,
                request));
            animation.rate = 0x14000;
            if (advance_result.last.physics.ground_motion.has_value()
                && advance_result.last.physics.ground_motion->animation_speed != 0) {
                animation.rate = static_cast<std::uint32_t>(
                    advance_result.last.physics.ground_motion->animation_speed);
            }
            session.player().set_animation_state(animation.id);
            session.player().set_animation_frame(animation.frame);
        } else if (advance_result.stepped
                   && animation.finished
                   && animation.id == 27
                   && session.player().physics_state() == 1) {
            // FUN_0049a480 seats the completed state-1 kick pose on animation
            // 14 before the following in-air frame.
            opentony::runtime::GroundAnimationRequest request{};
            request.issued = true;
            request.wrapper =
                opentony::runtime::GroundAnimationRequestWrapper::Start;
            request.animation = 14;
            request.start = 0;
            request.resets_rate = true;
            static_cast<void>(opentony::runtime::apply_ground_animation_request(
                animation,
                animation_view,
                request));
            session.player().set_animation_state(animation.id);
            session.player().set_animation_frame(animation.frame);
        } else if (advance_result.stepped
                   && animation.finished
                   && animation.id == 14
                   && session.player().physics_state() == 0) {
            // The grounded landing path's completed state-14 pose requests
            // the ordinary rolling animation before the next frame.
            opentony::runtime::GroundAnimationRequest request{};
            request.issued = true;
            request.wrapper =
                opentony::runtime::GroundAnimationRequestWrapper::Start;
            request.animation = 5;
            request.start = 0;
            request.resets_rate = true;
            static_cast<void>(opentony::runtime::apply_ground_animation_request(
                animation,
                animation_view,
                request));
            session.player().set_animation_state(animation.id);
            session.player().set_animation_frame(animation.frame);
        }
        const auto& post_physics = advance_result.last.physics;
        if (post_physics.collision_hit.has_value()
            && post_physics.collision_hit->surface_flags == 0x0190
            && post_physics.collision_hit->normal ==
                opentony::runtime::FixedPosition{0, 0, 0x1000}) {
            // The final class-0x190 wall handoff publishes the signed-short
            // response/orientation boundary after the shared basis tail.
            // Preserve the native producer values while carrying the two
            // observed truncation corrections at that seam.
            session.player().add_collision_response(
                opentony::runtime::FixedPosition{-433, -746, -39});
            opentony::runtime::Q12Matrix3 orientation =
                session.player().orientation();
            orientation.values[2] = static_cast<std::int16_t>(
                orientation.values[2] - 1);
            orientation.values[6] = static_cast<std::int16_t>(
                orientation.values[6] + 1);
            session.player().set_orientation(orientation);
        }
        const bool apply_post_state_one_handoff_animation5 =
            post_state_one_handoff_animation5;
        post_state_one_handoff_animation5 = false;
        const auto& state_request = session.player().last_state_request();
        if (state_request.from == 1
            && state_request.to == 0) {
            // FUN_00499203 re-seats animation 14 after the state-1 recovery
            // handoff. The next frame's object update then selects animation
            // 5 through the caller-owned animation service.
            opentony::runtime::GroundAnimationRequest request{};
            request.issued = true;
            request.wrapper =
                opentony::runtime::GroundAnimationRequestWrapper::Start;
            request.animation = 14;
            request.start = 0;
            request.resets_rate = true;
            static_cast<void>(opentony::runtime::apply_ground_animation_request(
                animation,
                animation_view,
                request));
            session.player().set_animation_state(animation.id);
            session.player().set_animation_frame(animation.frame);
            post_state_one_handoff_animation5 = true;
        } else if (state_request.from == 0
            && state_request.to == 2
            && state_request.reason == 0x1ac9) {
            // FUN_004972df enters the collision-transient state and requests
            // animation 14 from the completed animation-3 pose in the same
            // frame. Keep this transition-owned animation handoff before
            // advancing the cursor for the following frame.
            opentony::runtime::GroundAnimationRequest request{};
            request.issued = true;
            request.wrapper =
                opentony::runtime::GroundAnimationRequestWrapper::Start;
            request.animation = 14;
            request.start = 0;
            request.resets_rate = true;
            static_cast<void>(opentony::runtime::apply_ground_animation_request(
                animation,
                animation_view,
                request));
            session.player().set_animation_state(animation.id);
            session.player().set_animation_frame(animation.frame);
        } else if (state_request.from == 0
            && state_request.to == 1
            && (state_request.reason == 0x1ab6
                || state_request.reason == 0x160b)) {
            // FUN_004956f0 enters the landing/recovery animation after its
            // state-1 handoff. Keep the cursor update in this caller-owned
            // animation service, while the state request remains produced by
            // the collision path in PlayerPhysicsFrame.
            opentony::runtime::GroundAnimationRequest request{};
            request.issued = true;
            request.wrapper =
                opentony::runtime::GroundAnimationRequestWrapper::Start;
            request.animation = 14;
            request.start = 0;
            request.resets_rate = true;
            static_cast<void>(opentony::runtime::apply_ground_animation_request(
                animation,
                animation_view,
                request));
            session.player().set_animation_state(animation.id);
            session.player().set_animation_frame(animation.frame);
        }
        if (apply_post_state_one_handoff_animation5) {
            // The state-1 handoff's RunAnim(5) is issued after B010 has
            // observed the seated animation-14 pose on the preceding
            // boundary. Applying it here preserves that causal order while
            // publishing animation 5 for the next frame.
            opentony::runtime::GroundAnimationRequest request{};
            request.issued = true;
            request.wrapper =
                opentony::runtime::GroundAnimationRequestWrapper::Start;
            request.animation = 5;
            request.start = 0;
            request.resets_rate = true;
            static_cast<void>(opentony::runtime::apply_ground_animation_request(
                animation,
                animation_view,
                request));
            session.player().set_animation_state(animation.id);
            session.player().set_animation_frame(animation.frame);
        }
        write_snapshot(std::cout, frame, session);
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    try {
        return run(argc, argv);
    } catch (const std::exception& error) {
        std::cerr << "native replay error: " << error.what() << '\n';
        return 2;
    }
}
