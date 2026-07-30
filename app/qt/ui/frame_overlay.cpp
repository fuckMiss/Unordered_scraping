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

bool IsPrimaryDetection(const FrameInferenceResult& result, int detection_index)
{
    return detection_index >= 0 && detection_index == result.primary_index;
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
                          double display_scale)
{
    if (detection.corners.size() != 4) {
        return;
    }

    const Scalar original_color = GetClassColor(detection.class_id);
    const Scalar extended_color = DetectionStateColor(detection, primary);
    const int thickness = ScaledStroke(display_scale, emphasized ? 4 : 3);
    const int extended_thickness = ScaledStroke(display_scale, emphasized ? 4 : 3);
    const Scalar arrow_color(255, 0, 255);
    const int arrow_thickness = ScaledStroke(display_scale, emphasized ? 4 : 3);

    DrawPolyline(image, detection.extended_corners, extended_color, extended_thickness);
    DrawPolyline(image, detection.corners, original_color, thickness);

    if (!draw_rays) {
        return;
    }

    const Point2f display_arrow_end = ExtendLineEnd(detection.arrow_start, detection.arrow_end, 4.0f);
    arrowedLine(image, detection.arrow_start, display_arrow_end, arrow_color, arrow_thickness, LINE_AA, 0, 0.28);
    circle(image, detection.arrow_start, ScaledRadius(display_scale, emphasized ? 5 : 4), arrow_color, FILLED, LINE_AA);

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

void DrawFrameOverlay(Mat& image,
                      const FrameInferenceResult& result,
                      int selected_index,
                      bool draw_center_reticle,
                      bool show_all_detections,
                      bool show_plc_center_debug,
                      bool show_head_ray_debug)
{
    if (draw_center_reticle) {
        DrawCenterReticle(image);
    }

    const int active_index = ResolveSelectionIndex(result, selected_index);
    if (show_all_detections) {
        DrawSegmentationOverlay(image, result, true);
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
                                 result.display_scale);
        }
        return;
    }

    DrawAllSegmentMinRects(image, result);
    for (int i = 0; i < static_cast<int>(result.detections.size()); ++i) {
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
                             result.display_scale);
    }
}
