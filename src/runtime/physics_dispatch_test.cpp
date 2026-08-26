#include "physics_dispatch.hpp"

#include <cassert>
#include <iostream>
#include <vector>

int main() {
    using opentony::runtime::PhysicsDispatchHooks;
    using opentony::runtime::PhysicsDispatchStage;
    using opentony::runtime::PhysicsDispatcher;
    using opentony::runtime::PlayerState;

    PlayerState player({100, 200, 300});
    std::vector<PhysicsDispatchStage> stages;
    const PhysicsDispatchHooks hooks{
        [&stages](PhysicsDispatchStage stage, PlayerState&) {
            stages.push_back(stage);
        },
    };

    player.set_physics_state(0);
    player.set_ground_update_state(9);
    auto result = PhysicsDispatcher::dispatch(player, hooks);
    assert(result.handled);
    assert(player.ground_update_state() == 0);
    assert(stages.size() == 4);
    assert(stages[0] == PhysicsDispatchStage::GroundPreparation_9dad0);
    assert(stages[1] == PhysicsDispatchStage::GroundCollision_96550);
    assert(stages[2] == PhysicsDispatchStage::GroundPost_95cc0);
    assert(stages[3] == PhysicsDispatchStage::GroundFinal_9d9c0);

    stages.clear();
    player.set_previous_position({10, 20, 30});
    player.set_position({40, 50, 60});
    player.set_physics_state(7);
    result = PhysicsDispatcher::dispatch(player, hooks);
    assert(result.restored_previous_position);
    assert(player.position() == opentony::runtime::FixedPosition({10, 20, 30}));
    assert(stages.size() == 5);
    assert(stages[3] == PhysicsDispatchStage::RestorePreviousPosition);
    assert(stages[4] == PhysicsDispatchStage::GroundFinal_9d9c0);

    stages.clear();
    player.set_physics_state(6);
    result = PhysicsDispatcher::dispatch(player, hooks);
    assert(result.handled);
    assert(stages.size() == 2);
    assert(stages[0] == PhysicsDispatchStage::State6Routine_993f0);
    assert(stages[1] == PhysicsDispatchStage::InAir_97f40);

    player.set_physics_state(99);
    result = PhysicsDispatcher::dispatch(player, hooks);
    assert(!result.handled);
    std::cout << "Physics dispatch tests passed\n";
}
