// gui/tests/test_exporter_controller.cpp -- HDF5 and deterministic AVI export contracts.

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <metavision/sdk/base/events/event_cd.h>
#include <metavision/sdk/core/algorithms/periodic_frame_generation_algorithm.h>
#include <metavision/sdk/core/utils/colors.h>
#include <metavision/sdk/stream/camera.h>
#include <metavision/sdk/stream/file_config_hints.h>

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

#include "exporter/exporter_controller.h"

#ifndef EBPLUS_GUI_TEST_ARTIFACT_DIR
#error "EBPLUS_GUI_TEST_ARTIFACT_DIR must be defined"
#endif

#ifndef EBPLUS_REPO_ROOT
#error "EBPLUS_REPO_ROOT must be defined"
#endif

namespace {

using namespace std::chrono_literals;

constexpr int kOperationTimeoutMs = 30000;
constexpr auto kDecodeTimeout     = 30s;
constexpr double kExpectedFps     = 30.0;
constexpr double kFpsTolerance    = 0.1;

struct OperationOutcome {
    bool started{false};
    bool completed{false};
    bool failed{false};
    bool timed_out{false};
    QString output;
    QString failure;
};

struct FrameOracle {
    int width{0};
    int height{0};
    Metavision::timestamp duration_us{0};
    std::size_t output_frames{0};
};

struct AviReadback {
    bool opened{false};
    std::string backend;
    std::uintmax_t size_bytes{0};
    double fps{0.0};
    int width{0};
    int height{0};
    int reported_frame_count{0};
    std::string fourcc;
    std::size_t decoded_frames{0};
    bool all_dimensions_match{true};
    bool grayscale_channels_match{true};
    std::size_t colorful_pixels{0};
};

enum class FramePresentation {
    Any,
    Color,
    Grayscale,
};

struct CodecProbeResult {
    bool writer_opened{false};
    std::string writer_backend;
    AviReadback readback;
};

const std::filesystem::path &source_fixture() {
    static const std::filesystem::path source =
        std::filesystem::path(EBPLUS_REPO_ROOT) / "algo/tests/sparklers.raw";
    return source;
}

void reject_symlinked_path_components(const std::filesystem::path &path) {
    const std::filesystem::path normalized = std::filesystem::absolute(path).lexically_normal();
    std::filesystem::path current = normalized.root_path();
    for (const auto &component : normalized.relative_path()) {
        current /= component;
        std::error_code error;
        const std::filesystem::file_status status = std::filesystem::symlink_status(current, error);
        if (error) {
            throw std::runtime_error("could not inspect exporter test artifact path");
        }
        if (std::filesystem::is_symlink(status)) {
            throw std::runtime_error("exporter test artifact path must not traverse a symlink");
        }
        if (status.type() == std::filesystem::file_type::not_found) {
            return;
        }
    }
}

std::filesystem::path make_artifact_root(const char *case_name) {
    static std::atomic<std::uint64_t> sequence{0};
    const std::filesystem::path base =
        std::filesystem::path(EBPLUS_GUI_TEST_ARTIFACT_DIR) / "exporter_controller" / "slice4b";
    reject_symlinked_path_components(base);
    std::filesystem::create_directories(base);
    reject_symlinked_path_components(base);
    if (!std::filesystem::is_directory(base) || std::filesystem::is_symlink(base)) {
        throw std::runtime_error("exporter test artifact base must be a directory");
    }

    const auto unique_id = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto serial = sequence.fetch_add(1, std::memory_order_relaxed);
    const std::filesystem::path root =
        base / (std::string(case_name) + "-" + std::to_string(unique_id) + "-" +
                std::to_string(serial));
    if (!std::filesystem::create_directory(root)) {
        throw std::runtime_error("could not create unique exporter test artifact root");
    }
    if (std::filesystem::is_symlink(root)) {
        throw std::runtime_error("exporter test artifact root must not be a symlink");
    }
    return root;
}

OperationOutcome run_export(const gui::ExportParams &params) {
    gui::ExporterController controller;
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);

    OperationOutcome outcome;
    QObject::connect(&controller, &gui::ExporterController::completed, &loop,
                     [&](const QString &output) {
                         outcome.completed = true;
                         outcome.output = output;
                         loop.quit();
                     });
    QObject::connect(&controller, &gui::ExporterController::failed, &loop,
                     [&](const QString &message) {
                         outcome.failed = true;
                         outcome.failure = message;
                         loop.quit();
                     });
    QObject::connect(&timeout, &QTimer::timeout, &loop, [&]() {
        outcome.timed_out = true;
        loop.quit();
    });

