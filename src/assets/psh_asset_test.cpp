#include "psh_asset.hpp"

#include "tests/test_check.hpp"
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

int main() {
    const std::string source =
        "#ifndef _TEST_PSH\n"
        "#define TESTPART_ROOT 0\n"
        "#define TESTPART_BOARD 1\n"
        "#define OTHERPART_SKIP 9\n";
    std::vector<std::byte> bytes;
    for (const char character : source) {
        bytes.push_back(static_cast<std::byte>(character));
    }
    const auto manifest = opentony::assets::PshManifest::parse(
        std::move(bytes), "TEST.PSH");
    CHECK(manifest.base_name() == "TEST");
    CHECK(manifest.parts().size() == 2);
    CHECK(manifest.parts()[0].name == "ROOT");
    CHECK(manifest.parts()[1].name == "BOARD");
    CHECK(manifest.parts()[1].model_name.empty());
    CHECK(manifest.parts()[1].index == 1);

    const std::filesystem::path root =
        "/home/joao/dev/OpenTony/build/assets/all-pkr/files/data";
    const auto hawk_path = root / "HAWK2.PSH";
    if (std::filesystem::is_regular_file(hawk_path)) {
        const auto hawk = opentony::assets::PshManifest::load(hawk_path.string());
        CHECK(hawk.parts().size() == 19);
        CHECK(hawk.parts()[0].index == 0);
        CHECK(hawk.parts()[1].name == "RIGHT_SHOE");
        CHECK(hawk.parts()[1].model_name == "HAWK");
        CHECK(!hawk.parts()[0].name.empty());
    }
    const auto animation_path = root / "SK2ANIM.PSH";
    if (std::filesystem::is_regular_file(animation_path)
        && std::filesystem::is_regular_file(hawk_path)) {
        const auto animation = opentony::assets::PshManifest::load(
            animation_path.string());
        const auto model = opentony::assets::PshManifest::load(hawk_path.string());
        const auto matches = opentony::assets::match_psh_parts(animation, model);
        CHECK(matches.size() == 19);
        CHECK(matches[0].animation_index == 0);
        CHECK(matches[0].model_index == 0);
        CHECK(matches[1].animation_index == 1);
        CHECK(matches[1].model_index == 3);
        CHECK(matches[2].model_index == 1);
        CHECK(matches[3].model_index == 2);
        CHECK(matches[18].model_index == 18);
    }
    const auto taxi_path = root / "C_TAXI.PSH";
    if (std::filesystem::is_regular_file(taxi_path)) {
        const auto taxi = opentony::assets::PshManifest::load(taxi_path.string());
        CHECK(taxi.parts().size() == 6);
    }
    if (std::filesystem::is_directory(root)) {
        std::size_t manifest_count = 0;
        for (const auto& entry : std::filesystem::directory_iterator(root)) {
            if (entry.path().extension() == ".PSH") {
                const auto manifest = opentony::assets::PshManifest::load(
                    entry.path().string());
                CHECK(!manifest.parts().empty());
                ++manifest_count;
            }
        }
        CHECK(manifest_count == 106);
    }
    return 0;
}
