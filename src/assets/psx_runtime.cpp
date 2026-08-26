#include "psx_runtime.hpp"

#include <algorithm>
#include <stdexcept>

namespace opentony::assets {

std::uint16_t PsxRuntimeEnvironmentObject::u16(std::size_t offset) const noexcept {
    return static_cast<std::uint16_t>(
        std::to_integer<std::uint8_t>(bytes_[offset])
        | (static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes_[offset + 1])) << 8U));
}

std::uint32_t PsxRuntimeEnvironmentObject::u32(std::size_t offset) const noexcept {
    return static_cast<std::uint32_t>(
        std::to_integer<std::uint8_t>(bytes_[offset])
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes_[offset + 1])) << 8U)
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes_[offset + 2])) << 16U)
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes_[offset + 3])) << 24U));
}

std::int32_t PsxRuntimeEnvironmentObject::s32(std::size_t offset) const noexcept {
    return static_cast<std::int32_t>(u32(offset));
}

void PsxRuntimeEnvironmentObject::put16(
    std::size_t offset,
    std::uint16_t value) noexcept {
    bytes_[offset] = static_cast<std::byte>(value & 0xffU);
    bytes_[offset + 1] = static_cast<std::byte>(value >> 8U);
}

void PsxRuntimeEnvironmentObject::put32(
    std::size_t offset,
    std::uint32_t value) noexcept {
    put16(offset, static_cast<std::uint16_t>(value));
    put16(offset + 2, static_cast<std::uint16_t>(value >> 16U));
}

PsxRuntimeEnvironmentObject PsxRuntimeEnvironmentObject::from_source(
    std::size_t source_index,
    const PsxObject& source) {
    PsxRuntimeEnvironmentObject result{};
    result.source_index_ = source_index;

    // FUN_004b2450 copies the disk fields into the runtime record with the
    // four-byte object/vtable prefix.  The source +0x1c word is occupied by
    // the runtime link at +0x20, so unknown_3 is deliberately not copied.
    result.put32(0x04, source.flags);
    result.put32(0x08, static_cast<std::uint32_t>(source.position[0]));
    result.put32(0x0c, static_cast<std::uint32_t>(source.position[1]));
    result.put32(0x10, static_cast<std::uint32_t>(source.position[2]));
    result.put32(0x14, source.unknown_1);
    result.put16(0x18, source.unknown_2);
    result.put16(0x1a, source.model_index);
    result.put32(
        0x1c,
        static_cast<std::uint32_t>(static_cast<std::uint16_t>(source.unknown_x))
            | (static_cast<std::uint32_t>(static_cast<std::uint16_t>(source.unknown_y)) << 16U));
    result.put32(0x24, source.unknown_rgbx);
    result.put16(0x28, 0x1000);
    result.put16(0x2a, 0x1000);
    result.put16(0x2c, 0x1000);
    return result;
}

std::size_t PsxRuntimeEnvironmentObject::model_index() const noexcept {
    return u16(0x1a);
}

std::uint32_t PsxRuntimeEnvironmentObject::flags() const noexcept {
    return u32(0x04);
}

std::array<std::int32_t, 3> PsxRuntimeEnvironmentObject::position() const noexcept {
    return {s32(0x08), s32(0x0c), s32(0x10)};
}

std::uint32_t PsxRuntimeEnvironmentObject::source_word_at_14() const noexcept {
    return u32(0x14);
}

std::int16_t PsxRuntimeEnvironmentObject::source_transform_component() const noexcept {
    return static_cast<std::int16_t>(u16(0x18));
}

std::uint32_t PsxRuntimeEnvironmentObject::source_transform_tail() const noexcept {
    return u32(0x1c);
}

std::uint32_t PsxRuntimeEnvironmentObject::source_rgbx() const noexcept {
    return u32(0x24);
}

std::array<std::uint16_t, 3> PsxRuntimeEnvironmentObject::runtime_scale_q12() const noexcept {
    return {u16(0x28), u16(0x2a), u16(0x2c)};
}

std::uint8_t PsxRuntimeEnvironmentObject::model_feature_byte() const noexcept {
    return std::to_integer<std::uint8_t>(bytes_[0x19]);
}

void PsxRuntimeEnvironmentObject::set_flags(std::uint32_t value) noexcept {
    put32(0x04, value);
}

std::uint32_t PsxRuntimeMaterialRecord::u32(std::size_t offset) const noexcept {
    return static_cast<std::uint32_t>(
        std::to_integer<std::uint8_t>(bytes_[offset])
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes_[offset + 1])) << 8U)
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes_[offset + 2])) << 16U)
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes_[offset + 3])) << 24U));
}

void PsxRuntimeMaterialRecord::put32(
    std::size_t offset,
    std::uint32_t value) noexcept {
    bytes_[offset] = static_cast<std::byte>(value & 0xffU);
    bytes_[offset + 1] = static_cast<std::byte>((value >> 8U) & 0xffU);
    bytes_[offset + 2] = static_cast<std::byte>((value >> 16U) & 0xffU);
    bytes_[offset + 3] = static_cast<std::byte>((value >> 24U) & 0xffU);
}

