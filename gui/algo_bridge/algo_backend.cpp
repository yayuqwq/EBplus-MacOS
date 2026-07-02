// gui/algo_bridge/algo_backend.cpp — concrete backends wrapping real algo/ classes.
//
// 每个 backend 包装一个 algo/cv 或 algo/analytics 类：
//   - push_events: 零拷贝 reinterpret_cast EventCD→Event，调用 process()/filter()
//   - pull_result: 收集过滤事件 + 叠加层 + 帧
//   - set_param/get_param: 字符串↔类型化 setter 转换
//
// 分组模式：
//   A) In-place filter:  noise_filter, hot_pixel_filter, optical_gyro, perspective_undistort
//   B) Overlay detector: object_tracker, corner_detector, blob_detector, sparse_optical_flow
//   C) Result-vector:    hough_line, hough_circle, line_segment, orientation_cluster, cluster_lif
//   D) Frame producer:   time_surface, event_to_video, flow_statistics, isi_analyzer
//   E) Analyzer:         freq_detector, active_marker, particle_counter, auto_bias
//   F) Event-vector:     trigger_synced, ultra_slow_motion
//   G) Visualization:    xyt_visualizer, overlay
//   H) Misc filter:      orientation_filter, direction_selective, background_mask, bandpass

#include "algo_backend.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <string>
#include <utility>
#include <vector>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include "algo/common/event.h"
#include "algo/common/event_packet.h"

// algo/cv
#include "algo/cv/noise_filter.h"
#include "algo/cv/hot_pixel_filter.h"
#include "algo/cv/orientation_filter.h"
#include "algo/cv/direction_selective_filter.h"
#include "algo/cv/sparse_optical_flow.h"
#include "algo/cv/blob_detector.h"
#include "algo/cv/object_tracker.h"
#include "algo/cv/corner_detector.h"
#include "algo/cv/line_segment_detector.h"
#include "algo/cv/hough_line_tracker.h"
#include "algo/cv/hough_circle_tracker.h"
#include "algo/cv/orientation_cluster.h"
#include "algo/cv/cluster_lif.h"
#include "algo/cv/background_mask_filter.h"
#include "algo/cv/perspective_undistort.h"
#include "algo/cv/trigger_synced_filter.h"
#include "algo/cv/bandpass_filter.h"
#include "algo/cv/optical_gyro.h"
#include "algo/cv/ultra_slow_motion.h"
#include "algo/cv/xyt_visualizer.h"
#include "algo/cv/time_surface.h"
// NOTE: algo/cv/overlay.h intentionally NOT included — it redefines
// FlowVector/TrackedObject/Corner already defined in the per-algo headers
// above (sparse_optical_flow.h, object_tracker.h, corner_detector.h).
// OverlayBackend below is a pure pass-through; overlay drawing is handled
// by gui/display/frame_annotator from AlgoResult overlay vectors.

// algo/analytics
#include "algo/analytics/active_marker.h"
#include "algo/analytics/event_to_video.h"
#include "algo/analytics/flow_statistics.h"
#include "algo/analytics/isi_analyzer.h"
#include "algo/analytics/particle_counter.h"
#include "algo/analytics/auto_bias_controller.h"
#include "algo/analytics/freq_detector.h"

namespace gui {

namespace {

// ---- string ↔ value helpers -----------------------------------------------

int to_i(const std::string& s, int def = 0) {
    try { return std::stoi(s); } catch (...) { return def; }
}
double to_d(const std::string& s, double def = 0.0) {
    try { return std::stod(s); } catch (...) { return def; }
}
bool to_b(const std::string& s) {
    return s == "1" || s == "true" || s == "True" || s == "on" || s == "yes";
}
std::string from_i(int v) { return std::to_string(v); }
std::string from_d(double v) { return std::to_string(v); }
std::string from_b(bool v) { return v ? "true" : "false"; }

/// Zero-copy view of EventCD buffer as gui_algo::Event (layout-compatible).
const gui_algo::Event* as_events(const Metavision::EventCD* p) {
    static_assert(sizeof(gui_algo::Event) == sizeof(Metavision::EventCD),
                  "layout mismatch");
    return reinterpret_cast<const gui_algo::Event*>(p);
}

/// @brief Processing region (ROI) for complex algorithms.
/// Design §4.4.2 / §4.3.25-27: complex algorithms default to the center
/// 128×128 region to bound computational cost. The user can adjust the
/// region or disable the ROI via the Algorithms panel.
struct ProcessRegion {
    bool enabled{true};
    int x{-1};   ///< -1 = auto-center on sensor
    int y{-1};
    int w{128};  ///< 0 = full sensor width
    int h{128};  ///< 0 = full sensor height

    // Computed bounds (valid after compute() is called)
    int x0{0}, y0{0}, x1{0}, y1{0};  ///< [x0,x1) × [y0,y1)
    int rw{0}, rh{0};                  ///< ROI width / height

    void compute(int sensor_w, int sensor_h) {
        rw = (w <= 0) ? sensor_w : std::min(w, sensor_w);
        rh = (h <= 0) ? sensor_h : std::min(h, sensor_h);
        int rx = (x < 0) ? (sensor_w - rw) / 2
                          : std::min(std::max(0, x), sensor_w - rw);
        int ry = (y < 0) ? (sensor_h - rh) / 2
                          : std::min(std::max(0, y), sensor_h - rh);
        x0 = rx; y0 = ry;
        x1 = std::min(rx + rw, sensor_w);
        y1 = std::min(ry + rh, sensor_h);
        rw = x1 - x0;
        rh = y1 - y0;
    }

    bool contains(int ex, int ey) const {
        return ex >= x0 && ex < x1 && ey >= y0 && ey < y1;
    }
};

/// @brief Filters @p src events to @p roi and subtracts the ROI origin so the
/// output events are in ROI-relative coordinates. Used by frame-producer
/// backends (EventToVideo / TimeSurface / ISIAnalyzer) whose algo instances
/// are created at ROI dimensions.
std::vector<gui_algo::Event> crop_to_roi(const gui_algo::Event* src,
                                          std::size_t n,
                                          const ProcessRegion& roi) {
    std::vector<gui_algo::Event> out;
    out.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        if (roi.contains(src[i].x, src[i].y)) {
            gui_algo::Event e = src[i];
            e.x -= roi.x0;
            e.y -= roi.y0;
            out.push_back(e);
        }
    }
    return out;
}

/// @brief ROI helper for backends that keep sensor-scale coordinates (overlay
/// detectors, in-place filters, analyzers, event-vector backends).
///
/// Events outside the ROI are dropped; coordinates of kept events are
/// unchanged (sensor scale). This lets overlay primitives (boxes, points,
/// lines, circles) render at the correct position on the main display frame.
///
/// Usage: add as a member, call init(w,h) in the constructor, forward ROI
/// params in set_param(), call apply() in push_events() to get a
/// (pointer, count) pair for the filtered event range.
struct RoiFilter {
    int sensor_w{0};
    int sensor_h{0};
    ProcessRegion region;

    void init(int w, int h) {
        sensor_w = w;
        sensor_h = h;
        region.compute(w, h);
    }

    /// Handles the 5 ROI params. Returns true if @p k was a ROI param.
    bool set_param(const std::string& k, const std::string& v) {
        if (k == "roi_enabled") { region.enabled = to_b(v); region.compute(sensor_w, sensor_h); return true; }
        if (k == "roi_x") { region.x = to_i(v); region.compute(sensor_w, sensor_h); return true; }
        if (k == "roi_y") { region.y = to_i(v); region.compute(sensor_w, sensor_h); return true; }
        if (k == "roi_w") { region.w = to_i(v); region.compute(sensor_w, sensor_h); return true; }
        if (k == "roi_h") { region.h = to_i(v); region.compute(sensor_w, sensor_h); return true; }
        return false;
    }

    std::string get_param(const std::string& k) const {
        if (k == "roi_enabled") return from_b(region.enabled);
        return {};
    }

    /// @brief Filters events to ROI, keeping sensor coordinates.
    /// @return (pointer, count). If ROI is disabled, returns the original
    ///         (src, n); otherwise fills @p buf with the filtered events.
    std::pair<const gui_algo::Event*, std::size_t> apply(
        const gui_algo::Event* src, std::size_t n,
        std::vector<gui_algo::Event>& buf) const {
        if (!region.enabled || region.rw <= 0 || region.rh <= 0) {
            return {src, n};
        }
        buf.clear();
        buf.reserve(n);
        for (std::size_t i = 0; i < n; ++i) {
            if (region.contains(src[i].x, src[i].y)) {
                buf.push_back(src[i]);
            }
        }
        return {buf.data(), buf.size()};
    }
};

} // namespace

// ===========================================================================
// Group A: In-place event filters (compact / modify events)
// ===========================================================================

/// NoiseFilter backend — 8-mode denoiser, compacts events in place.
class NoiseFilterBackend final : public AlgoBackend {
    gui_algo::NoiseFilter algo_;
    std::vector<Metavision::EventCD> buf_;
    RoiFilter roi_;
    std::size_t last_kept_{0};
    double last_rate_{0.0};
public:
    NoiseFilterBackend(int w, int h)
        : algo_(w, h, gui_algo::NoiseFilter::Mode::STCF) {
        roi_.init(w, h);
    }
    void set_param(const std::string& k, const std::string& v) override {
        if (roi_.set_param(k, v)) return;
        if (k == "mode") {
            int m = to_i(v);
            if (m >= 0 && m <= 7) algo_.set_mode(static_cast<gui_algo::NoiseFilter::Mode>(m));
        } else if (k == "correlation_time_s") algo_.set_correlation_time_s(to_d(v));
        else if (k == "min_neighbors") algo_.set_min_neighbors(to_i(v));
        else if (k == "baf_dt_us") algo_.set_baf_dt_us(to_i(v));
        else if (k == "refractory_us") algo_.set_refractory_period_us(to_i(v));
        else if (k == "filter_hot_pixels") algo_.set_filter_hot_pixels(to_b(v));
        else if (k == "adaptive_correlation_time") algo_.set_adaptive_correlation_time(to_b(v));
        else if (k == "line_freq_hz") algo_.set_line_freq(to_i(v) == 60 ? gui_algo::NoiseFilter::LineFreq::Hz60 : gui_algo::NoiseFilter::LineFreq::Hz50);
    }
    std::string get_param(const std::string& k) const override {
        auto r = roi_.get_param(k); if (!r.empty()) return r;
        if (k == "correlation_time_s") return from_d(algo_.correlation_time_s());
        if (k == "min_neighbors") return from_i(algo_.min_neighbors());
        if (k == "filter_hot_pixels") return from_b(algo_.filter_hot_pixels());
        return {};
    }
    void push_events(const Metavision::EventCD* b, const Metavision::EventCD* e) override {
        buf_.assign(b, e);
        auto* ev = const_cast<gui_algo::Event*>(as_events(buf_.data()));
        std::size_t n = buf_.size();
        // Compact in place to ROI events (design §5.6.6).
        if (roi_.region.enabled && roi_.region.rw > 0 && roi_.region.rh > 0) {
            std::size_t kept = 0;
            for (std::size_t i = 0; i < n; ++i) {
                if (roi_.region.contains(ev[i].x, ev[i].y)) ev[kept++] = ev[i];
            }
            n = kept;
        }
        buf_.resize(n);
        last_kept_ = algo_.filter(ev, n);
        last_rate_ = algo_.filter_rate();
    }
    AlgoResult pull_result() override {
        AlgoResult r;
        r.filtered_events.assign(buf_.data(), buf_.data() + last_kept_);
        r.status = "noise_filter: kept " + std::to_string(last_kept_) + "/" +
                   std::to_string(buf_.size()) + " (" + std::to_string(last_rate_ * 100) + "%)" +
                   std::string(roi_.region.enabled ? " (ROI)" : "");
        return r;
    }
    void reset() override { algo_.reset(); buf_.clear(); last_kept_ = 0; }
};

