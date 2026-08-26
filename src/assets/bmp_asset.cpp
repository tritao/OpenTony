#include "bmp_asset.hpp"

#include <algorithm>
#include <fstream>
#include <limits>

namespace opentony::assets {
namespace {

[[nodiscard]] std::uint16_t u16(
    const std::vector<std::byte>& bytes,
    std::size_t offset,
    const std::string& source) {
    if (offset > bytes.size() || bytes.size() - offset < 2U) {
        throw BmpFormatError("BMP u16 is truncated: " + source);
    }
    return static_cast<std::uint16_t>(
        std::to_integer<std::uint8_t>(bytes[offset])
        | (static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes[offset + 1])) << 8U));
}

[[nodiscard]] std::uint32_t u32(
    const std::vector<std::byte>& bytes,
    std::size_t offset,
    const std::string& source) {
    if (offset > bytes.size() || bytes.size() - offset < 4U) {
        throw BmpFormatError("BMP u32 is truncated: " + source);
    }
    return static_cast<std::uint32_t>(
        std::to_integer<std::uint8_t>(bytes[offset])
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 1])) << 8U)
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 2])) << 16U)
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 3])) << 24U));
}

[[nodiscard]] std::int32_t i32(
    const std::vector<std::byte>& bytes,
    std::size_t offset,
    const std::string& source) {
    return static_cast<std::int32_t>(u32(bytes, offset, source));
}

[[nodiscard]] std::size_t checked_product(
    std::size_t left,
    std::size_t right,
    const std::string& source) {
    if (right != 0 && left > std::numeric_limits<std::size_t>::max() / right) {
        throw BmpFormatError("BMP dimensions overflow: " + source);
    }
    return left * right;
}

} // namespace

BmpAsset BmpAsset::load(const std::string& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        throw BmpFormatError("cannot open BMP: " + path);
    }
    const std::streamoff length = input.tellg();
    if (length < 0) {
        throw BmpFormatError("cannot determine BMP size: " + path);
    }
    input.seekg(0, std::ios::beg);
    std::vector<std::byte> bytes(static_cast<std::size_t>(length));
    if (!bytes.empty()) {
        input.read(reinterpret_cast<char*>(bytes.data()), length);
        if (!input) {
            throw BmpFormatError("cannot read BMP: " + path);
        }
    }
    return parse(std::move(bytes), path);
}

BmpAsset BmpAsset::parse(std::vector<std::byte> bytes, std::string source) {
    if (bytes.size() < 14U || bytes[0] != std::byte{'B'} || bytes[1] != std::byte{'M'}) {
        throw BmpFormatError("BMP file header is invalid: " + source);
    }
    const std::uint32_t pixel_offset = u32(bytes, 10, source);
    const std::uint32_t dib_size = u32(bytes, 14, source);
    if (dib_size < 40U || bytes.size() < 14U + dib_size) {
        throw BmpFormatError("BMP DIB header is unsupported: " + source);
    }
    const std::int32_t signed_width = i32(bytes, 18, source);
    const std::int32_t signed_height = i32(bytes, 22, source);
    if (signed_width <= 0 || signed_height == 0) {
        throw BmpFormatError("BMP dimensions are invalid: " + source);
    }
    if (u16(bytes, 26, source) != 1U || u16(bytes, 28, source) != 24U) {
        throw BmpFormatError("BMP is not a 24-bit image: " + source);
    }
    if (u32(bytes, 30, source) != 0U) {
        throw BmpFormatError("compressed BMP is unsupported: " + source);
    }
    const bool top_down = signed_height < 0;
    const std::size_t width = static_cast<std::size_t>(signed_width);
    const std::size_t height = top_down
        ? static_cast<std::size_t>(-(static_cast<std::int64_t>(signed_height)))
        : static_cast<std::size_t>(signed_height);
    if (pixel_offset > bytes.size()) {
        throw BmpFormatError("BMP pixel offset is outside the file: " + source);
    }
    const std::size_t row_bytes = checked_product(width, 3U, source);
    const std::size_t stride = (row_bytes + 3U) & ~std::size_t{3U};
    const std::size_t pixel_bytes = checked_product(stride, height, source);
    if (pixel_bytes > bytes.size() - pixel_offset) {
        throw BmpFormatError("BMP pixel data is truncated: " + source);
    }
    BmpAsset result{};
    result.bytes_ = std::move(bytes);
    result.source_ = std::move(source);
    result.dib_size_ = dib_size;
    result.pixel_offset_ = pixel_offset;
    result.image_.width = static_cast<std::uint32_t>(width);
    result.image_.height = static_cast<std::uint32_t>(height);
    result.image_.top_down = top_down;
    result.image_.rgb.resize(checked_product(row_bytes, height, result.source_));
    for (std::size_t destination_row = 0; destination_row < height; ++destination_row) {
        const std::size_t source_row = top_down ? destination_row : height - 1U - destination_row;
        const std::size_t source_offset = pixel_offset + source_row * stride;
        const std::size_t destination_offset = destination_row * row_bytes;
        for (std::size_t pixel = 0; pixel < width; ++pixel) {
            const std::size_t source_pixel = source_offset + pixel * 3U;
            const std::size_t destination_pixel = destination_offset + pixel * 3U;
            result.image_.rgb[destination_pixel + 0] = std::to_integer<std::uint8_t>(
                result.bytes_[source_pixel + 2]);
            result.image_.rgb[destination_pixel + 1] = std::to_integer<std::uint8_t>(
                result.bytes_[source_pixel + 1]);
            result.image_.rgb[destination_pixel + 2] = std::to_integer<std::uint8_t>(
                result.bytes_[source_pixel + 0]);
        }
    }
    return result;
}

} // namespace opentony::assets
