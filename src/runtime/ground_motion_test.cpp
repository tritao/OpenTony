#include "ground_motion.hpp"
#include "player_state.hpp"

#include <bit>
#include "tests/test_check.hpp"
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
    CHECK(profile_table.selected_index() == 3);
    CHECK(profile_table.selected_value_nonzero());
    profile_table.mode = 0;
    CHECK(!profile_table.selected_value_nonzero());

    GroundMotionProfileRecords records{};
    records.primary_field_10[0] = 1;
    records.primary_field_10[1] = 2;
    records.secondary_field_10[0] = 2;
    records.secondary_field_10[1] = 1;
    records.player_index = 0;
    const auto materialized =
        opentony::runtime::materialize_ground_motion_profile_table(records);
    CHECK(materialized.values[0] == 1);
    CHECK(materialized.values[1] == 0);
    CHECK(materialized.secondary_values[0] == 0);
    CHECK(materialized.secondary_values[1] == 1);
    CHECK(materialized.selected_value_nonzero());
    CHECK(!materialized.selected_secondary_value_nonzero());

    PlayerState player;
    CHECK(player.ground_motion_threshold() == 0x2e9b6);
    player.set_collision_response({0x1000, 0, 0});
    CHECK(player.ground_motion_speed_metric() == 0x1000);
    player.set_collision_response({});
    GroundMotionInput ordinary{};
    ordinary.producer_enabled = true;
    ordinary.profile_table_value_nonzero = true;
    ordinary.ordinary_ground_state = true;
    ordinary.response_speed_metric = 0x4000;
    ordinary.response_speed_threshold = 0x10000;
    ordinary.forward_basis_y = 0;
    const auto ordinary_result = player.apply_ground_motion(ordinary);
    CHECK(ordinary_result.applied);
    CHECK(ordinary_result.branch == GroundMotionBranch::Ordinary);
    CHECK(ordinary_result.scale == 1);
    CHECK(player.motion_correction() == FixedPosition({0, 0, -0x1000}));

    GroundMotionInput metric_limit = ordinary;
    metric_limit.response_speed_metric = 0x4e20;
    player.clear_motion_correction();
    const auto metric_limit_result =
        player.apply_ground_motion(metric_limit);
    CHECK(metric_limit_result.applied);
    CHECK(metric_limit_result.branch == GroundMotionBranch::Ordinary);

    GroundMotionInput threshold_limit = ordinary;
    threshold_limit.response_speed_metric = 0x10000;
    threshold_limit.response_speed_threshold = 0x10000;
    player.clear_motion_correction();
    CHECK(!player.apply_ground_motion(threshold_limit).applied);

    GroundMotionInput basis_limit = ordinary;
    basis_limit.forward_basis_y = 0x1f3;
    player.clear_motion_correction();
    CHECK(player.apply_ground_motion(basis_limit).applied);
    basis_limit.forward_basis_y = 0x1f4;
    player.clear_motion_correction();
    CHECK(!player.apply_ground_motion(basis_limit).applied);

    GroundMotionInput closed_correction_gate = ordinary;
    closed_correction_gate.correction_gate_open = false;
    player.clear_motion_correction();
    CHECK(!player.apply_ground_motion(closed_correction_gate).applied);

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
    CHECK(wrapped_basis.applied);
    CHECK(wrapped_correction[0]
        == std::bit_cast<std::int32_t>(std::uint32_t{0xc0000000U}));

    GroundMotionInput over_speed = ordinary;
    over_speed.response_speed_metric = 0x4e21;
    player.clear_motion_correction();
    CHECK(player.apply_ground_motion(over_speed).applied);

    GroundMotionInput over_speed_uphill = over_speed;
    over_speed_uphill.forward_basis_y = 0x1f4;
    player.clear_motion_correction();
    CHECK(player.apply_ground_motion(over_speed_uphill).applied);

    GroundMotionInput uphill = ordinary;
    uphill.forward_basis_y = 0x1f4;
    player.clear_motion_correction();
    CHECK(!player.apply_ground_motion(uphill).applied);

    GroundMotionInput no_profile = ordinary;
    no_profile.profile_table_value_nonzero = false;
    player.clear_motion_correction();
    CHECK(!player.apply_ground_motion(no_profile).applied);

    // Animation 0x5e has its own frame-window branch. It must not fall
    // through to the ordinary scale-1 writer before that window opens.
    GroundMotionInput animation_5e = ordinary;
    animation_5e.animation_state = 0x5e;
    animation_5e.animation_frame = 0;
    player.clear_motion_correction();
    CHECK(!player.apply_ground_motion(animation_5e).applied);

    player.clear_motion_correction();

    GroundMotionInput animation = ordinary;
    animation.animation_state = 3;
    animation.animation_frame = 11;
    animation.strong_profile = true;
    animation.cooldown_active = true;
    const auto animation_result = player.apply_ground_motion(animation);
    CHECK(animation_result.applied);
    CHECK(animation_result.branch == GroundMotionBranch::Animation2Or3);
    CHECK(animation_result.scale == 8);
    CHECK(player.motion_correction() == FixedPosition({0, 0, -0x8000}));

    GroundMotionInput rearm = ordinary;
    rearm.apply_control_side_effects = true;
    rearm.surface_response_metric = -0x4cd;
    rearm.rearm_random_available = true;
    rearm.rearm_random_roll = 0;
    const auto rearm_result = player.apply_ground_motion(rearm);
    CHECK(rearm_result.cooldown_written);
    CHECK(rearm_result.cooldown_value == 0x14);
    CHECK(rearm_result.threshold_written);
    CHECK(rearm_result.threshold_value == (0xaa * 0x2d000) / 0x118);
    CHECK(rearm_result.event_reason == 0x2570);
    CHECK(player.ground_motion_cooldown() == 0x14);
    CHECK(player.ground_motion_event_pending());
    CHECK(player.ground_motion_animation_speed() == 0x14000);

    GroundMotionInput animation_event{};
    animation_event.animation_event_enabled = true;
    animation_event.animation_state = 1;
    animation_event.animation_finished = true;
    const auto animation_event_result =
        player.apply_ground_motion(animation_event);
    CHECK(animation_event_result.animation_event_written);
    CHECK(animation_event_result.animation_event_parameter == 3);
    CHECK(animation_event_result.event_reason == 0x2531);
    CHECK(animation_event_result.animation_speed == 0x14000);
    CHECK(player.ground_motion_event_reason() == 0x2531);
    CHECK(player.ground_motion_event_parameter() == 3);

    animation_event.animation_state = 3;
    const auto animation_event_2537 =
        player.apply_ground_motion(animation_event);
    CHECK(animation_event_2537.animation_event_written);
    CHECK(animation_event_2537.animation_event_parameter == 0);
    CHECK(animation_event_2537.event_reason == 0x2537);

    GroundMotionInput event = ordinary;
    event.animation_state = 3;
    event.animation_frame = 11;
    event.pending_animation_event = true;
    event.response_speed_metric = 0x100;
    const auto event_result = player.apply_ground_motion(event);
    CHECK(event_result.event_reason == 0x22);
    CHECK(event_result.pending_animation_event_written);
    CHECK(!event_result.pending_animation_event);
    CHECK(!player.ground_motion_event_pending());

    GroundMotionInput blocked = ordinary;
    blocked.physics_locked = true;
    const auto blocked_result = player.apply_ground_motion(blocked);
    CHECK(!blocked_result.applied);
    CHECK(player.motion_correction() == FixedPosition({0, 0, -0x4000}));

    std::cout << "Ground motion tests passed\n";
}
