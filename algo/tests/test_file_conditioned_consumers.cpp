// U1C2 synthetic file-consumer integration tests.

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QImage>

#include <memory>
#include <string>
#include <vector>

#include <metavision/sdk/base/events/event_cd.h>

#include "algo_bridge/filter_chain.h"
#include "app/file_frame_generator.h"

namespace {

using gui::ConditionedBatch;
using gui::FilterAdmissionSource;
using gui::FilterChain;
using gui::GeometryExtent;
using Event = Metavision::EventCD;

constexpr int kWidth = 7;
constexpr int kHeight = 5;

Event event(const int x, const int y, const int polarity, const Metavision::timestamp t) {
    return Event(static_cast<unsigned short>(x), static_cast<unsigned short>(y),
                 static_cast<short>(polarity), t);
}

struct RenderCapture {
    std::shared_ptr<const ConditionedBatch> batch;
    QImage frame;
};

class FileRenderHarness {
public:
    FileRenderHarness() {
        ready_ = chain_.set_geometry(kWidth, kHeight) &&
                 chain_.try_set_admission_source(FilterAdmissionSource::File);
        generator_.set_geometry(kWidth, kHeight);
        generator_.set_filter_chain(&chain_);
        generator_.set_accumulation_time_us(100);
        QObject::connect(&generator_, &gui::FileFrameGenerator::events_window_ready,
                         &generator_, [this](std::shared_ptr<const ConditionedBatch> batch,
                                             Metavision::timestamp) {
                             capture_.batch = batch;
                         });
        QObject::connect(&generator_, &gui::FileFrameGenerator::frame_ready,
                         &generator_, [this](QImage frame, Metavision::timestamp) {
                             capture_.frame = frame;
                         });
    }

    FilterChain& chain() { return chain_; }
    gui::FileFrameGenerator& generator() { return generator_; }
    bool ready() const { return ready_; }

    void load(const std::vector<Event>& events) {
        if (!events.empty()) {
            generator_.add_events(events.data(), events.data() + events.size());
        }
        generator_.set_duration_us(100);
    }

    RenderCapture render_loaded() {
        capture_ = RenderCapture{};
        generator_.seek(0);
        return capture_;
    }

    RenderCapture render(const std::vector<Event>& events) {
        load(events);
        return render_loaded();
    }

private:
    // The chain is declared before the generator because the generator keeps
    // a non-owning pointer to its single file conditioning owner.
    FilterChain chain_;
    gui::FileFrameGenerator generator_;
    RenderCapture capture_;
    bool ready_{false};
};

void expect_batch_in_bounds(const RenderCapture& capture, const GeometryExtent expected_extent) {
    ASSERT_NE(capture.batch, nullptr);
    EXPECT_EQ(capture.batch->geometry.output_extent(), expected_extent);
    for (const Event& value : capture.batch->events) {
        EXPECT_GE(static_cast<int>(value.x), 0);
        EXPECT_LT(static_cast<int>(value.x), expected_extent.width);
        EXPECT_GE(static_cast<int>(value.y), 0);
        EXPECT_LT(static_cast<int>(value.y), expected_extent.height);
    }
}

void configure_rotation(FilterChain& chain, const int degrees) {
    ASSERT_TRUE(chain.set_stage_param("rotate", "rotation", std::to_string(degrees)));
    ASSERT_TRUE(chain.set_stage_enabled("rotate", true));
}

} // namespace

