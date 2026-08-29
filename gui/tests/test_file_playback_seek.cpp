// gui/tests/test_file_playback_seek.cpp -- paused seek presentation contract.

#include <gtest/gtest.h>

#include <QApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QImage>

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#if defined(__APPLE__) || defined(__linux__)
#include <unistd.h>
#endif

#include <opencv2/core.hpp>

#include <metavision/sdk/base/events/event_cd.h>
#include <metavision/sdk/stream/camera.h>
#include <metavision/sdk/stream/file_config_hints.h>

#include "algo_bridge/algo_bridge.h"
#include "algo_bridge/filter_chain.h"
#include "app/frame_pipeline.h"
#include "display/event_display_widget.h"

namespace {

struct SeekCapture {
    std::vector<std::string> order;
    QImage frame;
    std::shared_ptr<const gui::ConditionedBatch> batch;
    Metavision::timestamp position{-1};
};

constexpr int kRawRoiWidth = 128;
constexpr int kRawRoiHeight = 128;
constexpr Metavision::timestamp kRawWindowUs = 33333;
constexpr int kRawLoadTimeoutMs = 5000;
constexpr int kPlaybackTimeoutMs = 30000;
constexpr double kReplayTolerance = 1.0;

std::optional<std::filesystem::path> e2vid_test_model_path() {
    const char *const value = std::getenv("EBPLUS_E2VID_TEST_MODEL");
    if (value == nullptr || value[0] == '\0') return std::nullopt;
    return std::filesystem::path(value);
}

std::filesystem::path tracked_raw_fixture_path() {
    const std::filesystem::path source_path{__FILE__};
    return source_path.parent_path().parent_path().parent_path() /
           "algo/tests/sparklers.raw";
}

struct NeuralPlaybackFrame {
    Metavision::timestamp timestamp_us{0};
    int loop_epoch{0};
    std::size_t event_count{0};
    cv::Mat frame;
    std::string status;
    double min{0.0};
    double max{0.0};
};

bool valid_neural_playback_frame(const NeuralPlaybackFrame &capture) {
    return capture.event_count > 0 && !capture.frame.empty() &&
           capture.frame.type() == CV_8UC1 &&
           capture.frame.size() == cv::Size(kRawRoiWidth, kRawRoiHeight) &&
           cv::checkRange(capture.frame, true) && std::isfinite(capture.min) &&
           std::isfinite(capture.max) &&
           capture.status.find("model=loaded") != std::string::npos;
}

void print_neural_playback_frame(const char *label,
                                 const NeuralPlaybackFrame &capture) {
    std::cout << "M7Slice3E3 " << label << " ts=" << capture.timestamp_us
              << " loop_epoch=" << capture.loop_epoch
              << " events=" << capture.event_count << " frame="
              << capture.frame.cols << 'x' << capture.frame.rows
              << " type=" << capture.frame.type() << " range=["
              << capture.min << ',' << capture.max << "] status="
              << capture.status << '\n';
}

#if defined(__APPLE__) || defined(__linux__)
constexpr bool kSupportsRepoLocalStderrCapture = true;
// E2VIDInference logs a runtime ORT failure to stderr before returning its
// CV_8UC1 heuristic fallback. The GUI-facing result is also CV_8UC1, so a
// model-loaded status alone cannot distinguish a successful neural frame from
// that fallback. Capture stderr in the repository-local .tmp/ directory;
// GTest's own stream capture hard-codes /tmp on POSIX.
class RepoLocalStderrCapture {
public:
    ~RepoLocalStderrCapture() { cleanup(); }

    bool start(std::string *error) {
        std::error_code fs_error;
        const auto tmp_dir = std::filesystem::path(__FILE__).parent_path()
                                 .parent_path().parent_path() /
                             ".tmp";
        std::filesystem::create_directories(tmp_dir, fs_error);
        if (fs_error) return fail(error, "Failed to create repo-local stderr directory");

        std::string template_path =
            (tmp_dir / "m7-slice3e3-stderr-XXXXXX").string();
        template_path.push_back('\0');
        capture_fd_ = ::mkstemp(template_path.data());
        if (capture_fd_ < 0) return fail(error, "Failed to create repo-local stderr file");
        capture_path_ = template_path.data();

        saved_stderr_fd_ = ::dup(STDERR_FILENO);
        if (saved_stderr_fd_ < 0) return fail_with_cleanup(error, "Failed to duplicate stderr");
        if (std::fflush(stderr) == EOF ||
            ::dup2(capture_fd_, STDERR_FILENO) < 0) {
            return fail_with_cleanup(error, "Failed to redirect stderr");
        }
        active_ = true;
        return true;
    }