/// HotPixelFilter backend — learns hot-pixel mask + compacts events.
class HotPixelFilterBackend final : public AlgoBackend {
    gui_algo::HotPixelFilter algo_;
    std::vector<Metavision::EventCD> buf_;
    RoiFilter roi_;
    std::size_t last_kept_{0};
public:
    HotPixelFilterBackend(int w, int h) : algo_(w, h) { roi_.init(w, h); }
    void set_param(const std::string& k, const std::string& v) override {
        if (roi_.set_param(k, v)) return;
        if (k == "learning_window_s") algo_.set_learning_window_s(to_d(v));
        else if (k == "n_sigma") algo_.set_n_sigma(to_d(v));
        else if (k == "enable_fpn_correction") algo_.set_enable_fpn_correction(to_b(v));
        else if (k == "fpn_target_rate_hz") algo_.set_fpn_target_rate_hz(to_d(v));
    }
    std::string get_param(const std::string& k) const override {
        auto r = roi_.get_param(k); if (!r.empty()) return r;
        if (k == "n_sigma") return from_d(algo_.n_sigma());
        if (k == "enable_fpn_correction") return from_b(algo_.enable_fpn_correction());
        return {};
    }
    void push_events(const Metavision::EventCD* b, const Metavision::EventCD* e) override {
        buf_.assign(b, e);
        auto* ev = const_cast<gui_algo::Event*>(as_events(buf_.data()));
        std::size_t n = buf_.size();
        if (roi_.region.enabled && roi_.region.rw > 0 && roi_.region.rh > 0) {
            std::size_t kept = 0;
            for (std::size_t i = 0; i < n; ++i) {
                if (roi_.region.contains(ev[i].x, ev[i].y)) ev[kept++] = ev[i];
            }
            n = kept;
        }
        buf_.resize(n);
        algo_.learn(as_events(buf_.data()), n);
        last_kept_ = algo_.process(ev, n);
    }
    AlgoResult pull_result() override {
        AlgoResult r;
        r.filtered_events.assign(buf_.data(), buf_.data() + last_kept_);
        r.status = "hot_pixel: " + std::to_string(algo_.hot_pixel_count()) + " hot px, kept " +
                   std::to_string(last_kept_) + std::string(roi_.region.enabled ? " (ROI)" : "");
        return r;
    }
    void reset() override { algo_.reset(); buf_.clear(); last_kept_ = 0; }
};

/// OpticalGyro (EIS) backend — modifies event coordinates in place.
class OpticalGyroBackend final : public AlgoBackend {
    gui_algo::OpticalGyro algo_;
    std::vector<Metavision::EventCD> buf_;
    RoiFilter roi_;
public:
    OpticalGyroBackend(int w, int h) : algo_(w, h) { roi_.init(w, h); }
    void set_param(const std::string& k, const std::string& v) override {
        if (roi_.set_param(k, v)) return;
        if (k == "stabilize") algo_.set_stabilization_strength(to_b(v) ? 1.0F : 0.0F);
        else if (k == "smoothing_window_ms") algo_.set_smoothing_window_ms(static_cast<float>(to_d(v)));
    }
    std::string get_param(const std::string& k) const override {
        auto r = roi_.get_param(k); if (!r.empty()) return r;
        if (k == "stabilize") return from_b(algo_.stabilization_strength() > 0.0F);
        if (k == "smoothing_window_ms") return from_d(algo_.smoothing_window_ms());
        return {};
    }
    void push_events(const Metavision::EventCD* b, const Metavision::EventCD* e) override {
        buf_.assign(b, e);
        auto* ev = const_cast<gui_algo::Event*>(as_events(buf_.data()));
        std::size_t n = buf_.size();
        if (roi_.region.enabled && roi_.region.rw > 0 && roi_.region.rh > 0) {
            std::size_t kept = 0;
            for (std::size_t i = 0; i < n; ++i) {
                if (roi_.region.contains(ev[i].x, ev[i].y)) ev[kept++] = ev[i];
            }
            n = kept;
        }
        buf_.resize(n);
        gui_algo::MutableEventPacket pkt(ev, n);
        algo_.process(pkt);
    }
    AlgoResult pull_result() override {
        AlgoResult r;
        r.filtered_events = buf_;
        const auto m = algo_.smoothed_motion();
        r.status = "EIS: shift=(" + std::to_string(m.dx) + "," +
                   std::to_string(m.dy) + ")" +
                   std::string(roi_.region.enabled ? " (ROI)" : "");
        return r;
    }
    void reset() override { algo_.reset(); buf_.clear(); }
};

/// PerspectiveUndistort backend — remaps event coordinates in place.
class PerspectiveUndistortBackend final : public AlgoBackend {
    gui_algo::PerspectiveUndistort algo_;
    std::vector<Metavision::EventCD> buf_;
    RoiFilter roi_;
public:
    PerspectiveUndistortBackend(int w, int h) : algo_(w, h) { roi_.init(w, h); }
    void set_param(const std::string& k, const std::string& v) override {
        if (roi_.set_param(k, v)) return;
        if (k == "enable") algo_.set_undistort(to_b(v));
    }
    std::string get_param(const std::string& k) const override {
        auto r = roi_.get_param(k); if (!r.empty()) return r;
        if (k == "enable") return from_b(algo_.undistort());
        return {};
    }
    void push_events(const Metavision::EventCD* b, const Metavision::EventCD* e) override {
        buf_.assign(b, e);
        auto* ev = const_cast<gui_algo::Event*>(as_events(buf_.data()));
        std::size_t n = buf_.size();
        if (roi_.region.enabled && roi_.region.rw > 0 && roi_.region.rh > 0) {
            std::size_t kept = 0;
            for (std::size_t i = 0; i < n; ++i) {
                if (roi_.region.contains(ev[i].x, ev[i].y)) ev[kept++] = ev[i];
            }
            n = kept;
        }
        buf_.resize(n);
        gui_algo::MutableEventPacket pkt(ev, n);
        algo_.process(pkt);
    }
    AlgoResult pull_result() override {
        AlgoResult r;
        r.filtered_events = buf_;
        r.status = std::string("undistort: ") + (algo_.undistort() ? "on" : "off") +
                   std::string(roi_.region.enabled ? " (ROI)" : "");
        return r;
    }
    void reset() override { algo_.reset(); buf_.clear(); }
};

// ===========================================================================
// Group B: Overlay detectors (process events, produce overlay data)
// ===========================================================================

/// ObjectTracker backend — tracked objects as overlay boxes.
class ObjectTrackerBackend final : public AlgoBackend {
    gui_algo::ObjectTracker algo_;
    std::vector<Metavision::EventCD> passthrough_;
    RoiFilter roi_;
    std::vector<gui_algo::Event> roi_buf_;
public:
    ObjectTrackerBackend(int w, int h)
        : algo_(w, h, gui_algo::ObjectTracker::Mode::RCT) { roi_.init(w, h); }
    void set_param(const std::string& k, const std::string& v) override {
        if (roi_.set_param(k, v)) return;
        if (k == "mode") {
            int m = to_i(v);
            if (m >= 0 && m <= 3) algo_.set_mode(static_cast<gui_algo::ObjectTracker::Mode>(m));
        } else if (k == "cluster_size_px") algo_.set_cluster_size_px(to_i(v));
        else if (k == "cluster_time_us") algo_.set_cluster_time_us(to_i(v));
        else if (k == "min_cluster_events") algo_.set_min_cluster_events(to_i(v));
        else if (k == "max_lost_age_s") algo_.set_max_lost_age_s(to_d(v));
        else if (k == "enable_velocity_prediction") algo_.set_enable_velocity_prediction(to_b(v));
    }
    std::string get_param(const std::string& k) const override {
        auto r = roi_.get_param(k); if (!r.empty()) return r;
        if (k == "cluster_size_px") return from_i(algo_.cluster_size_px());
        return {};
    }
    void push_events(const Metavision::EventCD* b, const Metavision::EventCD* e) override {
        passthrough_.assign(b, e);
        auto [ev, n] = roi_.apply(as_events(passthrough_.data()), passthrough_.size(), roi_buf_);
        algo_.process(ev, n);
    }
    AlgoResult pull_result() override {
        AlgoResult r;
        r.filtered_events = passthrough_;
        const auto& objs = algo_.objects();
        for (const auto& o : objs) {
            if (!o.visible) continue;
            OverlayBox box;
            box.x = o.bbox.x; box.y = o.bbox.y;
            box.w = o.bbox.width; box.h = o.bbox.height;
            box.id = o.id;
            r.boxes.push_back(box);
            OverlayText t;
            t.x = o.bbox.x; t.y = o.bbox.y > 12 ? o.bbox.y - 12 : 0;
            t.text = "#" + std::to_string(o.id) + " v=(" +
                     std::to_string(static_cast<int>(o.vx)) + "," +
                     std::to_string(static_cast<int>(o.vy)) + ")";
            r.texts.push_back(t);
        }
        r.status = "tracker: " + std::to_string(objs.size()) + " objects" +
                   std::string(roi_.region.enabled ? " (ROI)" : "");
        return r;
    }
    void reset() override { algo_.reset(); passthrough_.clear(); }
};

/// CornerDetector backend — corners as overlay points.
class CornerDetectorBackend final : public AlgoBackend {
    gui_algo::CornerDetector algo_;
    std::vector<Metavision::EventCD> passthrough_;
    RoiFilter roi_;
    std::vector<gui_algo::Event> roi_buf_;
public:
    CornerDetectorBackend(int w, int h) : algo_(w, h) { roi_.init(w, h); }
    void set_param(const std::string& k, const std::string& v) override {
        if (roi_.set_param(k, v)) return;
        if (k == "mode") {
            int m = to_i(v);
            if (m >= 0 && m <= 2) algo_.set_mode(static_cast<gui_algo::CornerDetector::Mode>(m));
        } else if (k == "min_score") algo_.set_threshold(to_d(v));
    }
    std::string get_param(const std::string& k) const override {
        auto r = roi_.get_param(k); if (!r.empty()) return r;
        return {};
    }
    void push_events(const Metavision::EventCD* b, const Metavision::EventCD* e) override {
        passthrough_.assign(b, e);
        auto [ev, n] = roi_.apply(as_events(passthrough_.data()), passthrough_.size(), roi_buf_);
        algo_.process(ev, n);
    }
    AlgoResult pull_result() override {
        AlgoResult r;
        r.filtered_events = passthrough_;
        for (const auto& c : algo_.corners()) {
            OverlayPoint p;
            p.x = static_cast<int>(c.x);
            p.y = static_cast<int>(c.y);
            p.strength = c.strength;
            r.points.push_back(p);
        }
        r.status = "corners: " + std::to_string(algo_.corners().size()) +
                   std::string(roi_.region.enabled ? " (ROI)" : "");
        return r;
    }
    void reset() override { algo_.reset(); passthrough_.clear(); }
};

