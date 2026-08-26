#pragma once

// Evidence-backed native model for the reviewed portion of Camera_Update.
//
// This is intentionally not a packed replacement for the retail CCamera
// object. The retail object contains several embedded helper objects and
// opaque state regions. The model exposes raw fixed-point values and keeps the
// dispatch entries address-based until mode semantics are proven.

#include "camera_math.hpp"

#include <array>
#include <cstdint>

namespace opentony::camera {

struct CameraUpdateState {
    // Retail +0x3ac/+0x3bc/+0x3e0 flags. These are byte flags in the binary;
    // the native model widens them only as a domain boundary convenience.
    bool anchor_update_flag{};
    bool tripod_anchor_flag{};
    bool secondary_target_valid{};

    Q16Vec3 mirrored_anchor{}; // retail +0x3b0
    Q16Vec3 anchor_target{};   // retail +0x3c0
    Q16Vec3 secondary_anchor{}; // retail +0x3e4
    Q16Vec3 look_target{};     // retail +0x3f0

    Raw viewport_parameter_raw{}; // retail +0x40c
    std::uint32_t mode{1};        // retail +0x504
    std::uint32_t update_tick{};  // retail +0x510
};

enum class CameraUpdateEntry : std::uint32_t {
    entry_0040feef = 0x0040feef,
    entry_0040ff2b = 0x0040ff2b,
    entry_00410006 = 0x00410006,
    entry_0041000f = 0x0041000f,
    entry_0041001b = 0x0041001b,
    entry_00410027 = 0x00410027,
    unknown_in_range = 0,
};

struct CameraDispatch {
    std::uint32_t mode{};
    CameraUpdateEntry entry{CameraUpdateEntry::entry_00410027};
    std::uint32_t callee{};
    bool bounds_check_passed{};
    bool table_byte_is_known{};
};

constexpr std::uint32_t kFirstCheckedMode = 1;
constexpr std::uint32_t kLastCheckedMode = 25;
constexpr std::uint32_t kDefaultEntry = 0x00410027;

// The retail dispatcher uses mode - 1 as an index, checks it against 0x18,
// then uses the byte table at 0x00410390 to select one of six entries at
// 0x00410378. The table contains default-entry bytes for modes 3..22 and
// distinct entries for modes 23..25. No semantic enum names are asserted.
inline CameraDispatch dispatch_camera_mode(std::uint32_t mode) {
    if (mode < kFirstCheckedMode || mode > kLastCheckedMode) {
        return {mode, CameraUpdateEntry::entry_00410027, kDefaultEntry, false, false};
    }

    switch (mode) {
    case 1:
        return {mode, CameraUpdateEntry::entry_0040ff2b, 0, true, true};
    case 2:
        return {mode, CameraUpdateEntry::entry_00410006, 0x004113f0, true, true};
    case 23:
        return {mode, CameraUpdateEntry::entry_0041000f, 0x00410f70, true, true};
    case 24:
        return {mode, CameraUpdateEntry::entry_0041001b, 0x00410c90, true, true};
    case 25:
        return {mode, CameraUpdateEntry::entry_0040feef, 0, true, true};
    default:
        return {mode, CameraUpdateEntry::entry_00410027, kDefaultEntry, true, true};
    }
}

// Camera_Update first saves the existing anchor/target values. In the
// observed tripod-copy branch it then replaces them with the linked object
// positions. The vector-helper branch is deliberately left to the future
// smoothing unit because its semantic inputs are not established here.
inline void copy_observed_tripod_targets(
    CameraUpdateState& camera,
    const Q16Vec3& primary_position,
    bool primary_link_present,
    const Q16Vec3& secondary_position,
    bool secondary_link_present) {
    if (camera.anchor_update_flag) {
        camera.mirrored_anchor = camera.anchor_target;
        if (camera.tripod_anchor_flag && primary_link_present) {
            camera.anchor_target = primary_position;
        }
    }

    if (camera.secondary_target_valid) {
        camera.secondary_anchor = camera.look_target;
        if (camera.tripod_anchor_flag && secondary_link_present) {
            camera.look_target = secondary_position;
        }
    }
}

// The retail write at 0x0040fda9 publishes only the low 16 bits of +0x40c to
// the active viewport record at +0x0e. Keep this as a raw handoff rather than
// assigning it an unproven projection/FOV meaning.
inline std::uint16_t viewport_parameter_low_word(const CameraUpdateState& camera) {
    return static_cast<std::uint16_t>(camera.viewport_parameter_raw);
}

// In the observed normal follow path +0x510 advances once per accepted
// update. Keeping the increment separate avoids implying that every mode
// reaches the same state transition.
inline void advance_observed_update_tick(CameraUpdateState& camera) {
    ++camera.update_tick;
}

} // namespace opentony::camera
