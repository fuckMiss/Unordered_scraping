#include "grab_limit_evaluator.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <sstream>
#include <utility>

namespace {

std::string HeadTypeText(int head_type_code)
{
    switch (head_type_code) {
    case 1: return "大上右";
    case 2: return "小上左";
    case 3: return "大上左";
    case 4: return "小上右";
    default: return "未知";
    }
}

bool IsValueWithinRange(float value, const LimitRange& range)
{
    return value >= range.lower && value <= range.upper;
}

bool InnerLimitRange(const LimitRange& range, double roi_margin, double* lower, double* upper)
{
    if (lower == nullptr || upper == nullptr || !std::isfinite(roi_margin) || roi_margin < 0.0) {
        return false;
    }

    *lower = range.lower + roi_margin;
    *upper = range.upper - roi_margin;
    return *lower <= *upper;
}

std::string PointText(const cv::Point2f& point)
{
    std::ostringstream stream;
    stream.setf(std::ios::fixed);
    stream.precision(2);
    stream << "(" << point.x << "," << point.y << ")";
    return stream.str();
}

double MachineDistancePerImagePixel(const CoordinateTransformState& state,
                                    const cv::Point2f& image_center,
                                    const cv::Point2f& image_direction)
{
    cv::Point2f machine_center;
    cv::Point2f machine_offset;
    if (!TransformImagePointToMachine(state, image_center, &machine_center) ||
        !TransformImagePointToMachine(state, image_center + image_direction, &machine_offset)) {
        return 0.0;
    }

    const cv::Point2f delta = machine_offset - machine_center;
    return std::hypot(delta.x, delta.y);
}

bool SegmentMaskValid(const SegRegion& segment, int image_width, int image_height)
{
    return !segment.mask.empty() &&
           segment.mask.type() == CV_8U &&
           segment.mask.cols == image_width &&
           segment.mask.rows == image_height;
}

float PointLength(const cv::Point2f& point)
{
    return std::hypot(point.x, point.y);
}

float QuantizeStable(float value, float step)
{
    if (!std::isfinite(value) || step <= 0.0f) {
        return value;
    }
    return std::round(value / step) * step;
}

float ComputeRayAngleDeg(const cv::Point2f& start, const cv::Point2f& end)
{
    const cv::Point2f ray = end - start;
    float angle = std::atan2(ray.y, ray.x) * 180.0f / static_cast<float>(CV_PI);
    if (angle < 0.0f) {
        angle += 360.0f;
    }
    return angle;
}

cv::Point2f OffsetPointAlongRay(const cv::Point2f& ray_start,
                                const cv::Point2f& ray_end,
                                double offset_px)
{
    const cv::Point2f ray = ray_end - ray_start;
    const float length = PointLength(ray);
    if (length < 1e-6f || std::fabs(offset_px) < 1e-6) {
        return ray_start;
    }

    const float scale = static_cast<float>(offset_px) / length;
    return ray_start + ray * scale;
}

cv::Point2f ClosestPointOnSegment(const cv::Point2f& point,
                                  const cv::Point2f& start,
                                  const cv::Point2f& end)
{
    const cv::Point2f edge = end - start;
    const float length_squared = edge.dot(edge);
    if (length_squared < 1e-6f) {
        return start;
    }

    const float t = std::max(0.0f, std::min(1.0f, (point - start).dot(edge) / length_squared));
    return start + edge * t;
}

bool FinalizePoseFromMechanicalGripper(PoseDetection& detection,
                                       const MechanicalGripperCollisionConfig& config,
                                       int detection_index)
{
    if (detection.mechanical_gripper_corners.size() != 4) {
        return false;
    }

    const cv::Point2f ray_basis = detection.obb_center - detection.seg_center;
    const float ray_basis_length = PointLength(ray_basis);
    if (ray_basis_length < 1e-6f) {
        if (config.debug_logging_enabled) {
            std::cout << "[PostprocessDebug] mechanical_gripper_reject"
                      << " index=" << detection_index
                      << " reason=invalid_oa_geometry"
                      << " O=" << PointText(detection.seg_center)
                      << " A=" << PointText(detection.obb_center)
                      << std::endl;
        }
        return false;
    }

    const cv::Point2f ray_unit = ray_basis * (1.0f / ray_basis_length);
    float best_dot = -std::numeric_limits<float>::infinity();
    cv::Point2f best_foot = detection.obb_center;
    for (size_t i = 0; i < detection.mechanical_gripper_corners.size(); ++i) {
        const cv::Point2f& start = detection.mechanical_gripper_corners[i];
        const cv::Point2f& end = detection.mechanical_gripper_corners[(i + 1) % detection.mechanical_gripper_corners.size()];
        const cv::Point2f foot = ClosestPointOnSegment(detection.obb_center, start, end);
        const cv::Point2f ac = foot - detection.obb_center;
        const float ac_length = PointLength(ac);
        if (ac_length < 1e-6f) {
            continue;
        }

        const float dot = ray_unit.dot(ac * (1.0f / ac_length));
        if (dot > best_dot) {
            best_dot = dot;
            best_foot = foot;
        }
    }

    if (!std::isfinite(best_dot) || PointLength(best_foot - detection.obb_center) < 1e-6f) {
        if (config.debug_logging_enabled) {
            std::cout << "[PostprocessDebug] mechanical_gripper_reject"
                      << " index=" << detection_index
                      << " reason=invalid_mechanical_ac_geometry"
                      << " O=" << PointText(detection.seg_center)
                      << " A=" << PointText(detection.obb_center)
                      << std::endl;
        }
        return false;
    }

    constexpr float kCoordinateStepPx = 0.01f;
    constexpr float kAngleStepDeg = 0.01f;
    const cv::Point2f shifted_center =
        OffsetPointAlongRay(detection.obb_center, best_foot, config.center_ray_offset_px);
    detection.arrow_start = detection.obb_center;
    detection.arrow_end = best_foot;
    detection.center_x = QuantizeStable(shifted_center.x, kCoordinateStepPx);
    detection.center_y = QuantizeStable(shifted_center.y, kCoordinateStepPx);
    detection.angle_deg = QuantizeStable(ComputeRayAngleDeg(detection.arrow_start, detection.arrow_end),
                                         kAngleStepDeg);
    if (config.debug_logging_enabled) {
        std::cout << "[PostprocessDebug] mechanical_gripper_pose"
                  << " index=" << detection_index
                  << " O=" << PointText(detection.seg_center)
                  << " A=" << PointText(detection.arrow_start)
                  << " C=" << PointText(detection.arrow_end)
                  << " angle_deg=" << detection.angle_deg
                  << " center=(" << detection.center_x << "," << detection.center_y << ")"
                  << " center_ray_offset_px=" << config.center_ray_offset_px
                  << std::endl;
    }
    return true;
}

bool GripperCornersTouchSegmentMask(const std::vector<cv::Point2f>& corners,
                                    const SegRegion& matched_segment,
                                    const SegRegion& segment,
                                    int image_width,
                                    int image_height,
                                    int detection_index,
                                    int segment_index,
                                    bool debug_enabled,
                                    std::vector<std::pair<cv::Rect, cv::Mat>>* collisions)
{
    if (corners.size() < 3 || segment.mask.empty()) {
        return false;
    }
    if (!SegmentMaskValid(segment, image_width, image_height)) {
        if (debug_enabled) {
            std::cout << "[PostprocessDebug] mechanical_gripper_collision_skip"
                      << " index=" << detection_index
                      << " segment_index=" << segment_index
                      << " reason=invalid_mask"
                      << " mask_size=" << segment.mask.cols << "x" << segment.mask.rows
                      << " image_size=" << image_width << "x" << image_height
                      << std::endl;
        }
        return false;
    }
    if (!SegmentMaskValid(matched_segment, image_width, image_height)) {
        if (debug_enabled) {
            std::cout << "[PostprocessDebug] mechanical_gripper_collision_skip"
                      << " index=" << detection_index
                      << " segment_index=" << segment_index
                      << " reason=invalid_matched_mask"
                      << " matched_mask_size=" << matched_segment.mask.cols << "x" << matched_segment.mask.rows
                      << " image_size=" << image_width << "x" << image_height
                      << std::endl;
        }
        return false;
    }

    const cv::Rect image_rect(0, 0, image_width, image_height);
    const cv::Rect roi = cv::boundingRect(corners) & image_rect;
    if (roi.empty()) {
        return false;
    }

    std::vector<cv::Point> polygon;
    polygon.reserve(corners.size());
    for (const cv::Point2f& point : corners) {
        polygon.emplace_back(cvRound(point.x) - roi.x, cvRound(point.y) - roi.y);
    }

    cv::Mat gripper_mask(roi.height, roi.width, CV_8U, cv::Scalar(0));
    cv::fillConvexPoly(gripper_mask, polygon, cv::Scalar(255), cv::LINE_8);

    cv::Mat overlap;
    cv::bitwise_and(gripper_mask, segment.mask(roi), overlap);
    overlap.setTo(cv::Scalar(0), matched_segment.mask(roi));
    const int overlap_pixels = cv::countNonZero(overlap);
    if (overlap_pixels <= 0) {
        return false;
    }

    if (debug_enabled) {
        std::cout << "[PostprocessDebug] mechanical_gripper_collision"
                  << " index=" << detection_index
                  << " segment_index=" << segment_index
                  << " roi=(" << roi.x << "," << roi.y << "," << roi.width << "," << roi.height << ")"
                  << " overlap_pixels=" << overlap_pixels
                  << std::endl;
    }
    if (collisions != nullptr) {
        collisions->push_back({ roi, overlap.clone() });
    }
    return true;
}

float Cross(const cv::Point2f& first, const cv::Point2f& second)
{
    return first.x * second.y - first.y * second.x;
}

bool PointStrictlyInsideConvexPolygon(const cv::Point2f& point,
                                      const std::vector<cv::Point2f>& polygon)
{
    if (polygon.size() < 3) {
        return false;
    }

    constexpr float kBoundaryEpsilon = 1e-4f;
    float direction = 0.0f;
    for (size_t i = 0; i < polygon.size(); ++i) {
        const cv::Point2f start = polygon[i];
        const cv::Point2f end = polygon[(i + 1) % polygon.size()];
        const float edge_cross = Cross(end - start, point - start);
        if (std::fabs(edge_cross) <= kBoundaryEpsilon) {
            return false;
        }
        if (direction == 0.0f) {
            direction = edge_cross;
            continue;
        }
        if ((direction > 0.0f && edge_cross < 0.0f) ||
            (direction < 0.0f && edge_cross > 0.0f)) {
            return false;
        }
    }
    return true;
}

bool MechanicalGripperFullyInsideProtectionPolygon(const std::vector<cv::Point2f>& gripper_corners,
                                                   const std::vector<cv::Point2f>& protection_polygon)
{
    if (gripper_corners.size() != 4 || protection_polygon.size() != 4) {
        return false;
    }

    for (const cv::Point2f& corner : gripper_corners) {
        if (!PointStrictlyInsideConvexPolygon(corner, protection_polygon)) {
            return false;
        }
    }
    return true;
}

std::string FormatLimitReason(const char* label, float value, const LimitRange& range)
{
    std::ostringstream message;
    message.setf(std::ios::fixed);
    message.precision(1);
    message << label << "超限：" << value << " 不在 ["
            << range.lower << ", " << range.upper << "]";
    return message.str();
}

const char* FrontBackAxisLabel(AxisMappingMode mode)
{
    return mode == AxisMappingMode::FrontBackMachineX ? "前后 / 机械X" : "前后 / 机械Y";
}

const char* LeftRightAxisLabel(AxisMappingMode mode)
{
    return mode == AxisMappingMode::FrontBackMachineX ? "左右 / 机械Y" : "左右 / 机械X";
}

} // namespace

