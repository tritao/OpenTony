#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace opentony::physics {

// Addresses are evidence labels, not pointers into the native recreation.
// Keeping them here makes trace/replay output directly comparable with the
// retail captures without pretending that the native process has this layout.
namespace retail {
constexpr std::uint32_t kMainPhysics = 0x0049e680;
constexpr std::uint32_t kPrePhysics = 0x0049a280;
constexpr std::uint32_t kGroundPreparation = 0x00492ea0;
constexpr std::uint32_t kActionHistoryUpdate = 0x00492190;
constexpr std::uint32_t kActionHistoryWriter = 0x00491c90;
constexpr std::uint32_t kActionHistoryRingBaseOffset = 0x2a14;
constexpr std::uint32_t kActionHistoryPreviousBaseOffset = 0x2b18;
constexpr std::uint32_t kActionHistoryWriteIndexOffset = 0x2b14;
constexpr std::size_t kActionHistoryCapacity = 0x20;
constexpr std::size_t kActionHistoryActionCount = 0x11;
constexpr std::uint32_t kGroundPhysics = 0x0049df00;
constexpr std::uint32_t kCollisionPreparation = 0x00490730;
constexpr std::uint32_t kDispatcher = 0x0049db80;
constexpr std::uint32_t kBlockedPhysicsReset = 0x0049d8a0;
constexpr std::uint32_t kStateRequest = 0x004900b0;
constexpr std::uint32_t kStateWriter = 0x004902bf;
// These are direct stores, not calls through 0x004900b0. They are retained
// as provenance labels so lifecycle/replay/network writes are not mistaken
// for ordinary gameplay transitions.
constexpr std::uint32_t kStateWriterSkaterInit = 0x0046c9cb;
constexpr std::uint32_t kStateWriterRestartReset = 0x0044f332;
constexpr std::uint32_t kStateWriterSpecialAction = 0x004beb1a;
constexpr std::uint32_t kStateWriterScriptReset = 0x004c6b90;
constexpr std::uint32_t kStateWriterReplayDecode = 0x004cd041;
constexpr std::uint32_t kStateWriterNetworkReset = 0x004df022;
constexpr std::uint32_t kStateWriterNetworkTimeout = 0x004e0c01;
constexpr std::uint32_t kStateWriterNetworkCleanup = 0x004e0cd1;
constexpr std::uint32_t kStateWriterNetworkFrameDecode = 0x004e107e;
constexpr std::uint32_t kInAirHandler = 0x00497f40;
constexpr std::uint32_t kState6PreAirSetup = 0x004993f0;
constexpr std::uint32_t kState6PreAirRequestCallsite = 0x00499445;
constexpr std::uint32_t kState6VelocityLength = 0x004ca8f0;
constexpr std::uint32_t kState6VelocityNegate = 0x004cad30;
constexpr std::uint32_t kState6VelocityScale = 0x004cac30;
constexpr std::uint32_t kAirCollisionResultCallsite = 0x00498a7d;
constexpr std::uint32_t kAirCollisionCastSetup = 0x004624d0;
constexpr std::uint32_t kAirCollisionQuery = 0x00466090;
constexpr std::uint32_t kAirCollisionMaterialDecode = 0x0048ea80;
constexpr std::uint32_t kAirPositionApplyCallsite = 0x004983c0;
constexpr std::uint32_t kJumpActionRecordPointerOffset = 0x2ccc;
constexpr std::uint32_t kFirstJumpActionRecord = 0x0056aff8;
constexpr std::uint32_t kSecondJumpActionRecord = 0x0056b164;
constexpr std::uint32_t kKickActionRecordOffset = 0x30;
constexpr std::uint32_t kKickActionBit = 0x0040;
// The action-record bank is laid out at player+0x2ccc in 0x10-byte slots.
// These masks and slot offsets come from the retail input update chain at
// 0x00489a10; keeping the whole bank here avoids treating directional input
// as an unrelated high-level boolean.
constexpr std::uint32_t kGrindActionRecordOffset = 0x10;
constexpr std::uint32_t kGrindActionBit = 0x0080;
constexpr std::uint32_t kGrabActionRecordOffset = 0x20;
constexpr std::uint32_t kGrabActionBit = 0x0020;
constexpr std::uint32_t kSpinLeftActionRecordOffset = 0x40;
constexpr std::uint32_t kSpinLeftActionBit = 0x0004;
constexpr std::uint32_t kNollieActionRecordOffset = 0x50;
constexpr std::uint32_t kNollieActionBit = 0x0001;
constexpr std::uint32_t kSpinRightActionRecordOffset = 0x60;
constexpr std::uint32_t kSpinRightActionBit = 0x0008;
constexpr std::uint32_t kSwitchActionRecordOffset = 0x70;
constexpr std::uint32_t kSwitchActionBit = 0x0002;
constexpr std::uint32_t kLeftActionRecordOffset = 0x80;
constexpr std::uint32_t kLeftActionBit = 0x8000;
constexpr std::uint32_t kRightActionRecordOffset = 0x90;
constexpr std::uint32_t kRightActionBit = 0x2000;
constexpr std::uint32_t kUpActionRecordOffset = 0xa0;
constexpr std::uint32_t kUpActionBit = 0x1000;
constexpr std::uint32_t kDownActionRecordOffset = 0xb0;
constexpr std::uint32_t kDownActionBit = 0x4000;
constexpr std::uint32_t kAirJumpHoldCheck = 0x00497fff;
constexpr std::uint32_t kAirJumpAccelerationWriter = 0x00498009;
constexpr std::uint32_t kAirJumpVelocityWriter = 0x0049800c;
constexpr std::uint32_t kAirJumpHoldCounterThreshold = 2;
constexpr std::uint32_t kInAirGravityAddCallsite = 0x004992f0;
constexpr std::uint32_t kVelocityIntegrationCallsite = 0x0049f206;
constexpr std::uint32_t kPositionCommit = 0x00496060;
constexpr std::uint32_t kGroundPositionCallsite = 0x0049f0e5;
constexpr std::uint32_t kGroundActionStep = 0x00493370;
constexpr std::uint32_t kGroundAccelerationResetZ = 0x0049e996;
constexpr std::uint32_t kGroundAccelerationResetY = 0x0049e999;
constexpr std::uint32_t kGroundAccelerationResetX = 0x0049e99e;
constexpr std::uint32_t kGroundSteeringHelper = 0x00492f20;
constexpr std::uint32_t kLandingRequestCallsite = 0x004991fe;
constexpr std::uint32_t kLandingCleanup = 0x004914d0;
constexpr std::uint32_t kLandingCollisionPreparation = 0x004aaf70;
constexpr std::uint32_t kLandingMovementTargetClear = 0x004991a4;
constexpr std::uint32_t kLandingAuxiliaryClear = 0x004991b3;
constexpr std::uint32_t kLandingEffectWriter = 0x004991d4;
constexpr std::uint32_t kLandingContactIdentityWriter = 0x004991f8;
constexpr std::uint32_t kLandingFrameWriter = 0x00499255;
constexpr std::uint32_t kAuxiliaryCase2Writer = 0x0049dea8;
constexpr std::uint32_t kAuxiliaryClearWriter = 0x0049dd21;
constexpr std::uint32_t kOllieLatchWriterGround = 0x0049a640;
constexpr std::uint32_t kOllieLatchWriterSpecial = 0x0049a6ac;
constexpr std::uint32_t kOllieMovementTargetClear = 0x0049a669;
constexpr std::uint32_t kOlliePendingWriter = 0x0049a6bf;
constexpr std::uint32_t kOllieLaunchLatchClear = 0x0049a751;
constexpr std::uint32_t kOlliePendingClear = 0x0049a6d2;
constexpr std::uint32_t kOllieStaleLatchClear = 0x0049a6ef;
constexpr std::uint32_t kOllieChargeWriter = 0x0049a5b5;
constexpr std::uint32_t kOllieChargeCapWriter = 0x0049a623;
constexpr std::uint32_t kOllieWallieChargeCapWriter = 0x0049a8d2;
constexpr std::uint32_t kOllieLaunchChargeSnapshot = 0x0049a74b;
constexpr std::uint32_t kOllieInProgressWriter = 0x0049a758;
constexpr std::uint32_t kOllieRecoveryActionContextClear = 0x0049a4b3;
constexpr std::uint32_t kOllieRecoveryInProgressClear = 0x0049a4b9;
constexpr std::uint32_t kOllieRecoveryModeLatchClear = 0x0049a4bf;
constexpr std::uint32_t kOllieRecoveryLatchClear = 0x0049a587;
constexpr std::uint32_t kOllieSpeedMetricWriter = 0x0049a777;
constexpr std::uint32_t kOllieModeLatchWriter = 0x0049a851;
constexpr std::uint32_t kOllieWallieFlagWriter = 0x0049a86a;
constexpr std::uint32_t kOllieLaunchCountWriter = 0x0049ac14;
constexpr std::uint32_t kOllieEarlyReleaseWriter = 0x0049ac4d;
constexpr std::uint32_t kOllieLaunchFrameWriter = 0x0049af14;
constexpr std::uint32_t kOllieChargeResetWriter = 0x0049af1a;
constexpr std::uint32_t kOllieTimestampSetAir = 0x0049f169;
constexpr std::uint32_t kOllieTimestampResetGround = 0x0049f173;
constexpr std::uint32_t kAlternateLaunchRequestCallsite = 0x0049ac7f;
constexpr std::uint32_t kOrdinaryLaunchRequestCallsite = 0x0049ac9b;
constexpr std::uint32_t kGroundLeaveAirRequestCallsite = 0x0049ddcf;
constexpr std::uint32_t kGroundLeaveAirOffGroundCallsite = 0x0049dde1;
constexpr std::uint32_t kOffGroundReset = 0x004904d0;
constexpr std::uint32_t kOffGroundMarkerWriter = 0x0049dde6;
constexpr std::int32_t kGroundLeaveAirSlopeThreshold = 1000;
constexpr std::int32_t kGroundLeaveAirRecoveryThreshold = 0x5000;
constexpr std::int32_t kGroundLeaveAirFrameWindowMultiplier = 6;
constexpr std::int32_t kGroundLeaveAirMarker = 0x28;
constexpr std::uint32_t kLandingRecoveryOffGroundCallsite = 0x004916a9;
constexpr std::uint32_t kAlternateStateTimeoutRequestCallsite = 0x0049de9e;
constexpr std::uint32_t kVelocityDamping = 0x0049d480;
constexpr std::uint32_t kOrientationBasisCopy = 0x0049c7d0;
constexpr std::uint32_t kInAirOrientationPreparation = 0x00497df0;
constexpr std::uint32_t kOrientationRecovery = 0x0049d080;
constexpr std::uint32_t kUprightCorrection = 0x0049c330;
constexpr std::uint32_t kFixed12Normalize = 0x00465f60;
constexpr std::uint32_t kFixed12Cross = 0x004e2ff0;
constexpr std::uint32_t kCrossScratchClamp = 0x004e2070;
constexpr std::uint32_t kOrientationPublish = 0x0049c850;
constexpr std::uint32_t kSurfaceVelocityProject = 0x00490680;
constexpr std::uint32_t kSurfacePlaneProject = 0x00490610;
constexpr std::uint32_t kGroundSurfaceAcceleration = 0x0049772b;
constexpr std::uint32_t kFixed12Dot = 0x004f5f90;
constexpr std::uint32_t kFixed12ScalarMultiply = 0x004f5fc0;
constexpr std::uint32_t kGravityInitialization = 0x0049e680;
constexpr std::uint32_t kFixedPointShift = 8;
constexpr std::uint32_t kJumpActionBit = 0x0010;
constexpr std::uint32_t kOrdinaryLaunchReason = 0x245c;
constexpr std::uint32_t kAlternateLaunchReason = 0x2457;
constexpr std::uint32_t kGroundLeaveAirReason = 0x2ba1;
constexpr std::uint32_t kAlternateStateTimeoutReason = 0x2bf2;
constexpr std::uint32_t kCollisionTransientEnterReason = 0x1ac9;
constexpr std::uint32_t kCollisionTransientExitReason = 0x1b19;
constexpr std::uint32_t kCollisionTransientEnterCallsite = 0x004972da;
constexpr std::uint32_t kCollisionTransientExitCallsite = 0x00497479;
// The independent frontend trace observes a collision path that promotes raw
// state 2 to raw state 4, then leaves state 4 for the common air state. These
// are request call instructions, not the post-call return PCs reported by the
// debugger (0x004913e2 and 0x004905b0 respectively).
constexpr std::uint32_t kState4EnterReason = 0x0b1c;
constexpr std::uint32_t kState4EnterRequestCallsite = 0x004913dd;
constexpr std::uint32_t kOffGroundReason = 0x0715;
constexpr std::uint32_t kOffGroundStateRequestCallsite = 0x004905ab;
constexpr std::uint32_t kState4ExitReason = kOffGroundReason;
constexpr std::uint32_t kState4ExitRequestCallsite =
    kOffGroundStateRequestCallsite;
constexpr std::uint32_t kLandingReason = 0x1fd6;
constexpr std::uint32_t kState6PreAirReason = 0x2058;
constexpr std::uint32_t kAirCollisionTransientRequestCallsite = 0x00497ad9;
constexpr std::uint32_t kAirCollisionRecoveryRequestCallsite = 0x00497aec;
constexpr std::uint32_t kAirCollisionTransientReason = 0x1caa;
constexpr std::uint32_t kAirCollisionRecoveryReason = 0x1cb1;

// The bundled PSX source-symbol data lists EPhysicsState in this order
// (physics.h lines 20-28). The PC dispatcher has the same eight-value switch
// shape and matching handler roles; these are semantic labels with
// cross-build evidence, not replacement values for the raw PC field.
constexpr std::int32_t kPhysicsOnGround = 0;
constexpr std::int32_t kPhysicsInAir = 1;
constexpr std::int32_t kPhysicsOnInvisible = 2;
constexpr std::int32_t kPhysicsInAirStickTo = 3;
constexpr std::int32_t kPhysicsOnRail = 4;
constexpr std::int32_t kPhysicsInWallride = 5;
constexpr std::int32_t kPhysicsInFootplant = 6;
constexpr std::int32_t kPhysicsStopped = 7;
constexpr std::int32_t kPhysicsInHandplant = 8;
}  // namespace retail