    bool stop(std::string *captured, std::string *error) {
        if (!active_) return fail(error, "stderr capture was not active");
        const bool flushed = std::fflush(stderr) != EOF;
        const bool restored = ::dup2(saved_stderr_fd_, STDERR_FILENO) >= 0;
        bool success = flushed && restored;
        if (saved_stderr_fd_ >= 0) {
            ::close(saved_stderr_fd_);
            saved_stderr_fd_ = -1;
        }
        if (success && ::lseek(capture_fd_, 0, SEEK_SET) >= 0) {
            char buffer[4096];
            for (;;) {
                const ssize_t count = ::read(capture_fd_, buffer, sizeof(buffer));
                if (count < 0) {
                    success = false;
                    break;
                }
                if (count == 0) break;
                captured->append(buffer, static_cast<std::size_t>(count));
            }
        } else {
            success = false;
        }
        close_and_remove();
        active_ = false;
        return success || fail(error, "Failed to restore or read captured stderr");
    }

private:
    static bool fail(std::string *error, const char *message) {
        if (error != nullptr) *error = message;
        return false;
    }

    bool fail_with_cleanup(std::string *error, const char *message) {
        cleanup();
        return fail(error, message);
    }

    void close_and_remove() {
        if (capture_fd_ >= 0) {
            ::close(capture_fd_);
            capture_fd_ = -1;
        }
        if (!capture_path_.empty()) {
            std::error_code fs_error;
            std::filesystem::remove(capture_path_, fs_error);
            capture_path_.clear();
        }
    }

    void cleanup() {
        if (active_) {
            std::fflush(stderr);
            if (saved_stderr_fd_ >= 0) ::dup2(saved_stderr_fd_, STDERR_FILENO);
        }
        if (saved_stderr_fd_ >= 0) {
            ::close(saved_stderr_fd_);
            saved_stderr_fd_ = -1;
        }
        close_and_remove();
        active_ = false;
    }

