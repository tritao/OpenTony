#include "psh_asset.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <string_view>

namespace opentony::assets {
namespace {

[[nodiscard]] std::string upper_ascii(std::string value) {
    for (char& character : value) {
        character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
    }
    return value;
}

[[nodiscard]] bool starts_with_case_insensitive(
    std::string_view value,
    std::string_view prefix) {
    if (value.size() < prefix.size()) {
        return false;
    }
    for (std::size_t index = 0; index < prefix.size(); ++index) {
        if (std::toupper(static_cast<unsigned char>(value[index]))
            != std::toupper(static_cast<unsigned char>(prefix[index]))) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::string path_base(const std::string& source) {
    if (source.empty()) {
        return {};
    }
    return std::filesystem::path(source).stem().string();
}

} // namespace

PshManifest PshManifest::load(const std::string& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        throw PshFormatError("cannot open PSH: " + path);
    }
    const std::streamoff length = input.tellg();
    if (length < 0) {
        throw PshFormatError("cannot determine PSH size: " + path);
    }
    input.seekg(0, std::ios::beg);
    std::vector<std::byte> bytes(static_cast<std::size_t>(length));
    if (!bytes.empty()) {
        input.read(reinterpret_cast<char*>(bytes.data()), length);
        if (!input) {
            throw PshFormatError("cannot read PSH: " + path);
        }
    }
    return parse(std::move(bytes), path);
}

PshManifest PshManifest::parse(
    std::vector<std::byte> bytes,
    std::string source,
    std::string base_name) {
    PshManifest result{};
    result.bytes_ = std::move(bytes);
    result.source_ = std::move(source);
    result.base_name_ = upper_ascii(base_name.empty() ? path_base(result.source_) : base_name);
    if (result.base_name_.empty()) {
        throw PshFormatError("PSH base name is empty: " + result.source_);
    }
    const auto collect_parts = [&result](const std::string& token) {
        result.parts_.clear();
        std::string line;
        std::size_t line_offset = 0;
        while (line_offset < result.bytes_.size()) {
            const std::size_t line_start = line_offset;
            while (line_offset < result.bytes_.size()
                && result.bytes_[line_offset] != std::byte{'\n'}) {
                line.push_back(static_cast<char>(std::to_integer<std::uint8_t>(
                    result.bytes_[line_offset++])));
            }
            if (line_offset < result.bytes_.size()) {
                ++line_offset;
            }
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            const std::size_t define = line.find("#define");
            if (define != std::string::npos) {
                const std::size_t name_begin = line.find_first_not_of(" \t", define + 7U);
                if (name_begin != std::string::npos
                    && starts_with_case_insensitive(
                        std::string_view(line).substr(name_begin), token)) {
                    const std::size_t part_begin = name_begin + token.size();
                    const std::size_t value_begin = line.find_first_of(" \t", part_begin);
                    if (part_begin == line.size() || value_begin == std::string::npos) {
                        throw PshFormatError("PSH part definition is incomplete: " + result.source_);
                    }
                    const std::size_t part_end = line.find_first_of(" \t", part_begin);
                    const std::size_t index_begin = line.find_first_not_of(" \t", value_begin);
                    if (index_begin == std::string::npos) {
                        throw PshFormatError("PSH part index is missing: " + result.source_);
                    }
                    const std::string symbol = line.substr(
                        part_begin, part_end - part_begin);
                    const std::string_view value_text = std::string_view(line).substr(index_begin);
                    std::uint32_t index = 0;
                    const auto parsed = std::from_chars(
                        value_text.data(), value_text.data() + value_text.size(), index);
                    if (parsed.ec != std::errc{} || parsed.ptr == value_text.data()) {
                        throw PshFormatError("PSH part index is invalid: " + result.source_);
                    }
                    const std::size_t separator = symbol.find('_');
                    const std::string model_name = separator == std::string::npos
                        ? std::string{}
                        : symbol.substr(0, separator);
                    const std::string part_name = separator == std::string::npos
                        ? symbol
                        : symbol.substr(separator + 1U);
                    if (part_name.empty()) {
                        throw PshFormatError("PSH part name is empty: " + result.source_);
                    }
                    result.parts_.push_back({part_name, model_name, index, line_start});
                }
            }
            line.clear();
        }
    };
    collect_parts(result.base_name_ + "PART_");
    if (result.parts_.empty()) {
        std::string alias = result.base_name_;
        while (!alias.empty() && std::isdigit(static_cast<unsigned char>(alias.back()))) {
            alias.pop_back();
        }
        if (!alias.empty() && alias != result.base_name_) {
            collect_parts(alias + "PART_");
            result.base_name_ = alias;
        }
    }
    if (result.parts_.empty()) {
        throw PshFormatError("no PSH parts found: " + result.source_);
    }
    std::vector<std::uint32_t> indices;
    indices.reserve(result.parts_.size());
    for (const PshPart& part : result.parts_) {
        indices.push_back(part.index);
    }
    std::sort(indices.begin(), indices.end());
    for (std::size_t index = 0; index < indices.size(); ++index) {
        if (indices[index] != index) {
            throw PshFormatError("PSH part indices are not contiguous: " + result.source_);
        }
    }
    return result;
}

std::vector<PshPartMatch> match_psh_parts(
    const PshManifest& animation,
    const PshManifest& model) {
    std::vector<PshPartMatch> matches;
    matches.reserve(animation.parts().size());
    for (const PshPart& animation_part : animation.parts()) {
        const auto found = std::find_if(
            model.parts().begin(),
            model.parts().end(),
            [&animation_part](const PshPart& model_part) {
                return model_part.name == animation_part.name;
            });
        if (found != model.parts().end()) {
            matches.push_back({animation_part.index, found->index, animation_part.name});
        }
    }
    return matches;
}

} // namespace opentony::assets
