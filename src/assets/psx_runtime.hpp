#pragma once

#include "psx_asset.hpp"
#include "psx_animation_runtime.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace opentony::assets {

// The PC loader allocates a four-byte count prefix followed by one 0x4c-byte
// runtime record per PSX environment object.  Keep these constants public so
// callers can validate pointer arithmetic against the retail allocation.
inline constexpr std::size_t kPsxRuntimeObjectStride = 0x4c;
inline constexpr std::size_t kPsxRuntimeObjectDataOffset = 0x04;

// A byte-preserving view of the game-owned environment record.  The vtable
// and intrusive-list pointer at +0x00/+0x20 are intentionally not fabricated
// as host pointers; the recovered source-to-runtime fields are exposed as
// accessors below and the unresolved links remain zero in the raw image.
class PsxRuntimeEnvironmentObject final {
public:
    [[nodiscard]] std::size_t source_index() const noexcept { return source_index_; }
    [[nodiscard]] std::size_t model_index() const noexcept;
    [[nodiscard]] std::uint32_t flags() const noexcept;
    [[nodiscard]] std::array<std::int32_t, 3> position() const noexcept;
    [[nodiscard]] std::uint32_t source_word_at_14() const noexcept;
    [[nodiscard]] std::int16_t source_transform_component() const noexcept;
    [[nodiscard]] std::uint32_t source_transform_tail() const noexcept;
    [[nodiscard]] std::uint32_t source_rgbx() const noexcept;
    [[nodiscard]] std::array<std::uint16_t, 3> runtime_scale_q12() const noexcept;
    [[nodiscard]] std::uint8_t model_feature_byte() const noexcept;
    [[nodiscard]] std::span<const std::byte> raw_record() const noexcept {
        return bytes_;
    }

    void set_flags(std::uint32_t value) noexcept;

private:
    friend class PsxRuntimeEnvironment;

    static PsxRuntimeEnvironmentObject from_source(
        std::size_t source_index,
        const PsxObject& source);

    std::array<std::byte, kPsxRuntimeObjectStride> bytes_{};
    std::size_t source_index_{};

    [[nodiscard]] std::uint16_t u16(std::size_t offset) const noexcept;
    [[nodiscard]] std::uint32_t u32(std::size_t offset) const noexcept;
    [[nodiscard]] std::int32_t s32(std::size_t offset) const noexcept;
    void put16(std::size_t offset, std::uint16_t value) noexcept;
    void put32(std::size_t offset, std::uint32_t value) noexcept;
};

inline constexpr std::size_t kPsxRuntimeMaterialRecordSize = 0x2c;

// Shared material records created by the PSX finalizer. The retail object
// stores its checksum at +0x18, a reference count at +0x10, and intrusive
// links at +0x24/+0x28. The links are process-local pointers, so the native
// image keeps them zero while preserving the proven scalar fields.
class PsxRuntimeMaterialRecord final {
public:
    [[nodiscard]] std::uint32_t checksum() const noexcept;
    [[nodiscard]] std::uint32_t reference_count() const noexcept;
    [[nodiscard]] std::span<const std::byte> raw_record() const noexcept {
        return bytes_;
    }

private:
    friend class PsxRuntimeMaterialTable;
    std::array<std::byte, kPsxRuntimeMaterialRecordSize> bytes_{};

    [[nodiscard]] std::uint32_t u32(std::size_t offset) const noexcept;
    void put32(std::size_t offset, std::uint32_t value) noexcept;
};

// Native counterpart of the checksum/hash table populated while finalizing
// PSX model faces. Texture indices in a face address the source
// texture-name table; multiple source entries with the same checksum share
// one runtime record, matching the loader's hash-keyed material ownership.
class PsxRuntimeMaterialTable final {
public:
    void build(const PsxArchive& archive);

    [[nodiscard]] const std::vector<PsxRuntimeMaterialRecord>& records() const noexcept {
        return records_;
    }
    [[nodiscard]] const PsxRuntimeMaterialRecord& record(std::size_t index) const;
    [[nodiscard]] std::optional<std::size_t> material_index_for_texture(
        std::size_t texture_index) const noexcept;
    [[nodiscard]] std::optional<std::size_t> texture_index_for_material(
        std::size_t material_index) const noexcept;
    [[nodiscard]] const PsxRuntimeMaterialRecord* material_for_texture(
        std::size_t texture_index) const noexcept;

private:
    std::vector<PsxRuntimeMaterialRecord> records_;
    std::vector<std::size_t> texture_material_indices_;
};

// Native representation of the proven SKWARE.PSX parser/finalizer boundary:
// source objects become 0x4c-byte records, while model offsets become a
// relocated pointer array. The archive remains the owner of parsed models;
// this class owns the runtime records/material table and keeps the model
// pointer table as non-owning pointers into that stable archive.
class PsxRuntimeEnvironment final {
public:
    static PsxRuntimeEnvironment build(
        const PsxArchive& archive,
        std::uint8_t slot = 0);

    [[nodiscard]] std::uint8_t slot() const noexcept { return slot_; }
    [[nodiscard]] std::size_t allocation_size() const noexcept {
        return sizeof(std::uint32_t)
            + objects_.size() * kPsxRuntimeObjectStride;
    }
    [[nodiscard]] std::size_t object_count() const noexcept { return objects_.size(); }
    [[nodiscard]] std::size_t model_count() const noexcept { return model_pointers_.size(); }
    [[nodiscard]] const std::vector<PsxRuntimeEnvironmentObject>& objects() const noexcept {
        return objects_;
    }
    [[nodiscard]] const PsxRuntimeEnvironmentObject& object(std::size_t index) const;
    [[nodiscard]] const PsxModel* model_pointer(std::size_t index) const;
    [[nodiscard]] const PsxModel& model_for_object(std::size_t object_index) const;
    [[nodiscard]] const PsxRuntimeMaterialTable& materials() const noexcept {
        return materials_;
    }
    [[nodiscard]] std::optional<std::array<std::uint32_t, 2>>
    texture_dimensions_for_material(
        std::size_t material_index,
        std::uint32_t material_checksum) const noexcept;
    [[nodiscard]] const PsxAnimationRuntime& animations() const noexcept {
        return animations_;
    }
    [[nodiscard]] std::size_t object_record_offset(std::size_t index) const;
    [[nodiscard]] const PsxArchive& source_archive() const noexcept { return *archive_; }

private:
    const PsxArchive* archive_{};
    std::uint8_t slot_{};
    std::vector<PsxRuntimeEnvironmentObject> objects_;
    std::vector<const PsxModel*> model_pointers_;
    PsxRuntimeMaterialTable materials_;
    PsxAnimationRuntime animations_;
};

} // namespace opentony::assets
