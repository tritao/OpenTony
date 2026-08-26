#include "pc_texture_runtime.hpp"

#include <array>
#include <cassert>
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
    assert(external.material_count() == 89);
    assert(external.resolved_count() >= 4);
    assert(external.unresolved_count() + external.resolved_count()
        == external.material_count());

    std::size_t material_index = 0;
    assert(find_material(scene_runtime, 0x032bbb26U, material_index) != nullptr);
    const auto* record = external.record_for_material(
        material_index, 0x032bbb26U);
    assert(record != nullptr);
    assert(record->source_kind()
        == opentony::assets::PcTextureSourceKind::ExternalBitmap);
    assert(record->flags() == 0x1aU);
    const std::array<std::uint32_t, 2> empty_dimensions{0, 0};
    const std::array<std::uint32_t, 2> expected_dimensions{128, 128};
    assert(record->declared_dimensions() == empty_dimensions);
    assert(record->normalized_dimensions() == expected_dimensions);
    assert(record->source_dimensions() == expected_dimensions);
    assert(record->ready());
    assert(record->raw_record().size()
        == opentony::assets::kPcTextureRuntimeRecordSize);
    assert(record->source_path().find("032BBB26.BMP") != std::string::npos);
    assert(record->image().rgb.size() == 128U * 128U * 3U);
    const auto dimensions = external.dimensions_for_material(
        material_index, 0x032bbb26U);
    assert(dimensions.has_value());
    assert(*dimensions == expected_dimensions);
    assert(external.record_for_material(material_index, 0xdeadbeefU) == nullptr);

    const auto inline_archive =
        opentony::assets::PsxArchive::load(inline_path.string());
    const auto inline_runtime =
        opentony::assets::PsxRuntimeEnvironment::build(inline_archive);
    assert(!inline_archive.textures().empty());
    const auto inline_textures =
        opentony::assets::PcTextureRuntime::build_inline(inline_runtime);
    assert(inline_textures.resolved_count() > 0);
    const auto& inline_record = inline_textures.records().front();
    assert(inline_record.source_kind()
        == opentony::assets::PcTextureSourceKind::InlinePsx);
    assert(inline_record.flags() == 0x12U);
    assert(inline_record.ready());
    assert(inline_record.declared_dimensions()
        == inline_record.source_dimensions());
    assert(inline_record.image().rgb.size()
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
            assert(packaged_record->source_kind()
                == opentony::assets::PcTextureSourceKind::ExternalBitmap);
            assert(packaged_record->image().width == 128);
            assert(packaged_record->image().height == 128);
            assert(packaged_record->source_path().find("032BBB26.BMP")
                != std::string::npos);
        }
    }

    std::cout << "PC texture runtime tests passed\n";
}
