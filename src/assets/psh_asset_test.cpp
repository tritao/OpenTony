#include "psh_asset.hpp"

#include <cassert>
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
    assert(manifest.base_name() == "TEST");
    assert(manifest.parts().size() == 2);
    assert(manifest.parts()[0].name == "ROOT");
    assert(manifest.parts()[1].name == "BOARD");
    assert(manifest.parts()[1].model_name.empty());
    assert(manifest.parts()[1].index == 1);

    const std::filesystem::path root =
        "/home/joao/dev/OpenTony/build/assets/all-pkr/files/data";
    const auto hawk_path = root / "HAWK2.PSH";
    if (std::filesystem::is_regular_file(hawk_path)) {
        const auto hawk = opentony::assets::PshManifest::load(hawk_path.string());
        assert(hawk.parts().size() == 19);
        assert(hawk.parts()[0].index == 0);
        assert(hawk.parts()[1].name == "RIGHT_SHOE");
        assert(hawk.parts()[1].model_name == "HAWK");
        assert(!hawk.parts()[0].name.empty());
    }
    const auto animation_path = root / "SK2ANIM.PSH";
    if (std::filesystem::is_regular_file(animation_path)
        && std::filesystem::is_regular_file(hawk_path)) {
        const auto animation = opentony::assets::PshManifest::load(
            animation_path.string());
        const auto model = opentony::assets::PshManifest::load(hawk_path.string());
        const auto matches = opentony::assets::match_psh_parts(animation, model);
        assert(matches.size() == 19);
        assert(matches[0].animation_index == 0);
        assert(matches[0].model_index == 0);
        assert(matches[1].animation_index == 1);
        assert(matches[1].model_index == 3);
        assert(matches[2].model_index == 1);
        assert(matches[3].model_index == 2);
        assert(matches[18].model_index == 18);
    }
    const auto taxi_path = root / "C_TAXI.PSH";
    if (std::filesystem::is_regular_file(taxi_path)) {
        const auto taxi = opentony::assets::PshManifest::load(taxi_path.string());
        assert(taxi.parts().size() == 6);
    }
    if (std::filesystem::is_directory(root)) {
        std::size_t manifest_count = 0;
        for (const auto& entry : std::filesystem::directory_iterator(root)) {
            if (entry.path().extension() == ".PSH") {
                const auto manifest = opentony::assets::PshManifest::load(
                    entry.path().string());
                assert(!manifest.parts().empty());
                ++manifest_count;
            }
        }
        assert(manifest_count == 106);
    }
    return 0;
}