TEST(FileConditionedConsumers, ConditionedPlansDriveTheRasterizedFrame) {
    struct Case {
        const char* name;
        void (*configure)(FilterChain&);
        GeometryExtent extent;
        int x;
        int y;
    };

    const Case cases[] = {
        {"identity", [](FilterChain&) {}, {7, 5}, 6, 4},
        {"flipX", [](FilterChain& chain) {
             ASSERT_TRUE(chain.set_stage_enabled("flip_x", true));
         }, {7, 5}, 0, 4},
        {"flipY", [](FilterChain& chain) {
             ASSERT_TRUE(chain.set_stage_enabled("flip_y", true));
         }, {7, 5}, 6, 0},
        {"rotate180", [](FilterChain& chain) { configure_rotation(chain, 180); },
         {7, 5}, 0, 0},
        {"rotate90", [](FilterChain& chain) { configure_rotation(chain, 90); }, {5, 7}, 0, 6},
        {"rotate270", [](FilterChain& chain) { configure_rotation(chain, 270); }, {5, 7}, 4, 0},
        {"transpose", [](FilterChain& chain) {
             ASSERT_TRUE(chain.set_stage_enabled("transpose", true));
         }, {5, 7}, 4, 6},
        {"flipThenTranspose", [](FilterChain& chain) {
             ASSERT_TRUE(chain.set_stage_enabled("flip_x", true));
             ASSERT_TRUE(chain.set_stage_enabled("transpose", true));
         }, {5, 7}, 4, 0},
        {"rotateThenTranspose", [](FilterChain& chain) {
             configure_rotation(chain, 90);
             ASSERT_TRUE(chain.set_stage_enabled("transpose", true));
         }, {7, 5}, 6, 0},
        {"downscale", [](FilterChain& chain) {
             ASSERT_TRUE(chain.set_stage_param("rescale", "scale_width", "0.5"));
             ASSERT_TRUE(chain.set_stage_param("rescale", "scale_height", "0.5"));
             ASSERT_TRUE(chain.set_stage_enabled("rescale", true));
         }, {4, 3}, 3, 2},
        {"upscale", [](FilterChain& chain) {
             ASSERT_TRUE(chain.set_stage_param("rescale", "scale_width", "2"));
             ASSERT_TRUE(chain.set_stage_param("rescale", "scale_height", "2"));
             ASSERT_TRUE(chain.set_stage_enabled("rescale", true));
         }, {13, 9}, 12, 8},
        {"transposeThenDownscale", [](FilterChain& chain) {
             ASSERT_TRUE(chain.set_stage_enabled("transpose", true));
             ASSERT_TRUE(chain.set_stage_param("rescale", "scale_width", "0.5"));
             ASSERT_TRUE(chain.set_stage_param("rescale", "scale_height", "0.5"));
             ASSERT_TRUE(chain.set_stage_enabled("rescale", true));
         }, {3, 4}, 2, 3},
    };

    for (const Case& test : cases) {
        SCOPED_TRACE(test.name);
        FileRenderHarness harness;
        ASSERT_TRUE(harness.ready());
        test.configure(harness.chain());
        const RenderCapture capture = harness.render({event(6, 4, 1, 10)});

        ASSERT_NE(capture.batch, nullptr);
        expect_batch_in_bounds(capture, test.extent);
        ASSERT_EQ(capture.batch->events.size(), 1u);
        EXPECT_EQ(static_cast<int>(capture.batch->events.front().x), test.x);
        EXPECT_EQ(static_cast<int>(capture.batch->events.front().y), test.y);
        ASSERT_FALSE(capture.frame.isNull());
        EXPECT_EQ(capture.frame.width(), test.extent.width);
        EXPECT_EQ(capture.frame.height(), test.extent.height);
        const int background_x = test.x == 0 ? test.extent.width - 1 : 0;
        const int background_y = test.y == 0 && background_x == test.x
            ? test.extent.height - 1
            : 0;
        EXPECT_NE(capture.frame.pixel(test.x, test.y),
                  capture.frame.pixel(background_x, background_y));
    }
}

TEST(FileConditionedConsumers, ActualTransformOrderAndMappingOnlyRevisionReachConsumers) {
    FileRenderHarness harness;
    ASSERT_TRUE(harness.ready());
    ASSERT_TRUE(harness.chain().set_stage_enabled("flip_x", true));
    configure_rotation(harness.chain(), 90);
    harness.load({event(0, 0, 1, 10)});

    const RenderCapture first = harness.render_loaded();
    ASSERT_NE(first.batch, nullptr);
    expect_batch_in_bounds(first, {5, 7});
    ASSERT_EQ(first.batch->events.size(), 1u);
    EXPECT_EQ(static_cast<int>(first.batch->events.front().x), 4);
    EXPECT_EQ(static_cast<int>(first.batch->events.front().y), 6);

    ASSERT_TRUE(harness.chain().set_stage_enabled("rotate", false));
    const RenderCapture second = harness.render_loaded();
    ASSERT_NE(second.batch, nullptr);
    expect_batch_in_bounds(second, {7, 5});
    EXPECT_NE(first.batch->geometry.revision(), second.batch->geometry.revision());
    EXPECT_EQ(second.batch->geometry.output_extent(), (GeometryExtent{7, 5}));
    ASSERT_EQ(second.batch->events.size(), 1u);
    EXPECT_EQ(static_cast<int>(second.batch->events.front().x), 6);
    EXPECT_EQ(static_cast<int>(second.batch->events.front().y), 0);

    ASSERT_TRUE(harness.chain().set_stage_enabled("flip_x", false));
    const RenderCapture third = harness.render_loaded();
    ASSERT_NE(third.batch, nullptr);
    expect_batch_in_bounds(third, {7, 5});
    EXPECT_NE(second.batch->geometry.revision(), third.batch->geometry.revision());
    EXPECT_EQ(second.batch->geometry.output_extent(), third.batch->geometry.output_extent());
    ASSERT_EQ(third.batch->events.size(), 1u);
    EXPECT_EQ(static_cast<int>(third.batch->events.front().x), 0);
    EXPECT_EQ(static_cast<int>(third.batch->events.front().y), 0);
}

