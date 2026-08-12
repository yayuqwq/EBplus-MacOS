// gui/tests/test_camera_controller_lifecycle.cpp -- file-open lifecycle regression tests.

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QThread>
#include <QStringList>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <metavision/sdk/base/events/event_cd.h>
#include <metavision/sdk/stream/raw_evt2_event_file_writer.h>

#include "app/camera_controller.h"
#include "app/frame_pipeline.h"
#include "recorder/playback_controller.h"

#ifndef EBPLUS_GUI_TEST_ARTIFACT_DIR
#error "EBPLUS_GUI_TEST_ARTIFACT_DIR must be defined"
#endif

namespace {

using namespace std::chrono_literals;

constexpr int kVeryShortWidth            = 8;
constexpr int kVeryShortHeight           = 8;
constexpr int kFirstStopTimeoutMs        = 2000;
constexpr int kPlaybackTimeoutMs         = 3000;
constexpr std::size_t kFixtureEventCount = 2;

struct LifecycleTrace {
    int connected_count{0};
    int started_count{0};
    int stopped_count{0};
    int eof_count{0};
    int loop_count{0};
    bool connected_during_open_file{false};
    bool connected_on_receiver_thread{false};
    bool first_start_succeeded{false};
    bool first_source_stopped_before_connect_return{false};
    QStringList errors;
    QStringList warnings;
    std::vector<std::size_t> event_window_sizes;
};

std::filesystem::path make_very_short_raw_fixture(const char *case_name) {
    const auto artifact_dir = std::filesystem::path(EBPLUS_GUI_TEST_ARTIFACT_DIR) /
                              "camera_controller_lifecycle";
    std::filesystem::create_directories(artifact_dir);

    const auto unique_id = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto path = artifact_dir /
                      (std::string("very-short-") + case_name + "-" +
                       std::to_string(unique_id) + ".raw");

    // This is a generated EVT2 test fixture, not a sensor recording. Keep it
    // non-zero-duration so the separate zero-duration semantic question does
    // not obscure the direct-connected-start lifecycle regression.
    const std::vector<Metavision::EventCD> events = {
        Metavision::EventCD(1, 1, 1, 0),
        Metavision::EventCD(6, 6, 0, 1000),
    };
    Metavision::RAWEvt2EventFileWriter writer(kVeryShortWidth, kVeryShortHeight, path);
    if (!writer.is_open()) {
        throw std::runtime_error("very-short RAW fixture writer did not open");
    }
    if (!writer.add_events(events.data(), events.data() + events.size())) {
        throw std::runtime_error("very-short RAW fixture writer rejected events");
    }
    writer.close();

    if (!std::filesystem::is_regular_file(path) || std::filesystem::file_size(path) == 0) {
        throw std::runtime_error("very-short RAW fixture was not written");
    }
    return path;
}

class DirectConnectedStartHarness {
public:
    DirectConnectedStartHarness() {
        playback_.set_camera(&controller_);
        auto *pipeline = controller_.frame_pipeline();

        // Keep this direct observer atomic: the SDK status
        // callback may emit stopped() from its decoder thread while the
        // synchronous connected handler is still on the stack.
        QObject::connect(&controller_, &gui::CameraController::stopped, &receiver_,
                         [this]() {
                             stopped_emitted_inline_.fetch_add(1, std::memory_order_relaxed);
                         },
                         Qt::DirectConnection);
        QObject::connect(&controller_, &gui::CameraController::connected, &receiver_,
                         [this](const gui::SensorInfo &info) {
                             ++trace_.connected_count;
                             trace_.connected_during_open_file = open_file_call_active_;
                             trace_.connected_on_receiver_thread =
                                 QThread::currentThread() == receiver_.thread();
                             EXPECT_TRUE(info.is_file);

                             // This is the same direct connected -> start()
                             // topology as MainWindow. Do not pump Qt events
                             // here: waiting only on Camera::is_running and
                             // the direct STOPPED observer preserves the
                             // relevant "before connect_file returns" order.
                             trace_.first_start_succeeded = controller_.start();
                             trace_.first_source_stopped_before_connect_return =
                                 trace_.first_start_succeeded && wait_for_first_stop();
                         });
        QObject::connect(&controller_, &gui::CameraController::started, &receiver_, [this]() {
            ++trace_.started_count;
        });
        QObject::connect(&controller_, &gui::CameraController::stopped, &receiver_, [this]() {
            ++trace_.stopped_count;
        });
        QObject::connect(&controller_, &gui::CameraController::error, &receiver_,
                         [this](const QString &message) { trace_.errors.push_back(message); });
        QObject::connect(&controller_, &gui::CameraController::runtime_warning, &receiver_,
                         [this](const QString &message) { trace_.warnings.push_back(message); });
        QObject::connect(pipeline, &gui::FramePipeline::file_eof_reached, &receiver_, [this]() {
            ++trace_.eof_count;
        });
        QObject::connect(pipeline, &gui::FramePipeline::file_looped, &receiver_, [this]() {
            ++trace_.loop_count;
        });
        QObject::connect(
            pipeline, &gui::FramePipeline::events_window_ready, &receiver_,
            [this](std::shared_ptr<std::vector<Metavision::EventCD>> events,
                   Metavision::timestamp) {
                trace_.event_window_sizes.push_back(events ? events->size() : 0u);
            });
    }

    bool open(const std::filesystem::path &path, bool loop) {
        playback_.set_loop(loop);
        open_file_call_active_ = true;
        const bool opened = playback_.open_file(QString::fromStdString(path.string()));
        open_file_call_active_ = false;
        return opened;
    }

