#include "render_packet_builder.hpp"

#include "tests/test_check.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace {

opentony::trg::RenderPolygonPacket make_triangle(
    std::array<float, 3> depths,
    std::uint32_t clip_flags = 0) {
    using namespace opentony::trg;
    RenderPolygonPacket polygon{};
    polygon.vertex_count = 3;
    polygon.vertices.resize(3);
    const std::array<std::array<float, 2>, 3> positions{
        std::array<float, 2>{0.0F, 0.0F},
        std::array<float, 2>{1.0F, 0.0F},
        std::array<float, 2>{0.0F, 1.0F},
    };
    for (std::size_t index = 0; index < positions.size(); ++index) {
        polygon.vertices[index].projected.x = positions[index][0];
        polygon.vertices[index].projected.y = positions[index][1];
        polygon.vertices[index].projected.z = depths[index];
        polygon.vertices[index].projected.reciprocal_depth =
            100.0F + static_cast<float>(index);
        polygon.vertices[index].projected.clip_flags = clip_flags;
    }
    return polygon;
}

opentony::trg::RenderPolygonPacket make_quad() {
    using namespace opentony::trg;
    RenderPolygonPacket polygon{};
    polygon.vertex_count = 4;
    polygon.vertices.resize(4);
    const std::array<std::array<float, 2>, 4> positions{
        std::array<float, 2>{0.0F, 0.0F},
        std::array<float, 2>{1.0F, 0.0F},
        std::array<float, 2>{1.0F, 1.0F},
        std::array<float, 2>{0.0F, 1.0F},
    };
    for (std::size_t index = 0; index < positions.size(); ++index) {
        polygon.vertices[index].projected.x = positions[index][0];
        polygon.vertices[index].projected.y = positions[index][1];
        polygon.vertices[index].projected.z = 20.0F + index;
    }
    return polygon;
}

} // namespace

