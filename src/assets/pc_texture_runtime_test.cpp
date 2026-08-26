#include "pc_texture_runtime.hpp"

#include <array>
#include "tests/test_check.hpp"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>

namespace {

const std::filesystem::path kDataRoot =
    "/home/joao/dev/OpenTony/build/assets/all-pkr/files/data";

const opentony::assets::PsxRuntimeMaterialRecord* find_material(
    const opentony::assets::PsxRuntimeEnvironment& runtime,
    std::uint32_t checksum,
    std::size_t& index) {
    for (index = 0; index < runtime.materials().records().size(); ++index) {
        const auto& material = runtime.materials().record(index);
        if (material.checksum() == checksum) {
            return &material;
        }
    }
    return nullptr;
}

} // namespace

int main() {
    const std::filesystem::path scene_path = kDataRoot / "SKWARE.PSX";
    const std::filesystem::path inline_path = kDataRoot / "DEFAULT.PSX";
    if (!std::filesystem::is_regular_file(scene_path)
        || !std::filesystem::is_regular_file(inline_path)) {
        std::cout << "fixture unavailable\n";
        return 0;
    }

    const auto scene_archive =
        opentony::assets::PsxArchive::load(scene_path.string());
    const auto scene_runtime =
        opentony::assets::PsxRuntimeEnvironment::build(scene_archive);
    auto external = opentony::assets::PcTextureRuntime::build_external(
        scene_runtime, kDataRoot);
    CHECK(external.material_count() == 89);
    CHECK(external.resolved_count() >= 4);
    CHECK(external.unresolved_count() + external.resolved_count()
        == external.material_count());

    std::size_t material_index = 0;
    CHECK(find_material(scene_runtime, 0x032bbb26U, material_index) != nullptr);
    const auto* record = external.record_for_material(
        material_index, 0x032bbb26U);
    CHECK(record != nullptr);
    CHECK(record->source_kind()
        == opentony::assets::PcTextureSourceKind::ExternalBitmap);
    CHECK(record->flags() == 0x1aU);
    const std::array<std::uint32_t, 2> empty_dimensions{0, 0};
    const std::array<std::uint32_t, 2> expected_dimensions{128, 128};
    CHECK(record->declared_dimensions() == empty_dimensions);
    CHECK(record->normalized_dimensions() == expected_dimensions);
    CHECK(record->source_dimensions() == expected_dimensions);
    CHECK(record->ready());
    CHECK(record->raw_record().size()
        == opentony::assets::kPcTextureRuntimeRecordSize);
    CHECK(record->source_path().find("032BBB26.BMP") != std::string::npos);
    CHECK(record->image().rgb.size() == 128U * 128U * 3U);
    const auto dimensions = external.dimensions_for_material(
        material_index, 0x032bbb26U);
    CHECK(dimensions.has_value());
    CHECK(*dimensions == expected_dimensions);
    CHECK(external.record_for_material(material_index, 0xdeadbeefU) == nullptr);

    const auto inline_archive =
        opentony::assets::PsxArchive::load(inline_path.string());
    const auto inline_runtime =
        opentony::assets::PsxRuntimeEnvironment::build(inline_archive);
    CHECK(!inline_archive.textures().empty());
    const auto inline_textures =
        opentony::assets::PcTextureRuntime::build_inline(inline_runtime);
    CHECK(inline_textures.resolved_count() > 0);
    const auto& inline_record = inline_textures.records().front();
    CHECK(inline_record.source_kind()
        == opentony::assets::PcTextureSourceKind::InlinePsx);
    CHECK(inline_record.flags() == 0x12U);
    CHECK(inline_record.ready());
    CHECK(inline_record.declared_dimensions()
        == inline_record.source_dimensions());
    CHECK(inline_record.image().rgb.size()
        == static_cast<std::size_t>(inline_record.image().width)
            * inline_record.image().height * 3U);

    const std::filesystem::path package_path =
        "/home/joao/dev/OpenTony/build/disc/files/SETUP/data/ALL.PKR";
    if (std::filesystem::is_regular_file(package_path)) {
        const auto package =
            opentony::assets::PkrArchive::load(package_path.string());
        const auto packaged = opentony::assets::PcTextureRuntime::build_external(
            scene_runtime, package);
        const auto* packaged_record = packaged.record_for_material(
            material_index, 0x032bbb26U);
        // The retail ALL.PKR index used by this fixture keeps the four
        // Warehouse NEWTEX images outside the package. If a package variant
        // includes them, validate the same material/image bridge there too.
        if (packaged_record != nullptr) {
            CHECK(packaged_record->source_kind()
                == opentony::assets::PcTextureSourceKind::ExternalBitmap);
            CHECK(packaged_record->image().width == 128);
            CHECK(packaged_record->image().height == 128);
            CHECK(packaged_record->source_path().find("032BBB26.BMP")
                != std::string::npos);
        }
    }

    std::cout << "PC texture runtime tests passed\n";
}
