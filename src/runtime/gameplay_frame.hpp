#pragma once

#include "level_frame.hpp"
#include "input_bindings.hpp"
#include "physics_frame.hpp"
#include "../trg/level_trigger_state.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace opentony::runtime {

// Result of one renderer-independent gameplay iteration. The input snapshot
// is copied so replay and presentation code can retain the exact mask that
// reached both the level runtime and the player frame.
struct GameplayFrameResult {
    std::uint64_t frame_index{};
    std::uint32_t elapsed_ms{};
    std::int32_t frame_scale_q8{0x100};
    InputState input{};
    PlayerPhysicsFrameResult physics{};
    // Events appended by the level/script side during this fixed step. The
    // full level state remains authoritative; this bounded delta makes a
    // retail/native trace able to identify the gameplay effect at the frame
    // where it occurred.
    std::size_t trigger_event_count_before{};
    std::size_t trigger_event_count_after{};
    std::vector<opentony::trg::TriggerEvent> trigger_events{};
};

// Coordinates the recovered outer ordering: input history -> TRG/level tick
// -> player physics. Rendering, camera, audio, and device polling remain
// outside this class, while callers can still observe the two level-frame
// boundaries through LevelFrameObserver.
class GameplayFrame final {
public:
    GameplayFrame(
        opentony::trg::LevelRuntime& level,
        PlayerState& player) noexcept
        : scheduler_(level), player_(player) {}

    [[nodiscard]] GameplayFrameResult step(
        std::uint32_t elapsed_ms,
        std::uint16_t action_mask,
        const PlayerPhysicsFrameHooks& physics_hooks = {},
        LevelFrameObserver* observer = nullptr,
        std::int32_t frame_scale_q8 = 0x100);

    [[nodiscard]] GameplayFrameResult step(
        std::uint32_t elapsed_ms,
        const DirectInputKeyboardState& keyboard,
        const InputBindings& bindings,
        const PlayerPhysicsFrameHooks& physics_hooks = {},
        LevelFrameObserver* observer = nullptr,
        std::int32_t frame_scale_q8 = 0x100);

    [[nodiscard]] GameplayFrameResult step(
        std::uint32_t elapsed_ms,
        std::uint16_t action_mask,
        std::int8_t horizontal_axis,
        std::int8_t vertical_axis,
        const PlayerPhysicsFrameHooks& physics_hooks = {},
        LevelFrameObserver* observer = nullptr,
        std::int32_t frame_scale_q8 = 0x100);

    [[nodiscard]] std::uint64_t frame_index() const noexcept {
        return scheduler_.frame_index();
    }

private:
    LevelFrameScheduler scheduler_;
    PlayerState& player_;
};

} // namespace opentony::runtime
