// algo/tests/test_phase8_10.cpp — unit tests for Phase 8-10 modules.
//
// Covers Phase 8 (algo/cv/ §4.3.13–4.3.23), Phase 9 (algo/cv/ §4.3.24–4.3.27),
// and Phase 10 (algo/analytics/ §4.4.1–4.4.7). Compiled with -Wall -Wextra -Werror.

#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <utility>
#include <vector>

#include <opencv2/core.hpp>

#include <metavision/sdk/base/events/event_cd.h>

#include "algo/common/event.h"
#include "algo/common/event_packet.h"
#include "algo/cv/line_segment_detector.h"
#include "algo/cv/hough_line_tracker.h"
#include "algo/cv/hough_circle_tracker.h"
#include "algo/cv/orientation_cluster.h"
#include "algo/cv/cluster_lif.h"
#include "algo/cv/background_mask_filter.h"
#include "algo/cv/trigger_synced_filter.h"
#include "algo/cv/bandpass_filter.h"
#include "algo/cv/optical_gyro.h"
#include "algo/cv/xyt_visualizer.h"
#include "algo/cv/time_surface.h"
#include "algo/analytics/active_marker.h"
#include "algo/analytics/event_to_video.h"
#include "algo/analytics/e2vid/event_voxel_grid.h"
#include "algo/analytics/e2vid/intensity_rescaler.h"
#include "algo/analytics/e2vid/unsharp_mask.h"
#include "algo/analytics/e2vid/e2vid_inference.h"
#include "algo/analytics/flow_statistics.h"
#include "algo/analytics/isi_analyzer.h"
#include "algo/analytics/particle_counter.h"
#include "algo/analytics/auto_bias_controller.h"
#include "algo/analytics/freq_detector.h"
#include "algo/analytics/sensor_self_test.h"

#include "raw_event_stream.h"

using gui_algo::Event;
using gui_algo::EventPacket;
using gui_algo::MutableEventPacket;
using gui_algo::LineSegmentDetector;
using gui_algo::HoughLineTracker;
using gui_algo::HoughCircleTracker;
using gui_algo::OrientationCluster;
using gui_algo::ClusterLIF;
using gui_algo::BackgroundMaskFilter;
using gui_algo::TriggerSyncedFilter;
using gui_algo::BandpassFilter;
using gui_algo::OpticalGyro;
using gui_algo::XYTVisualizer;
using gui_algo::TimeSurface;
using gui_algo::ActiveMarker;
using gui_algo::EventToVideo;
using gui_algo::FlowStatistics;
using gui_algo::ISIAnalyzer;
using gui_algo::ParticleCounter;
using gui_algo::AutoBiasController;
using gui_algo::FreqDetector;
using gui_algo::SensorSelfTest;

std::filesystem::path missing_e2vid_model_path() {
    const std::filesystem::path source_path{__FILE__};
    return source_path.parent_path().parent_path().parent_path() /
           ".tmp/m7-e2vid-fallback/nonexistent-model.onnx";
}

static std::vector<Event> make_events(int w, int h, int count, int t0 = 0) {
    std::vector<Event> ev;
    ev.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        const uint16_t x = static_cast<uint16_t>(i % w);
        const uint16_t y = static_cast<uint16_t>((i / w) % h);
        ev.emplace_back(x, y, i & 1, t0 + i * 100);
    }
    return ev;
}

static EventPacket make_packet(const std::vector<Event>& v) {
    return EventPacket(v.data(), v.size());
}

static std::optional<std::filesystem::path> e2vid_test_model_path() {
    const char* const value = std::getenv("EBPLUS_E2VID_TEST_MODEL");
    if (value == nullptr || value[0] == '\0') return std::nullopt;
    return std::filesystem::path(value);
}

static std::vector<Event> make_recurrent_events_a(int width, int height) {
    std::vector<Event> events;
    events.reserve(96);
    for (int i = 0; i < 96; ++i) {
        const auto x = static_cast<std::uint16_t>((3 + 7 * i) % width);
        const auto y = static_cast<std::uint16_t>((5 + 5 * i) % height);
        events.emplace_back(x, y, i % 3 != 0, 1000 + i * 100);
    }
    return events;
}

static std::vector<Event> make_recurrent_events_b(int width, int height) {
    std::vector<Event> events;
    events.reserve(96);
    for (int i = 0; i < 96; ++i) {
        const auto x = static_cast<std::uint16_t>((11 + 11 * i) % width);
        const auto y = static_cast<std::uint16_t>((7 + 13 * i) % height);
        events.emplace_back(x, y, (i % 4) < 2, 20000 + i * 125);
    }
    return events;
}

static std::vector<Event> make_even_recurrent_events(int width, int height) {
    std::vector<Event> events;
    events.reserve(96);
    for (int i = 0; i < 96; ++i) {
        const auto x = static_cast<std::uint16_t>(
            2 * ((3 + 7 * i) % (width / 2)));
        const auto y = static_cast<std::uint16_t>(
            2 * ((5 + 5 * i) % (height / 2)));
        events.emplace_back(x, y, i % 3 != 0, 40000 + i * 150);
    }
    return events;
}

struct TimedFrame {
    cv::Mat frame;
    double elapsed_ms;
};

static TimedFrame infer_timed(gui_algo::E2VIDInference& inference,
                              const std::vector<Event>& events) {
    const auto start = std::chrono::steady_clock::now();
    cv::Mat frame = inference.infer(events.data(), events.size());
    const auto elapsed = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start);
    return {std::move(frame), elapsed.count()};
}

struct NeuralFrameStats {
    double min;
    double max;
};

static std::optional<NeuralFrameStats> neural_frame_stats(
    const cv::Mat& frame, const cv::Size& expected_size) {
    if (frame.empty() || frame.type() != CV_32FC1 ||
        frame.size() != expected_size || !cv::checkRange(frame, true)) {
        return std::nullopt;
    }
    double min = 0.0;
    double max = 0.0;
    cv::minMaxLoc(frame, &min, &max);
    if (!std::isfinite(min) || !std::isfinite(max)) return std::nullopt;
    return NeuralFrameStats{min, max};
}

static std::filesystem::path tracked_raw_fixture_path() {
    return std::filesystem::path(__FILE__).parent_path() / "sparklers.raw";
}

static std::vector<Event> centered_raw_window(
    const gui_algo_test::RawEventStream& stream,
    Metavision::timestamp start_us,
    Metavision::timestamp window_us,
    int roi_x,
    int roi_y,
    int roi_width,
    int roi_height) {
    std::vector<Event> out;
    const Metavision::timestamp end_us = start_us + window_us;
    for (const Event& event : stream.events()) {
        if (event.t < start_us) continue;
        if (event.t >= end_us) break;
        if (event.x < roi_x || event.x >= roi_x + roi_width ||
            event.y < roi_y || event.y >= roi_y + roi_height) {
            continue;
        }
        out.emplace_back(static_cast<std::uint16_t>(event.x - roi_x),
                         static_cast<std::uint16_t>(event.y - roi_y),
                         event.p, event.t);
    }
    return out;
}

// =========================================================================
// Phase 8: algo/cv/ §4.3.13–4.3.23
// =========================================================================

// --- 4.3.13 LineSegmentDetector ---
TEST(LineSegmentDetectorTest, Construction) {
    LineSegmentDetector d(64, 48);
    (void)d;
    SUCCEED();
}
TEST(LineSegmentDetectorTest, Params) {
    LineSegmentDetector d(32, 32);
    d.set_min_line_length_px(50);
    EXPECT_EQ(d.min_line_length_px(), 50);
    d.set_max_line_gap_px(10);
    EXPECT_EQ(d.max_line_gap_px(), 10);
}
TEST(LineSegmentDetectorTest, ProcessEmpty) {
    LineSegmentDetector d(32, 32);
    std::vector<Event> empty;
    auto pkt = make_packet(empty);
    auto result = d.process(pkt);
    EXPECT_TRUE(result.empty());
}
TEST(LineSegmentDetectorTest, ElisedParams) {
    LineSegmentDetector d(32, 32);
    d.set_max_age_us(50000);
    EXPECT_EQ(d.max_age_us(), 50000);
    d.set_num_orientations(8);
    EXPECT_EQ(d.num_orientations(), 8);
}
// ELiSeD port: a horizontal line of ON events at y=16 with temporal contrast
// supplied by neighbouring rows (y=15 older, y=17 newer) must produce a
// roughly horizontal segment whose length meets the minimum threshold.
TEST(LineSegmentDetectorTest, DetectsHorizontalLine) {
    LineSegmentDetector d(48, 48);
    d.set_min_line_length_px(10);
    std::vector<Event> ev;
    // Pre-fill rows 15 (older) and 17 (newer) for timestamp-contrast.
    for (int x = 4; x <= 43; ++x) {
        ev.emplace_back(static_cast<uint16_t>(x), 15, 1, 1000);
        ev.emplace_back(static_cast<uint16_t>(x), 17, 1, 5000);
    }
    // Line row, emitted last so neighbours are already populated.
    for (int x = 4; x <= 43; ++x) {
        ev.emplace_back(static_cast<uint16_t>(x), 16, 1, 3000);
    }
    auto pkt = make_packet(ev);
    auto result = d.process(pkt);
    ASSERT_GE(result.size(), 1u);
    // Segment should be roughly horizontal (angle within [0,180) and near 0).
    EXPECT_GE(result[0].angle, 0.0f);
    EXPECT_LT(result[0].angle, 180.0f);
    const float dx = result[0].end.x - result[0].start.x;
    const float dy = result[0].end.y - result[0].start.y;
    EXPECT_GT(dx * dx, dy * dy);  // horizontal extent dominates
    EXPECT_GE(result[0].track_id, 0);
}