struct FixedVec3 {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;

    friend bool operator==(const FixedVec3& left, const FixedVec3& right) {
        return left.x == right.x && left.y == right.y && left.z == right.z;
    }
};

// FUN_004f5f90 and FUN_004f5fc0 use the x87 helper at 0x005004f4. The
// constant at 0x00519908 is 1/4096 and the helper converts with the retail
// truncate-toward-zero control word, not an arithmetic shift.
std::int32_t fixed12_dot(const FixedVec3& left, const FixedVec3& right) noexcept;
std::int32_t fixed12_scalar_multiply(std::int32_t scalar,
                                     std::int32_t value) noexcept;
FixedVec3 fixed12_scalar_multiply(std::int32_t scalar,
                                  const FixedVec3& value) noexcept;

// 0x00490610 receives the collision normal as two packed words rather than
// as a pointer to a three-component vector: x is word0.low16, y is
// word0.high16, and z is word1.low16, all sign-extended. The retail helper
// subtracts dot(v, n) * n in Q12 arithmetic and does not normalize n.
struct PackedSurfaceNormal {
    std::uint32_t xy = 0;
    std::uint32_t z = 0;

    friend bool operator==(const PackedSurfaceNormal& left,
                           const PackedSurfaceNormal& right) {
        return left.xy == right.xy && left.z == right.z;
    }
};