void ResolveResultAfterFiltering(FrameInferenceResult& result)
{
    result.primary_index = -1;
    result.pick_status_code = 3;
    result.head_type_code = 0;
    result.head_type_text = HeadTypeText(0);

    float best_score = -std::numeric_limits<float>::infinity();
    int grabbable_count = 0;
    for (int i = 0; i < static_cast<int>(result.detections.size()); ++i) {
        PoseDetection& detection = result.detections[i];
        if (!detection.can_grab) {
            detection.pick_status_code = 3;
            continue;
        }

        ++grabbable_count;
        const float score = detection.confidence;
        if (score > best_score) {
            best_score = score;
            result.primary_index = i;
        }
    }

    if (grabbable_count == 0) {
        return;
    }

    result.pick_status_code = grabbable_count == 1 ? 1 : 2;
    for (PoseDetection& detection : result.detections) {
        detection.pick_status_code = detection.can_grab ? result.pick_status_code : 3;
    }

    if (result.primary_index >= 0 && result.primary_index < static_cast<int>(result.detections.size())) {
        result.head_type_code = result.detections[result.primary_index].head_type_code;
        result.head_type_text = result.detections[result.primary_index].head_type_text;
    }
}

GrabLimitOverlayPolygon BuildGrabLimitOverlayPolygon(const GrabLimitConfig& limits,
                                                     const CoordinateTransformState& coordinate_state,
                                                     AxisMappingMode axis_mapping_mode)
{
    GrabLimitOverlayPolygon polygon;
    if (!limits.enabled) {
        polygon.reason = "上下限保护未启用。";
        return polygon;
    }
    if (!coordinate_state.enabled || !coordinate_state.valid) {
        polygon.reason = "保护区域需有效坐标转换。";
        return polygon;
    }

    double front_back_lower = 0.0;
    double front_back_upper = 0.0;
    double left_right_lower = 0.0;
    double left_right_upper = 0.0;
    if (!InnerLimitRange(limits.x, limits.roi_margin, &front_back_lower, &front_back_upper) ||
        !InnerLimitRange(limits.y, limits.roi_margin, &left_right_lower, &left_right_upper)) {
        polygon.reason = "ROI 留边后保护区域为空。";
        return polygon;
    }

    const auto machine_point = [axis_mapping_mode](double front_back, double left_right) {
        return axis_mapping_mode == AxisMappingMode::FrontBackMachineX
            ? cv::Point2f(static_cast<float>(front_back), static_cast<float>(left_right))
            : cv::Point2f(static_cast<float>(left_right), static_cast<float>(front_back));
    };

    const std::vector<cv::Point2f> machine_points = {
        machine_point(front_back_lower, left_right_lower),
        machine_point(front_back_upper, left_right_lower),
        machine_point(front_back_upper, left_right_upper),
        machine_point(front_back_lower, left_right_upper),
    };

    polygon.image_points.reserve(machine_points.size());
    for (const cv::Point2f& point : machine_points) {
        cv::Point2f image_point;
        if (!TransformMachinePointToImage(coordinate_state, point, &image_point)) {
            polygon.reason = "保护区域坐标反投影失败。";
            polygon.image_points.clear();
            return polygon;
        }
        polygon.image_points.push_back(image_point);
    }

    polygon.visible = polygon.image_points.size() == 4;
    return polygon;
}

