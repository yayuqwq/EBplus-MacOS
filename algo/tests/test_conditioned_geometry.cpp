// Focused U1C1 geometry-contract and fail-closed admission tests.

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <string>

#include "algo_bridge/conditioned_geometry.h"
#include "algo_bridge/filter_chain.h"

namespace {

using gui::ConditionedGeometry;
using gui::ConditionedTransformPlan;
using gui::FilterChain;
using gui::FilterStageRequest;
using gui::GeometryExtent;
using gui::GeometryPoint;
using gui::GeometryRect;
using gui::OrthogonalRotation;

ConditionedGeometry geometry(const GeometryExtent raw,
                             const ConditionedTransformPlan& plan = {},
                             const gui::GeometryRevision revision = 1) {
    std::string reason;
    auto result = ConditionedGeometry::create(raw, plan, revision, &reason);
    EXPECT_TRUE(result.has_value()) << reason;
    return *result;
}

void expect_point(const ConditionedGeometry& value, const int raw_x, const int raw_y,
                  const int output_x, const int output_y) {
    const auto point = value.map_raw_point({raw_x, raw_y});
    ASSERT_TRUE(point.has_value());
    EXPECT_EQ(*point, (GeometryPoint{output_x, output_y}));
}

FilterStageRequest request(const std::string& stage, const bool enabled) {
    FilterStageRequest value;
    value.stage = stage;
    value.enabled = enabled;
    return value;
}

} // namespace

TEST(ConditionedGeometry, IdentityAndNonSquareExtentDerivation) {
    const auto small = geometry({7, 5});
    EXPECT_EQ(small.raw_extent(), (GeometryExtent{7, 5}));
    EXPECT_EQ(small.output_extent(), (GeometryExtent{7, 5}));
    expect_point(small, 0, 0, 0, 0);
    expect_point(small, 6, 0, 6, 0);
    expect_point(small, 0, 4, 0, 4);
    expect_point(small, 6, 4, 6, 4);

    ConditionedTransformPlan rotate90;
    rotate90.rotate_enabled = true;
    rotate90.rotation = OrthogonalRotation::Degrees90;
    const auto wide = geometry({1280, 720}, rotate90);
    EXPECT_EQ(wide.output_extent(), (GeometryExtent{720, 1280}));

    ConditionedTransformPlan rotate180;
    rotate180.rotate_enabled = true;
    rotate180.rotation = OrthogonalRotation::Degrees180;
    EXPECT_EQ(geometry({1280, 720}, rotate180).output_extent(), (GeometryExtent{1280, 720}));

    ConditionedTransformPlan transpose;
    transpose.transpose_enabled = true;
    EXPECT_EQ(geometry({1280, 720}, transpose).output_extent(), (GeometryExtent{720, 1280}));
}

TEST(ConditionedGeometry, EdgePointsCoverEveryDiscreteTransform) {
    ConditionedTransformPlan flip_x;
    flip_x.flip_x_enabled = true;
    const auto x_flip = geometry({7, 5}, flip_x);
    expect_point(x_flip, 0, 0, 6, 0);
    expect_point(x_flip, 6, 0, 0, 0);
    expect_point(x_flip, 0, 4, 6, 4);
    expect_point(x_flip, 6, 4, 0, 4);

    ConditionedTransformPlan flip_y;
    flip_y.flip_y_enabled = true;
    const auto y_flip = geometry({7, 5}, flip_y);
    expect_point(y_flip, 0, 0, 0, 4);
    expect_point(y_flip, 6, 0, 6, 4);
    expect_point(y_flip, 0, 4, 0, 0);
    expect_point(y_flip, 6, 4, 6, 0);

    for (const auto rotation : {OrthogonalRotation::Degrees0, OrthogonalRotation::Degrees90,
                                OrthogonalRotation::Degrees180, OrthogonalRotation::Degrees270}) {
        ConditionedTransformPlan plan;
        plan.rotate_enabled = true;
        plan.rotation = rotation;
        const auto value = geometry({7, 5}, plan);
        if (rotation == OrthogonalRotation::Degrees0) {
            EXPECT_EQ(value.output_extent(), (GeometryExtent{7, 5}));
            expect_point(value, 0, 0, 0, 0);
            expect_point(value, 6, 4, 6, 4);
        } else if (rotation == OrthogonalRotation::Degrees90) {
            EXPECT_EQ(value.output_extent(), (GeometryExtent{5, 7}));
            expect_point(value, 0, 0, 4, 0);
            expect_point(value, 6, 0, 4, 6);
            expect_point(value, 0, 4, 0, 0);
            expect_point(value, 6, 4, 0, 6);
        } else if (rotation == OrthogonalRotation::Degrees180) {
            EXPECT_EQ(value.output_extent(), (GeometryExtent{7, 5}));
            expect_point(value, 0, 0, 6, 4);
            expect_point(value, 6, 0, 0, 4);
            expect_point(value, 0, 4, 6, 0);
            expect_point(value, 6, 4, 0, 0);
        } else {
            EXPECT_EQ(value.output_extent(), (GeometryExtent{5, 7}));
            expect_point(value, 0, 0, 0, 6);
            expect_point(value, 6, 0, 0, 0);
            expect_point(value, 0, 4, 4, 6);
            expect_point(value, 6, 4, 4, 0);
        }
    }

    ConditionedTransformPlan transpose;
    transpose.transpose_enabled = true;
    const auto transposed = geometry({7, 5}, transpose);
    EXPECT_EQ(transposed.output_extent(), (GeometryExtent{5, 7}));
    expect_point(transposed, 0, 0, 0, 0);
    expect_point(transposed, 6, 0, 0, 6);
    expect_point(transposed, 0, 4, 4, 0);
    expect_point(transposed, 6, 4, 4, 6);
}

