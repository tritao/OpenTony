#pragma once

#include "trg_runtime.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace opentony::trg {

struct CameraPointEntry {
    std::size_t registry_index{};
    std::size_t source_node{};
    CameraPointRecord record{};
};

// Value-only result of the skater pre-physics camera handoff.  The eventual
// camera object applies these fields to its +0x504/+0x3dc/+0x3e0/+0x3bc and
// +0x3c0 position fields; this layer deliberately does not invent a host
// pointer for the skater back-reference.
struct CameraPointHandoff {
    std::size_t registry_index{};
    std::size_t source_node{};
    std::array<std::int32_t, 3> position{};
    std::uint32_t camera_mode{};
    std::uint32_t camera_state_bit{};
    std::uint16_t transition_variant{};
    std::int32_t distance{};
};

class CameraPointRegistry final {
public:
    static constexpr std::size_t kMaximumEntries = 0x46;
    static constexpr std::int32_t kRetailDistanceLimit = 0x2329;

    // Type-13 registration is a compact u16 node-index table, not a heap
    // object list. The source file must outlive this registry only for the
    // build call; decoded point records are owned here afterward.
    void build(const TrgFile& file);

    using DistanceEvaluator = std::function<std::optional<std::int32_t>(
        const CameraPointEntry&)>;

    // The retail caller supplies the visibility/bounds metric. Keeping that
    // metric as an explicit callback preserves the proven strict `< 0x2329`
    // selection rule without guessing its unresolved geometric primitive.
    [[nodiscard]] std::optional<CameraPointHandoff> select_nearest(
        const DistanceEvaluator& distance,
        std::int32_t limit = kRetailDistanceLimit) const;

    [[nodiscard]] const std::vector<CameraPointEntry>& entries() const noexcept {
        return entries_;
    }
    [[nodiscard]] const CameraPointEntry& entry(std::size_t index) const;

private:
    std::vector<CameraPointEntry> entries_;
};

} // namespace opentony::trg