std::vector<cv::Point2f> BuildMechanicalGripperCorners(const PoseDetection& detection,
                                                       const CoordinateTransformState& coordinate_state,
                                                       const MechanicalGripperCollisionConfig& config,
                                                       int detection_index)
{
    const bool invalid_size = config.length <= 0.0 || config.width <= 0.0 ||
                              !std::isfinite(config.length) || !std::isfinite(config.width);
    if (invalid_size || !coordinate_state.enabled || !coordinate_state.valid) {
        if (config.debug_logging_enabled) {
            std::cout << "[PostprocessDebug] mechanical_gripper_skip"
                      << " index=" << detection_index
                      << " reason=" << (invalid_size
                                           ? "invalid_gripper_size"
                                           : (!coordinate_state.enabled ? "coordinate_transform_disabled"
                                                                        : "coordinate_transform_invalid"))
                      << " length=" << config.length
                      << " width=" << config.width
                      << " coordinate_enabled=" << (coordinate_state.enabled ? 1 : 0)
                      << " coordinate_valid=" << (coordinate_state.valid ? 1 : 0)
                      << std::endl;
        }
        return {};
    }

    if (!std::isfinite(detection.grip_long_angle_deg)) {
        if (config.debug_logging_enabled) {
            std::cout << "[PostprocessDebug] mechanical_gripper_skip"
                      << " index=" << detection_index
                      << " reason=invalid_grip_long_angle"
                      << " detection_angle_deg=" << detection.angle_deg
                      << std::endl;
        }
        return {};
    }

    const float angle_rad = static_cast<float>(detection.grip_long_angle_deg * CV_PI / 180.0);
    const cv::Point2f length_unit(std::cos(angle_rad), std::sin(angle_rad));
    const cv::Point2f width_unit(-length_unit.y, length_unit.x);
    const cv::Point2f center = detection.obb_center;

    const double length_machine_per_px = MachineDistancePerImagePixel(coordinate_state, center, length_unit);
    const double width_machine_per_px = MachineDistancePerImagePixel(coordinate_state, center, width_unit);
    if (length_machine_per_px <= 1e-9 ||
        width_machine_per_px <= 1e-9 ||
        !std::isfinite(length_machine_per_px) ||
        !std::isfinite(width_machine_per_px)) {
        if (config.debug_logging_enabled) {
            std::cout << "[PostprocessDebug] mechanical_gripper_skip"
                      << " index=" << detection_index
                      << " reason=invalid_local_scale"
                      << " center=" << PointText(center)
                      << " detection_angle_deg=" << detection.angle_deg
                      << " grip_long_angle_deg=" << detection.grip_long_angle_deg
                      << " length_machine_per_px=" << length_machine_per_px
                      << " width_machine_per_px=" << width_machine_per_px
                      << std::endl;
        }
        return {};
    }

    const cv::Point2f half_length =
        length_unit * static_cast<float>(config.length / length_machine_per_px * 0.5);
    const cv::Point2f half_width =
        width_unit * static_cast<float>(config.width / width_machine_per_px * 0.5);
    std::vector<cv::Point2f> corners = {
        center - half_length - half_width,
        center + half_length - half_width,
        center + half_length + half_width,
        center - half_length + half_width,
    };
    if (config.debug_logging_enabled) {
        std::cout << "[PostprocessDebug] mechanical_gripper"
                  << " index=" << detection_index
                  << " center=" << PointText(center)
                  << " detection_angle_deg=" << detection.angle_deg
                  << " grip_long_angle_deg=" << detection.grip_long_angle_deg
                  << " length=" << config.length
                  << " width=" << config.width
                  << " length_machine_per_px=" << length_machine_per_px
                  << " width_machine_per_px=" << width_machine_per_px
                  << " pixel_length=" << std::hypot(corners[1].x - corners[0].x,
                                                     corners[1].y - corners[0].y)
                  << " pixel_width=" << std::hypot(corners[3].x - corners[0].x,
                                                    corners[3].y - corners[0].y)
                  << " corners=("
                  << corners[0].x << "," << corners[0].y << ";"
                  << corners[1].x << "," << corners[1].y << ";"
                  << corners[2].x << "," << corners[2].y << ";"
                  << corners[3].x << "," << corners[3].y << ")"
                  << std::endl;
    }
    return corners;
}