FixedVec3 decode_surface_normal(PackedSurfaceNormal packed) noexcept;
FixedVec3 project_vector_off_surface(const FixedVec3& velocity,
                                     PackedSurfaceNormal packed) noexcept;

// 0x00490680 projects velocity with 0x00490610 and then rescales the result
// by the pre/post integer speed-length ratio. The ratio uses
// (floor(sqrt(dot(v,v))) * 0x40) >> 8 and component-wise integer division,
// matching the retail helper's 0x004cac30/0x004cac90 sequence.
struct SurfaceVelocityProjectionResult {
    FixedVec3 velocity{};
    std::int32_t original_speed_metric = 0;
    std::int32_t projected_speed_metric = 0;
    bool speed_rescaled = false;
};

SurfaceVelocityProjectionResult project_velocity_preserving_speed(
    const FixedVec3& velocity, PackedSurfaceNormal packed) noexcept;

// FUN_0049c7d0 copies the object orientation shorts into the integer basis
// fields consumed by the ground and collision routines. The input order is
// the retail object layout +0x2e58, +0x2e5a, ..., +0x2e68.
struct OrientationBasis {
    FixedVec3 basis_30f4{};
    FixedVec3 basis_3100{};
    FixedVec3 basis_310c{};
};

OrientationBasis copy_orientation_basis(
    const std::array<std::int16_t, 9>& object_orientation) noexcept;

// FUN_00465f60 halves all components until they are inside the signed
// range [-0x6882, 0x6882], then divides each component by the integer
// Euclidean length and scales it to Q12. A zero vector falls back to +X.
FixedVec3 normalize_retail_fixed12(const FixedVec3& value) noexcept;

// The vector portion of FUN_004e2ff0. The retail caller supplies signed
// 16-bit globals and the helper performs each product/subtraction followed
// by an arithmetic Q12 shift. Its subsequent 0x004e2070 angle/clamp side
// effect is deliberately not hidden in this pure cross-product function.
FixedVec3 fixed12_cross(const FixedVec3& left,
                        const FixedVec3& right) noexcept;

// 0x004e2070 receives the raw cross-product result after 0x004e2ff0 and
// writes a separate signed-16-bit clamped scratch vector. The live raw
// cross-product slots are not modified by this side effect.
FixedVec3 clamp_cross_scratch_to_int16(const FixedVec3& value) noexcept;

struct GroundSurfaceAccelerationInput {
    FixedVec3 velocity{};
    FixedVec3 acceleration{};
    FixedVec3 basis_30f4{};
    FixedVec3 basis_3100{};
    std::int32_t frame_delta_fixed = 0; // DAT_0056865c
    bool surface_acceleration_enabled = false; // indexed DAT_0056a3d8
    bool speed_gate = false;                    // player +0x2d94
    std::int32_t contact_identity = 0;          // player +0x30b0
    bool special_mode = false;                  // player +0x2dd4
    std::int32_t special_coefficient = 0;       // player +0x2dfc
};

struct GroundSurfaceAccelerationResult {
    FixedVec3 velocity{};
    FixedVec3 acceleration{};
    bool velocity_projected = false;
};

// Recover the orientation-dependent tail of FUN_00496550. Collision selection
// and the packed-normal handoff are separate below; callers supply the
// already-published basis vectors and collision-owned flags here.
GroundSurfaceAccelerationResult apply_ground_surface_acceleration(
    const GroundSurfaceAccelerationInput& input) noexcept;

// The collision-result branch of 0x00496550 first projects this
// collision-owned reference vector with 0x00490610, adds the result to
// acceleration, publishes it at +0x3118 for non-state-2 paths, stores the
// packed normal words at +0x3128/+0x312c, and runs 0x00490680 on velocity.
// Collision detection supplies these inputs; this helper models only the
// deterministic handoff after a result has been selected.
struct GroundCollisionInput {
    FixedVec3 velocity{};
    FixedVec3 acceleration{};
    FixedVec3 reference_surface_vector{}; // state 2: player +0x2da8..+0x2db0
    PackedSurfaceNormal surface_normal{};
    bool collision_result = false;
    bool transient_state = false; // raw state 2 suppresses +0x3118 publication
};

struct GroundCollisionResult {
    FixedVec3 velocity{};
    FixedVec3 acceleration{};
    FixedVec3 projected_surface_vector{};
    PackedSurfaceNormal surface_normal{};
    SurfaceVelocityProjectionResult velocity_projection{};
    bool surface_vector_published = false;
};

GroundCollisionResult apply_ground_collision_handoff(
    const GroundCollisionInput& input) noexcept;

struct GravityInput {
    std::int16_t gravity_percent = 100; // player +0x29f4
    std::int32_t raw_state = 0;
    std::int32_t transient_random = 0; // FUN_0048f3a0(1), raw state 2 only
    bool level_variant_nine = false;    // DAT_0056a898 == 9
    bool modifier_c = false;            // FUN_00416980(0xc)
    bool modifier_e = false;            // FUN_00416980(0xe)
    bool global_half_modifier = false;  // DAT_0056b7ec != 0
};

// This is the frame-start +0x2dac initialization in FUN_0049e680. It is
// separate from PhysicsState so a replay can supply the retail global flags
// and random draw instead of silently inventing them.
std::int32_t compute_gravity_acceleration(const GravityInput& input) noexcept;

// FUN_0049d8a0 is called after the dispatcher on control-blocked frames. The
// decay divisor and the raw-state-2 exception are player/outer-loop inputs;
// keep them explicit rather than inventing a global timer or drag constant.
struct BlockedPhysicsInput {
    std::int32_t frame_delta_fixed = 0; // DAT_0056865c
    std::int32_t velocity_decay_divisor = 0; // player +0x2c10
    bool clamp_negative_vertical = false; // player +0x2f60 == 0 || raw state 2
};

struct BlockedPhysicsResult {
    bool handled = false;
    bool acceleration_xz_cleared = false;
    bool launch_bookkeeping_cleared = false;
    bool vertical_velocity_clamped = false;
    bool velocity_decayed = false;
};

struct VelocityDampingRandom {
    // Draw order in FUN_0049d480: initial cap, independent x/y/z cap-rescale
    // targets, then drag threshold. The three component draws are not aliases
    // of the initial cap draw; retail consumes a fresh type-3 value for each.
    std::int32_t cap = 0;
    std::int32_t cap_x = 0;
    std::int32_t cap_y = 0;
    std::int32_t cap_z = 0;
    std::int32_t threshold = 0;
};

struct VelocityDampingResult {
    FixedVec3 velocity{};
    std::int32_t initial_speed_metric = 0;
    std::int32_t final_speed_metric = 0;
    std::int32_t cap_threshold = 0;
    // Per-component Q12 targets after the retail cap expressions are shifted
    // by 12. These are useful when comparing a replay's independent draws.
    FixedVec3 cap_component_targets{};
    std::int32_t drag_threshold = 0;
    bool cap_applied = false;
    bool drag_applied = false;
    bool low_speed_damped = false;
};

// Deterministic portion of FUN_0049d480. The random draws are caller-owned
// because the retail stream is shared with animation/gameplay. The final
// low-speed damping is enabled only when the retail indexed player/global
// condition (+0x2cc4 -> DAT_0056a3d8) resolves to zero.
VelocityDampingResult apply_velocity_damping(
    const FixedVec3& velocity, std::int32_t frame_delta_fixed,
    const VelocityDampingRandom& random,
    bool apply_low_speed_damping = true) noexcept;

