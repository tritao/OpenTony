#include "trg_runtime.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <sstream>

namespace opentony::trg {
namespace {

[[nodiscard]] std::uint8_t byte_at(std::span<const std::byte> bytes, std::size_t offset) {
    if (offset >= bytes.size()) {
        throw FormatError("TRG byte read outside the file");
    }
    return std::to_integer<std::uint8_t>(bytes[offset]);
}

[[nodiscard]] std::uint16_t u16_at(std::span<const std::byte> bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 2) {
        throw FormatError("TRG u16 read outside the file");
    }
    return static_cast<std::uint16_t>(
        std::to_integer<std::uint8_t>(bytes[offset])
        | (static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes[offset + 1])) << 8));
}

[[nodiscard]] std::uint32_t u32_at(std::span<const std::byte> bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 4) {
        throw FormatError("TRG u32 read outside the file");
    }
    return static_cast<std::uint32_t>(
        std::to_integer<std::uint8_t>(bytes[offset])
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 1])) << 8)
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 2])) << 16)
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 3])) << 24));
}

[[nodiscard]] std::int32_t s32_at(std::span<const std::byte> bytes, std::size_t offset) {
    return static_cast<std::int32_t>(u32_at(bytes, offset));
}

[[nodiscard]] std::string string_at(
    std::span<const std::byte> bytes,
    std::size_t offset,
    std::size_t end,
    std::size_t* next = nullptr) {
    if (offset > end || end > bytes.size()) {
        throw FormatError("TRG string range is outside the file");
    }
    std::size_t terminator = offset;
    while (terminator < end && byte_at(bytes, terminator) != 0) {
        ++terminator;
    }
    if (terminator == end) {
        throw FormatError("unterminated TRG string");
    }
    std::string result;
    result.reserve(terminator - offset);
    for (std::size_t cursor = offset; cursor < terminator; ++cursor) {
        result.push_back(static_cast<char>(byte_at(bytes, cursor)));
    }
    if (next != nullptr) {
        *next = (terminator + 2) & ~static_cast<std::size_t>(1);
        if (*next > end) {
            throw FormatError("aligned TRG string cursor leaves the node");
        }
    }
    return result;
}

[[nodiscard]] std::size_t checked_add(std::size_t left, std::size_t right, std::size_t limit, std::string_view what) {
    if (right > limit || left > limit - right) {
        throw FormatError(std::string("TRG ") + std::string(what) + " exceeds its containing buffer");
    }
    return left + right;
}

