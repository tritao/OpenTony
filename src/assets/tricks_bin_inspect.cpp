#include "tricks_bin.hpp"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string_view>

int main(int argc, char** argv) {
    if (argc != 2 && argc != 3) {
        std::cerr << "usage: opentony_tricks_bin_inspect FILE [LOOKUP_INDEX]\n";
        return 2;
    }
    try {
        const auto archive = opentony::assets::TricksBinArchive::load(argv[1]);
        const auto view = archive.view();
        std::size_t index = 0;
        if (argc == 3) {
            const std::string_view value = argv[2];
            std::size_t parsed = 0;
            if (value.empty()) {
                throw opentony::assets::TricksBinFormatError(
                    "lookup index must be an unsigned decimal value");
            }
            for (const char digit : value) {
                if (digit < '0' || digit > '9') {
                    throw opentony::assets::TricksBinFormatError(
                        "lookup index must be an unsigned decimal value");
                }
                const std::size_t digit_value = static_cast<std::size_t>(digit - '0');
                if (parsed > (std::numeric_limits<std::size_t>::max() - digit_value) / 10U) {
                    throw opentony::assets::TricksBinFormatError(
                        "lookup index is too large");
                }
                parsed = parsed * 10U + digit_value;
            }
            index = parsed;
        }
        const auto lookup = view.action_lookup(index);
        const auto input_table = view.player_input_sequence_table(index);
        const auto stream = view.action_stream_for_lookup(index);
        std::cout << "bytes=" << archive.bytes().size()
                  << " offset_table=0x" << std::hex
                  << static_cast<std::uint16_t>(view.header.action_offset_table_offset())
                  << " metadata=0x"
                  << static_cast<std::uint16_t>(view.header.action_metadata_offset())
                  << std::dec
                  << " index=" << index
                  << " table_relative=" << (lookup.has_value() ? *lookup : 0)
                  << " input_table_bytes="
                  << (input_table.has_value() ? input_table->size() : 0)
                  << " image_tail_bytes="
                  << (stream.has_value() ? stream->size() : 0)
                  << '\n';
    } catch (const opentony::assets::TricksBinFormatError& error) {
        std::cerr << "TRICKS.BIN error: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
