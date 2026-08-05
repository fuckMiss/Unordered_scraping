#include "frame_overlay.h"

#include "model_utils.h"

#include <algorithm>
#include <cmath>

using namespace cv;
using namespace std;

namespace {

int ResolveSelectionIndex(const FrameInferenceResult& result, int selected_index)
{
    if (selected_index >= 0 && selected_index < static_cast<int>(result.detections.size())) {
        return selected_index;
    }
    if (result.primary_index >= 0 && result.primary_index < static_cast<int>(result.detections.size())) {
        return result.primary_index;
    }
    return -1;
}

int ScaledStroke(double display_scale, int original_thickness)
{
    return max(1, cvRound(original_thickness * display_scale));
}

int ScaledRadius(double display_scale, int original_radius)
{
    return max(2, cvRound(original_radius * display_scale));
}

Point2f ExtendLineEnd(const Point2f& start, const Point2f& end, float scale)
{
    const Point2f ray = end - start;
    if (std::hypot(ray.x, ray.y) < 1e-6f) {
        return end;
    }
    return start + ray * scale;
}

float NormalizeAngleDiffDeg(float target_angle_deg, float source_angle_deg)
{
    float diff = std::fmod(target_angle_deg - source_angle_deg + 180.0f, 360.0f);
    if (diff < 0.0f) {
        diff += 360.0f;
    }
    return diff - 180.0f;
}

Point2f RotatePointAroundCenter(const Point2f& point,
                                const Point2f& center,
                                float angle_deg)
{
    const float angle_rad = static_cast<float>(angle_deg * CV_PI / 180.0);
    const float cos_angle = std::cos(angle_rad);
    const float sin_angle = std::sin(angle_rad);
    const Point2f vector = point - center;
    return {
        center.x + vector.x * cos_angle - vector.y * sin_angle,
        center.y + vector.x * sin_angle + vector.y * cos_angle,
    };
}

void DrawPolyline(Mat& image,
                  const vector<Point2f>& points,
                  const Scalar& color,
                  int thickness)
{
    if (points.size() != 4) {
        return;
    }

    for (size_t i = 0; i < points.size(); ++i) {
        const Point2f start = points[i];
        const Point2f end = points[(i + 1) % points.size()];
        line(image, start, end, color, thickness, LINE_AA);
    }
}

void DrawPlcCenterDebug(Mat& image,
                        const PoseDetection& detection,
                        double display_scale,
                        bool emphasized)
{
    const Point2f original_center = detection.obb_center;
    const Point2f plc_center(detection.center_x, detection.center_y);
    const Scalar original_color(255, 255, 255);
    const Scalar plc_color(0, 255, 80);
    const Scalar outline_color(0, 0, 0);
    const int line_thickness = ScaledStroke(display_scale, emphasized ? 3 : 2);
    const int original_radius = ScaledRadius(display_scale, emphasized ? 6 : 5);
    const int plc_radius = ScaledRadius(display_scale, emphasized ? 9 : 8);
    const int cross_arm = ScaledRadius(display_scale, emphasized ? 14 : 12);

    line(image, original_center, plc_center, outline_color, line_thickness + ScaledStroke(display_scale, 3), LINE_AA);
    line(image, original_center, plc_center, plc_color, line_thickness, LINE_AA);
    circle(image, original_center, original_radius, outline_color, line_thickness + ScaledStroke(display_scale, 2), LINE_AA);
    circle(image, original_center, original_radius, original_color, line_thickness, LINE_AA);
    circle(image, plc_center, plc_radius, outline_color, FILLED, LINE_AA);
    circle(image, plc_center, max(2, plc_radius - ScaledRadius(display_scale, 3)), plc_color, FILLED, LINE_AA);
    circle(image, plc_center, plc_radius + ScaledRadius(display_scale, 4), plc_color, line_thickness, LINE_AA);
    line(image,
         Point2f(plc_center.x - cross_arm, plc_center.y),
         Point2f(plc_center.x + cross_arm, plc_center.y),
         outline_color,
         ScaledStroke(display_scale, 4),
         LINE_AA);
    line(image,
         Point2f(plc_center.x, plc_center.y - cross_arm),
         Point2f(plc_center.x, plc_center.y + cross_arm),
         outline_color,
         ScaledStroke(display_scale, 4),
         LINE_AA);
}

void DrawCenterReticle(Mat& image)
{
    const Point center(image.cols / 2, image.rows / 2);
    const int ring_radius = max(24, min(image.cols, image.rows) / 8);
    const Scalar ring_color(220, 220, 220);
    const Scalar cross_color(255, 140, 0);
    const int arm = max(18, ring_radius / 3);
    const int gap = max(10, ring_radius / 5);

    circle(image, center, ring_radius, ring_color, 10, LINE_AA);
    line(image, Point(center.x - ring_radius - arm, center.y), Point(center.x - gap, center.y), cross_color, 6, LINE_AA);
    line(image, Point(center.x + gap, center.y), Point(center.x + ring_radius + arm, center.y), cross_color, 6, LINE_AA);
    line(image, Point(center.x, center.y - ring_radius - arm), Point(center.x, center.y - gap), cross_color, 6, LINE_AA);
    line(image, Point(center.x, center.y + gap), Point(center.x, center.y + ring_radius + arm), cross_color, 6, LINE_AA);
}

Point ClampTextOrigin(const Mat& image, const Point2f& point)
{
    return Point(max(6, min(image.cols - 90, cvRound(point.x) + 8)),
                 max(18, min(image.rows - 8, cvRound(point.y) - 8)));
}

void DrawDashedLine(Mat& image,
                    const Point2f& start,
                    const Point2f& end,
                    const Scalar& color,
                    int thickness)
{
    const Point2f delta = end - start;
    const double length = std::hypot(delta.x, delta.y);
    if (length < 1.0) {
        return;
    }

    const Point2f direction(delta.x / static_cast<float>(length), delta.y / static_cast<float>(length));
    const double dash = max(8.0, length / 28.0);
    const double gap = dash * 0.55;
    for (double offset = 0.0; offset < length; offset += dash + gap) {
        const double next = min(length, offset + dash);
        line(image,
             start + direction * static_cast<float>(offset),
             start + direction * static_cast<float>(next),
             color,
             thickness,
             LINE_AA);
    }
}

void DrawGrabLimitOverlay(Mat& image, const GrabLimitOverlayView& overlay)
{
    if (!overlay.visible || overlay.polygon.size() != 4) {
        if (!overlay.message.empty()) {
            putText(image,
                    overlay.message,
                    Point(12, 28),
                    FONT_HERSHEY_SIMPLEX,
                    0.55,
                    Scalar(60, 220, 255),
                    2,
                    LINE_AA);
        }
        return;
    }

    vector<Point> polygon;
    polygon.reserve(overlay.polygon.size());
    for (const Point2f& point : overlay.polygon) {
        polygon.emplace_back(cvRound(point.x), cvRound(point.y));
    }

    Mat fill_layer = image.clone();
    const vector<vector<Point>> polygons = { polygon };
    fillPoly(fill_layer, polygons, Scalar(90, 210, 80), LINE_AA);
    addWeighted(fill_layer, 0.16, image, 0.84, 0.0, image);

    const int thickness = ScaledStroke(1.0, 2);
    const Scalar outline(45, 245, 80);
    const Scalar shadow(0, 0, 0);
    for (size_t i = 0; i < overlay.polygon.size(); ++i) {
        const Point2f start = overlay.polygon[i];
        const Point2f end = overlay.polygon[(i + 1) % overlay.polygon.size()];
        DrawDashedLine(image, start, end, shadow, thickness + 2);
        DrawDashedLine(image, start, end, outline, thickness);
    }

    if (!overlay.message.empty()) {
        putText(image,
                overlay.message,
                ClampTextOrigin(image, overlay.polygon.front()),
                FONT_HERSHEY_SIMPLEX,
                0.55,
                Scalar(45, 245, 80),
                2,
                LINE_AA);
    }
}

void DrawSegmentationOverlay(Mat& image, const FrameInferenceResult& result, bool show_all_detections)
{
    if (!show_all_detections || result.segments.empty()) {
        return;
    }

    Mat overlay = image.clone();
    bool has_mask = false;

    for (const auto& segment : result.segments) {
        const Scalar color = GetClassColor(segment.class_id);
        if (!segment.mask.empty()) {
            overlay.setTo(color, segment.mask);
            has_mask = true;
        }
    }

    if (has_mask) {
        addWeighted(overlay, 0.22, image, 0.78, 0.0, image);
    }

    for (const auto& segment : result.segments) {
        const Scalar color = GetClassColor(segment.class_id);
        if (segment.bbox.width > 0 && segment.bbox.height > 0) {
            rectangle(image, segment.bbox, color, ScaledStroke(result.display_scale, 2), LINE_AA);
        }
        if (segment.min_rect_corners.size() == 4) {
            for (size_t i = 0; i < segment.min_rect_corners.size(); ++i) {
                line(image,
                     segment.min_rect_corners[i],
                     segment.min_rect_corners[(i + 1) % segment.min_rect_corners.size()],
                     Scalar(255, 0, 0),
                     ScaledStroke(result.display_scale, 2),
                     LINE_AA);
            }
        }
        circle(image, segment.center, ScaledRadius(result.display_scale, 5), Scalar(0, 0, 255), FILLED, LINE_AA);
    }
}

void DrawAllSegmentMinRects(Mat& image, const FrameInferenceResult& result)
{
    const Scalar segment_color(175, 220, 255);
    const int thickness = ScaledStroke(result.display_scale, 2);
    for (const auto& segment : result.segments) {
        DrawPolyline(image, segment.min_rect_corners, segment_color, thickness);
    }
}

void DrawRawObbOverlay(Mat& image, const FrameInferenceResult& result)
{
    const int thickness = ScaledStroke(result.display_scale, 2);
    const int center_radius = ScaledRadius(result.display_scale, 4);
    for (const auto& region : result.raw_obb_regions) {
        const Scalar color = GetClassColor(region.class_id);
        DrawPolyline(image, region.corners, color, thickness);
        circle(image, region.center, center_radius, color, FILLED, LINE_AA);
    }
}

void DrawMaskCollisionOverlay(Mat& image, const FrameInferenceResult& result)
{
    Mat overlay = image.clone();
    bool has_collision = false;
    const Rect image_rect(0, 0, image.cols, image.rows);
    for (const PoseDetection& detection : result.detections) {
        for (const auto& collision : detection.mask_collisions) {
            const Rect roi = collision.first & image_rect;
            if (roi.empty() || collision.second.empty()) {
                continue;
            }

            const Rect local_crop(roi.x - collision.first.x,
                                  roi.y - collision.first.y,
                                  roi.width,
                                  roi.height);
            if (local_crop.x < 0 ||
                local_crop.y < 0 ||
                local_crop.x + local_crop.width > collision.second.cols ||
                local_crop.y + local_crop.height > collision.second.rows) {
                continue;
            }

            Mat display_mask = collision.second(local_crop).clone();
            dilate(display_mask, display_mask, Mat(), Point(-1, -1), 1);
            overlay(roi).setTo(Scalar(40, 40, 255), display_mask);
            has_collision = true;
        }
    }

    if (has_collision) {
        addWeighted(overlay, 0.45, image, 0.55, 0.0, image);
    }
}

bool IsPrimaryDetection(const FrameInferenceResult& result, int detection_index)
{
    return detection_index >= 0 && detection_index == result.primary_index;
}

bool DisplayCandidateBeats(const FrameInferenceResult& result, int candidate_index, int current_index)
{
    if (current_index < 0 ||
        current_index >= static_cast<int>(result.detections.size())) {
        return true;
    }
    if (IsPrimaryDetection(result, candidate_index) != IsPrimaryDetection(result, current_index)) {
        return IsPrimaryDetection(result, candidate_index);
    }

    const PoseDetection& candidate = result.detections[candidate_index];
    const PoseDetection& current = result.detections[current_index];
    if (candidate.can_grab != current.can_grab) {
        return candidate.can_grab;
    }
    return candidate.confidence > current.confidence;
}

Scalar DetectionStateColor(const PoseDetection& detection, bool primary)
{
    if (primary) {
        return Scalar(0, 165, 210);
    }
    if (detection.can_grab) {
        return Scalar(70, 220, 90);
    }
    return Scalar(40, 40, 240);
}

void DrawDetectionPolygon(Mat& image,
                          const PoseDetection& detection,
                          bool primary,
                          bool emphasized,
                          bool draw_rays,
                          bool draw_plc_center_debug,
                          bool show_head_ray_debug,
                          bool use_plc_command_pose,
                          bool draw_detection_corners,
                          double display_scale)
{
    if (detection.corners.size() != 4) {
        return;
    }

    const bool use_command_pose = use_plc_command_pose && detection.has_plc_command_pose;
    const Point2f raw_center(detection.center_x, detection.center_y);
    const Point2f command_delta = use_command_pose ? detection.plc_command_center - raw_center : Point2f();
    const float command_angle_delta_deg = use_command_pose
        ? NormalizeAngleDiffDeg(detection.plc_command_angle_deg, detection.angle_deg)
        : 0.0f;
    vector<Point2f> display_gripper_corners = detection.mechanical_gripper_corners;
    if (use_command_pose) {
        for (Point2f& point : display_gripper_corners) {
            point = RotatePointAroundCenter(point, raw_center, command_angle_delta_deg) + command_delta;
        }
    }
    vector<Point2f> display_detection_corners = detection.corners;
    if (use_command_pose) {
        for (Point2f& point : display_detection_corners) {
            point = RotatePointAroundCenter(point, raw_center, command_angle_delta_deg) + command_delta;
        }
    }
    const Point2f arrow_start = use_command_pose ? detection.plc_command_arrow_start : detection.arrow_start;
    const Point2f arrow_end = use_command_pose ? detection.plc_command_arrow_end : detection.arrow_end;

    const Scalar original_color = GetClassColor(detection.class_id);
    const Scalar state_color = DetectionStateColor(detection, primary);
    const int thickness = ScaledStroke(display_scale, emphasized ? 4 : 3);
    const int mechanical_gripper_thickness = ScaledStroke(display_scale, emphasized ? 3 : 2);
    const Scalar arrow_color(255, 0, 255);
    const int arrow_thickness = ScaledStroke(display_scale, emphasized ? 4 : 3);

    DrawPolyline(image, display_gripper_corners, state_color, mechanical_gripper_thickness);
    if (draw_detection_corners) {
        DrawPolyline(image, display_detection_corners, original_color, thickness);
    }

    if (!draw_rays) {
        return;
    }

    const Point2f display_arrow_end = ExtendLineEnd(arrow_start, arrow_end, 4.0f);
    arrowedLine(image, arrow_start, display_arrow_end, arrow_color, arrow_thickness, LINE_AA, 0, 0.28);
    circle(image, arrow_start, ScaledRadius(display_scale, emphasized ? 5 : 4), arrow_color, FILLED, LINE_AA);

    circle(image, detection.x_point, ScaledRadius(display_scale, emphasized ? 6 : 5), Scalar(255, 0, 255), FILLED, LINE_AA);
    const Scalar big_head_color(0, 255, 255);
    const Scalar small_head_color(255, 255, 0);
    const Scalar unknown_head_color(0, 165, 255);
    const Scalar head_color = detection.head_class_id == 1
                                  ? big_head_color
                                  : (detection.head_class_id == 2 ? small_head_color : unknown_head_color);
    if (show_head_ray_debug && detection.has_small_point) {
        circle(image, detection.small_point, ScaledRadius(display_scale, emphasized ? 7 : 6), head_color, FILLED, LINE_AA);
        line(image, detection.seg_center, detection.small_point, head_color, ScaledStroke(display_scale, 1), LINE_AA);
    }
    if (show_head_ray_debug && detection.has_small_ray) {
        arrowedLine(image,
                    detection.small_arrow_start,
                    detection.small_arrow_end,
                    head_color,
                    ScaledStroke(display_scale, emphasized ? 3 : 2),
                    LINE_AA,
                    0,
                    0.28);
        circle(image,
               detection.small_arrow_start,
               ScaledRadius(display_scale, emphasized ? 5 : 4),
               head_color,
               FILLED,
               LINE_AA);
    }

    if (draw_plc_center_debug) {
        DrawPlcCenterDebug(image, detection, display_scale, emphasized);
    }
}

}  // namespace

