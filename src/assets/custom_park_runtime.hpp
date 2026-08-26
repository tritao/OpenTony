#pragma once

#include "custom_park_asset.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace opentony::assets {

class CustomParkRuntimeError final : public std::runtime_error {
public:
    explicit CustomParkRuntimeError(const std::string& message)
        : std::runtime_error(message) {}
};

inline constexpr std::size_t kCustomParkGeneratedPieceSize = 0x238U;
inline constexpr std::size_t kCustomParkPublishedObjectSize = 0x4cU;
inline constexpr std::size_t kCustomParkGapMemberSize = 0x58U;

struct CustomParkRuntimeCell {
    std::array<std::byte, kCustomParkCellRuntimeSize> raw{};

    [[nodiscard]] std::uint16_t translated_reference(std::size_t index) const;
};

struct CustomParkRuntimeItem {
    std::array<std::byte, kCustomParkItemRuntimeSize> raw{};

    [[nodiscard]] std::uint8_t item_byte() const noexcept;
    [[nodiscard]] std::string item_name() const;
};

// The generator owns the source-model lookup and the exact grid-coordinate
// calculation. Those inputs are intentionally explicit here: the executable
// proves the generated-piece and finalizer offsets, while the model-reference
// table remains data-driven and is not guessed by this layer.
struct CustomParkGeneratedPieceInput {
    std::array<std::int16_t, 3> grid_position{};
    std::array<std::uint16_t, 3> source_dimensions{};
    std::uint8_t source_height_cells{};
    std::uint16_t model_index{};
    std::uint8_t slot{};
};

struct CustomParkPublishedObject {
    std::array<std::byte, kCustomParkPublishedObjectSize> raw{};

    [[nodiscard]] std::array<std::int32_t, 3> position() const noexcept;
    [[nodiscard]] std::uint16_t model_index() const noexcept;
    [[nodiscard]] std::uint8_t slot() const noexcept;
};

struct CustomParkGapPairInput {
    std::uint16_t source_item{};
    std::uint16_t first_model_index{};
    std::uint16_t second_model_index{};
    std::size_t first_object{};
    std::size_t second_object{};
    bool active{true};
};

struct CustomParkGapMember {
    std::array<std::byte, kCustomParkGapMemberSize> raw{};

    [[nodiscard]] std::uint32_t source_item() const noexcept;
    [[nodiscard]] std::uint32_t partner_index() const noexcept;
    [[nodiscard]] std::uint32_t next_index() const noexcept;
    [[nodiscard]] std::uint32_t published_object_index() const noexcept;
    [[nodiscard]] bool active() const noexcept;
};

// Native counterpart of the PRK generation/finalization seam. It owns the
// expanded generation images and publishes the proven runtime record shapes;
// platform PSX/model allocation and the unresolved source-model lookup stay
// outside this value-owned bridge.
class CustomParkRuntimeRegion final {
public:
    static CustomParkRuntimeRegion build(const CustomParkArchive& archive);

    [[nodiscard]] const CustomParkArchive* source_archive() const noexcept {
        return source_archive_;
    }
    [[nodiscard]] const std::vector<CustomParkRuntimeCell>& cells() const noexcept {
        return cells_;
    }
    [[nodiscard]] const std::vector<CustomParkRuntimeItem>& items() const noexcept {
        return items_;
    }
    [[nodiscard]] const std::vector<std::array<std::byte, kCustomParkGeneratedPieceSize>>&
        generated_pieces() const noexcept {
        return generated_pieces_;
    }
    [[nodiscard]] const std::vector<CustomParkPublishedObject>& published_objects() const noexcept {
        return published_objects_;
    }
    [[nodiscard]] const std::vector<CustomParkGapMember>& gap_members() const noexcept {
        return gap_members_;
    }

    std::size_t append_generated_piece(const CustomParkGeneratedPieceInput& input);
    void append_gap_pair(const CustomParkGapPairInput& input);

private:
    const CustomParkArchive* source_archive_{};
    std::vector<CustomParkRuntimeCell> cells_;
    std::vector<CustomParkRuntimeItem> items_;
    std::vector<std::array<std::byte, kCustomParkGeneratedPieceSize>> generated_pieces_;
    std::vector<CustomParkPublishedObject> published_objects_;
    std::vector<CustomParkGapMember> gap_members_;
};

} // namespace opentony::assets
