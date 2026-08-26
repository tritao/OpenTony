#include "render_command_dispatch.hpp"

#include <array>
#include "tests/test_check.hpp"
#include <iostream>
#include <vector>

int main() {
    using namespace opentony::trg;

    struct ExpectedHandler {
        std::uint8_t opcode_base;
        std::uint32_t address;
        RenderPrimitiveKind primitive;
        std::uint32_t vertex_count;
        bool textured;
    };
    const std::array expected_handlers{
        ExpectedHandler{0x20, 0x004d42f0, RenderPrimitiveKind::SolidTriangle, 3, false},
        ExpectedHandler{0x24, 0x004d45f0, RenderPrimitiveKind::TexturedTriangle, 3, true},
        ExpectedHandler{0x28, 0x004d49e0, RenderPrimitiveKind::SolidQuad, 4, false},
        ExpectedHandler{0x2c, 0x004d4bf0, RenderPrimitiveKind::TexturedQuad, 4, true},
        ExpectedHandler{0x30, 0x004d5040, RenderPrimitiveKind::GouraudTriangle, 3, false},
        ExpectedHandler{0x34, 0x004d5280, RenderPrimitiveKind::TexturedGouraudTriangle, 3, true},
        ExpectedHandler{0x38, 0x004d56c0, RenderPrimitiveKind::GouraudQuad, 4, false},
        ExpectedHandler{0x3c, 0x004d5960, RenderPrimitiveKind::TexturedGouraudQuad, 4, true},
        ExpectedHandler{0x40, 0x004d6560, RenderPrimitiveKind::Line, 2, false},
        ExpectedHandler{0x48, 0x004d6120, RenderPrimitiveKind::LineStrip, 4, false},
        ExpectedHandler{0x4c, 0x004d6320, RenderPrimitiveKind::ClosedLineStrip, 5, false},
        ExpectedHandler{0x50, 0x004d66f0, RenderPrimitiveKind::ColoredLine, 2, false},
        ExpectedHandler{0x60, 0x004d5e40, RenderPrimitiveKind::SolidRectangle, 4, false},
        ExpectedHandler{0x68, 0x004d6090, RenderPrimitiveKind::UnitRectangle, 4, false},
        ExpectedHandler{0x70, 0x004d60c0, RenderPrimitiveKind::Rectangle8, 4, false},
        ExpectedHandler{0x78, 0x004d60f0, RenderPrimitiveKind::Rectangle16, 4, false},
        ExpectedHandler{0xb0, 0x004d68b0, RenderPrimitiveKind::GeneralPolygon, 0, false},
    };
    for (const ExpectedHandler& expected : expected_handlers) {
        const auto record = RenderCommandDispatcher::dispatch_one(
            {1, expected.opcode_base, 0, 0, 0, false});
        CHECK(record.opcode_base == expected.opcode_base);
        CHECK(record.handler_address == expected.address);
        CHECK(record.primitive == expected.primitive);
        CHECK(record.vertex_count == expected.vertex_count);
        CHECK(record.textured == expected.textured);
    }

    const std::vector<RenderCommandRecord> commands{
        {1, 0x27, 0x1234, 11, 99, true},
        {1, 0x3f, 0, 12, 4, true},
        {1, 0xb0, 0, 13, 7, true},
        {0, 0x20, 0, 14, 3, false},
        {1, 0x44, 0, 15, 2, false},
    };
    const auto records = RenderCommandDispatcher::dispatch(commands);

    CHECK(records.size() == commands.size());
    for (std::size_t index = 0; index < records.size(); ++index) {
        CHECK(records[index].command_index == index);
        CHECK(records[index].polygon_index == commands[index].polygon_index);
    }
    CHECK(records[0].command_index == 0);
    CHECK(records[0].polygon_index == 11);
    CHECK(records[0].enabled_or_payload == 1);
    CHECK(records[0].render_flags == 0x1234);
    CHECK(records[0].opcode_base == 0x24);
    CHECK(records[0].handler_address == 0x004d45f0);
    CHECK(records[0].primitive == RenderPrimitiveKind::TexturedTriangle);
    CHECK(records[0].vertex_count == 3);
    CHECK(records[0].textured);
    CHECK(records[0].state.format_mode_bit == 1);
    CHECK(records[0].state.alpha_or_texture_state);
    CHECK(records[0].state.texture_blend_setup);

    CHECK(records[1].opcode_base == 0x3c);
    CHECK(records[1].handler_address == 0x004d5960);
    CHECK(records[1].primitive == RenderPrimitiveKind::TexturedGouraudQuad);
    CHECK(records[1].vertex_count == 4);
    CHECK(records[1].state.format_mode_bit == 1);
    CHECK(records[1].state.alpha_or_texture_state);
    CHECK(records[1].state.texture_blend_setup);
    CHECK(records[1].state.alternate_texture_word);

    CHECK(records[2].opcode_base == 0xb0);
    CHECK(records[2].handler_address == 0x004d68b0);
    CHECK(records[2].primitive == RenderPrimitiveKind::GeneralPolygon);
    CHECK(records[2].vertex_count == 7);
    CHECK(records[2].textured);

    // Disabled commands retain their position in the trace but do not reach
    // a concrete geometry handler.
    CHECK(records[3].command_index == 3);
    CHECK(!records[3].active);
    CHECK(records[3].primitive == RenderPrimitiveKind::NoOp);
    CHECK(records[3].handler_address == kRetailNoOpPolygonHandler);
    CHECK(records[3].vertex_count == 0);

    // Uninstalled opcode-base slots are the same no-op target as disabled
    // records, with the original opcode still available for diagnosis.
    CHECK(records[4].opcode == 0x44);
    CHECK(records[4].opcode_base == 0x44);
    CHECK(records[4].active);
    CHECK(records[4].primitive == RenderPrimitiveKind::NoOp);
    CHECK(records[4].handler_address == kRetailNoOpPolygonHandler);

    std::cout << "Render command dispatch tests passed\n";
}
