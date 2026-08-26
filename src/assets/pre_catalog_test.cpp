#include "pre_catalog.hpp"

#include "tests/test_check.hpp"
#include <iostream>

int main() {
    CHECK(opentony::assets::PreAssetCatalog::key_for("level.pre") == "LEVEL");
    CHECK(opentony::assets::PreAssetCatalog::key_for("/data/PLAYER.PRE") == "PLAYER");
    std::cout << "PRE catalog tests passed\n";
}