/// BlobDetector backend — blobs as overlay boxes.
class BlobDetectorBackend final : public AlgoBackend {
    gui_algo::BlobDetector algo_;
    std::vector<Metavision::EventCD> passthrough_;
    RoiFilter roi_;
    std::vector<gui_algo::Event> roi_buf_;
public:
    BlobDetectorBackend(int w, int h) : algo_(w, h) { roi_.init(w, h); }
    void set_param(const std::string& k, const std::string& v) override {
        if (roi_.set_param(k, v)) return;
        if (k == "threshold") algo_.set_threshold(static_cast<int>(to_d(v)));
        else if (k == "learning_rate") algo_.set_learning_rate(to_d(v));
    }
    std::string get_param(const std::string& k) const override {
        auto r = roi_.get_param(k); if (!r.empty()) return r;
        return {};
    }
    void push_events(const Metavision::EventCD* b, const Metavision::EventCD* e) override {
        passthrough_.assign(b, e);
        auto [ev, n] = roi_.apply(as_events(passthrough_.data()), passthrough_.size(), roi_buf_);
        algo_.process(ev, n);
    }
    AlgoResult pull_result() override {
        AlgoResult r;
        r.filtered_events = passthrough_;
        for (const auto& blob : algo_.blobs()) {
            OverlayBox box;
            box.x = blob.bbox.x; box.y = blob.bbox.y;
            box.w = blob.bbox.width; box.h = blob.bbox.height;
            r.boxes.push_back(box);
        }
        r.status = "blobs: " + std::to_string(algo_.blobs().size()) +
                   std::string(roi_.region.enabled ? " (ROI)" : "");
        return r;
    }
    void reset() override { algo_.reset(); passthrough_.clear(); }
};

/// SparseOpticalFlow backend — flow vectors as overlay lines.
class SparseOpticalFlowBackend final : public AlgoBackend {
    gui_algo::SparseOpticalFlow algo_;
    std::vector<Metavision::EventCD> passthrough_;
    std::vector<gui_algo::FlowVector> flows_;
    RoiFilter roi_;
    std::vector<gui_algo::Event> roi_buf_;
public:
    SparseOpticalFlowBackend(int w, int h)
        : algo_(w, h, gui_algo::SparseOpticalFlow::Mode::LocalPlanes) { roi_.init(w, h); }
    void set_param(const std::string& k, const std::string& v) override {
        if (roi_.set_param(k, v)) return;
        if (k == "mode") {
            int m = to_i(v);
            if (m >= 0 && m <= 3) algo_.set_mode(static_cast<gui_algo::SparseOpticalFlow::Mode>(m));
        } else if (k == "search_radius") algo_.set_search_radius_px(to_i(v));
    }
    std::string get_param(const std::string& k) const override {
        auto r = roi_.get_param(k); if (!r.empty()) return r;
        return {};
    }
    void push_events(const Metavision::EventCD* b, const Metavision::EventCD* e) override {
        passthrough_.assign(b, e);
        flows_.clear();
        auto [ev, n] = roi_.apply(as_events(passthrough_.data()), passthrough_.size(), roi_buf_);
        algo_.process(ev, n, flows_);
    }
    AlgoResult pull_result() override {
        AlgoResult r;
        r.filtered_events = passthrough_;
        // Optical-flow visualization: render each flow vector as a single
        // colored pixel/square at its origin. Color encodes direction (HSV
        // hue 0..360° mapped to the (vx,vy) angle) and brightness encodes
        // magnitude. This is the standard dense-flow visualization style
        // (Farneback/OpenCV hue-intensity scheme) and avoids the previous
        // "long lines all over the screen" problem caused by drawing
        // vx*10/vy*10 line segments.
        r.colored_points.reserve(flows_.size());
        for (const auto& f : flows_) {
            OverlayColoredPoint cp;
            cp.x = f.x;
            cp.y = f.y;
            // Direction -> hue (atan2 returns -pi..pi, map to 0..1).
            const float angle_rad = std::atan2(f.vy, f.vx);
            float hue = (angle_rad + static_cast<float>(M_PI)) / (2.0f * static_cast<float>(M_PI));
            if (hue < 0.0f) hue = 0.0f;
            if (hue > 1.0f) hue = 1.0f;
            // Magnitude -> value (normalize against a typical px/s reference).
            const float mag = std::sqrt(f.vx * f.vx + f.vy * f.vy);
            const float val = std::min(1.0f, mag / 1000.0f);  // 1000 px/s -> full brightness
            hsv_to_rgb(hue, 1.0f, val, cp.r, cp.g, cp.b);
            r.colored_points.push_back(cp);
        }
        r.status = "flow: " + std::to_string(flows_.size()) + " vectors" +
                   std::string(roi_.region.enabled ? " (ROI)" : "");
        return r;
    }
    void reset() override { algo_.reset(); passthrough_.clear(); flows_.clear(); }
private:
    /// HSV (h,s,v in [0,1]) -> RGB (r,g,b in [0,255]).
    static void hsv_to_rgb(float h, float s, float v,
                           std::uint8_t& r, std::uint8_t& g, std::uint8_t& b) {
        const float c = v * s;
        const float hp = h * 6.0f;
        const float x = c * (1.0f - std::fabs(std::fmod(hp, 2.0f) - 1.0f));
        float rf = 0.0f, gf = 0.0f, bf = 0.0f;
        if (hp < 1.0f)      { rf = c; gf = x; bf = 0.0f; }
        else if (hp < 2.0f) { rf = x; gf = c; bf = 0.0f; }
        else if (hp < 3.0f) { rf = 0.0f; gf = c; bf = x; }
        else if (hp < 4.0f) { rf = 0.0f; gf = x; bf = c; }
        else if (hp < 5.0f) { rf = x; gf = 0.0f; bf = c; }
        else                { rf = c; gf = 0.0f; bf = x; }
        const float m = v - c;
        r = static_cast<std::uint8_t>((rf + m) * 255.0f + 0.5f);
        g = static_cast<std::uint8_t>((gf + m) * 255.0f + 0.5f);
        b = static_cast<std::uint8_t>((bf + m) * 255.0f + 0.5f);
    }
};

// ===========================================================================
// Group C: Result-vector detectors (process returns vector<Result>)
// ===========================================================================

/// HoughLineTracker backend — detected lines as overlay lines.
///
/// Complex algorithm (design §4.3.14): the Hough accumulator is built at ROI
/// dimensions (not sensor dimensions) to bound memory and per-frame scan cost.
/// Events are cropped to ROI-relative coordinates; detected line endpoints are
/// shifted back by the ROI origin so they render at the correct sensor position.
class HoughLineBackend final : public AlgoBackend {
    int sensor_w_{0}, sensor_h_{0};
    ProcessRegion roi_;
    int num_theta_bins_{90};
    int num_rho_bins_{0};
    int threshold_{50};
    Metavision::timestamp decay_us_{100000};
    std::unique_ptr<gui_algo::HoughLineTracker> algo_;
    std::vector<Metavision::EventCD> passthrough_;
    std::vector<gui_algo::Event> roi_events_;
    std::vector<gui_algo::HoughLine> last_;
public:
    HoughLineBackend(int w, int h) : sensor_w_(w), sensor_h_(h) {
        roi_.compute(sensor_w_, sensor_h_);
        rebuild();
    }
    void rebuild() {
        const int aw = roi_.enabled ? roi_.rw : sensor_w_;
        const int ah = roi_.enabled ? roi_.rh : sensor_h_;
        algo_ = std::make_unique<gui_algo::HoughLineTracker>(
            aw, ah, num_theta_bins_, num_rho_bins_, threshold_, decay_us_);
    }
    void set_param(const std::string& k, const std::string& v) override {
        bool need_rebuild = false;
        if (k == "threshold") { threshold_ = to_i(v); if (algo_) algo_->set_threshold(threshold_); }
        else if (k == "num_theta_bins") { num_theta_bins_ = to_i(v); need_rebuild = true; }
        else if (k == "num_rho_bins") { num_rho_bins_ = to_i(v); need_rebuild = true; }
        else if (k == "accumulator_decay_us") {
            decay_us_ = static_cast<Metavision::timestamp>(to_i(v));
            if (algo_) algo_->set_accumulator_decay_us(decay_us_);
        }
        else if (k == "roi_enabled") { roi_.enabled = to_b(v); need_rebuild = true; }
        else if (k == "roi_x") { roi_.x = to_i(v); need_rebuild = true; }
        else if (k == "roi_y") { roi_.y = to_i(v); need_rebuild = true; }
        else if (k == "roi_w") { roi_.w = to_i(v); need_rebuild = true; }
        else if (k == "roi_h") { roi_.h = to_i(v); need_rebuild = true; }
        if (need_rebuild) { roi_.compute(sensor_w_, sensor_h_); rebuild(); }
    }
    std::string get_param(const std::string& k) const override {
        if (k == "roi_enabled") return from_b(roi_.enabled);
        return {};
    }
    void push_events(const Metavision::EventCD* b, const Metavision::EventCD* e) override {
        passthrough_.assign(b, e);
        if (roi_.enabled && roi_.rw > 0 && roi_.rh > 0) {
            roi_events_ = crop_to_roi(as_events(passthrough_.data()),
                                       passthrough_.size(), roi_);
            gui_algo::EventPacket pkt(roi_events_.data(), roi_events_.size());
            last_ = algo_->process(pkt);
        } else {
            gui_algo::EventPacket pkt(as_events(passthrough_.data()),
                                      passthrough_.size());
            last_ = algo_->process(pkt);
        }
    }
    AlgoResult pull_result() override {
        AlgoResult r;
        r.filtered_events = passthrough_;
        // Shift ROI-relative endpoints back to sensor coordinates.
        const int dx = (roi_.enabled && roi_.rw > 0) ? roi_.x0 : 0;
        const int dy = (roi_.enabled && roi_.rh > 0) ? roi_.y0 : 0;
        for (const auto& hl : last_) {
            OverlayLine l;
            l.x1 = static_cast<int>(hl.start.x) + dx;
            l.y1 = static_cast<int>(hl.start.y) + dy;
            l.x2 = static_cast<int>(hl.end.x) + dx;
            l.y2 = static_cast<int>(hl.end.y) + dy;
            r.lines.push_back(l);
        }
        r.status = "hough_line: " + std::to_string(last_.size()) + " lines" +
                   std::string(roi_.enabled ? " (ROI)" : " (full)");
        return r;
    }
    void reset() override {
        if (algo_) algo_->reset();
        passthrough_.clear(); roi_events_.clear(); last_.clear();
    }
};

