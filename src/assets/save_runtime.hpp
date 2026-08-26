#pragma once

#include "save_asset.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace opentony::assets {

class SaveRuntimeError final : public std::runtime_error {
public:
    explicit SaveRuntimeError(const std::string& message)
        : std::runtime_error(message) {}
};

inline constexpr std::size_t kSaveManagerFileRecordSize = 0x94U;
inline constexpr std::size_t kSaveManagerFileCountOffset = 0xb18U;
inline constexpr std::size_t kSaveManagerReadyOffset = 0xb1cU;
inline constexpr std::size_t kSaveManagerFreeBlocksOffset = 0xb20U;
inline constexpr std::size_t kSaveManagerBufferCountOffset = 0xb28U;
inline constexpr std::size_t kSaveManagerBufferPointerOffset = 0xb2cU;
inline constexpr std::size_t kSaveManagerBufferSizeOffset = 0xb3cU;
inline constexpr std::size_t kSaveManagerMaximumBuffers = 4U;
inline constexpr std::size_t kSaveManagerCardBlocks = 15U;
inline constexpr std::size_t kSaveManagerCardBlockSize = kSaveCardBlockSize;

struct SaveFileCandidate {
    std::string path;
    std::size_t byte_size{};
    SaveActionType action_type{SaveActionType::career};
    std::string display_name;
};

struct SaveManagerFileRecord {
    std::string path;
    std::size_t byte_size{};
    SaveActionType action_type{SaveActionType::career};
    std::string display_name;
};

struct SaveBufferRecord {
    std::vector<std::byte> bytes;
};

// Native state model for the executable's PC MMU/card compatibility object.
// The platform directory, read, write, and delete calls remain adapters; this
// class owns the proven record table, four registered buffer slots, and the
// common SC-file assembly contract.
class SaveManagerRuntime final {
public:
    static SaveFileCandidate candidate_from(
        std::string path,
        const SaveGameFile& file);

    void scan(std::span<const SaveFileCandidate> candidates);
    void clear_buffers() noexcept;
    void register_buffer(std::span<const std::byte> bytes);
    void register_career_buffer(std::span<const std::byte> bytes);
    void register_replay_streams(
        std::span<const std::byte> first,
        std::span<const std::byte> second = {});
    void register_custom_park_buffer(std::span<const std::byte> bytes);

    [[nodiscard]] const std::vector<SaveManagerFileRecord>& files() const noexcept {
        return files_;
    }
    [[nodiscard]] const SaveManagerFileRecord& file(std::size_t index) const;
    [[nodiscard]] std::size_t file_count() const noexcept { return files_.size(); }
    [[nodiscard]] std::size_t free_card_blocks() const noexcept { return free_card_blocks_; }
    [[nodiscard]] bool ready() const noexcept { return ready_; }
    [[nodiscard]] std::size_t buffer_count() const noexcept { return buffers_.size(); }
    [[nodiscard]] const std::vector<SaveBufferRecord>& buffers() const noexcept {
        return buffers_;
    }
    [[nodiscard]] std::size_t registered_payload_size() const noexcept;
    [[nodiscard]] std::size_t required_card_blocks() const noexcept;

    [[nodiscard]] const SaveManagerFileRecord* find_path(std::string_view path) const noexcept;
    [[nodiscard]] const SaveManagerFileRecord* find_display_name(
        std::string_view name) const noexcept;

    // Assemble the currently registered buffers after the common 0x200-byte
    // SC header. Type-specific callers retain ownership of their schema, but
    // the byte ordering and alignment are shared by all three save families.
    [[nodiscard]] SaveGameFile build_registered_file(
        SaveActionType action_type,
        std::string_view display_name,
        const std::array<std::array<std::byte, kSaveHeaderBlockSize>, kSaveHeaderBlockCount>&
            blocks,
        std::string source = {}) const;

    [[nodiscard]] static const char* type_label(SaveActionType type) noexcept;
    [[nodiscard]] static std::string make_filename(
        std::string_view user_name,
        std::array<char, 2> random_uppercase,
        SaveActionType type);

private:
    static void require_aligned_buffer(std::span<const std::byte> bytes);
    static bool equal_folded(std::string_view left, std::string_view right) noexcept;

    std::vector<SaveManagerFileRecord> files_;
    std::vector<SaveBufferRecord> buffers_;
    std::size_t free_card_blocks_{kSaveManagerCardBlocks};
    bool ready_{};
};

} // namespace opentony::assets