std::uint32_t PsxRuntimeMaterialRecord::checksum() const noexcept {
    return u32(0x18);
}

std::uint32_t PsxRuntimeMaterialRecord::reference_count() const noexcept {
    return u32(0x10);
}

void PsxRuntimeMaterialTable::build(const PsxArchive& archive) {
    records_.clear();
    texture_material_indices_.clear();
    texture_material_indices_.reserve(archive.texture_names().size());

    for (const std::uint32_t checksum : archive.texture_names()) {
        const auto found = std::find_if(
            records_.begin(),
            records_.end(),
            [checksum](const PsxRuntimeMaterialRecord& record) {
                return record.checksum() == checksum;
            });
        if (found == records_.end()) {
            PsxRuntimeMaterialRecord record{};
            record.put32(0x18, checksum);
            records_.push_back(record);
            texture_material_indices_.push_back(records_.size() - 1);
        } else {
            texture_material_indices_.push_back(
                static_cast<std::size_t>(std::distance(records_.begin(), found)));
        }
    }

    for (const PsxModel& model : archive.models()) {
        for (const PsxFace& face : model.faces) {
            if (!face.has_texture) {
                continue;
            }
            const auto material_index = material_index_for_texture(face.texture_index);
            if (!material_index.has_value()) {
                throw PsxFormatError(
                    "PSX textured face references a missing material checksum");
            }
            PsxRuntimeMaterialRecord& material = records_[*material_index];
            material.put32(0x10, material.reference_count() + 1U);
        }
    }
}

const PsxRuntimeMaterialRecord& PsxRuntimeMaterialTable::record(
    std::size_t index) const {
    if (index >= records_.size()) {
        throw PsxFormatError("PSX runtime material index is outside the table");
    }
    return records_[index];
}

std::optional<std::size_t> PsxRuntimeMaterialTable::material_index_for_texture(
    std::size_t texture_index) const noexcept {
    if (texture_index >= texture_material_indices_.size()) {
        return std::nullopt;
    }
    return texture_material_indices_[texture_index];
}

std::optional<std::size_t> PsxRuntimeMaterialTable::texture_index_for_material(
    std::size_t material_index) const noexcept {
    const auto found = std::find(
        texture_material_indices_.begin(),
        texture_material_indices_.end(),
        material_index);
    if (found == texture_material_indices_.end()) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(
        std::distance(texture_material_indices_.begin(), found));
}

const PsxRuntimeMaterialRecord* PsxRuntimeMaterialTable::material_for_texture(
    std::size_t texture_index) const noexcept {
    const auto index = material_index_for_texture(texture_index);
    return index.has_value() ? &records_[*index] : nullptr;
}

PsxRuntimeEnvironment PsxRuntimeEnvironment::build(
    const PsxArchive& archive,
    std::uint8_t slot) {
    PsxRuntimeEnvironment result{};
    result.archive_ = &archive;
    result.slot_ = slot;
    result.objects_.reserve(archive.objects().size());
    for (std::size_t index = 0; index < archive.objects().size(); ++index) {
        result.objects_.push_back(
            PsxRuntimeEnvironmentObject::from_source(index, archive.objects()[index]));
    }
    result.model_pointers_.reserve(archive.models().size());
    for (const PsxModel& model : archive.models()) {
        result.model_pointers_.push_back(&model);
    }
    result.materials_.build(archive);
    result.animations_.build(archive);
    return result;
}

const PsxRuntimeEnvironmentObject& PsxRuntimeEnvironment::object(
    std::size_t index) const {
    if (index >= objects_.size()) {
        throw PsxFormatError("PSX runtime object index is outside the environment");
    }
    return objects_[index];
}

const PsxModel* PsxRuntimeEnvironment::model_pointer(std::size_t index) const {
    if (index >= model_pointers_.size()) {
        throw PsxFormatError("PSX runtime model index is outside the pointer table");
    }
    return model_pointers_[index];
}

const PsxModel& PsxRuntimeEnvironment::model_for_object(
    std::size_t object_index) const {
    const PsxRuntimeEnvironmentObject& current = object(object_index);
    return *model_pointer(current.model_index());
}

std::optional<std::array<std::uint32_t, 2>>
PsxRuntimeEnvironment::texture_dimensions_for_material(
    std::size_t material_index,
    std::uint32_t material_checksum) const noexcept {
    if (archive_ == nullptr
        || material_index >= materials_.records().size()
        || materials_.record(material_index).checksum() != material_checksum) {
        return std::nullopt;
    }
    const auto texture_index =
        materials_.texture_index_for_material(material_index);
    if (!texture_index.has_value() || *texture_index >= archive_->textures().size()) {
        return std::nullopt;
    }
    const PsxTexture& texture = archive_->textures()[*texture_index];
    if (texture.width == 0 || texture.height == 0) {
        return std::nullopt;
    }
    return std::array<std::uint32_t, 2>{texture.width, texture.height};
}

std::size_t PsxRuntimeEnvironment::object_record_offset(std::size_t index) const {
    if (index >= objects_.size()) {
        throw PsxFormatError("PSX runtime object index is outside the environment");
    }
    return sizeof(std::uint32_t) + index * kPsxRuntimeObjectStride;
}

} // namespace opentony::assets