/// HoughCircleTracker backend — detected circles as overlay circles.
///
/// Complex algorithm (design §4.3.15): the 3D accumulator (a, b, r) is built
/// at ROI dimensions, NOT sensor dimensions. At sensor scale (e.g. 1280×720)
/// with max_radius=50 the accumulator would be ~42M cells (168 MB) and
/// find_peaks would scan every cell per frame — a memory/CPU blowup that
/// freezes/crashes the GUI. Using the ROI (default 128×128) bounds this to
/// ~750K cells. Events are cropped to ROI-relative coordinates; detected
/// circle centers are shifted back by the ROI origin for overlay rendering.
class HoughCircleBackend final : public AlgoBackend {
    int sensor_w_{0}, sensor_h_{0};
    ProcessRegion roi_;
    int min_radius_{8};
    int max_radius_{30};
    int threshold_{50};
    Metavision::timestamp decay_us_{100000};
    std::unique_ptr<gui_algo::HoughCircleTracker> algo_;
    std::vector<Metavision::EventCD> passthrough_;
    std::vector<gui_algo::Event> roi_events_;
    std::vector<gui_algo::HoughCircle> last_;
    /// Throttle: only run find_peaks every kMinProcessIntervalUs to avoid
    /// CPU saturation. Between runs the last cached result is returned.
    /// 50ms = 20Hz update, smooth enough for visual tracking.
    static constexpr Metavision::timestamp kMinProcessIntervalUs = 50000;
    Metavision::timestamp last_process_t_{0};
public:
    HoughCircleBackend(int w, int h) : sensor_w_(w), sensor_h_(h) {
        roi_.compute(sensor_w_, sensor_h_);
        rebuild();
    }
    void rebuild() {
        const int aw = roi_.enabled ? roi_.rw : sensor_w_;
        const int ah = roi_.enabled ? roi_.rh : sensor_h_;
        algo_ = std::make_unique<gui_algo::HoughCircleTracker>(
            aw, ah, min_radius_, max_radius_, threshold_, decay_us_);
    }
    void set_param(const std::string& k, const std::string& v) override {
        bool need_rebuild = false;
        if (k == "min_radius") { min_radius_ = to_i(v); need_rebuild = true; }
        else if (k == "max_radius") { max_radius_ = to_i(v); need_rebuild = true; }
        else if (k == "threshold") { threshold_ = to_i(v); if (algo_) algo_->set_threshold(threshold_); }
        else if (k == "accumulator_decay_us") {
            decay_us_ = static_cast<Metavision::timestamp>(to_i(v));
            if (algo_) algo_->set_accumulator_decay_us(decay_us_);
        }
        else if (k == "roi_enabled") { roi_.enabled = to_b(v); need_rebuild = true; }
        else if (k == "roi_x") { roi_.x = to_i(v); need_rebuild = true; }
        else if (k == "roi_y") { roi_.y = to_i(v); need_rebuild = true; }
        else if (k == "roi_w") { roi_.w = to_i(v); need_rebuild = true; }
        else if (k == "roi_h") { roi_.h = to_i(v); need_rebuild = true; }
        if (need_rebuild) { roi_.compute(sensor_w_, sensor_h_); rebuild(); }
    }
    std::string get_param(const std::string& k) const override {
        if (k == "roi_enabled") return from_b(roi_.enabled);
        return {};
    }
    void push_events(const Metavision::EventCD* b, const Metavision::EventCD* e) override {
        passthrough_.assign(b, e);
        // Throttle: skip the expensive process() call if not enough time has
        // elapsed since the last run. The accumulator still receives events
        // (accumulate is cheap), but find_peaks (the O(W×H×R) scan) only
        // runs at ~20Hz. This eliminates the "满屏幕卡顿" lag.
        const Metavision::timestamp cur_t =
            passthrough_.empty() ? last_process_t_ : passthrough_.back().t;
        if (last_process_t_ > 0 && cur_t - last_process_t_ < kMinProcessIntervalUs) {
            return;  // keep last_ cached result
        }
        last_process_t_ = cur_t;
        if (roi_.enabled && roi_.rw > 0 && roi_.rh > 0) {
            roi_events_ = crop_to_roi(as_events(passthrough_.data()),
                                       passthrough_.size(), roi_);
            gui_algo::EventPacket pkt(roi_events_.data(), roi_events_.size());
            last_ = algo_->process(pkt);
        } else {
            gui_algo::EventPacket pkt(as_events(passthrough_.data()),
                                      passthrough_.size());
            last_ = algo_->process(pkt);
        }
    }
    AlgoResult pull_result() override {
        AlgoResult r;
        r.filtered_events = passthrough_;
        // Shift ROI-relative centers back to sensor coordinates.
        const int dx = (roi_.enabled && roi_.rw > 0) ? roi_.x0 : 0;
        const int dy = (roi_.enabled && roi_.rh > 0) ? roi_.y0 : 0;
        for (const auto& c : last_) {
            OverlayCircle oc;
            oc.cx = static_cast<int>(c.center.x) + dx;
            oc.cy = static_cast<int>(c.center.y) + dy;
            oc.r = static_cast<int>(c.radius);
            r.circles.push_back(oc);
        }
        r.status = "hough_circle: " + std::to_string(last_.size()) + " circles" +
                   std::string(roi_.enabled ? " (ROI)" : " (full)");
        return r;
    }
    void reset() override {
        if (algo_) algo_->reset();
        passthrough_.clear(); roi_events_.clear(); last_.clear();
    }
};

/// LineSegmentDetector (ELiSeD) backend — detected segments as overlay lines.
class LineSegmentBackend final : public AlgoBackend {
    gui_algo::LineSegmentDetector algo_;
    std::vector<Metavision::EventCD> passthrough_;
    std::vector<gui_algo::LineSegment> last_;
    RoiFilter roi_;
    std::vector<gui_algo::Event> roi_buf_;
public:
    LineSegmentBackend(int w, int h) : algo_(w, h) { roi_.init(w, h); }
    void set_param(const std::string& k, const std::string& v) override {
        if (roi_.set_param(k, v)) return;
        if (k == "min_length") algo_.set_min_line_length_px(to_i(v));
        else if (k == "gap") algo_.set_max_line_gap_px(to_i(v));
    }
    std::string get_param(const std::string& k) const override {
        auto r = roi_.get_param(k); if (!r.empty()) return r;
        return {};
    }
    void push_events(const Metavision::EventCD* b, const Metavision::EventCD* e) override {
        passthrough_.assign(b, e);
        auto [ev, n] = roi_.apply(as_events(passthrough_.data()), passthrough_.size(), roi_buf_);
        gui_algo::EventPacket pkt(ev, n);
        last_ = algo_.process(pkt);
    }
    AlgoResult pull_result() override {
        AlgoResult r;
        r.filtered_events = passthrough_;
        for (const auto& seg : last_) {
            OverlayLine l;
            l.x1 = static_cast<int>(seg.start.x); l.y1 = static_cast<int>(seg.start.y);
            l.x2 = static_cast<int>(seg.end.x); l.y2 = static_cast<int>(seg.end.y);
            r.lines.push_back(l);
        }
        r.status = "elised: " + std::to_string(last_.size()) + " segments" +
                   std::string(roi_.region.enabled ? " (ROI)" : "");
        return r;
    }
    void reset() override { algo_.reset(); passthrough_.clear(); last_.clear(); }
};

/// OrientationCluster backend — detected orientation clusters as overlay lines.
class OrientationClusterBackend final : public AlgoBackend {
    gui_algo::OrientationCluster algo_;
    std::vector<Metavision::EventCD> passthrough_;
    std::vector<gui_algo::OrientationClusterResult> last_;
    RoiFilter roi_;
    std::vector<gui_algo::Event> roi_buf_;
public:
    OrientationClusterBackend(int w, int h) : algo_(w, h) { roi_.init(w, h); }
    void set_param(const std::string& k, const std::string& v) override {
        if (roi_.set_param(k, v)) return;
        if (k == "min_events") algo_.set_min_cluster_size(to_i(v));
    }
    std::string get_param(const std::string& k) const override {
        auto r = roi_.get_param(k); if (!r.empty()) return r;
        return {};
    }
    void push_events(const Metavision::EventCD* b, const Metavision::EventCD* e) override {
        passthrough_.assign(b, e);
        auto [ev, n] = roi_.apply(as_events(passthrough_.data()), passthrough_.size(), roi_buf_);
        gui_algo::EventPacket pkt(ev, n);
        last_ = algo_.process(pkt);
    }
    AlgoResult pull_result() override {
        AlgoResult r;
        r.filtered_events = passthrough_;
        for (const auto& c : last_) {
            OverlayLine l;
            l.x1 = static_cast<int>(c.centroid.x); l.y1 = static_cast<int>(c.centroid.y);
            l.x2 = static_cast<int>(c.centroid.x) + static_cast<int>(std::cos(c.orientation) * c.size);
            l.y2 = static_cast<int>(c.centroid.y) + static_cast<int>(std::sin(c.orientation) * c.size);
            r.lines.push_back(l);
        }
        r.status = "orient_cluster: " + std::to_string(last_.size()) + " clusters" +
                   std::string(roi_.region.enabled ? " (ROI)" : "");
        return r;
    }
    void reset() override { algo_.reset(); passthrough_.clear(); last_.clear(); }
};

/// ClusterLIF backend — LIF clusters as overlay boxes.
class ClusterLifBackend final : public AlgoBackend {
    gui_algo::ClusterLIF algo_;
    std::vector<Metavision::EventCD> passthrough_;
    std::vector<gui_algo::LifCluster> last_;
    RoiFilter roi_;
    std::vector<gui_algo::Event> roi_buf_;
public:
    ClusterLifBackend(int w, int h) : algo_(w, h) { roi_.init(w, h); }
    void set_param(const std::string& k, const std::string& v) override {
        if (roi_.set_param(k, v)) return;
        if (k == "tau_ms") algo_.set_tau_ms(static_cast<float>(to_d(v)));
        else if (k == "threshold") algo_.set_threshold(static_cast<float>(to_d(v)));
    }
    std::string get_param(const std::string& k) const override {
        auto r = roi_.get_param(k); if (!r.empty()) return r;
        return {};
    }
    void push_events(const Metavision::EventCD* b, const Metavision::EventCD* e) override {
        passthrough_.assign(b, e);
        auto [ev, n] = roi_.apply(as_events(passthrough_.data()), passthrough_.size(), roi_buf_);
        gui_algo::EventPacket pkt(ev, n);
        last_ = algo_.process(pkt);
    }
    AlgoResult pull_result() override {
        AlgoResult r;
        r.filtered_events = passthrough_;
        for (const auto& c : last_) {
            OverlayBox box;
            box.x = static_cast<int>(c.position.x) - 5;
            box.y = static_cast<int>(c.position.y) - 5;
            box.w = 10; box.h = 10;
            r.boxes.push_back(box);
        }
        r.status = "lif: " + std::to_string(last_.size()) + " clusters" +
                   std::string(roi_.region.enabled ? " (ROI)" : "");
        return r;
    }
    void reset() override { algo_.reset(); passthrough_.clear(); last_.clear(); }
};

