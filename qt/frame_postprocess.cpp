#include "frame_postprocess.h"

#include "model_utils.h"

#include <algorithm>
#include <cmath>
#include <limits>

using namespace cv;
using namespace std;

namespace {

constexpr float kExtendedObbScale = 2.0f;
constexpr int kPickStatusCanGrab = 1;
constexpr int kPickStatusMultiGrab = 2;
constexpr int kPickStatusCannotGrab = 3;
constexpr int kClassLeft = 0;
constexpr int kClassRight = 1;
constexpr int kClassSmall = 2;

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

Point2f ComputeSegmentCenter(const Mat& mask, const Rect& fallback_bbox)
{
    vector<vector<Point>> contours;
    findContours(mask, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

    double best_area = 0.0;
    int best_index = -1;
    for (int i = 0; i < static_cast<int>(contours.size()); ++i) {
        const double area = contourArea(contours[i]);
        if (area > best_area) {
            best_area = area;
            best_index = i;
        }
    }

    if (best_index >= 0 && contours[best_index].size() >= 3) {
        const RotatedRect rect = minAreaRect(contours[best_index]);
        return rect.center;
    }

    return Point2f(fallback_bbox.x + fallback_bbox.width * 0.5f,
                   fallback_bbox.y + fallback_bbox.height * 0.5f);
}

vector<Point2f> ExtractLargestContour(const Mat& mask)
{
    vector<vector<Point>> contours;
    findContours(mask, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

    double best_area = 0.0;
    int best_index = -1;
    for (int i = 0; i < static_cast<int>(contours.size()); ++i) {
        const double area = contourArea(contours[i]);
        if (area > best_area) {
            best_area = area;
            best_index = i;
        }
    }

    vector<Point2f> result;
    if (best_index < 0) {
        return result;
    }

    result.reserve(contours[best_index].size());
    for (const Point& point : contours[best_index]) {
        result.emplace_back(static_cast<float>(point.x), static_cast<float>(point.y));
    }
    return result;
}

vector<Point2f> BuildSegmentMinRectCorners(const Mat& mask)
{
    const vector<Point2f> contour = ExtractLargestContour(mask);
    if (contour.size() < 3) {
        return {};
    }

    Point2f points[4];
    minAreaRect(contour).points(points);
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

float ComputePythonStyleAngle(const Point2f& center, const Point2f& keep_point)
{
    const Point2f ray = keep_point - center;
    float ray_angle_deg = std::atan2(ray.x, ray.y) * 180.0f / static_cast<float>(CV_PI);
    if (ray_angle_deg < 0.0f) {
        ray_angle_deg += 360.0f;
    }
    return ray_angle_deg;
}

float AngleBetweenVectors(const Point2f& first, const Point2f& second)
{
    const float dot = first.x * second.x + first.y * second.y;
    const float first_mag = std::hypot(first.x, first.y);
    const float second_mag = std::hypot(second.x, second.y);
    if (first_mag < 1e-6f || second_mag < 1e-6f) {
        return 180.0f;
    }

    float cos_angle = dot / (first_mag * second_mag);
    cos_angle = std::max(-1.0f, std::min(1.0f, cos_angle));
    return std::acos(cos_angle) * 180.0f / static_cast<float>(CV_PI);
}

int ResolveQuadrant(const Point2f& origin, const Point2f& point)
{
    const float dx = point.x - origin.x;
    const float dy = -(point.y - origin.y);
    if (dx > 0.0f && dy > 0.0f) {
        return 1;
    }
    if (dx < 0.0f && dy > 0.0f) {
        return 2;
    }
    if (dx < 0.0f && dy < 0.0f) {
        return 3;
    }
    if (dx > 0.0f && dy < 0.0f) {
        return 4;
    }
    return 0;
}

int ResolveHeadTypeCodeFromQuadrant(int quadrant)
{
    switch (quadrant) {
    case 1: return 4;
    case 2: return 2;
    case 3: return 1;
    case 4: return 3;
    default: return 0;
    }
}

string ResolveHeadTypeText(int head_type_code)
{
    switch (head_type_code) {
    case 1: return "大上左";
    case 2: return "小上左";
    case 3: return "大上右";
    case 4: return "小上右";
    default: return "未知";
    }
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
        region.center = ComputeSegmentCenter(region.mask, region.bbox);
        region.contour = ExtractLargestContour(region.mask);
        region.min_rect_corners = BuildSegmentMinRectCorners(region.mask);
        segments.push_back(std::move(region));
    }

    return segments;
}

PoseDetection BuildPoseDetection(const OBBDetection& detection,
                                 const vector<string>& obb_class_names,
                                 int image_width,
                                 int image_height,
                                 int matched_segment_index,
                                 const string& segment_class_name,
                                 const Point2f& segment_center,
                                 const Point2f& x_point,
                                 const Point2f& small_point,
                                 int head_type_code)
{
    const auto& rect = detection.rotated_rect;
    const Point2f center = rect.center;
    const Point2f keep_point = ComputePythonStyleKeepPoint(detection);
    const float ray_angle_deg = ComputePythonStyleAngle(center, keep_point);

    PoseDetection pose;
    pose.class_id = detection.class_id;
    pose.class_name = GetClassName(obb_class_names, detection.class_id);
    pose.segment_class_name = segment_class_name;
    pose.matched_segment_index = matched_segment_index;
    pose.confidence = detection.conf;
    pose.center_x = center.x;
    pose.center_y = center.y;
    pose.angle_deg = ray_angle_deg;
    pose.pick_status_code = kPickStatusCanGrab;
    pose.head_type_code = head_type_code;
    pose.head_type_text = ResolveHeadTypeText(head_type_code);
    pose.seg_center = segment_center;
    pose.x_point = x_point;
    pose.small_point = small_point;
    pose.has_small_point = head_type_code > 0;
    pose.arrow_start = center;
    pose.arrow_end = keep_point;
    pose.corners = BuildExtendedObbCorners(detection);
    pose.bbox = BuildPoseBoundingRect(pose.corners, image_width, image_height);
    pose.can_grab = true;
    return pose;
}

vector<Point2f> FindClassCentersInsideSegment(const vector<OBBDetection>& obb_output,
                                              int class_id,
                                              const SegRegion& segment)
{
    vector<Point2f> centers;
    for (const auto& detection : obb_output) {
        if (detection.class_id != class_id) {
            continue;
        }
        const Point2f center = detection.rotated_rect.center;
        if (MaskContainsPoint(segment.mask, center)) {
            centers.push_back(center);
        }
    }
    return centers;
}

Point2f ResolveBestSmallPoint(const Point2f& segment_center,
                              const Point2f& x_point,
                              const vector<Point2f>& small_candidates)
{
    Point2f best_point;
    float best_diff = numeric_limits<float>::infinity();
    const Point2f vector_ox = x_point - segment_center;

    for (const Point2f& candidate : small_candidates) {
        const Point2f vector_oy = candidate - segment_center;
        const float angle = AngleBetweenVectors(vector_ox, vector_oy);
        const float diff = std::fabs(angle - 90.0f);
        if (diff < best_diff) {
            best_diff = diff;
            best_point = candidate;
        }
    }

    return best_point;
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

        const SegRegion& matched_segment = segments[matched_segment_index];
        const vector<Point2f> x_candidates_left = FindClassCentersInsideSegment(obb_output, kClassLeft, matched_segment);
        const vector<Point2f> x_candidates_right = FindClassCentersInsideSegment(obb_output, kClassRight, matched_segment);
        vector<Point2f> x_candidates = x_candidates_left;
        x_candidates.insert(x_candidates.end(), x_candidates_right.begin(), x_candidates_right.end());
        const vector<Point2f> small_candidates = FindClassCentersInsideSegment(obb_output, kClassSmall, matched_segment);

        Point2f x_point = center;
        Point2f small_point;
        int head_type_code = 0;
        if (!x_candidates.empty() && !small_candidates.empty()) {
            x_point = x_candidates.front();
            small_point = ResolveBestSmallPoint(matched_segment.center, x_point, small_candidates);
            head_type_code = ResolveHeadTypeCodeFromQuadrant(ResolveQuadrant(matched_segment.center, small_point));
        }

        detections.push_back(BuildPoseDetection(detection,
                                                obb_class_names,
                                                image_width,
                                                image_height,
                                                matched_segment_index,
                                                matched_segment.class_name,
                                                matched_segment.center,
                                                x_point,
                                                small_point,
                                                head_type_code));
    }

    return detections;
}

void ResolvePrimaryDetection(FrameInferenceResult& result)
{
    float best_score = -numeric_limits<float>::infinity();
    for (int i = 0; i < static_cast<int>(result.detections.size()); ++i) {
        auto& detection = result.detections[i];
        const float score = detection.confidence + (detection.can_grab ? 0.01f : 0.0f);
        if (score > best_score) {
            best_score = score;
            result.primary_index = i;
        }
    }

    if (result.primary_index >= 0 && result.primary_index < static_cast<int>(result.detections.size())) {
        result.pick_status_code = result.detections[result.primary_index].can_grab ? kPickStatusCanGrab
                                                                                  : kPickStatusCannotGrab;
        for (auto& detection : result.detections) {
            detection.pick_status_code = detection.can_grab ? kPickStatusCanGrab : kPickStatusCannotGrab;
        }
        result.head_type_code = result.detections[result.primary_index].head_type_code;
        result.head_type_text = result.detections[result.primary_index].head_type_text;
    } else {
        result.pick_status_code = kPickStatusCannotGrab;
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

