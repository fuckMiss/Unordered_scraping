#include "frame_postprocess.h"

#include "model_utils.h"

#include <algorithm>
#include <cmath>
#include <limits>

using namespace cv;
using namespace std;

namespace {

constexpr float kExtendedObbScale = 2.0f;

Rect ClipRectToImage(const Rect& rect, int image_width, int image_height)
{
    return rect & Rect(0, 0, image_width, image_height);
}

Rect BuildPoseBoundingRect(const vector<Point2f>& corners, int image_width, int image_height)
{
    if (corners.empty()) {
        return Rect();
    }
    return ClipRectToImage(boundingRect(corners), image_width, image_height);
}

vector<Point2f> BuildExtendedObbCorners(const OBBDetection& detection)
{
    const RotatedRect& rect = detection.rotated_rect;
    const float width = rect.size.width;
    const float height = rect.size.height;

    Size2f extended_size = rect.size;
    if (width > height) {
        extended_size.width *= kExtendedObbScale;
    } else {
        extended_size.height *= kExtendedObbScale;
    }

    Point2f points[4];
    RotatedRect(rect.center, extended_size, rect.angle).points(points);
    return { points[0], points[1], points[2], points[3] };
}

bool MaskContainsPoint(const Mat& mask, const Point2f& point)
{
    const int x = cvRound(point.x);
    const int y = cvRound(point.y);
    if (mask.empty() || x < 0 || y < 0 || x >= mask.cols || y >= mask.rows) {
        return false;
    }
    return mask.at<uchar>(y, x) > 0;
}

bool ExtendedObbTouchesSegment(const vector<Point2f>& extended_corners, const SegRegion& segment)
{
    if (extended_corners.size() < 3 || segment.mask.empty()) {
        return false;
    }

    Mat obb_mask = Mat::zeros(segment.mask.size(), CV_8U);
    vector<Point> polygon;
    polygon.reserve(extended_corners.size());
    for (const Point2f& corner : extended_corners) {
        polygon.emplace_back(cvRound(corner.x), cvRound(corner.y));
    }
    fillConvexPoly(obb_mask, polygon, Scalar(255), LINE_AA);

    Mat intersection;
    bitwise_and(obb_mask, segment.mask, intersection);
    return countNonZero(intersection) > 0;
}

Point2f ComputePythonStyleKeepPoint(const OBBDetection& detection)
{
    const RotatedRect& rect = detection.rotated_rect;
    const float angle_rad = rect.angle * static_cast<float>(CV_PI) / 180.0f;
    const float width = rect.size.width;
    const float height = rect.size.height;

    float dx = 0.0f;
    float dy = 0.0f;
    if (width > height) {
        dy = height * 0.5f;
    } else {
        dx = width * 0.5f;
    }

    const float c = std::cos(angle_rad);
    const float s = std::sin(angle_rad);
    const Point2f offset(dx * c - dy * s, dx * s + dy * c);
    const Point2f point_a = rect.center + offset;
    const Point2f point_b = rect.center - offset;

    if (detection.class_id == 0) {
        return point_a.x < point_b.x ? point_a : point_b;
    }
    return point_a.x > point_b.x ? point_a : point_b;
}

vector<SegRegion> BuildSegRegions(const vector<SegDetection>& seg_output,
                                  const vector<string>& seg_class_names,
                                  int image_width,
                                  int image_height)
{
    vector<SegRegion> segments;
    segments.reserve(seg_output.size());

    for (const auto& detection : seg_output) {
        SegRegion region;
        region.class_id = detection.class_id;
        region.class_name = GetClassName(seg_class_names, detection.class_id);
        region.confidence = detection.conf;
        region.bbox = ClipRectToImage(detection.bbox, image_width, image_height);
        region.mask = detection.mask;
        segments.push_back(std::move(region));
    }

    return segments;
}

PoseDetection BuildPoseDetection(const OBBDetection& detection,
                                 const vector<string>& obb_class_names,
                                 int image_width,
                                 int image_height,
                                 int matched_segment_index,
                                 const string& segment_class_name)
{
    const auto& rect = detection.rotated_rect;
    const Point2f center = rect.center;
    const Point2f keep_point = ComputePythonStyleKeepPoint(detection);
    const Point2f ray = keep_point - center;
    float ray_angle_deg = std::atan2(ray.y, ray.x) * 180.0f / static_cast<float>(CV_PI);
    if (ray_angle_deg < 0.0f) {
        ray_angle_deg += 360.0f;
    }

    PoseDetection pose;
    pose.class_id = detection.class_id;
    pose.class_name = GetClassName(obb_class_names, detection.class_id);
    pose.segment_class_name = segment_class_name;
    pose.matched_segment_index = matched_segment_index;
    pose.confidence = detection.conf;
    pose.center_x = center.x;
    pose.center_y = center.y;
    pose.angle_deg = ray_angle_deg;
    pose.arrow_start = center;
    pose.arrow_end = keep_point;
    pose.corners = BuildExtendedObbCorners(detection);
    pose.bbox = BuildPoseBoundingRect(pose.corners, image_width, image_height);
    pose.can_grab = true;
    return pose;
}

vector<PoseDetection> BuildFilteredPoseDetections(const vector<OBBDetection>& obb_output,
                                                  const vector<string>& obb_class_names,
                                                  const vector<SegRegion>& segments,
                                                  int image_width,
                                                  int image_height)
{
    vector<PoseDetection> detections;
    detections.reserve(obb_output.size());

    for (const auto& detection : obb_output) {
        const Point2f center = detection.rotated_rect.center;
        int matched_segment_index = -1;
        int match_count = 0;

        for (int i = 0; i < static_cast<int>(segments.size()); ++i) {
            if (MaskContainsPoint(segments[i].mask, center)) {
                matched_segment_index = i;
                ++match_count;
            }
        }

        if (match_count != 1) {
            continue;
        }

        const vector<Point2f> extended_corners = BuildExtendedObbCorners(detection);
        bool touches_other = false;
        for (int i = 0; i < static_cast<int>(segments.size()); ++i) {
            if (i == matched_segment_index) {
                continue;
            }
            if (ExtendedObbTouchesSegment(extended_corners, segments[i])) {
                touches_other = true;
                break;
            }
        }

        if (touches_other) {
            continue;
        }

        detections.push_back(BuildPoseDetection(detection,
                                                obb_class_names,
                                                image_width,
                                                image_height,
                                                matched_segment_index,
                                                segments[matched_segment_index].class_name));
    }

    return detections;
}

void ResolvePrimaryDetection(FrameInferenceResult& result)
{
    float best_score = -numeric_limits<float>::infinity();
    for (int i = 0; i < static_cast<int>(result.detections.size()); ++i) {
        const auto& detection = result.detections[i];
        const float score = detection.confidence + (detection.can_grab ? 0.01f : 0.0f);
        if (score > best_score) {
            best_score = score;
            result.primary_index = i;
        }
    }
}

}  // namespace

FrameInferenceResult BuildFrameInferenceResult(const vector<OBBDetection>& obb_output,
                                               const vector<string>& obb_class_names,
                                               const vector<SegDetection>& seg_output,
                                               const vector<string>& seg_class_names,
                                               int image_width,
                                               int image_height,
                                               double obb_inference_ms,
                                               double seg_inference_ms)
{
    FrameInferenceResult result;
    result.image_width = image_width;
    result.image_height = image_height;
    result.obb_inference_ms = obb_inference_ms;
    result.seg_inference_ms = seg_inference_ms;
    result.total_inference_ms = obb_inference_ms + seg_inference_ms;
    result.segments = BuildSegRegions(seg_output, seg_class_names, image_width, image_height);
    result.detections = BuildFilteredPoseDetections(obb_output,
                                                    obb_class_names,
                                                    result.segments,
                                                    image_width,
                                                    image_height);

    ResolvePrimaryDetection(result);
    return result;
}