// --- 4.3.14 HoughLineTracker ---
TEST(HoughLineTrackerTest, Construction) {
    HoughLineTracker t(64, 48);
    (void)t;
    SUCCEED();
}
TEST(HoughLineTrackerTest, Params) {
    HoughLineTracker t(32, 32);
    t.set_threshold(100);
    EXPECT_EQ(t.threshold(), 100);
    t.set_num_theta_bins(45);
    EXPECT_EQ(t.num_theta_bins(), 45);
    t.set_hough_decay_factor(0.5F);
    EXPECT_FLOAT_EQ(t.hough_decay_factor(), 0.5F);
}
TEST(HoughLineTrackerTest, ProcessEmpty) {
    HoughLineTracker t(32, 32);
    std::vector<Event> empty;
    auto pkt = make_packet(empty);
    auto result = t.process(pkt);
    EXPECT_TRUE(result.empty());
}

// --- 4.3.15 HoughCircleTracker ---
TEST(HoughCircleTrackerTest, Construction) {
    HoughCircleTracker t(64, 48);
    (void)t;
    SUCCEED();
}
TEST(HoughCircleTrackerTest, Params) {
    HoughCircleTracker t(32, 32);
    t.set_max_radius_px(100);
    EXPECT_EQ(t.max_radius_px(), 100);
    t.set_threshold(50);
    EXPECT_EQ(t.threshold(), 50);
}
TEST(HoughCircleTrackerTest, ProcessEmpty) {
    HoughCircleTracker t(32, 32);
    std::vector<Event> empty;
    auto pkt = make_packet(empty);
    auto result = t.process(pkt);
    EXPECT_TRUE(result.empty());
}

TEST(HoughCircleTrackerTest, SmallDtPacketsDoNotAmplifyAccumulator) {
    // Regression: the jAER decay formula 1/(0.0001*decay*dt) is > 1 for
    // dt < 10 ms. jAER never hits that branch (render-cycle packets), but
    // the GUI feeds SDK batches (~1-5 ms) — the accumulator was amplified
    // every packet until all cells saturated to the same value, and the
    // scan-order tie-break then reported a phantom circle at the bottom-
    // right interior cell, persisted by maxCoordinate. The factor is now
    // jAER-exact for dt >= T and exp((dt-T)/T) below T.
    HoughCircleTracker t(64, 48);
    t.set_max_radius_px(8);
    // Many small-dt packets, one event each at the center.
    for (int i = 0; i < 500; ++i) {
        std::vector<Event> ev;
        ev.emplace_back(32, 24, 1, 1000 + i * 1000);  // 1 ms apart
        auto pkt = make_packet(ev);
        t.accumulate_only(pkt);
    }
    const auto& accum = t.accum();
    const float mx = *std::max_element(accum.begin(), accum.end());
    ASSERT_TRUE(std::isfinite(mx));
    // No phantom detection at the bottom-right interior cell (62,46) —
    // the saturated-tiebreak artifact. (A legitimate detection near the
    // center is fine.)
    for (const auto& c : t.find_peaks()) {
        EXPECT_FALSE(std::abs(c.center.x - 62.0F) <= 1.0F &&
                     std::abs(c.center.y - 46.0F) <= 1.0F)
            << "phantom bottom-right circle detected";
    }
}

TEST(HoughCircleTrackerTest, DecayAppliesAtSmallPacketCadence) {
    // The earlier clamp-to-1 fix stalled decay below T (factor == 1 for
    // dt < 10 ms): votes never expired and every persistent structure
    // became a false-positive circle. The exp continuation must really
    // shrink the accumulator at small packet cadence.
    HoughCircleTracker t(64, 48);
    t.set_max_radius_px(8);
    std::vector<Event> ev;
    ev.emplace_back(32, 24, 1, 1000);
    auto pkt = make_packet(ev);
    t.accumulate_only(pkt);
    const float v0 = *std::max_element(t.accum().begin(), t.accum().end());
    ASSERT_GT(v0, 0.0f);
    for (int i = 0; i < 20; ++i) {  // +1 ms per empty packet
        std::vector<Event> empty;
        auto ep = make_packet(empty);
        t.accumulate_only(ep, 1000 + (i + 1) * 1000);
    }
    const float v1 = *std::max_element(t.accum().begin(), t.accum().end());
    EXPECT_LT(v1, v0 * 0.5f);
}

TEST(HoughCircleTrackerTest, DecayMatchesJaeRAtRenderCadence) {
    // jAER parity must be exact in jAER's operating range (dt >= T):
    // decay=1, dt=30 ms → factor 1/(0.0001*1*30000) = 1/3.
    HoughCircleTracker t(64, 48);
    t.set_max_radius_px(8);
    std::vector<Event> ev;
    ev.emplace_back(32, 24, 1, 1000);
    auto pkt = make_packet(ev);
    t.accumulate_only(pkt);
    const float v0 = *std::max_element(t.accum().begin(), t.accum().end());
    ASSERT_GT(v0, 0.0f);
    std::vector<Event> empty;
    auto ep = make_packet(empty);
    t.accumulate_only(ep, 31000);  // dt = 30000 us
    const float v1 = *std::max_element(t.accum().begin(), t.accum().end());
    EXPECT_NEAR(v1 / v0, 1.0f / 3.0f, 0.01f);
}

// --- 4.3.17 OrientationCluster ---
TEST(OrientationClusterTest, Construction) {
    OrientationCluster c(64, 48);
    (void)c;
    SUCCEED();
}
TEST(OrientationClusterTest, ProcessEmpty) {
    OrientationCluster c(32, 32);
    std::vector<Event> empty;
    auto pkt = make_packet(empty);
    auto result = c.process(pkt);
    EXPECT_TRUE(result.empty());
}

// --- 4.3.18 ClusterLIF ---
TEST(ClusterLIFTest, Construction) {
    ClusterLIF c(64, 48);
    (void)c;
    SUCCEED();
}
TEST(ClusterLIFTest, Params) {
    ClusterLIF c(32, 32);
    c.set_tau_ms(20.0f);
    EXPECT_FLOAT_EQ(c.tau_ms(), 20.0f);
    c.set_threshold(2.0f);
    EXPECT_FLOAT_EQ(c.threshold(), 2.0f);
}
TEST(ClusterLIFTest, ProcessEmpty) {
    ClusterLIF c(32, 32);
    std::vector<Event> empty;
    auto pkt = make_packet(empty);
    auto result = c.process(pkt);
    EXPECT_TRUE(result.empty());
}

// --- 4.3.19 BackgroundMaskFilter ---
TEST(BackgroundMaskFilterTest, Construction) {
    BackgroundMaskFilter f(64, 48);
    (void)f;
    SUCCEED();
}
TEST(BackgroundMaskFilterTest, Params) {
    BackgroundMaskFilter f(32, 32);
    f.set_learning_window_s(10.0f);
    EXPECT_FLOAT_EQ(f.learning_window_s(), 10.0f);
    f.set_background_rate_threshold_hz(5.0f);
    EXPECT_FLOAT_EQ(f.background_rate_threshold_hz(), 5.0f);
}
TEST(BackgroundMaskFilterTest, ProcessEmpty) {
    BackgroundMaskFilter f(32, 32);
    std::vector<Event> empty;
    auto pkt = make_packet(empty);
    const auto& mask = f.process(pkt);
    EXPECT_FALSE(mask.empty());
}

// --- 4.3.21 TriggerSyncedFilter (jAER FilterSyncedEvents port) ---
TEST(TriggerSyncedFilterTest, Construction) {
    TriggerSyncedFilter f;
    // jAER defaults: t0=500us, t1=500us (window). trigger_window_us() maps to t1.
    EXPECT_EQ(f.trigger_window_us(), 500);
    EXPECT_EQ(f.t0(), 500);
    EXPECT_EQ(f.t1(), 500);
    EXPECT_EQ(f.trigger_channel(), 0);
}
TEST(TriggerSyncedFilterTest, Params) {
    TriggerSyncedFilter f;
    f.set_trigger_window_us(50000);
    EXPECT_EQ(f.trigger_window_us(), 50000);
    f.set_t0(1000);
    f.set_t1(2000);
    EXPECT_EQ(f.t0(), 1000);
    EXPECT_EQ(f.t1(), 2000);
    f.set_trigger_channel(3);
    EXPECT_EQ(f.trigger_channel(), 3);
}
TEST(TriggerSyncedFilterTest, ProcessEmpty) {
    TriggerSyncedFilter f;
    std::vector<Event> empty;
    auto pkt = make_packet(empty);
    auto result = f.process(pkt);
    EXPECT_TRUE(result.empty());
}