TEST(ConditionedGeometry, RescaleMatchesOpenEbProducerExtentAndPoints) {
    ConditionedTransformPlan downscale;
    downscale.rescale_enabled = true;
    downscale.scale_x = 0.5f;
    downscale.scale_y = 0.5f;
    const auto smaller = geometry({7, 5}, downscale);
    EXPECT_EQ(smaller.output_extent(), (GeometryExtent{4, 3}));
    expect_point(smaller, 0, 0, 0, 0);
    expect_point(smaller, 6, 4, 3, 2);

    ConditionedTransformPlan same_scale;
    same_scale.rescale_enabled = true;
    const auto same = geometry({7, 5}, same_scale);
    EXPECT_EQ(same.output_extent(), (GeometryExtent{7, 5}));
    expect_point(same, 6, 4, 6, 4);

    ConditionedTransformPlan upscale;
    upscale.rescale_enabled = true;
    upscale.scale_x = 2.0f;
    upscale.scale_y = 2.0f;
    const auto larger = geometry({7, 5}, upscale);
    EXPECT_EQ(larger.output_extent(), (GeometryExtent{13, 9}));
    expect_point(larger, 0, 0, 0, 0);
    expect_point(larger, 6, 4, 12, 8);

    EXPECT_EQ(geometry({1280, 720}, downscale).output_extent(), (GeometryExtent{640, 360}));
    EXPECT_EQ(geometry({1280, 720}, upscale).output_extent(), (GeometryExtent{2559, 1439}));

    ConditionedTransformPlan asymmetric;
    asymmetric.rescale_enabled = true;
    asymmetric.scale_x = 2.0f;
    asymmetric.scale_y = 0.5f;
    const auto asymmetric_result = geometry({7, 5}, asymmetric);
    EXPECT_EQ(asymmetric_result.output_extent(), (GeometryExtent{13, 3}));
    expect_point(asymmetric_result, 0, 0, 0, 0);
    expect_point(asymmetric_result, 6, 0, 12, 0);
    expect_point(asymmetric_result, 0, 4, 0, 2);
    expect_point(asymmetric_result, 6, 4, 12, 2);
}

TEST(ConditionedGeometry, CompositionUsesOnlyFilterChainExecutionOrder) {
    ConditionedTransformPlan flip_rotate;
    flip_rotate.flip_x_enabled = true;
    flip_rotate.rotate_enabled = true;
    flip_rotate.rotation = OrthogonalRotation::Degrees90;
    const auto first = geometry({7, 5}, flip_rotate);
    expect_point(first, 0, 0, 4, 6);

    ConditionedTransformPlan flip_transpose;
    flip_transpose.flip_x_enabled = true;
    flip_transpose.transpose_enabled = true;
    const auto second = geometry({7, 5}, flip_transpose);
    expect_point(second, 0, 0, 0, 6);

    ConditionedTransformPlan rotate_transpose;
    rotate_transpose.rotate_enabled = true;
    rotate_transpose.rotation = OrthogonalRotation::Degrees90;
    rotate_transpose.transpose_enabled = true;
    const auto third = geometry({7, 5}, rotate_transpose);
    EXPECT_EQ(third.output_extent(), (GeometryExtent{7, 5}));
    expect_point(third, 0, 0, 0, 4);

    ConditionedTransformPlan transpose_rescale;
    transpose_rescale.transpose_enabled = true;
    transpose_rescale.rescale_enabled = true;
    transpose_rescale.scale_x = 0.5f;
    transpose_rescale.scale_y = 0.5f;
    const auto fourth = geometry({7, 5}, transpose_rescale);
    EXPECT_EQ(fourth.output_extent(), (GeometryExtent{3, 4}));
    expect_point(fourth, 6, 4, 2, 3);
}

