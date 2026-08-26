#include "save_runtime.hpp"

#include <algorithm>
#include <cctype>
#include <limits>

namespace opentony::assets {
namespace {

[[nodiscard]] std::string header_name(const SaveHeader& header) {
    std::size_t length = 0;
    while (length < header.name.size() && header.name[length] != '\0') {
        ++length;
    }
    return std::string(header.name.data(), length);
}

[[nodiscard]] std::size_t ceil_card_blocks(std::size_t bytes) noexcept {
    return (bytes + kSaveManagerCardBlockSize - 1U) / kSaveManagerCardBlockSize;
}

[[nodiscard]] char folded(char value) noexcept {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(value)));
}

} // namespace

SaveFileCandidate SaveManagerRuntime::candidate_from(
    std::string path,
    const SaveGameFile& file) {
    return SaveFileCandidate{
        .path = std::move(path),
        .byte_size = file.bytes().size(),
        .action_type = file.header().action_type,
        .display_name = header_name(file.header()),
    };
}

void SaveManagerRuntime::scan(std::span<const SaveFileCandidate> candidates) {
    files_.clear();
    std::size_t used_blocks = 0;
    for (const SaveFileCandidate& candidate : candidates) {
        if (candidate.byte_size == 0
            || candidate.byte_size % kSaveManagerCardBlockSize != 0) {
            continue;
        }
        files_.push_back(SaveManagerFileRecord{
            .path = candidate.path,
            .byte_size = candidate.byte_size,
            .action_type = candidate.action_type,
            .display_name = candidate.display_name,
        });
        const std::size_t blocks = ceil_card_blocks(candidate.byte_size);
        if (used_blocks > std::numeric_limits<std::size_t>::max() - blocks) {
            used_blocks = std::numeric_limits<std::size_t>::max();
        } else {
            used_blocks += blocks;
        }
    }
    free_card_blocks_ = used_blocks >= kSaveManagerCardBlocks
        ? 0U : kSaveManagerCardBlocks - used_blocks;
    ready_ = true;
}

void SaveManagerRuntime::clear_buffers() noexcept {
    buffers_.clear();
}

void SaveManagerRuntime::require_aligned_buffer(std::span<const std::byte> bytes) {
    if (bytes.size() % kSaveBlockAlignment != 0) {
        throw SaveRuntimeError("registered save buffer is not 128-byte aligned");
    }
}

void SaveManagerRuntime::register_buffer(std::span<const std::byte> bytes) {
    if (buffers_.size() >= kSaveManagerMaximumBuffers) {
        throw SaveRuntimeError("save manager has no free buffer slot");
    }
    require_aligned_buffer(bytes);
    buffers_.push_back(SaveBufferRecord{
        .bytes = std::vector<std::byte>(bytes.begin(), bytes.end()),
    });
}

void SaveManagerRuntime::register_career_buffer(std::span<const std::byte> bytes) {
    if (bytes.size() != kCareerRegisteredSize) {
        throw SaveRuntimeError("career save buffer is not 0x1e00 bytes");
    }
    clear_buffers();
    register_buffer(bytes);
}

void SaveManagerRuntime::register_replay_streams(
    std::span<const std::byte> first,
    std::span<const std::byte> second) {
    if (first.size() != 0x7fe00U
        || (!second.empty() && second.size() != 0x7fe00U)) {
        throw SaveRuntimeError("replay save stream is not 0x7fe00 bytes");
    }
    clear_buffers();
    register_buffer(first);
    if (!second.empty()) {
        register_buffer(second);
    }
}

void SaveManagerRuntime::register_custom_park_buffer(std::span<const std::byte> bytes) {
    if (bytes.empty()) {
        throw SaveRuntimeError("custom park save buffer is empty");
    }
    clear_buffers();
    register_buffer(bytes);
}

const SaveManagerFileRecord& SaveManagerRuntime::file(std::size_t index) const {
    if (index >= files_.size()) {
        throw SaveRuntimeError("save manager file index is out of range");
    }
    return files_[index];
}