// --- 4.3.22 BandpassFilter ---
TEST(BandpassFilterTest, Construction) {
    BandpassFilter f;
    EXPECT_DOUBLE_EQ(f.value(), 0.0);
}
TEST(BandpassFilterTest, Params) {
    BandpassFilter f(2.0f, 20.0f, 2, 0.01);
    f.set_cutoffs(5.0, 50.0);
    f.set_sample_dt(0.02);
    SUCCEED();
}
TEST(BandpassFilterTest, ProcessScalar) {
    BandpassFilter f;
    double y = f.process(100.0);
    EXPECT_TRUE(std::isfinite(y));
}
// Regression: bandpass order must be hp(lp(x)) — low-pass first, then high-pass.
// Match the common building block algo/common/filter/bandpass.h and jAER.
// Verify by comparing output to the reference BandpassFilter for a DC input:
// a band-pass must remove DC, so a constant input must converge toward 0.
TEST(BandpassFilterTest, RemovesDcAfterConvergence) {
    BandpassFilter f(0.5f, 10.0f, 4, 0.01);
    for (int i = 0; i < 1000; ++i) f.process(1.0);
    EXPECT_NEAR(f.value(), 0.0, 1e-3);
}

// --- 4.3.23 OpticalGyro ---
TEST(OpticalGyroTest, Construction) {
    OpticalGyro g(64, 48);
    auto m = g.total_motion();
    EXPECT_FLOAT_EQ(m.dx, 0.0f);
    EXPECT_FLOAT_EQ(m.dy, 0.0f);
}
TEST(OpticalGyroTest, Params) {
    OpticalGyro g(32, 32);
    g.set_stabilization_strength(0.5f);
    EXPECT_FLOAT_EQ(g.stabilization_strength(), 0.5f);
    g.set_smoothing_window_ms(200.0f);
    EXPECT_FLOAT_EQ(g.smoothing_window_ms(), 200.0f);
    // Rotation estimation toggle (jAER opticalGyroRotationEnabled default=false)
    EXPECT_FALSE(g.rotation_enabled());
    g.set_rotation_enabled(true);
    EXPECT_TRUE(g.rotation_enabled());
}

// =========================================================================
// Phase 9: algo/cv/ §4.3.24–4.3.27
// =========================================================================

// --- 4.3.25 XYTVisualizer ---
TEST(XYTVisualizerTest, Construction) {
    XYTVisualizer v;
    EXPECT_FLOAT_EQ(v.time_window_ms(), 50.0f);
}
TEST(XYTVisualizerTest, Params) {
    XYTVisualizer v;
    v.set_time_window_ms(500.0f);
    EXPECT_FLOAT_EQ(v.time_window_ms(), 500.0f);
    v.set_point_size(5.0f);
    EXPECT_FLOAT_EQ(v.point_size(), 5.0f);
}
TEST(XYTVisualizerTest, Process) {
    XYTVisualizer v;
    auto ev = make_events(32, 32, 50);
    v.process(ev.data(), ev.size());
    SUCCEED();
}

// --- 4.3.27 TimeSurface ---
TEST(TimeSurfaceTest, Construction) {
    TimeSurface ts(64, 48);
    EXPECT_EQ(ts.width(), 64);
    EXPECT_EQ(ts.height(), 48);
}
TEST(TimeSurfaceTest, Params) {
    TimeSurface ts(32, 32);
    ts.set_decay_time_us(200000);
    EXPECT_EQ(ts.decay_time_us(), 200000);
    ts.set_refresh_rate_hz(60);
    EXPECT_EQ(ts.refresh_rate_hz(), 60);
    EXPECT_EQ(ts.refresh_interval_us(), 16666);
}
TEST(TimeSurfaceTest, ProcessAndRender) {
    TimeSurface ts(32, 32);
    auto ev = make_events(32, 32, 50);
    ts.process(ev.data(), ev.size());
    cv::Mat img = ts.render();
    EXPECT_FALSE(img.empty());
    EXPECT_EQ(img.rows, 32);
    EXPECT_EQ(img.cols, 32);
}
TEST(TimeSurfaceTest, ExponentialDecay) {
    // dv Accumulator EXPONENTIAL (accumulator.hpp:119-154): per-event
    // decay + contribute. Defaults: eventContribution=0.15, neutral=0,
    // [min,max]=[0,1]. A pixel hit once at t=0, rendered tau later:
    //   display = 0.15 * exp(-1) ≈ 0.0552 → gray ≈ 14.
    TimeSurface ts(32, 32, TimeSurface::Channels::Merged, 100000,
                   TimeSurface::Palette::Gray, 30,
                   TimeSurface::Decay::Exponential, 100000);
    EXPECT_EQ(ts.decay(), TimeSurface::Decay::Exponential);
    EXPECT_EQ(ts.tau_us(), 100000);
    std::vector<Event> ev;
    ev.emplace_back(5, 5, 1, 0);         // reference pixel at t=0
    ev.emplace_back(10, 10, 1, 100000);  // advances current_t_ by exactly tau
    ts.process(ev.data(), ev.size());
    cv::Mat img = ts.render();
    const cv::Vec3b old_px = img.at<cv::Vec3b>(5, 5);
    const cv::Vec3b new_px = img.at<cv::Vec3b>(10, 10);
    // pot=0.15; display = 0.15 * exp(-tau/tau) = 0.15*exp(-1) ≈ 14.1
    const double expect_old = 255.0 * 0.15 * std::exp(-1.0);
    EXPECT_NEAR(old_px[0], expect_old, 2.0);
    EXPECT_EQ(old_px[0], old_px[1]);  // Gray palette: all channels equal
    EXPECT_EQ(old_px[1], old_px[2]);
    // pot=0.15; dt=0 → display = 0.15 → gray ≈ 38
    const double expect_new = 255.0 * 0.15;
    EXPECT_NEAR(new_px[0], expect_new, 2.0);
    EXPECT_EQ(img.at<cv::Vec3b>(0, 0)[0], 0);  // never-hit pixel stays black
    // tau_us setter round-trip.
    ts.set_tau_us(50000);
    EXPECT_EQ(ts.tau_us(), 50000);
}

TEST(TimeSurfaceTest, ExponentialAccumulation) {
    // dv Accumulator: multiple events at the same pixel accumulate
    // (contribute), saturating to max_potential (1.0). With
    // eventContribution=0.15 and tau=1s, 10 events 100us apart accumulate
    // well past 1.0 → clamped to 1.0 → gray 255 (dt≈0 at render).
    TimeSurface ts(32, 32, TimeSurface::Channels::Merged, 100000,
                   TimeSurface::Palette::Gray, 30,
                   TimeSurface::Decay::Exponential, 1000000);
    std::vector<Event> ev;
    for (int i = 0; i < 10; ++i)
        ev.emplace_back(5, 5, 1, i * 100);  // 100us apart, << tau=1s
    ev.emplace_back(10, 10, 1, 1000);       // single event, advances current_t_
    ts.process(ev.data(), ev.size());
    cv::Mat img = ts.render();
    // 10 events saturate to 1.0; dt=100us → exp(-0.0001)≈1.0 → gray 255.
    EXPECT_EQ(img.at<cv::Vec3b>(5, 5)[0], 255);
    // Single-event pixel: pot=0.15, dt=0 → gray ≈ 38.
    EXPECT_NEAR(img.at<cv::Vec3b>(10, 10)[0], 255.0 * 0.15, 2.0);
}
TEST(TimeSurfaceTest, LinearDecayUnchangedByDefault) {
    // Default decay stays Linear: hard cut to 0 at the window tail.
    TimeSurface ts(32, 32, TimeSurface::Channels::Merged, 100000,
                   TimeSurface::Palette::Gray);
    EXPECT_EQ(ts.decay(), TimeSurface::Decay::Linear);
    std::vector<Event> ev;
    ev.emplace_back(5, 5, 1, 0);
    ev.emplace_back(10, 10, 1, 100000);  // dt == decay_time_us -> cut to 0
    ts.process(ev.data(), ev.size());
    cv::Mat img = ts.render();
    EXPECT_EQ(img.at<cv::Vec3b>(5, 5)[0], 0);
    EXPECT_EQ(img.at<cv::Vec3b>(10, 10)[0], 255);
}

