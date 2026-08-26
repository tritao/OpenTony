#include "psx_catalog.hpp"

#include "tests/test_check.hpp"

int main() {
    using opentony::assets::PsxAssetCatalog;
    CHECK(PsxAssetCatalog::key_for("SkWare_L.PSX") == "SKWARE_L");
    CHECK(PsxAssetCatalog::key_for("c_taxi") == "C_TAXI");
    return 0;
}