TEST(FileConditionedConsumers, RawRoiAndRoniSelectBeforeConditioning) {
    const std::vector<Event> raw_events = {event(2, 1, 1, 10), event(6, 4, 1, 20)};

    FileRenderHarness flip_harness;
    ASSERT_TRUE(flip_harness.ready());
    ASSERT_TRUE(flip_harness.chain().set_stage_enabled("flip_x", true));
    flip_harness.generator().set_display_roi(true, 1, 1, 2, 1, false);
    const RenderCapture flip_capture = flip_harness.render(raw_events);
    ASSERT_NE(flip_capture.batch, nullptr);
    expect_batch_in_bounds(flip_capture, {7, 5});
    ASSERT_EQ(flip_capture.batch->events.size(), 1u);
    EXPECT_EQ(static_cast<int>(flip_capture.batch->events.front().x), 4);
    EXPECT_EQ(static_cast<int>(flip_capture.batch->events.front().y), 1);

    FileRenderHarness roi_harness;
    ASSERT_TRUE(roi_harness.ready());
    configure_rotation(roi_harness.chain(), 90);
    roi_harness.generator().set_display_roi(true, 1, 1, 2, 1, false);
    const RenderCapture roi_capture = roi_harness.render(raw_events);
    ASSERT_NE(roi_capture.batch, nullptr);
    expect_batch_in_bounds(roi_capture, {5, 7});
    ASSERT_EQ(roi_capture.batch->events.size(), 1u);
    EXPECT_EQ(static_cast<int>(roi_capture.batch->events.front().x), 3);
    EXPECT_EQ(static_cast<int>(roi_capture.batch->events.front().y), 2);

    FileRenderHarness transpose_harness;
    ASSERT_TRUE(transpose_harness.ready());
    ASSERT_TRUE(transpose_harness.chain().set_stage_enabled("transpose", true));
    transpose_harness.generator().set_display_roi(true, 1, 1, 2, 1, false);
    const RenderCapture transpose_capture = transpose_harness.render(raw_events);
    ASSERT_NE(transpose_capture.batch, nullptr);
    expect_batch_in_bounds(transpose_capture, {5, 7});
    ASSERT_EQ(transpose_capture.batch->events.size(), 1u);
    EXPECT_EQ(static_cast<int>(transpose_capture.batch->events.front().x), 1);
    EXPECT_EQ(static_cast<int>(transpose_capture.batch->events.front().y), 2);

    FileRenderHarness rescale_harness;
    ASSERT_TRUE(rescale_harness.ready());
    ASSERT_TRUE(rescale_harness.chain().set_stage_param("rescale", "scale_width", "2"));
    ASSERT_TRUE(rescale_harness.chain().set_stage_param("rescale", "scale_height", "2"));
    ASSERT_TRUE(rescale_harness.chain().set_stage_enabled("rescale", true));
    rescale_harness.generator().set_display_roi(true, 1, 1, 2, 1, false);
    const RenderCapture rescale_capture = rescale_harness.render(raw_events);
    ASSERT_NE(rescale_capture.batch, nullptr);
    expect_batch_in_bounds(rescale_capture, {13, 9});
    ASSERT_EQ(rescale_capture.batch->events.size(), 1u);
    EXPECT_EQ(static_cast<int>(rescale_capture.batch->events.front().x), 4);
    EXPECT_EQ(static_cast<int>(rescale_capture.batch->events.front().y), 2);

    FileRenderHarness roni_harness;
    ASSERT_TRUE(roni_harness.ready());
    configure_rotation(roni_harness.chain(), 90);
    roni_harness.generator().set_display_roi(true, 1, 1, 2, 1, true);
    const RenderCapture roni_capture = roni_harness.render(raw_events);
    ASSERT_NE(roni_capture.batch, nullptr);
    expect_batch_in_bounds(roni_capture, {5, 7});
    ASSERT_EQ(roni_capture.batch->events.size(), 1u);
    EXPECT_EQ(static_cast<int>(roni_capture.batch->events.front().x), 0);
    EXPECT_EQ(static_cast<int>(roni_capture.batch->events.front().y), 6);
}

TEST(FileConditionedConsumers, EmptyWindowPublishesItsGeometrySnapshot) {
    FileRenderHarness harness;
    ASSERT_TRUE(harness.ready());
    configure_rotation(harness.chain(), 90);
    const RenderCapture capture = harness.render({});

    ASSERT_NE(capture.batch, nullptr);
    expect_batch_in_bounds(capture, {5, 7});
    EXPECT_TRUE(capture.batch->events.empty());
    ASSERT_FALSE(capture.frame.isNull());
    EXPECT_EQ(capture.frame.width(), 5);
    EXPECT_EQ(capture.frame.height(), 7);
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    static_cast<void>(app);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
