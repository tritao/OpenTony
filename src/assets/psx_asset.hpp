#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace opentony::assets {

class PsxFormatError final : public std::runtime_error {
public:
    explicit PsxFormatError(const std::string& message)
        : std::runtime_error(message) {}
};

struct PsxObject {
    std::uint32_t flags{};
    std::array<std::int32_t, 3> position{};
    std::uint32_t unknown_1{};
    std::uint16_t unknown_2{};
    std::uint16_t model_index{};
    std::int16_t unknown_x{};
    std::int16_t unknown_y{};
    std::uint32_t unknown_3{};
    std::uint32_t unknown_rgbx{};
};

struct PsxFace {
    std::uint16_t flags{};
    std::array<std::uint16_t, 4> vertex_indices{};
    std::uint16_t normal_index{};
    std::uint16_t surface_flags{};
    // Packed little-endian word at the source face's +0x0c. Retail collision
    // code masks both halves of this word independently.
    std::uint32_t raw_collision_word{};
    std::uint32_t texture_index{};
    bool has_texture{};
    std::array<std::array<std::uint16_t, 2>, 4> uv{};
    std::uint8_t uv_count{};
};

struct PsxModel {
    std::size_t offset{};
    std::size_t size{};
    std::uint32_t flags{};
    std::uint32_t vertex_count{};
    std::uint32_t normal_count{};
    std::uint32_t face_count{};
    std::array<std::int16_t, 6> bounds{};
    std::vector<std::array<std::int16_t, 3>> vertices;
    std::vector<std::array<std::int16_t, 3>> normals;
    std::vector<PsxFace> faces;
};

struct PsxTag {
    std::size_t offset{};
    std::uint32_t type{};
    std::uint32_t size{};
};

struct PsxBlockmapCell {
    std::uint32_t unknown_1{};
    std::uint32_t unknown_2{};
    std::vector<std::uint32_t> object_indices;
};

struct PsxBlockmap {
    std::size_t tag_offset{};
    std::array<std::int32_t, 4> bounds{};
    std::array<std::uint16_t, 2> cell_counts{};
    std::vector<PsxBlockmapCell> cells;
};

struct PsxPalette {
    std::uint32_t name{};
    std::vector<std::uint16_t> colors;
};

struct PsxTexture {
    std::size_t offset{};
    std::uint32_t flags{};
    std::uint32_t color_count{};
    std::uint32_t palette_name{};
    std::uint32_t name_index{};
    std::uint16_t width{};
    std::uint16_t height{};
    std::size_t data_offset{};
    std::size_t data_size{};
};

struct PsxDecodedTexture {
    std::uint32_t width{};
    std::uint32_t height{};
    // Expanded RGB bytes in row-major PSX storage order.
    std::vector<std::uint8_t> rgb;
};

// The PSX container is the scene-side half of a TRG level. The parser keeps
// the exact fixed-point object records and model geometry, while deliberately
// leaving texture upload/material policy to the renderer.
class PsxArchive final {
public:
    static PsxArchive load(const std::string& path);
    static PsxArchive parse(std::vector<std::byte> bytes, std::string source = {});

    [[nodiscard]] std::uint16_t version() const noexcept { return version_; }
    [[nodiscard]] std::uint16_t marker() const noexcept { return marker_; }
    [[nodiscard]] std::size_t tag_offset() const noexcept { return tag_offset_; }
    [[nodiscard]] const std::vector<PsxObject>& objects() const noexcept { return objects_; }
    [[nodiscard]] const std::vector<PsxModel>& models() const noexcept { return models_; }
    [[nodiscard]] const std::vector<std::uint32_t>& model_names() const noexcept {
        return model_names_;
    }
    [[nodiscard]] const std::vector<PsxTag>& tags() const noexcept { return tags_; }
    [[nodiscard]] const std::vector<PsxBlockmap>& blockmaps() const noexcept {
        return blockmaps_;
    }
    [[nodiscard]] const std::vector<std::uint32_t>& texture_names() const noexcept {
        return texture_names_;
    }
    [[nodiscard]] const std::vector<PsxPalette>& palettes4() const noexcept {
        return palettes4_;
    }
    [[nodiscard]] const std::vector<PsxPalette>& palettes8() const noexcept {
        return palettes8_;
    }
    [[nodiscard]] const std::vector<PsxTexture>& textures() const noexcept {
        return textures_;
    }
    [[nodiscard]] PsxDecodedTexture decode_texture(
        std::size_t texture_index) const;
    [[nodiscard]] const std::vector<std::byte>& bytes() const noexcept { return bytes_; }

private:
    std::vector<std::byte> bytes_;
    std::string source_;
    std::uint16_t version_{};
    std::uint16_t marker_{};
    std::size_t tag_offset_{};
    std::vector<PsxObject> objects_;
    std::vector<PsxModel> models_;
    std::vector<std::uint32_t> model_names_;
    std::vector<PsxTag> tags_;
    std::vector<PsxBlockmap> blockmaps_;
    std::vector<std::uint32_t> texture_names_;
    std::vector<PsxPalette> palettes4_;
    std::vector<PsxPalette> palettes8_;
    std::vector<PsxTexture> textures_;
};

} // namespace opentony::assets