TEST(TimeSurfaceTest, SplitChannelsDoNotAccumulateOppositePolarities) {
    // In Merged mode, opposite-polarity events at the same pixel contribute
    // to one accumulator. Split mode maintains one accumulator per polarity
    // and merges their rendered colors with a per-channel maximum.
    const std::vector<Event> events{
        Event(5, 5, 0, 0),
        Event(5, 5, 1, 100),
    };
    TimeSurface merged(16, 12, TimeSurface::Channels::Merged, 100000,
                       TimeSurface::Palette::Gray, 30,
                       TimeSurface::Decay::Exponential, 1000000);
    TimeSurface split(16, 12, TimeSurface::Channels::Split, 100000,
                      TimeSurface::Palette::Gray, 30,
                      TimeSurface::Decay::Exponential, 1000000);
    merged.process(events.data(), events.size());
    split.process(events.data(), events.size());

    const cv::Vec3b merged_px = merged.render().at<cv::Vec3b>(5, 5);
    const cv::Vec3b split_px = split.render().at<cv::Vec3b>(5, 5);
    const double expected_merged =
        255.0 * (0.15 * std::exp(-100.0 / 1000000.0) + 0.15);
    const double expected_split = 255.0 * 0.15;
    EXPECT_NEAR(merged_px[0], expected_merged, 2.0);
    EXPECT_NEAR(split_px[0], expected_split, 2.0);
    EXPECT_GT(merged_px[0], split_px[0] + 20);
    EXPECT_EQ(split.channels(), TimeSurface::Channels::Split);
}

TEST(TimeSurfaceTest, ResetClearsAndReplaysDeterministically) {
    TimeSurface ts(16, 12, TimeSurface::Channels::Merged, 100000,
                   TimeSurface::Palette::Gray, 30,
                   TimeSurface::Decay::Exponential, 1000000);
    const std::vector<Event> events{
        Event(2, 3, 0, 100),
        Event(7, 8, 1, 500),
        Event(2, 3, 0, 900),
    };

    ts.process(events.data(), events.size());
    const cv::Mat first = ts.render();
    ASSERT_GT(cv::norm(first, cv::NORM_INF), 0.0);

    ts.reset();
    const cv::Mat cleared = ts.render();
    EXPECT_EQ(cv::norm(cleared, cv::NORM_INF), 0.0);

    ts.process(events.data(), events.size());
    const cv::Mat replay = ts.render();
    EXPECT_EQ(cv::norm(first, replay, cv::NORM_INF), 0.0);
}

TEST(TimeSurfaceTest, RepresentativePaletteMappingAtFullScale) {
    struct PaletteCase {
        TimeSurface::Palette palette;
        cv::Vec3b expected_bgr;
    };
    const PaletteCase cases[] = {
        {TimeSurface::Palette::Gray, cv::Vec3b(255, 255, 255)},
        {TimeSurface::Palette::Hot, cv::Vec3b(255, 255, 255)},
        {TimeSurface::Palette::Plasma, cv::Vec3b(0, 255, 255)},
        {TimeSurface::Palette::Turbo, cv::Vec3b(0, 0, 255)},
    };
    const Event event(1, 1, 1, 100);
    for (const PaletteCase& test : cases) {
        TimeSurface ts(4, 4, TimeSurface::Channels::Merged, 100000,
                       test.palette, 30);
        ts.process(&event, 1);
        const cv::Vec3b actual = ts.render().at<cv::Vec3b>(1, 1);
        EXPECT_EQ(actual[0], test.expected_bgr[0]);
        EXPECT_EQ(actual[1], test.expected_bgr[1]);
        EXPECT_EQ(actual[2], test.expected_bgr[2]);
    }
}

// =========================================================================
// Phase 10: algo/analytics/ §4.4.1–4.4.7
// =========================================================================

// --- 4.4.1 ActiveMarker ---
TEST(ActiveMarkerTest, Construction) {
    ActiveMarker m(64, 48);
    EXPECT_EQ(m.width(), 64);
    EXPECT_EQ(m.height(), 48);
}
TEST(ActiveMarkerTest, Params) {
    ActiveMarker m(32, 32);
    m.set_window_ms(50.0f);
    EXPECT_FLOAT_EQ(m.window_ms(), 50.0f);
    m.set_heatmap_threshold(100);
    EXPECT_EQ(m.heatmap_threshold(), 100);
    m.set_enable_freq_detect(true);
    EXPECT_TRUE(m.enable_freq_detect());
}
TEST(ActiveMarkerTest, Process) {
    ActiveMarker m(32, 32);
    auto ev = make_events(32, 32, 100);
    m.process(ev.data(), ev.size());
    SUCCEED();
}

// --- 4.4.2 EventToVideo ---
TEST(EventToVideoTest, Construction) {
    EventToVideo v(64, 48);
    EXPECT_EQ(v.width(), 64);
    EXPECT_EQ(v.height(), 48);
}
TEST(EventToVideoTest, ModeSwitching) {
    EventToVideo v(32, 32);
    v.set_mode(EventToVideo::Mode::InteractingMaps);
    EXPECT_EQ(v.mode(), EventToVideo::Mode::InteractingMaps);
    v.set_mode(EventToVideo::Mode::E2VID);
    EXPECT_EQ(v.mode(), EventToVideo::Mode::E2VID);
}
TEST(EventToVideoTest, ModeSwitchClearsPendingE2VIDTemporalState) {
    constexpr int kWidth = 8;
    constexpr int kHeight = 8;
    const std::vector<Event> pending_before_switch{
        Event(2, 2, 0, 100),
        Event(2, 2, 0, 200),
    };
    const std::vector<Event> intended_after_switch{
        Event(5, 4, 1, 1000),
        Event(5, 4, 1, 1100),
    };

    EventToVideo switched(kWidth, kHeight, EventToVideo::Mode::E2VID);
    switched.set_e2vid_downsample(false);
    ASSERT_FALSE(switched.e2vid_model_loaded());
    switched.process(pending_before_switch.data(), pending_before_switch.size());
    switched.set_mode(EventToVideo::Mode::BardowVariational);
    switched.set_mode(EventToVideo::Mode::E2VID);
    switched.process(intended_after_switch.data(), intended_after_switch.size());
    const cv::Mat switched_frame = switched.get_frame();

    EventToVideo fresh(kWidth, kHeight, EventToVideo::Mode::E2VID);
    fresh.set_e2vid_downsample(false);
    ASSERT_FALSE(fresh.e2vid_model_loaded());
    fresh.process(intended_after_switch.data(), intended_after_switch.size());
    const cv::Mat fresh_frame = fresh.get_frame();

    ASSERT_EQ(switched_frame.type(), CV_8UC1);
    ASSERT_EQ(switched_frame.size(), cv::Size(kWidth, kHeight));
    ASSERT_EQ(fresh_frame.type(), CV_8UC1);
    ASSERT_EQ(fresh_frame.size(), cv::Size(kWidth, kHeight));
    EXPECT_EQ(cv::norm(switched_frame, fresh_frame, cv::NORM_INF), 0.0)
        << "pending E2VID events leaked through a mode switch";
}
TEST(EventToVideoTest, ProcessAndGetFrame) {
    EventToVideo v(32, 32, EventToVideo::Mode::BardowVariational);
    auto ev = make_events(32, 32, 100);
    v.process(ev.data(), ev.size());
    cv::Mat frame = v.get_frame();
    EXPECT_FALSE(frame.empty());
}

// Diagnostic: BardowVariational with downsample should produce non-flat output.
TEST(EventToVideoTest, BardowVariationalNotFlat) {
    EventToVideo v(128, 128, EventToVideo::Mode::BardowVariational);
    v.set_downsample(true);
    auto ev = make_events(128, 128, 500);
    v.process(ev.data(), ev.size());
    cv::Mat frame = v.get_frame();
    EXPECT_FALSE(frame.empty());
    EXPECT_EQ(frame.rows, 128);
    EXPECT_EQ(frame.cols, 128);
    // Check that the frame is not uniformly gray (128).
    double min_val, max_val;
    cv::minMaxLoc(frame, &min_val, &max_val);
    EXPECT_LT(min_val, 100.0) << "min=" << min_val << " max=" << max_val;
    EXPECT_GT(max_val, 150.0) << "min=" << min_val << " max=" << max_val;
}

// Diagnostic: BardowVariational without downsample should produce non-flat output.
TEST(EventToVideoTest, BardowVariationalNotFlatNoDownsample) {
    EventToVideo v(128, 128, EventToVideo::Mode::BardowVariational);
    v.set_downsample(false);
    auto ev = make_events(128, 128, 500);
    v.process(ev.data(), ev.size());
    cv::Mat frame = v.get_frame();
    EXPECT_FALSE(frame.empty());
    double min_val, max_val;
    cv::minMaxLoc(frame, &min_val, &max_val);
    EXPECT_LT(min_val, 100.0) << "min=" << min_val << " max=" << max_val;
    EXPECT_GT(max_val, 150.0) << "min=" << min_val << " max=" << max_val;
}