// The retail action record is updated by 0x00489930. edge_latch models its
// byte +1; press_edge is the one-update event exposed to a native consumer.
// The same layout is used by the JUMP record at +0x00 and the KICK record at
// +0x30. The latter is the direct charge/latch input observed in
// Skater_PrePhysicsOllie; it must not be silently substituted with JUMP.
struct ActionState {
    std::uint8_t held = 0;
    std::uint8_t edge_latch = 0;
    std::uint32_t held_counter = 0;
    std::uint32_t inactive_counter = 0;
    std::uint32_t update_counter = 0;
    bool press_edge = false;

    void update(bool pressed) noexcept;
    bool consume_press_edge() noexcept;
};

// Source compatibility for callers that specifically model the configured
// JUMP record. Both records share the recovered retail layout.
using JumpActionState = ActionState;

// FUN_00491c90 stores the action index, pressed byte, and frame timestamp in
// an eight-byte ring entry. The remaining bytes in the retail entry are not
// read by this boundary and therefore stay outside the native event shape.
struct ActionHistoryEvent {
    std::uint8_t action_index = 0;
    std::uint8_t pressed = 0;
    std::int32_t frame = 0;

    friend bool operator==(const ActionHistoryEvent& left,
                           const ActionHistoryEvent& right) {
        return left.action_index == right.action_index &&
               left.pressed == right.pressed && left.frame == right.frame;
    }
};

// FUN_004904d0 is shared by the ground leave-ground path, collision/rail
// exits, and some in-air recovery branches. FUN_004914d0 also consumes a few
// adjacent landing/recovery markers. These fields intentionally retain raw
// offsets where the current evidence does not establish a semantic name; the
// reset/cleanup itself is nevertheless exact and useful to a replay.
struct OffGroundBookkeeping {
    std::int16_t word_2c64 = 0;
    std::int32_t control_argument_2f60 = 0;
    std::int32_t word_2c8c = 0;
    std::uint32_t transition_count_302c = 0;
    std::int32_t word_29e0 = 0;
    std::int32_t word_2bd8 = 0;
    std::int32_t word_2ec8 = 0;
    std::int32_t word_2e90 = 0;
    std::int32_t word_2e94 = 0;
    std::int32_t word_2e34 = 0;
    std::int32_t word_2ec4 = 0;
    std::int32_t word_29d4 = 0;
    std::int32_t word_29d0 = 0;
    std::int16_t word_29f0 = 0;
    std::int16_t gravity_percent_29f4 = 100;
    std::int32_t word_2dd0 = 0;
    std::int32_t word_2c88 = 0;
    std::int32_t word_2c90 = 0;
    std::int32_t word_2c6c = 0;
    std::int32_t word_2c70 = 0;
    std::int16_t word_29f2 = 0;
    std::int32_t counter_3034 = 0;
    std::int32_t word_2e2c = 0;
    std::int8_t byte_100 = 0;
    std::int8_t byte_101 = 0;
    std::int16_t word_f4 = 0;
    std::int16_t word_82 = 0;
    std::int32_t word_3064 = 0;
    std::int32_t word_2f68 = 0;
    std::int32_t marker_3204 = 0;
};

// The non-collision action portion of Skater_DoPhysicsInAir (0x00497f40)
// adds/subtracts orientation axes from acceleration before the position
// integration. The caller supplies the already-published basis because the
// retail orientation preparation at 0x00497df0 has its own animation/global
// inputs. Values use the same Q12-ish integer storage as the player fields.
struct InAirActionControlInput {
    FixedVec3 velocity{};
    FixedVec3 acceleration{};
    FixedVec3 basis_30f4{};
    FixedVec3 basis_3100{};
    FixedVec3 basis_310c{};
    std::int32_t gravity_acceleration = 0; // player +0x2dac
    bool control_enabled = false; // DAT_0056b7f0
    bool kick_held = false;       // action record +0x30
    bool up_held = false;         // action record +0xa0
    bool down_held = false;       // action record +0xb0
    bool spin_left_held = false;  // action record +0x40
    bool spin_right_held = false; // action record +0x60
};

struct InAirActionControlResult {
    FixedVec3 acceleration{};
    bool applied = false;
};

// Recoverable from the first 0x00497f40 block. KICK adds basis_310c at
// gravity*180%, UP subtracts basis_30f4 at gravity*150%, DOWN adds basis_30f4
// in the same X/Y/Z order at gravity*150%, SPINLEFT adds basis_3100, and
// SPINRIGHT subtracts basis_3100. Each term is multiplied then shifted right
// by 12 exactly as the retail vector helpers do. The block then subtracts
// one thirty-second of velocity projected onto basis_3100 from acceleration.
InAirActionControlResult apply_in_air_action_control(
    const InAirActionControlInput& input) noexcept;

// The random values are supplied by the caller because the retail random
// stream is shared with animation/gameplay. Keeping them explicit makes the
// fixed-point formula and launch bookkeeping deterministic in replay tests
// without inventing a new random-number sequence for the native port.
struct OllieImpulseRandom {
    std::int32_t first = 0;
    std::int32_t second = 0;
    std::int32_t third = 0;
    std::int32_t fourth = 0;
    std::int32_t fifth = 0;
};

struct OllieImpulseInput {
    std::int32_t charge = 0;                 // player +0x2de8
    std::int32_t slope_metric = 0;           // player +0x3110
    std::int32_t horizontal_speed_metric = 0;  // player +0x2f30
    std::int32_t height_delta_metric = 0;    // (+0x2f48 - +0x2f4c) >> 12
    bool wallie = false;                     // raw state 5 adds -0xf000
    OllieImpulseRandom random{};
    OllieImpulseRandom early_release_random{};
};

struct OllieImpulseResult {
    std::int32_t delta_y = 0;
    std::int32_t adjusted_height_delta = 0;
    bool high_slope_branch = false;
    bool speed_adjustment_applied = false;
};

// Exact integer arithmetic recovered from 0x0049a280. This returns only the
// vertical impulse; collision, heading, and the shared retail RNG remain
// outside this helper.
OllieImpulseResult compute_ollie_vertical_impulse(const OllieImpulseInput& input) noexcept;

// The in-air handler's recoverable position step is:
//   velocity * dt + acceleration * (dt * dt) / 2
// with each product converted through the retail 8-bit fixed-point shift.
// dt is DAT_0056865c and is supplied by the caller because it is a runtime
// clock value, not a gameplay constant.
FixedVec3 compute_in_air_position_delta(const FixedVec3& velocity,
                                        const FixedVec3& acceleration,
                                        std::int32_t frame_delta_fixed) noexcept;

// The outer frame's final velocity update at 0x0049f206 adds acceleration*dt
// after dispatcher/contact processing and before postphysics damping.
FixedVec3 compute_velocity_delta(const FixedVec3& acceleration,
                                 std::int32_t frame_delta_fixed) noexcept;