TEST(ConditionedGeometry, RectangleMappingUsesHalfOpenAndConservativeSemantics) {
    ConditionedTransformPlan rotate;
    rotate.rotate_enabled = true;
    rotate.rotation = OrthogonalRotation::Degrees90;
    const auto rotated = geometry({7, 5}, rotate);
    const auto forward = rotated.map_raw_rectangle({1, 1, 4, 3});
    ASSERT_TRUE(forward.has_value());
    EXPECT_EQ(*forward, (GeometryRect{2, 1, 4, 4}));
    const auto reverse = rotated.map_output_rectangle_to_raw_cover(*forward);
    ASSERT_TRUE(reverse.has_value());
    EXPECT_EQ(*reverse, (GeometryRect{1, 1, 4, 3}));

    ConditionedTransformPlan downscale;
    downscale.rescale_enabled = true;
    downscale.scale_x = 0.5f;
    downscale.scale_y = 0.5f;
    const auto smaller = geometry({7, 5}, downscale);
    const GeometryRect output_selection{1, 1, 3, 2};
    const auto forward_rescaled = smaller.map_raw_rectangle({2, 2, 6, 4});
    ASSERT_TRUE(forward_rescaled.has_value());
    EXPECT_EQ(*forward_rescaled, output_selection);
    const auto raw_cover = smaller.map_output_rectangle_to_raw_cover(output_selection);
    ASSERT_TRUE(raw_cover.has_value());
    EXPECT_EQ(*raw_cover, (GeometryRect{2, 2, 6, 4}));
    for (int y = 0; y < 5; ++y) {
        for (int x = 0; x < 7; ++x) {
            const auto mapped = smaller.map_raw_point({x, y});
            ASSERT_TRUE(mapped.has_value());
            const bool lands_in_selection = mapped->x >= output_selection.x0 &&
                                            mapped->x < output_selection.x1 &&
                                            mapped->y >= output_selection.y0 &&
                                            mapped->y < output_selection.y1;
            if (lands_in_selection) {
                EXPECT_GE(x, raw_cover->x0);
                EXPECT_LT(x, raw_cover->x1);
                EXPECT_GE(y, raw_cover->y0);
                EXPECT_LT(y, raw_cover->y1);
            }
        }
    }

    ConditionedTransformPlan upscale;
    upscale.rescale_enabled = true;
    upscale.scale_x = 2.0f;
    upscale.scale_y = 2.0f;
    const auto larger = geometry({7, 5}, upscale);
    const auto gap_cover = larger.map_output_rectangle_to_raw_cover({1, 1, 2, 2});
    ASSERT_TRUE(gap_cover.has_value());
    EXPECT_EQ(*gap_cover, (GeometryRect{1, 1, 1, 1}));
    EXPECT_TRUE(gap_cover->is_empty());
}

TEST(ConditionedGeometry, RevisionDistinguishesMappingOnlyAndSourceMutations) {
    ConditionedTransformPlan flip;
    flip.flip_x_enabled = true;
    const auto first = geometry({7, 5}, flip, 10);
    const auto second = geometry({7, 5}, flip, 11);
    EXPECT_TRUE(first.same_mapping_as(second));
    EXPECT_NE(first.revision(), second.revision());
    EXPECT_EQ(first.output_extent(), second.output_extent());

    FilterChain chain;
    ASSERT_TRUE(chain.set_geometry(7, 5));
    const auto initial = chain.conditioned_geometry();
    ASSERT_TRUE(initial.has_value());
    ASSERT_TRUE(chain.set_stage_enabled("flip_x", true));
    const auto after_flip = chain.conditioned_geometry();
    ASSERT_TRUE(after_flip.has_value());
    EXPECT_EQ(after_flip->output_extent(), initial->output_extent());
    EXPECT_NE(after_flip->revision(), initial->revision());

    ASSERT_TRUE(chain.set_stage_enabled("flip_x", false));
    const auto after_disable = chain.conditioned_geometry();
    ASSERT_TRUE(after_disable.has_value());
    EXPECT_NE(after_disable->revision(), after_flip->revision());

    FilterStageRequest rotate180 = request("rotate", true);
    rotate180.parameters["rotation"] = "180";
    ASSERT_TRUE(chain.try_apply_stage(rotate180).accepted);
    const auto after_rotation = chain.conditioned_geometry();
    ASSERT_TRUE(after_rotation.has_value());
    EXPECT_EQ(after_rotation->output_extent(), initial->output_extent());
    EXPECT_NE(after_rotation->revision(), after_disable->revision());

    FilterStageRequest rotate0 = request("rotate", true);
    rotate0.parameters["rotation"] = "0";
    ASSERT_TRUE(chain.try_apply_stage(rotate0).accepted);
    const auto after_parameter = chain.conditioned_geometry();
    ASSERT_TRUE(after_parameter.has_value());
    EXPECT_NE(after_parameter->revision(), after_rotation->revision());

    ASSERT_TRUE(chain.set_geometry(5, 7));
    const auto after_source = chain.conditioned_geometry();
    ASSERT_TRUE(after_source.has_value());
    EXPECT_NE(after_source->revision(), after_parameter->revision());
    EXPECT_EQ(after_source->raw_extent(), (GeometryExtent{5, 7}));
    EXPECT_EQ(after_source->output_extent(), (GeometryExtent{5, 7}));
}