    outcome.started = controller.start(params);
    if (!outcome.started) {
        return outcome;
    }
    timeout.start(kOperationTimeoutMs);
    loop.exec();
    timeout.stop();
    return outcome;
}

FrameOracle build_frame_oracle(int accumulation_us, bool color) {
    Metavision::FileConfigHints hints;
    hints.real_time_playback(false);
    hints.time_shift(true);

    Metavision::Camera camera = Metavision::Camera::from_file(source_fixture().string(), hints);
    const auto &geometry = camera.geometry();
    FrameOracle oracle;
    oracle.width = geometry.get_width();
    oracle.height = geometry.get_height();

    const auto period = static_cast<Metavision::timestamp>(std::max(accumulation_us, 1));
    Metavision::PeriodicFrameGenerationAlgorithm generator(
        oracle.width, oracle.height, static_cast<std::uint32_t>(period), 0.0,
        color ? Metavision::ColorPalette::Dark : Metavision::ColorPalette::Gray);
    Metavision::timestamp next_frame_ts = -1;
    bool has_black_frame = false;
    generator.set_output_callback([&](Metavision::timestamp ts, cv::Mat &frame) {
        if (!frame.empty()) {
            has_black_frame = true;
        }
        if (next_frame_ts < 0) {
            next_frame_ts = ts;
        }
        while (next_frame_ts < ts && has_black_frame) {
            ++oracle.output_frames;
            next_frame_ts += period;
        }
        next_frame_ts = ts + period;
        if (!frame.empty()) {
            ++oracle.output_frames;
        }
    });

    const auto callback_id = camera.cd().add_callback(
        [&](const Metavision::EventCD *begin, const Metavision::EventCD *end) {
            if (begin != end) {
                oracle.duration_us = (end - 1)->t;
            }
            generator.process_events(begin, end);
        });
    try {
        camera.start();
        const auto deadline = std::chrono::steady_clock::now() + kDecodeTimeout;
        while (camera.is_running() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(5ms);
        }
        if (camera.is_running()) {
            camera.stop();
            throw std::runtime_error("timed out while decoding AVI source oracle");
        }
    } catch (...) {
        try {
            if (camera.is_running()) {
                camera.stop();
            }
        } catch (...) {
        }
        camera.cd().remove_callback(callback_id);
        throw;
    }
    camera.cd().remove_callback(callback_id);
    generator.force_generate();
    return oracle;
}

std::string fourcc_text(int value) {
    std::string text(4, ' ');
    for (int index = 0; index < 4; ++index) {
        text[index] = static_cast<char>((value >> (8 * index)) & 0xff);
    }
    return text;
}

std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return value;
}

bool is_mjpg_fourcc(const std::string &fourcc) {
    const std::string normalized = lowercase(fourcc);
    return normalized == "mjpg" || normalized == "jpeg";
}

bool is_h264_fourcc(const std::string &fourcc) {
    const std::string normalized = lowercase(fourcc);
    return normalized == "h264" || normalized == "x264" || normalized == "avc1";
}

bool has_observable_fourcc(const std::string &fourcc) {
    return std::any_of(fourcc.begin(), fourcc.end(),
                       [](unsigned char character) { return character != '\0' && character != ' '; });
}

