#include "psx_animation_runtime.hpp"

#include "tests/test_check.hpp"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

void put16(std::vector<std::byte>& bytes, std::size_t offset, std::uint16_t value) {
    bytes[offset] = static_cast<std::byte>(value & 0xffU);
    bytes[offset + 1] = static_cast<std::byte>(value >> 8U);
}

void put32(std::vector<std::byte>& bytes, std::size_t offset, std::uint32_t value) {
    put16(bytes, offset, static_cast<std::uint16_t>(value));
    put16(bytes, offset + 2, static_cast<std::uint16_t>(value >> 16U));
}

void test_mode_zero_endpoint_transition() {
    using opentony::assets::PsxAnimationPlaybackState;

    PsxAnimationPlaybackState playback;
    playback.start(5, 8, 0, 2, -1);
    playback.set_playback_rate_fixed(0x10000);
    playback.advance(0x100);
    playback.advance(0x100);
    CHECK(playback.current_frame() == 2);
    CHECK(!playback.finished());

    // The endpoint test runs before the next clock step. A signed -1 byte is
    // a finish sentinel, so the extra step stays clamped at frame 2.
    playback.advance(0x100);
    CHECK(playback.current_frame() == 2);
    CHECK(playback.end_frame() == 2);
    CHECK(playback.alternate_frame() == -1);
    CHECK(playback.direction() == 1);
    CHECK(playback.finished());

    // A fractional step crossing the endpoint still lands on the endpoint;
    // it must not clamp to endpoint - 1.
    playback.start(5, 8, 0, 2, -2);
    playback.set_playback_rate_fixed(0x18000);
    playback.advance(0x100);
    CHECK(playback.current_frame() == 1);
    CHECK(std::to_integer<std::uint8_t>(playback.raw()[0x104]) == 0x00);
    CHECK(std::to_integer<std::uint8_t>(playback.raw()[0x105]) == 0x80);
    playback.advance(0x100);
    CHECK(playback.current_frame() == 2);
    CHECK(!playback.finished());
    playback.advance(0x100);
    CHECK(playback.current_frame() == 2);
    CHECK(playback.alternate_frame() == -2);
    CHECK(playback.finished());

    // Positive alternate endpoints exchange with the reached endpoint and
    // reverse direction, preserving the inclusive endpoint frame.
    playback.start(5, 8, 0, 2, 1);
    playback.set_playback_rate_fixed(0x10000);
    playback.advance(0x100);
    playback.advance(0x100);
    playback.advance(0x100);
    CHECK(playback.current_frame() == 1);
    CHECK(playback.end_frame() == 1);
    CHECK(playback.alternate_frame() == 2);
    CHECK(playback.direction() == -1);
    CHECK(!playback.finished());
    playback.advance(0x100);
    CHECK(playback.current_frame() == 2);
    CHECK(playback.end_frame() == 2);
    CHECK(playback.alternate_frame() == 1);
    CHECK(playback.direction() == 1);

    // Mode 2 shares the pre-step endpoint transition but intentionally has
    // no mode-0 post-step clamp or ordinary frame advance.
    playback.set_mode(2);
    playback.advance(0x180);
    CHECK(playback.current_frame() == 2);
    CHECK(playback.end_frame() == 1);
    CHECK(playback.alternate_frame() == 2);
    CHECK(playback.direction() == -1);

    // Only -1 means "use the last frame" for request endpoints. Other
    // negative inputs clamp to zero after substitution.
    playback.start(5, 8, -2, -3, -1);
    CHECK(playback.current_frame() == 0);
    CHECK(playback.end_frame() == 0);
    CHECK(playback.direction() == 0);
    CHECK(playback.finished());
}

} // namespace

