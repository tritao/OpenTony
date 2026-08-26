cmake_minimum_required(VERSION 3.20)

project(OpenTonyGroundMovement LANGUAGES CXX)

add_library(opentony_ground_movement ground_movement.cpp)
target_include_directories(opentony_ground_movement PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(opentony_ground_movement PUBLIC cxx_std_20)
target_compile_options(opentony_ground_movement PRIVATE -Wall -Wextra -Wpedantic)

include(CTest)
if(BUILD_TESTING)
    add_executable(opentony_ground_movement_test ground_movement_test.cpp)
    target_link_libraries(opentony_ground_movement_test PRIVATE opentony_ground_movement)
    target_compile_features(opentony_ground_movement_test PRIVATE cxx_std_20)
    target_compile_options(opentony_ground_movement_test PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME opentony_ground_movement_test COMMAND opentony_ground_movement_test)
endif()
