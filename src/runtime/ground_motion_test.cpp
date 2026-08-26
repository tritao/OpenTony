#include "ground_motion.hpp"
#include "player_state.hpp"

#include <bit>
#include <cassert>
#include <iostream>

int main() {
    using opentony::runtime::FixedPosition;
    using opentony::runtime::GroundMotionBranch;
    using opentony::runtime::GroundMotionInput;
    using opentony::runtime::GroundMotionProfileRecords;
    using opentony::runtime::GroundMotionProfileTable;
    using opentony::runtime::PlayerState;

    GroundMotionProfileTable profile_table{};
    profile_table.values[3] = 1;
    profile_table.player_index = 2;
    profile_table.mode = 7;
    profile_table.mode7_selector = 1;
    assert(profile_table.selected_index() == 3);
    assert(profile_table.selected_value_nonzero());
    profile_table.mode = 0;
    assert(!profile_table.selected_value_nonzero());

    GroundMotionProfileRecords records{};
    records.primary_field_10[0] = 1;
    records.primary_field_10[1] = 2;
    records.secondary_field_10[0] = 2;
    records.secondary_field_10[1] = 1;
    records.player_index = 0;
    const auto materialized =
        opentony::runtime::materialize_ground_motion_profile_table(records);
    assert(materialized.values[0] == 1);
    assert(materialized.values[1] == 0);
    assert(materialized.secondary_values[0] == 0);
    assert(materialized.secondary_values[1] == 1);
    assert(materialized.selected_value_nonzero());
    assert(!materialized.selected_secondary_value_nonzero());

    PlayerState player;
    assert(player.ground_motion_threshold() == 0x2e9b6);
    player.set_collision_response({0x1000, 0, 0});
    assert(player.ground_motion_speed_metric() == 0x1000);
    player.set_collision_response({});
    GroundMotionInput ordinary{};
    ordinary.producer_enabled = true;
    ordinary.profile_table_value_nonzero = true;
    ordinary.ordinary_ground_state = true;
    ordinary.response_speed_metric = 0x4000;
    ordinary.response_speed_threshold = 0x10000;
    ordinary.forward_basis_y = 0;
    const auto ordinary_result = player.apply_ground_motion(ordinary);
    assert(ordinary_result.applied);
    assert(ordinary_result.branch == GroundMotionBranch::Ordinary);
    assert(ordinary_result.scale == 1);
    assert(player.motion_correction() == FixedPosition({0, 0, -0x1000}));

    GroundMotionInput metric_limit = ordinary;
    metric_limit.response_speed_metric = 0x4e20;
    player.clear_motion_correction();
    const auto metric_limit_result =
        player.apply_ground_motion(metric_limit);
    assert(metric_limit_result.applied);
    assert(metric_limit_result.branch == GroundMotionBranch::Ordinary);

    GroundMotionInput threshold_limit = ordinary;
    threshold_limit.response_speed_metric = 0x10000;
    threshold_limit.response_speed_threshold = 0x10000;
    player.clear_motion_correction();
    assert(!player.apply_ground_motion(threshold_limit).applied);

    GroundMotionInput basis_limit = ordinary;
    basis_limit.forward_basis_y = 0x1f3;
    player.clear_motion_correction();
    assert(player.apply_ground_motion(basis_limit).applied);
    basis_limit.forward_basis_y = 0x1f4;
    player.clear_motion_correction();
    assert(!player.apply_ground_motion(basis_limit).applied);

    GroundMotionInput closed_correction_gate = ordinary;
    closed_correction_gate.correction_gate_open = false;
    player.clear_motion_correction();
    assert(!player.apply_ground_motion(closed_correction_gate).applied);

    // FUN_0049b010 stores the low word of the 32-bit basis*scale product;
    // it does not saturate a fixed-point component at INT32_MAX/MIN.
    GroundMotionInput wide_basis = ordinary;
    FixedPosition wrapped_correction{};
    const auto wrapped_basis = opentony::runtime::apply_ground_motion(
        wrapped_correction,
        opentony::runtime::RetailBasis{
            {0x40000000, 0, 0},
            {},
            {},
        },
        wide_basis);
    assert(wrapped_basis.applied);
    assert(wrapped_correction[0]
        == std::bit_cast<std::int32_t>(std::uint32_t{0xc0000000U}));

    GroundMotionInput over_speed = ordinary;
    over_speed.response_speed_metric = 0x4e21;
    player.clear_motion_correction();
    assert(!player.apply_ground_motion(over_speed).applied);

    GroundMotionInput uphill = ordinary;
    uphill.forward_basis_y = 0x1f4;
    player.clear_motion_correction();
    assert(!player.apply_ground_motion(uphill).applied);

    GroundMotionInput no_profile = ordinary;
    no_profile.profile_table_value_nonzero = false;
    player.clear_motion_correction();
    assert(!player.apply_ground_motion(no_profile).applied);

    player.clear_motion_correction();

    GroundMotionInput animation = ordinary;
    animation.animation_state = 3;
    animation.animation_frame = 11;
    animation.strong_profile = true;
    animation.cooldown_active = true;
    const auto animation_result = player.apply_ground_motion(animation);
    assert(animation_result.applied);
    assert(animation_result.branch == GroundMotionBranch::Animation2Or3);
    assert(animation_result.scale == 8);
    assert(player.motion_correction() == FixedPosition({0, 0, -0x8000}));

    GroundMotionInput rearm = ordinary;
    rearm.apply_control_side_effects = true;
    rearm.surface_response_metric = -0x4cd;
    rearm.rearm_random_available = true;
    rearm.rearm_random_roll = 0;
    const auto rearm_result = player.apply_ground_motion(rearm);
    assert(rearm_result.cooldown_written);
    assert(rearm_result.cooldown_value == 0x14);
    assert(rearm_result.threshold_written);
    assert(rearm_result.threshold_value == (0xaa * 0x2d000) / 0x118);
    assert(rearm_result.event_reason == 0x2570);
    assert(player.ground_motion_cooldown() == 0x14);
    assert(player.ground_motion_event_pending());
    assert(player.ground_motion_animation_speed() == 0x14000);

    GroundMotionInput animation_event{};
    animation_event.animation_event_enabled = true;
    animation_event.animation_state = 1;
    const auto animation_event_result =
        player.apply_ground_motion(animation_event);
    assert(animation_event_result.animation_event_written);
    assert(animation_event_result.animation_event_parameter == 3);
    assert(animation_event_result.event_reason == 0x2531);
    assert(animation_event_result.animation_speed == 0x14000);
    assert(player.ground_motion_event_reason() == 0x2531);
    assert(player.ground_motion_event_parameter() == 3);

    animation_event.animation_state = 3;
    const auto animation_event_2537 =
        player.apply_ground_motion(animation_event);
    assert(animation_event_2537.animation_event_written);
    assert(animation_event_2537.animation_event_parameter == 0);
    assert(animation_event_2537.event_reason == 0x2537);

    GroundMotionInput event = ordinary;
    event.animation_state = 3;
    event.animation_frame = 11;
    event.pending_animation_event = true;
    event.response_speed_metric = 0x100;
    const auto event_result = player.apply_ground_motion(event);
    assert(event_result.event_reason == 0x22);
    assert(event_result.pending_animation_event_written);
    assert(!event_result.pending_animation_event);
    assert(!player.ground_motion_event_pending());

    GroundMotionInput blocked = ordinary;
    blocked.physics_locked = true;
    const auto blocked_result = player.apply_ground_motion(blocked);
    assert(!blocked_result.applied);
    assert(player.motion_correction() == FixedPosition({0, 0, -0x4000}));

    std::cout << "Ground motion tests passed\n";
}
