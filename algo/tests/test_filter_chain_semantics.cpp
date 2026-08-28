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

TEST(FilterChainSemantics, StageParametersRequireCompleteNumericInput) {
    gui::FilterChain chain = configured_chain();

    EXPECT_TRUE(chain.set_stage_param("polarity_filter", "polarity", "1"));
    EXPECT_TRUE(chain.set_stage_param("polarity_filter", "polarity", " 1 "));
    EXPECT_FALSE(chain.set_stage_param("polarity_filter", "polarity", "1x"));
}

TEST(FilterChainSemantics, FlipsRespectNonSquareGeometry) {
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

}

TEST(FilterChainSemantics, Rotate180PreservesNonSquareBounds) {
    const std::vector<Event> input = input_events();

    gui::FilterChain chain = configured_chain();
    ASSERT_TRUE(chain.set_stage_param("rotate", "rotation", "180"));
    ASSERT_TRUE(chain.set_stage_enabled("rotate", true));
    const std::vector<Event> output = process(chain, input);
    ASSERT_EQ(output.size(), input.size());
    expect_event(output[0], 6, 4, 0, 100);
    expect_event(output[1], 0, 0, 1, 200);
    expect_event(output[2], 4, 1, 1, 300);
}

TEST(FilterChainSemantics, UnsafeExtentChangingStagesFailClosed) {
    gui::FilterChain transpose = configured_chain();
    EXPECT_FALSE(transpose.set_stage_enabled("transpose", true));
    EXPECT_FALSE(transpose.is_stage_enabled("transpose"));

    gui::FilterChain rotate = configured_chain();
    ASSERT_TRUE(rotate.set_stage_param("rotate", "rotation", "90"));
    EXPECT_FALSE(rotate.set_stage_enabled("rotate", true));
    EXPECT_FALSE(rotate.is_stage_enabled("rotate"));

    gui::FilterChain rescale = configured_chain();
    ASSERT_TRUE(rescale.set_stage_param("rescale", "scale_width", "2"));
    ASSERT_TRUE(rescale.set_stage_param("rescale", "scale_height", "0.5"));
    EXPECT_FALSE(rescale.set_stage_enabled("rescale", true));
    EXPECT_FALSE(rescale.is_stage_enabled("rescale"));
}
