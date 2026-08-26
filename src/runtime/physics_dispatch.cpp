#include "physics_dispatch.hpp"

namespace opentony::runtime {
namespace {

void emit(
    PhysicsDispatchResult& result,
    PlayerState& player,
    const PhysicsDispatchHooks& hooks,
    PhysicsDispatchStage stage) {
    if (result.stage_count < result.stages.size()) {
        result.stages[result.stage_count++] = stage;
    }
    if (hooks.on_stage) {
        hooks.on_stage(stage, player);
    }
}

void emit_ground(
    PhysicsDispatchResult& result,
    PlayerState& player,
    const PhysicsDispatchHooks& hooks,
    bool restore_before_final) {
    emit(result, player, hooks, PhysicsDispatchStage::GroundPreparation_9dad0);
    emit(result, player, hooks, PhysicsDispatchStage::GroundCollision_96550);
    emit(result, player, hooks, PhysicsDispatchStage::GroundPost_95cc0);
    if (restore_before_final) {
        player.restore_previous_position();
        result.restored_previous_position = true;
        emit(result, player, hooks, PhysicsDispatchStage::RestorePreviousPosition);
    }
    emit(result, player, hooks, PhysicsDispatchStage::GroundFinal_9d9c0);
}

} // namespace

PhysicsDispatchResult PhysicsDispatcher::dispatch(
    PlayerState& player,
    const PhysicsDispatchHooks& hooks) {
    PhysicsDispatchResult result{};
    result.state = player.physics_state();
    switch (result.state) {
    case 0:
        player.set_ground_update_state(0);
        emit_ground(result, player, hooks, false);
        result.handled = true;
        break;
    case 1:
        emit(result, player, hooks, PhysicsDispatchStage::InAir_97f40);
        result.handled = true;
        break;
    case 2:
        player.set_ground_update_state(1);
        emit(result, player, hooks, PhysicsDispatchStage::GroundCollision_96550);
        result.handled = true;
        break;
    case 3:
        emit(result, player, hooks, PhysicsDispatchStage::InAir_97f40);
        result.handled = true;
        break;
    case 4:
        player.set_ground_update_state(0);
        emit(result, player, hooks, PhysicsDispatchStage::State4Routine_94210);
        result.handled = true;
        break;
    case 5:
        player.set_ground_update_state(0);
        emit(result, player, hooks, PhysicsDispatchStage::State5Routine_99710);
        result.handled = true;
        break;
    case 6:
        emit(result, player, hooks, PhysicsDispatchStage::State6Routine_993f0);
        emit(result, player, hooks, PhysicsDispatchStage::InAir_97f40);
        result.handled = true;
        break;
    case 7:
        emit_ground(result, player, hooks, true);
        result.handled = true;
        break;
    case 8:
        player.set_ground_update_state(0);
        emit(result, player, hooks, PhysicsDispatchStage::State8Routine_995d0);
        result.handled = true;
        break;
    default:
        break;
    }
    return result;
}

} // namespace opentony::runtime