[[nodiscard]] bool is_one_word_opcode(std::uint16_t opcode) {
    switch (opcode) {
    case 0x0003:
    case 0x000b:
    case 0x000c:
    case 0x0066:
    case 0x0067:
    case 0x0079:
    case 0x007a:
    case 0x0081:
    case 0x0088:
    case 0x0089:
    case 0x0095:
    case 0x0098:
    case 0x009e:
    case 0x00ad:
    case 0x00af:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] bool is_counted_node_list_opcode(std::uint16_t opcode) {
    switch (opcode) {
    case 0x0004:
    case 0x0005:
    case 0x000a:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] bool is_two_word_opcode(std::uint16_t opcode) {
    switch (opcode) {
    case 0x000d:
    case 0x0069:
    case 0x006a:
    case 0x0083:
    case 0x0084:
    case 0x0086:
    case 0x008a:
    case 0x0093:
    case 0x0094:
    case 0x0096:
    case 0x0097:
    case 0x0099:
    case 0x009a:
    case 0x009b:
    case 0x009c:
    case 0x009d:
    case 0x00a0:
    case 0x00a1:
    case 0x00a3:
    case 0x00a4:
    case 0x00a5:
    case 0x00a6:
    case 0x00a8:
    case 0x00a9:
    case 0x00aa:
    case 0x00ac:
    case 0x00b1:
    case 0x00cb:
    case 0x00cc:
    case 0x00cd:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] bool is_string_opcode(std::uint16_t opcode) {
    switch (opcode) {
    case 0x0073:
    case 0x0077:
    case 0x007e:
    case 0x007f:
    case 0x0080:
    case 0x008c:
    case 0x008e:
    case 0x009f:
    case 0x00a2:
    case 0x00b0:
    case 0x00b2:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] bool is_three_u16_operand_opcode(std::uint16_t opcode) {
    switch (opcode) {
    case 0x0082:
    case 0x0087:
    case 0x008b:
    case 0x008f:
    case 0x0090:
    case 0x0091:
    case 0x0092:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] bool is_two_u16_operand_opcode(std::uint16_t opcode) {
    switch (opcode) {
    case 0x00a7:
    case 0x00ae:
    case 0x00c8:
    case 0x00ca:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] std::size_t link_count_offset(std::uint16_t type) {
    switch (type) {
    case 1:
        return 6;
    case 5:
        return 4;
    case 2:
    case 3:
    case 6:
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
        return 2;
    default:
        throw FormatError("TRG node type has no known link-list layout");
    }
}

} // namespace

std::array<std::int32_t, 6> FixedPathRecord::fixed12() const {
    std::array<std::int32_t, 6> result{};
    for (std::size_t index = 0; index < raw.size(); ++index) {
        // This is the retail `int << 12` representation. Unsigned shifting
        // avoids undefined signed overflow while preserving the low 32 bits.
        result[index] = static_cast<std::int32_t>(
            static_cast<std::uint32_t>(raw[index]) << 12);
    }
    return result;
}

TrgFile::TrgFile(
    std::vector<std::byte> backing,
    std::uint32_t version,
    std::vector<NodeView> nodes)
    : backing_(std::move(backing)), version_(version), nodes_(std::move(nodes)) {}

TrgFile TrgFile::parse(std::span<const std::byte> bytes) {
    if (bytes.size() < 12) {
        throw FormatError("TRG header is truncated");
    }
    constexpr std::array<std::uint8_t, 4> magic{'_', 'T', 'R', 'G'};
    for (std::size_t index = 0; index < magic.size(); ++index) {
        if (byte_at(bytes, index) != magic[index]) {
            throw FormatError("unsupported TRG magic");
        }
    }
    const std::uint32_t version = u32_at(bytes, 4);
    const std::uint32_t node_count = u32_at(bytes, 8);
    if (version != 2) {
        throw FormatError("unsupported TRG version");
    }
    if (node_count == 0) {
        throw FormatError("TRG has no nodes");
    }
    if (node_count > (std::numeric_limits<std::size_t>::max() - 12) / 4) {
        throw FormatError("TRG node table count overflows the host size");
    }
    const std::size_t table_end = checked_add(12, static_cast<std::size_t>(node_count) * 4, bytes.size(), "node table");
    std::vector<std::uint32_t> offsets;
    offsets.reserve(node_count);
    for (std::size_t index = 0; index < node_count; ++index) {
        const std::uint32_t offset = u32_at(bytes, 12 + index * 4);
        if (offset < table_end || offset > bytes.size() - 2) {
            throw FormatError("TRG node offset is outside the file");
        }
        if (!offsets.empty() && offset <= offsets.back()) {
            throw FormatError("TRG node offsets are not strictly increasing");
        }
        offsets.push_back(offset);
    }

    std::vector<NodeView> nodes;
    nodes.reserve(offsets.size());
    for (std::size_t index = 0; index < offsets.size(); ++index) {
        const std::size_t begin = offsets[index];
        const std::size_t end = index + 1 < offsets.size() ? offsets[index + 1] : bytes.size();
        if (end <= begin || end - begin < 2) {
            throw FormatError("TRG node has an invalid size");
        }
        nodes.push_back(NodeView{
            index,
            u16_at(bytes, begin),
            offsets[index],
            static_cast<std::uint32_t>(end - begin),
        });
    }
    if (nodes.back().type != 0xff) {
        throw FormatError("TRG final node is not the 0xff terminator");
    }
    return TrgFile(std::vector<std::byte>(bytes.begin(), bytes.end()), version, std::move(nodes));
}

TrgFile TrgFile::load(const std::string& path) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) {
        throw FormatError("could not open TRG file: " + path);
    }
    const std::streamoff size = stream.tellg();
    if (size < 0) {
        throw FormatError("could not determine TRG file size: " + path);
    }
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    stream.seekg(0);
    if (!bytes.empty()) {
        stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
    if (!stream) {
        throw FormatError("could not read TRG file: " + path);
    }
    return parse(bytes);
}

const NodeView& TrgFile::node(std::size_t index) const {
    if (index >= nodes_.size()) {
        throw FormatError("TRG node index is outside the node table");
    }
    return nodes_[index];
}

std::span<const std::byte> TrgFile::node_bytes(std::size_t index) const {
    const NodeView& current = node(index);
    const std::size_t begin = current.offset;
    const std::size_t size = current.size;
    if (begin > backing_.size() || size > backing_.size() - begin) {
        throw FormatError("TRG node byte view is outside the file");
    }
    return std::span<const std::byte>(backing_).subspan(begin, size);
}

std::uint16_t TrgFile::node_subtype(std::size_t index) const {
    const NodeView& current = node(index);
    if (current.type != 1 && current.type != 5 && current.type != 7) {
        throw FormatError("node has no spawn subtype field");
    }
    return u16_at(backing_, static_cast<std::size_t>(current.offset) + 2);
}

std::vector<std::uint8_t> TrgFile::node_spawn_options(std::size_t index) const {
    const NodeView& current = node(index);
    if (current.type != 1 && current.type != 7) {
        throw FormatError("node has no type-1/type-7 spawn options");
    }
    const std::size_t begin = current.offset;
    const std::size_t end = begin + current.size;
    const std::uint16_t link_words = u16_at(backing_, begin + 6);
    if (link_words > (std::numeric_limits<std::size_t>::max() - 8) / 2) {
        throw FormatError("TRG object option offset overflows the host size");
    }
    std::size_t cursor = begin + 8 + static_cast<std::size_t>(link_words) * 2;
    std::vector<std::uint8_t> result;
    while (cursor < end) {
        const std::uint8_t value = byte_at(backing_, cursor++);
        if (value == 0xff) {
            return result;
        }
        result.push_back(value);
    }
    throw FormatError("TRG object option list has no terminator");
}

std::array<std::int32_t, 3> TrgFile::node_position(std::size_t index) const {
    const NodeView& current = node(index);
    const std::size_t begin = current.offset;
    const std::size_t end = begin + current.size;
    std::size_t position = 0;
    switch (current.type) {
    case 1:
    case 7: {
        const std::uint16_t payload_words = u16_at(backing_, begin + 6);
        if (payload_words > (std::numeric_limits<std::size_t>::max() - 8) / 2) {
            throw FormatError("TRG object payload offset overflows the host size");
        }
        std::size_t marker = begin + 8 + static_cast<std::size_t>(payload_words) * 2;
        while (marker < end && byte_at(backing_, marker) != 0xff) {
            ++marker;
        }
        if (marker == end || marker > std::numeric_limits<std::size_t>::max() - 4) {
            throw FormatError("TRG object payload has no position marker");
        }
        position = (marker + 4) & ~static_cast<std::size_t>(3);
        break;
    }
    case 5: {
        const std::uint16_t payload_words = u16_at(backing_, begin + 4);
        position = (begin + static_cast<std::size_t>(payload_words) * 2 + 9)
            & ~static_cast<std::size_t>(3);
        break;
    }
    case 3:
    case 8:
    case 10:
    case 11:
    case 13: {
        const std::uint16_t link_words = u16_at(backing_, begin + 2);
        position = (begin + static_cast<std::size_t>(link_words) * 2 + 7)
            & ~static_cast<std::size_t>(3);
        break;
    }
    case 500:
    case 501:
        position = (begin + 5) & ~static_cast<std::size_t>(3);
        break;
    default:
        throw FormatError("node type has no recovered position layout");
    }
    if (position > end || end - position < 12) {
        throw FormatError("TRG node position exceeds its node");
    }
    FixedPathRecord raw{};
    for (std::size_t axis = 0; axis < 3; ++axis) {
        raw.raw[axis] = s32_at(backing_, position + axis * 4);
    }
    const std::array<std::int32_t, 6> fixed = raw.fixed12();
    return {fixed[0], fixed[1], fixed[2]};
}

CameraPointRecord TrgFile::camera_point(std::size_t index) const {
    const NodeView& current = node(index);
    if (current.type != 13) {
        throw FormatError("node is not a type-13 camera point");
    }
    const std::size_t begin = current.offset;
    const std::size_t end = begin + current.size;
    const std::uint16_t link_words = u16_at(backing_, begin + 2);
    const std::size_t position = (begin + static_cast<std::size_t>(link_words) * 2 + 7)
        & ~static_cast<std::size_t>(3);
    if (position > end || end - position < 16) {
        throw FormatError("camera-point payload exceeds its node");
    }
    return CameraPointRecord{
        node_position(index),
        u16_at(backing_, position + 12),
        u16_at(backing_, position + 14),
    };
}

std::uint16_t TrgFile::node_trigger_flags(std::size_t index) const {
    const NodeView& current = node(index);
    if (current.type != 10 && current.type != 11) {
        throw FormatError("node has no type-10/type-11 trigger flags");
    }
    const std::size_t begin = current.offset;
    const std::size_t end = begin + current.size;
    const std::uint16_t link_words = u16_at(backing_, begin + 2);
    const std::size_t position =
        (begin + static_cast<std::size_t>(link_words) * 2 + 7) & ~static_cast<std::size_t>(3);
    if (position > end || end - position < 14) {
        throw FormatError("type-10/type-11 trigger flags exceed the node");
    }
    // FUN_004c8650 returns the word immediately after the three coordinates;
    // FUN_004a9f70 reads that word for the PC build.
    return u16_at(backing_, position + 12);
}

std::array<std::uint16_t, 3> TrgFile::node_orientation(std::size_t index) const {
    const NodeView& current = node(index);
    if (current.type != 1 && current.type != 5 && current.type != 7) {
        throw FormatError("node has no spawn orientation fields");
    }
    const std::size_t begin = current.offset;
    const std::size_t end = begin + current.size;
    const std::array<std::int32_t, 3> unused_position = node_position(index);
    (void)unused_position;

    // All object/pickup constructors consume three u16 values immediately
    // after the fixed-point position triplet.
    std::size_t position = 0;
    const std::uint16_t payload_words = current.type == 5
        ? u16_at(backing_, begin + 4)
        : u16_at(backing_, begin + 6);
    if (current.type == 1 || current.type == 7) {
        std::size_t marker = begin + 8 + static_cast<std::size_t>(payload_words) * 2;
        while (marker < end && byte_at(backing_, marker) != 0xff) {
            ++marker;
        }
        if (marker == end) {
            throw FormatError("TRG object payload has no orientation marker");
        }
        position = (marker + 4) & ~static_cast<std::size_t>(3);
    } else {
        position = (begin + static_cast<std::size_t>(payload_words) * 2 + 9)
            & ~static_cast<std::size_t>(3);
    }
    if (position > end || end - position < 18) {
        throw FormatError("TRG spawn orientation exceeds its node");
    }
    return {
        u16_at(backing_, position + 12),
        u16_at(backing_, position + 14),
        u16_at(backing_, position + 16),
    };
}

std::uint32_t TrgFile::node_factory_cursor_offset(std::size_t index) const {
    const NodeView& current = node(index);
    if (current.type != 1 && current.type != 5 && current.type != 7) {
        throw FormatError("node has no object-constructor cursor");
    }
    const std::size_t begin = current.offset;
    const std::size_t end = begin + current.size;
    const std::uint16_t payload_words = current.type == 5
        ? u16_at(backing_, begin + 4)
        : u16_at(backing_, begin + 6);

    std::size_t position = 0;
    if (current.type == 1 || current.type == 7) {
        std::size_t marker = begin + 8 + static_cast<std::size_t>(payload_words) * 2;
        while (marker < end && byte_at(backing_, marker) != 0xff) {
            ++marker;
        }
        if (marker == end) {
            throw FormatError("TRG object payload has no constructor marker");
        }
        position = (marker + 4) & ~static_cast<std::size_t>(3);
    } else {
        position = (begin + static_cast<std::size_t>(payload_words) * 2 + 9)
            & ~static_cast<std::size_t>(3);
    }
    if (position > end || end - position < 18) {
        throw FormatError("TRG constructor cursor exceeds its node");
    }
    const std::size_t cursor = position + 18;
    if (cursor > std::numeric_limits<std::uint32_t>::max()) {
        throw FormatError("TRG constructor cursor does not fit the file offset");
    }
    return static_cast<std::uint32_t>(cursor);
}

std::uint32_t TrgFile::node_link_key_offset(std::size_t index) const {
    const NodeView& current = node(index);
    if (current.type != 2 && current.type != 6 && current.type != 9
        && current.type != 12 && current.type != 14) {
        throw FormatError("node has no linked-object key layout");
    }
    const std::size_t count_offset = static_cast<std::size_t>(current.offset) + 2;
    const std::size_t count = u16_at(backing_, count_offset);
    if (count > (std::numeric_limits<std::size_t>::max() - count_offset - 2) / 2) {
        throw FormatError("TRG command-point link count overflows the host size");
    }
    std::size_t key = count_offset + 2 + count * 2;
    if (key & 2) {
        key += 2;
    }
    const std::size_t end = static_cast<std::size_t>(current.offset) + current.size;
    if (key > backing_.size() || backing_.size() - key < 4 || key + 4 > end) {
        throw FormatError("command-point key is outside its node");
    }
    return static_cast<std::uint32_t>(key);
}

std::uint32_t TrgFile::command_point_key_offset(std::size_t index) const {
    const NodeView& current = node(index);
    if (current.type != 2 && current.type != 6 && current.type != 9) {
        throw FormatError("node is not a command-point layout");
    }
    return node_link_key_offset(index);
}

std::uint32_t TrgFile::command_point_checksum(std::size_t index) const {
    return u32_at(backing_, command_point_key_offset(index));
}

ScriptView TrgFile::script(std::size_t index) const {
    const NodeView& current = node(index);
    const std::size_t end = static_cast<std::size_t>(current.offset) + current.size;
    std::size_t start = 0;
    switch (current.type) {
    case 4:
    case 15:
        start = static_cast<std::size_t>(current.offset) + 2;
        break;
    case 6:
        start = static_cast<std::size_t>(command_point_key_offset(index)) + 4;
        break;
    case 8: {
        const std::size_t count = u16_at(backing_, static_cast<std::size_t>(current.offset) + 2);
        const std::size_t position = (static_cast<std::size_t>(current.offset) + count * 2 + 7) & ~static_cast<std::size_t>(3);
        const std::size_t name = checked_add(position, 18, end, "restart name");
        [[maybe_unused]] const std::string restart_name_value = string_at(backing_, name, end, &start);
        break;
    }
    default:
        throw FormatError("node type has no script stream");
    }
    if (start > end) {
        throw FormatError("TRG script starts outside its node");
    }
    return ScriptView{
        std::span<const std::byte>(backing_).subspan(start, end - start),
        static_cast<std::uint32_t>(start),
    };
}

std::vector<std::uint16_t> TrgFile::links(std::size_t index) const {
    const NodeView& current = node(index);
    const std::size_t count_offset = static_cast<std::size_t>(current.offset) + link_count_offset(current.type);
    const std::size_t count = u16_at(backing_, count_offset);
    const std::size_t first = count_offset + 2;
    const std::size_t end = static_cast<std::size_t>(current.offset) + current.size;
    if (first > end || count > (end - first) / 2) {
        throw FormatError("TRG link list exceeds its node");
    }
    std::vector<std::uint16_t> result;
    result.reserve(count);
    for (std::size_t link = 0; link < count; ++link) {
        result.push_back(u16_at(backing_, first + link * 2));
    }
    return result;
}

std::array<std::int32_t, 3> TrgFile::restart_position(std::size_t index) const {
    const NodeView& current = node(index);
    if (current.type != 8) {
        throw FormatError("node is not a restart node");
    }
    const std::size_t count = u16_at(backing_, static_cast<std::size_t>(current.offset) + 2);
    const std::size_t position = (static_cast<std::size_t>(current.offset) + count * 2 + 7) & ~static_cast<std::size_t>(3);
    const std::size_t end = static_cast<std::size_t>(current.offset) + current.size;
    if (position > end || end - position < 12) {
        throw FormatError("restart position exceeds its node");
    }
    return {s32_at(backing_, position), s32_at(backing_, position + 4), s32_at(backing_, position + 8)};
}

std::string TrgFile::restart_name(std::size_t index) const {
    const NodeView& current = node(index);
    if (current.type != 8) {
        throw FormatError("node is not a restart node");
    }
    const std::size_t count = u16_at(backing_, static_cast<std::size_t>(current.offset) + 2);
    const std::size_t position = (static_cast<std::size_t>(current.offset) + count * 2 + 7) & ~static_cast<std::size_t>(3);
    return string_at(backing_, position + 18, static_cast<std::size_t>(current.offset) + current.size);
}

std::uint32_t TrgFile::restart_auxiliary(std::size_t index) const {
    const NodeView& current = node(index);
    if (current.type != 8) {
        throw FormatError("node is not a restart node");
    }
    const std::size_t count = u16_at(backing_, static_cast<std::size_t>(current.offset) + 2);
    const std::size_t position = (static_cast<std::size_t>(current.offset) + count * 2 + 7) & ~static_cast<std::size_t>(3);
    const std::size_t end = static_cast<std::size_t>(current.offset) + current.size;
    if (position > end || end - position < 18) {
        throw FormatError("restart auxiliary fields exceed its node");
    }
    return u32_at(backing_, position + 12);
}

std::uint16_t TrgFile::restart_auxiliary_word(std::size_t index) const {
    const NodeView& current = node(index);
    if (current.type != 8) {
        throw FormatError("node is not a restart node");
    }
    const std::size_t count = u16_at(backing_, static_cast<std::size_t>(current.offset) + 2);
    const std::size_t position = (static_cast<std::size_t>(current.offset) + count * 2 + 7) & ~static_cast<std::size_t>(3);
    const std::size_t end = static_cast<std::size_t>(current.offset) + current.size;
    if (position > end || end - position < 18) {
        throw FormatError("restart auxiliary fields exceed its node");
    }
    return u16_at(backing_, position + 16);
}

CommandCursor::CommandCursor(
    std::span<const std::byte> bytes,
    std::uint32_t base_offset,
    std::size_t begin,
    std::size_t end)
    : bytes_(bytes), base_offset_(base_offset), position_(begin), end_(end) {
    if (begin > end || end > bytes.size() || base_offset > begin) {
        throw FormatError("TRG command cursor range is invalid");
    }
}

std::uint32_t CommandCursor::absolute_position() const noexcept {
    return base_offset_ + static_cast<std::uint32_t>(position_);
}

void CommandCursor::require(std::size_t size, std::string_view what) const {
    if (size > end_ || position_ > end_ - size || position_ > bytes_.size() || size > bytes_.size() - position_) {
        throw FormatError(std::string("TRG command ") + std::string(what) + " exceeds its node");
    }
}

void CommandCursor::set_position(std::size_t position) {
    if (position > end_ || position > bytes_.size()) {
        throw FormatError("TRG command cursor moved outside its node");
    }
    position_ = position;
}

std::uint16_t CommandCursor::peek_u16() const {
    if (position_ > end_ || end_ - position_ < 2) {
        throw FormatError("TRG command stream ends before an opcode");
    }
    return u16_at(bytes_, position_);
}

std::uint16_t CommandCursor::read_u16() {
    require(2, "u16 operand");
    const std::uint16_t result = u16_at(bytes_, position_);
    position_ += 2;
    return result;
}

std::uint32_t CommandCursor::read_u32() {
    require(4, "u32 operand");
    const std::uint32_t result = u32_at(bytes_, position_);
    position_ += 4;
    return result;
}

std::vector<std::uint16_t> CommandCursor::read_node_index_list() {
    const std::uint16_t count = read_u16();
    const std::size_t byte_count = static_cast<std::size_t>(count) * 2;
    require(byte_count, "counted node-index list");

    std::vector<std::uint16_t> result;
    result.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        result.push_back(read_u16());
    }
    return result;
}

std::string CommandCursor::read_string() {
    std::size_t next = position_;
    std::string result = string_at(bytes_, position_, end_, &next);
    set_position(next);
    return result;
}

std::array<std::uint32_t, 3> CommandCursor::read_u16_triplet() {
    return {read_u16(), read_u16(), read_u16()};
}

ScriptObjectCommand CommandCursor::read_script_object_operands(std::size_t opcode_offset) {
    const std::size_t aligned = align_down4_absolute(opcode_offset + 5);
    if (aligned > end_ || end_ - aligned < 10) {
        throw FormatError("TRG script-object operands exceed their node");
    }
    set_position(aligned);
    ScriptObjectCommand result{};
    result.key = read_u32();
    result.parameters = {
        read_u16(),
        read_u16(),
        read_u16(),
    };
    return result;
}

std::size_t CommandCursor::align_down4_absolute(std::size_t relative) const {
    const std::size_t absolute = static_cast<std::size_t>(base_offset_) + relative;
    const std::size_t aligned = absolute & ~static_cast<std::size_t>(3);
    if (aligned < base_offset_) {
        throw FormatError("TRG command alignment underflow");
    }
    return aligned - base_offset_;
}

void CommandCursor::skip_fixed_records(std::size_t first_record) {
    std::size_t cursor = first_record;
    while (true) {
        if (cursor >= end_) {
            throw FormatError("TRG fixed path table has no terminator");
        }
        if (byte_at(bytes_, cursor) == 0xff) {
            set_position(checked_add(cursor, 2, end_, "fixed path terminator"));
            return;
        }
        const std::size_t record = align_down4_absolute(cursor + 3);
        if (record > end_ || end_ - record < 0x18) {
            throw FormatError("TRG fixed path record exceeds its node");
        }
        cursor = record + 0x18;
    }
}

std::vector<FixedPathRecord> CommandCursor::read_fixed_path_records(std::size_t first_record) {
    std::vector<FixedPathRecord> records;
    std::size_t cursor = first_record;
    while (true) {
        if (cursor >= end_) {
            throw FormatError("TRG fixed path table has no terminator");
        }
        if (byte_at(bytes_, cursor) == 0xff) {
            set_position(checked_add(cursor, 2, end_, "fixed path terminator"));
            return records;
        }
        const std::size_t record = align_down4_absolute(cursor + 3);
        if (record > end_ || end_ - record < 0x18) {
            throw FormatError("TRG fixed path record exceeds its node");
        }
        FixedPathRecord value{};
        for (std::size_t word = 0; word < value.raw.size(); ++word) {
            value.raw[word] = s32_at(bytes_, record + word * 4);
        }
        records.push_back(value);
        cursor = record + 0x18;
    }
}

std::pair<std::uint32_t, std::uint16_t> CommandCursor::read_gap_operands(std::size_t opcode_offset) {
    const std::size_t aligned = align_down4_absolute(opcode_offset + 5);
    if (aligned > end_ || end_ - aligned < 6) {
        throw FormatError("TRG gap operands exceed their node");
    }
    set_position(aligned);
    return {read_u32(), read_u16()};
}

std::span<const std::byte> CommandCursor::raw(std::size_t begin) const {
    if (begin > position_ || position_ > bytes_.size()) {
        throw FormatError("TRG raw command range is invalid");
    }
    return bytes_.subspan(begin, position_ - begin);
}

void CommandCursor::skip_operands(std::uint16_t opcode, std::size_t opcode_offset) {
    if (opcode == 2) {
        while (read_string() != "") {
        }
        return;
    }
    if (is_counted_node_list_opcode(opcode)) {
        (void)read_node_index_list();
        return;
    }
    if (is_one_word_opcode(opcode)) {
        return;
    }
    if (is_two_word_opcode(opcode)) {
        (void)read_u16();
        return;
    }
    if (is_string_opcode(opcode)) {
        (void)read_string();
        return;
    }
    if (is_two_u16_operand_opcode(opcode)) {
        (void)read_u16();
        (void)read_u16();
        return;
    }
    if (is_three_u16_operand_opcode(opcode)) {
        (void)read_u16_triplet();
        return;
    }
    if (opcode == 0x0068) {
        (void)read_u16_triplet();
        return;
    }
    if (opcode == 0x0085) {
        skip_fixed_records(position_);
        return;
    }
    if (opcode == 0x008d) {
        (void)read_u16();
        (void)read_u16();
        skip_fixed_records(position_);
        return;
    }
    if (opcode == 0x00ab) {
        (void)read_script_object_operands(opcode_offset);
        return;
    }
    if (opcode == 0x00c9) {
        (void)read_gap_operands(opcode_offset);
        return;
    }
    std::ostringstream text;
    text << "unknown TRG opcode 0x" << std::hex << opcode;
    throw FormatError(text.str());
}

TriggerRuntime::TriggerRuntime(TrgFile file, TriggerServices& services)
    : file_(std::move(file)), services_(services), command_point_by_node_(file_.nodes().size(), CommandPointRuntime::npos) {
    bucket_heads_.fill(CommandPointRuntime::npos);
}

void TriggerRuntime::initialize(bool two_player) {
    two_player_mode_ = two_player;
    selected_restart_ = CommandPointRuntime::npos;
    run_autoexec(two_player);
    build();
}

void TriggerRuntime::create_command_point(std::size_t node_index, std::span<const std::byte> stream) {
    if (node_index > std::numeric_limits<std::uint16_t>::max()) {
        throw FormatError("TRG node index does not fit the retail command-point field");
    }
    const std::uint32_t checksum = file_.command_point_checksum(node_index);
    CommandPointRuntime point{};
    point.stream = stream;
    point.source_node = static_cast<std::uint16_t>(node_index);
    point.checksum = checksum;
    const std::size_t index = command_points_.size();
    point.bucket_next = bucket_heads_[checksum & 0xff];
    point.all_next = all_command_points_;
    bucket_heads_[checksum & 0xff] = index;
    all_command_points_ = index;
    command_points_.push_back(point);
    command_point_by_node_[node_index] = index;
}

void TriggerRuntime::build() {
    special_runtime_pulse_guard_ = false;
    command_points_.clear();
    command_point_by_node_.assign(file_.nodes().size(), CommandPointRuntime::npos);
    bucket_heads_.fill(CommandPointRuntime::npos);
    all_command_points_ = CommandPointRuntime::npos;

    for (const NodeView& current : file_.nodes()) {
        switch (current.type) {
        case 1:
        case 7: {
            services_.on_object_node(current.index);
            services_.on_object_node_data(current.index, file_.node_bytes(current.index));
            services_.on_spawn_node(
                current.index,
                current.type,
                file_.node_subtype(current.index),
                file_.node_position(current.index),
                file_.node_bytes(current.index));
            const std::vector<std::uint8_t> options = file_.node_spawn_options(current.index);
            services_.on_spawn_node_options(current.index, current.type, options);
            services_.on_spawn_factory_cursor(
                current.index,
                file_.node_factory_cursor_offset(current.index) - current.offset);
            services_.on_spawn_orientation(current.index, file_.node_orientation(current.index));
            break;
        }
        case 2:
        case 9:
            // Retail creates these records with a static link command stream;
            // they participate in lookup/link state but are not dispatched by
            // Trig_SendPulseToNode like type-6 records.
            services_.on_linked_node(
                current.index,
                current.type,
                file_.command_point_checksum(current.index),
                file_.node_bytes(current.index));
            create_command_point(current.index, {});
            break;
        case 5:
            services_.on_pickup_node(current.index);
            services_.on_pickup_node_data(current.index, file_.node_bytes(current.index));
            services_.on_spawn_node(
                current.index,
                current.type,
                file_.node_subtype(current.index),
                file_.node_position(current.index),
                file_.node_bytes(current.index));
            services_.on_spawn_factory_cursor(
                current.index,
                file_.node_factory_cursor_offset(current.index) - current.offset);
            services_.on_spawn_orientation(current.index, file_.node_orientation(current.index));
            break;
        case 6: {
            const ScriptView stream = file_.script(current.index);
            services_.on_linked_node(
                current.index,
                current.type,
                file_.command_point_checksum(current.index),
                file_.node_bytes(current.index));
            create_command_point(current.index, stream.bytes);
            break;
        }
        case 8:
            services_.on_restart_node(current.index, file_.restart_name(current.index), file_.restart_position(current.index));
            services_.on_restart_node_data(
                current.index,
                file_.restart_auxiliary(current.index),
                file_.restart_auxiliary_word(current.index));
            break;
        case 10:
        case 11:
            // FUN_004aa8c0 registers these nodes in the DAT_0056b860 list.
            // Its object stores the node index at +0x06 and the raw
            // FUN_004a9f70 word controls the initial +0x04 state byte.
            services_.on_special_node(current.index, current.type, file_.node_bytes(current.index));
            services_.on_special_node_state(
                current.index,
                current.type,
                file_.node_trigger_flags(current.index),
                file_.node_position(current.index));
            break;
        case 12:
        case 14:
            services_.on_linked_node(
                current.index,
                current.type,
                u32_at(file_.bytes(), file_.node_link_key_offset(current.index)),
                file_.node_bytes(current.index));
            services_.on_special_node(current.index, current.type, file_.node_bytes(current.index));
            break;
        default:
            if (current.type != 0xff) {
                services_.on_unhandled_node(current.index, current.type, file_.node_bytes(current.index));
            }
            break;
        }
    }

    // FUN_004aa8c0 resolves type-10/type-11 aliases only after the complete
    // node pointer table exists. Feed the raw link lists in a second pass so
    // the native service can resolve chains without depending on node order.
    for (const NodeView& current : file_.nodes()) {
        if (current.type == 10 || current.type == 11) {
            const std::vector<std::uint16_t> links = file_.links(current.index);
            services_.on_special_node_links(current.index, links);
        } else if (current.type == 12 || current.type == 14) {
            // FUN_004bdbd0 consumes this source record's links after it
            // resolves the +0x04 key. Preserve them at the same runtime
            // boundary instead of treating the key as the whole record.
            const std::vector<std::uint16_t> links = file_.links(current.index);
            services_.on_special_runtime_links(current.index, current.type, links);
        }
    }
    services_.on_special_node_aliases_complete();
}

std::size_t TriggerRuntime::find_autoexec(bool two_player) const {
    const std::uint16_t preferred = two_player ? 15 : 4;
    const std::uint16_t fallback = 4;
    for (const NodeView& current : file_.nodes()) {
        if (current.type == preferred) {
            return current.index;
        }
    }
    if (preferred != fallback) {
        for (const NodeView& current : file_.nodes()) {
            if (current.type == fallback) {
                return current.index;
            }
        }
    }
    return CommandPointRuntime::npos;
}

void TriggerRuntime::run_autoexec(bool two_player) {
    two_player_mode_ = two_player;
    const std::size_t index = find_autoexec(two_player);
    if (index == CommandPointRuntime::npos) {
        services_.on_diagnostic("TRG has no matching autoexec node");
        return;
    }
    const ScriptView stream = file_.script(index);
    dispatch(stream.bytes, stream.offset, index, nullptr, true);
}

const CommandPointRuntime* TriggerRuntime::command_point(std::size_t node_index) const {
    if (node_index >= command_point_by_node_.size()) {
        return nullptr;
    }
    const std::size_t point = command_point_by_node_[node_index];
    return point == CommandPointRuntime::npos ? nullptr : &command_points_[point];
}

const CommandPointRuntime* TriggerRuntime::command_point_by_checksum(std::uint32_t checksum) const {
    std::size_t point = bucket_heads_[checksum & 0xff];
    while (point != CommandPointRuntime::npos) {
        const CommandPointRuntime& current = command_points_[point];
        if (current.checksum == checksum) {
            return &current;
        }
        point = current.bucket_next;
    }
    return nullptr;
}

std::size_t TriggerRuntime::find_restart_by_name(std::string_view name) const {
    for (const NodeView& current : file_.nodes()) {
        if (current.type == 8 && file_.restart_name(current.index) == name) {
            return current.index;
        }
    }
    return CommandPointRuntime::npos;
}

void TriggerRuntime::pulse_node(std::size_t node_index) {
    const NodeView& current = file_.node(node_index);
    if (current.type == 6) {
        const std::size_t point_index = node_index < command_point_by_node_.size()
            ? command_point_by_node_[node_index]
            : CommandPointRuntime::npos;
        if (point_index == CommandPointRuntime::npos) {
            services_.on_diagnostic("pulse sent to an unbuilt type-6 command point");
            return;
        }
        CommandPointRuntime& point = command_points_[point_index];
        ++point.pulse_count;
        const ScriptView stream = file_.script(node_index);
        dispatch(point.stream, stream.offset, node_index, &point, false);
        return;
    }
    if (current.type == 1 || current.type == 5 || current.type == 7
        || current.type == 10 || current.type == 11
        || current.type == 12 || current.type == 14) {
        services_.on_node_pulse(node_index);
        if ((current.type != 12 && current.type != 14)
            || special_runtime_pulse_guard_
            || !services_.should_traverse_special_runtime_links(node_index)) {
            return;
        }

        // FUN_004bdbd0 sets DAT_0056db88 while it sends non-special linked
        // nodes. A direct type-12/type-14 link calls the asset activation
        // helper instead, so it must not recursively traverse that target's
        // own link list.
        const bool previous_guard = special_runtime_pulse_guard_;
        special_runtime_pulse_guard_ = true;
        try {
            for (const std::uint16_t target : file_.links(node_index)) {
                const NodeView& target_node = file_.node(target);
                if (target_node.type == 12 || target_node.type == 14) {
                    services_.on_node_pulse(target);
                } else {
                    pulse_node(target);
                }
            }
        } catch (...) {
            special_runtime_pulse_guard_ = previous_guard;
            throw;
        }
        special_runtime_pulse_guard_ = previous_guard;
    }
}

void TriggerRuntime::execute_restart(std::size_t node_index) {
    const NodeView& current = file_.node(node_index);
    if (current.type != 8) {
        throw FormatError("attempted to execute a non-restart TRG node");
    }
    services_.on_apply_restart(node_index, file_.restart_position(node_index));
    services_.on_apply_restart_data(
        node_index,
        file_.restart_auxiliary(node_index),
        file_.restart_auxiliary_word(node_index));
    const ScriptView stream = file_.script(node_index);
    dispatch(stream.bytes, stream.offset, node_index, nullptr, true);
}

void TriggerRuntime::pulse_links(std::size_t node_index) {
    const std::vector<std::uint16_t> linked = file_.links(node_index);
    for (const std::uint16_t target : linked) {
        pulse_node(target);
    }
}

void TriggerRuntime::skip_to_endif(CommandCursor& cursor) {
    std::size_t depth = 0;
    while (!cursor.at_end()) {
        const std::size_t opcode_offset = cursor.position();
        const std::uint16_t opcode = cursor.read_u16();
        if (opcode == 0xffff) {
            services_.on_diagnostic("conditional TRG block reaches the stream terminator");
            return;
        }
        if (opcode == 0x0095) {
            if (depth == 0) {
                return;
            }
            --depth;
            continue;
        }
        if (opcode == 0x0094 || opcode == 0x00cc || opcode == 0x00cd) {
            ++depth;
        }
        cursor.skip_operands(opcode, opcode_offset);
    }
    services_.on_diagnostic("conditional TRG block reaches the end of its node");
}

void TriggerRuntime::dispatch(
    std::span<const std::byte> stream,
    std::uint32_t stream_offset,
    std::size_t source_node,
    CommandPointRuntime* command_point,
    bool resource_flush) {
    if (stream.empty()) {
        services_.on_diagnostic("attempted to dispatch an empty TRG command stream");
        return;
    }
    const NodeView& source = file_.node(source_node);
    const std::size_t begin = static_cast<std::size_t>(stream_offset);
    const std::size_t end = static_cast<std::size_t>(source.offset) + source.size;
    CommandCursor cursor(file_.bytes(), 0, begin, end);
    std::size_t conditional_depth = 0;
    std::array<std::uint32_t, 3> pending_fog{};
    bool has_pending_fog = false;

    while (!cursor.at_end()) {
        const std::size_t opcode_offset = cursor.position();
        const std::uint16_t opcode = cursor.read_u16();
        if (opcode == 0xffff) {
            break;
        }
        switch (opcode) {
        case 0x0002: {
            std::vector<std::string> values;
            while (true) {
                std::string value = cursor.read_string();
                if (value.empty()) {
                    break;
                }
                values.push_back(std::move(value));
            }
            services_.on_cheat_restart_strings(values);
            break;
        }
        case 0x0003:
            // SendPulse is budgeted by the command-point state initialized by
            // 0x86. Retail suppresses the pulse at zero, decrements finite
            // budgets after dispatch, and treats 0xffff as unlimited.
            if (command_point == nullptr || command_point->state != 0) {
                pulse_links(source_node);
                if (command_point != nullptr && command_point->state != 0xffff) {
                    --command_point->state;
                }
            }
            break;
        case 0x0004:
        case 0x0005: {
            const std::vector<std::uint16_t> targets = cursor.read_node_index_list();
            services_.on_suspend_activate(source_node, opcode, targets);
            break;
        }
        case 0x000a: {
            const std::vector<std::uint16_t> targets = cursor.read_node_index_list();
            services_.on_signal(source_node, targets);
            break;
        }
        case 0x000b:
        case 0x000c: {
            const std::vector<std::uint16_t> linked = file_.links(source_node);
            services_.on_kill(source_node, opcode, linked);
            break;
        }
        case 0x000d: {
            const std::uint16_t value = cursor.read_u16();
            const std::vector<std::uint16_t> linked = file_.links(source_node);
            services_.on_visible(source_node, value, linked);
            break;
        }
        case 0x0083:
            services_.on_object_flag_by_id(cursor.read_u16(), false);
            break;
        case 0x0084:
            services_.on_object_flag_by_id(cursor.read_u16(), true);
            break;
        case 0x0068:
            pending_fog = cursor.read_u16_triplet();
            has_pending_fog = true;
            break;
        case 0x0069:
            services_.on_music(static_cast<std::int16_t>(cursor.read_u16()));
            break;
        case 0x006a:
            services_.on_sound(static_cast<std::int16_t>(cursor.read_u16()));
            break;
        case 0x007e:
        case 0x007f:
        case 0x0080: {
            const std::string value = cursor.read_string();
            const std::uint16_t mode = opcode == 0x007e ? 0 : opcode == 0x007f ? 2 : 1;
            services_.on_resource(mode, value);
            if (resource_flush && (opcode == 0x007e || opcode == 0x0080)) {
                services_.on_flush_resources();
            }
            break;
        }
        case 0x0081:
            services_.on_flush_resources();
            break;
        case 0x0073:
            // The PC dispatcher consumes the string and reports this command
            // as unavailable; it does not mutate the level state.
            (void)cursor.read_string();
            services_.on_diagnostic("TRG command no longer supported");
            break;
        case 0x0077:
            // Resource-stream cursor path with no PC-side helper.
            (void)cursor.read_string();
            break;
        case 0x0079:
            services_.on_diagnostic("CamFollowPath command not currently supported");
            break;
        case 0x007a:
            services_.on_diagnostic("ClearOtherRegion command not currently supported");
            break;
        case 0x008a:
            (void)cursor.read_u16();
            services_.on_diagnostic("SeekXA command not supported");
            break;
        case 0x008b:
            (void)cursor.read_u16_triplet();
            services_.on_diagnostic("PlayXA command not supported");
            break;
        case 0x0082:
        case 0x0087:
        case 0x008f:
        case 0x0090:
        case 0x0091:
        case 0x0092:
        case 0x0096:
        case 0x009b:
        case 0x009c:
        case 0x009f:
        case 0x00a1:
            // These helpers are present in the PC dispatcher but are empty
            // in this executable (or only route to an unavailable subsystem).
            // Consume their verified operands and retain the raw command for
            // traceability without inventing a gameplay mutation.
            if (opcode == 0x009f) {
                (void)cursor.read_string();
            } else if (opcode == 0x0082 || opcode == 0x0087
                       || opcode == 0x008f || opcode == 0x0090
                       || opcode == 0x0091 || opcode == 0x0092) {
                (void)cursor.read_u16_triplet();
            } else {
                (void)cursor.read_u16();
            }
            services_.on_legacy_command(opcode, cursor.raw(opcode_offset), source_node);
            break;
        case 0x0085: {
            const std::size_t records_start = cursor.position();
            const std::vector<FixedPathRecord> records = cursor.read_fixed_path_records(records_start);
            services_.on_fixed_path_records(records);
            services_.on_legacy_command(opcode, cursor.raw(opcode_offset), source_node);
            break;
        }
        case 0x0086: {
            const std::uint16_t count = cursor.read_u16();
            if (command_point != nullptr && command_point->initialized == 0) {
                command_point->initialized = 1;
                command_point->state = count;
            }
            break;
        }
        case 0x008d: {
            const std::uint16_t first = cursor.read_u16();
            const std::uint16_t second = cursor.read_u16();
            const std::vector<FixedPathRecord> records = cursor.read_fixed_path_records(cursor.position());
            services_.on_fixed_path(first, second, records);
            break;
        }
        case 0x008e:
            services_.on_competition_name(cursor.read_string());
            break;
        case 0x008c:
        case 0x00b0: {
            const std::string name = cursor.read_string();
            const std::size_t restart = find_restart_by_name(name);
            if (restart == CommandPointRuntime::npos) {
                services_.on_diagnostic("restart command names a missing restart node");
            } else {
                selected_restart_ = restart;
                services_.on_restart_selected(opcode, restart, name);
            }
            break;
        }
        case 0x00b2: {
            // Retail only honors Re_Start_2P in the two-player game mode;
            // in other modes it consumes the string as a no-op. This is the
            // Warehouse autoexec's second restart-selection command.
            const std::string name = cursor.read_string();
            if (two_player_mode_) {
                const std::size_t restart = find_restart_by_name(name);
                if (restart == CommandPointRuntime::npos) {
                    services_.on_diagnostic("two-player restart command names a missing restart node");
                } else {
                    selected_restart_ = restart;
                    services_.on_restart_selected(opcode, restart, name);
                }
            }
            break;
        }
        case 0x0093:
            services_.on_initial_state(cursor.read_u16());
            break;
        case 0x0094: {
            const std::uint16_t count = cursor.read_u16();
            if (command_point == nullptr) {
                services_.on_diagnostic("IfPulseCount used without a command point");
            } else if (command_point->pulse_count == count) {
                ++conditional_depth;
            } else {
                skip_to_endif(cursor);
            }
            break;
        }
        case 0x0095:
            if (conditional_depth == 0) {
                services_.on_diagnostic("Endif without a conditional TRG command");
            } else {
                --conditional_depth;
            }
            break;
        case 0x0097:
            services_.on_timer(static_cast<std::uint32_t>(cursor.read_u16()) * 1000U);
            break;
        case 0x0098:
            services_.on_kill_bruce_restart(source_node);
            {
                const std::vector<std::uint16_t> linked = file_.links(source_node);
                if (linked.empty()) {
                    services_.on_diagnostic("KILLBRUCE has no linked restart node");
                    break;
                }
                if (linked.size() != 1) {
                    services_.on_diagnostic("KILLBRUCE has more than one linked restart node");
                }
                const std::size_t restart_node = linked.front();
                if (restart_node >= file_.nodes().size()
                    || file_.node(restart_node).type != 8) {
                    services_.on_diagnostic("KILLBRUCE link is not a restart node");
                    break;
                }
                services_.on_apply_restart(
                    restart_node,
                    file_.restart_position(restart_node));
                services_.on_apply_restart_data(
                    restart_node,
                    file_.restart_auxiliary(restart_node),
                    file_.restart_auxiliary_word(restart_node));
            }
            break;
        case 0x00ab: {
            // Retail allocates a 0xcc-byte script object through
            // FUN_00401060. Its constructor resolves the aligned u32 key and
            // registers the object; preserve the trailing raw words because
            // this PC constructor does not read them.
            const ScriptObjectCommand command =
                cursor.read_script_object_operands(opcode_offset);
            services_.on_script_object(
                source_node,
                command.key,
                command.parameters);
            break;
        }
        case 0x009d:
            services_.on_reverb_type(static_cast<std::uint8_t>(cursor.read_u16()));
            break;
        case 0x009e:
            services_.on_level_event_state();
            break;
        case 0x00a6:
        case 0x00a9:
        case 0x00aa:
            services_.on_global_word(opcode, cursor.read_u16());
            break;
        case 0x0099:
        case 0x009a:
        case 0x00a0:
        case 0x00a4:
        case 0x00a5:
        case 0x00a8:
        case 0x00ac:
            services_.on_current_object_word(source_node, opcode, cursor.read_u16());
            break;
        case 0x00a3:
        case 0x00b1:
            services_.on_current_skater_word(source_node, opcode, cursor.read_u16());
            break;
        case 0x00a7: {
            const std::uint16_t first = cursor.read_u16();
            const std::uint16_t second = cursor.read_u16();
            services_.on_current_object_pair(source_node, opcode, first, second);
            break;
        }
        case 0x00a2:
            // The retail branch consumes an aligned string and reports that
            // LoadAI is unsupported in this PC dispatcher. Keep the command
            // visible to the legacy hook after reproducing that diagnostic.
            (void)cursor.read_string();
            services_.on_diagnostic("LoadAI command not supported");
            services_.on_legacy_command(opcode, cursor.raw(opcode_offset), source_node);
            break;
        case 0x00ad:
            services_.on_current_object_copy(source_node, opcode);
            break;
        case 0x00c8: {
            const std::uint32_t value = (static_cast<std::uint32_t>(cursor.read_u16()) << 16) | cursor.read_u16();
            services_.on_script_value(value);
            break;
        }
        case 0x00c9: {
            const auto [checksum, argument] = cursor.read_gap_operands(opcode_offset);
            if (command_point != nullptr && command_point->checksum != checksum) {
                services_.on_diagnostic("gap command checksum does not match its command point");
            }
            services_.on_gap(source_node, checksum, argument);
            if (services_.take_gap_pulse(checksum, argument)) {
                pulse_links(source_node);
            }
            break;
        }
        case 0x00ca: {
            const std::uint32_t value = (static_cast<std::uint32_t>(cursor.read_u16()) << 16) | cursor.read_u16();
            services_.on_level_value(value);
            break;
        }
        case 0x00cb: {
            const std::uint16_t flag = cursor.read_u16();
            if (flag >= 8) {
                services_.on_diagnostic("career flag index is outside the retail range");
            } else {
                services_.set_career_flag(flag);
            }
            break;
        }
        case 0x00cc: {
            const std::uint16_t flag = cursor.read_u16();
            if (flag >= 8 || !services_.career_flag(flag)) {
                skip_to_endif(cursor);
            } else {
                ++conditional_depth;
            }
            break;
        }
        case 0x00cd: {
            const std::uint16_t goal = cursor.read_u16();
            if (goal >= 11 || !services_.goal_complete(goal)) {
                skip_to_endif(cursor);
            } else {
                ++conditional_depth;
            }
            break;
        }
        default:
            try {
                cursor.skip_operands(opcode, opcode_offset);
            } catch (const FormatError& error) {
                const std::string_view message(error.what());
                if (message.starts_with("unknown TRG opcode")) {
                    const std::span<const std::byte> remaining =
                        file_.bytes().subspan(opcode_offset, end - opcode_offset);
                    services_.on_unknown_command(
                        opcode,
                        static_cast<std::uint32_t>(opcode_offset),
                        source_node,
                        remaining);
                    // Retail's default switch arm reports the unknown word
                    // and advances past the opcode only. Do the same here;
                    // guessing an operand width would desynchronise the
                    // historical opaque tails in SKATE_T/SKPARK_T.
                    services_.on_diagnostic("unknown TRG command");
                    continue;
                }
                throw;
            }
            services_.on_legacy_command(opcode, cursor.raw(opcode_offset), source_node);
            break;
        }
    }
    if (has_pending_fog) {
        services_.on_fog_range(
            static_cast<std::uint16_t>(pending_fog[0]),
            static_cast<std::uint16_t>(pending_fog[1]),
            static_cast<std::uint16_t>(pending_fog[2]));
    }
}

} // namespace opentony::trg
