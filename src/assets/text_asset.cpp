#include "text_asset.hpp"

#include <charconv>
#include <fstream>
#include <limits>
#include <string_view>

namespace opentony::assets {
namespace {

[[nodiscard]] std::vector<std::string> lines(
    const std::vector<std::byte>& bytes,
    const std::string& source) {
    std::vector<std::string> result;
    std::string current;
    for (const std::byte byte : bytes) {
        const char character = static_cast<char>(std::to_integer<std::uint8_t>(byte));
        if (character == '\n') {
            if (!current.empty() && current.back() == '\r') {
                current.pop_back();
            }
            result.push_back(std::move(current));
            current.clear();
        } else {
            current.push_back(character);
        }
    }
    if (!current.empty()) {
        result.push_back(std::move(current));
    }
    if (result.empty() && !bytes.empty()) {
        throw TextFormatError("text line conversion failed: " + source);
    }
    return result;
}

[[nodiscard]] std::uint32_t parse_index(std::string_view value, const std::string& source) {
    std::uint32_t result = 0;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) {
        throw TextFormatError("park label index is invalid: " + source);
    }
    return result;
}

} // namespace

ParkLabelTable ParkLabelTable::load(const std::string& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        throw TextFormatError("cannot open park labels: " + path);
    }
    const std::streamoff length = input.tellg();
    if (length < 0) {
        throw TextFormatError("cannot determine park label size: " + path);
    }
    input.seekg(0, std::ios::beg);
    std::vector<std::byte> bytes(static_cast<std::size_t>(length));
    if (!bytes.empty()) {
        input.read(reinterpret_cast<char*>(bytes.data()), length);
        if (!input) {
            throw TextFormatError("cannot read park labels: " + path);
        }
    }
    return parse(std::move(bytes), path);
}

ParkLabelTable ParkLabelTable::parse(
    std::vector<std::byte> bytes,
    std::string source) {
    ParkLabelTable result{};
    result.bytes_ = std::move(bytes);
    result.source_ = std::move(source);
    for (const std::string& line : lines(result.bytes_, result.source_)) {
        if (line.empty()) {
            continue;
        }
        const std::size_t tab = line.find('\t');
        if (tab == std::string::npos || tab + 1U >= line.size() || line[tab + 1U] != '$') {
            throw TextFormatError("park label record is malformed: " + result.source_);
        }
        const std::uint32_t index = parse_index(
            std::string_view(line).substr(0, tab), result.source_);
        if (index != result.labels_.size()) {
            throw TextFormatError("park label indices are not contiguous: " + result.source_);
        }
        result.labels_.push_back({index, line.substr(tab + 2U)});
    }
    if (result.labels_.size() != 50U) {
        throw TextFormatError("park label table does not contain 50 records: " + result.source_);
    }
    return result;
}

PresentationTextAsset PresentationTextAsset::load(const std::string& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        throw TextFormatError("cannot open presentation text: " + path);
    }
    const std::streamoff length = input.tellg();
    if (length < 0) {
        throw TextFormatError("cannot determine presentation text size: " + path);
    }
    input.seekg(0, std::ios::beg);
    std::vector<std::byte> bytes(static_cast<std::size_t>(length));
    if (!bytes.empty()) {
        input.read(reinterpret_cast<char*>(bytes.data()), length);
        if (!input) {
            throw TextFormatError("cannot read presentation text: " + path);
        }
    }
    return parse(std::move(bytes), path);
}

PresentationTextAsset PresentationTextAsset::parse(
    std::vector<std::byte> bytes,
    std::string source) {
    PresentationTextAsset result{};
    result.bytes_ = std::move(bytes);
    result.source_ = std::move(source);
    bool alternate_font = false;
    for (const std::string& line : lines(result.bytes_, result.source_)) {
        if (!line.empty() && line.front() == '#') {
            break;
        }
        if (line.size() >= 3U && line[0] == '@') {
            const char tag = line[1];
            if ((tag == 'F' || tag == 'f') && line.size() >= 4U) {
                alternate_font = true;
                continue;
            }
            if ((tag == 'B' || tag == 'b') && line.size() >= 3U) {
                result.records_.push_back({
                    PresentationTextRecordKind::bitmap,
                    line.substr(3), 0, 0, alternate_font});
                alternate_font = false;
                continue;
            }
            if ((tag == 'M' || tag == 'm') && line.size() >= 5U
                && line[2] >= '0' && line[2] <= '9'
                && line[3] == ',' && line[4] >= '0' && line[4] <= '9') {
                result.records_.push_back({
                    PresentationTextRecordKind::marker,
                    {}, static_cast<std::uint8_t>(line[2] - '0'),
                    static_cast<std::uint8_t>(line[4] - '0'), alternate_font});
                alternate_font = false;
                continue;
            }
        }
        result.records_.push_back({
            PresentationTextRecordKind::text, line, 0, 0, alternate_font});
        alternate_font = false;
    }
    if (result.records_.size() > 1000U) {
        throw TextFormatError("presentation text exceeds the 1000-record runtime bound: "
            + result.source_);
    }
    return result;
}

} // namespace opentony::assets
