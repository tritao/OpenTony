#include "tricks_bin.hpp"

#include <cassert>
#include <cstdint>
#include <vector>

namespace {

void put_i16(std::vector<std::uint8_t>& bytes, std::size_t offset, std::int16_t value) {
    const auto raw = static_cast<std::uint16_t>(value);
    bytes[offset] = static_cast<std::uint8_t>(raw & 0xffU);
    bytes[offset + 1] = static_cast<std::uint8_t>(raw >> 8U);
}

} // namespace

int main() {
    std::vector<std::uint8_t> bytes(0x60, 0);
    put_i16(bytes, 0x00, 0x10); // DAT_0056a894
    put_i16(bytes, 0x02, 0x20); // DAT_0056a840 metadata section
    put_i16(bytes, 0x04, 0x30);
    put_i16(bytes, 0x06, 0x40);
    put_i16(bytes, 0x08, 0x50);
    put_i16(bytes, 0x0a, 0x30);
    put_i16(bytes, 0x0c, 0x38);
    put_i16(bytes, 0x0e, 0x1c);
    put_i16(bytes, 0x10, 0x18); // lookup[0] -> image offset 0x18
    bytes[0x18] = 0x0f;
    bytes[0x19] = 0x01;
    bytes[0x1a] = 0x00;
    bytes[0x1b] = 0x02;
    bytes[0x1c] = 0x00;
    bytes[0x1d] = 0x03;
    bytes[0x1e] = 0x00;

    const auto view = opentony::assets::parse_tricks_bin(bytes);
    assert(view.valid);
    assert(view.header.action_offset_table_offset() == 0x10);
    assert(view.action_lookup(0).value() == 0x18);
    const auto stream = view.action_stream_for_lookup(0);
    assert(stream.has_value());
    assert(stream->front() == 0x0f);
    assert(stream->size() == bytes.size() - 0x18);

    const auto archive = opentony::assets::TricksBinArchive::parse(bytes);
    assert(archive.bytes().size() == bytes.size());
    assert(archive.view().action_stream_for_lookup(0)->front() == 0x0f);
    assert(archive.view().player_sequence_table()->size() == bytes.size() - 0x40);
    assert(archive.view().other_player_sequence_table()->size() == bytes.size() - 0x50);
    assert(archive.view().source_sequence_table()->size() == 2);
    assert(archive.view().alternate_sequence_table()->size() == 2);
    assert(archive.view().special_other_resource_table()->size() == bytes.size() - 0x1c);

    bytes[0] = 0xff;
    assert(!opentony::assets::parse_tricks_bin(bytes).valid);
    bool rejected = false;
    try {
        static_cast<void>(opentony::assets::TricksBinArchive::parse(bytes));
    } catch (const opentony::assets::TricksBinFormatError&) {
        rejected = true;
    }
    assert(rejected);
    return 0;
}