void ApplyMechanicalGripperCollisionFilter(FrameInferenceResult& result,
                                           const CoordinateTransformState& coordinate_state,
                                           const MechanicalGripperCollisionConfig& config,
                                           const GrabLimitConfig& limits,
                                           AxisMappingMode axis_mapping_mode)
{
    const GrabLimitOverlayPolygon protection_polygon =
        BuildGrabLimitOverlayPolygon(limits, coordinate_state, axis_mapping_mode);
    const bool global_failure = config.length <= 0.0 || config.width <= 0.0 ||
                                !std::isfinite(config.length) || !std::isfinite(config.width) ||
                                !coordinate_state.enabled || !coordinate_state.valid ||
                                !protection_polygon.visible || protection_polygon.image_points.size() != 4;
    if (global_failure && config.debug_logging_enabled) {
        std::cout << "[PostprocessDebug] mechanical_gripper_filter"
                  << " reason=" << ((config.length <= 0.0 || config.width <= 0.0 ||
                                      !std::isfinite(config.length) || !std::isfinite(config.width))
                                         ? "invalid_gripper_size"
                                         : (!coordinate_state.enabled ? "coordinate_transform_disabled"
                                                                      : (!coordinate_state.valid
                                                                             ? "coordinate_transform_invalid"
                                                                             : "invalid_protection_polygon")))
                  << " length=" << config.length
                  << " width=" << config.width
                  << " coordinate_enabled=" << (coordinate_state.enabled ? 1 : 0)
                  << " coordinate_valid=" << (coordinate_state.valid ? 1 : 0)
                  << std::endl;
    }

    for (int i = 0; i < static_cast<int>(result.detections.size()); ++i) {
        PoseDetection& detection = result.detections[i];
        detection.mask_collisions.clear();
        detection.mechanical_gripper_corners =
            BuildMechanicalGripperCorners(detection, coordinate_state, config, i);

        if (global_failure || detection.mechanical_gripper_corners.size() != 4) {
            detection.can_grab = false;
            continue;
        }
        if (!FinalizePoseFromMechanicalGripper(detection, config, i)) {
            detection.can_grab = false;
            continue;
        }
        if (!MechanicalGripperFullyInsideProtectionPolygon(detection.mechanical_gripper_corners,
                                                           protection_polygon.image_points)) {
            if (config.debug_logging_enabled) {
                std::cout << "[PostprocessDebug] mechanical_gripper_reject"
                          << " index=" << i
                          << " reason=mechanical_gripper_outside_protection_polygon"
                          << std::endl;
            }
            detection.can_grab = false;
            continue;
        }

        const int matched_index = detection.matched_segment_index;
        if (matched_index < 0 || matched_index >= static_cast<int>(result.segments.size()) ||
            !SegmentMaskValid(result.segments[matched_index], result.image_width, result.image_height)) {
            if (config.debug_logging_enabled) {
                std::cout << "[PostprocessDebug] mechanical_gripper_reject"
                          << " index=" << i
                          << " reason=invalid_matched_segment"
                          << " matched_segment_index=" << matched_index
                          << std::endl;
            }
            detection.can_grab = false;
            continue;
        }

        bool collided = false;
        const SegRegion& matched_segment = result.segments[matched_index];
        for (int segment_index = 0; segment_index < static_cast<int>(result.segments.size()); ++segment_index) {
            if (segment_index == matched_index) {
                continue;
            }
            if (GripperCornersTouchSegmentMask(detection.mechanical_gripper_corners,
                                               matched_segment,
                                               result.segments[segment_index],
                                               result.image_width,
                                               result.image_height,
                                               i,
                                               segment_index,
                                               config.debug_logging_enabled,
                                               &detection.mask_collisions)) {
                collided = true;
            }
        }

        if (collided) {
            if (config.debug_logging_enabled) {
                std::cout << "[PostprocessDebug] mechanical_gripper_reject"
                          << " index=" << i
                          << " reason=mechanical_gripper_mask_collision"
                          << " collision_count=" << detection.mask_collisions.size()
                          << std::endl;
            }
            detection.can_grab = false;
        }
    }

    ResolveResultAfterFiltering(result);
}

