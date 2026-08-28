// gui/algo_bridge/filter_chain.cpp

#include "filter_chain.h"

#include <cmath>
#include <limits>
#include <mutex>
#include <sstream>

#include <metavision/sdk/core/algorithms/event_rescaler_algorithm.h>
#include <metavision/sdk/core/algorithms/flip_x_algorithm.h>
#include <metavision/sdk/core/algorithms/flip_y_algorithm.h>
#include <metavision/sdk/core/algorithms/polarity_filter_algorithm.h>
#include <metavision/sdk/core/algorithms/polarity_inverter_algorithm.h>
#include <metavision/sdk/core/algorithms/transpose_events_algorithm.h>

namespace gui {

// FilterChain is mutated from the GUI thread (set_enabled / set_param /
// set_geometry) and read from the SDK data thread (process / has_enabled).
// A mutex serialises the two; the per-stage algorithms themselves are not
// otherwise thread-safe.
namespace {
std::mutex& chain_mutex() {
    static std::mutex m;
    return m;
}
} // namespace

namespace {

// Helper to parse a typed value from a string.
template<class T>
bool parse(const std::string& s, T& out) {
    std::istringstream iss(s);
    if (!(iss >> out)) return false;
    iss >> std::ws;
    // A successful extraction can already be at EOF; std::ws then adds
    // failbit while preserving eofbit. Full consumption remains valid.
    return iss.eof();
}
template<>
bool parse<bool>(const std::string& s, bool& out) {
    out = (s == "1" || s == "true" || s == "True");
    return true;
}
template<>
bool parse<std::string>(const std::string& s, std::string& out) {
    out = s;
    return true;
}

// --- Concrete stages ---

class PolarityFilterStage : public FilterStage {
public:
    void process(const Metavision::EventCD* b, const Metavision::EventCD* e,
                 std::vector<Metavision::EventCD>& out) override {
        if (!enabled_) return;
        algo_.process_events(b, e, std::back_inserter(out));
    }
    bool set_param(const std::string& k, const std::string& v) override {
        if (k == "polarity") {
            std::int16_t p = 0;
            if (!parse(v, p)) return false;
            algo_.set_polarity(p);
            return true;
        }
        return false;
    }
    std::string name() const override { return "polarity_filter"; }
private:
    Metavision::PolarityFilterAlgorithm algo_{0};
};

class PolarityInvertStage : public FilterStage {
public:
    void process(const Metavision::EventCD* b, const Metavision::EventCD* e,
                 std::vector<Metavision::EventCD>& out) override {
        if (!enabled_) return;
        algo_.process_events(b, e, std::back_inserter(out));
    }
    bool set_param(const std::string&, const std::string&) override { return false; }
    std::string name() const override { return "polarity_invert"; }
private:
    Metavision::PolarityInverterAlgorithm algo_;
};

class FlipXStage : public FilterStage {
public:
    explicit FlipXStage(int w) : algo_(static_cast<std::int16_t>(w - 1)) {}
    void process(const Metavision::EventCD* b, const Metavision::EventCD* e,
                 std::vector<Metavision::EventCD>& out) override {
        if (!enabled_) return;
        algo_.process_events(b, e, std::back_inserter(out));
    }
    bool set_param(const std::string& k, const std::string& v) override {
        if (k == "width_minus_one") {
            std::int16_t w = 0;
            if (!parse(v, w)) return false;
            algo_.set_width_minus_one(w);
            return true;
        }
        return false;
    }
    std::string name() const override { return "flip_x"; }
private:
    Metavision::FlipXAlgorithm algo_;
};

class FlipYStage : public FilterStage {
public:
    explicit FlipYStage(int h) : algo_(static_cast<std::int16_t>(h - 1)) {}
    void process(const Metavision::EventCD* b, const Metavision::EventCD* e,
                 std::vector<Metavision::EventCD>& out) override {
        if (!enabled_) return;
        algo_.process_events(b, e, std::back_inserter(out));
    }
    bool set_param(const std::string& k, const std::string& v) override {
        if (k == "height_minus_one") {
            std::int16_t hgt = 0;
            if (!parse(v, hgt)) return false;
            algo_.set_height_minus_one(hgt);
            return true;
        }
        return false;
    }
    std::string name() const override { return "flip_y"; }
private:
    Metavision::FlipYAlgorithm algo_;
};

class RotateStage : public FilterStage {
public:
    void process(const Metavision::EventCD* b, const Metavision::EventCD* e,
                 std::vector<Metavision::EventCD>& out) override {
        if (!enabled_) return;
        // For 90/180/270 we apply the transform directly so the output
        // coordinates are mathematically correct. RotateEventsAlgorithm
        // rotates about the original frame centre and clips against that raw
        // extent, so U1C1 deliberately accepts only the discrete plan below.
        const std::int16_t W = width_minus_one_;
        const std::int16_t H = height_minus_one_;
        for (auto it = b; it != e; ++it) {
            Metavision::EventCD ev = *it;
            switch (angle_) {
                case 90:  { std::int16_t nx = static_cast<std::int16_t>(H - ev.y);
                            std::int16_t ny = ev.x;
                            if (nx < 0 || ny < 0 || ny > W) continue;
                            ev.x = nx; ev.y = ny; break; }
                case 180: { ev.x = static_cast<std::int16_t>(W - ev.x);
                            ev.y = static_cast<std::int16_t>(H - ev.y); break; }
                case 270: { std::int16_t nx = ev.y;
                            std::int16_t ny = static_cast<std::int16_t>(W - ev.x);
                            if (nx < 0 || nx > H || ny < 0 || ny > W) continue;
                            ev.x = nx; ev.y = ny; break; }
                default: break; // 0° = identity
            }
            out.push_back(ev);
        }
    }
    bool set_param(const std::string& k, const std::string& v) override {
        if (k == "rotation") {
            // U1C1 rejects arbitrary radians. The frozen contract contains
            // only these typed discrete rotations, with explicit output extent
            // derivation and mapping semantics.
            if (v == "0")   { angle_ = 0; return true; }
            if (v == "90")  { angle_ = 90; return true; }
            if (v == "180") { angle_ = 180; return true; }
            if (v == "270") { angle_ = 270; return true; }
            return false;
        }
        if (k == "width_minus_one") {
            std::int16_t w = 0;
            if (!parse(v, w)) return false;
            width_minus_one_ = w;
            return true;
        }
        if (k == "height_minus_one") {
            std::int16_t h = 0;
            if (!parse(v, h)) return false;
            height_minus_one_ = h;
            return true;
        }
        return false;
    }
    std::string name() const override { return "rotate"; }
private:
    int angle_{0};
    std::int16_t width_minus_one_{0};
    std::int16_t height_minus_one_{0};
};

class TransposeStage : public FilterStage {
public:
    void process(const Metavision::EventCD* b, const Metavision::EventCD* e,
                 std::vector<Metavision::EventCD>& out) override {
        if (!enabled_) return;
        algo_.process_events(b, e, std::back_inserter(out));
    }
    bool set_param(const std::string&, const std::string&) override { return false; }
    std::string name() const override { return "transpose"; }
private:
    Metavision::TransposeEventsAlgorithm algo_;
};

class RescaleStage : public FilterStage {
public:
    RescaleStage() { rebuild(); }
    void process(const Metavision::EventCD* b, const Metavision::EventCD* e,
                 std::vector<Metavision::EventCD>& out) override {
        if (!enabled_ || !algo_) return;
        algo_->process_events(b, e, std::back_inserter(out));
    }
    bool set_param(const std::string& k, const std::string& v) override {
        float f = 0;
        if (!parse(v, f) || !std::isfinite(f) || f <= 0) return false;
        if (k == "scale_width")  { sw_ = f; rebuild(); return true; }
        if (k == "scale_height") { sh_ = f; rebuild(); return true; }
        return false;
    }
    std::string name() const override { return "rescale"; }
private:
    void rebuild() { algo_ = std::make_unique<Metavision::EventRescalerAlgorithm>(sw_, sh_); }
    float sw_{1.0f}, sh_{1.0f};
    std::unique_ptr<Metavision::EventRescalerAlgorithm> algo_;
};

// Phase 2.6 debug D-6: RoiFilterStage was deleted (superseded by the
// unified ROI — see the FilterChain ctor).

} // namespace

namespace {

constexpr int kMaxSignedGeometryExtent = std::numeric_limits<std::int16_t>::max() + 1;

bool plans_equal(const ConditionedTransformPlan& lhs, const ConditionedTransformPlan& rhs) {
    return lhs.flip_x_enabled == rhs.flip_x_enabled &&
           lhs.flip_y_enabled == rhs.flip_y_enabled &&
           lhs.rotate_enabled == rhs.rotate_enabled && lhs.rotation == rhs.rotation &&
           lhs.transpose_enabled == rhs.transpose_enabled &&
           lhs.rescale_enabled == rhs.rescale_enabled &&
           lhs.scale_x == rhs.scale_x && lhs.scale_y == rhs.scale_y;
}

bool parse_rotation(const std::string& value, OrthogonalRotation& rotation) {
    if (value == "0") {
        rotation = OrthogonalRotation::Degrees0;
        return true;
    }
    if (value == "90") {
        rotation = OrthogonalRotation::Degrees90;
        return true;
    }
    if (value == "180") {
        rotation = OrthogonalRotation::Degrees180;
        return true;
    }
    if (value == "270") {
        rotation = OrthogonalRotation::Degrees270;
        return true;
    }
    return false;
}

bool parse_finite_float(const std::string& value, float& parsed) {
    return parse(value, parsed) && std::isfinite(parsed) && parsed > 0.0f;
}

std::optional<GeometryRevision> next_geometry_revision(const GeometryRevision current) {
    if (current == std::numeric_limits<GeometryRevision>::max()) {
        return std::nullopt;
    }
    return current + 1;
}

bool producer_supports_plan(const GeometryExtent& raw_extent,
                            const ConditionedTransformPlan& plan,
                            std::string& reason) {
    // Current FlipX/FlipY/Rotate OpenEB producers receive signed int16
    // width_minus_one/height_minus_one. The core's EventCD bound is broader,
    // so retain this producer-specific check at the FilterChain boundary.
    if (plan.flip_x_enabled && raw_extent.width > kMaxSignedGeometryExtent) {
        reason = "Flip X source width exceeds the current OpenEB producer limit";
        return false;
    }
    if (plan.flip_y_enabled && raw_extent.height > kMaxSignedGeometryExtent) {
        reason = "Flip Y source height exceeds the current OpenEB producer limit";
        return false;
    }
    if (plan.rotate_enabled &&
        (raw_extent.width > kMaxSignedGeometryExtent ||
         raw_extent.height > kMaxSignedGeometryExtent)) {
        reason = "rotation source extent exceeds the current OpenEB producer limit";
        return false;
    }
    return true;
}

bool has_supported_parameter(const std::string& stage, const std::string& key) {
    return (stage == "polarity_filter" && key == "polarity") ||
           (stage == "rotate" && key == "rotation") ||
           (stage == "rescale" && (key == "scale_width" || key == "scale_height"));
}

} // namespace

FilterChain::FilterChain() {
    auto add = [this](const std::string& name, std::unique_ptr<FilterStage> stage,
                      std::unordered_map<std::string, std::string> parameters = {}) {
        order_.push_back(name);
        stages_[name] = std::move(stage);
        stage_states_[name] = {false, std::move(parameters)};
    };
    add("polarity_filter", std::make_unique<PolarityFilterStage>(), {{"polarity", "0"}});
    add("polarity_invert", std::make_unique<PolarityInvertStage>());
    add("flip_x", std::make_unique<FlipXStage>(width_));
    add("flip_y", std::make_unique<FlipYStage>(height_));
    add("rotate", std::make_unique<RotateStage>(), {{"rotation", "0"}});
    add("transpose", std::make_unique<TransposeStage>());
    add("rescale", std::make_unique<RescaleStage>(),
        {{"scale_width", "1.0"}, {"scale_height", "1.0"}});
}

bool FilterChain::set_geometry(const int width, const int height) {
    std::lock_guard<std::mutex> lk(chain_mutex());
    width_ = width;
    height_ = height;

    // Keep the existing producer algorithms in sync only while their signed
    // geometry arguments are representable. A later coordinate-stage request
    // performs the matching fail-closed producer check before enabling it.
    if (width > 0 && width <= kMaxSignedGeometryExtent) {
        stages_.at("flip_x")->set_param("width_minus_one", std::to_string(width - 1));
        stages_.at("rotate")->set_param("width_minus_one", std::to_string(width - 1));
    }
    if (height > 0 && height <= kMaxSignedGeometryExtent) {
        stages_.at("flip_y")->set_param("height_minus_one", std::to_string(height - 1));
        stages_.at("rotate")->set_param("height_minus_one", std::to_string(height - 1));
    }
    return rebuild_conditioned_geometry_locked();
}

const FilterStage* FilterChain::stage(const std::string& name) const {
    std::lock_guard<std::mutex> lk(chain_mutex());
    const auto it = stages_.find(name);
    return it == stages_.end() ? nullptr : it->second.get();
}

bool FilterChain::set_stage_enabled(const std::string& name, const bool enabled) {
    FilterStageRequest request;
    request.stage = name;
    request.enabled = enabled;
    return try_apply_stage(request).accepted;
}

bool FilterChain::set_stage_param(const std::string& name, const std::string& key,
                                  const std::string& value) {
    std::lock_guard<std::mutex> lk(chain_mutex());
    const auto state = stage_states_.find(name);
    if (state == stage_states_.end()) return false;
    FilterStageRequest request;
    request.stage = name;
    request.enabled = state->second.enabled;
    request.parameters[key] = value;
    return try_apply_stage_locked(request).accepted;
}

bool FilterChain::is_stage_enabled(const std::string& name) const {
    std::lock_guard<std::mutex> lk(chain_mutex());
    const auto it = stage_states_.find(name);
    return it != stage_states_.end() && it->second.enabled;
}

FilterAdmissionResult FilterChain::try_apply_stage(const FilterStageRequest& request) {
    std::lock_guard<std::mutex> lk(chain_mutex());
    return try_apply_stage_locked(request);
}

std::optional<FilterStageState> FilterChain::stage_state(const std::string& name) const {
    std::lock_guard<std::mutex> lk(chain_mutex());
    const auto it = stage_states_.find(name);
    if (it == stage_states_.end()) return std::nullopt;
    return it->second;
}

std::optional<ConditionedGeometry> FilterChain::conditioned_geometry() const {
    std::lock_guard<std::mutex> lk(chain_mutex());
    return conditioned_geometry_;
}

bool FilterChain::can_activate_raw_roi_or_roni(std::string* reason) const {
    std::lock_guard<std::mutex> lk(chain_mutex());
    if (plan_.has_non_identity_coordinate_mapping()) {
        if (reason) *reason = "ROI/RONI requires raw-space migration before coordinate transforms";
        return false;
    }
    return true;
}

bool FilterChain::can_start_processed_recording(std::string* reason) const {
    std::lock_guard<std::mutex> lk(chain_mutex());
    if (plan_.has_non_identity_coordinate_mapping()) {
        if (reason) *reason =
            "processed recording requires qualified geometry metadata for coordinate transforms";
        return false;
    }
    return true;
}

bool FilterChain::try_set_raw_roi_or_roni_active(const bool active, std::string* reason) {
    std::lock_guard<std::mutex> lk(chain_mutex());
    if (active && plan_.has_non_identity_coordinate_mapping()) {
        if (reason) *reason = "ROI/RONI requires raw-space migration before coordinate transforms";
        return false;
    }
    admission_context_.raw_roi_or_roni_active = active;
    return true;
}

bool FilterChain::try_set_processed_recording_active(const bool active, std::string* reason) {
    std::lock_guard<std::mutex> lk(chain_mutex());
    if (active && plan_.has_non_identity_coordinate_mapping()) {
        if (reason) *reason =
            "processed recording requires qualified geometry metadata for coordinate transforms";
        return false;
    }
    admission_context_.processed_recording_active = active;
    return true;
}

bool FilterChain::has_non_identity_coordinate_plan() const {
    std::lock_guard<std::mutex> lk(chain_mutex());
    return plan_.has_non_identity_coordinate_mapping();
}

ConditionedTransformPlan FilterChain::candidate_plan_for_request_locked(
    const FilterStageRequest& request, bool& valid, std::string& reason) const {
    valid = false;
    auto state = stage_states_.find(request.stage);
    if (state == stage_states_.end()) {
        reason = "unknown preprocessing stage";
        return plan_;
    }

    FilterStageState candidate_state = state->second;
    candidate_state.enabled = request.enabled;
    for (const auto& parameter : request.parameters) {
        if (!has_supported_parameter(request.stage, parameter.first)) {
            reason = "unsupported preprocessing parameter";
            return plan_;
        }
        candidate_state.parameters[parameter.first] = parameter.second;
    }

    ConditionedTransformPlan candidate = plan_;
    if (request.stage == "flip_x") {
        candidate.flip_x_enabled = candidate_state.enabled;
    } else if (request.stage == "flip_y") {
        candidate.flip_y_enabled = candidate_state.enabled;
    } else if (request.stage == "rotate") {
        candidate.rotate_enabled = candidate_state.enabled;
        const auto rotation = candidate_state.parameters.find("rotation");
        if (rotation == candidate_state.parameters.end() ||
            !parse_rotation(rotation->second, candidate.rotation)) {
            reason = "only 0, 90, 180, and 270 degree rotations are supported";
            return plan_;
        }
    } else if (request.stage == "transpose") {
        candidate.transpose_enabled = candidate_state.enabled;
    } else if (request.stage == "rescale") {
        candidate.rescale_enabled = candidate_state.enabled;
        const auto scale_x = candidate_state.parameters.find("scale_width");
        const auto scale_y = candidate_state.parameters.find("scale_height");
        if (scale_x == candidate_state.parameters.end() ||
            scale_y == candidate_state.parameters.end() ||
            !parse_finite_float(scale_x->second, candidate.scale_x) ||
            !parse_finite_float(scale_y->second, candidate.scale_y)) {
            reason = "rescale factors must be finite and positive";
            return plan_;
        }
    } else if (request.stage == "polarity_filter") {
        const auto polarity = candidate_state.parameters.find("polarity");
        std::int16_t parsed = 0;
        if (polarity == candidate_state.parameters.end() ||
            !parse(polarity->second, parsed) || (parsed != 0 && parsed != 1)) {
            reason = "polarity must be 0 or 1";
            return plan_;
        }
    }

    valid = true;
    return candidate;
}

bool FilterChain::apply_stage_parameters_locked(const std::string& stage,
                                                const FilterStageState& state) {
    const auto implementation = stages_.find(stage);
    if (implementation == stages_.end()) return false;
    for (const auto& parameter : state.parameters) {
        if (!implementation->second->set_param(parameter.first, parameter.second)) {
            return false;
        }
    }
    return true;
}

bool FilterChain::rebuild_conditioned_geometry_locked(std::string* reason) {
    if (width_ <= 0 || height_ <= 0) {
        conditioned_geometry_.reset();
        if (reason) *reason = "raw source extent must be positive";
        return false;
    }
    if (plan_.has_non_identity_coordinate_mapping() &&
        (admission_context_.raw_roi_or_roni_active ||
         admission_context_.processed_recording_active)) {
        conditioned_geometry_.reset();
        if (reason) *reason =
            "coordinate transforms cannot be rebuilt with active ROI/RONI or processed recording";
        return false;
    }
    std::string producer_reason;
    if (!producer_supports_plan({width_, height_}, plan_, producer_reason)) {
        conditioned_geometry_.reset();
        if (reason) *reason = producer_reason;
        return false;
    }
    const auto next_revision = next_geometry_revision(geometry_revision_);
    if (!next_revision) {
        conditioned_geometry_.reset();
        if (reason) *reason = "geometry revision space is exhausted";
        return false;
    }
    std::string geometry_reason;
    auto candidate = ConditionedGeometry::create({width_, height_}, plan_, *next_revision,
                                                 &geometry_reason);
    if (!candidate) {
        conditioned_geometry_.reset();
        if (reason) *reason = geometry_reason;
        return false;
    }
    conditioned_geometry_ = std::move(candidate);
    geometry_revision_ = *next_revision;
    return true;
}

FilterAdmissionResult FilterChain::try_apply_stage_locked(const FilterStageRequest& request) {
    FilterAdmissionResult result;
    const auto current_state = stage_states_.find(request.stage);
    if (current_state == stage_states_.end()) {
        result.reason = "unknown preprocessing stage";
        return result;
    }
    result.committed_state = current_state->second;
    result.geometry_revision = geometry_revision_;

    bool valid = false;
    std::string reason;
    const ConditionedTransformPlan candidate_plan =
        candidate_plan_for_request_locked(request, valid, reason);
    if (!valid) {
        result.reason = reason;
        return result;
    }

    FilterStageState candidate_state = current_state->second;
    candidate_state.enabled = request.enabled;
    for (const auto& parameter : request.parameters) {
        candidate_state.parameters[parameter.first] = parameter.second;
    }

    const bool plan_changed = !plans_equal(plan_, candidate_plan);
    const bool disabling_active_coordinate_stage =
        !request.enabled && current_state->second.enabled &&
        (request.stage == "flip_x" || request.stage == "flip_y" ||
         request.stage == "rotate" || request.stage == "transpose" ||
         request.stage == "rescale");

    // A source-geometry change can fail closed and clear the snapshot before
    // the UI receives its disconnected signal. Disabling an already-active
    // coordinate stage is always a safe recovery operation in that state:
    // it cannot publish events, and it lets the next valid source begin from
    // coherent UI/backend enablement. No revision is published because there
    // is no valid geometry to bind to one.
    if (plan_changed && !conditioned_geometry_ && disabling_active_coordinate_stage) {
        if (!apply_stage_parameters_locked(request.stage, candidate_state)) {
            result.reason = "failed to apply validated preprocessing parameters";
            return result;
        }
        stages_.at(request.stage)->set_enabled(false);
        stage_states_[request.stage] = std::move(candidate_state);
        plan_ = candidate_plan;
        result.accepted = true;
        result.committed_state = stage_states_.at(request.stage);
        return result;
    }

    if (plan_changed) {
        if (candidate_plan.transpose_enabled) {
            result.reason = "Transpose is unavailable until geometry consumers are migrated.";
            return result;
        }
        if (candidate_plan.rotate_enabled &&
            (candidate_plan.rotation == OrthogonalRotation::Degrees90 ||
             candidate_plan.rotation == OrthogonalRotation::Degrees270)) {
            result.reason = "Rotate 90/270 is unavailable until geometry consumers are migrated.";
            return result;
        }
        if (candidate_plan.rescale_enabled) {
            result.reason = "Rescale is unavailable until geometry consumers are migrated.";
            return result;
        }
        if (admission_context_.raw_roi_or_roni_active &&
            candidate_plan.has_non_identity_coordinate_mapping()) {
            result.reason =
                "Coordinate transforms are unavailable while raw ROI/RONI is active.";
            return result;
        }
        if (admission_context_.processed_recording_active &&
            candidate_plan.has_non_identity_coordinate_mapping()) {
            result.reason =
                "Coordinate transforms are unavailable during processed recording.";
            return result;
        }
        if (!producer_supports_plan({width_, height_}, candidate_plan, reason)) {
            result.reason = reason;
            return result;
        }

        // A plan mutation needs a valid source snapshot even when its output
        // extent is unchanged: flip and Rotate180 are geometry revisions too.
        if (width_ <= 0 || height_ <= 0) {
            result.reason = "A valid raw source geometry is required before enabling preprocessing.";
            return result;
        }
        const auto next_revision = next_geometry_revision(geometry_revision_);
        if (!next_revision) {
            result.reason = "geometry revision space is exhausted";
            return result;
        }
        auto candidate_geometry = ConditionedGeometry::create(
            {width_, height_}, candidate_plan, *next_revision, &reason);
        if (!candidate_geometry) {
            result.reason = reason;
            return result;
        }
        if (!apply_stage_parameters_locked(request.stage, candidate_state)) {
            result.reason = "failed to apply validated preprocessing parameters";
            return result;
        }
        stages_.at(request.stage)->set_enabled(candidate_state.enabled);
        stage_states_[request.stage] = std::move(candidate_state);
        plan_ = candidate_plan;
        conditioned_geometry_ = std::move(candidate_geometry);
        geometry_revision_ = *next_revision;
        result.accepted = true;
        result.committed_state = stage_states_.at(request.stage);
        result.geometry_revision = geometry_revision_;
        return result;
    }

    // A non-geometry parameter or a no-op request never changes the geometry
    // revision, but it still commits atomically after parameter validation.
    if (!apply_stage_parameters_locked(request.stage, candidate_state)) {
        result.reason = "failed to apply validated preprocessing parameters";
        return result;
    }
    stages_.at(request.stage)->set_enabled(candidate_state.enabled);
    stage_states_[request.stage] = std::move(candidate_state);
    result.accepted = true;
    result.committed_state = stage_states_.at(request.stage);
    return result;
}

void FilterChain::process(const Metavision::EventCD* begin,
                          const Metavision::EventCD* end,
                          std::vector<Metavision::EventCD>& out) {
    std::lock_guard<std::mutex> lk(chain_mutex());
    out.clear();
    if (plan_.has_enabled_coordinate_stage() && !conditioned_geometry_) {
        // No representable snapshot means no conditioned batch is published.
        return;
    }
    process_locked(begin, end, out);
}

std::optional<ConditionedBatch> FilterChain::process_conditioned(
    const Metavision::EventCD* begin,
    const Metavision::EventCD* end) {
    std::lock_guard<std::mutex> lk(chain_mutex());
    if (!conditioned_geometry_) {
        return std::nullopt;
    }
    ConditionedBatch batch{{}, *conditioned_geometry_};
    process_locked(begin, end, batch.events);
    return batch;
}

void FilterChain::process_locked(const Metavision::EventCD* begin,
                                 const Metavision::EventCD* end,
                                 std::vector<Metavision::EventCD>& out) {
    std::vector<Metavision::EventCD> cur(begin, end);
    std::vector<Metavision::EventCD> next;
    next.reserve(cur.size());
    for (const auto& name : order_) {
        auto* stage = stages_.at(name).get();
        if (!stage || !stage->enabled()) continue;
        next.clear();
        stage->process(cur.data(), cur.data() + cur.size(), next);
        cur.swap(next);
    }
    out = std::move(cur);
}

bool FilterChain::has_enabled() const {
    std::lock_guard<std::mutex> lk(chain_mutex());
    for (const auto& state : stage_states_) {
        if (state.second.enabled) return true;
    }
    return false;
}

} // namespace gui