// ===========================================================================
// Group D: Frame producers (process events, produce cv::Mat frame)
// ===========================================================================

/// TimeSurface backend — produces a time-decay pseudo-color frame.
/// Complex algorithm (design §4.3.27): defaults to the center 128×128 ROI.
class TimeSurfaceBackend final : public AlgoBackend {
    int sensor_w_{0}, sensor_h_{0};
    ProcessRegion roi_;
    int decay_time_us_{100000};
    gui_algo::TimeSurface::Palette palette_{gui_algo::TimeSurface::Palette::Hot};
    gui_algo::TimeSurface::Channels channels_{gui_algo::TimeSurface::Channels::Merged};
    std::unique_ptr<gui_algo::TimeSurface> algo_;
    std::vector<Metavision::EventCD> passthrough_;
    std::vector<gui_algo::Event> roi_events_;
public:
    TimeSurfaceBackend(int w, int h) : sensor_w_(w), sensor_h_(h) {
        roi_.compute(sensor_w_, sensor_h_);
        rebuild();
    }
    void rebuild() {
        const int aw = roi_.enabled ? roi_.rw : sensor_w_;
        const int ah = roi_.enabled ? roi_.rh : sensor_h_;
        algo_ = std::make_unique<gui_algo::TimeSurface>(
            aw, ah, channels_, decay_time_us_, palette_, 30);
    }
    void set_param(const std::string& k, const std::string& v) override {
        bool need_rebuild = false;
        if (k == "decay_time_us") {
            decay_time_us_ = to_i(v);
            if (algo_) algo_->set_decay_time_us(decay_time_us_);
        } else if (k == "palette") {
            int p = to_i(v);
            if (p >= 0 && p <= 3) {
                palette_ = static_cast<gui_algo::TimeSurface::Palette>(p);
                if (algo_) algo_->set_palette(palette_);
            }
        } else if (k == "channels") {
            int c = to_i(v);
            channels_ = (c == 2) ? gui_algo::TimeSurface::Channels::Split
                                  : gui_algo::TimeSurface::Channels::Merged;
            if (algo_) algo_->set_channels(channels_);
        } else if (k == "roi_enabled") { roi_.enabled = to_b(v); need_rebuild = true; }
        else if (k == "roi_x") { roi_.x = to_i(v); need_rebuild = true; }
        else if (k == "roi_y") { roi_.y = to_i(v); need_rebuild = true; }
        else if (k == "roi_w") { roi_.w = to_i(v); need_rebuild = true; }
        else if (k == "roi_h") { roi_.h = to_i(v); need_rebuild = true; }
        if (need_rebuild) { roi_.compute(sensor_w_, sensor_h_); rebuild(); }
    }
    std::string get_param(const std::string& k) const override {
        if (k == "roi_enabled") return from_b(roi_.enabled);
        return {};
    }
    void push_events(const Metavision::EventCD* b, const Metavision::EventCD* e) override {
        passthrough_.assign(b, e);
        if (roi_.enabled && roi_.rw > 0 && roi_.rh > 0) {
            roi_events_ = crop_to_roi(as_events(passthrough_.data()),
                                       passthrough_.size(), roi_);
            algo_->process(roi_events_.data(), roi_events_.size());
        } else {
            algo_->process(as_events(passthrough_.data()), passthrough_.size());
        }
    }
    AlgoResult pull_result() override {
        AlgoResult r;
        r.filtered_events = passthrough_;
        r.has_frame = true;
        r.frame = algo_->render().clone();
        r.status = "time_surface: " + std::to_string(algo_->width()) + "x" +
                   std::to_string(algo_->height()) +
                   (roi_.enabled ? " (ROI)" : " (full)");
        return r;
    }
    void reset() override {
        if (algo_) algo_->reset();
        passthrough_.clear();
        roi_events_.clear();
    }
};

/// EventToVideo backend — produces reconstructed intensity frame.
/// Complex algorithm (design §4.4.2): defaults to the center 128×128 ROI to
/// bound computational cost. The ROI, output fps, window and theta params
/// are exposed in the Algorithms panel and this backend.
class EventToVideoBackend final : public AlgoBackend {
    int sensor_w_{0}, sensor_h_{0};
    ProcessRegion roi_;
    // Current param values (re-applied after ROI rebuild).
    gui_algo::EventToVideo::Mode mode_{gui_algo::EventToVideo::Mode::BardowVariational};
    int output_fps_{30};
    float window_ms_{15.0F};
    float delta_t_ms_{15.0F};
    float theta_{0.22F};
    int num_iterations_{30};
    std::unique_ptr<gui_algo::EventToVideo> algo_;
    std::vector<Metavision::EventCD> passthrough_;
    std::vector<gui_algo::Event> roi_events_;
public:
    EventToVideoBackend(int w, int h) : sensor_w_(w), sensor_h_(h) {
        roi_.compute(sensor_w_, sensor_h_);
        rebuild();
    }
    void rebuild() {
        const int aw = roi_.enabled ? roi_.rw : sensor_w_;
        const int ah = roi_.enabled ? roi_.rh : sensor_h_;
        algo_ = std::make_unique<gui_algo::EventToVideo>(aw, ah, mode_, output_fps_);
        algo_->set_window_ms(window_ms_);
        algo_->set_delta_t_ms(delta_t_ms_);
        algo_->set_theta(theta_);
        algo_->set_num_iterations(num_iterations_);
    }
    void set_param(const std::string& k, const std::string& v) override {
        bool need_rebuild = false;
        if (k == "mode") {
            int m = to_i(v);
            if (m >= 0 && m <= 2) {
                mode_ = static_cast<gui_algo::EventToVideo::Mode>(m);
                if (algo_) algo_->set_mode(mode_);
            }
        } else if (k == "output_fps") {
            output_fps_ = to_i(v);
            if (algo_) algo_->set_output_fps(output_fps_);
        } else if (k == "window_ms") {
            window_ms_ = static_cast<float>(to_d(v));
            if (algo_) algo_->set_window_ms(window_ms_);
        } else if (k == "delta_t_ms") {
            delta_t_ms_ = static_cast<float>(to_d(v));
            if (algo_) algo_->set_delta_t_ms(delta_t_ms_);
        } else if (k == "theta") {
            theta_ = static_cast<float>(to_d(v));
            if (algo_) algo_->set_theta(theta_);
        } else if (k == "num_iterations") {
            num_iterations_ = to_i(v);
            if (algo_) algo_->set_num_iterations(num_iterations_);
        } else if (k == "roi_enabled") { roi_.enabled = to_b(v); need_rebuild = true; }
        else if (k == "roi_x") { roi_.x = to_i(v); need_rebuild = true; }
        else if (k == "roi_y") { roi_.y = to_i(v); need_rebuild = true; }
        else if (k == "roi_w") { roi_.w = to_i(v); need_rebuild = true; }
        else if (k == "roi_h") { roi_.h = to_i(v); need_rebuild = true; }
        if (need_rebuild) { roi_.compute(sensor_w_, sensor_h_); rebuild(); }
    }
    std::string get_param(const std::string& k) const override {
        if (k == "output_fps") return from_i(output_fps_);
        if (k == "roi_enabled") return from_b(roi_.enabled);
        return {};
    }
    void push_events(const Metavision::EventCD* b, const Metavision::EventCD* e) override {
        passthrough_.assign(b, e);
        if (roi_.enabled && roi_.rw > 0 && roi_.rh > 0) {
            roi_events_ = crop_to_roi(as_events(passthrough_.data()),
                                       passthrough_.size(), roi_);
            algo_->process(roi_events_.data(), roi_events_.size());
        } else {
            algo_->process(as_events(passthrough_.data()), passthrough_.size());
        }
    }
    AlgoResult pull_result() override {
        AlgoResult r;
        r.filtered_events = passthrough_;
        r.has_frame = true;
        r.frame = algo_->get_frame().clone();
        r.status = "e2v: " + std::to_string(algo_->width()) + "x" +
                   std::to_string(algo_->height()) +
                   (roi_.enabled ? " (ROI)" : " (full)");
        return r;
    }
    void reset() override {
        if (algo_) algo_->reset();
        passthrough_.clear();
        roi_events_.clear();
    }
};

/// FlowStatistics backend — requires ground-truth flow samples (not available
/// in real-time). Counts events and reports a status; no frame is produced.
/// Supports ROI (design §5.6.6): when enabled, only ROI events are counted.
class FlowStatisticsBackend final : public AlgoBackend {
    gui_algo::FlowStatistics algo_;
    std::vector<Metavision::EventCD> passthrough_;
    RoiFilter roi_;
    std::vector<gui_algo::Event> roi_buf_;
    std::size_t total_events_{0};
public:
    FlowStatisticsBackend(int w, int h)
        : algo_(gui_algo::FlowStatistics::Source::Synthetic, 5) {
        roi_.init(w, h);
    }
    void set_param(const std::string& k, const std::string& v) override {
        roi_.set_param(k, v);
    }
    std::string get_param(const std::string& k) const override {
        auto r = roi_.get_param(k); if (!r.empty()) return r;
        return {};
    }
    void push_events(const Metavision::EventCD* b, const Metavision::EventCD* e) override {
        passthrough_.assign(b, e);
        auto [ev, n] = roi_.apply(as_events(passthrough_.data()),
                                   passthrough_.size(), roi_buf_);
        total_events_ += n;
    }
    AlgoResult pull_result() override {
        AlgoResult r;
        r.filtered_events = passthrough_;
        r.status = "flow_stats: " + std::to_string(total_events_) +
                   " events (no GT — Passive mode)" +
                   std::string(roi_.region.enabled ? " (ROI)" : "");
        return r;
    }
    void reset() override { algo_.reset(); passthrough_.clear(); roi_buf_.clear(); total_events_ = 0; }
};

