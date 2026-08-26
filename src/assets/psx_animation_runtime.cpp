#include "psx_animation_runtime.hpp"

#include <algorithm>
#include <limits>

namespace opentony::assets {
namespace {

[[nodiscard]] std::uint32_t read_u32(
    const std::vector<std::byte>& bytes,
    std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 4U) {
        throw PsxFormatError("PSX animation payload has a truncated u32");
    }
    return static_cast<std::uint32_t>(
        std::to_integer<std::uint8_t>(bytes[offset])
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 1])) << 8U)
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 2])) << 16U)
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 3])) << 24U));
}

[[nodiscard]] std::uint16_t read_u16(
    std::span<const std::byte> bytes,
    std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 2U) {
        throw PsxFormatError("PSX animation channel has a truncated u16");
    }
    return static_cast<std::uint16_t>(
        std::to_integer<std::uint8_t>(bytes[offset])
        | (static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes[offset + 1U])) << 8U));
}

[[nodiscard]] std::int16_t wrap_s16(std::int32_t value) noexcept {
    return static_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

[[nodiscard]] std::int32_t read_signed_bits(
    std::span<const std::byte> bytes,
    std::size_t bit_offset,
    std::uint8_t width) {
    if (width == 0U || width > 14U) {
        throw PsxFormatError("PSX animation channel has an invalid packed width");
    }
    if (bit_offset > bytes.size() * 8U
        || width > bytes.size() * 8U - bit_offset) {
        throw PsxFormatError("PSX animation channel packed delta is truncated");
    }
    const std::size_t byte_offset = bit_offset / 8U;
    const std::uint32_t window = static_cast<std::uint32_t>(
        std::to_integer<std::uint8_t>(bytes[byte_offset]) << 16U)
        | (static_cast<std::uint32_t>(byte_offset + 1U < bytes.size()
                    ? std::to_integer<std::uint8_t>(bytes[byte_offset + 1U])
                    : 0U) << 8U)
        | static_cast<std::uint32_t>(byte_offset + 2U < bytes.size()
            ? std::to_integer<std::uint8_t>(bytes[byte_offset + 2U])
            : 0U);
    const std::uint8_t bit_in_byte = static_cast<std::uint8_t>(bit_offset & 7U);
    const std::uint8_t shift = static_cast<std::uint8_t>(24U - bit_in_byte - width);
    const std::uint32_t mask = (1U << width) - 1U;
    std::uint32_t value = (window >> shift) & mask;
    const std::uint32_t sign = 1U << (width - 1U);
    if ((value & sign) != 0U) {
        value |= ~mask;
    }
    return static_cast<std::int32_t>(value);
}

} // namespace

PsxAnimationChannelDecode decode_psx_animation_channel(
    std::span<const std::byte> stream,
    std::size_t source_value_count) {
    if (stream.empty()) {
        throw PsxFormatError("PSX animation channel has no header");
    }
    if (source_value_count == 0U) {
        throw PsxFormatError("PSX animation channel has no source values");
    }
    const std::uint8_t header = std::to_integer<std::uint8_t>(stream[0]);
    const std::uint8_t interpolation_count = static_cast<std::uint8_t>(
        (header >> 4U) + 1U);
    const std::uint8_t encoding = static_cast<std::uint8_t>(header & 0x0fU);
    PsxAnimationChannelDecode result{
        interpolation_count, encoding, {}, 1U};

    const auto append = [&result](std::int32_t value) {
        result.samples.push_back(wrap_s16(value));
    };
    if (encoding == 0x0eU || encoding == 0x0fU) {
        if (encoding == 0x0eU) {
            const std::int16_t value = static_cast<std::int16_t>(
                read_u16(stream, 1U));
            result.consumed_bytes = 3U;
            result.samples.assign(source_value_count, value);
        } else {
            result.samples.assign(source_value_count, 0);
        }
        if (result.consumed_bytes > stream.size()) {
            throw PsxFormatError("PSX animation channel repeat value is truncated");
        }
        return result;
    }
    if (encoding > 0x0dU) {
        throw PsxFormatError("PSX animation channel encoding is unsupported");
    }

    std::int16_t current = static_cast<std::int16_t>(read_u16(stream, 1U));
    result.consumed_bytes = 3U;
    append(current);
    if (encoding == 0U) {
        std::size_t cursor = 3U;
        for (std::size_t value_index = 1; value_index < source_value_count; ++value_index) {
            const std::int16_t target = static_cast<std::int16_t>(read_u16(stream, cursor));
            cursor += 2U;
            const std::int32_t delta = static_cast<std::int32_t>(target)
                - static_cast<std::int32_t>(current);
            const std::int32_t increment = delta / interpolation_count;
            for (std::uint8_t step = 1U; step < interpolation_count; ++step) {
                current = wrap_s16(static_cast<std::int32_t>(current) + increment);
                append(current);
            }
            current = target;
            append(current);
        }
        result.consumed_bytes = cursor;
        return result;
    }

    const std::uint8_t width = static_cast<std::uint8_t>(encoding + 1U);
    std::size_t bit_offset = 24U;
    for (std::size_t value_index = 1; value_index < source_value_count; ++value_index) {
        const std::int32_t delta = read_signed_bits(stream, bit_offset, width);
        bit_offset += width;
        const std::int16_t target = wrap_s16(static_cast<std::int32_t>(current) + delta);
        const std::int32_t increment = delta / interpolation_count;
        for (std::uint8_t step = 1U; step < interpolation_count; ++step) {
            current = wrap_s16(static_cast<std::int32_t>(current) + increment);
            append(current);
        }
        current = target;
        append(current);
    }
    result.consumed_bytes = (bit_offset + 7U) / 8U;
    return result;
}

