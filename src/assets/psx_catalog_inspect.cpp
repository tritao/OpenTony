#include "psx_catalog.hpp"

#include <array>
#include <iostream>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: opentony_psx_catalog_inspect ASSET_ROOT\n";
        return 2;
    }
    try {
        const opentony::assets::PsxAssetCatalog catalog =
            opentony::assets::PsxAssetCatalog::scan(argv[1]);
        constexpr std::array<std::string_view, 8> vehicle_names{
            "c_taxi", "c_police", "c_bus", "c_cable",
            "c_kart", "c_mar", "c_bull", "c_gull",
        };
        std::size_t vehicles = 0;
        for (const std::string_view name : vehicle_names) {
            if (catalog.contains(name)) {
                ++vehicles;
            }
        }
        std::cout << "psx_assets=" << catalog.size()
                  << " vehicle_assets=" << vehicles << '\n';
    } catch (const opentony::assets::PsxFormatError& error) {
        std::cerr << "PSX catalog error: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
