#include "psx_catalog.hpp"

#include <cassert>

int main() {
    using opentony::assets::PsxAssetCatalog;
    assert(PsxAssetCatalog::key_for("SkWare_L.PSX") == "SKWARE_L");
    assert(PsxAssetCatalog::key_for("c_taxi") == "C_TAXI");
    return 0;
}
