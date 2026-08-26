#include "pre_asset.hpp"

#include <iostream>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: opentony_pre_inspect FILE.PRE\n";
        return 2;
    }
    try {
        const opentony::assets::PreArchive archive =
            opentony::assets::PreArchive::load(argv[1]);
        std::size_t payload_bytes = 0;
        for (const auto& entry : archive.entries()) {
            payload_bytes += entry.size;
        }
        std::cout << "entries=" << archive.entries().size()
                  << " payload_bytes=" << payload_bytes << '\n';
    } catch (const opentony::assets::PreFormatError& error) {
        std::cerr << "PRE error: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