TEST(ConditionedGeometry, InvalidPlansFailClosedBeforeSnapshotCreation) {
    std::string reason;
    EXPECT_FALSE(ConditionedGeometry::create({0, 5}, {}, 1, &reason).has_value());
    EXPECT_FALSE(ConditionedGeometry::create({-1, 5}, {}, 1, &reason).has_value());
    EXPECT_FALSE(ConditionedGeometry::create({7, 5}, {}, 0, &reason).has_value());
    EXPECT_FALSE(ConditionedGeometry::create({65537, 1}, {}, 1, &reason).has_value());

    ConditionedTransformPlan invalid_scale;
    invalid_scale.rescale_enabled = true;
    invalid_scale.scale_x = 0.0f;
    EXPECT_FALSE(ConditionedGeometry::create({7, 5}, invalid_scale, 1, &reason).has_value());
    invalid_scale.scale_x = -1.0f;
    EXPECT_FALSE(ConditionedGeometry::create({7, 5}, invalid_scale, 1, &reason).has_value());
    invalid_scale.scale_x = std::numeric_limits<float>::infinity();
    EXPECT_FALSE(ConditionedGeometry::create({7, 5}, invalid_scale, 1, &reason).has_value());
    invalid_scale.scale_x = std::numeric_limits<float>::quiet_NaN();
    EXPECT_FALSE(ConditionedGeometry::create({7, 5}, invalid_scale, 1, &reason).has_value());

    ConditionedTransformPlan unsupported_rotation;
    unsupported_rotation.rotate_enabled = true;
    unsupported_rotation.rotation = static_cast<OrthogonalRotation>(45);
    EXPECT_FALSE(ConditionedGeometry::create({7, 5}, unsupported_rotation, 1, &reason).has_value());

    ConditionedTransformPlan overflow;
    overflow.rescale_enabled = true;
    overflow.scale_x = 2.0f;
    overflow.scale_y = 1.0f;
    EXPECT_FALSE(ConditionedGeometry::create({65536, 1}, overflow, 1, &reason).has_value());
}

TEST(FilterChainAdmission, UnsupportedExtentChangingAndArbitraryRequestsRemainInactive) {
    FilterChain chain;
    ASSERT_TRUE(chain.set_geometry(7, 5));
    const auto initial = chain.conditioned_geometry();
    ASSERT_TRUE(initial.has_value());

    for (const auto& stage : {std::string("transpose"), std::string("rescale")}) {
        auto result = chain.try_apply_stage(request(stage, true));
        EXPECT_FALSE(result.accepted) << stage;
        EXPECT_FALSE(result.committed_state.enabled) << stage;
        EXPECT_FALSE(chain.is_stage_enabled(stage)) << stage;
    }

    for (const auto rotation : {std::string("90"), std::string("270"), std::string("0.5")}) {
        auto rotate = request("rotate", true);
        rotate.parameters["rotation"] = rotation;
        const auto result = chain.try_apply_stage(rotate);
        EXPECT_FALSE(result.accepted) << rotation;
        EXPECT_FALSE(result.committed_state.enabled) << rotation;
        const auto state = chain.stage_state("rotate");
        ASSERT_TRUE(state.has_value());
        EXPECT_EQ(state->parameters.at("rotation"), "0");
    }

    const auto final = chain.conditioned_geometry();
    ASSERT_TRUE(final.has_value());
    EXPECT_EQ(final->revision(), initial->revision());
}

