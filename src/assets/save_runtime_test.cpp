#include "save_runtime.hpp"

#include <array>
#include "tests/test_check.hpp"
#include <cstddef>
#include <filesystem>
#include <vector>

int main() {
    using namespace opentony::assets;

    std::array<std::array<std::byte, kSaveHeaderBlockSize>, kSaveHeaderBlockCount> blocks{};
    blocks[0][0] = std::byte{0x44};
    std::vector<std::byte> career(kCareerRegisteredSize, std::byte{0x11});
    const SaveGameFile file = SaveGameFile::build(
        SaveActionType::career, "THPS2_CAREER", blocks, career, "career.sav");

    SaveManagerRuntime manager;
    const SaveFileCandidate candidate = SaveManagerRuntime::candidate_from("career.sav", file);
    const std::array<SaveFileCandidate, 2> candidates{
        candidate,
        SaveFileCandidate{"bad.sav", 13, SaveActionType::replay, "bad"},
    };
    manager.scan(candidates);
    CHECK(manager.ready());
    CHECK(manager.file_count() == 1);
    CHECK(manager.free_card_blocks() == 14);
    CHECK(manager.find_path("CAREER.SAV") != nullptr);
    CHECK(manager.find_display_name("thps2_career") != nullptr);
    CHECK(manager.file(0).action_type == SaveActionType::career);
    CHECK(std::string(SaveManagerRuntime::type_label(SaveActionType::custom_park))
        == "THPS2_PARK");

    manager.register_career_buffer(career);
    CHECK(manager.buffer_count() == 1);
    CHECK(manager.registered_payload_size() == kCareerRegisteredSize);
    CHECK(manager.required_card_blocks() == 1);
    const SaveGameFile rebuilt = manager.build_registered_file(
        SaveActionType::career, "THPS2_CAREER", blocks, "rebuilt.sav");
    CHECK(rebuilt.bytes().size() == kSaveCardBlockSize);
    CHECK(rebuilt.payload().size() == kCareerRegisteredSize);
    CHECK(rebuilt.payload()[0] == std::byte{0x11});

    CHECK(SaveManagerRuntime::make_filename(
        "HAWK", {'Q', 'Z'}, SaveActionType::career) == "THPS2_HQZKG.SAV");

    bool rejected = false;
    try {
        manager.register_buffer(std::vector<std::byte>(3, std::byte{0}));
    } catch (const SaveRuntimeError&) {
        rejected = true;
    }
    CHECK(rejected);

    const std::filesystem::path retail_save =
        "/home/joao/dev/OpenTony/build/assets/all-pkr/files/data/THPS2_HQZKG.SAV";
    (void)retail_save;
    return 0;
}