/// ISIAnalyzer backend — renders ISI histogram as frame.
/// Complex algorithm (design §4.4.4): defaults to the center 128×128 ROI.
class ISIAnalyzerBackend final : public AlgoBackend {
    int sensor_w_{0}, sensor_h_{0};
    ProcessRegion roi_;
    bool per_pixel_{false};
    float max_isi_ms_{100.0F};
    std::unique_ptr<gui_algo::ISIAnalyzer> algo_;
    std::vector<Metavision::EventCD> passthrough_;
    std::vector<gui_algo::Event> roi_events_;
public:
    ISIAnalyzerBackend(int w, int h) : sensor_w_(w), sensor_h_(h) {
        roi_.compute(sensor_w_, sensor_h_);
        rebuild();
    }
    void rebuild() {
        const int aw = roi_.enabled ? roi_.rw : sensor_w_;
        const int ah = roi_.enabled ? roi_.rh : sensor_h_;
        algo_ = std::make_unique<gui_algo::ISIAnalyzer>(aw, ah, 32, max_isi_ms_, per_pixel_);
    }
    void set_param(const std::string& k, const std::string& v) override {
        bool need_rebuild = false;
        if (k == "per_pixel") {
            per_pixel_ = to_b(v);
            need_rebuild = true;
        } else if (k == "max_isi_ms") {
            max_isi_ms_ = static_cast<float>(to_d(v));
            if (algo_) algo_->set_max_isi_ms(max_isi_ms_);
        } else if (k == "roi_enabled") { roi_.enabled = to_b(v); need_rebuild = true; }
        else if (k == "roi_x") { roi_.x = to_i(v); need_rebuild = true; }
        else if (k == "roi_y") { roi_.y = to_i(v); need_rebuild = true; }
        else if (k == "roi_w") { roi_.w = to_i(v); need_rebuild = true; }
        else if (k == "roi_h") { roi_.h = to_i(v); need_rebuild = true; }
        if (need_rebuild) { roi_.compute(sensor_w_, sensor_h_); rebuild(); }
    }
    std::string get_param(const std::string& k) const override {
        if (k == "roi_enabled") return from_b(roi_.enabled);
        return {};
    }
    void push_events(const Metavision::EventCD* b, const Metavision::EventCD* e) override {
        passthrough_.assign(b, e);
        if (roi_.enabled && roi_.rw > 0 && roi_.rh > 0) {
            roi_events_ = crop_to_roi(as_events(passthrough_.data()),
                                       passthrough_.size(), roi_);
            algo_->process(roi_events_.data(), roi_events_.size());
        } else {
            algo_->process(as_events(passthrough_.data()), passthrough_.size());
        }
    }
    AlgoResult pull_result() override {
        AlgoResult r;
        r.filtered_events = passthrough_;
        r.has_frame = true;
        r.frame = algo_->render();
        r.status = "isi: histogram" + std::string(roi_.enabled ? " (ROI)" : " (full)");
        return r;
    }
    void reset() override {
        if (algo_) algo_->reset();
        passthrough_.clear();
        roi_events_.clear();
    }
};

// ===========================================================================
// Group E: Analyzers (process events, produce text/point overlay)
// ===========================================================================

/// FreqDetector backend — detected light sources as overlay circles + text.
/// Complex algorithm (design §4.4.7): defaults to the center 128×128 ROI.
/// Events outside the ROI are dropped before reaching the analyzer; the algo
/// keeps sensor-sized internal buffers so output coordinates remain at sensor
/// scale (correct for overlay rendering).
class FreqDetectorBackend final : public AlgoBackend {
    int sensor_w_{0}, sensor_h_{0};
    ProcessRegion roi_;
    gui_algo::FreqDetector algo_;
    std::vector<Metavision::EventCD> passthrough_;
    std::vector<gui_algo::Event> roi_events_;
    std::vector<gui_algo::LightSource> last_;
public:
    FreqDetectorBackend(int w, int h) : sensor_w_(w), sensor_h_(h), algo_(w, h) {
        roi_.compute(sensor_w_, sensor_h_);
    }
    void set_param(const std::string& k, const std::string& v) override {
        if (k == "update_interval_s") algo_.set_update_interval_s(static_cast<float>(to_d(v)));
        else if (k == "min_events") algo_.set_min_cc_area(to_i(v));
        else if (k == "roi_enabled") { roi_.enabled = to_b(v); roi_.compute(sensor_w_, sensor_h_); }
        else if (k == "roi_x") { roi_.x = to_i(v); roi_.compute(sensor_w_, sensor_h_); }
        else if (k == "roi_y") { roi_.y = to_i(v); roi_.compute(sensor_w_, sensor_h_); }
        else if (k == "roi_w") { roi_.w = to_i(v); roi_.compute(sensor_w_, sensor_h_); }
        else if (k == "roi_h") { roi_.h = to_i(v); roi_.compute(sensor_w_, sensor_h_); }
    }
    std::string get_param(const std::string& k) const override {
        if (k == "roi_enabled") return from_b(roi_.enabled);
        return {};
    }
    void push_events(const Metavision::EventCD* b, const Metavision::EventCD* e) override {
        passthrough_.assign(b, e);
        if (roi_.enabled && roi_.rw > 0 && roi_.rh > 0) {
            // Filter events to ROI (no coordinate adjustment — sensor-scale
            // buffers + overlay coordinates are preserved).
            roi_events_.clear();
            const auto* ev = as_events(passthrough_.data());
            for (std::size_t i = 0; i < passthrough_.size(); ++i) {
                if (roi_.contains(ev[i].x, ev[i].y)) roi_events_.push_back(ev[i]);
            }
            algo_.process(roi_events_.data(), roi_events_.size());
        } else {
            algo_.process(as_events(passthrough_.data()), passthrough_.size());
        }
        if (algo_.should_analyze()) last_ = algo_.analyze();
    }
    AlgoResult pull_result() override {
        AlgoResult r;
        r.filtered_events = passthrough_;
        for (const auto& src : last_) {
            OverlayCircle c;
            c.cx = src.u; c.cy = src.v; c.r = 8;
            r.circles.push_back(c);
            OverlayText t;
            t.x = src.u + 12; t.y = src.v;
            t.text = std::to_string(static_cast<int>(src.blink_freq_hz)) + "Hz";
            r.texts.push_back(t);
        }
        r.status = "freq: " + std::to_string(last_.size()) + " sources" +
                   std::string(roi_.enabled ? " (ROI)" : " (full)");
        return r;
    }
    void reset() override { algo_.reset(); passthrough_.clear(); roi_events_.clear(); last_.clear(); }
};

/// ActiveMarker backend — detected markers as overlay circles + text.
/// Supports ROI (design §5.6.6): processes only ROI events; overlay coords
/// remain at sensor scale. Throttled: analyze() only runs every 50ms to
/// avoid CPU saturation (the per-event process() is cheap, but the
/// cluster-detection sweep in analyze() is expensive).
class ActiveMarkerBackend final : public AlgoBackend {
    gui_algo::ActiveMarker algo_;
    std::vector<Metavision::EventCD> passthrough_;
    RoiFilter roi_;
    std::vector<gui_algo::Event> roi_buf_;
    std::vector<gui_algo::ClusterAnnotation> last_;
    static constexpr Metavision::timestamp kMinAnalyzeIntervalUs = 50000;
    Metavision::timestamp last_analyze_t_{0};
public:
    ActiveMarkerBackend(int w, int h) : algo_(w, h) { roi_.init(w, h); }
    void set_param(const std::string& k, const std::string& v) override {
        if (roi_.set_param(k, v)) return;
        if (k == "window_us") algo_.set_window_ms(static_cast<float>(to_i(v)) / 1000.0F);
        else if (k == "min_events") algo_.set_min_cluster_area(to_i(v));
    }
    std::string get_param(const std::string& k) const override {
        auto r = roi_.get_param(k); if (!r.empty()) return r;
        return {};
    }
    void push_events(const Metavision::EventCD* b, const Metavision::EventCD* e) override {
        passthrough_.assign(b, e);
        auto [ev, n] = roi_.apply(as_events(passthrough_.data()),
                                   passthrough_.size(), roi_buf_);
        algo_.process(ev, n);
        // Throttle the expensive cluster-detection sweep to ~20Hz.
        const Metavision::timestamp cur_t =
            passthrough_.empty() ? last_analyze_t_ : passthrough_.back().t;
        if (last_analyze_t_ > 0 && cur_t - last_analyze_t_ < kMinAnalyzeIntervalUs) {
            return;  // keep last_ cached result
        }
        last_analyze_t_ = cur_t;
        last_ = algo_.analyze();
    }
    AlgoResult pull_result() override {
        AlgoResult r;
        r.filtered_events = passthrough_;
        for (const auto& m : last_) {
            OverlayCircle c;
            c.cx = m.cx; c.cy = m.cy; c.r = 6;
            r.circles.push_back(c);
            OverlayText t;
            t.x = m.cx + 10; t.y = m.cy;
            t.text = std::to_string(static_cast<int>(m.frequency_hz)) + "Hz";
            r.texts.push_back(t);
        }
        r.status = "marker: " + std::to_string(last_.size()) + " markers" +
                   std::string(roi_.region.enabled ? " (ROI)" : "");
        return r;
    }
    void reset() override { algo_.reset(); passthrough_.clear(); roi_buf_.clear(); last_.clear(); }
};

/// ParticleCounter backend — count as overlay text.
/// Supports ROI (design §5.6.6): processes only ROI events. Throttled:
/// detect_and_track() only runs every 50ms to avoid CPU saturation.
class ParticleCounterBackend final : public AlgoBackend {
    gui_algo::ParticleCounter algo_;
    std::vector<Metavision::EventCD> passthrough_;
    RoiFilter roi_;
    std::vector<gui_algo::Event> roi_buf_;
    Metavision::timestamp last_t_{0};
    static constexpr Metavision::timestamp kMinDetectIntervalUs = 50000;
    Metavision::timestamp last_detect_t_{0};
public:
    ParticleCounterBackend(int w, int h) : algo_(w, h) { roi_.init(w, h); }
    void set_param(const std::string& k, const std::string& v) override {
        if (roi_.set_param(k, v)) return;
        if (k == "line_y") algo_.set_counting_line_y(to_i(v));
        else if (k == "min_area") algo_.set_min_particle_size_px(to_i(v));
    }
    std::string get_param(const std::string& k) const override {
        auto r = roi_.get_param(k); if (!r.empty()) return r;
        return {};
    }
    void push_events(const Metavision::EventCD* b, const Metavision::EventCD* e) override {
        passthrough_.assign(b, e);
        if (!passthrough_.empty()) last_t_ = passthrough_.back().t;
        auto [ev, n] = roi_.apply(as_events(passthrough_.data()),
                                   passthrough_.size(), roi_buf_);
        algo_.process(ev, n, last_t_);
        // Throttle the expensive detect_and_track sweep to ~20Hz.
        if (last_detect_t_ > 0 && last_t_ - last_detect_t_ < kMinDetectIntervalUs) {
            return;
        }
        last_detect_t_ = last_t_;
        algo_.detect_and_track();
    }
    AlgoResult pull_result() override {
        AlgoResult r;
        r.filtered_events = passthrough_;
        OverlayText t;
        t.x = 10; t.y = 20;
        t.text = "count: " + std::to_string(algo_.cumulative_count());
        r.texts.push_back(t);
        r.status = "counter: " + std::to_string(algo_.cumulative_count()) +
                   std::string(roi_.region.enabled ? " (ROI)" : "");
        return r;
    }
    void reset() override { algo_.reset(); passthrough_.clear(); roi_buf_.clear(); last_t_ = 0; }
};

