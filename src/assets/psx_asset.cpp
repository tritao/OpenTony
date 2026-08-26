#include "psx_asset.hpp"

#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>
#include <cmath>
#include <utility>

namespace opentony::assets {
namespace {

[[nodiscard]] std::string at_label(const std::string& source, std::size_t offset) {
    std::ostringstream stream;
    stream << (source.empty() ? "PSX" : source) << " at 0x" << std::hex << offset;
    return stream.str();
}

void require_range(
    std::size_t offset,
    std::size_t length,
    std::size_t size,
    const std::string& source) {
    if (offset > size || length > size - offset) {
        throw PsxFormatError(at_label(source, offset) + " seeks outside the file");
    }
}

[[nodiscard]] std::uint16_t u16(
    const std::vector<std::byte>& bytes,
    std::size_t offset,
    const std::string& source) {
    require_range(offset, 2, bytes.size(), source);
    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes[offset]))
        | (static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes[offset + 1])) << 8U));
}

[[nodiscard]] std::uint32_t u32(
    const std::vector<std::byte>& bytes,
    std::size_t offset,
    const std::string& source) {
    require_range(offset, 4, bytes.size(), source);
    return static_cast<std::uint32_t>(
        static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset]))
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 1])) << 8U)
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 2])) << 16U)
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 3])) << 24U));
}

[[nodiscard]] std::int16_t i16(
    const std::vector<std::byte>& bytes,
    std::size_t offset,
    const std::string& source) {
    return static_cast<std::int16_t>(u16(bytes, offset, source));
}

[[nodiscard]] std::int32_t i32(
    const std::vector<std::byte>& bytes,
    std::size_t offset,
    const std::string& source) {
    return static_cast<std::int32_t>(u32(bytes, offset, source));
}

[[nodiscard]] std::size_t checked_add(
    std::size_t left,
    std::size_t right,
    const std::string& source) {
    if (right > std::numeric_limits<std::size_t>::max() - left) {
        throw PsxFormatError(source + " offset arithmetic overflow");
    }
    return left + right;
}

[[nodiscard]] std::size_t checked_count(
    std::uint32_t count,
    std::size_t element_size,
    std::size_t remaining,
    const std::string& label) {
    if (element_size != 0 && count > remaining / element_size) {
        throw PsxFormatError(label + " is unreasonably large");
    }
    return static_cast<std::size_t>(count);
}

[[nodiscard]] std::size_t model_header_size(std::uint16_t version) {
    return version >= 4 ? 28U : 32U;
}

[[nodiscard]] std::array<std::uint8_t, 3> psx_color_to_rgb(
    std::uint16_t color) noexcept {
    const auto expand = [](std::uint16_t component) {
        return static_cast<std::uint8_t>(std::lround(
            static_cast<double>(component & 0x1fU) * 255.0 / 31.0));
    };
    return {
        expand(color),
        expand(static_cast<std::uint16_t>(color >> 5U)),
        expand(static_cast<std::uint16_t>(color >> 10U)),
    };
}

void skip(const std::vector<std::byte>& bytes, std::size_t& cursor, std::size_t amount, const std::string& source) {
    require_range(cursor, amount, bytes.size(), source);
    cursor += amount;
}

[[nodiscard]] std::vector<PsxTag> read_tags(
    const std::vector<std::byte>& bytes,
    std::size_t offset,
    std::size_t& post_offset,
    const std::string& source) {
    std::vector<PsxTag> tags;
    if (offset == bytes.size()) {
        post_offset = offset;
        return tags;
    }
    std::size_t cursor = offset;
    while (cursor < bytes.size()) {
        const std::size_t tag_start = cursor;
        const std::uint32_t type = u32(bytes, cursor, source);
        cursor += 4;
        if (type == 0xffffffffU) {
            post_offset = cursor;
            return tags;
        }
        const std::uint32_t size = u32(bytes, cursor, source);
        cursor += 4;
        const std::size_t end = checked_add(cursor, static_cast<std::size_t>(size), source);
        require_range(cursor, size, bytes.size(), source);
        cursor = end;
        tags.push_back(PsxTag{tag_start, type, size});
    }
    post_offset = cursor;
    return tags;
}