bool ApplyMechanicalRoiFilter(FrameInferenceResult& result,
                              const GrabLimitConfig& limits,
                              const CoordinateTransformState& coordinate_state,
                              AxisMappingMode axis_mapping_mode,
                              std::string* error_message)
{
    if (!limits.enabled) {
        return true;
    }

    if (!coordinate_state.enabled || !coordinate_state.valid) {
        if (error_message != nullptr) {
            *error_message = "机械 ROI 已启用，请先启用有效的九点坐标转换。";
        }
        result.detections.clear();
        ResolveResultAfterFiltering(result);
        return false;
    }

    double front_back_lower = 0.0;
    double front_back_upper = 0.0;
    double left_right_lower = 0.0;
    double left_right_upper = 0.0;
    if (!InnerLimitRange(limits.x, limits.roi_margin, &front_back_lower, &front_back_upper) ||
        !InnerLimitRange(limits.y, limits.roi_margin, &left_right_lower, &left_right_upper)) {
        if (error_message != nullptr) {
            *error_message = "ROI 留边过大，内缩后的机械 ROI 为空。";
        }
        result.detections.clear();
        ResolveResultAfterFiltering(result);
        return false;
    }

    const int total_count = static_cast<int>(result.detections.size());
    std::vector<PoseDetection> filtered;
    filtered.reserve(result.detections.size());
    for (const PoseDetection& detection : result.detections) {
        if (!detection.has_machine_coords) {
            continue;
        }

        const float front_back = axis_mapping_mode == AxisMappingMode::FrontBackMachineX
                                     ? detection.machine_x
                                     : detection.machine_y;
        const float left_right = axis_mapping_mode == AxisMappingMode::FrontBackMachineX
                                     ? detection.machine_y
                                     : detection.machine_x;
        if (front_back >= front_back_lower &&
            front_back <= front_back_upper &&
            left_right >= left_right_lower &&
            left_right <= left_right_upper) {
            filtered.push_back(detection);
        }
    }

    result.detections = std::move(filtered);
    ResolveResultAfterFiltering(result);
    std::cout << "[ROI] total=" << total_count
              << " inside=" << result.detections.size()
              << " margin=" << limits.roi_margin
              << " front_back=[" << front_back_lower << "," << front_back_upper << "]"
              << " left_right=[" << left_right_lower << "," << left_right_upper << "]"
              << " mapping=" << (axis_mapping_mode == AxisMappingMode::FrontBackMachineX
                                      ? "front_back=machine_x,left_right=machine_y"
                                      : "front_back=machine_y,left_right=machine_x")
              << std::endl;
    return true;
}