// Diagnostic: InteractingMaps should produce non-flat output (warm-start fix).
TEST(EventToVideoTest, InteractingMapsNotFlat) {
    EventToVideo v(128, 128, EventToVideo::Mode::InteractingMaps);
    v.set_downsample(true);
    auto ev = make_events(128, 128, 500);
    v.process(ev.data(), ev.size());
    cv::Mat frame = v.get_frame();
    EXPECT_FALSE(frame.empty());
    EXPECT_EQ(frame.rows, 128);
    EXPECT_EQ(frame.cols, 128);
    double min_val, max_val;
    cv::minMaxLoc(frame, &min_val, &max_val);
    EXPECT_LT(min_val, 100.0) << "min=" << min_val << " max=" << max_val;
    EXPECT_GT(max_val, 150.0) << "min=" << min_val << " max=" << max_val;
}
TEST(EventToVideoTest, E2VIDModeHeuristic) {
    // E2VID without a model reconstructs a deterministic heuristic frame.
    const std::vector<Event> events{
        Event(2, 2, 0, 100),
        Event(5, 4, 1, 200),
        Event(2, 2, 0, 600),
        Event(6, 1, 1, 900),
    };
    EventToVideo v(8, 8, EventToVideo::Mode::E2VID);
    v.set_e2vid_downsample(false);
    ASSERT_FALSE(v.e2vid_model_loaded());
    v.process(events.data(), events.size());
    const cv::Mat first = v.get_frame();
    ASSERT_FALSE(first.empty());
    ASSERT_EQ(first.type(), CV_8UC1);
    ASSERT_EQ(first.rows, 8);
    ASSERT_EQ(first.cols, 8);
    double mn = 0.0, mx = 0.0;
    cv::minMaxLoc(first, &mn, &mx);
    EXPECT_GE(mn, 0.0);
    EXPECT_LE(mx, 255.0);
    EXPECT_GT(first.at<std::uint8_t>(4, 5), first.at<std::uint8_t>(2, 2));

    v.reset();
    v.process(events.data(), events.size());
    const cv::Mat replay = v.get_frame();
    EXPECT_EQ(cv::norm(first, replay, cv::NORM_INF), 0.0);
}
TEST(EventToVideoTest, E2VIDParams) {
    EventToVideo v(32, 32, EventToVideo::Mode::E2VID);
    v.set_e2vid_num_bins(10);
    EXPECT_EQ(v.e2vid_num_bins(), 10);
    v.set_e2vid_auto_hdr(true);
    EXPECT_TRUE(v.e2vid_auto_hdr());
    v.set_unsharp_amount(0.5f);
    EXPECT_FLOAT_EQ(v.unsharp_amount(), 0.5f);
    v.set_unsharp_sigma(2.0f);
    EXPECT_FLOAT_EQ(v.unsharp_sigma(), 2.0f);
    v.set_bilateral_sigma(1.0f);
    EXPECT_FLOAT_EQ(v.bilateral_sigma(), 1.0f);
}
TEST(EventToVideoTest, E2VIDModelLoadFailure) {
    // A repository-local missing path must leave the heuristic usable.
    const auto missing_model_path = missing_e2vid_model_path();
    ASSERT_FALSE(std::filesystem::exists(missing_model_path));
    const std::vector<Event> events{
        Event(2, 2, 1, 100), Event(4, 4, 0, 300),
    };
    EventToVideo v(8, 8, EventToVideo::Mode::E2VID);
    v.set_e2vid_downsample(false);
    v.set_model_path(missing_model_path.string());
    EXPECT_FALSE(v.e2vid_model_loaded());
    v.process(events.data(), events.size());
    const cv::Mat frame = v.get_frame();
    EXPECT_FALSE(frame.empty());
    EXPECT_EQ(frame.type(), CV_8UC1);
    EXPECT_EQ(frame.size(), cv::Size(8, 8));
    EXPECT_FALSE(std::filesystem::exists(missing_model_path));
}
TEST(EventToVideoTest, RealModelSmoke) {
    const auto model_path = e2vid_test_model_path();
    if (!model_path.has_value()) {
        GTEST_SKIP() << "EBPLUS_E2VID_TEST_MODEL is not set";
    }
    std::error_code model_error;
    ASSERT_TRUE(std::filesystem::is_regular_file(*model_path, model_error))
        << "EBPLUS_E2VID_TEST_MODEL must name a regular file: "
        << model_path->string() << " error=" << model_error.message();

    constexpr int kWidth = 64;
    constexpr int kHeight = 48;
    EventToVideo video(kWidth, kHeight, EventToVideo::Mode::E2VID);
    video.set_e2vid_downsample(false);
    video.set_model_path(model_path->string());
    ASSERT_TRUE(video.e2vid_model_loaded());
    ASSERT_EQ(video.e2vid_num_bins(), 5);

    const auto events = make_recurrent_events_b(kWidth, kHeight);
    video.process(events.data(), events.size());
    const cv::Mat frame = video.get_frame();
    ASSERT_FALSE(frame.empty());
    EXPECT_EQ(frame.type(), CV_8UC1);
    EXPECT_EQ(frame.size(), cv::Size(kWidth, kHeight));
    double min = 0.0;
    double max = 0.0;
    cv::minMaxLoc(frame, &min, &max);
    EXPECT_GE(min, 0.0);
    EXPECT_LE(max, 255.0);
    std::cout << "M7Slice3E2 EventToVideo frame_range=[" << min << ',' << max
              << "]\n";
}

