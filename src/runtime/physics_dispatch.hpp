#pragma once

#include "player_state.hpp"

#include <array>
#include <cstdint>
#include <functional>

namespace opentony::runtime {

// These are deliberately address-oriented until the retail state enum and
// every callee's gameplay meaning are confirmed. The ordering is recovered
// from Skater_PhysicsDispatcher at 0x0049db80.
enum class PhysicsDispatchStage : std::uint8_t {
    GroundPreparation_9dad0,
    GroundCollision_96550,
    GroundPost_95cc0,
    GroundFinal_9d9c0,
    InAir_97f40,
    State4Routine_94210,
    State5Routine_99710,
    State6Routine_993f0,
    State8Routine_995d0,
    RestorePreviousPosition,
};

struct PhysicsDispatchHooks {
    std::function<void(PhysicsDispatchStage, PlayerState&)> on_stage;
};

struct PhysicsDispatchResult {
    std::int32_t state{};
    bool handled{};
    bool restored_previous_position{};
    std::array<PhysicsDispatchStage, 5> stages{};
    std::uint8_t stage_count{};
};

// Renderer-independent control-flow shell for the confirmed state dispatcher.
// The callbacks are where the still-unrecovered ground/in-air producers plug
// in; this class owns the state-specific ordering and raw +0x30c4 writes.
class PhysicsDispatcher final {
public:
    [[nodiscard]] static PhysicsDispatchResult dispatch(
        PlayerState& player,
        const PhysicsDispatchHooks& hooks = {});
};

} // namespace opentony::runtime
