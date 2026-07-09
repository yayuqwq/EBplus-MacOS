// algo/tests/raw_event_stream.h — real-event test fixture for algorithm tests.
//
// Loads ALL CD events from a Metavision .raw file into an in-memory
// std::vector<Event> (one-shot, real_time_playback(false)) so that every
// algorithm can be exercised against real sensor data deterministically.
//
// The synthetic-event unit tests (test_phase6/7/8_10) only verify API contracts
// with tiny hand-crafted event batches. They pass even when an algorithm
// produces flat/blank output on real streams — the historical cause of
// "268 tests green, GUI still broken". This harness closes that gap: tests
// built on it assert real-world behaviour (non-flat frames, sane filter
// rates, no NaN divergence, valid bounding boxes) that synthetic data
// cannot exercise.
//
// Usage:
//   gui_algo_test::RawEventStream stream(GUI_TEST_RAW_FILE);
//   ASSERT_TRUE(stream.loaded());
//   for (const auto& batch : stream.batches(/*window_us=*/33000)) {
//       algo.process(batch.data(), batch.size());
//   }
//
// The .raw path is injected via the GUI_TEST_RAW_FILE compile definition
// (absolute source path) so the test binary finds the data regardless of
// the current working directory.

#ifndef GUI_ALGO_TESTS_RAW_EVENT_STREAM_H
#define GUI_ALGO_TESTS_RAW_EVENT_STREAM_H

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

#include <metavision/sdk/base/events/event_cd.h>
#include <metavision/sdk/stream/camera.h>
#include <metavision/sdk/stream/file_config_hints.h>

#include "algo/common/event.h"

namespace gui_algo_test {

using gui_algo::Event;

/// @brief Loads a .raw file into memory and exposes it for algorithm testing.
class RawEventStream {
public:
    /// @brief Constructs and loads @p path. Loading is synchronous and blocks
    ///        until EOF (real_time_playback(false) drains the file instantly).
    explicit RawEventStream(const std::string& path) {
        Metavision::FileConfigHints hints;
        hints.real_time_playback(false);

        Metavision::Camera cam;
        try {
            cam = Metavision::Camera::from_file(path, hints);
        } catch (const std::exception& e) {
            std::fprintf(stderr, "RawEventStream: failed to open %s: %s\n",
                         path.c_str(), e.what());
            return;
        }
        width_ = cam.geometry().get_width();
        height_ = cam.geometry().get_height();

        cam.cd().add_callback([&](const Metavision::EventCD* b,
                                  const Metavision::EventCD* e) {
            const std::size_t n = static_cast<std::size_t>(e - b);
            const std::size_t base = events_.size();
            events_.resize(base + n);
            // Layout-compatible copy: EventCD -> gui_algo::Event.
            for (std::size_t i = 0; i < n; ++i) {
                events_[base + i] = Event(b[i].x, b[i].y, b[i].p, b[i].t);
            }
            if (n > 0) last_t_ = b[n - 1].t;
        });

        cam.start();
        // Drain the file. real_time_playback(false) delivers events at full
        // speed; a short bounded wait is sufficient for the small fixture.
        for (int i = 0; i < 100 && cam.is_running(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        if (cam.is_running()) cam.stop();

        loaded_ = !events_.empty();
        if (loaded_) {
            std::fprintf(stderr,
                         "[RawEventStream] %s: %dx%d, %zu events, duration %lld us\n",
                         path.c_str(), width_, height_, events_.size(),
                         static_cast<long long>(last_t_));
        }
    }

    bool loaded() const { return loaded_; }
    int width() const { return width_; }
    int height() const { return height_; }
    Metavision::timestamp duration_us() const { return last_t_; }
    std::size_t size() const { return events_.size(); }

    /// @brief All events (time-ordered).
    const std::vector<Event>& events() const { return events_; }

    /// @brief Splits the stream into time-windowed batches of contiguous
    ///        events for incremental algorithm feeding. The last batch may be
    ///        shorter than the window. Returns one batch per @p window_us of
    ///        event time (NOT wall-clock time).
    std::vector<std::vector<Event>> batches(Metavision::timestamp window_us) const {
        std::vector<std::vector<Event>> out;
        if (events_.empty() || window_us <= 0) return out;
        std::vector<Event> cur;
        Metavision::timestamp base = events_.front().t;
        for (const Event& e : events_) {
            if (e.t - base >= window_us && !cur.empty()) {
                out.push_back(std::move(cur));
                cur.clear();
                base = e.t;
            }
            cur.push_back(e);
        }
        if (!cur.empty()) out.push_back(std::move(cur));
        return out;
    }

    /// @brief Events whose coordinates fall inside the centered ROI of size
    ///        @p roi_w x @p roi_h, with coordinates shifted to the ROI origin.
    /// Used by EventToVideo (which operates on a 128x128 ROI per project
    /// convention).
    std::vector<Event> centered_roi(int roi_w, int roi_h) const {
        std::vector<Event> out;
        if (width_ <= 0 || height_ <= 0) return out;
        const int rx = (width_ - roi_w) / 2;
        const int ry = (height_ - roi_h) / 2;
        out.reserve(events_.size() / 4);
        for (const Event& e : events_) {
            if (e.x < rx || e.x >= rx + roi_w || e.y < ry || e.y >= ry + roi_h) continue;
            out.emplace_back(static_cast<std::uint16_t>(e.x - rx),
                             static_cast<std::uint16_t>(e.y - ry),
                             e.p, e.t);
        }
        return out;
    }

private:
    bool loaded_{false};
    int width_{0};
    int height_{0};
    Metavision::timestamp last_t_{0};
    std::vector<Event> events_;
};

} // namespace gui_algo_test

#endif // GUI_ALGO_TESTS_RAW_EVENT_STREAM_H
