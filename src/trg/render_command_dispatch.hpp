#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace opentony::trg {

inline constexpr std::uint32_t kRetailNoOpPolygonHandler = 0x004d7420;

// The concrete handlers installed by D3D_InitPrimitiveDispatch.  These are
// backend-neutral names: the native renderer still decides how a primitive is
// represented by its eventual graphics API.
enum class RenderPrimitiveKind : std::uint8_t {
    NoOp,
    SolidTriangle,
    TexturedTriangle,
    SolidQuad,
    TexturedQuad,
    GouraudTriangle,
    TexturedGouraudTriangle,
    GouraudQuad,
    TexturedGouraudQuad,
    Line,
    LineStrip,
    ClosedLineStrip,
    ColoredLine,
    SolidRectangle,
    UnitRectangle,
    Rectangle8,
    Rectangle16,
    GeneralPolygon,
};

// A native representation of the fields needed by D3D_DrawPolygonList
// 0x004d3160.  A span is already in linked-list traversal order; no retail
// pointer is fabricated in this adapter.
struct RenderCommandRecord {
    static constexpr std::size_t npos = static_cast<std::size_t>(-1);

    std::uint32_t enabled_or_payload{}; // command +0x04
    std::uint8_t opcode{};              // command +0x07
    std::uint32_t render_flags{};       // command +0x08
    std::size_t polygon_index{npos};
    std::uint32_t vertex_count{};
    bool textured{};
};

struct RenderDispatchState {
    // These bits are read from the command opcode before handler dispatch.
    std::uint8_t format_mode_bit{};       // opcode bit 0
    bool alpha_or_texture_state{};         // opcode bit 1
    bool texture_blend_setup{};            // opcode bit 2
    bool alternate_texture_word{};         // opcode bit 4
};

struct RenderDispatchRecord {
    std::size_t command_index{};
    std::size_t polygon_index{RenderCommandRecord::npos};
    std::uint32_t enabled_or_payload{};
    std::uint8_t opcode{};
    std::uint8_t opcode_base{};
    std::uint32_t render_flags{};
    std::uint32_t handler_address{kRetailNoOpPolygonHandler};
    RenderPrimitiveKind primitive{RenderPrimitiveKind::NoOp};
    std::uint32_t vertex_count{};
    bool textured{};
    bool active{};
    RenderDispatchState state{};
};

class RenderCommandDispatcher final {
public:
    [[nodiscard]] static RenderDispatchRecord dispatch_one(
        const RenderCommandRecord& command,
        std::size_t command_index = 0);

    // The result has one record per input command and preserves the input
    // linked-list order.  No dispatch record is a presentation event;
    // presentation remains the separate 0x004d0ca4 Flip boundary.
    [[nodiscard]] static std::vector<RenderDispatchRecord> dispatch(
        std::span<const RenderCommandRecord> commands);
};

} // namespace opentony::trg