[[nodiscard]] PsxBlockmap read_blockmap(
    const std::vector<std::byte>& bytes,
    const PsxTag& tag,
    std::size_t object_count,
    const std::string& source) {
    const std::size_t payload = checked_add(tag.offset, 8, source);
    require_range(payload, tag.size, bytes.size(), source);
    std::size_t cursor = payload;
    const std::size_t end = payload + tag.size;
    PsxBlockmap result{};
    result.tag_offset = tag.offset;
    for (std::int32_t& bound : result.bounds) {
        bound = i32(bytes, cursor, source);
        cursor += 4;
    }
    result.cell_counts = {u16(bytes, cursor, source), u16(bytes, cursor + 2, source)};
    cursor += 4;
    if (result.cell_counts[0] == 0 || result.cell_counts[1] == 0) {
        throw PsxFormatError("PSX blockmap has an empty grid");
    }
    const std::size_t cell_count = static_cast<std::size_t>(result.cell_counts[0])
        * static_cast<std::size_t>(result.cell_counts[1]);
    result.cells.reserve(cell_count);
    for (std::size_t cell = 0; cell < cell_count; ++cell) {
        require_range(cursor, 12, end, source);
        PsxBlockmapCell entry{};
        entry.unknown_1 = u32(bytes, cursor, source);
        entry.unknown_2 = u32(bytes, cursor + 4, source);
        const std::uint32_t reference_count = u32(bytes, cursor + 8, source);
        cursor += 12;
        if (reference_count > (end - cursor) / 4U) {
            throw PsxFormatError("PSX blockmap reference list is unreasonably large");
        }
        entry.object_indices.reserve(static_cast<std::size_t>(reference_count));
        for (std::uint32_t index = 0; index < reference_count; ++index) {
            const std::uint32_t object_index = u32(bytes, cursor, source);
            cursor += 4;
            if (object_index >= object_count) {
                throw PsxFormatError("PSX blockmap references a missing object");
            }
            entry.object_indices.push_back(object_index);
        }
        if (u32(bytes, cursor, source) != 0) {
            throw PsxFormatError("PSX blockmap cell has a non-zero terminator");
        }
        cursor += 4;
        result.cells.push_back(std::move(entry));
    }
    if (cursor != end) {
        throw PsxFormatError("PSX blockmap has trailing bytes");
    }
    return result;
}

} // namespace

PsxArchive PsxArchive::load(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw PsxFormatError("cannot open PSX file: " + path);
    }
    input.seekg(0, std::ios::end);
    const std::streamoff length = input.tellg();
    if (length < 0) {
        throw PsxFormatError("cannot determine PSX file size: " + path);
    }
    input.seekg(0, std::ios::beg);
    std::vector<std::byte> bytes(static_cast<std::size_t>(length));
    if (!bytes.empty()) {
        input.read(reinterpret_cast<char*>(bytes.data()), length);
        if (!input) {
            throw PsxFormatError("cannot read PSX file: " + path);
        }
    }
    return parse(std::move(bytes), path);
}