TEST(FilterChainAdmission, RoiAndProcessedRecordingGateBothOrderingDirections) {
    FilterChain chain;
    ASSERT_TRUE(chain.set_geometry(7, 5));
    ASSERT_TRUE(chain.try_set_raw_roi_or_roni_active(true));
    const auto rejected_flip = chain.try_apply_stage(request("flip_x", true));
    EXPECT_FALSE(rejected_flip.accepted);
    EXPECT_FALSE(chain.is_stage_enabled("flip_x"));
    ASSERT_TRUE(chain.try_set_raw_roi_or_roni_active(false));
    ASSERT_TRUE(chain.try_apply_stage(request("flip_x", true)).accepted);
    std::string reason;
    EXPECT_FALSE(chain.try_set_raw_roi_or_roni_active(true, &reason));
    EXPECT_FALSE(reason.empty());
    ASSERT_TRUE(chain.set_stage_enabled("flip_x", false));

    ASSERT_TRUE(chain.try_set_processed_recording_active(true));
    auto rotate180 = request("rotate", true);
    rotate180.parameters["rotation"] = "180";
    const auto rejected_rotate = chain.try_apply_stage(rotate180);
    EXPECT_FALSE(rejected_rotate.accepted);
    EXPECT_FALSE(chain.is_stage_enabled("rotate"));
    ASSERT_TRUE(chain.try_set_processed_recording_active(false));

    ASSERT_TRUE(chain.try_apply_stage(rotate180).accepted);
    EXPECT_FALSE(chain.try_set_processed_recording_active(true, &reason));
    EXPECT_FALSE(reason.empty());
}

TEST(FilterChainAdmission, InvalidSourceGeometryStillAllowsCoordinateStageRecovery) {
    FilterChain chain;
    ASSERT_TRUE(chain.set_geometry(7, 5));
    ASSERT_TRUE(chain.set_stage_enabled("flip_x", true));

    auto rotate180 = request("rotate", true);
    rotate180.parameters["rotation"] = "180";
    ASSERT_TRUE(chain.try_apply_stage(rotate180).accepted);

    EXPECT_FALSE(chain.set_geometry(0, 5));
    EXPECT_FALSE(chain.conditioned_geometry().has_value());

    // The failed source change must not trap the UI/backend in an enabled
    // transform state. These removals still publish no batch until a valid
    // source geometry is installed again.
    EXPECT_TRUE(chain.set_stage_enabled("flip_x", false));
    EXPECT_TRUE(chain.set_stage_enabled("rotate", false));
    EXPECT_FALSE(chain.is_stage_enabled("flip_x"));
    EXPECT_FALSE(chain.is_stage_enabled("rotate"));
    EXPECT_FALSE(chain.conditioned_geometry().has_value());

    ASSERT_TRUE(chain.set_geometry(7, 5));
    const auto recovered = chain.conditioned_geometry();
    ASSERT_TRUE(recovered.has_value());
    EXPECT_EQ(recovered->output_extent(), (GeometryExtent{7, 5}));
    EXPECT_FALSE(recovered->has_non_identity_coordinate_plan());
}

TEST(FilterChainConditionedBatch, GeometrySnapshotStaysBoundToPublishedEvents) {
    FilterChain chain;
    ASSERT_TRUE(chain.set_geometry(7, 5));
    ASSERT_TRUE(chain.set_stage_enabled("flip_x", true));
    Metavision::EventCD event(0, 0, 1, 42);
    const auto batch = chain.process_conditioned(&event, &event + 1);
    ASSERT_TRUE(batch.has_value());
    ASSERT_EQ(batch->events.size(), 1u);
    EXPECT_EQ(batch->events.front().x, 6);
    EXPECT_EQ(batch->geometry.raw_extent(), (GeometryExtent{7, 5}));
    EXPECT_EQ(batch->geometry.output_extent(), (GeometryExtent{7, 5}));
    const auto published_revision = batch->geometry.revision();

    FilterStageRequest rotate180 = request("rotate", true);
    rotate180.parameters["rotation"] = "180";
    ASSERT_TRUE(chain.try_apply_stage(rotate180).accepted);
    const auto current = chain.conditioned_geometry();
    ASSERT_TRUE(current.has_value());
    EXPECT_NE(current->revision(), published_revision);
    EXPECT_EQ(batch->geometry.revision(), published_revision);
    const auto published_mapping = batch->geometry.map_raw_point({0, 0});
    ASSERT_TRUE(published_mapping.has_value());
    EXPECT_EQ(*published_mapping, (GeometryPoint{6, 0}));
}
