// gui/algo_bridge/conditioned_geometry.cpp

#include "conditioned_geometry.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>

namespace gui {
namespace {

bool extent_is_representable(const GeometryExtent& extent) {
    return extent.width > 0 && extent.height > 0 &&
           extent.width <= ConditionedGeometry::kMaxEventExtent &&
           extent.height <= ConditionedGeometry::kMaxEventExtent;
}

bool point_is_within(const GeometryPoint& point, const GeometryExtent& extent) {
    return point.x >= 0 && point.y >= 0 &&
           point.x < extent.width && point.y < extent.height;
}

bool is_known_kind(const CoordinateTransformKind kind) {
    switch (kind) {
        case CoordinateTransformKind::FlipX:
        case CoordinateTransformKind::FlipY:
        case CoordinateTransformKind::Rotate0:
        case CoordinateTransformKind::Rotate90:
        case CoordinateTransformKind::Rotate180:
        case CoordinateTransformKind::Rotate270:
        case CoordinateTransformKind::Transpose:
        case CoordinateTransformKind::Rescale:
            return true;
    }
    return false;
}

bool is_finite_positive(const float value) {
    return std::isfinite(value) && value > 0.0f;
}

std::optional<CoordinateTransformKind> rotation_kind(const OrthogonalRotation rotation) {
    switch (rotation) {
        case OrthogonalRotation::Degrees0:
            return CoordinateTransformKind::Rotate0;
        case OrthogonalRotation::Degrees90:
            return CoordinateTransformKind::Rotate90;
        case OrthogonalRotation::Degrees180:
            return CoordinateTransformKind::Rotate180;
        case OrthogonalRotation::Degrees270:
            return CoordinateTransformKind::Rotate270;
    }
    return std::nullopt;
}

bool derive_scaled_extent(const int input_extent, const float scale, int& output_extent) {
    if (input_extent <= 0 || !is_finite_positive(scale)) {
        return false;
    }

    // Preserve the exact float expression used by OpenEB's producer. Check
    // its result before conversion to int so non-finite or out-of-range
    // products fail closed rather than overflowing EventCD coordinates.
    const float producer_coordinate =
        static_cast<float>(input_extent - 1) * scale + (scale < 1.0f ? 0.0f : 0.5f);
    if (!std::isfinite(producer_coordinate) || producer_coordinate < 0.0f ||
        producer_coordinate >= static_cast<float>(ConditionedGeometry::kMaxEventExtent)) {
        return false;
    }
    const int maximum = static_cast<int>(producer_coordinate);
    if (maximum < 0 || maximum >= ConditionedGeometry::kMaxEventExtent) {
        return false;
    }
    output_extent = maximum + 1;
    return true;
}

bool derive_next_extent(const GeometryExtent& input,
                        const CoordinateTransformStep& step,
                        GeometryExtent& output,
                        std::string& reason) {
    if (!is_known_kind(step.kind)) {
        reason = "unsupported coordinate transform kind";
        return false;
    }
    switch (step.kind) {
        case CoordinateTransformKind::FlipX:
        case CoordinateTransformKind::FlipY:
        case CoordinateTransformKind::Rotate0:
        case CoordinateTransformKind::Rotate180:
            output = input;
            return true;
        case CoordinateTransformKind::Rotate90:
        case CoordinateTransformKind::Rotate270:
        case CoordinateTransformKind::Transpose:
            output = {input.height, input.width};
            return true;
        case CoordinateTransformKind::Rescale:
            if (!is_finite_positive(step.scale_x) || !is_finite_positive(step.scale_y)) {
                reason = "rescale factors must be finite and positive";
                return false;
            }
            if (!derive_scaled_extent(input.width, step.scale_x, output.width) ||
                !derive_scaled_extent(input.height, step.scale_y, output.height)) {
                reason = "rescale output extent is not EventCD-representable";
                return false;
            }
            return true;
    }
    reason = "unsupported coordinate transform kind";
    return false;
}

int scale_coordinate(const int coordinate, const float scale) {
    return static_cast<int>(static_cast<float>(coordinate) * scale +
                            (scale < 1.0f ? 0.0f : 0.5f));
}

GeometryPoint map_point_through_step(GeometryPoint point,
                                     const GeometryExtent& input,
                                     const CoordinateTransformStep& step) {
    switch (step.kind) {
        case CoordinateTransformKind::FlipX:
            point.x = input.width - 1 - point.x;
            break;
        case CoordinateTransformKind::FlipY:
            point.y = input.height - 1 - point.y;
            break;
        case CoordinateTransformKind::Rotate0:
            break;
        case CoordinateTransformKind::Rotate90: {
            const int old_x = point.x;
            point.x = input.height - 1 - point.y;
            point.y = old_x;
            break;
        }
        case CoordinateTransformKind::Rotate180:
            point.x = input.width - 1 - point.x;
            point.y = input.height - 1 - point.y;
            break;
        case CoordinateTransformKind::Rotate270: {
            const int old_x = point.x;
            point.x = point.y;
            point.y = input.width - 1 - old_x;
            break;
        }
        case CoordinateTransformKind::Transpose:
            std::swap(point.x, point.y);
            break;
        case CoordinateTransformKind::Rescale:
            point.x = scale_coordinate(point.x, step.scale_x);
            point.y = scale_coordinate(point.y, step.scale_y);
            break;
    }
    return point;
}

GeometryRect reverse_rectangle_through_step(GeometryRect output_rectangle,
                                            const GeometryExtent& input,
                                            const CoordinateTransformStep& step) {
    switch (step.kind) {
        case CoordinateTransformKind::FlipX:
            return {input.width - output_rectangle.x1, output_rectangle.y0,
                    input.width - output_rectangle.x0, output_rectangle.y1};
        case CoordinateTransformKind::FlipY:
            return {output_rectangle.x0, input.height - output_rectangle.y1,
                    output_rectangle.x1, input.height - output_rectangle.y0};
        case CoordinateTransformKind::Rotate0:
            return output_rectangle;
        case CoordinateTransformKind::Rotate90:
            return {output_rectangle.y0, input.height - output_rectangle.x1,
                    output_rectangle.y1, input.height - output_rectangle.x0};
        case CoordinateTransformKind::Rotate180:
            return {input.width - output_rectangle.x1,
                    input.height - output_rectangle.y1,
                    input.width - output_rectangle.x0,
                    input.height - output_rectangle.y0};
        case CoordinateTransformKind::Rotate270:
            return {input.width - output_rectangle.y1, output_rectangle.x0,
                    input.width - output_rectangle.y0, output_rectangle.x1};
        case CoordinateTransformKind::Transpose:
            return {output_rectangle.y0, output_rectangle.x0,
                    output_rectangle.y1, output_rectangle.x1};
        case CoordinateTransformKind::Rescale:
            break;
    }

    // A lower-bound search preserves the producer's float rounding exactly
    // while remaining conservative for downscale many-to-one mapping.
    const auto inverse_axis = [](const int input_extent, const float scale,
                                 const int output_begin, const int output_end) {
        const auto lower_bound = [input_extent, scale](const int value) {
            int lo = 0;
            int hi = input_extent;
            while (lo < hi) {
                const int mid = lo + (hi - lo) / 2;
                if (scale_coordinate(mid, scale) < value) {
                    lo = mid + 1;
                } else {
                    hi = mid;
                }
            }
            return lo;
        };
        return std::pair<int, int>{lower_bound(output_begin), lower_bound(output_end)};
    };

    const auto x = inverse_axis(input.width, step.scale_x,
                                output_rectangle.x0, output_rectangle.x1);
    const auto y = inverse_axis(input.height, step.scale_y,
                                output_rectangle.y0, output_rectangle.y1);
    return {x.first, y.first, x.second, y.second};
}

} // namespace

bool GeometryRect::is_within(const GeometryExtent& extent) const {
    return is_non_inverted() && x0 >= 0 && y0 >= 0 &&
           x1 <= extent.width && y1 <= extent.height;
}

std::optional<std::vector<CoordinateTransformStep>> ConditionedTransformPlan::ordered_steps(
    std::string* reason) const {
    const auto fail = [reason](const std::string& text)
        -> std::optional<std::vector<CoordinateTransformStep>> {
        if (reason) *reason = text;
        return std::nullopt;
    };

    std::vector<CoordinateTransformStep> steps;
    if (flip_x_enabled) steps.push_back({CoordinateTransformKind::FlipX});
    if (flip_y_enabled) steps.push_back({CoordinateTransformKind::FlipY});
    if (rotate_enabled) {
        const auto kind = rotation_kind(rotation);
        if (!kind) return fail("rotation must be 0, 90, 180, or 270 degrees");
        steps.push_back({*kind});
    }
    if (transpose_enabled) steps.push_back({CoordinateTransformKind::Transpose});
    if (rescale_enabled) {
        if (!is_finite_positive(scale_x) || !is_finite_positive(scale_y)) {
            return fail("rescale factors must be finite and positive");
        }
        steps.push_back({CoordinateTransformKind::Rescale, scale_x, scale_y});
    }
    return steps;
}

bool ConditionedTransformPlan::has_enabled_coordinate_stage() const {
    return flip_x_enabled || flip_y_enabled || rotate_enabled || transpose_enabled || rescale_enabled;
}

bool ConditionedTransformPlan::has_non_identity_coordinate_mapping() const {
    if (flip_x_enabled || flip_y_enabled || transpose_enabled) return true;
    if (rotate_enabled && rotation != OrthogonalRotation::Degrees0) return true;
    return rescale_enabled && (scale_x != 1.0f || scale_y != 1.0f);
}

ConditionedGeometry::ConditionedGeometry(
    const GeometryExtent raw_extent,
    const GeometryExtent output_extent,
    std::vector<CoordinateTransformStep> ordered_steps,
    std::vector<GeometryExtent> step_input_extents,
    const GeometryRevision revision) :
    raw_extent_(raw_extent),
    output_extent_(output_extent),
    ordered_steps_(std::move(ordered_steps)),
    step_input_extents_(std::move(step_input_extents)),
    revision_(revision) {}

std::optional<ConditionedGeometry> ConditionedGeometry::create_from_ordered_steps(
    const GeometryExtent raw_extent,
    std::vector<CoordinateTransformStep> ordered_steps,
    const GeometryRevision revision,
    std::string* reason) {
    const auto fail = [reason](const std::string& text) -> std::optional<ConditionedGeometry> {
        if (reason) *reason = text;
        return std::nullopt;
    };
    if (!extent_is_representable(raw_extent)) {
        return fail("raw source extent must be positive and EventCD-representable");
    }
    if (revision == 0) {
        return fail("geometry revision must be non-zero");
    }

    GeometryExtent current = raw_extent;
    std::vector<GeometryExtent> step_input_extents;
    step_input_extents.reserve(ordered_steps.size());
    for (const auto& step : ordered_steps) {
        step_input_extents.push_back(current);
        GeometryExtent next;
        std::string failure;
        if (!derive_next_extent(current, step, next, failure) || !extent_is_representable(next)) {
            return fail(failure.empty() ? "derived output extent is not representable" : failure);
        }
        current = next;
    }

    return ConditionedGeometry(raw_extent, current, std::move(ordered_steps),
                               std::move(step_input_extents), revision);
}

std::optional<ConditionedGeometry> ConditionedGeometry::create(
    const GeometryExtent raw_extent,
    const ConditionedTransformPlan& plan,
    const GeometryRevision revision,
    std::string* reason) {
    std::string plan_reason;
    auto steps = plan.ordered_steps(&plan_reason);
    if (!steps) {
        if (reason) *reason = plan_reason;
        return std::nullopt;
    }
    return create_from_ordered_steps(raw_extent, std::move(*steps), revision, reason);
}

bool ConditionedGeometry::has_coordinate_changing_step() const {
    return std::any_of(ordered_steps_.begin(), ordered_steps_.end(),
                       [](const CoordinateTransformStep& step) {
                           switch (step.kind) {
                               case CoordinateTransformKind::Rotate0:
                                   return false;
                               case CoordinateTransformKind::FlipX:
                               case CoordinateTransformKind::FlipY:
                               case CoordinateTransformKind::Rotate90:
                               case CoordinateTransformKind::Rotate180:
                               case CoordinateTransformKind::Rotate270:
                               case CoordinateTransformKind::Transpose:
                                   return true;
                               case CoordinateTransformKind::Rescale:
                                   return step.scale_x != 1.0f || step.scale_y != 1.0f;
                           }
                           return true;
                       });
}

bool ConditionedGeometry::has_non_identity_coordinate_plan() const {
    return std::any_of(ordered_steps_.begin(), ordered_steps_.end(),
                       [](const CoordinateTransformStep& step) {
                           switch (step.kind) {
                               case CoordinateTransformKind::Rotate0:
                                   return false;
                               case CoordinateTransformKind::Rescale:
                                   return step.scale_x != 1.0f || step.scale_y != 1.0f;
                               case CoordinateTransformKind::FlipX:
                               case CoordinateTransformKind::FlipY:
                               case CoordinateTransformKind::Rotate90:
                               case CoordinateTransformKind::Rotate180:
                               case CoordinateTransformKind::Rotate270:
                               case CoordinateTransformKind::Transpose:
                                   return true;
                           }
                           return true;
                       });
}

std::optional<GeometryPoint> ConditionedGeometry::map_raw_point(GeometryPoint point) const {
    if (!point_is_within(point, raw_extent_)) {
        return std::nullopt;
    }
    for (std::size_t i = 0; i < ordered_steps_.size(); ++i) {
        point = map_point_through_step(point, step_input_extents_[i], ordered_steps_[i]);
    }
    if (!point_is_within(point, output_extent_)) {
        return std::nullopt;
    }
    return point;
}

std::optional<GeometryRect> ConditionedGeometry::map_raw_rectangle(
    const GeometryRect rectangle) const {
    if (!rectangle.is_within(raw_extent_) || rectangle.is_empty()) {
        return std::nullopt;
    }
    const GeometryPoint corners[] = {
        {rectangle.x0, rectangle.y0},
        {rectangle.x1 - 1, rectangle.y0},
        {rectangle.x0, rectangle.y1 - 1},
        {rectangle.x1 - 1, rectangle.y1 - 1},
    };
    int min_x = std::numeric_limits<int>::max();
    int min_y = std::numeric_limits<int>::max();
    int max_x = std::numeric_limits<int>::min();
    int max_y = std::numeric_limits<int>::min();
    for (const auto& corner : corners) {
        const auto mapped = map_raw_point(corner);
        if (!mapped) return std::nullopt;
        min_x = std::min(min_x, mapped->x);
        min_y = std::min(min_y, mapped->y);
        max_x = std::max(max_x, mapped->x);
        max_y = std::max(max_y, mapped->y);
    }
    GeometryRect result{min_x, min_y, max_x + 1, max_y + 1};
    return result.is_within(output_extent_) ? std::optional<GeometryRect>(result) : std::nullopt;
}

std::optional<GeometryRect> ConditionedGeometry::map_output_rectangle_to_raw_cover(
    GeometryRect rectangle) const {
    if (!rectangle.is_within(output_extent_)) {
        return std::nullopt;
    }
    for (std::size_t i = ordered_steps_.size(); i > 0; --i) {
        rectangle = reverse_rectangle_through_step(rectangle, step_input_extents_[i - 1],
                                                   ordered_steps_[i - 1]);
    }
    return rectangle.is_within(raw_extent_) ? std::optional<GeometryRect>(rectangle) : std::nullopt;
}

bool ConditionedGeometry::same_mapping_as(const ConditionedGeometry& other) const {
    return raw_extent_ == other.raw_extent_ && ordered_steps_ == other.ordered_steps_;
}

} // namespace gui