PsxArchive PsxArchive::parse(std::vector<std::byte> bytes, std::string source) {
    if (bytes.size() < 12) {
        throw PsxFormatError("PSX file is shorter than its 12-byte header");
    }
    PsxArchive archive{};
    archive.bytes_ = std::move(bytes);
    archive.source_ = std::move(source);
    archive.version_ = u16(archive.bytes_, 0, archive.source_);
    archive.marker_ = u16(archive.bytes_, 2, archive.source_);
    archive.tag_offset_ = u32(archive.bytes_, 4, archive.source_);
    const std::uint32_t object_count = u32(archive.bytes_, 8, archive.source_);
    if (archive.version_ != 3 && archive.version_ != 4 && archive.version_ != 6) {
        throw PsxFormatError("unsupported PSX version");
    }
    if (archive.marker_ != 2) {
        throw PsxFormatError("unsupported PSX marker");
    }
    require_range(archive.tag_offset_, 0, archive.bytes_.size(), archive.source_);
    const std::size_t object_count_size = checked_count(
        object_count,
        36,
        archive.bytes_.size() - 12,
        "PSX object table");

    std::size_t cursor = 12;
    archive.objects_.reserve(object_count_size);
    for (std::uint32_t index = 0; index < object_count; ++index) {
        require_range(cursor, 36, archive.bytes_.size(), archive.source_);
        PsxObject object{};
        object.flags = u32(archive.bytes_, cursor, archive.source_);
        cursor += 4;
        for (std::int32_t& value : object.position) {
            value = i32(archive.bytes_, cursor, archive.source_);
            cursor += 4;
        }
        object.unknown_1 = u32(archive.bytes_, cursor, archive.source_);
        object.unknown_2 = u16(archive.bytes_, cursor + 4, archive.source_);
        object.model_index = u16(archive.bytes_, cursor + 6, archive.source_);
        object.unknown_x = i16(archive.bytes_, cursor + 8, archive.source_);
        object.unknown_y = i16(archive.bytes_, cursor + 10, archive.source_);
        object.unknown_3 = u32(archive.bytes_, cursor + 12, archive.source_);
        object.unknown_rgbx = u32(archive.bytes_, cursor + 16, archive.source_);
        cursor += 20;
        archive.objects_.push_back(object);
    }

    const std::uint32_t model_count = u32(archive.bytes_, cursor, archive.source_);
    cursor += 4;
    const std::size_t model_count_size = checked_count(
        model_count,
        4,
        archive.bytes_.size() - cursor,
        "PSX model table");
    std::vector<std::size_t> model_offsets;
    model_offsets.reserve(model_count_size);
    for (std::uint32_t index = 0; index < model_count; ++index) {
        const std::size_t offset = u32(archive.bytes_, cursor, archive.source_);
        cursor += 4;
        if (offset < cursor || offset >= archive.bytes_.size()) {
            throw PsxFormatError("PSX model offset is outside the model table");
        }
        if (!model_offsets.empty() && offset <= model_offsets.back()) {
            throw PsxFormatError("PSX model offsets are not strictly increasing");
        }
        model_offsets.push_back(offset);
    }
    for (const PsxObject& object : archive.objects_) {
        if (object.model_index >= model_count) {
            throw PsxFormatError("PSX object references a missing model");
        }
    }

    archive.models_.reserve(model_offsets.size());
    for (std::size_t model_index = 0; model_index < model_offsets.size(); ++model_index) {
        const std::size_t model_start = model_offsets[model_index];
        const std::size_t model_end = model_index + 1 < model_offsets.size()
            ? model_offsets[model_index + 1]
            : archive.tag_offset_;
        if (model_end <= model_start || model_end > archive.bytes_.size()) {
            throw PsxFormatError("PSX model has an invalid boundary");
        }
        std::size_t model_cursor = model_start;
        PsxModel model{};
        model.offset = model_start;
        require_range(
            model_cursor,
            model_header_size(archive.version_),
            model_end,
            archive.source_);
        if (archive.version_ >= 4) {
            model.flags = u16(archive.bytes_, model_cursor, archive.source_);
            model.vertex_count = u16(archive.bytes_, model_cursor + 2, archive.source_);
            model.normal_count = u16(archive.bytes_, model_cursor + 4, archive.source_);
            model.face_count = u16(archive.bytes_, model_cursor + 6, archive.source_);
            model_cursor += 8;
        } else {
            model.flags = u32(archive.bytes_, model_cursor, archive.source_);
            model.vertex_count = u32(archive.bytes_, model_cursor + 4, archive.source_);
            model.normal_count = u32(archive.bytes_, model_cursor + 8, archive.source_);
            model.face_count = u32(archive.bytes_, model_cursor + 12, archive.source_);
            model_cursor += 16;
        }
        (void)u32(archive.bytes_, model_cursor, archive.source_);
        model_cursor += 4;
        for (std::int16_t& bound : model.bounds) {
            bound = i16(archive.bytes_, model_cursor, archive.source_);
            model_cursor += 2;
        }
        (void)u32(archive.bytes_, model_cursor, archive.source_);
        model_cursor += 4;
        const std::size_t model_bytes = model_end - model_start;
        if (model.vertex_count > model_bytes / 8U || model.normal_count > model_bytes / 8U) {
            throw PsxFormatError("PSX model vertex/normal count is unreasonably large");
        }
        model.vertices.reserve(static_cast<std::size_t>(model.vertex_count));
        for (std::uint32_t index = 0; index < model.vertex_count; ++index) {
            require_range(model_cursor, 8, model_end, archive.source_);
            model.vertices.push_back({
                i16(archive.bytes_, model_cursor, archive.source_),
                i16(archive.bytes_, model_cursor + 2, archive.source_),
                i16(archive.bytes_, model_cursor + 4, archive.source_),
            });
            model_cursor += 8;
        }
        model.normals.reserve(static_cast<std::size_t>(model.normal_count));
        for (std::uint32_t index = 0; index < model.normal_count; ++index) {
            require_range(model_cursor, 8, model_end, archive.source_);
            model.normals.push_back({
                i16(archive.bytes_, model_cursor, archive.source_),
                i16(archive.bytes_, model_cursor + 2, archive.source_),
                i16(archive.bytes_, model_cursor + 4, archive.source_),
            });
            model_cursor += 8;
        }
        if (model.face_count > model_bytes / 4U) {
            throw PsxFormatError("PSX model face count is unreasonably large");
        }
        model.faces.reserve(static_cast<std::size_t>(model.face_count));
        for (std::uint32_t index = 0; index < model.face_count; ++index) {
            const std::size_t face_start = model_cursor;
            require_range(model_cursor, 4, model_end, archive.source_);
            PsxFace face{};
            face.flags = u16(archive.bytes_, model_cursor, archive.source_);
            const std::size_t face_length = u16(archive.bytes_, model_cursor + 2, archive.source_);
            model_cursor += 4;
            const std::size_t face_end = checked_add(face_start, face_length, archive.source_);
            if (face_length < 4 || face_end > model_end) {
                throw PsxFormatError("PSX face has an invalid length");
            }
            if (archive.version_ >= 4) {
                for (std::uint16_t& vertex : face.vertex_indices) {
                    require_range(model_cursor, 1, model_end, archive.source_);
                    vertex = std::to_integer<std::uint8_t>(archive.bytes_[model_cursor]);
                    if (vertex >= model.vertex_count) {
                        throw PsxFormatError("PSX face references a missing vertex");
                    }
                    model_cursor += 1;
                }
            } else {
                for (std::uint16_t& vertex : face.vertex_indices) {
                    vertex = u16(archive.bytes_, model_cursor, archive.source_);
                    if (vertex >= model.vertex_count) {
                        throw PsxFormatError("PSX face references a missing vertex");
                    }
                    model_cursor += 2;
                }
            }
            skip(archive.bytes_, model_cursor, 4, archive.source_);
            face.normal_index = u16(archive.bytes_, model_cursor, archive.source_);
            model_cursor += 2;
            if (face.normal_index >= model.normal_count) {
                throw PsxFormatError("PSX face references a missing normal");
            }
            face.surface_flags = u16(archive.bytes_, model_cursor, archive.source_);
            model_cursor += 2;
            face.raw_collision_word =
                static_cast<std::uint32_t>(face.normal_index)
                | (static_cast<std::uint32_t>(face.surface_flags) << 16U);
            if ((model.flags & 1U) == 0U && (face.flags & 2U) != 0U) {
                face.texture_index = u32(archive.bytes_, model_cursor, archive.source_);
                face.has_texture = true;
                model_cursor += 4;
            }
            if ((face.flags & 1U) != 0U) {
                face.uv_count = 4;
                if (archive.version_ >= 6) {
                    for (auto& uv : face.uv) {
                        uv[0] = u16(archive.bytes_, model_cursor, archive.source_);
                        model_cursor += 2;
                    }
                    for (auto& uv : face.uv) {
                        uv[1] = u16(archive.bytes_, model_cursor, archive.source_);
                        model_cursor += 2;
                    }
                } else {
                    for (auto& uv : face.uv) {
                        uv[0] = std::to_integer<std::uint8_t>(archive.bytes_[model_cursor++]);
                    }
                    for (auto& uv : face.uv) {
                        uv[1] = std::to_integer<std::uint8_t>(archive.bytes_[model_cursor++]);
                    }
                }
            }
            if ((face.flags & 8U) != 0U) {
                skip(archive.bytes_, model_cursor, 8, archive.source_);
            }
            if ((model.flags & 1U) == 0U && (face.flags & 0x20U) != 0U) {
                skip(archive.bytes_, model_cursor, 4, archive.source_);
            }
            if (model_cursor > face_end) {
                throw PsxFormatError("PSX face fields exceed its declared length");
            }
            model_cursor = face_end;
            model.faces.push_back(face);
        }
        if (model_cursor > model_end) {
            throw PsxFormatError("PSX model exceeds its boundary");
        }
        model.size = model_end - model_start;
        archive.models_.push_back(std::move(model));
    }

    std::size_t post_offset = archive.tag_offset_;
    archive.tags_ = read_tags(archive.bytes_, archive.tag_offset_, post_offset, archive.source_);
    for (const PsxTag& tag : archive.tags_) {
        if (tag.type == 0x0000000aU && tag.size >= 36U) {
            archive.blockmaps_.push_back(
                read_blockmap(archive.bytes_, tag, archive.objects_.size(), archive.source_));
        }
    }
    if (post_offset < archive.bytes_.size() && model_count != 0) {
        require_range(post_offset, static_cast<std::size_t>(model_count) * 4U, archive.bytes_.size(), archive.source_);
        archive.model_names_.reserve(static_cast<std::size_t>(model_count));
        for (std::uint32_t index = 0; index < model_count; ++index) {
            archive.model_names_.push_back(u32(archive.bytes_, post_offset, archive.source_));
            post_offset += 4;
        }
        const std::uint32_t texture_name_count = u32(archive.bytes_, post_offset, archive.source_);
        post_offset += 4;
        (void)checked_count(
            texture_name_count,
            4,
            archive.bytes_.size() - post_offset,
            "PSX texture-name table");
        archive.texture_names_.reserve(static_cast<std::size_t>(texture_name_count));
        for (std::uint32_t index = 0; index < texture_name_count; ++index) {
            archive.texture_names_.push_back(u32(archive.bytes_, post_offset, archive.source_));
            post_offset += 4;
        }
        const auto read_palettes = [&](std::vector<PsxPalette>& destination, std::size_t color_count) {
            const std::uint32_t count = u32(archive.bytes_, post_offset, archive.source_);
            post_offset += 4;
            (void)checked_count(
                count,
                4 + color_count * 2,
                archive.bytes_.size() - post_offset,
                "PSX palette table");
            destination.reserve(static_cast<std::size_t>(count));
            for (std::uint32_t index = 0; index < count; ++index) {
                PsxPalette palette{};
                palette.name = u32(archive.bytes_, post_offset, archive.source_);
                post_offset += 4;
                palette.colors.reserve(color_count);
                for (std::size_t color = 0; color < color_count; ++color) {
                    palette.colors.push_back(u16(archive.bytes_, post_offset, archive.source_));
                    post_offset += 2;
                }
                destination.push_back(std::move(palette));
            }
        };
        read_palettes(archive.palettes4_, 16);
        read_palettes(archive.palettes8_, 256);
        std::uint32_t texture_count = u32(archive.bytes_, post_offset, archive.source_);
        post_offset += 4;
        if (archive.version_ >= 6 && texture_count == 0xffffffffU) {
            const std::uint32_t reference_count = u32(archive.bytes_, post_offset, archive.source_);
            post_offset += 4;
            (void)checked_count(
                reference_count,
                36,
                archive.bytes_.size() - post_offset,
                "PSX texture references");
            post_offset += static_cast<std::size_t>(reference_count) * 36U;
            const std::uint32_t cubemap_count = u32(archive.bytes_, post_offset, archive.source_);
            post_offset += 4;
            (void)checked_count(
                cubemap_count,
                36,
                archive.bytes_.size() - post_offset,
                "PSX cubemap references");
            post_offset += static_cast<std::size_t>(cubemap_count) * 36U;
            texture_count = u32(archive.bytes_, post_offset, archive.source_);
            post_offset += 4;
        }
        const std::size_t texture_count_size = checked_count(
            texture_count,
            4,
            archive.bytes_.size() - post_offset,
            "PSX texture table");
        std::vector<std::size_t> texture_offsets;
        texture_offsets.reserve(texture_count_size);
        for (std::uint32_t index = 0; index < texture_count; ++index) {
            const std::size_t offset = u32(archive.bytes_, post_offset, archive.source_);
            post_offset += 4;
            require_range(offset, 20, archive.bytes_.size(), archive.source_);
            if (!texture_offsets.empty() && offset <= texture_offsets.back()) {
                throw PsxFormatError("PSX texture offsets are not strictly increasing");
            }
            texture_offsets.push_back(offset);
        }
        archive.textures_.reserve(texture_offsets.size());
        for (std::size_t index = 0; index < texture_offsets.size(); ++index) {
            const std::size_t offset = texture_offsets[index];
            const std::size_t data_end = index + 1 < texture_offsets.size()
                ? texture_offsets[index + 1]
                : archive.bytes_.size();
            if (data_end < offset + 20) {
                throw PsxFormatError("PSX texture has an invalid boundary");
            }
            PsxTexture texture{};
            texture.offset = offset;
            texture.flags = u32(archive.bytes_, offset, archive.source_);
            texture.color_count = u32(archive.bytes_, offset + 4, archive.source_);
            texture.palette_name = u32(archive.bytes_, offset + 8, archive.source_);
            texture.name_index = u32(archive.bytes_, offset + 12, archive.source_);
            texture.width = u16(archive.bytes_, offset + 16, archive.source_);
            texture.height = u16(archive.bytes_, offset + 18, archive.source_);
            if (texture.color_count != 16U
                && texture.color_count != 256U
                && texture.color_count != 65536U) {
                throw PsxFormatError("PSX texture has an unsupported color count");
            }
            if (texture.name_index >= texture_name_count) {
                throw PsxFormatError("PSX texture references a missing texture name");
            }
            texture.data_offset = offset + 20;
            texture.data_size = data_end - texture.data_offset;
            archive.textures_.push_back(texture);
        }
    }
    return archive;
}