// --- E2VID submodule tests ---
TEST(EventVoxelGridTest, Construction) {
    gui_algo::EventVoxelGrid g(64, 48, 5);
    EXPECT_EQ(g.width(), 64);
    EXPECT_EQ(g.height(), 48);
    EXPECT_EQ(g.num_bins(), 5);
    EXPECT_EQ(g.size(), static_cast<std::size_t>(5 * 64 * 48));
}
TEST(EventVoxelGridTest, BuildAndNormalize) {
    gui_algo::EventVoxelGrid g(32, 32, 5);
    auto ev = make_events(32, 32, 100);
    const auto& grid = g.build(ev.data(), ev.size());
    EXPECT_EQ(grid.size(), static_cast<std::size_t>(5 * 32 * 32));
    g.normalize();  // should not crash
    SUCCEED();
}
TEST(EventVoxelGridTest, RenderPreview) {
    gui_algo::EventVoxelGrid g(32, 32, 5);
    auto ev = make_events(32, 32, 50);
    g.build(ev.data(), ev.size());
    cv::Mat preview = g.render_preview();
    EXPECT_EQ(preview.type(), CV_8UC3);
    EXPECT_EQ(preview.rows, 32);
    EXPECT_EQ(preview.cols, 32);
}
TEST(IntensityRescalerTest, Construction) {
    gui_algo::IntensityRescaler r;
    EXPECT_FALSE(r.auto_hdr());
    EXPECT_FLOAT_EQ(r.imin(), 0.0f);
    EXPECT_FLOAT_EQ(r.imax(), 1.0f);
}
TEST(IntensityRescalerTest, AutoHDR) {
    gui_algo::IntensityRescaler r(true, 5);
    EXPECT_TRUE(r.auto_hdr());
    cv::Mat img(32, 32, CV_32FC1, cv::Scalar(0.5));
    cv::Mat out = r(img);
    EXPECT_EQ(out.type(), CV_8UC1);
    EXPECT_EQ(out.rows, 32);
}
TEST(IntensityRescalerTest, ResetClearsBounds) {
    // NIT 2 regression: reset() should clear imin_/imax_ to defaults.
    gui_algo::IntensityRescaler r(true, 3);
    cv::Mat img(32, 32, CV_32FC1, cv::Scalar(0.5));
    r(img);
    r.reset();
    EXPECT_FLOAT_EQ(r.imin(), 0.0f);
    EXPECT_FLOAT_EQ(r.imax(), 1.0f);
}
TEST(UnsharpMaskTest, Construction) {
    gui_algo::UnsharpMaskFilter f(0.3f, 1.0f);
    EXPECT_FLOAT_EQ(f.amount(), 0.3f);
    EXPECT_FLOAT_EQ(f.sigma(), 1.0f);
}
TEST(UnsharpMaskTest, Apply) {
    gui_algo::UnsharpMaskFilter f(0.3f, 1.0f);
    cv::Mat img(32, 32, CV_32FC1, cv::Scalar(0.5));
    cv::Mat out = f(img);
    EXPECT_EQ(out.type(), CV_32FC1);
    EXPECT_EQ(out.rows, 32);
}
TEST(BilateralFilterTest, Apply) {
    gui_algo::BilateralImageFilter f(1.0f);
    cv::Mat img(32, 32, CV_8UC1, cv::Scalar(128));
    cv::Mat out = f(img);
    EXPECT_EQ(out.type(), CV_8UC1);
    EXPECT_EQ(out.rows, 32);
}
TEST(E2VIDInferenceTest, Construction) {
    gui_algo::E2VIDInference e(64, 48, 5);
    EXPECT_EQ(e.width(), 64);
    EXPECT_EQ(e.height(), 48);
    EXPECT_EQ(e.num_bins(), 5);
    EXPECT_FALSE(e.is_model_loaded());
}
TEST(E2VIDInferenceTest, HeuristicInference) {
    const std::vector<Event> events{
        Event(1, 1, 1, 100),
        Event(3, 2, 0, 200),
        Event(1, 1, 1, 600),
        Event(6, 4, 0, 900),
    };
    gui_algo::E2VIDInference e(8, 6, 5);
    e.set_downsample(false);
    ASSERT_FALSE(e.is_model_loaded());
    const cv::Mat frame = e.infer(events.data(), events.size());
    ASSERT_FALSE(frame.empty());
    ASSERT_EQ(frame.type(), CV_8UC1);
    ASSERT_EQ(frame.size(), cv::Size(8, 6));
    double mn = 0.0, mx = 0.0;
    cv::minMaxLoc(frame, &mn, &mx);
    EXPECT_GE(mn, 0.0);
    EXPECT_LE(mx, 255.0);
    EXPECT_GT(frame.at<std::uint8_t>(1, 1), frame.at<std::uint8_t>(2, 3));
}
TEST(E2VIDInferenceTest, ModelLoadFailure) {
    const auto missing_model_path = missing_e2vid_model_path();
    ASSERT_FALSE(std::filesystem::exists(missing_model_path));
    const std::vector<Event> events{
        Event(2, 2, 1, 100), Event(4, 4, 0, 300),
    };
    gui_algo::E2VIDInference e(8, 8, 5);
    e.set_downsample(false);
    EXPECT_FALSE(e.load_model(missing_model_path.string()));
    EXPECT_FALSE(e.is_model_loaded());
    const cv::Mat frame = e.infer(events.data(), events.size());
    EXPECT_FALSE(frame.empty());
    EXPECT_EQ(frame.type(), CV_8UC1);
    EXPECT_EQ(frame.size(), cv::Size(8, 8));
    EXPECT_FALSE(std::filesystem::exists(missing_model_path));
}
TEST(E2VIDInferenceTest, NumBinsClamp) {
    // BUG 1 regression: num_bins must be clamped to [1, 20].
    gui_algo::E2VIDInference e(32, 32, 100);
    EXPECT_EQ(e.num_bins(), 20);
    e.set_num_bins(0);
    EXPECT_EQ(e.num_bins(), 1);
    e.set_num_bins(10);
    EXPECT_EQ(e.num_bins(), 10);
    const std::vector<Event> events{
        Event(2, 2, 1, 100), Event(4, 4, 0, 300),
    };
    const cv::Mat alternate_bins = e.infer(events.data(), events.size());
    EXPECT_EQ(alternate_bins.type(), CV_8UC1);
    EXPECT_EQ(alternate_bins.size(), cv::Size(32, 32));
}
TEST(E2VIDInferenceTest, ResetReplaysHeuristicFallbackDeterministically) {
    const std::vector<Event> events{
        Event(2, 2, 1, 100),
        Event(4, 3, 0, 300),
        Event(6, 5, 1, 900),
    };
    gui_algo::E2VIDInference e(8, 8, 5);
    e.set_downsample(false);
    const cv::Mat first = e.infer(events.data(), events.size());
    e.reset();
    const cv::Mat replay = e.infer(events.data(), events.size());
    EXPECT_EQ(cv::norm(first, replay, cv::NORM_INF), 0.0);
}
TEST(E2VIDInferenceTest, DownsampleModesPreserveSensorDimensions) {
    const std::vector<Event> even_events{
        Event(2, 2, 1, 100),
        Event(4, 4, 0, 300),
        Event(6, 2, 1, 900),
    };
    gui_algo::E2VIDInference downsampled(8, 6, 5);
    ASSERT_TRUE(downsampled.downsample());
    const cv::Mat downsampled_frame =
        downsampled.infer(even_events.data(), even_events.size());
    EXPECT_EQ(downsampled_frame.type(), CV_8UC1);
    EXPECT_EQ(downsampled_frame.size(), cv::Size(8, 6));

    gui_algo::E2VIDInference full_resolution(8, 6, 5);
    full_resolution.set_downsample(false);
    const cv::Mat full_resolution_frame =
        full_resolution.infer(even_events.data(), even_events.size());
    EXPECT_EQ(full_resolution_frame.type(), CV_8UC1);
    EXPECT_EQ(full_resolution_frame.size(), cv::Size(8, 6));
}
TEST(E2VIDInferenceTest, HotPixelMaskPreservedAcrossNumBins) {
    // BUG 7 regression: set_num_bins must not drop the hot-pixel mask.
    gui_algo::E2VIDInference e(32, 32, 5);
    std::vector<std::uint8_t> mask(32 * 32, 1);
    mask[0] = 0;  // mark (0,0) as hot
    e.set_hot_pixel_mask(mask);
    e.set_num_bins(10);
    // After rebuilding the voxel grid, the mask should still be active.
    // Verify by checking that infer still works (no crash).
    auto ev = make_events(32, 32, 100);
    cv::Mat frame = e.infer(ev.data(), ev.size());
    EXPECT_FALSE(frame.empty());
}
TEST(E2VIDInferenceTest, CropToSensor) {
    // BUG 2 regression: crop_to_sensor should be a no-op for sensor-sized images.
    gui_algo::E2VIDInference e(32, 32, 5);
    cv::Mat sensor_sized(32, 32, CV_8UC1, cv::Scalar(128));
    cv::Mat out1 = e.crop_to_sensor(sensor_sized);
    EXPECT_EQ(out1.rows, 32);
    EXPECT_EQ(out1.cols, 32);
    // A larger image should be cropped back.
    cv::Mat padded(64, 64, CV_8UC1, cv::Scalar(200));
    cv::Mat out2 = e.crop_to_sensor(padded);
    EXPECT_EQ(out2.rows, 32);
    EXPECT_EQ(out2.cols, 32);
}
TEST(E2VIDInferenceTest, RealRecurrentModelStateAndReset) {
    const auto model_path = e2vid_test_model_path();
    if (!model_path.has_value()) {
        GTEST_SKIP() << "EBPLUS_E2VID_TEST_MODEL is not set";
    }
    std::error_code model_error;
    ASSERT_TRUE(std::filesystem::is_regular_file(*model_path, model_error))
        << "EBPLUS_E2VID_TEST_MODEL must name a regular file: "
        << model_path->string() << " error=" << model_error.message();

    constexpr int kWidth = 64;
    constexpr int kHeight = 48;
    constexpr double kReplayTolerance = 1e-5;
    constexpr double kStateEffectMinimum = 1e-4;
    const cv::Size sensor_size(kWidth, kHeight);
    const auto events_a = make_recurrent_events_a(kWidth, kHeight);
    const auto events_b = make_recurrent_events_b(kWidth, kHeight);

    gui_algo::E2VIDInference inference(kWidth, kHeight, 2);
    inference.set_downsample(false);
    ASSERT_TRUE(inference.load_model(model_path->string()));
    ASSERT_TRUE(inference.is_model_loaded());
    ASSERT_EQ(inference.num_bins(), 5);

    inference.reset();
    const TimedFrame a1 = infer_timed(inference, events_a);
    const TimedFrame b_stateful = infer_timed(inference, events_b);

    inference.reset();
    const TimedFrame b_zero = infer_timed(inference, events_b);

    inference.reset();
    const TimedFrame a2 = infer_timed(inference, events_a);
    const TimedFrame b_replay = infer_timed(inference, events_b);

    const auto a1_stats = neural_frame_stats(a1.frame, sensor_size);
    const auto b_stateful_stats = neural_frame_stats(b_stateful.frame, sensor_size);
    const auto b_zero_stats = neural_frame_stats(b_zero.frame, sensor_size);
    const auto a2_stats = neural_frame_stats(a2.frame, sensor_size);
    const auto b_replay_stats = neural_frame_stats(b_replay.frame, sensor_size);
    ASSERT_TRUE(a1_stats.has_value()) << "Expected finite CV_32FC1 neural output for A1; type="
                                      << a1.frame.type();
    ASSERT_TRUE(b_stateful_stats.has_value())
        << "Expected finite CV_32FC1 neural output for stateful B; type="
        << b_stateful.frame.type();
    ASSERT_TRUE(b_zero_stats.has_value())
        << "Expected finite CV_32FC1 neural output for zero-state B; type="
        << b_zero.frame.type();
    ASSERT_TRUE(a2_stats.has_value()) << "Expected finite CV_32FC1 neural output for A2; type="
                                      << a2.frame.type();
    ASSERT_TRUE(b_replay_stats.has_value())
        << "Expected finite CV_32FC1 neural output for replay B; type="
        << b_replay.frame.type();

    const double b_stateful_vs_zero =
        cv::norm(b_stateful.frame, b_zero.frame, cv::NORM_INF);
    const double a_replay_difference = cv::norm(a1.frame, a2.frame, cv::NORM_INF);
    const double b_replay_difference =
        cv::norm(b_stateful.frame, b_replay.frame, cv::NORM_INF);
    EXPECT_GT(b_stateful_vs_zero, kStateEffectMinimum);
    EXPECT_LE(a_replay_difference, kReplayTolerance);
    EXPECT_LE(b_replay_difference, kReplayTolerance);
    std::cout << "M7Slice3E2 recurrent ranges A1=[" << a1_stats->min << ','
              << a1_stats->max << "] B_stateful=[" << b_stateful_stats->min
              << ',' << b_stateful_stats->max << "] B_zero=["
              << b_zero_stats->min << ',' << b_zero_stats->max
              << "] B_stateful_vs_zero_inf=" << b_stateful_vs_zero
              << " A_replay_inf=" << a_replay_difference
              << " B_replay_inf=" << b_replay_difference
              << " elapsed_ms=[" << a1.elapsed_ms << ','
              << b_stateful.elapsed_ms << ',' << b_zero.elapsed_ms << ','
              << a2.elapsed_ms << ',' << b_replay.elapsed_ms << "]\n";
}
TEST(E2VIDInferenceTest, RealRecurrentModelDownsampleSmoke) {
    const auto model_path = e2vid_test_model_path();
    if (!model_path.has_value()) {
        GTEST_SKIP() << "EBPLUS_E2VID_TEST_MODEL is not set";
    }
    std::error_code model_error;
    ASSERT_TRUE(std::filesystem::is_regular_file(*model_path, model_error))
        << "EBPLUS_E2VID_TEST_MODEL must name a regular file: "
        << model_path->string() << " error=" << model_error.message();

    constexpr int kWidth = 62;
    constexpr int kHeight = 46;
    const cv::Size padded_size(64, 48);
    const cv::Size sensor_size(kWidth, kHeight);
    const auto events = make_even_recurrent_events(kWidth, kHeight);

    gui_algo::E2VIDInference inference(kWidth, kHeight, 2);
    ASSERT_TRUE(inference.downsample());
    ASSERT_TRUE(inference.load_model(model_path->string()));
    ASSERT_TRUE(inference.is_model_loaded());
    ASSERT_EQ(inference.num_bins(), 5);

    const TimedFrame result = infer_timed(inference, events);
    const auto result_stats = neural_frame_stats(result.frame, padded_size);
    ASSERT_TRUE(result_stats.has_value())
        << "Expected finite CV_32FC1 padded neural output for downsampled inference; type="
        << result.frame.type() << " size=" << result.frame.cols << 'x'
        << result.frame.rows;
    const cv::Mat cropped = inference.crop_to_sensor(result.frame);
    const auto cropped_stats = neural_frame_stats(cropped, sensor_size);
    ASSERT_TRUE(cropped_stats.has_value())
        << "Expected finite sensor-sized crop after downsampled inference; type="
        << cropped.type() << " size=" << cropped.cols << 'x' << cropped.rows;
    std::cout << "M7Slice3E2 downsample range=[" << result_stats->min << ','
              << result_stats->max << "] crop_range=[" << cropped_stats->min
              << ',' << cropped_stats->max << "] elapsed_ms="
              << result.elapsed_ms << '\n';
}