    int capture_fd_{-1};
    int saved_stderr_fd_{-1};
    bool active_{false};
    std::filesystem::path capture_path_;
};
#else
constexpr bool kSupportsRepoLocalStderrCapture = false;

class RepoLocalStderrCapture {
public:
    bool start(std::string *) { return false; }
    bool stop(std::string *, std::string *) { return false; }
};
#endif

void expect_no_e2vid_runtime_fallback(const std::string &captured_stderr) {
    EXPECT_EQ(captured_stderr.find("ONNX inference failed"), std::string::npos)
        << captured_stderr;
    EXPECT_EQ(captured_stderr.find("falling back to heuristic"), std::string::npos)
        << captured_stderr;
}

class RawNeuralPlaybackHarness {
public:
    RawNeuralPlaybackHarness() {
        // This non-GUI harness mirrors MainWindow's reset body after the
        // actual FileFrameGenerator signals. It exercises the signal ordering
        // and AlgoBridge reset contract, not MainWindow runtime itself.
        QObject::connect(&pipeline_, &gui::FramePipeline::file_seeked, &pipeline_,
                         [this](Metavision::timestamp) {
                             ++seek_reset_count_;
                             reset_live_instances();
                         });
        QObject::connect(&pipeline_, &gui::FramePipeline::file_looped, &pipeline_,
                         [this]() {
                             ++loop_count_;
                             ++loop_reset_count_;
                             reset_live_instances();
                         });
        QObject::connect(&pipeline_, &gui::FramePipeline::events_window_ready,
                         &pipeline_,
                         [this](std::shared_ptr<const gui::ConditionedBatch> batch,
                                Metavision::timestamp timestamp_us) {
                             have_pending_window_ = true;
                             pending_timestamp_us_ = timestamp_us;
                             pending_event_count_ = batch ? batch->events.size() : 0u;
                             if (!batch || batch->events.empty()) return;
                             for (auto &instance : bridge_.list_live()) {
                                 if (instance->is_enabled()) {
                                     instance->push_events(batch->events.data(),
                                                           batch->events.data() +
                                                               batch->events.size());
                                 }
                             }
                         });
        QObject::connect(&pipeline_, &gui::FramePipeline::frame_ready, &pipeline_,
                         [this](QImage, Metavision::timestamp timestamp_us) {
                             if (!have_pending_window_ ||
                                 pending_timestamp_us_ != timestamp_us ||
                                 pending_event_count_ == 0) {
                                 return;
                             }
                             const std::size_t event_count = pending_event_count_;
                             have_pending_window_ = false;
                             pending_event_count_ = 0;
                             for (auto &instance : bridge_.list_live()) {
                                 if (instance.get() != e2vid_.get()) continue;
                                 const gui::AlgoResult result = instance->pull_result();
                                 NeuralPlaybackFrame capture;
                                 capture.timestamp_us = timestamp_us;
                                 capture.loop_epoch = loop_count_;
                                 capture.event_count = event_count;
                                 capture.frame = result.frame;
                                 capture.status = result.status;
                                 if (!capture.frame.empty()) {
                                     cv::minMaxLoc(capture.frame, &capture.min,
                                                   &capture.max);
                                 }
                                 frames_.push_back(std::move(capture));
                             }
                             if (pause_at_timestamp_us_.has_value() &&
                                 timestamp_us == *pause_at_timestamp_us_ &&
                                 !pause_triggered_) {
                                 pause_triggered_ = true;
                                 pipeline_.pause_file();
                             }
                         });
    }

    bool initialize(const std::filesystem::path &model_path, std::string *error) {
        const auto raw_path = tracked_raw_fixture_path();
        std::error_code fs_error;
        if (!std::filesystem::is_regular_file(raw_path, fs_error)) {
            return fail(error, "Tracked RAW fixture is not a regular file: " +
                               raw_path.string() + " error=" + fs_error.message());
        }

        try {
            Metavision::FileConfigHints hints;
            hints.real_time_playback(false);
            Metavision::Camera camera =
                Metavision::Camera::from_file(raw_path.string(), hints);
            const int source_width = camera.geometry().get_width();
            const int source_height = camera.geometry().get_height();
            if (source_width < kRawRoiWidth || source_height < kRawRoiHeight) {
                return fail(error, "Tracked RAW geometry is too small for the test ROI");
            }
            const int roi_x = (source_width - kRawRoiWidth) / 2;
            const int roi_y = (source_height - kRawRoiHeight) / 2;

            if (!filter_chain_.set_geometry(source_width, source_height)) {
                return fail(error, "Failed to initialize conditioned geometry");
            }
            pipeline_.set_file_filter_chain(&filter_chain_);
            if (!pipeline_.start_file(source_width, source_height, 30, kRawWindowUs)) {
                return fail(error, "FramePipeline::start_file failed");
            }
            pipeline_.set_file_roi(true, roi_x, roi_y, kRawRoiWidth, kRawRoiHeight);

            bridge_.set_sensor_dimensions(source_width, source_height);
            bridge_.set_unified_roi_state(true, roi_x, roi_y,
                                          roi_x + kRawRoiWidth,
                                          roi_y + kRawRoiHeight);
            e2vid_ = bridge_.find_or_create("event_to_video");
            if (!e2vid_) {
                return fail(error, "Failed to create event_to_video instance");
            }
            e2vid_->set_param("mode", "2");
            e2vid_->set_param("model_path", model_path.string());
            if (e2vid_->get_param("model_loaded") != "true") {
                return fail(error, "EventToVideo did not load the supplied ONNX model");
            }
            if (e2vid_->get_param("num_bins") != "5") {
                return fail(error, "EventToVideo did not sync num_bins from the model");
            }
            if (!bridge_.set_algo_enabled("event_to_video", true) ||
                !e2vid_->is_enabled()) {
                return fail(error, "Failed to enable event_to_video instance");
            }

            std::atomic_bool source_stopped{false};
            camera.cd().add_callback(
                [this](const Metavision::EventCD *begin,
                       const Metavision::EventCD *end) {
                    pipeline_.add_events(begin, end);
                });
            camera.add_status_change_callback(
                [&source_stopped](const Metavision::CameraStatus &status) {
                    if (status == Metavision::CameraStatus::STOPPED) {
                        source_stopped.store(true, std::memory_order_release);
                    }
                });
            camera.start();
            const bool drained = wait_until(
                [&]() {
                    return source_stopped.load(std::memory_order_acquire) ||
                           !camera.is_running();
                },
                kRawLoadTimeoutMs);
            if (camera.is_running()) camera.stop();
            if (!drained) {
                return fail(error, "Timed out while buffering tracked RAW fixture");
            }

            const Metavision::timestamp duration_us = pipeline_.file_duration_us();
            if (duration_us <= 2 * kRawWindowUs) {
                return fail(error, "Tracked RAW fixture duration is too short for lifecycle tests");
            }
            pipeline_.set_file_duration_us(duration_us);
            pipeline_.set_file_loading_complete(true);
            return true;
        } catch (const std::exception &exception) {
            return fail(error, "Failed to prepare tracked RAW playback: " +
                               std::string(exception.what()));
        }
    }