PsxDecodedTexture PsxArchive::decode_texture(
    std::size_t texture_index) const {
    if (texture_index >= textures_.size()) {
        throw PsxFormatError("PSX texture index is outside the texture table");
    }
    const PsxTexture& texture = textures_[texture_index];
    const std::uint32_t alignment = texture.color_count == 16U
        ? 3U
        : texture.color_count == 256U ? 1U : 0U;
    const std::uint32_t width =
        (static_cast<std::uint32_t>(texture.width) + alignment) & ~alignment;
    const std::uint32_t height =
        (static_cast<std::uint32_t>(texture.height) + alignment) & ~alignment;
    const std::uint64_t expected_size = texture.color_count == 16U
        ? static_cast<std::uint64_t>(width) * height / 2U
        : texture.color_count == 256U
        ? static_cast<std::uint64_t>(width) * height
        : static_cast<std::uint64_t>(width) * height * 2U;
    if (expected_size > texture.data_size
        || texture.data_offset > bytes_.size()
        || expected_size > bytes_.size() - texture.data_offset) {
        throw PsxFormatError("PSX texture payload is truncated");
    }

    const auto find_palette = [&](const std::vector<PsxPalette>& palettes)
        -> const PsxPalette* {
        for (const PsxPalette& palette : palettes) {
            if (palette.name == texture.palette_name) {
                return &palette;
            }
        }
        return nullptr;
    };
    const PsxPalette* palette = nullptr;
    if (texture.color_count == 16U) {
        palette = find_palette(palettes4_);
    } else if (texture.color_count == 256U) {
        palette = find_palette(palettes8_);
    }
    if ((texture.color_count == 16U || texture.color_count == 256U)
        && (palette == nullptr || palette->colors.size() < texture.color_count)) {
        throw PsxFormatError("PSX indexed texture has an incomplete palette");
    }

    PsxDecodedTexture result{width, height, {}};
    const std::uint64_t pixel_count = static_cast<std::uint64_t>(width) * height;
    result.rgb.reserve(static_cast<std::size_t>(pixel_count * 3U));
    const auto append_color = [&result](std::uint16_t color) {
        const std::array<std::uint8_t, 3> rgb = psx_color_to_rgb(color);
        result.rgb.insert(result.rgb.end(), rgb.begin(), rgb.end());
    };
    const std::byte* raw = bytes_.data() + texture.data_offset;
    if (texture.color_count == 16U) {
        for (std::uint64_t index = 0; index < expected_size; ++index) {
            const std::uint8_t value = std::to_integer<std::uint8_t>(raw[index]);
            append_color(palette->colors[value & 0x0fU]);
            append_color(palette->colors[value >> 4U]);
        }
    } else if (texture.color_count == 256U) {
        for (std::uint64_t index = 0; index < expected_size; ++index) {
            append_color(palette->colors[
                std::to_integer<std::uint8_t>(raw[index])]);
        }
    } else {
        for (std::uint64_t index = 0; index < expected_size; index += 2U) {
            const std::uint16_t color = static_cast<std::uint16_t>(
                std::to_integer<std::uint8_t>(raw[index])
                | (static_cast<std::uint16_t>(
                    std::to_integer<std::uint8_t>(raw[index + 1U])) << 8U));
            append_color(color);
        }
    }
    if (result.rgb.size() != static_cast<std::size_t>(pixel_count * 3U)) {
        throw PsxFormatError("PSX texture decoded pixel count is inconsistent");
    }
    return result;
}

} // namespace opentony::assets
