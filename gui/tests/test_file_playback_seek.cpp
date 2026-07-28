// gui/tests/test_file_playback_seek.cpp -- paused seek presentation contract.

#include <gtest/gtest.h>

#include <QApplication>
#include <QImage>

#include <memory>
#include <string>
#include <vector>

#include <metavision/sdk/base/events/event_cd.h>

#include "app/frame_pipeline.h"
#include "display/event_display_widget.h"

namespace {

struct SeekCapture {
    std::vector<std::string> order;
    QImage frame;
    std::shared_ptr<std::vector<Metavision::EventCD>> events;
    Metavision::timestamp position{-1};
};

} // namespace

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

TEST(PausedSeekImmediateRender, FramesAndDisplayUpdateSynchronously) {
    gui::FramePipeline pipeline;
    gui::EventDisplayWidget display;

    ASSERT_TRUE(pipeline.start_file(4, 4, 30, 100));
    ASSERT_FALSE(pipeline.file_is_playing());

    const std::vector<Metavision::EventCD> events = {
        Metavision::EventCD(0, 0, 1, 10),
        Metavision::EventCD(3, 3, 1, 210),
    };
    pipeline.add_events(events.data(), events.data() + events.size());
    pipeline.set_file_duration_us(400);

    SeekCapture capture;
    QObject::connect(&pipeline, &gui::FramePipeline::file_seeked,
                     [&](Metavision::timestamp) { capture.order.push_back("seeked"); });
    QObject::connect(&pipeline, &gui::FramePipeline::events_window_ready,
                     [&](std::shared_ptr<std::vector<Metavision::EventCD>> window,
                         Metavision::timestamp) {
                         capture.order.push_back("events");
                         capture.events = std::move(window);
                     });
    QObject::connect(&pipeline, &gui::FramePipeline::frame_ready,
                     [&](QImage frame, Metavision::timestamp) {
                         capture.order.push_back("frame");
                         capture.frame = frame;
                         display.set_frame(frame);
                     });
    QObject::connect(&pipeline, &gui::FramePipeline::file_position_changed,
                     [&](Metavision::timestamp position, Metavision::timestamp) {
                         capture.order.push_back("position");
                         capture.position = position;
                     });

    pipeline.seek_file(0);

    ASSERT_EQ(capture.order, (std::vector<std::string>{"seeked", "events", "frame", "position"}));
    ASSERT_NE(capture.events, nullptr);
    ASSERT_EQ(capture.events->size(), 1u);
    EXPECT_EQ(capture.events->front().x, 0);
    EXPECT_EQ(capture.events->front().y, 0);
    EXPECT_EQ(capture.position, 0);
    ASSERT_FALSE(capture.frame.isNull());
    const QImage first_frame = capture.frame.copy();
    EXPECT_EQ(display.current_frame(), first_frame);
    EXPECT_FALSE(pipeline.file_is_playing());

    capture = SeekCapture{};
    pipeline.seek_file(200);

    ASSERT_EQ(capture.order, (std::vector<std::string>{"seeked", "events", "frame", "position"}));
    ASSERT_NE(capture.events, nullptr);
    ASSERT_EQ(capture.events->size(), 1u);
    EXPECT_EQ(capture.events->front().x, 3);
    EXPECT_EQ(capture.events->front().y, 3);
    EXPECT_EQ(capture.position, 200);
    ASSERT_FALSE(capture.frame.isNull());
    EXPECT_NE(capture.frame, first_frame);
    EXPECT_EQ(display.current_frame(), capture.frame);
    EXPECT_FALSE(pipeline.file_is_playing());
}