int main() {
    using namespace opentony::trg;

    RenderPolygonPacket triangle = make_triangle({20.0F, 80.0F, 40.0F});
    const RenderBucketDecision accepted = RenderPacketBuilder::classify_polygon(
        triangle, RenderPacketBuilder::summarize_clip(triangle));
    CHECK(accepted.disposition == RenderBucketDisposition::accepted);
    CHECK(accepted.first_winding_determinant == 1.0F);
    CHECK(accepted.bucket_index == 20);
    CHECK(accepted.selected_depth == 80.0F);
    CHECK(std::fabs(triangle.vertices[0].projected.z - 0.5F) < 0.00001F);
    CHECK(std::fabs(triangle.vertices[1].projected.z - 0.125F) < 0.00001F);
    CHECK(std::fabs(triangle.vertices[2].projected.z - 0.25F) < 0.00001F);
    CHECK(triangle.vertices[0].projected.reciprocal_depth == 100.0F);

    RenderPolygonPacket reversed = make_triangle({20.0F, 80.0F, 40.0F});
    std::swap(reversed.vertices[1], reversed.vertices[2]);
    const RenderBucketDecision rejected = RenderPacketBuilder::classify_polygon(
        reversed, RenderPacketBuilder::summarize_clip(reversed));
    CHECK(rejected.disposition == RenderBucketDisposition::rejected_winding);
    RenderDepthBucketOptions reverse_options;
    reverse_options.reverse_winding = true;
    const RenderBucketDecision reverse_accepted =
        RenderPacketBuilder::classify_polygon(
            reversed,
            RenderPacketBuilder::summarize_clip(reversed),
            reverse_options);
    CHECK(reverse_accepted.disposition == RenderBucketDisposition::accepted);

    RenderPolygonPacket quad = make_quad();
    const RenderBucketDecision quad_decision = RenderPacketBuilder::classify_polygon(
        quad, RenderPacketBuilder::summarize_clip(quad));
    CHECK(quad_decision.disposition == RenderBucketDisposition::accepted);
    CHECK(quad_decision.first_winding_determinant == 1.0F);
    CHECK(quad_decision.second_winding_determinant == 1.0F);

    quad.textured = true;
    for (std::size_t index = 0; index < quad.vertices.size(); ++index) {
        quad.vertices[index].projected.auxiliary =
            static_cast<float>(index);
    }
    const std::vector<RenderPolygonPacket> split =
        RenderPacketBuilder::split_textured_quad(quad, true);
    CHECK(split.size() == 2);
    CHECK(split[0].vertex_count == 3);
    CHECK(split[1].vertex_count == 3);
    CHECK(split[0].vertices[0].projected.auxiliary == 0.0F);
    CHECK(split[0].vertices[1].projected.auxiliary == 1.0F);
    CHECK(split[0].vertices[2].projected.auxiliary == 3.0F);
    CHECK(split[1].vertices[0].projected.auxiliary == 3.0F);
    CHECK(split[1].vertices[1].projected.auxiliary == 1.0F);
    CHECK(split[1].vertices[2].projected.auxiliary == 2.0F);
    CHECK(RenderPacketBuilder::split_textured_quad(quad, false).size() == 1);

    RenderPolygonPacket all_near = make_triangle({20.0F, 80.0F, 40.0F}, 0x10);
    CHECK(RenderPacketBuilder::classify_polygon(
              all_near, RenderPacketBuilder::summarize_clip(all_near))
              .disposition
          == RenderBucketDisposition::rejected_clip);
    RenderPolygonPacket side_clipped = make_triangle({20.0F, 80.0F, 40.0F});
    side_clipped.vertices[0].projected.clip_flags = 0x01;
    CHECK(RenderPacketBuilder::classify_polygon(
              side_clipped, RenderPacketBuilder::summarize_clip(side_clipped))
              .disposition
          == RenderBucketDisposition::accepted);
    RenderPolygonPacket all_side_clipped =
        make_triangle({20.0F, 80.0F, 40.0F}, 0x01);
    CHECK(RenderPacketBuilder::classify_polygon(
              all_side_clipped,
              RenderPacketBuilder::summarize_clip(all_side_clipped))
              .disposition
          == RenderBucketDisposition::rejected_clip);
    RenderPolygonPacket partial_near = make_triangle({20.0F, 80.0F, 40.0F});
    partial_near.vertices[0].projected.clip_flags = 0x10;
    CHECK(RenderPacketBuilder::classify_polygon(
              partial_near, RenderPacketBuilder::summarize_clip(partial_near))
              .disposition
          == RenderBucketDisposition::requires_near_clip);

    RenderPolygonPacket scaled = make_triangle({20.0F, 80.0F, 40.0F});
    RenderDepthBucketOptions scaled_options;
    scaled_options.mode = RenderDepthBucketMode::scaled_nearest;
    scaled_options.choose_nearest_depth = true;
    const RenderBucketDecision scaled_decision =
        RenderPacketBuilder::classify_polygon(
            scaled,
            RenderPacketBuilder::summarize_clip(scaled),
            scaled_options);
    CHECK(scaled_decision.selected_depth == 19.0F);
    CHECK(scaled_decision.bucket_index == 4);

    RenderPolygonPacket forced = make_triangle({1.0F, 2.0F, 3.0F});
    RenderDepthBucketOptions forced_options;
    forced_options.mode = RenderDepthBucketMode::forced_offset;
    const RenderBucketDecision forced_decision =
        RenderPacketBuilder::classify_polygon(
            forced,
            RenderPacketBuilder::summarize_clip(forced),
            forced_options);
    CHECK((forced.flags & 0x80000000U) != 0);
    CHECK(forced_decision.selected_depth == 16383.0F);
    CHECK(forced_decision.bucket_index == 0xfff);

    std::vector<RenderPolygonPacket> source_order{
        make_triangle({10.0F, 10.0F, 10.0F}),
        make_triangle({11.0F, 11.0F, 11.0F}),
        make_triangle({20.0F, 20.0F, 20.0F}),
    };
    const RenderBucketBuildResult buckets = RenderPacketBuilder::bucketize(
        source_order);
    CHECK(buckets.decisions.size() == 3);
    CHECK(buckets.decisions[0].bucket_index == 2);
    CHECK(buckets.decisions[1].bucket_index == 2);
    CHECK(buckets.decisions[2].bucket_index == 5);
    CHECK(buckets.bucket_heads[2] == 1);
    CHECK(buckets.next_polygon[1] == 0);
    CHECK(buckets.bucket_heads[5] == 2);
    CHECK(buckets.next_polygon[2] == kRenderNoPolygonIndex);

    return 0;
}
