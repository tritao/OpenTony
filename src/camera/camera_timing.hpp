#pragma once

// Timing contracts adjacent to the camera reference model.  The retail
// level loop updates the public clock in 0x004f5ff0, while the render-prep
// helper 0x00468b30 derives the Q8 simulation rate consumed by the next
// camera update.  Keep these as integer state machines instead of hiding the
// one-frame producer/consumer delay behind a floating-point dt.

#include "src/camera/camera_math.hpp"

#include <array>
#include <cstdint>

namespace opentony::camera {

struct SimulationClockStateRaw {
    Raw public_tick{};       // DAT_0056e31c
    Raw simulation_time{};   // DAT_0056e320
    Raw last_timestamp_ms{}; // DAT_029d836c
    bool timestamp_initialized{};
};

struct SimulationClockStepRaw {
    Raw elapsed_ms{};
    Raw tick_delta{};
    bool advanced{};
};

// 0x004f5ff0 converts the millisecond source returned by 0x00502ccb into
// integer 60-Hz ticks.  The source is not sampled while 0x004dab30 reports a
// disabled timer; a positive delta advances the public tick even when the
// simulation-time accumulator is paused by either global guard.
inline SimulationClockStepRaw advance_simulation_clock_ms(
    SimulationClockStateRaw& state,
    Raw timestamp_ms,
    bool timer_disabled,
    bool simulation_paused) {
    if (timer_disabled) {
        return {};
    }

    if (!state.timestamp_initialized) {
        state.last_timestamp_ms = timestamp_ms;
        state.timestamp_initialized = true;
    }

    const Raw elapsed_ms = subtract_s32(
        timestamp_ms, state.last_timestamp_ms);
    state.last_timestamp_ms = timestamp_ms;

    // The PE32 sequence is signed IMUL by 60 followed by signed division by
    // 1000.  Runtime values are normally positive, but preserve the signed
    // operation for replay and wrap the product like x86.
    const Raw tick_delta = divide_toward_zero(
        multiply_s32(elapsed_ms, 0x3c), 1000);
    if (tick_delta <= 0) {
        return {elapsed_ms, tick_delta, false};
    }

    state.public_tick = add_s32(state.public_tick, tick_delta);
    if (!simulation_paused) {
        state.simulation_time = add_s32(state.simulation_time, tick_delta);
    }
    return {elapsed_ms, tick_delta, true};
}

struct CameraTimingStateRaw {
    Raw previous_simulation_time{}; // DAT_00568604
    std::array<Raw, 3> recent_deltas{}; // DAT_0056868c..+8
    std::uint32_t ring_index{}; // DAT_0056a934
    // The retail runtime seeds DAT_0056865c to one 60-Hz step before the
    // first render-preparation sample is available.  Keeping that seed here
    // matters because the timing producer is consumed by the next camera
    // update, not the update that produced it.
    Raw simulation_delta_q8{0x100}; // DAT_0056865c
    Raw simulation_delta_square_q8{}; // DAT_00568804
    Raw simulation_progress_q8{}; // DAT_00568810
    Raw progress_integer{}; // DAT_005685f4
    Raw delta_q11{}; // DAT_0056a93c
    Raw phase_half{}; // DAT_0056a940
    std::uint32_t phase_parity{}; // DAT_0056a944 low bit
    Raw inverse_rate{}; // DAT_00568808
};

struct CameraTimingStepRaw {
    Raw sample_delta{};
    Raw sample_sum{};
    Raw clamped_sample_sum{};
    bool updated{};
    bool quarter_rate_applied{};
    bool slow_rate_applied{};
};

// 0x00468b30.  This runs in render preparation after Camera_Update; its
// simulation_delta_q8 is consequently the smoothing input for the following
// camera update, not the current one.  The three-sample window, 15-tick cap,
// quarter-rate branch, and 125/100 slow-rate branch are all retained.
inline CameraTimingStepRaw advance_camera_timing(
    CameraTimingStateRaw& state,
    Raw simulation_time,
    bool timing_paused,
    bool quarter_rate,
    bool slow_rate,
    bool progress_paused) {
    const Raw sample_delta = subtract_s32(
        simulation_time, state.previous_simulation_time);
    if (!timing_paused) {
        state.recent_deltas[state.ring_index % state.recent_deltas.size()]
            = sample_delta;
        state.ring_index = (state.ring_index + 1) % state.recent_deltas.size();
    }

    state.delta_q11 = multiply_s32(sample_delta, 0x800);
    const Raw phase_sum = add_s32(
        static_cast<Raw>(state.phase_parity), sample_delta);
    state.phase_parity = static_cast<std::uint32_t>(phase_sum) & 1U;
    state.phase_half = divide_toward_zero(phase_sum, 2);
    state.previous_simulation_time = simulation_time;

    Raw sample_sum = 0;
    for (const Raw value : state.recent_deltas) {
        sample_sum = add_s32(sample_sum, value);
    }
    if (sample_sum == 0) {
        sample_sum = 1;
    }
    const Raw clamped_sample_sum = sample_sum > 0xf
        ? 0xf
        : sample_sum;
    state.inverse_rate = divide_toward_zero(0xb4, sample_sum);

    CameraTimingStepRaw result{
        sample_delta, sample_sum, clamped_sample_sum, false,
        quarter_rate, slow_rate};
    if (!timing_paused) {
        Raw delta_q8 = divide_toward_zero(
            static_cast<Raw>(clamped_sample_sum << 8), 6);
        if (quarter_rate) {
            delta_q8 = divide_toward_zero(delta_q8, 4);
        }
        if (slow_rate) {
            delta_q8 = divide_toward_zero(multiply_s32(delta_q8, 0x7d), 100);
        }
        state.simulation_delta_q8 = delta_q8;
        state.simulation_delta_square_q8 = arithmetic_shift_right(
            multiply_s32(delta_q8, delta_q8), 8);
        if (!progress_paused) {
            state.simulation_progress_q8 = add_s32(
                state.simulation_progress_q8,
                multiply_s32(delta_q8, 2));
        }
        state.progress_integer = arithmetic_shift_right(
            state.simulation_progress_q8, 8);
        result.updated = true;
    }
    return result;
}

} // namespace opentony::camera
