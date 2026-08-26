cmake_minimum_required(VERSION 3.20)

project(OpenTonyNative LANGUAGES CXX)

option(OPENTONY_BUILD_TESTS "Build native reconstruction tests" ON)

add_library(opentony_camera STATIC
    src/camera/camera_runtime.cpp
)
target_compile_features(opentony_camera PUBLIC cxx_std_20)
target_include_directories(opentony_camera PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(opentony_camera PRIVATE -Wall -Wextra -Wpedantic)

add_library(opentony_assets STATIC
    src/assets/pre_catalog.cpp
    src/assets/pre_asset.cpp
    src/assets/psx_asset.cpp
    src/assets/psx_catalog.cpp
    src/assets/psx_collision.cpp
    src/assets/tricks_bin.cpp
)
target_compile_features(opentony_assets PUBLIC cxx_std_20)
target_include_directories(opentony_assets PUBLIC src/assets)
target_compile_options(opentony_assets PRIVATE -Wall -Wextra -Wpedantic)

add_library(opentony_trg STATIC
    src/trg/trg_runtime.cpp
    src/trg/gap_table.cpp
    src/trg/retail_gap_table.cpp
    src/trg/pickup_runtime.cpp
    src/trg/level_trigger_state.cpp
    src/trg/level_scene_registry.cpp
    src/trg/level_render_snapshot.cpp
    src/trg/level_runtime.cpp
)
target_compile_features(opentony_trg PUBLIC cxx_std_20)
target_include_directories(opentony_trg PUBLIC src/trg)
target_compile_options(opentony_trg PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(opentony_trg PUBLIC opentony_assets)

add_library(opentony_runtime STATIC
    src/runtime/collision_response.cpp
    src/runtime/air_motion.cpp
    src/runtime/fixed_matrix.cpp
    src/runtime/fixed_math.cpp
    src/runtime/fixed_step_driver.cpp
    src/runtime/gameplay_frame.cpp
    src/runtime/gameplay_presentation.cpp
    src/runtime/gameplay_session.cpp
    src/runtime/ground_brake.cpp
    src/runtime/ground_physics.cpp
    src/runtime/ground_motion.cpp
    src/runtime/ground_animation.cpp
    src/runtime/ground_motion_threshold.cpp
    src/runtime/ground_turn.cpp
    src/runtime/action_profile.cpp
    src/runtime/action_sequence.cpp
    src/runtime/action_sequence_builder.cpp
    src/runtime/input_state.cpp
    src/runtime/input_bindings.cpp
    src/runtime/level_frame.cpp
    src/runtime/ollie.cpp
    src/runtime/physics_dispatch.cpp
    src/runtime/physics_frame.cpp
    src/runtime/physics_replay.cpp
    src/runtime/platform_response.cpp
    src/runtime/player_state.cpp
    src/runtime/position_commit.cpp
    src/runtime/psx_collision_probe.cpp
    src/runtime/queued_motion.cpp
    src/runtime/action_commands.cpp
    src/runtime/velocity_damping.cpp
    src/runtime/velocity_projection.cpp
)
target_compile_features(opentony_runtime PUBLIC cxx_std_20)
target_include_directories(opentony_runtime PUBLIC src/runtime)
target_compile_options(opentony_runtime PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(opentony_runtime PUBLIC opentony_trg opentony_camera)

if(OPENTONY_BUILD_TESTS)
    enable_testing()

    add_executable(opentony_camera_runtime_test
        src/camera/camera_runtime_test.cpp
    )
    target_link_libraries(opentony_camera_runtime_test PRIVATE opentony_camera)
    target_compile_features(opentony_camera_runtime_test PRIVATE cxx_std_20)
    target_compile_options(opentony_camera_runtime_test PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME opentony_camera_runtime_test COMMAND opentony_camera_runtime_test)

    add_executable(opentony_trg_test
        src/trg/trg_runtime_test.cpp
    )
    target_link_libraries(opentony_trg_test PRIVATE opentony_trg)
    target_compile_features(opentony_trg_test PRIVATE cxx_std_20)
    target_compile_options(opentony_trg_test PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME opentony_trg_test COMMAND opentony_trg_test)

    add_executable(opentony_trg_state_test
        src/trg/level_trigger_state_test.cpp
    )
    target_link_libraries(opentony_trg_state_test PRIVATE opentony_trg)
    target_compile_features(opentony_trg_state_test PRIVATE cxx_std_20)
    target_compile_options(opentony_trg_state_test PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME opentony_trg_state_test COMMAND opentony_trg_state_test)

    add_executable(opentony_level_runtime_test
        src/trg/level_runtime_test.cpp
    )
    target_link_libraries(opentony_level_runtime_test PRIVATE opentony_runtime)
    target_compile_features(opentony_level_runtime_test PRIVATE cxx_std_20)
    target_compile_options(opentony_level_runtime_test PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME opentony_level_runtime_test COMMAND opentony_level_runtime_test)

    add_executable(opentony_level_render_snapshot_test
        src/trg/level_render_snapshot_test.cpp
    )
    target_link_libraries(opentony_level_render_snapshot_test PRIVATE opentony_trg)
    target_compile_features(opentony_level_render_snapshot_test PRIVATE cxx_std_20)
    target_compile_options(opentony_level_render_snapshot_test PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME opentony_level_render_snapshot_test COMMAND opentony_level_render_snapshot_test)

    add_executable(opentony_input_state_test
        src/runtime/input_state_test.cpp
    )
    target_link_libraries(opentony_input_state_test PRIVATE opentony_runtime)
    target_compile_features(opentony_input_state_test PRIVATE cxx_std_20)
    target_compile_options(opentony_input_state_test PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME opentony_input_state_test COMMAND opentony_input_state_test)

    add_executable(opentony_action_profile_test
        src/runtime/action_profile_test.cpp
    )
    target_link_libraries(opentony_action_profile_test PRIVATE opentony_runtime)
    target_compile_features(opentony_action_profile_test PRIVATE cxx_std_20)
    target_compile_options(opentony_action_profile_test PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME opentony_action_profile_test COMMAND opentony_action_profile_test)

    add_executable(opentony_input_bindings_test
        src/runtime/input_bindings_test.cpp
    )
    target_link_libraries(opentony_input_bindings_test PRIVATE opentony_runtime)
    target_compile_features(opentony_input_bindings_test PRIVATE cxx_std_20)
    target_compile_options(opentony_input_bindings_test PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME opentony_input_bindings_test COMMAND opentony_input_bindings_test)

    add_executable(opentony_gameplay_frame_test
        src/runtime/gameplay_frame_test.cpp
    )
    target_link_libraries(opentony_gameplay_frame_test PRIVATE opentony_runtime)
    target_compile_features(opentony_gameplay_frame_test PRIVATE cxx_std_20)
    target_compile_options(opentony_gameplay_frame_test PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME opentony_gameplay_frame_test COMMAND opentony_gameplay_frame_test)

    add_executable(opentony_fixed_step_driver_test
        src/runtime/fixed_step_driver_test.cpp
    )
    target_link_libraries(opentony_fixed_step_driver_test PRIVATE opentony_runtime)
    target_compile_features(opentony_fixed_step_driver_test PRIVATE cxx_std_20)
    target_compile_options(opentony_fixed_step_driver_test PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME opentony_fixed_step_driver_test COMMAND opentony_fixed_step_driver_test)

    add_executable(opentony_gameplay_session_test
        src/runtime/gameplay_session_test.cpp
    )
    target_link_libraries(opentony_gameplay_session_test PRIVATE opentony_runtime)
    target_compile_features(opentony_gameplay_session_test PRIVATE cxx_std_20)
    target_compile_options(opentony_gameplay_session_test PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME opentony_gameplay_session_test COMMAND opentony_gameplay_session_test)

    add_executable(opentony_ground_brake_test
        src/runtime/ground_brake_test.cpp
    )
    target_link_libraries(opentony_ground_brake_test PRIVATE opentony_runtime)
    target_compile_features(opentony_ground_brake_test PRIVATE cxx_std_20)
    target_compile_options(opentony_ground_brake_test PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME opentony_ground_brake_test COMMAND opentony_ground_brake_test)

    add_executable(opentony_ground_physics_test
        src/runtime/ground_physics_test.cpp
    )
    target_link_libraries(opentony_ground_physics_test PRIVATE opentony_runtime)
    target_compile_features(opentony_ground_physics_test PRIVATE cxx_std_20)
    target_compile_options(opentony_ground_physics_test PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME opentony_ground_physics_test COMMAND opentony_ground_physics_test)

    add_executable(opentony_position_commit_test
        src/runtime/position_commit_test.cpp
    )
    target_link_libraries(opentony_position_commit_test PRIVATE opentony_runtime)
    target_compile_features(opentony_position_commit_test PRIVATE cxx_std_20)
    target_compile_options(opentony_position_commit_test PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME opentony_position_commit_test COMMAND opentony_position_commit_test)

    add_executable(opentony_player_state_test
        src/runtime/player_state_test.cpp
    )
    target_link_libraries(opentony_player_state_test PRIVATE opentony_runtime)
    target_compile_features(opentony_player_state_test PRIVATE cxx_std_20)
    target_compile_options(opentony_player_state_test PRIVATE -Wall -Wextra -Wpedantic)

    add_executable(opentony_action_commands_test
        src/runtime/action_commands_test.cpp
    )
    target_link_libraries(opentony_action_commands_test PRIVATE opentony_runtime)
    target_compile_features(opentony_action_commands_test PRIVATE cxx_std_20)
    target_compile_options(opentony_action_commands_test PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME opentony_action_commands_test COMMAND opentony_action_commands_test)
    add_test(NAME opentony_player_state_test COMMAND opentony_player_state_test)

    add_executable(opentony_action_sequence_test
        src/runtime/action_sequence_test.cpp
    )
    target_link_libraries(opentony_action_sequence_test PRIVATE opentony_runtime)
    target_compile_features(opentony_action_sequence_test PRIVATE cxx_std_20)
    target_compile_options(opentony_action_sequence_test PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME opentony_action_sequence_test COMMAND opentony_action_sequence_test)

    add_executable(opentony_action_sequence_builder_test
        src/runtime/action_sequence_builder_test.cpp
    )
    target_link_libraries(opentony_action_sequence_builder_test PRIVATE opentony_runtime)
    target_compile_features(opentony_action_sequence_builder_test PRIVATE cxx_std_20)
    target_compile_options(opentony_action_sequence_builder_test PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME opentony_action_sequence_builder_test COMMAND opentony_action_sequence_builder_test)

    add_executable(opentony_ollie_test
        src/runtime/ollie_test.cpp
    )
    target_link_libraries(opentony_ollie_test PRIVATE opentony_runtime)
    target_compile_features(opentony_ollie_test PRIVATE cxx_std_20)
    target_compile_options(opentony_ollie_test PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME opentony_ollie_test COMMAND opentony_ollie_test)

    add_executable(opentony_velocity_damping_test
        src/runtime/velocity_damping_test.cpp
    )
    target_link_libraries(opentony_velocity_damping_test PRIVATE opentony_runtime)
    target_compile_features(opentony_velocity_damping_test PRIVATE cxx_std_20)
    target_compile_options(opentony_velocity_damping_test PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME opentony_velocity_damping_test COMMAND opentony_velocity_damping_test)

    add_executable(opentony_velocity_projection_test
        src/runtime/velocity_projection_test.cpp
    )
    target_link_libraries(opentony_velocity_projection_test PRIVATE opentony_runtime)
    target_compile_features(opentony_velocity_projection_test PRIVATE cxx_std_20)
    target_compile_options(opentony_velocity_projection_test PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME opentony_velocity_projection_test COMMAND opentony_velocity_projection_test)

    add_executable(opentony_air_motion_test
        src/runtime/air_motion_test.cpp
    )
    target_link_libraries(opentony_air_motion_test PRIVATE opentony_runtime)
    target_compile_features(opentony_air_motion_test PRIVATE cxx_std_20)
    target_compile_options(opentony_air_motion_test PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME opentony_air_motion_test COMMAND opentony_air_motion_test)

    add_executable(opentony_ground_turn_test
        src/runtime/ground_turn_test.cpp
    )
    target_link_libraries(opentony_ground_turn_test PRIVATE opentony_runtime)
    target_compile_features(opentony_ground_turn_test PRIVATE cxx_std_20)
    target_compile_options(opentony_ground_turn_test PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME opentony_ground_turn_test COMMAND opentony_ground_turn_test)

    add_executable(opentony_ground_motion_test
        src/runtime/ground_motion_test.cpp
    )
    target_link_libraries(opentony_ground_motion_test PRIVATE opentony_runtime)
    target_compile_features(opentony_ground_motion_test PRIVATE cxx_std_20)
    target_compile_options(opentony_ground_motion_test PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME opentony_ground_motion_test COMMAND opentony_ground_motion_test)

    add_executable(opentony_ground_animation_test
        src/runtime/ground_animation_test.cpp
    )
    target_link_libraries(opentony_ground_animation_test PRIVATE opentony_runtime)
    target_compile_features(opentony_ground_animation_test PRIVATE cxx_std_20)
    target_compile_options(opentony_ground_animation_test PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME opentony_ground_animation_test COMMAND opentony_ground_animation_test)

    add_executable(opentony_ground_motion_threshold_test
        src/runtime/ground_motion_threshold_test.cpp
    )
    target_link_libraries(opentony_ground_motion_threshold_test PRIVATE opentony_runtime)
    target_compile_features(opentony_ground_motion_threshold_test PRIVATE cxx_std_20)
    target_compile_options(opentony_ground_motion_threshold_test PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME opentony_ground_motion_threshold_test COMMAND opentony_ground_motion_threshold_test)

    add_executable(opentony_fixed_matrix_test
        src/runtime/fixed_matrix_test.cpp
    )
    target_link_libraries(opentony_fixed_matrix_test PRIVATE opentony_runtime)
    target_compile_features(opentony_fixed_matrix_test PRIVATE cxx_std_20)
    target_compile_options(opentony_fixed_matrix_test PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME opentony_fixed_matrix_test COMMAND opentony_fixed_matrix_test)

    add_executable(opentony_physics_dispatch_test
        src/runtime/physics_dispatch_test.cpp
    )
    target_link_libraries(opentony_physics_dispatch_test PRIVATE opentony_runtime)
    target_compile_features(opentony_physics_dispatch_test PRIVATE cxx_std_20)
    target_compile_options(opentony_physics_dispatch_test PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME opentony_physics_dispatch_test COMMAND opentony_physics_dispatch_test)

    add_executable(opentony_physics_frame_test
        src/runtime/physics_frame_test.cpp
    )
    target_link_libraries(opentony_physics_frame_test PRIVATE opentony_runtime)
    target_compile_features(opentony_physics_frame_test PRIVATE cxx_std_20)
    target_compile_options(opentony_physics_frame_test PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME opentony_physics_frame_test COMMAND opentony_physics_frame_test)

    add_executable(opentony_psx_collision_probe_test
        src/runtime/psx_collision_probe_test.cpp
    )
    target_link_libraries(opentony_psx_collision_probe_test PRIVATE opentony_runtime)
    target_compile_features(opentony_psx_collision_probe_test PRIVATE cxx_std_20)
    target_compile_options(opentony_psx_collision_probe_test PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME opentony_psx_collision_probe_test COMMAND opentony_psx_collision_probe_test)

    add_executable(opentony_psx_asset_test
        src/assets/psx_asset_test.cpp
    )
    target_link_libraries(opentony_psx_asset_test PRIVATE opentony_assets)
    target_compile_features(opentony_psx_asset_test PRIVATE cxx_std_20)
    target_compile_options(opentony_psx_asset_test PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME opentony_psx_asset_test COMMAND opentony_psx_asset_test)

    add_executable(opentony_psx_texture_test
        src/assets/psx_texture_test.cpp
    )
    target_link_libraries(opentony_psx_texture_test PRIVATE opentony_assets)
    target_compile_features(opentony_psx_texture_test PRIVATE cxx_std_20)
    target_compile_options(opentony_psx_texture_test PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME opentony_psx_texture_test COMMAND opentony_psx_texture_test)

    add_executable(opentony_pre_asset_test
        src/assets/pre_asset_test.cpp
    )
    target_link_libraries(opentony_pre_asset_test PRIVATE opentony_assets)
    target_compile_features(opentony_pre_asset_test PRIVATE cxx_std_20)
    target_compile_options(opentony_pre_asset_test PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME opentony_pre_asset_test COMMAND opentony_pre_asset_test)

    add_executable(opentony_pre_catalog_test
        src/assets/pre_catalog_test.cpp
    )
    target_link_libraries(opentony_pre_catalog_test PRIVATE opentony_assets)
    target_compile_features(opentony_pre_catalog_test PRIVATE cxx_std_20)
    target_compile_options(opentony_pre_catalog_test PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME opentony_pre_catalog_test COMMAND opentony_pre_catalog_test)

    add_executable(opentony_psx_catalog_test
        src/assets/psx_catalog_test.cpp
    )
    target_link_libraries(opentony_psx_catalog_test PRIVATE opentony_assets)
    target_compile_features(opentony_psx_catalog_test PRIVATE cxx_std_20)
    target_compile_options(opentony_psx_catalog_test PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME opentony_psx_catalog_test COMMAND opentony_psx_catalog_test)

    add_executable(opentony_psx_collision_test
        src/assets/psx_collision_test.cpp
    )
    target_link_libraries(opentony_psx_collision_test PRIVATE opentony_assets)
    target_compile_features(opentony_psx_collision_test PRIVATE cxx_std_20)
    target_compile_options(opentony_psx_collision_test PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME opentony_psx_collision_test COMMAND opentony_psx_collision_test)

    add_executable(opentony_tricks_bin_inspect
        src/assets/tricks_bin_inspect.cpp
    )
    target_link_libraries(opentony_tricks_bin_inspect PRIVATE opentony_assets)
    target_compile_features(opentony_tricks_bin_inspect PRIVATE cxx_std_20)
    target_compile_options(opentony_tricks_bin_inspect PRIVATE -Wall -Wextra -Wpedantic)

    add_executable(opentony_tricks_bin_test
        src/assets/tricks_bin_test.cpp
    )
    target_link_libraries(opentony_tricks_bin_test PRIVATE opentony_assets)
    target_compile_features(opentony_tricks_bin_test PRIVATE cxx_std_20)
    target_compile_options(opentony_tricks_bin_test PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME opentony_tricks_bin_test COMMAND opentony_tricks_bin_test)
endif()

add_executable(opentony_trg_inspect
    src/trg/trg_inspect.cpp
)
target_link_libraries(opentony_trg_inspect PRIVATE opentony_trg)
target_compile_features(opentony_trg_inspect PRIVATE cxx_std_20)
target_compile_options(opentony_trg_inspect PRIVATE -Wall -Wextra -Wpedantic)

add_executable(opentony_trg_scene_inspect
    src/trg/trg_scene_inspect.cpp
)
target_link_libraries(opentony_trg_scene_inspect PRIVATE opentony_trg)
target_compile_features(opentony_trg_scene_inspect PRIVATE cxx_std_20)
target_compile_options(opentony_trg_scene_inspect PRIVATE -Wall -Wextra -Wpedantic)

add_executable(opentony_psx_inspect
    src/assets/psx_inspect.cpp
)
target_link_libraries(opentony_psx_inspect PRIVATE opentony_assets)
target_compile_features(opentony_psx_inspect PRIVATE cxx_std_20)
target_compile_options(opentony_psx_inspect PRIVATE -Wall -Wextra -Wpedantic)

add_executable(opentony_psx_catalog_inspect
    src/assets/psx_catalog_inspect.cpp
)
target_link_libraries(opentony_psx_catalog_inspect PRIVATE opentony_assets)
target_compile_features(opentony_psx_catalog_inspect PRIVATE cxx_std_20)
target_compile_options(opentony_psx_catalog_inspect PRIVATE -Wall -Wextra -Wpedantic)

add_executable(opentony_pre_inspect
    src/assets/pre_inspect.cpp
)
target_link_libraries(opentony_pre_inspect PRIVATE opentony_assets)
target_compile_features(opentony_pre_inspect PRIVATE cxx_std_20)
target_compile_options(opentony_pre_inspect PRIVATE -Wall -Wextra -Wpedantic)
