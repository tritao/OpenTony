#include "animation_cursor.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>

namespace opentony::runtime {
namespace {

template <typename Signed, typename Unsigned>
[[nodiscard]] Signed bits_as_signed(Unsigned value) noexcept {
    static_assert(sizeof(Signed) == sizeof(Unsigned));
    return std::bit_cast<Signed>(value);
}

[[nodiscard]] std::int8_t narrow_i8(std::int32_t value) noexcept {
    return bits_as_signed<std::int8_t>(
        static_cast<std::uint8_t>(static_cast<std::uint32_t>(value)));
}

[[nodiscard]] std::int16_t narrow_i16(std::int32_t value) noexcept {
    return bits_as_signed<std::int16_t>(
        static_cast<std::uint16_t>(static_cast<std::uint32_t>(value)));
}

[[nodiscard]] std::int32_t arithmetic_shift_right_8(
    std::int64_t value) noexcept {
    if (value >= 0) {
        return static_cast<std::int32_t>(value / 0x100);
    }
    // C++ division truncates toward zero; x86 SAR rounds negative values
    // toward negative infinity.
    return static_cast<std::int32_t>(-
        ((-value + 0xff) / 0x100));
}

[[nodiscard]] std::int32_t retail_frame_delta(
    std::uint32_t rate,
    std::int32_t global_scale_q8) noexcept {
    // The retail instruction multiplies two 32-bit integers before the
    // arithmetic shift. Do the product in unsigned space to make the x86
    // two's-complement wrap explicit, then apply the signed shift rule.
    const std::uint32_t product_bits =
        rate * static_cast<std::uint32_t>(global_scale_q8);
    const std::int32_t product = bits_as_signed<std::int32_t>(product_bits);
    return arithmetic_shift_right_8(product);
}

[[nodiscard]] std::uint32_t packed_bits(
    std::int16_t frame,
    std::uint16_t fraction) noexcept {
    return (static_cast<std::uint32_t>(
                static_cast<std::uint16_t>(frame)) << 16U)
        | fraction;
}

[[nodiscard]] std::int32_t packed_value(
    std::int16_t frame,
    std::uint16_t fraction) noexcept {
    return bits_as_signed<std::int32_t>(packed_bits(frame, fraction));
}

void unpacked_frame(
    std::uint32_t packed,
    std::int16_t& frame,
    std::uint16_t& fraction) noexcept {
    frame = bits_as_signed<std::int16_t>(
        static_cast<std::uint16_t>(packed >> 16U));
    fraction = static_cast<std::uint16_t>(packed);
}

[[nodiscard]] std::int32_t packed_with_low_word(
    std::uint32_t high_word,
    std::int16_t low_word) noexcept {
    return bits_as_signed<std::int32_t>(
        (high_word & 0xffff0000U)
        | static_cast<std::uint16_t>(low_word));
}

} // namespace

std::int16_t approach_animation_frame(
    std::int16_t current,
    std::int16_t target) noexcept {
    const std::int32_t difference =
        static_cast<std::int32_t>(target) - current;
    if (difference == 0) {
        return current;
    }
    const std::int32_t magnitude = difference < 0
        ? -difference
        : difference;
    const std::int32_t step = magnitude > 0xc
        ? 5
        : (magnitude > 3 ? 3 : 1);
    const std::int32_t signed_step = difference < 0 ? -step : step;
    // The retail targets are within the step distance when this helper is
    // called. Clamp here as a safe standalone contract so it never overshoots
    // if a future caller supplies a one- or two-frame remainder.
    const std::int32_t next = static_cast<std::int32_t>(current) + signed_step;
    if (difference > 0 && next > target) {
        return target;
    }
    if (difference < 0 && next < target) {
        return target;
    }
    return narrow_i16(next);
}

AnimationRequestResult AnimationCursor::request(
    AnimationTableView table,
    std::uint16_t animation,
    std::int32_t start,
    std::int32_t end,
    std::int32_t alternate) noexcept {
    AnimationRequestResult result{};
    result.requested_id = animation;
    result.effective_id = animation;

    if (!table.contains(animation)) {
        result.invalid_id = true;
        result.effective_id = 0x2e;
    }
    result.frame_count = table.frame_count(result.effective_id);
    if (result.frame_count == 0) {
        return result;
    }

    const std::int32_t last_frame =
        static_cast<std::int32_t>(result.frame_count) - 1;
    if (start == -1) {
        start = last_frame;
    }
    if (end == -1) {
        end = last_frame;
    }
    if (start < 0) {
        start = 0;
    }
    if (end < 0) {
        end = 0;
    }
    if (start >= result.frame_count) {
        start = last_frame;
    }
    if (end >= result.frame_count) {
        end = last_frame;
    }

    id = result.effective_id;
    frame_count = result.frame_count;
    mode = static_cast<std::uint8_t>(AnimationPlaybackMode::Stop);
    direction = start < end ? 1 : (start <= end ? 0 : -1);
    endpoint = narrow_i8(end);
    alternate_endpoint = narrow_i8(alternate);
    request_start = narrow_i16(start);
    frame = narrow_i16(start);
    fraction = 0;
    finished = start == static_cast<std::int32_t>(endpoint);
    result.applied = true;
    return result;
}

AnimationRequestResult AnimationCursor::cycle(
    AnimationTableView table,
    std::uint16_t animation,
    std::int32_t playback_direction) noexcept {
    AnimationRequestResult result{};
    result.requested_id = animation;
    result.effective_id = animation;
    result.frame_count = table.frame_count(animation);
    if (result.frame_count == 0) {
        result.invalid_id = !table.contains(animation);
        return result;
    }

    id = animation;
    direction = narrow_i8(playback_direction);
    frame_count = result.frame_count;
    mode = static_cast<std::uint8_t>(AnimationPlaybackMode::Loop);
    frame = 0;
    fraction = 0;
    finished = false;
    result.applied = true;
    return result;
}

std::int32_t AnimationCursor::advance(
    std::int32_t global_scale_q8,
    std::int32_t animation_clock) noexcept {
    const auto playback_mode = static_cast<AnimationPlaybackMode>(mode);

    if ((playback_mode == AnimationPlaybackMode::Stop
         || playback_mode == AnimationPlaybackMode::Hold)
        && ((direction == 1 && endpoint <= frame)
            || (direction == -1 && frame <= endpoint))) {
        if (alternate_endpoint < 1) {
            finished = true;
        } else {
            const std::int8_t reached = endpoint;
            endpoint = alternate_endpoint;
            alternate_endpoint = reached;
            direction = narrow_i8(-direction);
        }
    }

    std::int32_t frame_delta = retail_frame_delta(rate, global_scale_q8);
    if (playback_mode == AnimationPlaybackMode::Hold
        || playback_mode == AnimationPlaybackMode::ClockPingPong) {
        frame_delta = 0;
    }

    std::uint32_t accumulator = packed_bits(frame, fraction);
    if (direction == 1) {
        accumulator += static_cast<std::uint32_t>(frame_delta);
    } else if (direction == -1) {
        accumulator -= static_cast<std::uint32_t>(frame_delta);
    }
    unpacked_frame(accumulator, frame, fraction);

    switch (playback_mode) {
    case AnimationPlaybackMode::Stop:
        if (direction == 1) {
            // The retail return value is rebuilt with the endpoint in its
            // low word even when the frame has not reached it. The stored
            // fraction remains the actual low half of the accumulator.
            const std::int32_t returned = packed_with_low_word(
                accumulator,
                static_cast<std::int16_t>(endpoint));
            if (frame >= endpoint) {
                frame = bits_as_signed<std::int16_t>(
                    static_cast<std::uint16_t>(endpoint));
            }
            return returned;
        }
        if (direction == -1) {
            const std::int32_t returned = packed_with_low_word(
                accumulator,
                static_cast<std::int16_t>(endpoint));
            if (frame <= endpoint) {
                frame = bits_as_signed<std::int16_t>(
                    static_cast<std::uint16_t>(endpoint));
            }
            return returned;
        }
        return bits_as_signed<std::int32_t>(accumulator);

    case AnimationPlaybackMode::Loop: {
        if (frame_count <= frame) {
            frame = 0;
        }
        if (frame >= 0) {
            return packed_with_low_word(
                accumulator,
                static_cast<std::int16_t>(frame_count));
        }
        // Retail rebuilds the low word from the selected frame count before
        // subtracting one; it does not use the accumulator's low fraction as
        // the wrapped frame number.
        const std::uint32_t wrapped =
            (accumulator & 0xffff0000U)
            | static_cast<std::uint16_t>(frame_count);
        const std::uint32_t returned = wrapped - 1U;
        frame = static_cast<std::int16_t>(frame_count - 1U);
        return bits_as_signed<std::int32_t>(returned);
    }

    case AnimationPlaybackMode::Hold:
        return packed_value(frame, fraction);

    case AnimationPlaybackMode::ClockPingPong: {
        const std::int32_t difference =
            static_cast<std::int32_t>(target_frame2)
            - static_cast<std::int32_t>(target_frame);
        if (difference == 0 || rate == 0) {
            return packed_value(frame, fraction);
        }
        const std::int32_t ticks_per_frame = 0x20000
            / static_cast<std::int32_t>(rate);
        if (ticks_per_frame == 0) {
            return packed_value(frame, fraction);
        }
        const std::int32_t ticks =
            (animation_clock - static_cast<std::int32_t>(mode3_clock))
            / ticks_per_frame;
        const std::int32_t period = difference > 0
            ? difference * 2
            : -difference * 2;
        std::int32_t position = ticks % period;
        if (difference > 0) {
            // This is x86 IDIV's signed remainder. Do not normalize a
            // negative phase into [0, period): the retail path keeps the
            // remainder's sign and consequently permits a pre-origin frame.
            if (difference < position) {
                position = period - position;
            }
            frame = narrow_i16(
                static_cast<std::int32_t>(target_frame) + position);
            return ticks / period;
        }
        if (-difference < position) {
            position = period - position;
        }
        frame = narrow_i16(
            static_cast<std::int32_t>(target_frame) - position);
        const std::int32_t quotient = ticks / period;
        // The retail return keeps the quotient's high word and overwrites
        // its low word with the resulting frame.
        return packed_with_low_word(
            static_cast<std::uint32_t>(quotient),
            frame);
    }

    case AnimationPlaybackMode::Reverse:
        if ((direction == 1 && endpoint <= frame)
            || (direction == -1 && frame <= endpoint)) {
            const std::uint16_t old_high =
                static_cast<std::uint16_t>(accumulator >> 16U);
            const std::int16_t reached =
                bits_as_signed<std::int16_t>(
                    static_cast<std::uint16_t>(endpoint));
            direction = narrow_i8(-direction);
            frame = reached;
            endpoint = narrow_i8(request_start);
            request_start = reached;
            return packed_with_low_word(
                static_cast<std::uint32_t>(old_high) << 16U,
                reached);
        }
        return packed_value(frame, fraction);
    }

    return packed_value(frame, fraction);
}

bool AnimationCursor::frame_reached(
    std::uint16_t animation,
    std::int16_t queried_frame) const noexcept {
    if (animation != old_anim) {
        return false;
    }
    if (old_frame <= new_frame) {
        return old_anim_dir >= 0
            ? old_frame <= queried_frame && queried_frame <= new_frame
            : queried_frame <= old_frame || queried_frame >= new_frame;
    }
    return old_anim_dir >= 0
        ? queried_frame <= new_frame || queried_frame >= old_frame
        : new_frame <= queried_frame && queried_frame <= old_frame;
}

} // namespace opentony::runtime
