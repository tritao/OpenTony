#pragma once

#include <array>
#include <cstdint>
#include <functional>

namespace opentony::ground {

// The original player stores positions, velocities, and correction vectors as
// signed 32-bit fixed-point words.  The orientation matrix is stored as nine
// signed Q12 shorts, in row-major memory order at player+0x2e58.
struct Vec3i32 {
    std::int32_t x{};
    std::int32_t y{};
    std::int32_t z{};
};

constexpr bool operator==(const Vec3i32& lhs, const Vec3i32& rhs) noexcept {
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
}

constexpr bool operator!=(const Vec3i32& lhs, const Vec3i32& rhs) noexcept {
    return !(lhs == rhs);
}

struct MatrixQ12 {
    // words[row * 3 + column] corresponds to the packed short sequence
    // +0x2e58, +0x2e5a, ..., +0x2e68.
    std::array<std::int16_t, 9> words{};

    [[nodiscard]] static constexpr MatrixQ12 identity() noexcept {
        MatrixQ12 result{};
        result.at(0, 0) = 0x1000;
        result.at(1, 1) = 0x1000;
        result.at(2, 2) = 0x1000;
        return result;
    }

    [[nodiscard]] constexpr std::int16_t& at(std::size_t row, std::size_t column) noexcept {
        return words[row * 3 + column];
    }

    [[nodiscard]] constexpr const std::int16_t& at(std::size_t row,
                                                    std::size_t column) const noexcept {
        return words[row * 3 + column];
    }
};

struct BasisQ12 {
    // This is the mapping performed by 0x0049c7d0:
    // axis0 = matrix column 2, axis1 = column 0, axis2 = column 1.
    Vec3i32 axis0{};
    Vec3i32 axis1{};
    Vec3i32 axis2{};
};

struct TurnState {
    // player+0x3144 and its +0x3148 mirror.
    std::int32_t accumulator{};
    std::int32_t mirror{};
    bool active{};
};

struct TurnConfig {
    // player+0x29b7 selects the grounded turn constant.
    std::int8_t tuning{};
    // This is the already-resolved fast-turn branch (+0x2e78).
    bool fast_turn{};
};

struct GroundState {
    // player+0x08/+0x0c/+0x10.
    Vec3i32 position_q16{};
    // player+0x4c/+0x50/+0x54.
    Vec3i32 velocity_q16{};
    // player+0x2e58..+0x2e68 and player+0x30f4..+0x3114.
    MatrixQ12 orientation = MatrixQ12::identity();
    BasisQ12 basis{};
    TurnState turn{};
};

struct GroundFrameInput {
    // Controller +0x80 / +0x90 are represented by these global action bits.
    std::uint16_t action_mask{};
    // DAT_0056865c, Q8 frame scale.
    std::int32_t frame_scale_q8{0x100};
    TurnConfig turn_config{};

    // Output of the pre-dispatcher 0x0049b010 stage.  It is supplied by the
    // caller because its animation/surface gates are not one universal rule.
    // This value is player+0x58/+0x5c/+0x60.
    Vec3i32 prephysics_correction_q16{};

    // 0x00496360 calls 0x0049b500 with this phase enabled in ordinary state 0
    // (param_3 == !bVar1 == 1).  Keep it configurable for state-specific use.
    bool rotate_velocity{true};
    std::int16_t velocity_rotation_offset_q12{}; // param_4; normal ground path is zero.