void PsxAnimationRuntime::build(const PsxArchive& archive) {
    archive_ = &archive;
    products_.clear();
    hierarchy_payload_.clear();
    for (const PsxTag& tag : archive.tags()) {
        const std::size_t payload = tag.offset + 8U;
        if (payload > archive.bytes().size()
            || tag.size > archive.bytes().size() - payload) {
            throw PsxFormatError("PSX animation tag payload is outside the archive");
        }
        const std::size_t end = payload + tag.size;
        if (tag.type == kPsxHierarchyTag) {
            hierarchy_payload_.assign(
                archive.bytes().begin() + static_cast<std::ptrdiff_t>(payload),
                archive.bytes().begin() + static_cast<std::ptrdiff_t>(end));
            continue;
        }
        if (tag.type != kPsxAnimationTag
            && tag.type != kPsxCompressedAnimationTag) {
            continue;
        }
        if (tag.size < 4U) {
            throw PsxFormatError("PSX animation tag is shorter than its count");
        }
        const std::uint32_t count = read_u32(archive.bytes(), payload);
        const std::size_t records_size = static_cast<std::size_t>(count) * 8U;
        if (count != 0U && records_size / 8U != count) {
            throw PsxFormatError("PSX animation record count overflows the host size");
        }
        if (records_size > tag.size - 4U) {
            throw PsxFormatError("PSX animation records exceed the tag payload");
        }
        PsxAnimationProduct product{};
        product.tag_type = tag.type;
        product.tag_offset = tag.offset;
        product.records.reserve(static_cast<std::size_t>(count));
        std::size_t cursor = payload + 4U;
        for (std::uint32_t index = 0; index < count; ++index) {
            std::array<std::byte, 8> record{};
            std::copy_n(
                archive.bytes().begin() + static_cast<std::ptrdiff_t>(cursor),
                record.size(), record.begin());
            cursor += record.size();
            product.records.push_back(record);
        }
        product.source_stream.assign(
            archive.bytes().begin() + static_cast<std::ptrdiff_t>(cursor),
            archive.bytes().begin() + static_cast<std::ptrdiff_t>(end));
        products_.push_back(std::move(product));
    }
}

std::uint8_t PsxAnimationPlaybackState::u8(std::size_t offset) const noexcept {
    return std::to_integer<std::uint8_t>(raw_[offset]);
}

std::uint16_t PsxAnimationPlaybackState::u16(std::size_t offset) const noexcept {
    return static_cast<std::uint16_t>(
        u8(offset) | (static_cast<std::uint16_t>(u8(offset + 1U)) << 8U));
}

std::int16_t PsxAnimationPlaybackState::s16(std::size_t offset) const noexcept {
    return static_cast<std::int16_t>(u16(offset));
}

std::int32_t PsxAnimationPlaybackState::s32(std::size_t offset) const noexcept {
    return static_cast<std::int32_t>(
        static_cast<std::uint32_t>(u16(offset))
        | (static_cast<std::uint32_t>(u16(offset + 2U)) << 16U));
}

void PsxAnimationPlaybackState::put8(
    std::size_t offset,
    std::uint8_t value) noexcept {
    raw_[offset] = static_cast<std::byte>(value);
}