    void seek(Metavision::timestamp timestamp_us) { pipeline_.seek_file(timestamp_us); }
    void play() { pipeline_.play_file(); }
    void pause() { pipeline_.pause_file(); }
    void set_loop(bool enabled) { pipeline_.set_file_loop(enabled); }

    void pause_on_frame(Metavision::timestamp timestamp_us) {
        pause_at_timestamp_us_ = timestamp_us;
        pause_triggered_ = false;
    }

    void clear_pause_on_frame() {
        pause_at_timestamp_us_.reset();
        pause_triggered_ = false;
    }

    bool wait_until(const std::function<bool()> &predicate,
                    int timeout_ms = kPlaybackTimeoutMs) const {
        QElapsedTimer timer;
        timer.start();
        while (!predicate()) {
            QApplication::processEvents(QEventLoop::AllEvents, 10);
            if (timer.elapsed() >= timeout_ms) return predicate();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return true;
    }

    const std::vector<NeuralPlaybackFrame> &frames() const { return frames_; }
    std::size_t frame_count() const { return frames_.size(); }
    int loop_count() const { return loop_count_; }
    int loop_reset_count() const { return loop_reset_count_; }
    int seek_reset_count() const { return seek_reset_count_; }
    bool pause_triggered() const { return pause_triggered_; }
    Metavision::timestamp position_us() const { return pipeline_.file_position_us(); }
    const std::shared_ptr<gui::AlgoInstance> &e2vid() const { return e2vid_; }

private:
    static bool fail(std::string *error, const std::string &message) {
        if (error != nullptr) *error = message;
        return false;
    }

    void reset_live_instances() {
        for (auto &instance : bridge_.list_live()) {
            instance->reset();
        }
    }

    gui::FilterChain filter_chain_;
    gui::FramePipeline pipeline_;
    gui::AlgoBridge bridge_;
    std::shared_ptr<gui::AlgoInstance> e2vid_;
    std::vector<NeuralPlaybackFrame> frames_;
    bool have_pending_window_{false};
    Metavision::timestamp pending_timestamp_us_{0};
    std::size_t pending_event_count_{0};
    int loop_count_{0};
    int loop_reset_count_{0};
    int seek_reset_count_{0};
    std::optional<Metavision::timestamp> pause_at_timestamp_us_;
    bool pause_triggered_{false};
};

const NeuralPlaybackFrame *find_frame(const RawNeuralPlaybackHarness &harness,
                                      std::size_t begin,
                                      int loop_epoch,
                                      Metavision::timestamp timestamp_us) {
    const auto &frames = harness.frames();
    for (std::size_t index = begin; index < frames.size(); ++index) {
        if (frames[index].loop_epoch == loop_epoch &&
            frames[index].timestamp_us == timestamp_us) {
            return &frames[index];
        }
    }
    return nullptr;
}

} // namespace

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

TEST(PausedSeekImmediateRender, FramesAndDisplayUpdateSynchronously) {
    gui::FilterChain filter_chain;
    gui::FramePipeline pipeline;
    gui::EventDisplayWidget display;

    ASSERT_TRUE(filter_chain.set_geometry(4, 4));
    pipeline.set_file_filter_chain(&filter_chain);
    ASSERT_TRUE(pipeline.start_file(4, 4, 30, 100));
    // FramePipeline no longer exposes FileFrameGenerator's internal playing
    // state. The synchronous signal order below is the supported paused-seek
    // presentation contract.

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
                     [&](std::shared_ptr<const gui::ConditionedBatch> batch,
                         Metavision::timestamp) {
                         capture.order.push_back("events");
                         capture.batch = batch;
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
    ASSERT_NE(capture.batch, nullptr);
    ASSERT_EQ(capture.batch->events.size(), 1u);
    EXPECT_EQ(capture.batch->events.front().x, 0);
    EXPECT_EQ(capture.batch->events.front().y, 0);
    EXPECT_EQ(capture.position, 0);
    ASSERT_FALSE(capture.frame.isNull());
    const QImage first_frame = capture.frame.copy();
    EXPECT_EQ(display.current_frame(), first_frame);

    capture = SeekCapture{};
    pipeline.seek_file(200);

    ASSERT_EQ(capture.order, (std::vector<std::string>{"seeked", "events", "frame", "position"}));
    ASSERT_NE(capture.batch, nullptr);
    ASSERT_EQ(capture.batch->events.size(), 1u);
    EXPECT_EQ(capture.batch->events.front().x, 3);
    EXPECT_EQ(capture.batch->events.front().y, 3);
    EXPECT_EQ(capture.position, 200);
    ASSERT_FALSE(capture.frame.isNull());
    EXPECT_NE(capture.frame, first_frame);
    EXPECT_EQ(display.current_frame(), capture.frame);
}

TEST(RealModelTrackedRawPlayback, ProducesSequentialNeuralFrames) {
    const auto model_path = e2vid_test_model_path();
    if (!model_path.has_value()) {
        GTEST_SKIP() << "EBPLUS_E2VID_TEST_MODEL is not set";
    }
    if (!kSupportsRepoLocalStderrCapture) {
        GTEST_SKIP() << "This platform cannot capture E2VID stderr repo-locally";
    }
    std::error_code model_error;
    ASSERT_TRUE(std::filesystem::is_regular_file(*model_path, model_error))
        << "EBPLUS_E2VID_TEST_MODEL must name a regular file: "
        << model_path->string() << " error=" << model_error.message();

    RawNeuralPlaybackHarness harness;
    std::string setup_error;
    ASSERT_TRUE(harness.initialize(*model_path, &setup_error)) << setup_error;
    ASSERT_NE(harness.e2vid(), nullptr);
    ASSERT_EQ(harness.e2vid()->get_param("model_loaded"), "true");
    ASSERT_EQ(harness.e2vid()->get_param("num_bins"), "5");

    RepoLocalStderrCapture stderr_capture;
    std::string capture_error;
    ASSERT_TRUE(stderr_capture.start(&capture_error)) << capture_error;
    harness.pause_on_frame(2 * kRawWindowUs);
    harness.play();
    ASSERT_TRUE(harness.wait_until([&]() {
        return harness.pause_triggered() && harness.frame_count() >= 3;
    })) << "Timed out waiting for three bounded RAW playback windows";
    harness.pause();
    std::string captured_stderr;
    ASSERT_TRUE(stderr_capture.stop(&captured_stderr, &capture_error)) << capture_error;
    expect_no_e2vid_runtime_fallback(captured_stderr);

    const auto &frames = harness.frames();
    ASSERT_GE(frames.size(), 3u);
    EXPECT_EQ(frames[0].timestamp_us, 0);
    EXPECT_EQ(frames[1].timestamp_us, kRawWindowUs);
    EXPECT_EQ(frames[2].timestamp_us, 2 * kRawWindowUs);
    for (const auto &frame : frames) {
        ASSERT_TRUE(valid_neural_playback_frame(frame));
        print_neural_playback_frame("raw-playback", frame);
    }
}

TEST(RealModelSeekResetReplay, ActualSeekSignalRestoresZeroStateOutput) {
    const auto model_path = e2vid_test_model_path();
    if (!model_path.has_value()) {
        GTEST_SKIP() << "EBPLUS_E2VID_TEST_MODEL is not set";
    }
    if (!kSupportsRepoLocalStderrCapture) {
        GTEST_SKIP() << "This platform cannot capture E2VID stderr repo-locally";
    }
    std::error_code model_error;
    ASSERT_TRUE(std::filesystem::is_regular_file(*model_path, model_error))
        << "EBPLUS_E2VID_TEST_MODEL must name a regular file: "
        << model_path->string() << " error=" << model_error.message();

    constexpr Metavision::timestamp kSeekTargetUs = kRawWindowUs;
    constexpr Metavision::timestamp kPostSeekSecondWindowUs = 2 * kRawWindowUs;

    RawNeuralPlaybackHarness reference;
    std::string reference_setup_error;
    ASSERT_TRUE(reference.initialize(*model_path, &reference_setup_error))
        << reference_setup_error;
    RepoLocalStderrCapture stderr_capture;
    std::string capture_error;
    ASSERT_TRUE(stderr_capture.start(&capture_error)) << capture_error;
    const std::size_t reference_begin = reference.frame_count();
    reference.seek(kSeekTargetUs);
    ASSERT_EQ(reference.seek_reset_count(), 1);
    reference.pause_on_frame(kPostSeekSecondWindowUs);
    reference.play();
    ASSERT_TRUE(reference.wait_until([&]() {
        return reference.pause_triggered() &&
               find_frame(reference, reference_begin, 0, kSeekTargetUs) != nullptr &&
               find_frame(reference, reference_begin, 0, kPostSeekSecondWindowUs) != nullptr;
    })) << "Timed out collecting the fresh post-seek reference windows";
    reference.pause();

    RawNeuralPlaybackHarness replay;
    std::string replay_setup_error;
    ASSERT_TRUE(replay.initialize(*model_path, &replay_setup_error)) << replay_setup_error;
    replay.pause_on_frame(kRawWindowUs);
    replay.play();
    ASSERT_TRUE(replay.wait_until([&]() {
        return replay.pause_triggered() && replay.frame_count() >= 2;
    })) << "Timed out creating pre-seek recurrent state";
    replay.pause();

    const std::size_t replay_begin = replay.frame_count();
    replay.seek(kSeekTargetUs);
    ASSERT_EQ(replay.seek_reset_count(), 1);
    replay.pause_on_frame(kPostSeekSecondWindowUs);
    replay.play();
    ASSERT_TRUE(replay.wait_until([&]() {
        return replay.pause_triggered() &&
               find_frame(replay, replay_begin, 0, kSeekTargetUs) != nullptr &&
               find_frame(replay, replay_begin, 0, kPostSeekSecondWindowUs) != nullptr;
    })) << "Timed out collecting replayed post-seek windows";
    replay.pause();
    std::string captured_stderr;
    ASSERT_TRUE(stderr_capture.stop(&captured_stderr, &capture_error)) << capture_error;
    expect_no_e2vid_runtime_fallback(captured_stderr);

    const auto *reference_first =
        find_frame(reference, reference_begin, 0, kSeekTargetUs);
    const auto *reference_second =
        find_frame(reference, reference_begin, 0, kPostSeekSecondWindowUs);
    const auto *replay_first = find_frame(replay, replay_begin, 0, kSeekTargetUs);
    const auto *replay_second =
        find_frame(replay, replay_begin, 0, kPostSeekSecondWindowUs);
    ASSERT_NE(reference_first, nullptr);
    ASSERT_NE(reference_second, nullptr);
    ASSERT_NE(replay_first, nullptr);
    ASSERT_NE(replay_second, nullptr);
    ASSERT_TRUE(valid_neural_playback_frame(*reference_first));
    ASSERT_TRUE(valid_neural_playback_frame(*reference_second));
    ASSERT_TRUE(valid_neural_playback_frame(*replay_first));
    ASSERT_TRUE(valid_neural_playback_frame(*replay_second));

    const double first_difference =
        cv::norm(reference_first->frame, replay_first->frame, cv::NORM_INF);
    const double second_difference =
        cv::norm(reference_second->frame, replay_second->frame, cv::NORM_INF);
    EXPECT_LE(first_difference, kReplayTolerance);
    EXPECT_LE(second_difference, kReplayTolerance);
    print_neural_playback_frame("seek-reference-first", *reference_first);
    print_neural_playback_frame("seek-replay-first", *replay_first);
    print_neural_playback_frame("seek-reference-second", *reference_second);
    print_neural_playback_frame("seek-replay-second", *replay_second);
    std::cout << "M7Slice3E3 seek replay NORM_INF first=" << first_difference
              << " second=" << second_difference
              << " tolerance=" << kReplayTolerance << '\n';
}

TEST(RealModelLoopResetReplay, ActualLoopSignalRestoresFirstWindows) {
    const auto model_path = e2vid_test_model_path();
    if (!model_path.has_value()) {
        GTEST_SKIP() << "EBPLUS_E2VID_TEST_MODEL is not set";
    }
    if (!kSupportsRepoLocalStderrCapture) {
        GTEST_SKIP() << "This platform cannot capture E2VID stderr repo-locally";
    }
    std::error_code model_error;
    ASSERT_TRUE(std::filesystem::is_regular_file(*model_path, model_error))
        << "EBPLUS_E2VID_TEST_MODEL must name a regular file: "
        << model_path->string() << " error=" << model_error.message();

    RawNeuralPlaybackHarness harness;
    std::string setup_error;
    ASSERT_TRUE(harness.initialize(*model_path, &setup_error)) << setup_error;
    RepoLocalStderrCapture stderr_capture;
    std::string capture_error;
    ASSERT_TRUE(stderr_capture.start(&capture_error)) << capture_error;
    harness.set_loop(true);
    harness.play();
    ASSERT_TRUE(harness.wait_until([&]() {
        return harness.loop_count() >= 1 &&
               find_frame(harness, 0, 0, 0) != nullptr &&
               find_frame(harness, 0, 0, kRawWindowUs) != nullptr &&
               find_frame(harness, 0, 1, 0) != nullptr &&
               find_frame(harness, 0, 1, kRawWindowUs) != nullptr;
    })) << "Timed out waiting for a real FileFrameGenerator loop replay";
    harness.pause();
    std::string captured_stderr;
    ASSERT_TRUE(stderr_capture.stop(&captured_stderr, &capture_error)) << capture_error;
    expect_no_e2vid_runtime_fallback(captured_stderr);

    ASSERT_GE(harness.loop_count(), 1);
    ASSERT_EQ(harness.loop_reset_count(), harness.loop_count());
    const auto *initial_first = find_frame(harness, 0, 0, 0);
    const auto *initial_second = find_frame(harness, 0, 0, kRawWindowUs);
    const auto *loop_first = find_frame(harness, 0, 1, 0);
    const auto *loop_second = find_frame(harness, 0, 1, kRawWindowUs);
    ASSERT_NE(initial_first, nullptr);
    ASSERT_NE(initial_second, nullptr);
    ASSERT_NE(loop_first, nullptr);
    ASSERT_NE(loop_second, nullptr);
    ASSERT_TRUE(valid_neural_playback_frame(*initial_first));
    ASSERT_TRUE(valid_neural_playback_frame(*initial_second));
    ASSERT_TRUE(valid_neural_playback_frame(*loop_first));
    ASSERT_TRUE(valid_neural_playback_frame(*loop_second));

    const double first_difference =
        cv::norm(initial_first->frame, loop_first->frame, cv::NORM_INF);
    const double second_difference =
        cv::norm(initial_second->frame, loop_second->frame, cv::NORM_INF);
    EXPECT_LE(first_difference, kReplayTolerance);
    EXPECT_LE(second_difference, kReplayTolerance);
    print_neural_playback_frame("loop-initial-first", *initial_first);
    print_neural_playback_frame("loop-replay-first", *loop_first);
    print_neural_playback_frame("loop-initial-second", *initial_second);
    print_neural_playback_frame("loop-replay-second", *loop_second);
    std::cout << "M7Slice3E3 loop replay NORM_INF first=" << first_difference
              << " second=" << second_difference
              << " tolerance=" << kReplayTolerance << '\n';
}

TEST(RealModelModeSwitchReset, ReturningToE2VIDClearsRecurrentState) {
    const auto model_path = e2vid_test_model_path();
    if (!model_path.has_value()) {
        GTEST_SKIP() << "EBPLUS_E2VID_TEST_MODEL is not set";
    }
    if (!kSupportsRepoLocalStderrCapture) {
        GTEST_SKIP() << "This platform cannot capture E2VID stderr repo-locally";
    }
    std::error_code model_error;
    ASSERT_TRUE(std::filesystem::is_regular_file(*model_path, model_error))
        << "EBPLUS_E2VID_TEST_MODEL must name a regular file: "
        << model_path->string() << " error=" << model_error.message();

    constexpr Metavision::timestamp kTargetUs = 2 * kRawWindowUs;

    RawNeuralPlaybackHarness reference;
    std::string reference_setup_error;
    ASSERT_TRUE(reference.initialize(*model_path, &reference_setup_error))
        << reference_setup_error;
    RepoLocalStderrCapture stderr_capture;
    std::string capture_error;
    ASSERT_TRUE(stderr_capture.start(&capture_error)) << capture_error;
    const std::size_t reference_begin = reference.frame_count();
    reference.seek(kTargetUs);
    ASSERT_EQ(reference.seek_reset_count(), 1);
    const auto *reference_frame =
        find_frame(reference, reference_begin, 0, kTargetUs);
    ASSERT_NE(reference_frame, nullptr);
    ASSERT_TRUE(valid_neural_playback_frame(*reference_frame));

    RawNeuralPlaybackHarness mode_switch;
    std::string mode_switch_setup_error;
    ASSERT_TRUE(mode_switch.initialize(*model_path, &mode_switch_setup_error))
        << mode_switch_setup_error;
    mode_switch.pause_on_frame(kRawWindowUs);
    mode_switch.play();
    ASSERT_TRUE(mode_switch.wait_until([&]() {
        return mode_switch.pause_triggered() && mode_switch.frame_count() >= 2;
    })) << "Timed out creating recurrent state before mode switch";
    mode_switch.pause();
    ASSERT_EQ(mode_switch.position_us(), kTargetUs);

    mode_switch.e2vid()->set_param("mode", "0");
    mode_switch.e2vid()->set_param("mode", "2");
    ASSERT_EQ(mode_switch.e2vid()->get_param("model_loaded"), "true");
    ASSERT_EQ(mode_switch.e2vid()->get_param("num_bins"), "5");

    const std::size_t mode_switch_begin = mode_switch.frame_count();
    mode_switch.pause_on_frame(kTargetUs);
    mode_switch.play();
    ASSERT_TRUE(mode_switch.wait_until([&]() {
        return mode_switch.pause_triggered() &&
               find_frame(mode_switch, mode_switch_begin, 0, kTargetUs) != nullptr;
    })) << "Timed out collecting the mode-switch-return frame";
    mode_switch.pause();
    std::string captured_stderr;
    ASSERT_TRUE(stderr_capture.stop(&captured_stderr, &capture_error)) << capture_error;
    expect_no_e2vid_runtime_fallback(captured_stderr);

    const auto *mode_switch_frame =
        find_frame(mode_switch, mode_switch_begin, 0, kTargetUs);
    ASSERT_NE(mode_switch_frame, nullptr);
    ASSERT_TRUE(valid_neural_playback_frame(*mode_switch_frame));
    const double difference =
        cv::norm(reference_frame->frame, mode_switch_frame->frame, cv::NORM_INF);
    EXPECT_LE(difference, kReplayTolerance);
    print_neural_playback_frame("mode-switch-reference", *reference_frame);
    print_neural_playback_frame("mode-switch-return", *mode_switch_frame);
    std::cout << "M7Slice3E3 mode-switch replay NORM_INF=" << difference
              << " tolerance=" << kReplayTolerance << '\n';
}