    // The post-collision tail of 0x00496550 projects velocity against the
    // refreshed basis.  It is enabled only when the caller has recovered the
    // corresponding surface gate (+local_d0) and +0x2d94 condition.
    bool apply_ground_projection{};
    bool surface_gate{};
    bool suppress_forward_correction{}; // player+0x2d94 != 0
};

struct GroundFrameResult {
    MatrixQ12 old_orientation{};
    BasisQ12 old_basis{};
    Vec3i32 old_position_q16{};
    Vec3i32 integrated_position_q16{};
    Vec3i32 correction_after_projection_q16{};
    Vec3i32 candidate_before_commit_q16{};
    Vec3i32 committed_position_q16{};
    std::int32_t angle_q12{};
};

// A resolver models the collision/surface portion of 0x00496060.  The core
// supplies the candidate and the post-orientation state; the default resolver
// is the no-collision path.  A future collision implementation can preserve
// the same call boundary without changing the recovered movement arithmetic.
using CandidateResolver = std::function<Vec3i32(const GroundState&, const Vec3i32&)>;

inline constexpr std::uint16_t kLeftAction = 0x8000;
inline constexpr std::uint16_t kRightAction = 0x2000;

// Defined integer operations matching the relevant 32-bit x86 instructions.
[[nodiscard]] std::int32_t wrap32(std::int64_t value) noexcept;
[[nodiscard]] std::int16_t wrap16(std::int32_t value) noexcept;
[[nodiscard]] std::int32_t add32(std::int32_t lhs, std::int32_t rhs) noexcept;
[[nodiscard]] std::int32_t sub32(std::int32_t lhs, std::int32_t rhs) noexcept;
[[nodiscard]] std::int32_t mul_low32(std::int32_t lhs, std::int32_t rhs) noexcept;
[[nodiscard]] std::int32_t arithmetic_shift_right(std::int32_t value,
                                                    unsigned amount) noexcept;
[[nodiscard]] std::int32_t trunc_div32(std::int32_t numerator, std::int32_t denominator) noexcept;

[[nodiscard]] std::int32_t frame_scale_squared_q8(std::int32_t frame_scale_q8) noexcept;
[[nodiscard]] std::int32_t multiply_q12_trunc(std::int32_t lhs,
                                              std::int32_t rhs) noexcept;
[[nodiscard]] Vec3i32 integrate_position_q16(const Vec3i32& position_q16,
                                              const Vec3i32& velocity_q16,
                                              const Vec3i32& correction_q16,
                                              std::int32_t frame_scale_q8) noexcept;

// Implements the grounded Left/Right branch of 0x00493370.  The no-input
// branch is the low-lean decay path; the richer trick/lean steering branches
// are intentionally outside this Session-A core.
void update_grounded_turn(TurnState& state,
                          std::uint16_t action_mask,
                          const TurnConfig& config,
                          std::int32_t frame_scale_q8) noexcept;

[[nodiscard]] std::int32_t turn_angle_q12(std::int32_t turn_accumulator,
                                          std::int32_t frame_scale_q8) noexcept;

[[nodiscard]] MatrixQ12 multiply_q12(const MatrixQ12& lhs, const MatrixQ12& rhs) noexcept;
[[nodiscard]] MatrixQ12 transpose(const MatrixQ12& matrix) noexcept;
[[nodiscard]] MatrixQ12 rotation_y_q12(std::int32_t angle_q12) noexcept;
[[nodiscard]] BasisQ12 basis_from_matrix(const MatrixQ12& matrix) noexcept;
[[nodiscard]] Vec3i32 transform_q12(const MatrixQ12& matrix, const Vec3i32& vector_q16) noexcept;

// Models the ordinary state-0 tail of 0x00496550.  It removes velocity's
// lateral component along basis_1, then optionally feeds the forward
// component along basis_0 back into player+0x58/+0x5c/+0x60.
void project_ground_motion(Vec3i32& velocity_q16,
                           Vec3i32& correction_q16,
                           const BasisQ12& basis,
                           std::int32_t frame_scale_q8,
                           bool surface_gate,
                           bool suppress_forward_correction) noexcept;

// This is the param_3 != 0 phase of 0x0049b500.  It uses the saved pre-frame
// matrix (+0x2e38), applies the recovered fixed-point matrix chain to velocity,
// and preserves the original integer magnitude ratio when possible.
void rotate_velocity_for_ground_turn(Vec3i32& velocity_q16,
                                     const MatrixQ12& saved_old_orientation,
                                     std::int32_t angle_q12,
                                     std::int16_t offset_q12 = 0) noexcept;

// Executes the recovered ordinary grounded order:
// old basis copy -> supplied 0x0049b010 correction -> 0x004967b6 position add
// -> 0x00496360/0x0049b500 orientation -> basis refresh -> optional ground
// projection -> commit boundary.
[[nodiscard]] GroundFrameResult step_grounded(GroundState& state,
                                               const GroundFrameInput& input,
                                               CandidateResolver resolver = {});

} // namespace opentony::ground