void PsxAnimationPlaybackState::put16(
    std::size_t offset,
    std::uint16_t value) noexcept {
    put8(offset, static_cast<std::uint8_t>(value));
    put8(offset + 1U, static_cast<std::uint8_t>(value >> 8U));
}

void PsxAnimationPlaybackState::put32(
    std::size_t offset,
    std::uint32_t value) noexcept {
    put16(offset, static_cast<std::uint16_t>(value));
    put16(offset + 2U, static_cast<std::uint16_t>(value >> 16U));
}

void PsxAnimationPlaybackState::set_accumulator(std::int32_t value) noexcept {
    const auto bits = static_cast<std::uint32_t>(value);
    put16(0x0f4U, static_cast<std::uint16_t>(bits >> 16U));
    put16(0x104U, static_cast<std::uint16_t>(bits));
}

void PsxAnimationPlaybackState::start(
    std::uint16_t animation_index,
    std::uint8_t frame_count,
    std::int32_t start_frame,
    std::int32_t end_frame,
    std::uint8_t alternate_frame) noexcept {
    put16(0x0f6U, animation_index);
    put8(0x106U, frame_count);
    const std::int32_t last = frame_count == 0U
        ? 0
        : static_cast<std::int32_t>(frame_count) - 1;
    if (start_frame < 0) {
        start_frame = last;
    }
    if (end_frame < 0) {
        end_frame = last;
    }
    start_frame = std::clamp(start_frame, 0, last);
    end_frame = std::clamp(end_frame, 0, last);
    put8(0x0f8U, 0);
    put8(0x101U, static_cast<std::uint8_t>(end_frame));
    put8(0x102U, alternate_frame);
    const std::int8_t direction = start_frame < end_frame
        ? 1
        : (start_frame > end_frame ? -1 : 0);
    put8(0x100U, static_cast<std::uint8_t>(direction));
    put16(0x114U, static_cast<std::uint16_t>(start_frame));
    set_accumulator(static_cast<std::int32_t>(start_frame) << 16);
    put8(0x107U, static_cast<std::uint8_t>(start_frame == end_frame));
}

void PsxAnimationPlaybackState::start_special(
    std::uint16_t animation_index,
    std::uint8_t frame_count,
    std::int8_t direction) noexcept {
    put16(0x0f6U, animation_index);
    put8(0x106U, frame_count);
    put8(0x100U, static_cast<std::uint8_t>(direction));
    put8(0x0f8U, 1);
    set_accumulator(0);
    put8(0x107U, 0);
}