TEST(E2VIDInferenceTest, RealRecurrentModelTrackedRawWindows) {
    const auto model_path = e2vid_test_model_path();
    if (!model_path.has_value()) {
        GTEST_SKIP() << "EBPLUS_E2VID_TEST_MODEL is not set";
    }
    std::error_code model_error;
    ASSERT_TRUE(std::filesystem::is_regular_file(*model_path, model_error))
        << "EBPLUS_E2VID_TEST_MODEL must name a regular file: "
        << model_path->string() << " error=" << model_error.message();

    gui_algo_test::RawEventStream stream(tracked_raw_fixture_path().string());
    ASSERT_TRUE(stream.loaded()) << "Failed to decode tracked RAW fixture: "
                                 << tracked_raw_fixture_path().string();
    ASSERT_GE(stream.width(), 128);
    ASSERT_GE(stream.height(), 128);

    constexpr int kRoiWidth = 128;
    constexpr int kRoiHeight = 128;
    constexpr Metavision::timestamp kWindowUs = 33333;
    const int roi_x = (stream.width() - kRoiWidth) / 2;
    const int roi_y = (stream.height() - kRoiHeight) / 2;
    const cv::Size expected_size(kRoiWidth, kRoiHeight);

    gui_algo::E2VIDInference inference(kRoiWidth, kRoiHeight, 2);
    inference.set_downsample(false);
    ASSERT_TRUE(inference.load_model(model_path->string()));
    ASSERT_TRUE(inference.is_model_loaded());
    ASSERT_EQ(inference.num_bins(), 5);

    for (const Metavision::timestamp start_us :
         {Metavision::timestamp{0}, kWindowUs, 2 * kWindowUs}) {
        const auto events = centered_raw_window(stream, start_us, kWindowUs,
                                                roi_x, roi_y, kRoiWidth, kRoiHeight);
        ASSERT_FALSE(events.empty()) << "Tracked RAW ROI window [" << start_us << ','
                                     << (start_us + kWindowUs) << ") is empty";
        const TimedFrame result = infer_timed(inference, events);
        const auto stats = neural_frame_stats(result.frame, expected_size);
        ASSERT_TRUE(stats.has_value())
            << "Expected finite CV_32FC1 neural output for tracked RAW window at "
            << start_us << " us; type=" << result.frame.type() << " size="
            << result.frame.cols << 'x' << result.frame.rows;
        std::cout << "M7Slice3E3 tracked RAW neural start_us=" << start_us
                  << " events=" << events.size() << " range=[" << stats->min
                  << ',' << stats->max << "] elapsed_ms=" << result.elapsed_ms << '\n';
    }
}

// --- 4.4.3 FlowStatistics ---
TEST(FlowStatisticsTest, Construction) {
    FlowStatistics fs;
    EXPECT_EQ(fs.output_hz(), 5);
}
TEST(FlowStatisticsTest, Params) {
    FlowStatistics fs;
    fs.set_output_hz(10);
    EXPECT_EQ(fs.output_hz(), 10);
    fs.set_source(FlowStatistics::Source::Annotated);
    EXPECT_EQ(fs.source(), FlowStatistics::Source::Annotated);
}
TEST(FlowStatisticsTest, AddSamples) {
    FlowStatistics fs;
    std::vector<gui_algo::FlowSample> samples;
    samples.push_back(gui_algo::FlowSample{1.0f, 0.0f, 1.0f, 0.0f}); // perfect match
    samples.push_back(gui_algo::FlowSample{2.0f, 0.0f, 1.0f, 0.0f}); // off by 1
    fs.add_samples(samples.data(), samples.size());
    EXPECT_GT(fs.epe_mean(), 0.0);
    EXPECT_GE(fs.epe_median(), 0.0);
}

// --- 4.4.4 ISIAnalyzer ---
TEST(ISIAnalyzerTest, Construction) {
    ISIAnalyzer a(64, 48);
    EXPECT_EQ(a.bin_count(), 32);  // default bin count
}
TEST(ISIAnalyzerTest, Params) {
    ISIAnalyzer a(32, 32);
    a.set_bin_count(64);
    EXPECT_EQ(a.bin_count(), 64);
    a.set_min_isi_ms(5.0f);
    EXPECT_FLOAT_EQ(a.min_isi_ms(), 5.0f);
}
TEST(ISIAnalyzerTest, Process) {
    ISIAnalyzer a(32, 32);
    auto ev = make_events(32, 32, 50);
    a.process(ev.data(), ev.size());
    SUCCEED();
}
// Regression: set_bin_count / set_max_isi_ms must preserve the histogram
// range in µs (previous bug divided by 1000, causing all samples to be
// dropped and counts() to be all-zero).
TEST(ISIAnalyzerTest, SetterPreservesRange) {
    ISIAnalyzer a(32, 32, 32, 100.0f);  // max_isi = 100 ms = 100000 us
    a.set_bin_count(64);
    // Feed two events 50000 us apart (< 100000 us, must land in a bin).
    gui_algo::Event ev[2] = {{16, 16, 1, 0}, {16, 16, 1, 50000}};
    a.process(ev, 2);
    const auto& counts = a.counts();
    std::uint64_t total = 0;
    for (auto c : counts) total += c;
    EXPECT_GT(total, 0u);  // at least one ISI sample must be counted
}

// --- 4.4.5 ParticleCounter ---
TEST(ParticleCounterTest, Construction) {
    ParticleCounter c(64, 48);
    EXPECT_EQ(c.width(), 64);
    EXPECT_EQ(c.height(), 48);
}
TEST(ParticleCounterTest, Params) {
    ParticleCounter c(32, 32);
    c.set_min_particle_size_px(10);
    EXPECT_EQ(c.min_particle_size_px(), 10);
    c.set_max_particle_size_px(200);
    EXPECT_EQ(c.max_particle_size_px(), 200);
}
TEST(ParticleCounterTest, InitialCount) {
    ParticleCounter c(32, 32);
    EXPECT_EQ(c.cumulative_count(), 0u);
}

