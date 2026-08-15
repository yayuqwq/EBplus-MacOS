// Test: flip_x survives a file-playback loop for the same source event window.
//
// The test intentionally observes events_window_ready rather than rendered
// pixels: a frame callback carries an arbitrary timer-selected window, while
// this signal identifies the exact [start, start + accumulation) event batch.

#include <QCoreApplication>
#include <QEventLoop>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <metavision/sdk/base/events/event_cd.h>
#include <metavision/sdk/stream/camera.h>
#include <metavision/sdk/stream/file_config_hints.h>
#include <metavision/sdk/stream/offline_streaming_control.h>

#include "algo_bridge/filter_chain.h"
#include "app/file_frame_generator.h"

namespace {

using Event = Metavision::EventCD;
using Metavision::timestamp;
using std::chrono::milliseconds;
using std::chrono::steady_clock;

struct CapturedWindow {
    timestamp start_us{};
    std::vector<Event> events;
};

void print_event(const char* label, std::size_t index, const Event& event) {
    std::fprintf(stderr, "%s[%zu] = (x=%d, y=%d, p=%d, t=%lld)\n", label, index,
                 static_cast<int>(event.x), static_cast<int>(event.y),
                 static_cast<int>(event.p), static_cast<long long>(event.t));
}

bool verify_source_window(const CapturedWindow& window, timestamp expected_start,
                          timestamp accumulation_us) {
    if (window.start_us != expected_start) {
        std::fprintf(stderr, "FAIL: baseline window start=%lld, expected=%lld\n",
                     static_cast<long long>(window.start_us),
                     static_cast<long long>(expected_start));
        return false;
    }
    if (window.events.empty()) {
        std::fprintf(stderr, "FAIL: baseline window [%lld,%lld) contains no events\n",
                     static_cast<long long>(expected_start),
                     static_cast<long long>(expected_start + accumulation_us));
        return false;
    }
    const timestamp end_us = expected_start + accumulation_us;
    for (std::size_t i = 0; i < window.events.size(); ++i) {
        const Event& event = window.events[i];
        if (event.t < expected_start || event.t >= end_us) {
            std::fprintf(stderr, "FAIL: baseline event outside [%lld,%lld)\n",
                         static_cast<long long>(expected_start), static_cast<long long>(end_us));
            print_event("actual", i, event);
            return false;
        }
    }
    return true;
}

bool verify_flip_x(const char* label, const CapturedWindow& actual,
                   const CapturedWindow& source, long width) {
    if (actual.start_us != source.start_us) {
        std::fprintf(stderr, "FAIL: %s window start=%lld, expected=%lld\n", label,
                     static_cast<long long>(actual.start_us),
                     static_cast<long long>(source.start_us));
        return false;
    }
    if (actual.events.size() != source.events.size()) {
        std::fprintf(stderr, "FAIL: %s event count=%zu, expected=%zu\n", label,
                     actual.events.size(), source.events.size());
        return false;
    }
    for (std::size_t i = 0; i < source.events.size(); ++i) {
        const Event& input = source.events[i];
        const Event& output = actual.events[i];
        const int expected_x = static_cast<int>(width) - 1 - static_cast<int>(input.x);
        if (static_cast<int>(output.x) != expected_x || output.y != input.y ||
            output.p != input.p || output.t != input.t) {
            std::fprintf(stderr,
                         "FAIL: %s flip mismatch at event %zu; expected "
                         "(x=%d, y=%d, p=%d, t=%lld)\n",
                         label, i, expected_x, static_cast<int>(input.y),
                         static_cast<int>(input.p), static_cast<long long>(input.t));
            print_event("actual", i, output);
            return false;
        }
    }
    return true;
}

bool verify_identical_windows(const CapturedWindow& baseline,
                              const CapturedWindow& post_loop) {
    if (post_loop.start_us != baseline.start_us) {
        std::fprintf(stderr, "FAIL: post-loop window start=%lld, baseline=%lld\n",
                     static_cast<long long>(post_loop.start_us),
                     static_cast<long long>(baseline.start_us));
        return false;
    }
    if (post_loop.events.size() != baseline.events.size()) {
        std::fprintf(stderr, "FAIL: post-loop event count=%zu, baseline=%zu\n",
                     post_loop.events.size(), baseline.events.size());
        return false;
    }
    for (std::size_t i = 0; i < baseline.events.size(); ++i) {
        const Event& expected = baseline.events[i];
        const Event& actual = post_loop.events[i];
        if (actual.x != expected.x || actual.y != expected.y ||
            actual.p != expected.p || actual.t != expected.t) {
            std::fprintf(stderr, "FAIL: post-loop output differs from initial flip at event %zu\n", i);
            print_event("expected", i, expected);
            print_event("actual", i, actual);
            return false;
        }
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "Usage: %s <file.raw>\n", argv[0]);
        return 1;
    }

    QCoreApplication app(argc, argv);
    const std::string path = argv[1];

    gui::FilterChain chain;
    gui::FileFrameGenerator gen;
    gen.set_fps(60);
    gen.set_accumulation_time_us(33000);
    const timestamp baseline_start_us = 0;
    const timestamp accumulation_us = gen.accumulation_time_us();