struct PhysicsState {
    std::int32_t raw_state = 0;      // retail player +0x30b8
    std::int32_t phase_state = 0;    // retail player +0x30c0
    std::int32_t auxiliary = 0;      // retail player +0x30c4
    std::int32_t prephysics_blocked = 0; // retail player +0x2f64
    FixedVec3 position{};
    FixedVec3 old_position{};
    FixedVec3 position_history{};  // retail player +0xbc/+0xc0/+0xc4
    FixedVec3 older_position{};    // retail player +0x2e00/+0x2e04/+0x2e08
    FixedVec3 velocity{};
    FixedVec3 acceleration{};      // retail player +0x58/+0x5c/+0x60
    FixedVec3 basis_30f4{};         // retail player +0x30f4/+0x30f8/+0x30fc
    FixedVec3 basis_3100{};         // retail player +0x3100/+0x3104/+0x3108
    FixedVec3 basis_310c{};         // retail player +0x310c/+0x3110/+0x3114
    // Object orientation shorts published by FUN_0049c850. The array order
    // is +0x2e58, +0x2e5a, +0x2e5c, +0x2e5e, ..., +0x2e68, matching
    // copy_orientation_basis().
    std::array<std::int16_t, 9> orientation_shorts{};
    std::array<std::int16_t, 3> orientation_target_shorts{};
    FixedVec3 orientation_recovery_base{}; // retail +0x3134/+0x3138/+0x313c
    std::int32_t orientation_recovery_progress = 0; // retail +0x3130
    std::int32_t collision_recovery_frame = 0; // retail +0x2d8c
    std::int32_t gravity_acceleration = 0; // retail player +0x2dac
    bool air_control_enabled = false; // retail DAT_0056b7f0
    FixedVec3 contact_surface_vector{}; // retail player +0x3118/+0x311c/+0x3120
    PackedSurfaceNormal contact_surface_normal{}; // retail player +0x3128/+0x312c
    bool contact_surface_vector_valid = false;
    std::int32_t movement_target_x = 0; // retail player +0x3144
    std::int32_t movement_target_z = 0; // retail player +0x3148
    std::int32_t steering_active = 0;   // retail player +0x2e7c
    std::int32_t brake_mode = 0;        // retail player +0x2e78
    std::int8_t ground_speed_profile = 0; // retail player +0x29b7
    std::int8_t heading_input = 0;      // retail player +0x31a1
    std::int8_t heading_deadband = 0;   // retail player +0x31a2
    std::uint32_t action_mask = 0;
    JumpActionState jump{};
    ActionState kick{};            // action record at +0x30, mask bit 0x0040
    ActionState grind{};           // action record at +0x10, mask bit 0x0080
    ActionState grab{};            // action record at +0x20, mask bit 0x0020
    ActionState spin_left{};       // action record at +0x40, mask bit 0x0004
    ActionState nollie{};          // action record at +0x50, mask bit 0x0001
    ActionState spin_right{};      // action record at +0x60, mask bit 0x0008
    ActionState switch_stance{};   // action record at +0x70, mask bit 0x0002
    ActionState left{};            // action record at +0x80, mask bit 0x8000
    ActionState right{};           // action record at +0x90, mask bit 0x2000
    ActionState up{};              // action record at +0xa0, mask bit 0x1000
    ActionState down{};            // action record at +0xb0, mask bit 0x4000
    std::uint32_t frame_counter = 0;  // diagnostic mirror of DAT_005685f4
    // Previous pressed values used by FUN_00491c90's change filter. Indices
    // 0..0x10 correspond to the retail action-history pad-button numbers;
    // FUN_00492190 updates only the documented subset of those indices.
    std::array<std::uint8_t, retail::kActionHistoryActionCount>
        action_history_previous{};
    std::array<ActionHistoryEvent, retail::kActionHistoryCapacity>
        action_history_events{};
    std::uint32_t action_history_write_index = 0;
    OffGroundBookkeeping off_ground{};

    struct OllieBookkeeping {
        std::int32_t charge = 0;       // +0x2de8
        std::int32_t latched = 0;      // +0x2de0
        std::int32_t pending = 0;      // +0x2dd8; prephysics launch pending
        std::int32_t in_progress = 0;  // +0x2ddc
        std::int32_t mode = 0;         // +0x2db4
        std::int32_t mode_latched = 0; // +0x2db8
        std::int32_t launch_charge = 0;  // +0x2dec
        std::int32_t launch_frame = 0; // +0x2f34
        std::int32_t launch_count = 0; // +0x303c
        std::int32_t early_release_count = 0; // +0x3040
        std::int32_t alternate_state_frame = 0; // +0x2da0
        std::int32_t animation_gate = 0; // +0x2e30
        std::int32_t special_mode = 0; // +0x2dd4
        std::int32_t latch_timestamp = 0; // +0x2de4
        std::int32_t speed_metric = 0; // +0x2f30
        std::int32_t wallie = 0; // +0x2df4
        std::int32_t launch_auxiliary = 0; // +0x2c08
        std::int32_t action_context = 0; // +0x29c8; trick/action context
        std::int32_t recovery_latch = 0; // +0x2c68; cleared on ground re-entry
        std::int32_t launch_angle_accumulator = 0; // +0x3068
        std::int32_t launch_angle_turns = 0; // +0x306c
        std::int32_t landing_contact_auxiliary = 0; // +0x29dc
        std::int32_t landing_effect_state = 0; // +0x2ec0
        std::int32_t landing_contact_identity = 0; // +0x30b0; collision-owned
        std::int32_t landing_frame = 0; // +0x2d98
    } ollie{};
};

struct OllieChargeResult {
    std::int32_t charge = 0;
    std::int32_t cap = 0;
    bool capped = false;
    bool latched = false;
    bool pending = false;
};

struct StateRequest {
    std::int32_t from = 0;
    std::int32_t to = 0;
    std::uint32_t reason = 0;
    bool changed = false;
    std::uint32_t writer_pc = retail::kStateWriter;
    // Actual retail call instruction that invoked 0x004900b0. A zero value
    // means the caller did not attribute this synthetic/native request to a
    // recovered callsite.
    std::uint32_t request_callsite = 0;
};

struct OffGroundTransitionInput {
    std::int32_t mode = 0;              // first argument to 0x004904d0
    std::int32_t control_argument = 0;  // second argument -> +0x2f60
    std::uint32_t invocation_callsite = 0; // caller of 0x004904d0, if known
};

struct OffGroundTransitionResult {
    bool handled = false;
    bool bookkeeping_reset = false;
    std::int32_t mode = 0;
    std::uint32_t invocation_callsite = 0;
    StateRequest request{};
};

struct LandingCleanupResult {
    bool handled = false;
    bool collision_gate_cleared = false;
    bool animation_gate_cleared = false;
    bool timer_decremented = false;
    bool landing_marker_consumed = false;
    bool recovery_marker_cleared = false;
    bool recovery_reset_requested = false;
    bool trick_scan_requested = false;
    bool trick_dispatch_requested = false;
    bool trick_direction_hint = false;
    bool returned_early = false;
    OffGroundTransitionResult off_ground{};
};

// The post-helper case-0 tail in 0x0049db80 can leave a grounded player for
// raw state 1 without going through the ollie path. Collision/normal
// selection remains outside this boundary; callers supply the already
// published slope/recovery values and frame clock.
struct GroundAirTransitionInput {
    std::int32_t current_frame = 0; // DAT_005685f4
    std::int32_t frame_delta_fixed = 0; // DAT_0056865c
    std::int32_t slope_metric = 0; // player +0x3110
    std::int32_t recovery_progress = 0; // player +0x3130
    std::int32_t last_landing_frame = 0; // player +0x2d98
};

struct GroundAirTransitionResult {
    // Distinguishes a supplied input that failed the strict predicate from a
    // frame without a caller-owned producer observation.
    bool predicate_evaluated = false;
    bool eligible = false;
    bool transitioned = false;
    bool vertical_velocity_clamped = false;
    StateRequest request{};
    OffGroundTransitionResult off_ground{};
};

// Exact post-case-0 predicate at 0x0049dd6b-0x0049dd91. This is pure so a
// replay can exercise the strict boundaries without mutating the player. The
// caller still owns the raw-state gate and the producers of these values.
bool ground_leave_air_predicate(
    const GroundAirTransitionInput& input) noexcept;

struct OlliePrePhysicsInput {
    // The outer frame supplies these values. They are kept separate from the
    // action record because retail checks +0x2f64 before it checks KICK.
    bool prephysics_blocked = false;
    std::int32_t current_frame = -1;
    std::int32_t global_release_mode = 0; // DAT_00568658
    bool force_cap = false; // DAT_0056a890

    // The impulse input is populated at the release boundary. Its charge is
    // overwritten with the latched +0x2de8 value by the native operation.
    OllieImpulseInput impulse{};

    // Retail draws a fresh (type 2, type 0) pair when the held-charge cap is
    // recomputed at 0x0049a5f6. Supplying it separately prevents a replay from
    // accidentally reusing the launch impulse stream.
    OllieImpulseRandom charge_cap_random{};