AviReadback read_avi(const std::filesystem::path &path, FramePresentation expected_presentation) {
    AviReadback readback;
    if (std::filesystem::is_symlink(path) || !std::filesystem::is_regular_file(path)) {
        return readback;
    }
    readback.size_bytes = std::filesystem::file_size(path);

    cv::VideoCapture capture(path.string());
    readback.opened = capture.isOpened();
    if (!readback.opened) {
        return readback;
    }
    readback.backend = capture.getBackendName();
    readback.fps = capture.get(cv::CAP_PROP_FPS);
    readback.width = static_cast<int>(capture.get(cv::CAP_PROP_FRAME_WIDTH));
    readback.height = static_cast<int>(capture.get(cv::CAP_PROP_FRAME_HEIGHT));
    readback.reported_frame_count = static_cast<int>(capture.get(cv::CAP_PROP_FRAME_COUNT));
    readback.fourcc = fourcc_text(static_cast<int>(capture.get(cv::CAP_PROP_FOURCC)));

    cv::Mat frame;
    while (capture.read(frame)) {
        ++readback.decoded_frames;
        if (frame.cols != readback.width || frame.rows != readback.height) {
            readback.all_dimensions_match = false;
        }
        if (expected_presentation == FramePresentation::Any) {
            continue;
        }
        if (expected_presentation == FramePresentation::Color) {
            if (frame.channels() != 3) {
                continue;
            }
            std::vector<cv::Mat> channels;
            cv::split(frame, channels);
            cv::Mat bg_delta;
            cv::Mat gr_delta;
            cv::Mat max_delta;
            cv::absdiff(channels[0], channels[1], bg_delta);
            cv::absdiff(channels[1], channels[2], gr_delta);
            cv::max(bg_delta, gr_delta, max_delta);
            cv::Mat colorful_mask;
            cv::compare(max_delta, 12, colorful_mask, cv::CMP_GT);
            readback.colorful_pixels += static_cast<std::size_t>(cv::countNonZero(colorful_mask));
            continue;
        }
        if (frame.channels() == 1) {
            continue;
        }
        if (frame.channels() != 3) {
            readback.grayscale_channels_match = false;
            continue;
        }
        std::vector<cv::Mat> channels;
        cv::split(frame, channels);
        cv::Mat bg_delta;
        cv::Mat gr_delta;
        cv::absdiff(channels[0], channels[1], bg_delta);
        cv::absdiff(channels[1], channels[2], gr_delta);
        double max_bg_delta = 0.0;
        double max_gr_delta = 0.0;
        cv::minMaxLoc(bg_delta, nullptr, &max_bg_delta);
        cv::minMaxLoc(gr_delta, nullptr, &max_gr_delta);
        if (max_bg_delta > 3.0 || max_gr_delta > 3.0) {
            readback.grayscale_channels_match = false;
        }
    }
    return readback;
}

void expect_valid_avi(const std::filesystem::path &path, const FrameOracle &oracle,
                      const AviReadback &readback, FramePresentation expected_presentation,
                      bool require_mjpg_or_h264 = false) {
    SCOPED_TRACE("backend=" + readback.backend + ", fourcc=" + readback.fourcc +
                 ", reported_frames=" + std::to_string(readback.reported_frame_count));
    EXPECT_FALSE(std::filesystem::is_symlink(path));
    EXPECT_TRUE(std::filesystem::is_regular_file(path));
    EXPECT_GT(readback.size_bytes, 0u);
    ASSERT_TRUE(readback.opened);
    EXPECT_EQ(readback.width, oracle.width);
    EXPECT_EQ(readback.height, oracle.height);
    EXPECT_NEAR(readback.fps, kExpectedFps, kFpsTolerance);
    EXPECT_GT(readback.decoded_frames, 0u);
    EXPECT_EQ(readback.decoded_frames, oracle.output_frames);
    EXPECT_TRUE(readback.all_dimensions_match);
    if (expected_presentation == FramePresentation::Color) {
        EXPECT_GT(readback.colorful_pixels, 1000u);
    }
    if (expected_presentation == FramePresentation::Grayscale) {
        EXPECT_TRUE(readback.grayscale_channels_match);
    }
    if (require_mjpg_or_h264 && has_observable_fourcc(readback.fourcc)) {
        EXPECT_TRUE(is_mjpg_fourcc(readback.fourcc) || is_h264_fourcc(readback.fourcc));
    }
}

gui::ExportParams make_avi_params(const std::filesystem::path &output, const FrameOracle &oracle,
                                  int accumulation_us, int quality, bool color) {
    gui::ExportParams params;
    params.source_path = QString::fromStdString(source_fixture().string());
    params.output_path = QString::fromStdString(output.string());
    params.format = gui::ExportParams::Format::AVI;
    params.fps = static_cast<int>(kExpectedFps);
    params.accumulation_us = accumulation_us;
    params.quality = quality;
    params.color = color;
    params.duration_us = oracle.duration_us;
    return params;
}

bool is_valid_avi(const std::filesystem::path &path) {
    if (std::filesystem::is_symlink(path) || !std::filesystem::is_regular_file(path) ||
        std::filesystem::file_size(path) == 0) {
        return false;
    }
    cv::VideoCapture capture(path.string());
    cv::Mat frame;
    return capture.isOpened() && capture.read(frame);
}

