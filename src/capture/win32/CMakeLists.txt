cmake_minimum_required(VERSION 3.20)

project(OpenTonyCaptureWin32 LANGUAGES C CXX)

if(NOT WIN32)
    message(STATUS "OpenTonyCaptureWin32 is Windows-only; no targets configured")
    return()
endif()
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "OpenTonyCaptureWin32 must be configured for a 32-bit/Win32 generator")
endif()

add_library(opentony_capture SHARED
    capture_dll.cpp
    hook_engine.cpp
    hooks.cpp
    shared_buffer.cpp
)
add_executable(opentony_capture_host capture_host.cpp)
set_target_properties(opentony_capture PROPERTIES PREFIX "")

# Keep the checked-in header usable by direct VC6 builds, while letting normal
# CMake builds regenerate it from the YAML manifest instead of maintaining a
# second hand-edited hook table.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    set(OTCAP_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
    set(OTCAP_GENERATED_HOOKS "${OTCAP_GENERATED_DIR}/capture_hooks_generated.h")
    add_custom_command(
        OUTPUT "${OTCAP_GENERATED_HOOKS}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${OTCAP_GENERATED_DIR}"
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/../../../scripts/generate_capture_hooks.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/../../../re/config/capture_hooks.yml"
            "${OTCAP_GENERATED_HOOKS}"
        DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/../../../scripts/generate_capture_hooks.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/../../../re/config/capture_hooks.yml"
        VERBATIM
    )
    add_custom_target(opentony_capture_manifest_codegen DEPENDS "${OTCAP_GENERATED_HOOKS}")
    add_dependencies(opentony_capture opentony_capture_manifest_codegen)
    target_compile_definitions(opentony_capture PRIVATE OTCAP_CAPTURE_GENERATED_INCLUDE=1)
endif()

foreach(target opentony_capture opentony_capture_host)
    target_compile_definitions(${target} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
        set_property(TARGET ${target} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    set_property(TARGET ${target} PROPERTY CXX_STANDARD 98)
    set_property(TARGET ${target} PROPERTY CXX_STANDARD_REQUIRED ON)
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    if(Python3_Interpreter_FOUND AND target STREQUAL "opentony_capture")
        target_include_directories(${target} PRIVATE "${OTCAP_GENERATED_DIR}")
    endif()
endforeach()
