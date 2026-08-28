// gui/algo_bridge/conditioned_geometry.h
//
// Immutable geometry snapshot for a typed, discrete FilterChain plan.  All
// rectangles use half-open coordinates: [x0, x1) x [y0, y1).

#ifndef GUI_ALGO_BRIDGE_CONDITIONED_GEOMETRY_H
#define GUI_ALGO_BRIDGE_CONDITIONED_GEOMETRY_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace gui {

using GeometryRevision = std::uint64_t;

struct GeometryExtent {
    int width{0};
    int height{0};

    bool operator==(const GeometryExtent& other) const {
        return width == other.width && height == other.height;
    }
    bool operator!=(const GeometryExtent& other) const { return !(*this == other); }
};

struct GeometryPoint {
    int x{0};
    int y{0};

    bool operator==(const GeometryPoint& other) const {
        return x == other.x && y == other.y;
    }
};

/// A rectangle in the named coordinate space.  It may be empty, but never
/// inverted; an empty result is meaningful when an upscaled output selection
/// has no contributing raw pixels.
struct GeometryRect {
    int x0{0};
    int y0{0};
    int x1{0};
    int y1{0};

    bool is_non_inverted() const { return x0 <= x1 && y0 <= y1; }
    bool is_empty() const { return x0 == x1 || y0 == y1; }
    bool is_within(const GeometryExtent& extent) const;

    bool operator==(const GeometryRect& other) const {
        return x0 == other.x0 && y0 == other.y0 && x1 == other.x1 && y1 == other.y1;
    }
};

/// The bounded U1 typed mapping vocabulary.  Arbitrary-radian rotation is
/// intentionally absent: it is not part of the U1C1 conditioned contract.
enum class CoordinateTransformKind {
    FlipX,
    FlipY,
    Rotate0,
    Rotate90,
    Rotate180,
    Rotate270,
    Transpose,
    Rescale,
};

struct CoordinateTransformStep {
    CoordinateTransformKind kind{CoordinateTransformKind::Rotate0};
    /// Used only by Rescale.  The values intentionally remain float so the
    /// core matches OpenEB EventRescalerAlgorithm producer semantics.
    float scale_x{1.0f};
    float scale_y{1.0f};

    bool operator==(const CoordinateTransformStep& other) const {
        return kind == other.kind && scale_x == other.scale_x && scale_y == other.scale_y;
    }
};

enum class OrthogonalRotation {
    Degrees0 = 0,
    Degrees90 = 90,
    Degrees180 = 180,
    Degrees270 = 270,
};

/// The current FilterChain's deliberately small typed plan.  Its field layout
/// captures enablement as well as mapping so a flip/180/identity mutation can
/// receive a fresh geometry revision even when the output extent is unchanged.
struct ConditionedTransformPlan {
    bool flip_x_enabled{false};
    bool flip_y_enabled{false};
    bool rotate_enabled{false};
    OrthogonalRotation rotation{OrthogonalRotation::Degrees0};
    bool transpose_enabled{false};
    bool rescale_enabled{false};
    float scale_x{1.0f};
    float scale_y{1.0f};

    /// Produces steps only in the actual FilterChain execution order.  A
    /// caller cannot manufacture a reversed runtime order through this plan.
    std::optional<std::vector<CoordinateTransformStep>> ordered_steps(
        std::string* reason = nullptr) const;

    /// Whether a coordinate-stage is enabled, including an identity-valued
    /// Rotate0 or Rescale 1x1 stage.  This supports revision accounting.
    bool has_enabled_coordinate_stage() const;

    /// Whether the plan changes coordinate mapping.  This is intentionally
    /// distinct from extent change: flips and Rotate180 are non-identity.
    bool has_non_identity_coordinate_mapping() const;
};

/// A validated, immutable mapping from Raw Source Space to Conditioned Output
/// Space.  It is a value object: callers receive snapshots, not mutable
/// shared geometry state.
class ConditionedGeometry {
public:
    /// EventCD coordinates are unsigned short, so no output axis may exceed
    /// 65536 addressable positions.  FilterChain adds any stricter producer
    /// limits required by the current OpenEB algorithms.
    static constexpr int kMaxEventExtent = 65536;

    /// Builds a geometry snapshot only when raw extent, the current typed
    /// FilterChain plan, and its derived output extent are representable. On
    /// failure, returns nullopt and writes a human-readable reason when @p
    /// reason is non-null.
    static std::optional<ConditionedGeometry> create(
        GeometryExtent raw_extent,
        const ConditionedTransformPlan& plan,
        GeometryRevision revision,
        std::string* reason = nullptr);

    GeometryExtent raw_extent() const { return raw_extent_; }
    GeometryExtent output_extent() const { return output_extent_; }
    GeometryRevision revision() const { return revision_; }
    const std::vector<CoordinateTransformStep>& ordered_steps() const { return ordered_steps_; }

    /// True for a plan containing a non-identity coordinate mapping. Rotate0
    /// and Rescale 1x1 are intentionally not counted; an enabled flip remains
    /// non-identity even on a degenerate one-pixel source.
    bool has_coordinate_changing_step() const;

    /// True when the plan has no coordinate-changing step.  It is used for
    /// the interim processed-recording policy, which is deliberately stricter
    /// than extent comparison alone.
    bool has_non_identity_coordinate_plan() const;

    /// Maps one valid raw-space pixel coordinate into Conditioned Output
    /// Space.  Out-of-bounds raw inputs return nullopt rather than being
    /// clipped or silently wrapped.
    std::optional<GeometryPoint> map_raw_point(GeometryPoint point) const;

    /// Maps a non-empty, raw-space half-open rectangle to the smallest
    /// enclosing half-open output rectangle for the discrete plan.
    std::optional<GeometryRect> map_raw_rectangle(GeometryRect rectangle) const;

    /// Maps a non-inverted, output-space half-open rectangle back to the raw
    /// rectangle that covers every raw pixel contributing to that selection.
    /// The result can be empty for an output-only gap created by upscaling.
    std::optional<GeometryRect> map_output_rectangle_to_raw_cover(
        GeometryRect rectangle) const;

    /// Equality excluding revision, useful when an owner decides whether a
    /// candidate plan requires a new immutable revision.
    bool same_mapping_as(const ConditionedGeometry& other) const;

private:
    /// Internal construction path used by the bounded typed plan. Keeping it
    /// private prevents callers from creating a runtime-impossible reversed,
    /// duplicated, or otherwise non-FilterChain step order.
    static std::optional<ConditionedGeometry> create_from_ordered_steps(
        GeometryExtent raw_extent,
        std::vector<CoordinateTransformStep> ordered_steps,
        GeometryRevision revision,
        std::string* reason = nullptr);

    ConditionedGeometry(GeometryExtent raw_extent,
                        GeometryExtent output_extent,
                        std::vector<CoordinateTransformStep> ordered_steps,
                        std::vector<GeometryExtent> step_input_extents,
                        GeometryRevision revision);

    GeometryExtent raw_extent_;
    GeometryExtent output_extent_;
    std::vector<CoordinateTransformStep> ordered_steps_;
    /// Extent immediately before each corresponding ordered step.  Retained
    /// so reverse rectangle mapping never guesses an intermediate geometry.
    std::vector<GeometryExtent> step_input_extents_;
    GeometryRevision revision_{0};
};

} // namespace gui

#endif // GUI_ALGO_BRIDGE_CONDITIONED_GEOMETRY_H