GrabLimitDecision EvaluateGrabLimits(const FrameInferenceResult& result,
                                     const GrabLimitConfig& limits,
                                     const PlcOutputConfig& plc_config)
{
    GrabLimitDecision decision;
    if (!limits.enabled) {
        return decision;
    }
    if (result.primary_index < 0 || result.primary_index >= static_cast<int>(result.detections.size())) {
        decision.rejected = true;
        decision.reason = "无有效主目标";
        return decision;
    }

    const PlcWriteResult write_result = BuildPlcWriteResult(result, plc_config);
    if (write_result.type_compensation_failed) {
        decision.rejected = true;
        decision.reason = "类型中心补偿需要有效的九点坐标转换和 AC 射线方向。";
        return decision;
    }
    const float active_front_back = write_result.x;
    const float active_left_right = write_result.y;
    if (!IsValueWithinRange(active_front_back, limits.x)) {
        decision.rejected = true;
        decision.reason = FormatLimitReason(FrontBackAxisLabel(plc_config.axis_mapping_mode),
                                            active_front_back,
                                            limits.x);
        return decision;
    }
    if (!IsValueWithinRange(active_left_right, limits.y)) {
        decision.rejected = true;
        decision.reason = FormatLimitReason(LeftRightAxisLabel(plc_config.axis_mapping_mode),
                                            active_left_right,
                                            limits.y);
        return decision;
    }

    const float active_angle = write_result.angle;
    if (!IsValueWithinRange(active_angle, limits.angle)) {
        decision.rejected = true;
        decision.reason = FormatLimitReason("旋转角度", active_angle, limits.angle);
        return decision;
    }
    return decision;
}