    // If the first cap check takes the refresh branch, retail draws a second
    // pair immediately before the +0x2de8 cap write at 0x0049a623.
    OllieImpulseRandom charge_cap_refresh_random{};

    // The launch-count/early-release comparison has its own fresh pair after
    // the vertical impulse formula, at the sequence ending in 0x0049ac4d.
    OllieImpulseRandom early_release_random{};

    // Raw state 5 (wallie) replaces the held charge with a fresh cap pair at
    // 0x0049a8d2 immediately before the impulse formula runs.
    OllieImpulseRandom wallie_charge_random{};
};

enum class OlliePrePhysicsEvent {
    None,
    Charging,
    StaleLatchCleared,
    Cancelled,
    Launched,
};

struct OlliePrePhysicsResult {
    OlliePrePhysicsEvent event = OlliePrePhysicsEvent::None;
    std::int32_t charge = 0;
    std::int32_t cap = 0;
    bool capped = false;
    bool latch_set = false;
    bool pending_set = false;
    bool stale_latch_cleared = false;
    bool launch_consumed = false;
    bool recovery_cleared = false;
    bool state_requested = false;
    StateRequest request{};
};

// Dispatcher case 6 calls 0x004993f0 before falling through to the common
// in-air handler. The helper is deterministic from its five explicit random
// draws and the already-published velocity/orientation basis.
struct State6PreAirInput {
    std::int32_t charge_random_first = 0;  // FUN_0048f3a0(2)
    std::int32_t charge_random_second = 0; // FUN_0048f3a0(0)
    std::int32_t velocity_random_first = 0;  // FUN_0048f3a0(2)
    std::int32_t velocity_random_second = 0; // FUN_0048f3a0(2)
    std::int32_t velocity_random_third = 0;  // FUN_0048f3a0(2)
};

struct State6PreAirResult {
    bool handled = false;
    std::int32_t charge = 0; // player +0x2de8
    FixedVec3 velocity{};
    bool velocity_shaped = false;
    bool state_requested = false;
    StateRequest request{};
};

struct AirContactInput {
    bool accepted = false;
    FixedVec3 position{};
    PackedSurfaceNormal surface_normal{};
    bool surface_normal_valid = false;
    std::int32_t contact_identity = 0; // retail +0x30b0 publication
};

// FUN_00497aa0 runs after the in-air collision selector has chosen a result.
// The caller supplies the two horizontal collision-direction shorts from the
// selected result and the frame/material inputs used by its state gate.
struct AirCollisionRecoveryInput {
    std::int32_t current_frame = 0;
    std::int32_t recovery_frame = 0; // player +0x2d8c
    bool material_contact = false; // DAT_0056b7ac
    FixedVec3 collision_direction{}; // result +0x78/+0x7c; x and z are shorts
};

struct AirCollisionRecoveryResult {
    bool handled = false;
    bool transient_requested = false;
    bool recovery_requested = false;
    bool velocity_adjusted = false;
    bool in_progress_set = false;
    StateRequest request{};
};

struct AirOrientationPreparationResult {
    bool handled = false;
    bool rolling_axis_reset = false;
    OrientationBasis basis{};
};

// FUN_0049c330 keeps the board upright relative to the engine's global up
// vector. The normal is supplied by the caller because DAT_0056b7c0 is a
// shared renderer/physics value rather than a player-owned field.
struct UprightCorrectionResult {
    bool handled = false;
    bool rotation_applied = false;
    std::int32_t cross_dot = 0;
    std::int32_t rotation_angle = 0; // +0xb or the equivalent -0xb turn
    OrientationBasis basis{};
};

// FUN_0049d080 rebuilds the object basis while a collision/heading target is
// being approached. The target is stored as three signed shorts at +0x80,
// +0x82, +0x84; the base vector is +0x3134/+0x3138/+0x313c and the progress
// gate is +0x3130. The caller supplies those fields through PhysicsState
// because the collision/heading code that chooses them remains outside this
// model.
struct OrientationRecoveryResult {
    bool handled = false;
    bool skipped = false;
    FixedVec3 target{};
    OrientationBasis basis{};
};

// Raw result/material boundary observed at 0x00498a7d, immediately after
// 0x004624d0 -> 0x00466090 -> 0x0048ea80. The retail result is a nonzero
// collision-owned value (pointer-like in the Warehouse trace), not a C++
// boolean. Keep the material words separate because the handler's standard
// landing predicate tests them before the state request at 0x004991fe.
struct AirCollisionObservation {
    std::uint32_t result = 0; // EAX and the post-cleanup [ESP+0xf0] local
    std::uint32_t material_flags = 0; // DAT_0056b768
    std::uint32_t material_flags_secondary = 0; // DAT_0056b7a8
    std::uint32_t material_flags_contact = 0; // DAT_0056b7ac
    std::uint32_t material_flags_transient = 0; // DAT_0056b7b8
    std::uint32_t material_type = 0; // DAT_0056b7e8

    bool present() const noexcept { return result != 0; }
};

struct AirLandingPredicateInput {
    std::int32_t raw_state = 1;
    bool prephysics_blocked = false; // player +0x2f64
    bool jump_held = false; // **(player +0x2ccc)
    std::uint32_t jump_inactive_counter = 0; // action record +0x8
    std::int32_t current_frame = 0; // DAT_005685f4
    std::int32_t last_surface_frame = 0; // player +0x2f50
};

// Models only the standard landing branch after a collision result has been
// selected. It does not choose geometry, decode the cast payload, or model
// the wallride/special-contact alternatives that follow the false branch.
bool standard_air_landing_accepted(
    const AirCollisionObservation& observation,
    const AirLandingPredicateInput& input) noexcept;

struct GroundActionStepResult {
    bool grounded_path = false;
    bool target_changed = false;
    bool steering_active = false;
    bool brake_mode = false;
    std::int32_t target_step = 0;
    std::int32_t target_limit = 0;
};

// Semantic labels recovered by correlating the PC dispatcher with the
// original PSX EPhysicsState symbol ordering. Raw state values remain the
// authoritative replay key until the PC enum itself is independently
// symbolized.
enum class PhysicsStateSemantic {
    OnGround,
    InAir,
    OnInvisible,
    InAirStickTo,
    OnRail,
    InWallride,
    InFootplant,
    Stopped,
    InHandplant,
    Unknown,
};

PhysicsStateSemantic classify_physics_state(
    std::int32_t raw_state) noexcept;

enum class DispatchKind {
    Ground,
    InAir,
    CollisionTransient,
    State4,
    State5,
    State6PreAir,
    State7Ground,
    State8,
    Unknown,
};

struct DispatchResult {
    std::int32_t raw_state = 0;
    PhysicsStateSemantic semantic_state = PhysicsStateSemantic::OnGround;
    DispatchKind kind = DispatchKind::Unknown;
    std::array<std::uint32_t, 4> handler_pcs{};
    std::size_t handler_count = 0;
    std::uint32_t dispatcher_pc = retail::kDispatcher;
    // Populated by step_frame() after the case-0 handlers have returned. The
    // standalone dispatch() call has no caller-owned slope/recovery producer,
    // so it leaves this result at its default value.
    GroundAirTransitionResult ground_leave_air{};
};

using DispatchCallback = void (*)(PhysicsState&, const DispatchResult&, void*);
using LaunchCallback = void (*)(PhysicsState&, std::int32_t launch_state,
                                std::uint32_t reason, void*);
using PositionCommitCallback = void (*)(PhysicsState&, const FixedVec3& previous,
                                         const FixedVec3& next, void*);

