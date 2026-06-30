#include "frame_overlay.h"

#include "model_utils.h"

#include <algorithm>

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
            rectangle(image, segment.bbox, color, 2, LINE_AA);
        }
        if (segment.min_rect_corners.size() == 4) {
            for (size_t i = 0; i < segment.min_rect_corners.size(); ++i) {
                line(image,
                     segment.min_rect_corners[i],
                     segment.min_rect_corners[(i + 1) % segment.min_rect_corners.size()],
                     Scalar(255, 0, 0),
                     2,
                     LINE_AA);
            }
        }
        circle(image, segment.center, 5, Scalar(0, 0, 255), FILLED, LINE_AA);
    }
}

void DrawDetectionPolygon(Mat& image, const PoseDetection& detection, bool selected)
{
    if (detection.corners.size() != 4) {
        return;
    }

    const Scalar color = selected ? Scalar(0, 215, 255) : GetClassColor(detection.class_id);
    const int thickness = selected ? 4 : 2;
    const Scalar arrow_color(255, 0, 0);
    const int arrow_thickness = selected ? 4 : 3;

    for (size_t i = 0; i < detection.corners.size(); ++i) {
        const Point2f start = detection.corners[i];
        const Point2f end = detection.corners[(i + 1) % detection.corners.size()];
        line(image, start, end, color, thickness, LINE_AA);
    }

    arrowedLine(image, detection.arrow_start, detection.arrow_end, arrow_color, arrow_thickness, LINE_AA, 0, 0.28);
    circle(image, detection.arrow_start, selected ? 5 : 4, arrow_color, FILLED, LINE_AA);

    circle(image, detection.x_point, selected ? 6 : 5, Scalar(255, 0, 255), FILLED, LINE_AA);
    if (detection.has_small_point) {
        circle(image, detection.small_point, selected ? 7 : 6, Scalar(0, 255, 255), FILLED, LINE_AA);
        line(image, detection.seg_center, detection.small_point, Scalar(0, 255, 255), 1, LINE_AA);
    }
}

}  // namespace

void DrawFrameOverlay(Mat& image,
                      const FrameInferenceResult& result,
                      int selected_index,
                      bool draw_center_reticle,
                      bool show_all_detections)
{
    if (draw_center_reticle) {
        DrawCenterReticle(image);
    }

    const int active_index = ResolveSelectionIndex(result, selected_index);
    if (show_all_detections) {
        DrawSegmentationOverlay(image, result, true);
        for (int i = 0; i < static_cast<int>(result.detections.size()); ++i) {
            DrawDetectionPolygon(image, result.detections[i], i == active_index);
        }
        return;
    }

    if (active_index >= 0) {
        DrawDetectionPolygon(image, result.detections[active_index], true);
    }
}