/// AutoBiasController backend — bias command as overlay text.
/// Supports ROI (design §5.6.6): rate computed from ROI events only.
class AutoBiasBackend final : public AlgoBackend {
    gui_algo::AutoBiasController algo_;
    std::vector<Metavision::EventCD> passthrough_;
    RoiFilter roi_;
    std::vector<gui_algo::Event> roi_buf_;
    Metavision::timestamp last_t_{0};
    gui_algo::BiasCommand last_cmd_;
public:
    AutoBiasBackend(int w, int h) : algo_(5.0F, 0.5F, 0.01F, 0.0F, 10.0F) { roi_.init(w, h); }
    void set_param(const std::string& k, const std::string& v) override {
        if (roi_.set_param(k, v)) return;
        if (k == "target_event_rate_mev") algo_.set_target_event_rate_mev(to_d(v));
    }
    std::string get_param(const std::string& k) const override {
        auto r = roi_.get_param(k); if (!r.empty()) return r;
        if (k == "target_event_rate_mev") return from_d(algo_.target_event_rate_mev());
        return {};
    }
    void push_events(const Metavision::EventCD* b, const Metavision::EventCD* e) override {
        passthrough_.assign(b, e);
        auto [ev, n] = roi_.apply(as_events(passthrough_.data()),
                                   passthrough_.size(), roi_buf_);
        Metavision::timestamp cur_t = passthrough_.empty() ? last_t_ : passthrough_.back().t;
        const auto dt = cur_t - last_t_;
        if (dt > 0) {
            const double rate_mev = static_cast<double>(n) / (static_cast<double>(dt) * 1e-6) / 1e6;
            last_cmd_ = algo_.update(rate_mev, dt);
        }
        last_t_ = cur_t;
    }
    AlgoResult pull_result() override {
        AlgoResult r;
        r.filtered_events = passthrough_;
        OverlayText t;
        t.x = 10; t.y = 40;
        t.text = "bias: d_diff=" + std::to_string(last_cmd_.delta_diff) +
                 " d_on=" + std::to_string(last_cmd_.delta_on);
        r.texts.push_back(t);
        r.status = "auto_bias: target=" + std::to_string(algo_.target_event_rate_mev()) + "Mev" +
                   std::string(roi_.region.enabled ? " (ROI)" : "");
        return r;
    }
    void reset() override { algo_.reset(); passthrough_.clear(); roi_buf_.clear(); last_t_ = 0; }
};

// ===========================================================================
// Group F: Event-vector (process returns vector<Event>)
// ===========================================================================

/// TriggerSyncedFilter backend — outputs filtered event vector.
/// Supports ROI (design §5.6.6): only ROI events reach the algo, so the
/// output vector contains only ROI events.
class TriggerSyncedBackend final : public AlgoBackend {
    gui_algo::TriggerSyncedFilter algo_;
    std::vector<Metavision::EventCD> last_out_;
    RoiFilter roi_;
public:
    TriggerSyncedBackend(int w, int h) : algo_(w, h) { roi_.init(w, h); }
    void set_param(const std::string& k, const std::string& v) override {
        if (roi_.set_param(k, v)) return;
        if (k == "window_us") algo_.set_trigger_window_us(to_i(v));
    }
    std::string get_param(const std::string& k) const override {
        auto r = roi_.get_param(k); if (!r.empty()) return r;
        return {};
    }
    void push_events(const Metavision::EventCD* b, const Metavision::EventCD* e) override {
        std::vector<Metavision::EventCD> inp(b, e);
        auto* ev = const_cast<gui_algo::Event*>(as_events(inp.data()));
        std::size_t n = inp.size();
        // Compact in place to ROI events (design §5.6.6).
        if (roi_.region.enabled && roi_.region.rw > 0 && roi_.region.rh > 0) {
            std::size_t kept = 0;
            for (std::size_t i = 0; i < n; ++i) {
                if (roi_.region.contains(ev[i].x, ev[i].y)) ev[kept++] = ev[i];
            }
            n = kept;
        }
        gui_algo::EventPacket pkt(ev, n);
        auto out = algo_.process(pkt);
        last_out_.assign(out.begin(), out.end());
    }
    AlgoResult pull_result() override {
        AlgoResult r;
        r.filtered_events = last_out_;
        r.status = "trigger_synced: " + std::to_string(last_out_.size()) + " events" +
                   std::string(roi_.region.enabled ? " (ROI)" : "");
        return r;
    }
    void reset() override { algo_.reset(); last_out_.clear(); }
};

/// UltraSlowMotion backend — outputs time-dilated event vector.
/// Supports ROI (design §5.6.6): only ROI events are time-dilated.
class UltraSlowMotionBackend final : public AlgoBackend {
    gui_algo::UltraSlowMotion algo_;
    std::vector<Metavision::EventCD> last_out_;
    RoiFilter roi_;
public:
    UltraSlowMotionBackend(int w, int h) : algo_(w, h) { roi_.init(w, h); }
    void set_param(const std::string& k, const std::string& v) override {
        if (roi_.set_param(k, v)) return;
        if (k == "factor") algo_.set_dilation_factor(static_cast<float>(to_d(v)));
    }
    std::string get_param(const std::string& k) const override {
        auto r = roi_.get_param(k); if (!r.empty()) return r;
        return {};
    }
    void push_events(const Metavision::EventCD* b, const Metavision::EventCD* e) override {
        std::vector<Metavision::EventCD> inp(b, e);
        auto* ev = const_cast<gui_algo::Event*>(as_events(inp.data()));
        std::size_t n = inp.size();
        // Compact in place to ROI events (design §5.6.6).
        if (roi_.region.enabled && roi_.region.rw > 0 && roi_.region.rh > 0) {
            std::size_t kept = 0;
            for (std::size_t i = 0; i < n; ++i) {
                if (roi_.region.contains(ev[i].x, ev[i].y)) ev[kept++] = ev[i];
            }
            n = kept;
        }
        auto out = algo_.process(ev, n);
        last_out_.assign(out.begin(), out.end());
    }
    AlgoResult pull_result() override {
        AlgoResult r;
        r.filtered_events = last_out_;
        r.status = "slow_motion: " + std::to_string(last_out_.size()) + " events" +
                   std::string(roi_.region.enabled ? " (ROI)" : "");
        return r;
    }
    void reset() override { algo_.reset(); last_out_.clear(); }
};

// ===========================================================================
// Group G: Visualization
// ===========================================================================

/// XYTVisualizer backend — 3D point cloud data for space_time_display.
/// Complex algorithm (design §4.3.25): defaults to the center 128×128 ROI to
/// bound the point-cloud size. Events outside the ROI are dropped.
///
/// NOTE: this backend does NOT produce overlay output. The 3D point cloud is
/// rendered by SpaceTimeDisplay, which owns its own XYTVisualizer instance and
/// receives events directly from the SDK callback (see install_algo_callback).
/// This backend exists only so the AlgoWindow can expose ROI/parameters and
/// reflect the buffer state in its status label.
class XYTVisualizerBackend final : public AlgoBackend {
    int sensor_w_{0}, sensor_h_{0};
    ProcessRegion roi_;
    gui_algo::XYTVisualizer algo_;
    std::vector<Metavision::EventCD> passthrough_;
    std::vector<gui_algo::Event> roi_events_;
public:
    XYTVisualizerBackend(int w, int h)
        : sensor_w_(w), sensor_h_(h),
          algo_(1000.0f,
                gui_algo::XYTVisualizer::ColorMode::Polarity,
                2.5f,
                false,
                false) {
        roi_.compute(sensor_w_, sensor_h_);
    }
    void set_param(const std::string& k, const std::string& v) override {
        if (k == "time_window_us") algo_.set_time_window_ms(static_cast<float>(to_i(v)) / 1000.0F);
        else if (k == "roi_enabled") { roi_.enabled = to_b(v); roi_.compute(sensor_w_, sensor_h_); }
        else if (k == "roi_x") { roi_.x = to_i(v); roi_.compute(sensor_w_, sensor_h_); }
        else if (k == "roi_y") { roi_.y = to_i(v); roi_.compute(sensor_w_, sensor_h_); }
        else if (k == "roi_w") { roi_.w = to_i(v); roi_.compute(sensor_w_, sensor_h_); }
        else if (k == "roi_h") { roi_.h = to_i(v); roi_.compute(sensor_w_, sensor_h_); }
    }
    std::string get_param(const std::string& k) const override {
        if (k == "roi_enabled") return from_b(roi_.enabled);
        return {};
    }
    void push_events(const Metavision::EventCD* b, const Metavision::EventCD* e) override {
        passthrough_.assign(b, e);
        if (roi_.enabled && roi_.rw > 0 && roi_.rh > 0) {
            roi_events_.clear();
            const auto* ev = as_events(passthrough_.data());
            for (std::size_t i = 0; i < passthrough_.size(); ++i) {
                if (roi_.contains(ev[i].x, ev[i].y)) roi_events_.push_back(ev[i]);
            }
            algo_.process(roi_events_.data(), roi_events_.size());
        } else {
            algo_.process(as_events(passthrough_.data()), passthrough_.size());
        }
        // NOTE: do NOT call render() here — the 3D point cloud is rendered by
        // SpaceTimeDisplay which owns its own XYTVisualizer instance. This
        // backend only forwards events so the AlgoWindow status reflects the
        // ROI filter. Generating 50K OverlayPoints here would splatter 2D
        // points across the main display (wrong) and stall the GUI thread.
    }
    AlgoResult pull_result() override {
        AlgoResult r;
        r.filtered_events = passthrough_;
        // No overlay/frame: the 3D rendering is handled by SpaceTimeDisplay.
        r.status = "xyt: " + std::to_string(algo_.size()) + " buffered events" +
                   std::string(roi_.enabled ? " (ROI)" : " (full)");
        return r;
    }
    void reset() override { algo_.clear(); passthrough_.clear(); roi_events_.clear(); }
};

// ===========================================================================
// Group H: Misc filters
// ===========================================================================

/// OrientationFilter backend — orientation histogram as overlay text.
/// Supports ROI (design §5.6.6): only ROI events feed the histogram.
class OrientationFilterBackend final : public AlgoBackend {
    gui_algo::OrientationFilter algo_;
    std::vector<Metavision::EventCD> passthrough_;
    RoiFilter roi_;
    std::vector<gui_algo::Event> roi_buf_;
    int hist_[gui_algo::OrientationFilter::kNumOrientations]{};
public:
    OrientationFilterBackend(int w, int h) : algo_(w, h) { roi_.init(w, h); }
    void set_param(const std::string& k, const std::string& v) override {
        if (roi_.set_param(k, v)) return;
        if (k == "tau_us") algo_.set_time_window_us(to_i(v));
    }
    std::string get_param(const std::string& k) const override {
        auto r = roi_.get_param(k); if (!r.empty()) return r;
        return {};
    }
    void push_events(const Metavision::EventCD* b, const Metavision::EventCD* e) override {
        passthrough_.assign(b, e);
        auto [ev, n] = roi_.apply(as_events(passthrough_.data()),
                                   passthrough_.size(), roi_buf_);
        std::vector<int> out;
        algo_.process(ev, n, out);
        for (int v : out) {
            if (v >= 0 && v < gui_algo::OrientationFilter::kNumOrientations) ++hist_[v];
        }
    }
    AlgoResult pull_result() override {
        AlgoResult r;
        r.filtered_events = passthrough_;
        OverlayText t;
        t.x = 10; t.y = 20;
        t.text = "orient: 0=" + std::to_string(hist_[0]) +
                 " 45=" + std::to_string(hist_[1]) +
                 " 90=" + std::to_string(hist_[2]) +
                 " 135=" + std::to_string(hist_[3]);
        r.texts.push_back(t);
        r.status = "orient_filter: histogram updated" +
                   std::string(roi_.region.enabled ? " (ROI)" : "");
        return r;
    }
    void reset() override { algo_.reset(); passthrough_.clear(); roi_buf_.clear(); std::fill(hist_, hist_ + 4, 0); }
};