CodecProbeResult probe_codec(const std::filesystem::path &output, int fourcc) {
    CodecProbeResult result;
    cv::VideoWriter writer;
    result.writer_opened = writer.open(output.string(), fourcc, kExpectedFps, cv::Size(64, 48), true);
    if (!result.writer_opened) {
        return result;
    }
    result.writer_backend = writer.getBackendName();
    for (int frame_index = 0; frame_index < 8; ++frame_index) {
        cv::Mat frame(48, 64, CV_8UC3,
                      cv::Scalar((frame_index * 29) % 256, (frame_index * 53) % 256,
                                 (frame_index * 97) % 256));
        writer.write(frame);
    }
    writer.release();
    result.readback = read_avi(output, FramePresentation::Any);
    return result;
}

} // namespace

TEST(ExporterController, Hdf5ExportPreservesGeometry) {
    const std::filesystem::path artifact_dir =
        std::filesystem::path(EBPLUS_GUI_TEST_ARTIFACT_DIR) / "exporter_controller";
    std::filesystem::create_directories(artifact_dir);
    const auto unique_id = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path output = artifact_dir / ("sparklers-" + std::to_string(unique_id) + ".h5");
    const std::filesystem::path source = source_fixture();

    gui::ExporterController controller;
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    bool completed = false;
    bool timed_out = false;
    QString failure;
    QObject::connect(&controller, &gui::ExporterController::completed, [&](const QString&) {
        completed = true;
        loop.quit();
    });
    QObject::connect(&controller, &gui::ExporterController::failed, [&](const QString& message) {
        failure = message;
        loop.quit();
    });
    QObject::connect(&timeout, &QTimer::timeout, [&]() {
        timed_out = true;
        loop.quit();
    });

    gui::ExportParams params;
    params.source_path = QString::fromStdString(source.string());
    params.output_path = QString::fromStdString(output.string());
    params.format = gui::ExportParams::Format::HDF5;
    ASSERT_TRUE(controller.start(params));
    timeout.start(kOperationTimeoutMs);
    loop.exec();

    ASSERT_FALSE(timed_out);
    ASSERT_TRUE(completed) << failure.toStdString();
    ASSERT_TRUE(std::filesystem::is_regular_file(output));

    auto source_camera = Metavision::Camera::from_file(
        source.string(), Metavision::FileConfigHints().real_time_playback(false));
    const auto& source_geometry = source_camera.geometry();
    auto camera = Metavision::Camera::from_file(
        output.string(), Metavision::FileConfigHints().real_time_playback(false));
    const auto& geometry = camera.geometry();
    EXPECT_EQ(geometry.get_width(), source_geometry.get_width());
    EXPECT_EQ(geometry.get_height(), source_geometry.get_height());
    EXPECT_GT(camera.offline_streaming_control().get_duration(), 0);
}

TEST(ExporterController, AviMjpgColorAndGrayRespectFrameCountAndAccumulation) {
    constexpr int color_accumulation_us = 20000;
    constexpr int gray_accumulation_us = 40000;
    const FrameOracle color_oracle = build_frame_oracle(color_accumulation_us, true);
    const FrameOracle gray_oracle = build_frame_oracle(gray_accumulation_us, false);
    ASSERT_EQ(color_oracle.width, 640);
    ASSERT_EQ(color_oracle.height, 480);
    ASSERT_GT(color_oracle.duration_us, 0);
    ASSERT_GT(color_oracle.output_frames, gray_oracle.output_frames);

    const std::filesystem::path artifact_root = make_artifact_root("avi-mjpg-color-gray");
    const std::filesystem::path color_output = artifact_root / "sparklers-color-mjpg.avi";
    const std::filesystem::path gray_output = artifact_root / "sparklers-gray-mjpg.avi";

    const OperationOutcome color_outcome = run_export(
        make_avi_params(color_output, color_oracle, color_accumulation_us, 49, true));
    ASSERT_TRUE(color_outcome.started);
    ASSERT_FALSE(color_outcome.timed_out);
    ASSERT_FALSE(color_outcome.failed) << color_outcome.failure.toStdString();
    ASSERT_TRUE(color_outcome.completed);
    EXPECT_EQ(color_outcome.output.toStdString(), color_output.string());
    expect_valid_avi(color_output, color_oracle, read_avi(color_output, FramePresentation::Color),
                     FramePresentation::Color, true);

    const OperationOutcome gray_outcome = run_export(
        make_avi_params(gray_output, gray_oracle, gray_accumulation_us, 49, false));
    ASSERT_TRUE(gray_outcome.started);
    ASSERT_FALSE(gray_outcome.timed_out);
    ASSERT_FALSE(gray_outcome.failed) << gray_outcome.failure.toStdString();
    ASSERT_TRUE(gray_outcome.completed);
    EXPECT_EQ(gray_outcome.output.toStdString(), gray_output.string());
    expect_valid_avi(gray_output, gray_oracle, read_avi(gray_output, FramePresentation::Grayscale),
                     FramePresentation::Grayscale, true);
}

