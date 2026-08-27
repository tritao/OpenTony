#include "gameplay_session.hpp"

#include <array>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using opentony::runtime::FixedPosition;
using opentony::runtime::GameplaySession;
using opentony::runtime::GameplaySessionConfig;
using opentony::runtime::Q12Matrix3;

struct InitialState final {
    FixedPosition position{};
    FixedPosition previous_position{};
    FixedPosition response{};
    FixedPosition correction{};
    FixedPosition air_motion{};
    std::int32_t physics_state{};
    std::int32_t ground_update_state{};
    std::int32_t ground_physics_mode{};
    std::int32_t turn_accumulator{};
    Q12Matrix3 orientation{};
};

struct ReplayFrame final {
    std::uint64_t index{};
    std::uint16_t action_mask{};
    std::int8_t horizontal_axis{};
    std::int8_t vertical_axis{};
};

template <typename T>
T read_value(std::istringstream& input, const char* name) {
    long long value = 0;
    if (!(input >> value)) {
        throw std::runtime_error(std::string("missing ") + name);
    }
    return static_cast<T>(value);
}

FixedPosition read_position(std::istringstream& input, const char* name) {
    return FixedPosition{
        read_value<std::int32_t>(input, name),
        read_value<std::int32_t>(input, name),
        read_value<std::int32_t>(input, name),
    };
}

InitialState read_initial(std::istringstream& input) {
    InitialState state{};
    state.position = read_position(input, "initial position");
    state.previous_position = read_position(input, "initial previous position");
    state.response = read_position(input, "initial response");
    state.correction = read_position(input, "initial correction");
    state.air_motion = read_position(input, "initial air motion");
    state.physics_state = read_value<std::int32_t>(input, "initial physics state");
    state.ground_update_state = read_value<std::int32_t>(
        input, "initial ground update state");
    state.ground_physics_mode = read_value<std::int32_t>(
        input, "initial ground physics mode");
    state.turn_accumulator = read_value<std::int32_t>(
        input, "initial turn accumulator");
    for (std::int16_t& value : state.orientation.values) {
        value = read_value<std::int16_t>(input, "initial orientation");
    }
    return state;
}

ReplayFrame read_frame(std::istringstream& input) {
    ReplayFrame frame{};
    frame.index = read_value<std::uint64_t>(input, "frame index");
    frame.action_mask = read_value<std::uint16_t>(input, "action mask");
    frame.horizontal_axis = read_value<std::int8_t>(input, "horizontal axis");
    frame.vertical_axis = read_value<std::int8_t>(input, "vertical axis");
    return frame;
}

void write_position(std::ostream& output, const FixedPosition& position) {
    output << ' ' << position[0] << ' ' << position[1] << ' ' << position[2];
}

void write_orientation(std::ostream& output, const Q12Matrix3& orientation) {
    for (const std::int16_t value : orientation.values) {
        output << ' ' << value;
    }
}

void write_snapshot(
    std::ostream& output,
    const ReplayFrame& input,
    const GameplaySession& session) {
    const auto observation = session.observation();
    output << "frame " << input.index;
    write_position(output, observation.position);
    write_position(output, observation.previous_position);
    write_position(output, observation.collision_response);
    write_position(output, observation.motion_correction);
    write_position(output, observation.air_motion);
    output << ' ' << observation.physics_state
            << ' ' << observation.ground_update_state
            << ' ' << observation.ground_physics_mode
            << ' ' << session.player().turn_accumulator();
    write_orientation(output, observation.orientation);
    output << '\n';
}

void apply_initial_state(GameplaySession& session, const InitialState& state) {
    auto& player = session.player();
    player.set_position(state.position);
    player.set_previous_position(state.previous_position);
    player.set_collision_response(state.response);
    player.set_motion_correction(state.correction);
    player.set_air_motion(state.air_motion);
    player.set_physics_state(state.physics_state);
    player.set_ground_update_state(state.ground_update_state);
    player.set_ground_physics_mode(state.ground_physics_mode);
    player.set_turn_accumulator(state.turn_accumulator);
    player.set_orientation(state.orientation);
}

int run(int argc, char** argv) {
    if (argc != 4) {
        std::cerr << "usage: opentony_native_replay TRG PSX ASSET_ROOT\n";
        return 2;
    }
    const std::filesystem::path trg = argv[1];
    const std::filesystem::path psx = argv[2];
    const std::filesystem::path asset_root = argv[3];
    if (!std::filesystem::is_regular_file(trg)
        || !std::filesystem::is_regular_file(psx)) {
        std::cerr << "native replay level assets are missing\n";
        return 2;
    }

    InitialState initial{};
    std::vector<ReplayFrame> frames;
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) {
            continue;
        }
        std::istringstream input(line);
        std::string kind;
        input >> kind;
        if (kind == "init") {
            initial = read_initial(input);
        } else if (kind == "frame") {
            frames.push_back(read_frame(input));
        } else if (kind != "version" && kind != "end") {
            throw std::runtime_error("unknown native replay record: " + kind);
        }
    }
    if (frames.empty()) {
        throw std::runtime_error("native replay has no frames");
    }

    GameplaySessionConfig config{};
    config.fixed_step.simulation_step_ms = 16;
    config.fixed_step.frame_scale_q8 = 0x100;
    config.apply_air_gravity = true;
    config.classify_retail_air_contacts = true;
    GameplaySession session(
        trg.string(),
        psx.string(),
        asset_root.string(),
        opentony::runtime::PlayerState{},
        config);
    session.initialize();
    apply_initial_state(session, initial);
    session.reset_clock();

    std::cout << "native-replay-v1\n";
    for (const ReplayFrame& frame : frames) {
        static_cast<void>(session.advance(
            config.fixed_step.simulation_step_ms,
            frame.action_mask,
            frame.horizontal_axis,
            frame.vertical_axis));
        write_snapshot(std::cout, frame, session);
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    try {
        return run(argc, argv);
    } catch (const std::exception& error) {
        std::cerr << "native replay error: " << error.what() << '\n';
        return 2;
    }
}