/// DirectionSelectiveFilter backend — direction histogram as overlay text.
/// Supports ROI (design §5.6.6): only ROI events feed the histogram.
class DirectionSelectiveBackend final : public AlgoBackend {
    gui_algo::DirectionSelectiveFilter algo_;
    std::vector<Metavision::EventCD> passthrough_;
    RoiFilter roi_;
    std::vector<gui_algo::Event> roi_buf_;
public:
    DirectionSelectiveBackend(int w, int h) : algo_(w, h) {
        roi_.init(w, h);
        algo_.set_enable_global_mode(true);
    }
    void set_param(const std::string& k, const std::string& v) override {
        if (roi_.set_param(k, v)) return;
        if (k == "tau_us") algo_.set_time_window_us(to_i(v));
    }
    std::string get_param(const std::string& k) const override {
        auto r = roi_.get_param(k); if (!r.empty()) return r;
        return {};
    }
    void push_events(const Metavision::EventCD* b, const Metavision::EventCD* e) override {
        passthrough_.assign(b, e);
        auto [ev, n] = roi_.apply(as_events(passthrough_.data()),
                                   passthrough_.size(), roi_buf_);
        std::vector<int> out;
        algo_.process(ev, n, out);
    }
    AlgoResult pull_result() override {
        AlgoResult r;
        r.filtered_events = passthrough_;
        const auto& hist = algo_.global_histogram();
        OverlayText t;
        t.x = 10; t.y = 40;
        t.text = "dir: ";
        for (int i = 0; i < gui_algo::DirectionSelectiveFilter::kNumDirections; ++i) {
            t.text += std::to_string(i * 45) + "=" + std::to_string(hist[i]) + " ";
        }
        r.texts.push_back(t);
        const int dom = algo_.global_direction();
        r.status = "dir_selective: dominant=" + std::to_string(dom) +
                   std::string(roi_.region.enabled ? " (ROI)" : "");
        return r;
    }
    void reset() override { algo_.reset(); passthrough_.clear(); roi_buf_.clear(); }
};

/// BackgroundMaskFilter backend — produces background mask frame.
/// Supports ROI (design §5.6.6): only ROI events update the mask; the mask
/// remains sensor-sized so only the ROI region is updated.
class BackgroundMaskBackend final : public AlgoBackend {
    gui_algo::BackgroundMaskFilter algo_;
    std::vector<Metavision::EventCD> passthrough_;
    RoiFilter roi_;
    std::vector<gui_algo::Event> roi_buf_;
public:
    BackgroundMaskBackend(int w, int h) : algo_(w, h) { roi_.init(w, h); }
    void set_param(const std::string& k, const std::string& v) override {
        if (roi_.set_param(k, v)) return;
        if (k == "learning_rate") algo_.set_learning_window_s(static_cast<float>(to_d(v)));
        else if (k == "threshold") algo_.set_background_rate_threshold_hz(static_cast<float>(to_d(v)));
    }
    std::string get_param(const std::string& k) const override {
        auto r = roi_.get_param(k); if (!r.empty()) return r;
        return {};
    }
    void push_events(const Metavision::EventCD* b, const Metavision::EventCD* e) override {
        passthrough_.assign(b, e);
        auto [ev, n] = roi_.apply(as_events(passthrough_.data()),
                                   passthrough_.size(), roi_buf_);
        gui_algo::EventPacket pkt(ev, n);
        algo_.process(pkt);
    }
    AlgoResult pull_result() override {
        AlgoResult r;
        r.filtered_events = passthrough_;
        r.has_frame = true;
        r.frame = algo_.mask().clone();
        r.status = "bg_mask: active" +
                   std::string(roi_.region.enabled ? " (ROI)" : "");
        return r;
    }
    void reset() override { algo_.reset(); passthrough_.clear(); roi_buf_.clear(); }
};

/// BandpassFilter backend — event-rate band-pass, text overlay.
/// Supports ROI (design §5.6.6): rate computed from ROI events only.
class BandpassFilterBackend final : public AlgoBackend {
    gui_algo::BandpassFilter algo_;
    std::vector<Metavision::EventCD> passthrough_;
    RoiFilter roi_;
    std::vector<gui_algo::Event> roi_buf_;
    Metavision::timestamp last_t_{0};
    double low_cutoff_hz_{1.0};
    double high_cutoff_hz_{10.0};
public:
    BandpassFilterBackend(int w, int h) : algo_(1.0F, 10.0F, 1, 0.033) { roi_.init(w, h); }
    void set_param(const std::string& k, const std::string& v) override {
        if (roi_.set_param(k, v)) return;
        if (k == "low_cutoff_hz") {
            low_cutoff_hz_ = to_d(v);
            algo_.set_cutoffs(low_cutoff_hz_, high_cutoff_hz_);
        } else if (k == "high_cutoff_hz") {
            high_cutoff_hz_ = to_d(v);
            algo_.set_cutoffs(low_cutoff_hz_, high_cutoff_hz_);
        }
    }
    std::string get_param(const std::string& k) const override {
        auto r = roi_.get_param(k); if (!r.empty()) return r;
        return {};
    }
    void push_events(const Metavision::EventCD* b, const Metavision::EventCD* e) override {
        passthrough_.assign(b, e);
        auto [ev, n] = roi_.apply(as_events(passthrough_.data()),
                                   passthrough_.size(), roi_buf_);
        if (!passthrough_.empty()) {
            const auto t = passthrough_.back().t;
            algo_.add_events(n, t);
            last_t_ = t;
        }
    }
    AlgoResult pull_result() override {
        AlgoResult r;
        r.filtered_events = passthrough_;
        OverlayText t;
        t.x = 10; t.y = 60;
        t.text = "bp: " + std::to_string(algo_.value()) + " ev/s";
        r.texts.push_back(t);
        r.status = "bandpass: " + std::to_string(algo_.value()) + " ev/s" +
                   std::string(roi_.region.enabled ? " (ROI)" : "");
        return r;
    }
    void reset() override { algo_.reset(); passthrough_.clear(); roi_buf_.clear(); last_t_ = 0; }
};

/// Overlay backend — pass-through (overlay is drawn by frame_annotator from
/// other algos). Supports ROI (design §5.6.6): when enabled, only ROI events
/// pass through to filtered_events.
class OverlayBackend final : public AlgoBackend {
    std::vector<Metavision::EventCD> passthrough_;
    RoiFilter roi_;
public:
    OverlayBackend(int w, int h) { roi_.init(w, h); }
    void set_param(const std::string& k, const std::string& v) override {
        roi_.set_param(k, v);
    }
    std::string get_param(const std::string& k) const override {
        auto r = roi_.get_param(k); if (!r.empty()) return r;
        return {};
    }
    void push_events(const Metavision::EventCD* b, const Metavision::EventCD* e) override {
        passthrough_.assign(b, e);
        // Compact in place to ROI events (design §5.6.6).
        if (roi_.region.enabled && roi_.region.rw > 0 && roi_.region.rh > 0) {
            auto* ev = const_cast<gui_algo::Event*>(as_events(passthrough_.data()));
            std::size_t n = passthrough_.size();
            std::size_t kept = 0;
            for (std::size_t i = 0; i < n; ++i) {
                if (roi_.region.contains(ev[i].x, ev[i].y)) ev[kept++] = ev[i];
            }
            passthrough_.resize(kept);
        }
    }
    AlgoResult pull_result() override {
        AlgoResult r;
        r.filtered_events = passthrough_;
        r.status = "overlay: pass-through" +
                   std::string(roi_.region.enabled ? " (ROI)" : "");
        return r;
    }
    void reset() override { passthrough_.clear(); }
};

// ===========================================================================
// Factory
// ===========================================================================

std::unique_ptr<AlgoBackend> create_algo_backend(const std::string& name,
                                                  int width, int height) {
    // algo/cv filters
    if (name == "noise_filter")          return std::make_unique<NoiseFilterBackend>(width, height);
    if (name == "hot_pixel_filter")      return std::make_unique<HotPixelFilterBackend>(width, height);
    if (name == "orientation_filter")    return std::make_unique<OrientationFilterBackend>(width, height);
    if (name == "direction_selective")   return std::make_unique<DirectionSelectiveBackend>(width, height);
    if (name == "sparse_optical_flow")   return std::make_unique<SparseOpticalFlowBackend>(width, height);
    if (name == "blob_detector")         return std::make_unique<BlobDetectorBackend>(width, height);
    if (name == "object_tracker")        return std::make_unique<ObjectTrackerBackend>(width, height);
    if (name == "corner_detector")       return std::make_unique<CornerDetectorBackend>(width, height);
    if (name == "line_segment")          return std::make_unique<LineSegmentBackend>(width, height);
    if (name == "hough_line")            return std::make_unique<HoughLineBackend>(width, height);
    if (name == "hough_circle")          return std::make_unique<HoughCircleBackend>(width, height);
    if (name == "orientation_cluster")   return std::make_unique<OrientationClusterBackend>(width, height);
    if (name == "cluster_lif")           return std::make_unique<ClusterLifBackend>(width, height);
    if (name == "background_mask")       return std::make_unique<BackgroundMaskBackend>(width, height);
    if (name == "perspective_undistort") return std::make_unique<PerspectiveUndistortBackend>(width, height);
    if (name == "trigger_synced")        return std::make_unique<TriggerSyncedBackend>(width, height);
    if (name == "bandpass_filter")       return std::make_unique<BandpassFilterBackend>(width, height);
    if (name == "optical_gyro")          return std::make_unique<OpticalGyroBackend>(width, height);
    if (name == "ultra_slow_motion")     return std::make_unique<UltraSlowMotionBackend>(width, height);
    if (name == "xyt_visualizer")        return std::make_unique<XYTVisualizerBackend>(width, height);
    if (name == "time_surface")          return std::make_unique<TimeSurfaceBackend>(width, height);
    if (name == "overlay")               return std::make_unique<OverlayBackend>(width, height);
    // algo/analytics
    if (name == "active_marker")         return std::make_unique<ActiveMarkerBackend>(width, height);
    if (name == "event_to_video")        return std::make_unique<EventToVideoBackend>(width, height);
    if (name == "flow_statistics")       return std::make_unique<FlowStatisticsBackend>(width, height);
    if (name == "isi_analyzer")          return std::make_unique<ISIAnalyzerBackend>(width, height);
    if (name == "particle_counter")      return std::make_unique<ParticleCounterBackend>(width, height);
    if (name == "auto_bias")             return std::make_unique<AutoBiasBackend>(width, height);
    if (name == "freq_detector")         return std::make_unique<FreqDetectorBackend>(width, height);
    return nullptr;
}

} // namespace gui