std::size_t SaveManagerRuntime::registered_payload_size() const noexcept {
    std::size_t total = 0;
    for (const SaveBufferRecord& buffer : buffers_) {
        if (total > std::numeric_limits<std::size_t>::max() - buffer.bytes.size()) {
            return std::numeric_limits<std::size_t>::max();
        }
        total += buffer.bytes.size();
    }
    return total;
}

std::size_t SaveManagerRuntime::required_card_blocks() const noexcept {
    const std::size_t payload = registered_payload_size();
    if (payload == std::numeric_limits<std::size_t>::max()
        || payload > std::numeric_limits<std::size_t>::max() - kSaveHeaderSize) {
        return std::numeric_limits<std::size_t>::max();
    }
    return ceil_card_blocks(kSaveHeaderSize + payload);
}

bool SaveManagerRuntime::equal_folded(
    std::string_view left,
    std::string_view right) noexcept {
    if (left.size() != right.size()) {
        return false;
    }
    return std::equal(left.begin(), left.end(), right.begin(),
        [](char a, char b) { return folded(a) == folded(b); });
}

const SaveManagerFileRecord* SaveManagerRuntime::find_path(
    std::string_view path) const noexcept {
    const auto found = std::find_if(files_.begin(), files_.end(),
        [path](const SaveManagerFileRecord& record) {
            return equal_folded(record.path, path);
        });
    return found == files_.end() ? nullptr : &*found;
}

const SaveManagerFileRecord* SaveManagerRuntime::find_display_name(
    std::string_view name) const noexcept {
    const auto found = std::find_if(files_.begin(), files_.end(),
        [name](const SaveManagerFileRecord& record) {
            return equal_folded(record.display_name, name);
        });
    return found == files_.end() ? nullptr : &*found;
}

SaveGameFile SaveManagerRuntime::build_registered_file(
    SaveActionType action_type,
    std::string_view display_name,
    const std::array<std::array<std::byte, kSaveHeaderBlockSize>, kSaveHeaderBlockCount>& blocks,
    std::string source) const {
    std::vector<std::byte> payload;
    const std::size_t total = registered_payload_size();
    if (total == std::numeric_limits<std::size_t>::max()) {
        throw SaveRuntimeError("registered save payload size overflows");
    }
    payload.reserve(total);
    for (const SaveBufferRecord& buffer : buffers_) {
        payload.insert(payload.end(), buffer.bytes.begin(), buffer.bytes.end());
    }
    return SaveGameFile::build(action_type, display_name, blocks, std::move(payload),
        std::move(source));
}

const char* SaveManagerRuntime::type_label(SaveActionType type) noexcept {
    switch (type) {
    case SaveActionType::career:
        return "THPS2_CAREER";
    case SaveActionType::replay:
        return "THPS2_REPLAY";
    case SaveActionType::custom_park:
        return "THPS2_PARK";
    }
    return "";
}

std::string SaveManagerRuntime::make_filename(
    std::string_view user_name,
    std::array<char, 2> random_uppercase,
    SaveActionType type) {
    if (user_name.empty()) {
        throw SaveRuntimeError("save filename requires a user name");
    }
    for (char value : random_uppercase) {
        if (value < 'A' || value > 'Z') {
            throw SaveRuntimeError("save filename token requires uppercase letters");
        }
    }
    char suffix = '\0';
    switch (type) {
    case SaveActionType::career:
        suffix = 'G';
        break;
    case SaveActionType::replay:
        suffix = 'V';
        break;
    case SaveActionType::custom_park:
        suffix = 'P';
        break;
    }
    if (suffix == '\0') {
        throw SaveRuntimeError("save filename action type is unsupported");
    }
    std::string result = "THPS2_";
    result.push_back(user_name.front());
    result.append(random_uppercase.data(), random_uppercase.size());
    result.push_back(user_name.back());
    result.push_back(suffix);
    result += ".SAV";
    return result;
}

} // namespace opentony::assets