class PhysicsStateMachine;
using FrameStageCallback = void (*)(PhysicsStateMachine&, void*);
using AirContactCallback = bool (*)(PhysicsStateMachine&, FixedVec3&, void*);
using State6PreAirCallback = void (*)(PhysicsStateMachine&, void*);
// The case-0 tail consumes values produced by collision/outer-frame code. The
// callback only publishes those values; the state machine owns the predicate,
// request, reset, and marker ordering after it returns true.
using GroundAirTransitionInputCallback = bool (*)(
    PhysicsStateMachine&, GroundAirTransitionInput&, void*);

struct PhysicsFrameCallbacks {
    // 0x0049e680 publishes +0x2dac before the action/ollie stage. The
    // callback owns the surrounding global flags and random draw and may
    // call initialize_gravity_acceleration().
    FrameStageCallback gravity_initialization = nullptr;
    // 0x00493370 consumes the action bank before the prephysics/ollie gate.
    // A callback may call apply_ground_action_step() here. That method owns
    // the recovered fixed-point target/clamp/decay/brake stage; the caller
    // still supplies the frame delta and any heading/animation side effects.
    FrameStageCallback movement_action_step = nullptr;
    FrameStageCallback prephysics = nullptr;
    // FUN_00492ea0 begins with 0x00492190 -> 0x00491c90 action-history
    // updates. The callback owns the unresolved FUN_00492120 result and may
    // call update_action_history() before the remaining ground helpers.
    FrameStageCallback action_history = nullptr;
    FrameStageCallback ground_preparation = nullptr;
    FrameStageCallback collision_preparation = nullptr;
    DispatchCallback dispatcher = nullptr;
    // 0x0049dd6b-0x0049dd91 runs after the four case-0 handlers and before the
    // outer post-dispatch work. The callback supplies the already-published
    // slope/recovery/frame values; returning false means that this frame has
    // no producer observation and the predicate is not evaluated.
    GroundAirTransitionInputCallback ground_leave_air_input = nullptr;
    // Dispatcher case 6 enters 0x004993f0 before falling through to the
    // common in-air handler. The callback supplies the five retail random
    // draws to run_state6_preair_setup() without inventing RNG ownership.
    State6PreAirCallback state6_preair_setup = nullptr;
    // 0x00497f40 enters 0x00497df0 before applying the recoverable action
    // acceleration terms. The callback publishes the basis/global inputs that
    // the native action-control block consumes.
    FrameStageCallback air_preparation = nullptr;
    FrameStageCallback air_motion = nullptr;
    // The common in-air handler invokes 0x0049c330 after its motion/effect
    // bookkeeping and before the collision query. The callback supplies the
    // shared global up vector and may call apply_upright_correction().
    FrameStageCallback upright_correction = nullptr;
    AirContactCallback air_contact = nullptr;
    // 0x0049d8a0 runs after the selected dispatcher handler and before the
    // outer-frame acceleration*dt update. The callback may call
    // apply_blocked_physics_reset() with the runtime decay inputs.
    FrameStageCallback blocked_physics_reset = nullptr;
    // 0x004914d0 runs after the post-dispatch position handoff and before
    // final velocity/postphysics work. Its 0x004aaf70 collision query is a
    // separate caller-owned callback immediately before this one. The
    // callback may call apply_landing_cleanup(); accepted air contact invokes
    // the same helper at its in-air landing branch.
    FrameStageCallback landing_collision_preparation = nullptr;
    FrameStageCallback landing_cleanup = nullptr;
    PositionCommitCallback position_commit = nullptr;
    // 0x0049e680 integrates acceleration into velocity at 0x0049f206 before
    // entering 0x0049d480. The callback supplies the runtime frame delta.
    FrameStageCallback velocity_integration = nullptr;
    FrameStageCallback postphysics = nullptr;
};

enum class LaunchPath {
    Ordinary,  // raw state 1, reason 0x245c
    Alternate, // raw state 3, reason 0x2457
};

class PhysicsStateMachine {
public:
    PhysicsStateMachine() = default;

    PhysicsState& state() noexcept { return state_; }
    const PhysicsState& state() const noexcept { return state_; }
    const std::vector<StateRequest>& requests() const noexcept { return requests_; }

    // Mirrors the action-mask-level observation after input polling. The
    // detailed keyboard/device path remains outside this subsystem.
    void update_action_mask(std::uint32_t action_mask) noexcept;

    // The retail helper writes raw_state and then copies the pre-request state
    // into +0x30c0. A request with the same value is retained as a write event
    // but is not a state transition.
    StateRequest request_state(std::int32_t next, std::uint32_t reason,
                               std::uint32_t request_callsite = 0);

    // Exact request boundaries recovered in FUN_00496550. These expose the
    // raw-0 -> raw-2 -> raw-0 collision-transient path while leaving collision
    // selection and response math to the caller.
    StateRequest enter_collision_transient();
    StateRequest exit_collision_transient();

    // Exact raw requests observed in the independent frontend path
    // 0 -> 2 -> 4 -> 1 -> 0. The surrounding collision/rail geometry and
    // state-4 handler remain caller-owned; these methods only preserve the
    // recovered request boundaries and metadata.
    StateRequest enter_state4_from_collision();
    StateRequest leave_state4_to_air();

    // Mirror the shared FUN_004904d0 off-ground reset. The mode argument is
    // retained for the external animation/speed side effects not modeled in
    // this slice; the deterministic player-field reset and its state-1
    // request are applied here.
    OffGroundTransitionResult apply_off_ground_transition(
        const OffGroundTransitionInput& input = {});

    // Mirror the deterministic cleanup and recovery-reset portion of
    // FUN_004914d0. Its animation/trick-recognition calls remain external,
    // but their exact branch conditions are reported in the result. Accepted
    // air contact invokes this helper at the retail point before the landing
    // identity and state request.
    LandingCleanupResult apply_landing_cleanup() noexcept;

    // 0x0049e680 writes +0x30c0 = +0x30b8 before entering 0x0049db80.
    void begin_dispatcher_phase() noexcept;

    // The main frame clears +0x58/+0x5c/+0x60 after 0x00493370 whenever the
    // current raw state is not 1. This is a deterministic frame boundary,
    // separate from gravity/collision callbacks.
    bool reset_non_air_acceleration() noexcept;

    // The main frame then rotates +0xbc history into +0x2e00 and publishes
    // the live position into +0xbc before 0x00490730 and the dispatcher.
    void begin_position_history() noexcept;

    // Mirror the frame-start maintenance of +0x2d8c. Retail arms this
    // recovery timestamp only while UP and directional steering are inactive
    // and the heading input is within its deadband.
    bool update_collision_recovery_window(std::int32_t current_frame) noexcept;

    // Publish the frame-start +0x2dac gravity value computed by
    // 0x0049e680. The global modifiers and raw-state-2 transient draw remain
    // explicit inputs because they are owned by the surrounding game loop.
    std::int32_t initialize_gravity_acceleration(
        const GravityInput& input) noexcept;

    // Apply the deterministic control-blocked post-dispatch reset at
    // 0x0049d8a0. It is a no-op unless prephysics_blocked is set.
    BlockedPhysicsResult apply_blocked_physics_reset(
        const BlockedPhysicsInput& input) noexcept;

    // Execute the recoverable outer frame contract. Callbacks own collision,
    // orientation-dependent grounded acceleration, and position math that
    // remains intentionally outside the recovered native boundaries.
    DispatchResult step_frame(std::uint32_t action_mask,
                              const PhysicsFrameCallbacks& callbacks = {},
                              void* user = nullptr);

    DispatchResult dispatch(DispatchCallback callback = nullptr,
                            void* user = nullptr);

    // The prephysics code applies the vertical impulse/bookkeeping first, then
    // requests raw state 1 or 3. The impulse itself stays a callback boundary.
    StateRequest begin_ollie(LaunchPath path, LaunchCallback callback = nullptr,
                             void* user = nullptr);

