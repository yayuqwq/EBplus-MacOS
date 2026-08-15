// Exact synthetic semantics for the OpenEB FilterChain catalog stages.

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include <metavision/sdk/base/events/event_cd.h>

#include "algo_bridge/filter_chain.h"

namespace {

constexpr int kWidth = 7;
constexpr int kHeight = 5;

using Event = Metavision::EventCD;

Event make_event(int x, int y, int polarity, Metavision::timestamp timestamp) {
    return Event(static_cast<unsigned short>(x), static_cast<unsigned short>(y),
                 static_cast<short>(polarity), timestamp);
}

std::vector<Event> input_events() {
    return {
        make_event(0, 0, 0, 100),
        make_event(6, 4, 1, 200),
        make_event(2, 3, 1, 300),
    };
}

std::vector<Event> process(gui::FilterChain& chain, const std::vector<Event>& input) {
    std::vector<Event> output;
    chain.process(input.data(), input.data() + input.size(), output);
    return output;
}

void expect_event(const Event& actual, int x, int y, int polarity,
                  Metavision::timestamp timestamp) {
    EXPECT_EQ(actual.x, x);
    EXPECT_EQ(actual.y, y);
    EXPECT_EQ(actual.p, polarity);
    EXPECT_EQ(actual.t, timestamp);
}

gui::FilterChain configured_chain() {
    gui::FilterChain chain;
    chain.set_geometry(kWidth, kHeight);
    return chain;
}

} // namespace

TEST(FilterChainSemantics, ExposesOnlyTheCurrentSevenTransformStages) {
    gui::FilterChain chain = configured_chain();
    const char* stages[] = {
        "polarity_filter", "polarity_invert", "flip_x", "flip_y",
        "rotate", "transpose", "rescale",
    };
    for (const char* stage : stages) {
        EXPECT_NE(chain.stage(stage), nullptr) << stage;
        EXPECT_FALSE(chain.is_stage_enabled(stage)) << stage;
    }
    EXPECT_EQ(chain.stage("roi_filter"), nullptr);
}

TEST(FilterChainSemantics, PolarityFilterAndInvertPreserveExactEventFields) {
    const std::vector<Event> input = input_events();

    gui::FilterChain filter = configured_chain();
    ASSERT_TRUE(filter.set_stage_param("polarity_filter", "polarity", "1"));
    filter.set_stage_enabled("polarity_filter", true);
    const std::vector<Event> filtered = process(filter, input);
    ASSERT_EQ(filtered.size(), 2u);
    expect_event(filtered[0], 6, 4, 1, 200);
    expect_event(filtered[1], 2, 3, 1, 300);

    gui::FilterChain inverted = configured_chain();
    inverted.set_stage_enabled("polarity_invert", true);
    const std::vector<Event> output = process(inverted, input);
    ASSERT_EQ(output.size(), input.size());
    expect_event(output[0], 0, 0, 1, 100);
    expect_event(output[1], 6, 4, 0, 200);
    expect_event(output[2], 2, 3, 0, 300);
}

TEST(FilterChainSemantics, FlipsAndTransposeRespectNonSquareGeometry) {
    const std::vector<Event> input = input_events();

    gui::FilterChain flip_x = configured_chain();
    flip_x.set_stage_enabled("flip_x", true);
    const std::vector<Event> x_output = process(flip_x, input);
    ASSERT_EQ(x_output.size(), input.size());
    expect_event(x_output[0], 6, 0, 0, 100);
    expect_event(x_output[1], 0, 4, 1, 200);
    expect_event(x_output[2], 4, 3, 1, 300);

    gui::FilterChain flip_y = configured_chain();
    flip_y.set_stage_enabled("flip_y", true);
    const std::vector<Event> y_output = process(flip_y, input);
    ASSERT_EQ(y_output.size(), input.size());
    expect_event(y_output[0], 0, 4, 0, 100);
    expect_event(y_output[1], 6, 0, 1, 200);
    expect_event(y_output[2], 2, 1, 1, 300);

    gui::FilterChain transpose = configured_chain();
    transpose.set_stage_enabled("transpose", true);
    const std::vector<Event> transposed = process(transpose, input);
    ASSERT_EQ(transposed.size(), input.size());
    expect_event(transposed[0], 0, 0, 0, 100);
    expect_event(transposed[1], 4, 6, 1, 200);
    expect_event(transposed[2], 3, 2, 1, 300);
    for (const Event& event : transposed) {
        EXPECT_LT(event.x, kHeight);
        EXPECT_LT(event.y, kWidth);
    }
}

TEST(FilterChainSemantics, OrthogonalRotationUsesSwappedNonSquareBounds) {
    const std::vector<Event> input = input_events();

    gui::FilterChain clockwise = configured_chain();
    ASSERT_TRUE(clockwise.set_stage_param("rotate", "rotation", "90"));
    clockwise.set_stage_enabled("rotate", true);
    const std::vector<Event> rotated_90 = process(clockwise, input);
    ASSERT_EQ(rotated_90.size(), input.size());
    expect_event(rotated_90[0], 4, 0, 0, 100);
    expect_event(rotated_90[1], 0, 6, 1, 200);
    expect_event(rotated_90[2], 1, 2, 1, 300);

    gui::FilterChain counter_clockwise = configured_chain();
    ASSERT_TRUE(counter_clockwise.set_stage_param("rotate", "rotation", "270"));
    counter_clockwise.set_stage_enabled("rotate", true);
    const std::vector<Event> rotated_270 = process(counter_clockwise, input);
    ASSERT_EQ(rotated_270.size(), input.size());
    expect_event(rotated_270[0], 0, 6, 0, 100);
    expect_event(rotated_270[1], 4, 0, 1, 200);
    expect_event(rotated_270[2], 3, 4, 1, 300);

    for (const Event& event : rotated_90) {
        EXPECT_LT(event.x, kHeight);
        EXPECT_LT(event.y, kWidth);
    }
    for (const Event& event : rotated_270) {
        EXPECT_LT(event.x, kHeight);
        EXPECT_LT(event.y, kWidth);
    }
}

TEST(FilterChainSemantics, RescaleUsesOpenEBTruncationAndScaleOffsets) {
    gui::FilterChain chain = configured_chain();
    ASSERT_TRUE(chain.set_stage_param("rescale", "scale_width", "2"));
    ASSERT_TRUE(chain.set_stage_param("rescale", "scale_height", "0.5"));
    chain.set_stage_enabled("rescale", true);

    const std::vector<Event> output = process(chain, input_events());
    ASSERT_EQ(output.size(), 3u);
    expect_event(output[0], 0, 0, 0, 100);
    expect_event(output[1], 12, 2, 1, 200);
    expect_event(output[2], 4, 1, 1, 300);
}
