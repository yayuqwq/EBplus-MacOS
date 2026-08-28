// gui/algo_bridge/filter_chain.h — sequential chain of OpenEB event filters
// (design §4.3.1), wrapped behind a uniform interface so the GUI can enable /
// disable / re-parameterize each stage without depending on the concrete
// algorithm headers.
//
// Supported stages: polarity filter, polarity invert, flip X/Y, rotate,
// transpose, rescale. Each stage is identified by the same name
// used in AlgoBridge::registry_ (e.g. "polarity_filter").

#ifndef GUI_ALGO_BRIDGE_FILTER_CHAIN_H
#define GUI_ALGO_BRIDGE_FILTER_CHAIN_H

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <metavision/sdk/base/events/event_cd.h>

#include "conditioned_geometry.h"

namespace gui {

/// @brief One configurable stage in the preprocessing filter chain.
class FilterStage {
public:
    virtual ~FilterStage() = default;
    /// @brief Process the input range, appending to @p out.
    virtual void process(const Metavision::EventCD* begin,
                         const Metavision::EventCD* end,
                         std::vector<Metavision::EventCD>& out) = 0;
    /// @brief Set a named parameter; returns false if unknown.
    virtual bool set_param(const std::string& key, const std::string& value) = 0;
    virtual bool enabled() const { return enabled_; }
    virtual void set_enabled(bool e) { enabled_ = e; }
    virtual std::string name() const = 0;

protected:
    bool enabled_{false};
};

/// Complete desired state for one stage.  Supplying the stage's relevant
/// parameters in the same request makes admission atomic: a rejected rotate or
/// rescale request cannot leave a checkbox enabled with stale backend state.
struct FilterStageRequest {
    std::string stage;
    bool enabled{false};
    std::unordered_map<std::string, std::string> parameters;
};

struct FilterStageState {
    bool enabled{false};
    std::unordered_map<std::string, std::string> parameters;
};

/// Cross-owner facts needed by U1C1 containment. Geometry itself remains
/// Qt-free; CameraController owns the transitions that update this snapshot.
struct FilterAdmissionContext {
    bool raw_roi_or_roni_active{false};
    bool processed_recording_active{false};
};

struct FilterAdmissionResult {
    bool accepted{false};
    std::string reason;
    FilterStageState committed_state;
    GeometryRevision geometry_revision{0};
};

/// Events and the immutable geometry snapshot that was active while they were
/// conditioned. A returned batch may contain zero events; std::nullopt means
/// no valid snapshot exists and the caller must fail closed.
struct ConditionedBatch {
    std::vector<Metavision::EventCD> events;
    const ConditionedGeometry geometry;
};

/// @brief Ordered chain of event filters applied left-to-right.
class FilterChain {
public:
    FilterChain();

    /// @brief Sets the raw source geometry and atomically derives a new
    /// conditioned snapshot.  False means the candidate is not representable;
    /// coordinate-conditioned batches then fail closed until a valid source
    /// geometry is installed.
    bool set_geometry(int width, int height);

    /// @brief Returns the named stage for read-only presence queries. The
    /// const pointer deliberately prevents callers from bypassing U1C1
    /// admission by mutating FilterStage directly.
    const FilterStage* stage(const std::string& name) const;

    /// @brief Compatibility wrappers for a single stage mutation. They use the
    /// same fail-closed admission policy as the transactional request API.
    bool set_stage_enabled(const std::string& name, bool enabled);
    bool set_stage_param(const std::string& name, const std::string& key,
                         const std::string& value);
    bool is_stage_enabled(const std::string& name) const;

    /// @brief Atomically validates and commits a complete stage request. On
    /// failure neither stage enablement nor parameters nor geometry revision
    /// change. This is the runtime admission path used by PreprocessingPanel.
    FilterAdmissionResult try_apply_stage(const FilterStageRequest& request);

    /// @brief Returns the committed stage snapshot so callers can roll UI
    /// controls back to actual backend state after a rejected request.
    std::optional<FilterStageState> stage_state(const std::string& name) const;

    /// @brief Current immutable geometry snapshot. It is returned by value so
    /// callers cannot observe later mutation through a shared object.
    std::optional<ConditionedGeometry> conditioned_geometry() const;

    /// @brief U1C1 reverse-order admission checks. These are used when ROI or
    /// processed recording is requested after transforms are already active.
    bool can_activate_raw_roi_or_roni(std::string* reason = nullptr) const;
    bool can_start_processed_recording(std::string* reason = nullptr) const;
    /// Commits a context transition only if it does not create a forbidden
    /// active combination. These methods make direct FilterChain callers obey
    /// the same live context as the panel path.
    bool try_set_raw_roi_or_roni_active(bool active, std::string* reason = nullptr);
    bool try_set_processed_recording_active(bool active, std::string* reason = nullptr);
    bool has_non_identity_coordinate_plan() const;

    /// @brief Applies all enabled stages in order.
    void process(const Metavision::EventCD* begin,
                 const Metavision::EventCD* end,
                 std::vector<Metavision::EventCD>& out);

    /// @brief Applies the chain and returns events paired atomically with the
    /// exact immutable geometry revision used for that batch. This is the
    /// U1C1 core API for later U1C2/U1C3 consumer migration; legacy callers
    /// may continue using process() while unsafe extent-changing plans remain
    /// gated.
    std::optional<ConditionedBatch> process_conditioned(
        const Metavision::EventCD* begin,
        const Metavision::EventCD* end);

    /// @brief True if at least one stage is enabled.
    bool has_enabled() const;

private:
    FilterAdmissionResult try_apply_stage_locked(const FilterStageRequest& request);
    bool rebuild_conditioned_geometry_locked(std::string* reason = nullptr);
    ConditionedTransformPlan candidate_plan_for_request_locked(
        const FilterStageRequest& request, bool& valid, std::string& reason) const;
    bool apply_stage_parameters_locked(const std::string& stage,
                                       const FilterStageState& state);
    void process_locked(const Metavision::EventCD* begin,
                        const Metavision::EventCD* end,
                        std::vector<Metavision::EventCD>& out);

    int width_{0};
    int height_{0};
    std::unordered_map<std::string, std::unique_ptr<FilterStage>> stages_;
    std::unordered_map<std::string, FilterStageState> stage_states_;
    std::vector<std::string> order_;
    ConditionedTransformPlan plan_;
    std::optional<ConditionedGeometry> conditioned_geometry_;
    GeometryRevision geometry_revision_{0};
    FilterAdmissionContext admission_context_;
};

} // namespace gui

#endif // GUI_ALGO_BRIDGE_FILTER_CHAIN_H