int main() {
    test_mode_zero_endpoint_transition();

    constexpr std::size_t first_tag = 0x10;
    constexpr std::size_t first_payload = first_tag + 8;
    constexpr std::size_t first_size = 4 + 2 * 8 + 3;
    constexpr std::size_t second_tag = first_payload + first_size;
    constexpr std::size_t second_payload = second_tag + 8;
    constexpr std::size_t end = second_payload + 4;
    std::vector<std::byte> bytes(end + 4, std::byte{0});
    put16(bytes, 0, 4);
    put16(bytes, 2, 2);
    put32(bytes, 4, static_cast<std::uint32_t>(first_tag));
    put32(bytes, first_tag, opentony::assets::kPsxCompressedAnimationTag);
    put32(bytes, first_tag + 4, static_cast<std::uint32_t>(first_size));
    put32(bytes, first_payload, 2);
    for (std::size_t index = 0; index < 16; ++index) {
        bytes[first_payload + 4 + index] = static_cast<std::byte>(0x10 + index);
    }
    bytes[first_payload + 20] = std::byte{0xa1};
    bytes[first_payload + 21] = std::byte{0xa2};
    bytes[first_payload + 22] = std::byte{0xa3};
    put32(bytes, second_tag, opentony::assets::kPsxHierarchyTag);
    put32(bytes, second_tag + 4, 4);
    bytes[second_payload + 0] = std::byte{0xb1};
    bytes[second_payload + 1] = std::byte{0xb2};
    bytes[second_payload + 2] = std::byte{0xb3};
    bytes[second_payload + 3] = std::byte{0xb4};
    put32(bytes, end, 0xffffffffU);

    const opentony::assets::PsxArchive archive =
        opentony::assets::PsxArchive::parse(std::move(bytes), "animation.psx");
    opentony::assets::PsxAnimationRuntime runtime;
    runtime.build(archive);
    CHECK(runtime.products().size() == 1);
    CHECK(runtime.products()[0].tag_type
        == opentony::assets::kPsxCompressedAnimationTag);
    CHECK(runtime.products()[0].records.size() == 2);
    CHECK(runtime.products()[0].records[1][7] == std::byte{0x1f});
    CHECK(runtime.products()[0].source_stream.size() == 3);
    CHECK(runtime.products()[0].source_stream[0] == std::byte{0xa1});
    CHECK(runtime.hierarchy_payload().size() == 4);
    CHECK(runtime.hierarchy_payload()[3] == std::byte{0xb4});

    const auto literal = opentony::assets::decode_psx_animation_channel(
        std::array<std::byte, 5>{
            std::byte{0x20}, std::byte{0x00}, std::byte{0x00},
            std::byte{0x06}, std::byte{0x00}},
        2);
    CHECK(literal.interpolation_count == 3);
    CHECK(literal.encoding == 0);
    CHECK((literal.samples == std::vector<std::int16_t>{0, 2, 4, 6}));
    CHECK(literal.consumed_bytes == 5);

    const auto packed = opentony::assets::decode_psx_animation_channel(
        std::array<std::byte, 4>{
            std::byte{0x01}, std::byte{100}, std::byte{0}, std::byte{0xc0}},
        2);
    CHECK((packed.samples == std::vector<std::int16_t>{100, 99}));
    CHECK(packed.consumed_bytes == 4);

    const auto repeated = opentony::assets::decode_psx_animation_channel(
        std::array<std::byte, 3>{std::byte{0x0e}, std::byte{7}, std::byte{0}}, 3);
    CHECK((repeated.samples == std::vector<std::int16_t>{7, 7, 7}));
    const auto zeroes = opentony::assets::decode_psx_animation_channel(
        std::array<std::byte, 1>{std::byte{0x0f}}, 2);
    CHECK((zeroes.samples == std::vector<std::int16_t>{0, 0}));

    opentony::assets::PsxAnimationPlaybackState playback;
    playback.start(7, 10, 0, 9);
    playback.set_playback_rate_fixed(0x10000);
    playback.advance(0x100);
    CHECK(playback.animation_index() == 7);
    CHECK(playback.current_frame() == 1);
    CHECK(playback.direction() == 1);

    // The retail multiply wraps to 32 bits before its arithmetic shift. This
    // deliberately overlarge signed scale therefore produces a zero step.
    opentony::assets::PsxAnimationPlaybackState wrap_probe;
    wrap_probe.start_special(7, 10, 1);
    wrap_probe.set_playback_rate_fixed(0x10000);
    wrap_probe.advance(static_cast<std::int32_t>(0xffff0000U));
    CHECK(wrap_probe.current_frame() == 0);

    playback.set_mode(1);
    for (int index = 0; index < 10; ++index) {
        playback.advance(0x100);
    }
    CHECK(playback.current_frame() == 1);

    playback.start(3, 8, 0, 3);
    playback.set_playback_rate_fixed(0x10000);
    playback.set_mode(4);
    for (int index = 0; index < 4; ++index) {
        playback.advance(0x100);
    }
    CHECK(playback.direction() == -1);
    CHECK(playback.end_frame() == 0);

    playback.start(4, 8, 0, 3);
    playback.set_mode(3);
    playback.set_playback_rate_fixed(0x10000);
    playback.set_pingpong_range(2, 4, 0);
    playback.advance(0x100, 2);
    CHECK(playback.current_frame() == 3);

    // The range calculation keeps the signed IDIV remainder. A pre-origin
    // clock is therefore one frame below the start, not one frame below the
    // end after modulo normalization.
    playback.advance(0x100, -2);
    CHECK(playback.current_frame() == 1);

    // Equal targets do not overwrite the ordinary accumulator in mode 3.
    playback.start(4, 8, 0, 3);
    playback.set_mode(3);
    playback.set_playback_rate_fixed(0x10000);
    playback.set_pingpong_range(5, 5, 0);
    playback.advance(0x100, 100);
    CHECK(playback.current_frame() == 0);
    return 0;
}
