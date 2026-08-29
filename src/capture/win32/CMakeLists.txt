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
    hooks.cpp
    shared_buffer.cpp
)
add_executable(opentony_capture_host capture_host.cpp)
set_target_properties(opentony_capture PROPERTIES PREFIX "")

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
endforeach()
