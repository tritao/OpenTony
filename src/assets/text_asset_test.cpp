#include "text_asset.hpp"

#include "tests/test_check.hpp"
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

int main() {
    const std::string labels = "0\t$First\r\n1\t$Second\r\n";
    std::vector<std::byte> label_bytes;
    for (const char character : labels) {
        label_bytes.push_back(static_cast<std::byte>(character));
    }
    // The retail park-label loader has a fixed 50-entry table; fill a compact
    // synthetic table while preserving the same tab/$ record grammar.
    label_bytes.clear();
    for (std::size_t index = 0; index < 50; ++index) {
        const std::string line = std::to_string(index) + "\t$Label "
            + std::to_string(index) + "\r\n";
        for (const char character : line) {
            label_bytes.push_back(static_cast<std::byte>(character));
        }
    }
    const auto labels_asset = opentony::assets::ParkLabelTable::parse(
        std::move(label_bytes), "cdparks.txt");
    CHECK(labels_asset.labels().size() == 50);
    CHECK(labels_asset.labels()[0].text == "Label 0");
    CHECK(labels_asset.labels()[49].index == 49);

    const std::string presentation =
        "@M2,0\r\n"
        "Title\r\n"
        "@F 1\r\n"
        "Alternate\r\n"
        "@B image.bmp\r\n"
        "#\r\n"
        "Ignored\r\n";
    std::vector<std::byte> presentation_bytes;
    for (const char character : presentation) {
        presentation_bytes.push_back(static_cast<std::byte>(character));
    }
    const auto text_asset = opentony::assets::PresentationTextAsset::parse(
        std::move(presentation_bytes), "credits.txt");
    CHECK(text_asset.records().size() == 4);
    CHECK(text_asset.records()[0].kind ==
        opentony::assets::PresentationTextRecordKind::marker);
    CHECK(text_asset.records()[0].marker_a == 2);
    CHECK(text_asset.records()[0].marker_b == 0);
    CHECK(text_asset.records()[1].text == "Title");
    CHECK(text_asset.records()[2].text == "Alternate");
    CHECK(text_asset.records()[2].alternate_font);
    CHECK(text_asset.records()[3].kind ==
        opentony::assets::PresentationTextRecordKind::bitmap);
    CHECK(text_asset.records()[3].text == "image.bmp");

    const std::filesystem::path root =
        "/home/joao/dev/OpenTony/build/assets/all-pkr/files/data";
    const auto park_path = root / "CDPARKS.TXT";
    if (std::filesystem::is_regular_file(park_path)) {
        const auto real = opentony::assets::ParkLabelTable::load(park_path.string());
        CHECK(real.labels().size() == 50);
        CHECK(real.labels()[0].text == "Up. Down. Repeat.");
    }
    const auto credits_path = root / "CREDITS.TXT";
    if (std::filesystem::is_regular_file(credits_path)) {
        const auto real = opentony::assets::PresentationTextAsset::load(
            credits_path.string());
        CHECK(real.records().size() == 716);
    }
    const auto music_path = root / "MUSIC.TXT";
    if (std::filesystem::is_regular_file(music_path)) {
        const auto real = opentony::assets::PresentationTextAsset::load(
            music_path.string());
        CHECK(real.records().size() == 401);
    }
    return 0;
}
