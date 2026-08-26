#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace opentony::assets {

class WavFormatError final : public std::runtime_error {
public:
    explicit WavFormatError(const std::string& message)
        : std::runtime_error(message) {}
};

struct WavFormat {
    std::uint16_t format_tag{};
    std::uint16_t channels{};
    std::uint32_t samples_per_second{};
    std::uint32_t average_bytes_per_second{};
    std::uint16_t block_align{};
    std::uint16_t bits_per_sample{};
    std::uint16_t extension_size{};
};

// Runtime-facing result of the retail WAV loader: WAVEFORMATEX fields plus a
// separately allocated, four-byte-aligned PCM sample buffer. The parser
// accepts only the observed PCM format and keeps no pointer into the source
// resource.
class WavPcmAsset final {
public:
    static WavPcmAsset load(const std::string& path);
    static WavPcmAsset parse(std::vector<std::byte> bytes, std::string source = {});

    [[nodiscard]] const WavFormat& format() const noexcept { return format_; }
    [[nodiscard]] const std::vector<std::byte>& samples() const noexcept {
        return samples_;
    }
    [[nodiscard]] std::size_t sample_size() const noexcept { return sample_size_; }
    [[nodiscard]] std::size_t allocated_sample_size() const noexcept {
        return samples_.size();
    }

private:
    std::string source_;
    WavFormat format_{};
    std::vector<std::byte> samples_;
    std::size_t sample_size_{};
};

} // namespace opentony::assets