    // Mirror the case-0 post-helper leave-ground predicate at 0x0049ddcf.
    // It requests raw state 1 with reason 0x2ba1 and clamps descending Y
    // velocity; the collision/slope producer remains caller-owned.
    GroundAirTransitionResult try_ground_to_air(
        const GroundAirTransitionInput& input);

    // Apply the recovered +0x50 impulse and launch bookkeeping without
    // selecting raw state 1/3. This mirrors the ordering in which the retail
    // prephysics routine computes the impulse before calling 0x004900b0.
    OllieImpulseResult apply_ollie_impulse(const OllieImpulseInput& input);

    // Mirror the charge/latch half of 0x0049a280. `animation_eligible` is the
    // already-resolved animation/skater handoff condition; the KICK record is
    // checked here because retail reads +0x30, not the configured JUMP record
    // at +0x00. Keeping both gates explicit prevents a false direct
    // JUMP-to-ollie mapping in the native recreation.
    OllieChargeResult advance_ollie_charge(bool animation_eligible,
                                           std::int32_t first_random,
                                           std::int32_t second_random,
                                           bool force_cap = false) noexcept;

    // Execute the stateful KICK half of 0x0049a280. While KICK is held this
    // charges +0x2de8 and arms +0x2de0/+0x2dd8. On KICK release it clears the
    // pending flag, rejects stale/cancelled latches, or consumes the latch,
    // applies the impulse, and requests raw state 1 or 3. The configured
    // JUMP record is intentionally not consulted here.
    OlliePrePhysicsResult run_ollie_prephysics(
        const OlliePrePhysicsInput& input, LaunchCallback callback = nullptr,
        void* user = nullptr);

    // Apply dispatcher case 6's 0x004993f0 pre-air setup. This requests raw
    // state 1 with reason 0x2058, sets +0x2ddc, and applies the recovered
    // velocity shaping using state_'s published +0x30f4 basis.
    State6PreAirResult run_state6_preair_setup(
        const State6PreAirInput& input);

    // Apply the selected-result recovery branch at 0x00497aa0. This is
    // separate from collision selection and from the ordinary landing branch:
    // it can request raw state 2, reassert raw state 1, and apply the
    // recovered horizontal velocity correction.
    AirCollisionRecoveryResult handle_air_collision_recovery(
        const AirCollisionRecoveryInput& input);

    // Execute the deterministic basis portion of 0x00497df0 and publish the
    // resulting orientation shorts as 0x0049c850 does. The retail
    // 0x004e2070 angle/clamp helper invoked by each cross product remains an
    // explicit unresolved side effect; the fixed-point basis math itself is
    // reproduced here.
    AirOrientationPreparationResult prepare_in_air_orientation() noexcept;

    // Apply the deterministic upright correction at 0x0049c330. The retail
    // caller supplies the shared global up vector and invokes this only when
    // raw state != 2 and auxiliary == 0; both conditions remain explicit.
    UprightCorrectionResult apply_upright_correction(
        const FixedVec3& global_up) noexcept;

    // Apply the deterministic basis rebuild at 0x0049d080 using the
    // collision/heading target fields already present in state_. It updates
    // the published orientation shorts, basis fields, and recovery base;
    // target selection and the caller-owned 0x004e2070 scratch side effect
    // remain separate.
    OrientationRecoveryResult apply_orientation_recovery() noexcept;

    // Recover the grounded/raw-state-7 target update in 0x00493370. The
    // caller supplies DAT_0056865c because it is a runtime fixed-point frame
    // delta. This updates +0x3144/+0x3148, +0x2e7c, +0x2e78, and the grounded
    // velocity damping branch. Animation/heading side effects of 0x00492f20
    // remain outside this method; the target arithmetic is deterministic from
    // the action records and exposed raw fields.
    GroundActionStepResult apply_ground_action_step(
        std::int32_t frame_delta_fixed) noexcept;

    // Apply the recovered orientation-relative acceleration tail to the
    // machine's velocity/acceleration fields. The caller supplies collision
    // basis/flags because collision selection and orientation publication
    // remain outside this state machine.
    GroundSurfaceAccelerationResult apply_ground_surface_acceleration(
        const GroundSurfaceAccelerationInput& input) noexcept;

    // Apply the collision-result handoff in 0x00496550. This does not run
    // collision detection; it updates velocity/acceleration and contact
    // publication fields from an already selected packed-normal result.
    GroundCollisionResult apply_ground_collision_handoff(
        const GroundCollisionInput& input) noexcept;

    // 0x00497f40 checks the held byte and counter through player+0x2ccc
    // before integrating. When held for more than two updates it writes zero
    // to +0x5c (acceleration Y) and +0x50 (velocity Y).
    bool apply_in_air_jump_hold_effect() noexcept;

    // Apply the fixed-point position delta used by 0x00497f40 before its
    // collision tests. This is deliberately separate from commit_position:
    // retail writes the live position first, then uses 0x00496060 for the
    // collision-constrained commit path.
    FixedVec3 integrate_in_air_position(std::int32_t frame_delta_fixed) noexcept;

    // 0x004992f0 adds the frame-start gravity vector (+0x2da8/+0x2dac/
    // +0x2db0) to acceleration after the in-air motion/collision block. The
    // outer frame currently publishes only its nonzero Y component through
    // gravity_acceleration, matching the recovered initialization.
    FixedVec3 apply_in_air_gravity() noexcept;

    // Apply the recoverable action-to-acceleration portion at the start of
    // 0x00497f40. Collision selection and the orientation-preparation helper
    // remain caller-owned; publish the basis and gravity into state_ first.
    InAirActionControlResult apply_in_air_action_control() noexcept;

    // Apply the deterministic part of 0x0049d480 to the machine velocity.
    // Random draws and the indexed low-speed gate remain explicit inputs.
    VelocityDampingResult apply_postphysics_velocity_damping(
        std::int32_t frame_delta_fixed, const VelocityDampingRandom& random,
        bool apply_low_speed_damping = true) noexcept;

    // Apply the outer-frame acceleration*dt velocity update at 0x0049f206.
    FixedVec3 integrate_velocity(std::int32_t frame_delta_fixed) noexcept;

    // Mirror FUN_00492190 -> FUN_00491c90. `physics_action` is the opaque
    // result of FUN_00492120: values 1..8 select one of those transient
    // action-history indices. The remaining entries come from the recovered
    // action bank. Returns the number of changed events written to the ring.
    std::size_t update_action_history(
        std::int32_t physics_action, std::int32_t frame = -1) noexcept;

    // The in-air handler accepts collision/contact, commits contact position,
    // and only then requests raw state 0. Collision detection is deliberately
    // supplied by the caller.
    bool accept_air_contact(bool accepted, const FixedVec3& contact_position,
                            PositionCommitCallback callback = nullptr,
                            void* user = nullptr);

    // Extended landing boundary: after the exact position/state ordering, a
    // supplied landing normal drives the same 0x00490680 velocity projection
    // and contact-identity publication used by the retail air handler.
    bool accept_air_contact(const AirContactInput& input,
                            PositionCommitCallback callback = nullptr,
                            void* user = nullptr);

    // Compose the recovered standard collision predicate with the accepted
    // contact path. Collision detection still owns geometry and material
    // observation; this boundary owns the retail predicate -> commit -> state
    // request ordering for ordinary raw-state-1/3/6 contacts.
    bool accept_standard_air_collision(
        const AirCollisionObservation& observation,
        std::int32_t last_surface_frame, const AirContactInput& contact,
        PositionCommitCallback callback = nullptr, void* user = nullptr);

    void commit_position(const FixedVec3& next,
                         PositionCommitCallback callback = nullptr,
                         void* user = nullptr);

private:
    DispatchResult dispatch_impl(DispatchCallback callback, void* user,
                                 bool finalize_post_handler);
    void finalize_dispatch_post_handler(const DispatchResult& result) noexcept;

    PhysicsState state_{};
    std::vector<StateRequest> requests_{};
};

}  // namespace opentony::physics
