#include "pre_catalog.hpp"

#include <cassert>
#include <iostream>

int main() {
    assert(opentony::assets::PreAssetCatalog::key_for("level.pre") == "LEVEL");
    assert(opentony::assets::PreAssetCatalog::key_for("/data/PLAYER.PRE") == "PLAYER");
    std::cout << "PRE catalog tests passed\n";
}
