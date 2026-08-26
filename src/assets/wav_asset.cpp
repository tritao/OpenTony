#include "wav_asset.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <limits>
#include <utility>

namespace opentony::assets {
namespace {

[[nodiscard]] std::uint16_t u16(
    const std::vector<std::byte>& bytes,
    std::size_t offset,
    const std::string& source) {
    if (offset > bytes.size() || bytes.size() - offset < 2U) {
        throw WavFormatError("WAV u16 is truncated: " + source);
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
        throw WavFormatError("WAV u32 is truncated: " + source);
    }
    return static_cast<std::uint32_t>(
        std::to_integer<std::uint8_t>(bytes[offset])
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 1])) << 8U)
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 2])) << 16U)
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 3])) << 24U));
}

[[nodiscard]] bool fourcc(
    const std::vector<std::byte>& bytes,
    std::size_t offset,
    const char* value) noexcept {
    return offset <= bytes.size() && bytes.size() - offset >= 4U
        && std::to_integer<std::uint8_t>(bytes[offset + 0])
            == static_cast<std::uint8_t>(value[0])
        && std::to_integer<std::uint8_t>(bytes[offset + 1])
            == static_cast<std::uint8_t>(value[1])
        && std::to_integer<std::uint8_t>(bytes[offset + 2])
            == static_cast<std::uint8_t>(value[2])
        && std::to_integer<std::uint8_t>(bytes[offset + 3])
            == static_cast<std::uint8_t>(value[3]);
}

} // namespace

WavPcmAsset WavPcmAsset::load(const std::string& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        throw WavFormatError("cannot open WAV file: " + path);
    }
    const std::streamoff length = input.tellg();
    if (length < 0) {
        throw WavFormatError("cannot determine WAV file size: " + path);
    }
    input.seekg(0, std::ios::beg);
    std::vector<std::byte> bytes(static_cast<std::size_t>(length));
    if (!bytes.empty()) {
        input.read(reinterpret_cast<char*>(bytes.data()), length);
        if (!input) {
            throw WavFormatError("cannot read WAV file: " + path);
        }
    }
    return parse(std::move(bytes), path);
}

WavPcmAsset WavPcmAsset::parse(
    std::vector<std::byte> bytes,
    std::string source) {
    if (bytes.size() < 12U || !fourcc(bytes, 0, "RIFF")
        || !fourcc(bytes, 8, "WAVE")) {
        throw WavFormatError("WAV header is not RIFF/WAVE: " + source);
    }
    const std::uint32_t riff_size = u32(bytes, 4, source);
    if (riff_size < 4U || riff_size > bytes.size() - 8U) {
        throw WavFormatError("WAV RIFF boundary is invalid: " + source);
    }
    const std::size_t end = 8U + riff_size;
    WavPcmAsset result{};
    result.source_ = std::move(source);
    bool have_format = false;
    bool have_data = false;
    std::size_t cursor = 12U;
    std::size_t data_offset = 0;
    std::size_t data_size = 0;
    while (cursor < end) {
        if (end - cursor < 8U) {
            throw WavFormatError("WAV chunk header is truncated: " + result.source_);
        }
        const bool is_format = fourcc(bytes, cursor, "fmt ");
        const bool is_data = fourcc(bytes, cursor, "data");
        const std::uint32_t chunk_size = u32(bytes, cursor + 4U, result.source_);
        cursor += 8U;
        if (chunk_size > end - cursor) {
            throw WavFormatError("WAV chunk exceeds RIFF boundary: " + result.source_);
        }
        if (is_format) {
            if (chunk_size < 16U) {
                throw WavFormatError("WAV fmt chunk is too short: " + result.source_);
            }
            result.format_.format_tag = u16(bytes, cursor, result.source_);
            result.format_.channels = u16(bytes, cursor + 2U, result.source_);
            result.format_.samples_per_second = u32(bytes, cursor + 4U, result.source_);
            result.format_.average_bytes_per_second = u32(bytes, cursor + 8U, result.source_);
            result.format_.block_align = u16(bytes, cursor + 12U, result.source_);
            result.format_.bits_per_sample = u16(bytes, cursor + 14U, result.source_);
            if (chunk_size >= 18U) {
                result.format_.extension_size = u16(bytes, cursor + 16U, result.source_);
            }
            have_format = true;
        } else if (is_data && !have_data) {
            data_offset = cursor;
            data_size = static_cast<std::size_t>(chunk_size);
            have_data = true;
        }
        cursor += chunk_size;
        if ((chunk_size & 1U) != 0U) {
            if (cursor == end) {
                break;
            }
            ++cursor;
        }
    }
    if (!have_format || !have_data) {
        throw WavFormatError("WAV is missing fmt or data: " + result.source_);
    }
    if (result.format_.format_tag != 1U) {
        throw WavFormatError("WAV is not PCM: " + result.source_);
    }
    if (data_size > std::numeric_limits<std::size_t>::max() - 3U) {
        throw WavFormatError("WAV sample allocation overflows the host: " + result.source_);
    }
    result.sample_size_ = data_size;
    const std::size_t aligned_size = (data_size + 3U) & ~static_cast<std::size_t>(3U);
    result.samples_.assign(aligned_size, std::byte{0});
    std::copy_n(
        bytes.begin() + static_cast<std::ptrdiff_t>(data_offset),
        data_size,
        result.samples_.begin());
    return result;
}

} // namespace opentony::assets