void PsxAnimationPlaybackState::advance(
    std::int32_t time_scale_q8,
    std::int32_t animation_clock) {
    const std::uint8_t mode_value = mode();
    const std::int8_t direction_value = direction();
    const std::int16_t current = current_frame();
    const std::int16_t endpoint = static_cast<std::int16_t>(end_frame());

    // The shared pre-switch boundary in 0x00480950 handles modes 0 and 2.
    // A positive alternate frame exchanges the endpoint and reverses; with
    // no alternate it records completion at the selected endpoint.
    if (mode_value == 0U || mode_value == 2U) {
        const bool at_endpoint = direction_value == 1
            ? current >= endpoint
            : (direction_value == -1 && current <= endpoint);
        if (at_endpoint) {
            if (alternate_frame() != 0U) {
                const std::uint8_t old_endpoint = end_frame();
                put8(0x101U, alternate_frame());
                put8(0x102U, old_endpoint);
                put8(0x100U, static_cast<std::uint8_t>(-direction_value));
            } else {
                put8(0x107U, 1);
            }
        }
    }

    std::int32_t frame_step = 0;
    if (mode_value != 2U && mode_value != 3U) {
        const std::int64_t scaled = static_cast<std::int64_t>(
            playback_rate_fixed()) * time_scale_q8;
        frame_step = static_cast<std::int32_t>(scaled >> 8U);
    }
    std::int32_t accumulator = static_cast<std::int32_t>(
        (static_cast<std::uint32_t>(static_cast<std::uint16_t>(current_frame())) << 16U)
        | u16(0x104U));
    if (direction() == 1) {
        accumulator = static_cast<std::int32_t>(
            static_cast<std::uint32_t>(accumulator)
            + static_cast<std::uint32_t>(frame_step));
    } else if (direction() == -1) {
        accumulator = static_cast<std::int32_t>(
            static_cast<std::uint32_t>(accumulator)
            - static_cast<std::uint32_t>(frame_step));
    }
    set_accumulator(accumulator);
    const std::int16_t updated = current_frame();

    switch (mode_value) {
    case 0U:
        if (direction() == 1 && updated >= static_cast<std::int16_t>(end_frame())) {
            set_accumulator(static_cast<std::int32_t>(end_frame() - 1U) << 16);
        } else if (direction() == -1 && updated <= static_cast<std::int16_t>(end_frame())) {
            set_accumulator(static_cast<std::int32_t>(end_frame()) << 16);
        }
        break;
    case 1U: {
        const std::uint8_t count = frame_count();
        if (count != 0U && updated >= static_cast<std::int16_t>(count)) {
            set_accumulator(0);
        } else if (updated < 0 && count != 0U) {
            set_accumulator(static_cast<std::int32_t>(count - 1U) << 16);
        }
        break;
    }
    case 2U:
        break;
    case 3U: {
        const std::int32_t range = static_cast<std::int32_t>(
            s16(0x0fcU)) - s16(0x0faU);
        if (range == 0) {
            set_accumulator(static_cast<std::int32_t>(s16(0x0faU)) << 16);
            break;
        }
        if (playback_rate_fixed() == 0) {
            throw PsxFormatError("PSX animation ping-pong rate is zero");
        }
        const std::int32_t positive_range = range < 0 ? -range : range;
        const std::int32_t tick = static_cast<std::int32_t>(
            0x20000 / playback_rate_fixed());
        if (tick == 0) {
            throw PsxFormatError("PSX animation ping-pong tick is zero");
        }
        std::int32_t phase = (animation_clock - s16(0x0feU)) / tick;
        const std::int32_t period = positive_range * 2;
        phase %= period;
        if (phase < 0) {
            phase += period;
        }
        if (phase > positive_range) {
            phase = period - phase;
        }
        const std::int32_t result = range > 0
            ? static_cast<std::int32_t>(s16(0x0faU)) + phase
            : static_cast<std::int32_t>(s16(0x0faU)) - phase;
        set_accumulator(result << 16);
        break;
    }
    case 4U:
        if ((direction() == 1 && updated >= static_cast<std::int16_t>(end_frame()))
            || (direction() == -1 && updated <= static_cast<std::int16_t>(end_frame()))) {
            const std::uint8_t old_direction = static_cast<std::uint8_t>(-direction());
            const std::uint8_t old_endpoint = end_frame();
            put8(0x100U, old_direction);
            put8(0x101U, alternate_frame());
            put8(0x102U, old_endpoint);
            put16(0x114U, static_cast<std::uint16_t>(updated));
            set_accumulator(static_cast<std::int32_t>(updated) << 16);
        }
        break;
    default:
        break;
    }
}

std::uint16_t PsxAnimationPlaybackState::animation_index() const noexcept {
    return u16(0x0f6U);
}

std::uint8_t PsxAnimationPlaybackState::frame_count() const noexcept {
    return u8(0x106U);
}

std::int16_t PsxAnimationPlaybackState::current_frame() const noexcept {
    return s16(0x0f4U);
}

std::uint8_t PsxAnimationPlaybackState::mode() const noexcept {
    return u8(0x0f8U);
}

std::int8_t PsxAnimationPlaybackState::direction() const noexcept {
    return static_cast<std::int8_t>(u8(0x100U));
}

std::uint8_t PsxAnimationPlaybackState::end_frame() const noexcept {
    return u8(0x101U);
}

std::uint8_t PsxAnimationPlaybackState::alternate_frame() const noexcept {
    return u8(0x102U);
}

std::int16_t PsxAnimationPlaybackState::original_start_frame() const noexcept {
    return s16(0x114U);
}

std::int32_t PsxAnimationPlaybackState::playback_rate_fixed() const noexcept {
    return s32(0x108U);
}

bool PsxAnimationPlaybackState::finished() const noexcept {
    return u8(0x107U) != 0U;
}

void PsxAnimationPlaybackState::set_mode(std::uint8_t value) noexcept {
    put8(0x0f8U, value);
}

void PsxAnimationPlaybackState::set_playback_rate_fixed(std::int32_t value) noexcept {
    put32(0x108U, static_cast<std::uint32_t>(value));
}

void PsxAnimationPlaybackState::set_pingpong_range(
    std::int16_t start,
    std::int16_t end,
    std::int16_t time_origin) noexcept {
    put16(0x0faU, static_cast<std::uint16_t>(start));
    put16(0x0fcU, static_cast<std::uint16_t>(end));
    put16(0x0feU, static_cast<std::uint16_t>(time_origin));
}

} // namespace opentony::assets