    bool wait_until(const std::function<bool()> &predicate, int timeout_ms = kPlaybackTimeoutMs) {
        QElapsedTimer timer;
        timer.start();
        while (!predicate()) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
            if (timer.elapsed() >= timeout_ms) {
                return predicate();
            }
            std::this_thread::sleep_for(1ms);
        }
        return true;
    }

    Metavision::timestamp duration_us() const {
        return playback_.duration_us();
    }

    void stop() {
        playback_.pause();
        controller_.disconnect();
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }

    LifecycleTrace trace_;

private:
    bool wait_for_first_stop() {
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < kFirstStopTimeoutMs) {
            if (stopped_emitted_inline_.load(std::memory_order_relaxed) > 0 &&
                !controller_.is_running()) {
                return true;
            }
            std::this_thread::sleep_for(1ms);
        }
        return stopped_emitted_inline_.load(std::memory_order_relaxed) > 0 &&
               !controller_.is_running();
    }

    QObject receiver_;
    gui::CameraController controller_;
    gui::PlaybackController playback_;
    std::atomic<int> stopped_emitted_inline_{0};
    bool open_file_call_active_{false};
};

void expect_direct_first_start_and_short_fixture(const DirectConnectedStartHarness &harness) {
    EXPECT_EQ(harness.trace_.connected_count, 1);
    EXPECT_TRUE(harness.trace_.connected_during_open_file);
    EXPECT_TRUE(harness.trace_.connected_on_receiver_thread);
    EXPECT_TRUE(harness.trace_.first_start_succeeded);
    EXPECT_TRUE(harness.trace_.first_source_stopped_before_connect_return);
}

void expect_original_events_not_replayed(const DirectConnectedStartHarness &harness,
                                         std::size_t required_windows) {
    ASSERT_GE(harness.trace_.event_window_sizes.size(), required_windows);
    for (std::size_t i = 0; i < required_windows; ++i) {
        EXPECT_EQ(harness.trace_.event_window_sizes[i], kFixtureEventCount);
    }
}

} // namespace

TEST(CameraControllerLifecycle, EmptyRawFailureIsCaught) {
    const std::filesystem::path artifact_dir =
        std::filesystem::path(EBPLUS_GUI_TEST_ARTIFACT_DIR) / "camera_controller_lifecycle";
    const std::filesystem::path empty_raw = artifact_dir / "empty.raw";
    std::filesystem::create_directories(artifact_dir);
    std::ofstream raw_file(empty_raw, std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(raw_file.is_open());
    raw_file.close();

    gui::CameraController controller;
    int disconnected_count = 0;
    int error_count = 0;
    QString error_message;
    QObject::connect(&controller, &gui::CameraController::disconnected, [&]() {
        ++disconnected_count;
    });
    QObject::connect(&controller, &gui::CameraController::error, [&](const QString& message) {
        ++error_count;
        error_message = message;
    });

    bool opened = true;
    EXPECT_NO_THROW(opened = controller.connect_file(empty_raw.string()));

    EXPECT_FALSE(opened);
    EXPECT_EQ(disconnected_count, 1);
    EXPECT_EQ(error_count, 1);
    EXPECT_FALSE(error_message.isEmpty());
    EXPECT_FALSE(controller.is_connected());
    EXPECT_FALSE(controller.is_file_source());
    EXPECT_EQ(controller.biases_facility(), nullptr);
    EXPECT_EQ(controller.roi_facility(), nullptr);
    EXPECT_EQ(controller.anti_flicker_facility(), nullptr);
    EXPECT_EQ(controller.trail_filter_facility(), nullptr);
    EXPECT_EQ(controller.erc_facility(), nullptr);
    EXPECT_EQ(controller.trigger_in_facility(), nullptr);
    EXPECT_EQ(controller.trigger_out_facility(), nullptr);
}

TEST(CameraControllerLifecycle, VeryShortRawDirectConnectedStartLoopOffStopsOnce) {
    std::filesystem::path fixture;
    ASSERT_NO_THROW(fixture = make_very_short_raw_fixture("loop-off"));

    DirectConnectedStartHarness harness;
    ASSERT_TRUE(harness.open(fixture, false));
    expect_direct_first_start_and_short_fixture(harness);
    ASSERT_GT(harness.duration_us(), 0);

    ASSERT_TRUE(harness.wait_until([&]() {
        return harness.trace_.eof_count > 0 || !harness.trace_.errors.empty();
    }));
    EXPECT_TRUE(harness.trace_.errors.isEmpty())
        << harness.trace_.errors.join(" | ").toStdString();
    EXPECT_EQ(harness.trace_.eof_count, 1);
    EXPECT_EQ(harness.trace_.loop_count, 0);
    expect_original_events_not_replayed(harness, 1);

    harness.stop();
}

TEST(CameraControllerLifecycle, VeryShortRawDirectConnectedStartLoopOnWrapsWithoutTerminalEof) {
    std::filesystem::path fixture;
    ASSERT_NO_THROW(fixture = make_very_short_raw_fixture("loop-on"));

    DirectConnectedStartHarness harness;
    ASSERT_TRUE(harness.open(fixture, true));
    expect_direct_first_start_and_short_fixture(harness);
    ASSERT_GT(harness.duration_us(), 0);

    ASSERT_TRUE(harness.wait_until([&]() {
        return (harness.trace_.loop_count > 0 && harness.trace_.event_window_sizes.size() >= 2) ||
               !harness.trace_.errors.empty();
    }));
    EXPECT_TRUE(harness.trace_.errors.isEmpty())
        << harness.trace_.errors.join(" | ").toStdString();
    EXPECT_GE(harness.trace_.loop_count, 1);
    EXPECT_EQ(harness.trace_.eof_count, 0);
    expect_original_events_not_replayed(harness, 2);

    harness.stop();
}

int main(int argc, char **argv) {
    QCoreApplication application(argc, argv);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