// --- 4.4.6 AutoBiasController ---
TEST(AutoBiasControllerTest, Construction) {
    AutoBiasController c;
    EXPECT_FLOAT_EQ(c.target_event_rate_mev(), 5.0f);
    EXPECT_FLOAT_EQ(c.kp(), 0.5f);
}
TEST(AutoBiasControllerTest, Params) {
    AutoBiasController c;
    c.set_target_event_rate_mev(10.0f);
    EXPECT_FLOAT_EQ(c.target_event_rate_mev(), 10.0f);
    c.set_gains(0.3f, 0.02f, 0.01f);
    EXPECT_FLOAT_EQ(c.kp(), 0.3f);
    EXPECT_FLOAT_EQ(c.ki(), 0.02f);
    EXPECT_FLOAT_EQ(c.kd(), 0.01f);
}
TEST(AutoBiasControllerTest, Update) {
    AutoBiasController c(5.0f);
    auto cmd = c.update(3.0, 1000000); // measured 3 Mev/s, target 5
    EXPECT_NE(cmd.delta_diff, 0.0f); // should have some correction
}
TEST(AutoBiasControllerTest, Reset) {
    AutoBiasController c(5.0f);
    c.update(3.0, 1000000);
    c.reset();
    EXPECT_DOUBLE_EQ(c.integral(), 0.0);
}

// --- 4.4.7 FreqDetector ---
TEST(FreqDetectorTest, Construction) {
    FreqDetector d(64, 48);
    EXPECT_EQ(d.width(), 64);
    EXPECT_EQ(d.height(), 48);
}
TEST(FreqDetectorTest, Params) {
    FreqDetector d(32, 32);
    d.set_f_min(200.0f);
    EXPECT_FLOAT_EQ(d.f_min(), 200.0f);
    d.set_f_max(5000.0f);
    EXPECT_FLOAT_EQ(d.f_max(), 5000.0f);
    d.set_heatmap_threshold(30);
    EXPECT_EQ(d.heatmap_threshold(), 30);
}
TEST(FreqDetectorTest, AnalyzeEmpty) {
    FreqDetector d(32, 32);
    auto sources = d.analyze();
    EXPECT_TRUE(sources.empty());
}
TEST(FreqDetectorTest, ProcessAndAnalyze) {
    FreqDetector d(32, 32);
    auto ev = make_events(32, 32, 100);
    d.process(ev.data(), ev.size());
    auto sources = d.analyze();
    // May or may not find light sources.
    SUCCEED();
}

// --- 4.4.8 SensorSelfTest ---

TEST(SensorSelfTestTest, Construction) {
    SensorSelfTest s(64, 48);
    EXPECT_EQ(s.width(), 64);
    EXPECT_EQ(s.height(), 48);
}

TEST(SensorSelfTestTest, NoEventsAllBadPixels) {
    // With no events fed, every pixel is a suspected bad pixel.
    SensorSelfTest s(8, 4);
    auto stats = s.compute_stats();
    EXPECT_EQ(stats.total_pixels, 32u);
    EXPECT_EQ(stats.triggered_pixels, 0u);
    EXPECT_EQ(stats.measured_pixels, 0u);
    EXPECT_EQ(stats.bad_pixels, 32u);
    auto coords = s.bad_pixel_coords();
    EXPECT_EQ(coords.size(), 32u);
}

TEST(SensorSelfTestTest, SingleEventNoInterval) {
    // A pixel with only one event has no interval (measured_pixels == 0).
    SensorSelfTest s(4, 4);
    Event ev[1] = {{2, 2, 1, 1000}};
    s.process(ev, 1);
    auto stats = s.compute_stats();
    EXPECT_EQ(stats.triggered_pixels, 1u);
    EXPECT_EQ(stats.measured_pixels, 0u);
    EXPECT_EQ(stats.bad_pixels, 15u);
}

TEST(SensorSelfTestTest, MinIntervalTracked) {
    // Feed three events at the same pixel with intervals 500us and 200us.
    // The per-pixel min interval should be 200us (the shorter of the two).
    // Stats operate on per-pixel minimums, so min=max=mean=200 for one pixel.
    SensorSelfTest s(4, 4);
    Event ev[3] = {{2, 2, 1, 1000}, {2, 2, 1, 1500}, {2, 2, 1, 1700}};
    s.process(ev, 3);
    auto stats = s.compute_stats();
    EXPECT_EQ(stats.triggered_pixels, 1u);
    EXPECT_EQ(stats.measured_pixels, 1u);
    EXPECT_EQ(stats.min_us, 200);
    EXPECT_EQ(stats.max_us, 200);
    EXPECT_EQ(stats.mean_us, 200.0);
}

TEST(SensorSelfTestTest, MinIntervalUpdatedOnShorter) {
    // First interval = 1000us, then 500us → min should be 500us.
    SensorSelfTest s(4, 4);
    Event ev1[2] = {{0, 0, 1, 0}, {0, 0, 1, 1000}};
    s.process(ev1, 2);
    Event ev2[2] = {{0, 0, 1, 2000}, {0, 0, 1, 2500}};
    s.process(ev2, 2);
    auto stats = s.compute_stats();
    EXPECT_EQ(stats.min_us, 500);
    EXPECT_EQ(stats.measured_pixels, 1u);
}

TEST(SensorSelfTestTest, OutOfBoundsEventsIgnored) {
    SensorSelfTest s(4, 4);
    Event ev[2] = {{10, 10, 1, 100}, {3, 3, 1, 200}};
    s.process(ev, 2);
    auto stats = s.compute_stats();
    EXPECT_EQ(stats.triggered_pixels, 1u);  // only (3,3) is in bounds
}

TEST(SensorSelfTestTest, ResetClearsState) {
    SensorSelfTest s(4, 4);
    Event ev[2] = {{0, 0, 1, 0}, {0, 0, 1, 500}};
    s.process(ev, 2);
    EXPECT_EQ(s.compute_stats().triggered_pixels, 1u);
    s.reset();
    auto stats = s.compute_stats();
    EXPECT_EQ(stats.triggered_pixels, 0u);
    EXPECT_EQ(stats.bad_pixels, 16u);
}

TEST(SensorSelfTestTest, RenderProducesCorrectSize) {
    SensorSelfTest s(16, 8);
    cv::Mat img = s.render();
    ASSERT_FALSE(img.empty());
    EXPECT_EQ(img.cols, 16);
    EXPECT_EQ(img.rows, 8);
    EXPECT_EQ(img.type(), CV_8UC3);
}

TEST(SensorSelfTestTest, RenderBadPixelIsRed) {
    // With no events, all pixels should be red (BGR 0,0,255).
    SensorSelfTest s(4, 2);
    cv::Mat img = s.render();
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 4; ++x) {
            const auto& px = img.at<cv::Vec3b>(y, x);
            EXPECT_EQ(px[0], 0);    // B
            EXPECT_EQ(px[1], 0);    // G
            EXPECT_EQ(px[2], 255);  // R
        }
    }
}

TEST(SensorSelfTestTest, RenderTriggeredPixelIsGrayscale) {
    // A pixel with two events (interval=1us) should render bright (non-red,
    // non-black). Bad pixels remain red.
    SensorSelfTest s(4, 4);
    Event ev[2] = {{0, 0, 1, 100}, {0, 0, 1, 101}};
    s.process(ev, 2);
    cv::Mat img = s.render();
    const auto& triggered = img.at<cv::Vec3b>(0, 0);
    // Grayscale: R == G == B, and bright (interval=1us → ~255).
    EXPECT_EQ(triggered[0], triggered[1]);
    EXPECT_EQ(triggered[1], triggered[2]);
    EXPECT_GT(triggered[0], 200);
    // An untriggered pixel is still red.
    const auto& bad = img.at<cv::Vec3b>(1, 1);
    EXPECT_EQ(bad[2], 255);
    EXPECT_EQ(bad[0], 0);
}

TEST(SensorSelfTestTest, ReportNotEmpty) {
    SensorSelfTest s(4, 4);
    Event ev[3] = {{0, 0, 1, 0}, {0, 0, 1, 100}, {0, 0, 1, 150}};
    s.process(ev, 3);
    const std::string r = s.report();
    EXPECT_FALSE(r.empty());
    EXPECT_NE(r.find("Sensor Self-Test Report"), std::string::npos);
    EXPECT_NE(r.find("bad"), std::string::npos);
}

TEST(SensorSelfTestTest, MultiplePixelsStats) {
    // Two pixels: one with min interval 100us, one with 200us.
    SensorSelfTest s(4, 4);
    Event ev[4] = {{0, 0, 1, 0}, {0, 0, 1, 100},
                   {1, 1, 1, 0}, {1, 1, 1, 200}};
    s.process(ev, 4);
    auto stats = s.compute_stats();
    EXPECT_EQ(stats.measured_pixels, 2u);
    EXPECT_EQ(stats.min_us, 100);
    EXPECT_EQ(stats.max_us, 200);
    EXPECT_EQ(stats.mean_us, 150.0);
    // Sorted intervals: [100, 200]. median = intervals[1] = 200.
    EXPECT_EQ(stats.median_us, 200.0);
    EXPECT_EQ(stats.bad_pixels, 14u);  // 16 - 2 triggered
}