TEST(ExporterController, AviCodecCapabilityProbe) {
    const std::filesystem::path artifact_root = make_artifact_root("avi-codec-probe");
    const CodecProbeResult mjpg =
        probe_codec(artifact_root / "synthetic-mjpg.avi", cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
    ASSERT_TRUE(mjpg.writer_opened);
    EXPECT_FALSE(mjpg.writer_backend.empty());
    ASSERT_TRUE(mjpg.readback.opened);
    EXPECT_EQ(mjpg.readback.width, 64);
    EXPECT_EQ(mjpg.readback.height, 48);
    EXPECT_NEAR(mjpg.readback.fps, kExpectedFps, kFpsTolerance);
    EXPECT_EQ(mjpg.readback.decoded_frames, 8u);
    if (has_observable_fourcc(mjpg.readback.fourcc)) {
        EXPECT_TRUE(is_mjpg_fourcc(mjpg.readback.fourcc));
    }

    const CodecProbeResult h264 =
        probe_codec(artifact_root / "synthetic-h264.avi", cv::VideoWriter::fourcc('H', '2', '6', '4'));
    if (h264.writer_opened) {
        EXPECT_FALSE(h264.writer_backend.empty());
        ASSERT_TRUE(h264.readback.opened);
        EXPECT_EQ(h264.readback.width, 64);
        EXPECT_EQ(h264.readback.height, 48);
        EXPECT_NEAR(h264.readback.fps, kExpectedFps, kFpsTolerance);
        EXPECT_EQ(h264.readback.decoded_frames, 8u);
        if (has_observable_fourcc(h264.readback.fourcc)) {
            EXPECT_TRUE(is_h264_fourcc(h264.readback.fourcc));
        }
    }
}

TEST(ExporterController, AviHighQualityCompletesWithH264OrMjpgFallback) {
    constexpr int accumulation_us = 20000;
    const FrameOracle oracle = build_frame_oracle(accumulation_us, true);
    ASSERT_GT(oracle.output_frames, 0u);

    const std::filesystem::path artifact_root = make_artifact_root("avi-high-quality");
    const std::filesystem::path output = artifact_root / "sparklers-high-quality.avi";
    const OperationOutcome outcome =
        run_export(make_avi_params(output, oracle, accumulation_us, 90, true));

    ASSERT_TRUE(outcome.started);
    ASSERT_FALSE(outcome.timed_out);
    ASSERT_FALSE(outcome.failed) << outcome.failure.toStdString();
    ASSERT_TRUE(outcome.completed);
    EXPECT_EQ(outcome.output.toStdString(), output.string());
    expect_valid_avi(output, oracle, read_avi(output, FramePresentation::Any),
                     FramePresentation::Any, true);
}

TEST(ExporterController, AviMissingParentFailsWithoutCompleted) {
    ASSERT_TRUE(std::filesystem::is_regular_file(source_fixture()));
    const std::filesystem::path artifact_root = make_artifact_root("avi-missing-parent");
    const std::filesystem::path missing_parent = artifact_root / "missing-parent";
    const std::filesystem::path output = missing_parent / "output.avi";
    ASSERT_FALSE(std::filesystem::exists(missing_parent));

    gui::ExportParams params;
    params.source_path = QString::fromStdString(source_fixture().string());
    params.output_path = QString::fromStdString(output.string());
    params.format = gui::ExportParams::Format::AVI;
    params.fps = static_cast<int>(kExpectedFps);
    params.accumulation_us = 20000;
    params.quality = 49;
    params.color = true;
    const OperationOutcome outcome = run_export(params);

    ASSERT_TRUE(outcome.started);
    ASSERT_FALSE(outcome.timed_out);
    ASSERT_FALSE(outcome.completed);
    ASSERT_TRUE(outcome.failed);
    EXPECT_FALSE(outcome.failure.isEmpty());
    EXPECT_FALSE(is_valid_avi(output));
}

int main(int argc, char** argv) {
    QCoreApplication application(argc, argv);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