    Metavision::FileConfigHints hints;
    hints.real_time_playback(false);
    Metavision::Camera cam = Metavision::Camera::from_file(path, hints);
    const long width = cam.geometry().get_width();
    const long height = cam.geometry().get_height();
    std::fprintf(stderr, "File: %s  geometry: %ldx%ld\n", path.c_str(), width, height);

    chain.set_geometry(static_cast<int>(width), static_cast<int>(height));
    gen.set_filter_chain(&chain);
    gen.set_geometry(width, height);
    gen.set_loop(true);

    std::atomic_bool camera_done{false};
    cam.cd().add_callback([&](const Event* begin, const Event* end) {
        gen.add_events(begin, end);
    });
    cam.add_status_change_callback([&](const Metavision::CameraStatus& status) {
        if (status == Metavision::CameraStatus::STOPPED) {
            camera_done.store(true, std::memory_order_release);
        }
    });

    timestamp file_duration_us = 0;
    cam.start();
    try {
        auto& osc = cam.offline_streaming_control();
        if (osc.is_ready()) file_duration_us = osc.get_duration();
    } catch (...) {
    }
    gen.set_duration_us(file_duration_us);

    const auto buffer_deadline = steady_clock::now() + milliseconds(3000);
    while (!camera_done.load(std::memory_order_acquire) && steady_clock::now() < buffer_deadline) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        std::this_thread::sleep_for(milliseconds(1));
    }
    if (cam.is_running()) cam.stop();
    if (!camera_done.load(std::memory_order_acquire)) {
        std::fprintf(stderr, "FAIL: RAW buffering did not finish before timeout\n");
        return 1;
    }

    file_duration_us = gen.duration_us();
    gen.set_loading_complete(true);
    std::fprintf(stderr, "Buffered. duration=%lld us  events=%zu\n",
                 static_cast<long long>(file_duration_us), gen.event_count());
    if (file_duration_us <= baseline_start_us || gen.event_count() == 0) {
        std::fprintf(stderr, "FAIL: RAW fixture did not provide a playable event stream\n");
        return 1;
    }

    enum class Capture { None, RawBaseline, FlippedBaseline };
    Capture capture = Capture::None;
    std::optional<CapturedWindow> raw_baseline;
    std::optional<CapturedWindow> flipped_baseline;
    std::optional<CapturedWindow> post_loop;
    int loop_count = 0;
    bool loop_seen = false;

    QObject::connect(&gen, &gui::FileFrameGenerator::looped, &app, [&]() {
        ++loop_count;
        loop_seen = true;
    });
    QObject::connect(&gen, &gui::FileFrameGenerator::events_window_ready,
                     &app,
                     [&](std::shared_ptr<std::vector<Event>> events, timestamp start_us) {
                         if (!events) return;
                         const CapturedWindow window{start_us, *events};
                         if (capture == Capture::RawBaseline &&
                             start_us == baseline_start_us && !raw_baseline) {
                             raw_baseline = window;
                         }
                         if (capture == Capture::FlippedBaseline &&
                             start_us == baseline_start_us && !flipped_baseline) {
                             flipped_baseline = window;
                         }
                         if (loop_seen && start_us == baseline_start_us && !post_loop) {
                             post_loop = window;
                         }
                     });

    // seek(0) renders synchronously, so each baseline is the exact [0, window) input.
    capture = Capture::RawBaseline;
    gen.seek(baseline_start_us);
    capture = Capture::None;
    if (!raw_baseline || !verify_source_window(*raw_baseline, baseline_start_us, accumulation_us)) {
        return 1;
    }

    chain.set_stage_enabled("flip_x", true);
    if (!chain.is_stage_enabled("flip_x")) {
        std::fprintf(stderr, "FAIL: flip_x was not enabled before playback\n");
        return 1;
    }
    capture = Capture::FlippedBaseline;
    gen.seek(baseline_start_us);
    capture = Capture::None;
    if (!flipped_baseline || !verify_flip_x("initial", *flipped_baseline, *raw_baseline, width)) {
        return 1;
    }

    // The timeout bounds event-loop waiting only; the sample is selected by
    // a real looped() signal followed by the first matching source window.
    gen.play();
    const auto loop_deadline = steady_clock::now() + milliseconds(2000);
    while (!post_loop && steady_clock::now() < loop_deadline) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        std::this_thread::sleep_for(milliseconds(1));
    }
    gen.pause();

    if (loop_count < 1 || !loop_seen) {
        std::fprintf(stderr, "FAIL: no loop boundary observed before timeout\n");
        return 1;
    }
    if (!post_loop) {
        std::fprintf(stderr,
                     "FAIL: no post-loop matching window at start=%lld after %d loop(s)\n",
                     static_cast<long long>(baseline_start_us), loop_count);
        return 1;
    }
    if (!chain.is_stage_enabled("flip_x")) {
        std::fprintf(stderr, "FAIL: flip_x is disabled after %d loop(s)\n", loop_count);
        return 1;
    }
    if (!verify_flip_x("post-loop", *post_loop, *raw_baseline, width) ||
        !verify_identical_windows(*flipped_baseline, *post_loop)) {
        return 1;
    }

    std::fprintf(stderr,
                 "PASS: flip_x preserved through %d real loop(s); baseline and post-loop "
                 "window=[%lld,%lld), events=%zu\n",
                 loop_count, static_cast<long long>(baseline_start_us),
                 static_cast<long long>(baseline_start_us + accumulation_us),
                 post_loop->events.size());
    return 0;
}
