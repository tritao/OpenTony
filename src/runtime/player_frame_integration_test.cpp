#include "gameplay_frame.hpp"

#include "tests/test_check.hpp"

#include <filesystem>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

[[nodiscard]] std::string asset_path(const char* relative) {
    const std::filesystem::path root =
        "/home/joao/dev/OpenTony/build/assets/all-pkr/files/data";
    return (root / relative).string();
}

class Observer final : public opentony::runtime::LevelFrameObserver {
public:
    void on_input_frame(
        std::uint64_t,
        const opentony::runtime::InputState&) override {
        order.push_back("input-frame");
    }

    void on_level_tick(
        std::uint64_t,
        std::uint32_t,
        const opentony::runtime::InputState&,
        const opentony::trg::LevelRuntime&) override {
        order.push_back("level-tick");
    }

    std::vector<std::string> order;
};

} // namespace

int main() {
    using opentony::runtime::FixedPosition;
    using opentony::runtime::GroundAnimationRequest;
    using opentony::runtime::GroundAnimationRequestWrapper;
    using opentony::runtime::InputState;
    using opentony::runtime::MovementAction;
    using opentony::runtime::PhysicsDispatchStage;
    using opentony::runtime::PlayerPhysicsFrameHooks;
    using opentony::runtime::PlayerState;
    using opentony::runtime::PositionCollisionHit;
    using opentony::runtime::PositionCommitCandidate;
    using opentony::runtime::movement_bit;

    const std::string trg = asset_path("SKWARE_T.TRG");
    const std::string psx = asset_path("SKWARE.PSX");
    if (!std::filesystem::is_regular_file(trg)
        || !std::filesystem::is_regular_file(psx)) {
        std::cout << "fixture unavailable\n";
        return 0;
    }

    opentony::trg::LevelRuntime level(trg, psx);
    level.initialize();

    PlayerState player({0, 0x2000, 0});
    player.set_physics_state(0);
    player.set_collision_response({0x180, 0, 0x80});

    Observer observer;
    PlayerPhysicsFrameHooks hooks{};
    hooks.on_action_history = [&observer](
        const PlayerState& current_player,
        const opentony::runtime::ActionProfileState& profile) {
        observer.order.push_back("action-history");
        CHECK(current_player.action_history().write_index() ==
            (profile.selected_action == 4 ? 1U : 2U));
    };
    hooks.on_action_stream = [&observer](PlayerState&) {
        observer.order.push_back("action-stream");
    };
    hooks.on_stage = [&observer](
        PhysicsDispatchStage stage,
        PlayerState&,
        const InputState&) {
        switch (stage) {
        case PhysicsDispatchStage::GroundPreparation_9dad0:
            observer.order.push_back("stage-ground-preparation");
            break;
        case PhysicsDispatchStage::GroundCollision_96550:
            observer.order.push_back("stage-ground-collision");
            break;
        case PhysicsDispatchStage::GroundPost_95cc0:
            observer.order.push_back("stage-ground-post");
            break;
        case PhysicsDispatchStage::GroundFinal_9d9c0:
            observer.order.push_back("stage-ground-final");
            break;
        case PhysicsDispatchStage::InAir_97f40:
            observer.order.push_back("stage-in-air");
            break;
        default:
            observer.order.push_back("stage-other");
            break;
        }
    };

    bool landing_phase = false;
    std::vector<FixedPosition> query_starts;
    std::vector<FixedPosition> query_ends;
    hooks.collision_query = [
        &landing_phase,
        &query_starts,
        &query_ends,
        &observer](
        const FixedPosition& start,
        const FixedPosition& end) -> std::optional<PositionCollisionHit> {
        observer.order.push_back("collision-query");
        query_starts.push_back(start);
        query_ends.push_back(end);
        if (!landing_phase || end[1] >= 0) {
            return std::nullopt;
        }
        PositionCollisionHit hit{};
        hit.position = {0, 0, 0};
        hit.normal = {0, 0x1000, 0};
        hit.face_flags = 0x0080;
        hit.surface_flags = 0x0200;
        hit.raw_collision_word = 0x00000280;
        hit.surface_bit_7_clear = true;
        hit.surface_bit_8_clear = true;
        hit.raw_type_bits_9_12 = 2;
        return hit;
    };
    hooks.on_collision = [&observer](
        PlayerState&,
        const PositionCollisionHit& hit,
        const opentony::runtime::PositionCommitResult& commit) {
        observer.order.push_back("collision-consumer");
        CHECK(hit.normal == FixedPosition({0, 0x1000, 0}));
        CHECK(commit.selected_candidate == static_cast<std::uint8_t>(
            PositionCommitCandidate::OldY));
    };
    hooks.on_air_contact = [&observer](
        PlayerState&,
        const PositionCollisionHit& hit,
        const opentony::runtime::PositionCommitResult&) {
        observer.order.push_back("air-contact");
        return opentony::runtime::accepts_retail_ground_contact(hit);
    };
    hooks.landing_animation_request = [&observer](
        const PlayerState&,
        const PositionCollisionHit&) -> std::optional<GroundAnimationRequest> {
        observer.order.push_back("landing-animation");
        GroundAnimationRequest request{};
        request.issued = true;
        request.wrapper = GroundAnimationRequestWrapper::Range;
        request.animation = 5;
        request.start = 0;
        request.end = -1;
        request.alternate = -1;
        request.resets_rate = true;
        return request;
    };

    opentony::runtime::GameplayFrame frame(level, player);
    const auto grounded = frame.step(
        16,
        movement_bit(MovementAction::Right),
        hooks,
        &observer,
        0x100);
    CHECK(grounded.frame_index == 1);
    CHECK(grounded.input.action_mask() == movement_bit(MovementAction::Right));
    CHECK(grounded.input.pressed(movement_bit(MovementAction::Right)));
    CHECK(grounded.physics.action_profile.selected_action == 4);
    CHECK(grounded.physics.physics_state_before == 0);
    CHECK(grounded.physics.physics_state_after == 0);
    CHECK(!grounded.physics.state_request.changed);
    CHECK(grounded.physics.ground_turn.has_value());
    CHECK(grounded.physics.ground_turn->accumulator == 0x3c00);
    CHECK(grounded.physics.ground_animation.has_value());
    CHECK(grounded.physics.ground_animation->request.issued);
    CHECK(grounded.physics.ground_animation->request.animation == 7);
    CHECK(grounded.physics.ground_animation->request.start == 1);
    CHECK(grounded.physics.ground_animation->request.end == 1);
    CHECK(grounded.physics.position_commit.position
        == FixedPosition({0x180, 0x2000, 0x80}));
    CHECK(grounded.physics.position_commit.probes == 1);
    CHECK(grounded.physics.position_commit.selected_candidate ==
        static_cast<std::uint8_t>(PositionCommitCandidate::Desired));
    CHECK(!grounded.physics.collision_hit.has_value());
    const std::vector<FixedPosition> grounded_starts{
        FixedPosition{0, 0x2000, 0},
    };
    const std::vector<FixedPosition> grounded_ends{
        FixedPosition{0x180, 0x2000, 0x80},
    };
    CHECK(query_starts == grounded_starts);
    CHECK(query_ends == grounded_ends);
    CHECK(player.action_history().write_index() == 1);
    CHECK(player.action_history().record(0).action == 4);
    CHECK(player.action_history().record(0).pressed);
    const std::vector<std::string> grounded_order{
        "input-frame",
        "level-tick",
        "action-history",
        "action-stream",
        "stage-ground-preparation",
        "stage-ground-collision",
        "collision-query",
        "stage-ground-post",
        "stage-ground-final",
    };
    CHECK(observer.order == grounded_order);

    // The second frame starts from an in-air state and crosses a fixed floor.
    // Candidates 1-3 retain the below-floor Y and hit; candidate 4 retains
    // old Y and is accepted, matching the shared retail axis order.
    landing_phase = true;
    player.set_position({0, 0x1000, 0});
    player.set_physics_state(1);
    player.set_collision_response({0, -0x3000, 0});
    const auto landed = frame.step(
        16,
        0,
        hooks,
        &observer,
        0x100);
    CHECK(landed.frame_index == 2);
    CHECK(landed.physics.physics_state_before == 1);
    CHECK(landed.physics.physics_state_after == 0);
    CHECK(landed.physics.state_request.from == 1);
    CHECK(landed.physics.state_request.to == 0);
    CHECK(landed.physics.state_request.reason == 0x1fd6);
    CHECK(landed.physics.position_commit.position
        == FixedPosition({0, 0x1000, 0}));
    CHECK(landed.physics.position_commit.probes == 4);
    CHECK(landed.physics.position_commit.selected_candidate ==
        static_cast<std::uint8_t>(PositionCommitCandidate::OldY));
    CHECK(landed.physics.collision_hit.has_value());
    CHECK(landed.physics.landed);
    CHECK(landed.physics.landing_animation_request.has_value());
    CHECK(landed.physics.landing_animation_request->animation == 5);
    CHECK(landed.physics.landing_animation_request->start == 0);
    CHECK(landed.physics.landing_animation_request->end == -1);
    CHECK(landed.physics.landing_animation_request->alternate == -1);
    CHECK(query_starts.size() == 5);
    CHECK(query_ends[1] == FixedPosition({0, -0x2000, 0}));
    CHECK(query_ends[2] == FixedPosition({0, -0x2000, 0}));
    CHECK(query_ends[3] == FixedPosition({0, -0x2000, 0}));
    CHECK(query_ends[4] == FixedPosition({0, 0x1000, 0}));
    CHECK(player.action_history().write_index() == 2);
    CHECK(player.action_history().record(1).action == 4);
    CHECK(!player.action_history().record(1).pressed);
    const std::vector<std::string> complete_order{
        "input-frame",
        "level-tick",
        "action-history",
        "action-stream",
        "stage-ground-preparation",
        "stage-ground-collision",
        "collision-query",
        "stage-ground-post",
        "stage-ground-final",
        "input-frame",
        "level-tick",
        "action-history",
        "action-stream",
        "stage-in-air",
        "collision-query",
        "collision-query",
        "collision-query",
        "collision-query",
        "collision-consumer",
        "air-contact",
        "landing-animation",
    };
    CHECK(observer.order == complete_order);

    std::cout << "player frame integration ok\n";
}
