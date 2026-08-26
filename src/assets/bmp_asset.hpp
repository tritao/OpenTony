#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace opentony::assets {

class BmpFormatError final : public std::runtime_error {
public:
    explicit BmpFormatError(const std::string& message)
        : std::runtime_error(message) {}
};

struct BmpImage {
    std::uint32_t width{};
    std::uint32_t height{};
    bool top_down{};
    // RGB bytes in top-left-origin row-major order, matching the source
    // pixel values after the PC loader's BGR24 conversion boundary.
    std::vector<std::uint8_t> rgb;

    [[nodiscard]] std::size_t pixel_count() const noexcept {
        return static_cast<std::size_t>(width) * height;
    }
};

// Bounded PC bitmap reader for the D3DTex_OpenImage path. The runtime branch
// proven for PSX material lookups accepts only uncompressed 24-bit BMP/DIB
// images; indexed upload is a separate path for inline PSX textures.
class BmpAsset final {
public:
    static BmpAsset load(const std::string& path);
    static BmpAsset parse(std::vector<std::byte> bytes, std::string source = {});

    [[nodiscard]] const BmpImage& image() const noexcept { return image_; }
    [[nodiscard]] const std::vector<std::byte>& bytes() const noexcept { return bytes_; }
    [[nodiscard]] std::uint32_t dib_size() const noexcept { return dib_size_; }
    [[nodiscard]] std::uint32_t pixel_offset() const noexcept { return pixel_offset_; }

private:
    std::vector<std::byte> bytes_;
    std::string source_;
    std::uint32_t dib_size_{};
    std::uint32_t pixel_offset_{};
    BmpImage image_{};
};

} // namespace opentony::assets