vector<int> SelectNormalDisplayDetectionIndices(const FrameInferenceResult& result)
{
    vector<int> indices;
    vector<int> segment_indices;
    for (int i = 0; i < static_cast<int>(result.detections.size()); ++i) {
        const int segment_index = result.detections[i].matched_segment_index;
        if (segment_index < 0) {
            indices.push_back(i);
            segment_indices.push_back(segment_index);
            continue;
        }

        auto found = find(segment_indices.begin(), segment_indices.end(), segment_index);
        if (found == segment_indices.end()) {
            indices.push_back(i);
            segment_indices.push_back(segment_index);
            continue;
        }

        const int output_index = static_cast<int>(distance(segment_indices.begin(), found));
        if (DisplayCandidateBeats(result, i, indices[output_index])) {
            indices[output_index] = i;
        }
    }
    return indices;
}

void DrawFrameOverlay(Mat& image,
                      const FrameInferenceResult& result,
                      int selected_index,
                      bool draw_center_reticle,
                      bool show_all_detections,
                      bool show_plc_center_debug,
                      bool show_head_ray_debug,
                      const GrabLimitOverlayView* grab_limit_overlay)
{
    if (draw_center_reticle) {
        DrawCenterReticle(image);
    }
    if (grab_limit_overlay != nullptr) {
        DrawGrabLimitOverlay(image, *grab_limit_overlay);
    }

    const int active_index = ResolveSelectionIndex(result, selected_index);
    if (show_all_detections) {
        DrawSegmentationOverlay(image, result, true);
        DrawMaskCollisionOverlay(image, result);
        DrawRawObbOverlay(image, result);
        for (int i = 0; i < static_cast<int>(result.detections.size()); ++i) {
            const bool primary = IsPrimaryDetection(result, i);
            const bool draw_rays = primary && result.detections[i].can_grab;
            const bool draw_plc_debug = show_plc_center_debug && draw_rays;
            DrawDetectionPolygon(image,
                                 result.detections[i],
                                 primary,
                                 primary || i == active_index,
                                 draw_rays,
                                 draw_plc_debug,
                                 show_head_ray_debug,
                                 true,
                                 false,
                                 result.display_scale);
        }
        return;
    }

    DrawAllSegmentMinRects(image, result);
    const vector<int> normal_indices = SelectNormalDisplayDetectionIndices(result);
    for (const int i : normal_indices) {
        const bool primary = IsPrimaryDetection(result, i);
        const bool draw_rays = primary && result.detections[i].can_grab;
        const bool draw_plc_debug = show_plc_center_debug && draw_rays;
        DrawDetectionPolygon(image,
                             result.detections[i],
                             primary,
                             primary,
                             draw_rays,
                             draw_plc_debug,
                             show_head_ray_debug,
                             true,
                             true,
                             result.display_scale);
    }
}
