#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace opentony::assets {

class TextFormatError final : public std::runtime_error {
public:
    explicit TextFormatError(const std::string& message)
        : std::runtime_error(message) {}
};

struct ParkLabel {
    std::uint32_t index{};
    std::string text;
};

class ParkLabelTable final {
public:
    static ParkLabelTable load(const std::string& path);
    static ParkLabelTable parse(std::vector<std::byte> bytes, std::string source = {});

    [[nodiscard]] const std::vector<ParkLabel>& labels() const noexcept { return labels_; }
    [[nodiscard]] const std::vector<std::byte>& bytes() const noexcept { return bytes_; }

private:
    std::vector<std::byte> bytes_;
    std::string source_;
    std::vector<ParkLabel> labels_;
};

enum class PresentationTextRecordKind : std::uint8_t {
    text = 0,
    bitmap = 1,
    marker = 2,
};

struct PresentationTextRecord {
    PresentationTextRecordKind kind{PresentationTextRecordKind::text};
    std::string text;
    std::uint8_t marker_a{};
    std::uint8_t marker_b{};
    bool alternate_font{};
};

// Reader for the bounded line/tag stream consumed by CREDITS.TXT and
// MUSIC.TXT. The returned records own their text because the retail parser
// frees its temporary source buffer after publishing runtime records.
class PresentationTextAsset final {
public:
    static PresentationTextAsset load(const std::string& path);
    static PresentationTextAsset parse(std::vector<std::byte> bytes, std::string source = {});

    [[nodiscard]] const std::vector<PresentationTextRecord>& records() const noexcept {
        return records_;
    }
    [[nodiscard]] const std::vector<std::byte>& bytes() const noexcept { return bytes_; }

private:
    std::vector<std::byte> bytes_;
    std::string source_;
    std::vector<PresentationTextRecord> records_;
};

} // namespace opentony::assets
