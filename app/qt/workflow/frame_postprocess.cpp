#include "frame_postprocess.h"

#include "YOLOv11_OBB.h"
#include "YOLOv11_SEG.h"
#include "model_utils.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <sstream>
#include <utility>

using namespace cv;
using namespace std;

namespace {

constexpr float kExtendedObbScale = 3.0f;
constexpr int kPickStatusCanGrab = 1;
constexpr int kPickStatusMultiGrab = 2;
constexpr int kPickStatusCannotGrab = 3;
constexpr int kClassLeft = 0;
constexpr int kClassBig = 1;
constexpr int kClassSmall = 2;
constexpr float kStableTieEpsilon = 1e-4f;
constexpr float kPoseCoordinateStepPx = 0.01f;
constexpr float kPoseAngleStepDeg = 0.01f;

struct SegmentMaskAnalysis
{
    Point2f center;
    vector<Point2f> contour;
    vector<Point2f> min_rect_corners;
};

struct AcRayGeometry
{
    bool valid = false;
    string invalid_reason;
    Point2f segment_center;
    Point2f grip_center;
    Point2f head_center;
    Point2f arrow_start;
    Point2f arrow_end;
    vector<Point2f> aligned_corners;
    vector<Point2f> extended_corners;
    float ac_dot = -numeric_limits<float>::infinity();
    float rotation_abs_rad = numeric_limits<float>::infinity();
    bool ac_uses_long_edge = false;
    float raw_grip_long_angle_rad = 0.0f;
    float target_long_angle_rad = 0.0f;
    float final_long_angle_rad = 0.0f;
};

float QuantizeStable(float value, float step)
{
    if (!isfinite(value) || step <= 0.0f) {
        return value;
    }
    return round(value / step) * step;
}

bool ObbStableLess(const OBBDetection& left, const OBBDetection& right)
{
    if (left.class_id != right.class_id) {
        return left.class_id < right.class_id;
    }
    if (fabs(left.rotated_rect.center.y - right.rotated_rect.center.y) > kStableTieEpsilon) {
        return left.rotated_rect.center.y < right.rotated_rect.center.y;
    }
    if (fabs(left.rotated_rect.center.x - right.rotated_rect.center.x) > kStableTieEpsilon) {
        return left.rotated_rect.center.x < right.rotated_rect.center.x;
    }
    if (fabs(left.rotated_rect.size.width - right.rotated_rect.size.width) > kStableTieEpsilon) {
        return left.rotated_rect.size.width > right.rotated_rect.size.width;
    }
    if (fabs(left.rotated_rect.size.height - right.rotated_rect.size.height) > kStableTieEpsilon) {
        return left.rotated_rect.size.height > right.rotated_rect.size.height;
    }
    return left.rotated_rect.angle < right.rotated_rect.angle;
}

bool PoseStableLess(const PoseDetection& left, const PoseDetection& right)
{
    if (left.matched_segment_index != right.matched_segment_index) {
        return left.matched_segment_index < right.matched_segment_index;
    }
    if (left.head_type_code != right.head_type_code) {
        return left.head_type_code < right.head_type_code;
    }
    if (fabs(left.center_y - right.center_y) > kStableTieEpsilon) {
        return left.center_y < right.center_y;
    }
    if (fabs(left.center_x - right.center_x) > kStableTieEpsilon) {
        return left.center_x < right.center_x;
    }
    return left.angle_deg < right.angle_deg;
}

struct ObbEdgeBasis
{
    float angle_rad = 0.0f;
    int long_index = 1;
    int short_index = 3;
};

bool IsPostprocessDebugEnabled()
{
#if defined(_WIN32)
    char* value = nullptr;
    size_t value_size = 0;
    const errno_t error = _dupenv_s(&value, &value_size, "TANKEYE_DEBUG_POSTPROCESS");
    const bool enabled = error == 0 && value != nullptr && string(value) == "1";
    free(value);
    return enabled;
#else
    const char* value = std::getenv("TANKEYE_DEBUG_POSTPROCESS");
    return value != nullptr && string(value) == "1";
#endif
}

string BoolText(bool value)
{
    return value ? "true" : "false";
}

string PointText(const Point2f& point)
{
    ostringstream stream;
    stream << "(" << point.x << "," << point.y << ")";
    return stream.str();
}

string RectText(const Rect& rect)
{
    ostringstream stream;
    stream << "(" << rect.x << "," << rect.y << "," << rect.width << "," << rect.height << ")";
    return stream.str();
}

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

SegmentMaskAnalysis AnalyzeSegmentMask(const Mat& mask, const Rect& fallback_bbox)
{
    SegmentMaskAnalysis analysis;
    analysis.center = Point2f(fallback_bbox.x + fallback_bbox.width * 0.5f,
                              fallback_bbox.y + fallback_bbox.height * 0.5f);

    if (mask.empty()) {
        return analysis;
    }

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

    if (best_index < 0) {
        return analysis;
    }

    analysis.contour.reserve(contours[best_index].size());
    for (const Point& point : contours[best_index]) {
        analysis.contour.emplace_back(static_cast<float>(point.x), static_cast<float>(point.y));
    }

    if (analysis.contour.size() >= 3) {
        const RotatedRect rect = minAreaRect(analysis.contour);
        analysis.center = rect.center;

        Point2f points[4];
        rect.points(points);
        analysis.min_rect_corners = { points[0], points[1], points[2], points[3] };
    }
    return analysis;
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

float ComputeObbRayAngle(const Point2f& center, const Point2f& keep_point)
{
    const Point2f ray = keep_point - center;
    float ray_angle_deg = std::atan2(ray.y, ray.x) * 180.0f / static_cast<float>(CV_PI);
    if (ray_angle_deg < 0.0f) {
        ray_angle_deg += 360.0f;
    }
    return ray_angle_deg;
}

float ComputeFinalRayAngle(const Point2f& obb_center, const Point2f& keep_point)
{
    return ComputeObbRayAngle(obb_center, keep_point);
}

float PointLength(const Point2f& point)
{
    return std::hypot(point.x, point.y);
}

float NormalizeAngleDiffRad(float first, float second)
{
    float diff = std::fmod(first - second, 2.0f * static_cast<float>(CV_PI));
    if (diff > static_cast<float>(CV_PI)) {
        diff -= 2.0f * static_cast<float>(CV_PI);
    } else if (diff < -static_cast<float>(CV_PI)) {
        diff += 2.0f * static_cast<float>(CV_PI);
    }
    return diff;
}

float AngleBetweenVectors(const Point2f& first, const Point2f& second)
{
    const float first_length = PointLength(first);
    const float second_length = PointLength(second);
    if (first_length < 1e-6f || second_length < 1e-6f) {
        return numeric_limits<float>::quiet_NaN();
    }

    const float dot = first.dot(second) / (first_length * second_length);
    return std::acos(std::max(-1.0f, std::min(1.0f, dot)));
}

Point2f OffsetPointAlongRay(const Point2f& ray_start,
                            const Point2f& ray_end,
                            double offset_px)
{
    const Point2f ray = ray_end - ray_start;
    const float length = std::hypot(ray.x, ray.y);
    if (length < 1e-6f || std::fabs(offset_px) < 1e-6) {
        return ray_start;
    }

    const float scale = static_cast<float>(offset_px) / length;
    return ray_start + Point2f(ray.x * scale, ray.y * scale);
}

Point2f AveragePoint(const vector<Point2f>& points)
{
    Point2f sum(0.0f, 0.0f);
    if (points.empty()) {
        return sum;
    }
    for (const Point2f& point : points) {
        sum += point;
    }
    return sum * (1.0f / static_cast<float>(points.size()));
}

ObbEdgeBasis ResolveObbEdgeBasis(const vector<Point2f>& corners)
{
    ObbEdgeBasis basis;
    if (corners.size() != 4) {
        return basis;
    }

    const Point2f first_edge = corners[1] - corners[0];
    const Point2f second_edge = corners[3] - corners[0];
    if (PointLength(second_edge) > PointLength(first_edge)) {
        basis.long_index = 3;
        basis.short_index = 1;
        basis.angle_rad = std::atan2(second_edge.y, second_edge.x);
    } else {
        basis.long_index = 1;
        basis.short_index = 3;
        basis.angle_rad = std::atan2(first_edge.y, first_edge.x);
    }
    return basis;
}

vector<Point2f> RotateCornersAroundCenter(const vector<Point2f>& corners, float delta_angle)
{
    vector<Point2f> rotated;
    rotated.reserve(corners.size());
    const Point2f center = AveragePoint(corners);
    const float cosine = std::cos(delta_angle);
    const float sine = std::sin(delta_angle);
    for (const Point2f& point : corners) {
        const Point2f shifted = point - center;
        rotated.emplace_back(center.x + shifted.x * cosine - shifted.y * sine,
                             center.y + shifted.x * sine + shifted.y * cosine);
    }
    return rotated;
}

vector<Point2f> RotateGripToTargetLongAngle(const vector<Point2f>& grip_corners,
                                            float grip_angle,
                                            float target_angle)
{
    if (grip_corners.size() != 4) {
        return {};
    }

    return RotateCornersAroundCenter(grip_corners, NormalizeAngleDiffRad(target_angle, grip_angle));
}

vector<Point2f> ScaleObbLongEdge(const vector<Point2f>& corners, float scale)
{
    if (corners.size() != 4) {
        return {};
    }

    const Point2f center = AveragePoint(corners);
    const ObbEdgeBasis basis = ResolveObbEdgeBasis(corners);
    const Point2f long_vector = (corners[basis.long_index] - corners[0]) * scale;
    const Point2f short_vector = corners[basis.short_index] - corners[0];
    const Point2f new_p0 = center - (long_vector + short_vector) * 0.5f;
    const Point2f new_p1 = new_p0 + long_vector;
    const Point2f new_p2 = new_p1 + short_vector;
    const Point2f new_p3 = new_p0 + short_vector;
    return { new_p0, new_p1, new_p2, new_p3 };
}

Point2f ClosestPointOnSegment(const Point2f& point, const Point2f& start, const Point2f& end)
{
    const Point2f edge = end - start;
    const float length_squared = edge.dot(edge);
    if (length_squared < 1e-6f) {
        return start;
    }

    const float t = std::max(0.0f, std::min(1.0f, (point - start).dot(edge) / length_squared));
    return start + edge * t;
}

bool CandidateBeats(const AcRayGeometry& candidate, const AcRayGeometry& current)
{
    if (!current.valid) {
        return candidate.valid;
    }
    if (!candidate.valid) {
        return false;
    }
    if (candidate.ac_uses_long_edge != current.ac_uses_long_edge) {
        return candidate.ac_uses_long_edge;
    }

    constexpr float kDotTieEpsilon = 1e-4f;
    if (candidate.ac_dot > current.ac_dot + kDotTieEpsilon) {
        return true;
    }
    if (std::fabs(candidate.ac_dot - current.ac_dot) <= kDotTieEpsilon &&
        candidate.rotation_abs_rad < current.rotation_abs_rad) {
        return true;
    }
    return false;
}

float CrossProduct(const Point2f& first, const Point2f& second)
{
    return first.x * second.y - first.y * second.x;
}

bool PointOnSegment(const Point2f& point, const Point2f& start, const Point2f& end)
{
    constexpr float kEpsilon = 1e-4f;
    return point.x >= std::min(start.x, end.x) - kEpsilon &&
           point.x <= std::max(start.x, end.x) + kEpsilon &&
           point.y >= std::min(start.y, end.y) - kEpsilon &&
           point.y <= std::max(start.y, end.y) + kEpsilon &&
           std::fabs(CrossProduct(point - start, end - start)) <= kEpsilon;
}

bool LineSegmentsIntersect(const Point2f& first_start,
                           const Point2f& first_end,
                           const Point2f& second_start,
                           const Point2f& second_end)
{
    constexpr float kEpsilon = 1e-4f;
    const Point2f first_direction = first_end - first_start;
    const Point2f second_direction = second_end - second_start;
    if (std::hypot(first_direction.x, first_direction.y) < kEpsilon ||
        std::hypot(second_direction.x, second_direction.y) < kEpsilon) {
        return false;
    }

    const float first_cross_second_start = CrossProduct(first_direction, second_start - first_start);
    const float first_cross_second_end = CrossProduct(first_direction, second_end - first_start);
    const float second_cross_first_start = CrossProduct(second_direction, first_start - second_start);
    const float second_cross_first_end = CrossProduct(second_direction, first_end - second_start);

    if (((first_cross_second_start > kEpsilon && first_cross_second_end < -kEpsilon) ||
         (first_cross_second_start < -kEpsilon && first_cross_second_end > kEpsilon)) &&
        ((second_cross_first_start > kEpsilon && second_cross_first_end < -kEpsilon) ||
         (second_cross_first_start < -kEpsilon && second_cross_first_end > kEpsilon))) {
        return true;
    }

    return PointOnSegment(second_start, first_start, first_end) ||
           PointOnSegment(second_end, first_start, first_end) ||
           PointOnSegment(first_start, second_start, second_end) ||
           PointOnSegment(first_end, second_start, second_end);
}

int ResolveHeadTypeCode(int head_class_id, int side)
{
    if (head_class_id == kClassBig) {
        return side >= 0 ? 1 : 3;
    }
    if (head_class_id == kClassSmall) {
        return side >= 0 ? 4 : 2;
    }
    return 0;
}

int ResolveHeadSideFromSegmentCenter(const Point2f& left_center, const Point2f& segment_center)
{
    return left_center.x > segment_center.x ? 1 : -1;
}

bool IsFinitePoint(const Point2f& point)
{
    return std::isfinite(point.x) && std::isfinite(point.y);
}

AcRayGeometry BuildAcRayGeometry(const OBBDetection& grip_detection,
                                 const OBBDetection* head_detection,
                                 const SegRegion& segment)
{
    AcRayGeometry fallback;
    fallback.segment_center = segment.center;
    fallback.grip_center = grip_detection.rotated_rect.center;
    fallback.head_center = head_detection != nullptr ? head_detection->rotated_rect.center
                                                     : Point2f();
    fallback.arrow_start = grip_detection.rotated_rect.center;
    fallback.arrow_end = grip_detection.rotated_rect.center;
    if (head_detection == nullptr) {
        fallback.invalid_reason = "missing_head_candidate";
        return fallback;
    }
    if (grip_detection.corners.size() != 4 || head_detection->corners.size() != 4) {
        fallback.invalid_reason = "invalid_obb_corners";
        return fallback;
    }
    if (segment.min_rect_corners.size() != 4 || !IsFinitePoint(segment.center)) {
        fallback.invalid_reason = "invalid_segment_min_rect";
        return fallback;
    }

    const Point2f grip_center = AveragePoint(grip_detection.corners);
    const Point2f oa = grip_center - segment.center;
    if (PointLength(oa) < 1e-6f) {
        fallback.invalid_reason = "invalid_oa_geometry";
        return fallback;
    }
    const Point2f oa_unit = oa * (1.0f / PointLength(oa));
    const ObbEdgeBasis grip_basis = ResolveObbEdgeBasis(grip_detection.corners);
    const float head_long_angle = ResolveObbEdgeBasis(head_detection->corners).angle_rad;
    const float target_angles[] = {
        head_long_angle,
        head_long_angle + static_cast<float>(CV_PI) * 0.5f,
    };

    AcRayGeometry best_geometry;
    best_geometry.segment_center = segment.center;
    best_geometry.grip_center = grip_detection.rotated_rect.center;
    best_geometry.head_center = head_detection->rotated_rect.center;
    best_geometry.arrow_start = grip_detection.rotated_rect.center;
    best_geometry.arrow_end = grip_detection.rotated_rect.center;
    best_geometry.invalid_reason = "invalid_ac_geometry";

    for (float target_angle : target_angles) {
        AcRayGeometry candidate;
        candidate.segment_center = segment.center;
        candidate.grip_center = grip_detection.rotated_rect.center;
        candidate.head_center = head_detection->rotated_rect.center;
        candidate.raw_grip_long_angle_rad = grip_basis.angle_rad;
        candidate.target_long_angle_rad = target_angle;
        candidate.rotation_abs_rad = std::fabs(NormalizeAngleDiffRad(target_angle, grip_basis.angle_rad));
        candidate.aligned_corners =
            RotateGripToTargetLongAngle(grip_detection.corners, grip_basis.angle_rad, target_angle);
        candidate.extended_corners = ScaleObbLongEdge(candidate.aligned_corners, kExtendedObbScale);
        if (candidate.extended_corners.size() != 4) {
            candidate.invalid_reason = "invalid_aligned_grip";
            continue;
        }

        candidate.arrow_start = AveragePoint(candidate.extended_corners);
        if (PointLength(candidate.arrow_start - segment.center) < 1e-6f) {
            candidate.invalid_reason = "invalid_oa_geometry";
            continue;
        }

        float best_dot = -numeric_limits<float>::infinity();
        Point2f best_foot = candidate.arrow_start;
        bool best_edge_is_long = false;
        for (size_t i = 0; i < candidate.extended_corners.size(); ++i) {
            const Point2f edge = candidate.extended_corners[(i + 1) % candidate.extended_corners.size()] -
                                 candidate.extended_corners[i];
            const Point2f foot = ClosestPointOnSegment(candidate.arrow_start,
                                                       candidate.extended_corners[i],
                                                       candidate.extended_corners[(i + 1) % candidate.extended_corners.size()]);
            const Point2f ac = foot - candidate.arrow_start;
            const float ac_length = PointLength(ac);
            if (ac_length < 1e-6f) {
                continue;
            }

            const float dot = oa_unit.dot(ac * (1.0f / ac_length));
            if (dot > best_dot) {
                best_dot = dot;
                best_foot = foot;
                best_edge_is_long = PointLength(edge) >
                                    std::min(PointLength(candidate.extended_corners[1] - candidate.extended_corners[0]),
                                             PointLength(candidate.extended_corners[3] - candidate.extended_corners[0]));
            }
        }

        if (!std::isfinite(best_dot) || PointLength(best_foot - candidate.arrow_start) < 1e-6f) {
            candidate.invalid_reason = "invalid_ac_geometry";
            continue;
        }

        candidate.arrow_end = best_foot;
        candidate.ac_dot = best_dot;
        candidate.ac_uses_long_edge = best_edge_is_long;
        candidate.final_long_angle_rad = ResolveObbEdgeBasis(candidate.aligned_corners).angle_rad;
        candidate.valid = true;
        if (CandidateBeats(candidate, best_geometry)) {
            best_geometry = std::move(candidate);
        }
    }

    if (!best_geometry.valid) {
        return best_geometry.invalid_reason.empty() ? fallback : best_geometry;
    }
    return best_geometry;
}

bool ExtendedObbTouchesSegmentMask(const vector<Point2f>& extended_corners,
                                   const SegRegion& matched_segment,
                                   const SegRegion& segment,
                                   int image_width,
                                   int image_height,
                                   int segment_index,
                                   bool debug_enabled,
                                   vector<pair<Rect, Mat>>* collisions)
{
    if (extended_corners.size() < 3 || segment.mask.empty()) {
        return false;
    }
    if (segment.mask.type() != CV_8U ||
        segment.mask.cols != image_width ||
        segment.mask.rows != image_height) {
        if (debug_enabled) {
            cout << "[PostprocessDebug] mask_collision_skip"
                 << " segment_index=" << segment_index
                 << " reason=invalid_mask"
                 << " mask_size=" << segment.mask.cols << "x" << segment.mask.rows
                 << " image_size=" << image_width << "x" << image_height
                 << endl;
        }
        return false;
    }
    if (matched_segment.mask.empty() ||
        matched_segment.mask.type() != CV_8U ||
        matched_segment.mask.cols != image_width ||
        matched_segment.mask.rows != image_height) {
        if (debug_enabled) {
            cout << "[PostprocessDebug] mask_collision_skip"
                 << " segment_index=" << segment_index
                 << " reason=invalid_matched_mask"
                 << " matched_mask_size=" << matched_segment.mask.cols << "x" << matched_segment.mask.rows
                 << " image_size=" << image_width << "x" << image_height
                 << endl;
        }
        return false;
    }

    const Rect image_rect(0, 0, image_width, image_height);
    const Rect roi = boundingRect(extended_corners) & image_rect;
    if (roi.empty()) {
        return false;
    }

    vector<Point> polygon;
    polygon.reserve(extended_corners.size());
    for (const Point2f& point : extended_corners) {
        polygon.emplace_back(cvRound(point.x) - roi.x, cvRound(point.y) - roi.y);
    }

    Mat extended_mask(roi.height, roi.width, CV_8U, Scalar(0));
    fillConvexPoly(extended_mask, polygon, Scalar(255), LINE_8);

    Mat overlap;
    bitwise_and(extended_mask, segment.mask(roi), overlap);
    overlap.setTo(Scalar(0), matched_segment.mask(roi));
    if (countNonZero(overlap) <= 0) {
        return false;
    }

    if (collisions != nullptr) {
        collisions->push_back({ roi, overlap.clone() });
    }
    return true;
}

string ResolveHeadTypeText(int head_type_code)
{
    switch (head_type_code) {
    case 1: return "大上右";
    case 2: return "小上左";
    case 3: return "大上左";
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
        const SegmentMaskAnalysis analysis = AnalyzeSegmentMask(region.mask, region.bbox);
        region.center = analysis.center;
        region.contour = analysis.contour;
        region.min_rect_corners = analysis.min_rect_corners;
        segments.push_back(std::move(region));
    }

    return segments;
}

vector<ObbRegion> BuildRawObbRegions(const vector<OBBDetection>& obb_output,
                                     const vector<string>& obb_class_names,
                                     int image_width,
                                     int image_height)
{
    vector<ObbRegion> regions;
    regions.reserve(obb_output.size());
    for (const auto& detection : obb_output) {
        ObbRegion region;
        region.class_id = detection.class_id;
        region.class_name = GetClassName(obb_class_names, detection.class_id);
        region.confidence = detection.conf;
        region.center = detection.rotated_rect.center;
        region.corners = detection.corners;
        region.bbox = BuildPoseBoundingRect(region.corners, image_width, image_height);
        regions.push_back(std::move(region));
    }
    return regions;
}

PoseDetection BuildPoseDetection(const OBBDetection& detection,
                                 const vector<string>& obb_class_names,
                                 int image_width,
                                 int image_height,
                                 int matched_segment_index,
                                 const string& segment_class_name,
                                 const SegRegion& segment,
                                 const Point2f& segment_center,
                                 const Point2f& x_point,
                                 const Point2f& small_point,
                                 const Point2f& small_arrow_start,
                                 const Point2f& small_arrow_end,
                                 bool has_small_ray,
                                 int small_ray_quadrant,
                                 int head_type_code,
                                 int head_class_id,
                                 vector<pair<Rect, Mat>> mask_collisions,
                                 bool can_grab,
                                 const AcRayGeometry& ac_geometry,
                                 double center_ray_offset_px)
{
    const auto& rect = detection.rotated_rect;
    const Point2f original_center = rect.center;
    const Point2f arrow_start = ac_geometry.valid ? ac_geometry.arrow_start : original_center;
    const Point2f arrow_end = ac_geometry.valid ? ac_geometry.arrow_end : original_center;
    const Point2f shifted_center = OffsetPointAlongRay(arrow_start,
                                                       arrow_end,
                                                       center_ray_offset_px);
    const float ray_angle_deg = ComputeFinalRayAngle(arrow_start, arrow_end);

    PoseDetection pose;
    pose.class_id = detection.class_id;
    pose.class_name = GetClassName(obb_class_names, detection.class_id);
    pose.segment_class_name = segment_class_name;
    pose.matched_segment_index = matched_segment_index;
    pose.confidence = detection.conf;
    pose.center_x = QuantizeStable(shifted_center.x, kPoseCoordinateStepPx);
    pose.center_y = QuantizeStable(shifted_center.y, kPoseCoordinateStepPx);
    pose.angle_deg = QuantizeStable(ray_angle_deg, kPoseAngleStepDeg);
    pose.pick_status_code = can_grab ? kPickStatusCanGrab : kPickStatusCannotGrab;
    pose.head_type_code = can_grab ? head_type_code : 0;
    pose.head_class_id = can_grab ? head_class_id : -1;
    pose.head_type_text = ResolveHeadTypeText(pose.head_type_code);
    pose.obb_center = original_center;
    pose.seg_center = segment_center;
    pose.x_point = x_point;
    pose.small_point = small_point;
    pose.has_small_point = has_small_ray;
    pose.small_arrow_start = small_arrow_start;
    pose.small_arrow_end = small_arrow_end;
    pose.has_small_ray = has_small_ray;
    pose.small_ray_quadrant = small_ray_quadrant;
    pose.arrow_start = arrow_start;
    pose.arrow_end = arrow_end;
    pose.corners = ac_geometry.valid ? ac_geometry.aligned_corners : detection.corners;
    pose.extended_corners = ac_geometry.valid ? ac_geometry.extended_corners : vector<Point2f>{};
    pose.bbox = BuildPoseBoundingRect(pose.corners, image_width, image_height);
    pose.mask_collisions = std::move(mask_collisions);
    pose.can_grab = can_grab;
    return pose;
}

vector<const OBBDetection*> FindClassDetectionsInsideSegment(const vector<OBBDetection>& obb_output,
                                                            int class_id,
                                                            const SegRegion& segment,
                                                            const OBBDetection* excluded_detection)
{
    vector<const OBBDetection*> detections;
    for (const auto& detection : obb_output) {
        if (&detection == excluded_detection) {
            continue;
        }
        if (detection.class_id != class_id) {
            continue;
        }
        const Point2f center = detection.rotated_rect.center;
        if (MaskContainsPoint(segment.mask, center)) {
            detections.push_back(&detection);
        }
    }
    return detections;
}

int CountClassDetectionsInsideSegment(const vector<OBBDetection>& obb_output,
                                      int class_id,
                                      const SegRegion& segment)
{
    int count = 0;
    for (const auto& detection : obb_output) {
        if (detection.class_id != class_id) {
            continue;
        }
        if (MaskContainsPoint(segment.mask, detection.rotated_rect.center)) {
            ++count;
        }
    }
    return count;
}

const OBBDetection* ResolveBestHeadDetection(const vector<const OBBDetection*>& head_candidates,
                                             const Point2f& segment_center,
                                             const Point2f& grip_center,
                                             float* best_aob_angle_deg,
                                             float* best_aob_diff_deg)
{
    const OBBDetection* best_detection = nullptr;
    float best_angle_diff = numeric_limits<float>::infinity();
    float best_confidence = -numeric_limits<float>::infinity();
    float best_angle = numeric_limits<float>::quiet_NaN();
    const Point2f oa = grip_center - segment_center;
    if (PointLength(oa) < 1e-6f) {
        return nullptr;
    }

    for (const OBBDetection* candidate : head_candidates) {
        if (candidate == nullptr) {
            continue;
        }
        const float angle = AngleBetweenVectors(oa, candidate->rotated_rect.center - segment_center);
        if (!std::isfinite(angle)) {
            continue;
        }

        const float angle_diff = std::fabs(angle - static_cast<float>(CV_PI) * 0.5f);
        constexpr float kTieEpsilon = 1e-4f;
        if (angle_diff + kTieEpsilon < best_angle_diff ||
            (std::fabs(angle_diff - best_angle_diff) <= kTieEpsilon &&
             (candidate->conf > best_confidence + kStableTieEpsilon ||
              (std::fabs(candidate->conf - best_confidence) <= kStableTieEpsilon &&
               (best_detection == nullptr || ObbStableLess(*candidate, *best_detection)))))) {
            best_angle = angle;
            best_angle_diff = angle_diff;
            best_confidence = candidate->conf;
            best_detection = candidate;
        }
    }
    if (best_aob_angle_deg != nullptr) {
        *best_aob_angle_deg = best_angle * 180.0f / static_cast<float>(CV_PI);
    }
    if (best_aob_diff_deg != nullptr) {
        *best_aob_diff_deg = best_angle_diff * 180.0f / static_cast<float>(CV_PI);
    }
    return best_detection;
}

vector<PoseDetection> BuildFilteredPoseDetections(const vector<OBBDetection>& obb_output,
                                                  const vector<string>& obb_class_names,
                                                  const vector<SegRegion>& segments,
                                                  int image_width,
                                                  int image_height,
                                                  const FramePostprocessConfig& config,
                                                  bool debug_enabled)
{
    vector<PoseDetection> detections;
    detections.reserve(obb_output.size());

    for (int detection_index = 0; detection_index < static_cast<int>(obb_output.size()); ++detection_index) {
        const auto& detection = obb_output[detection_index];
        if (debug_enabled) {
            cout << "[PostprocessDebug] obb index=" << detection_index
                 << " class_id=" << detection.class_id
                 << " class_name=" << GetClassName(obb_class_names, detection.class_id)
                 << " conf=" << detection.conf
                 << " center=" << PointText(detection.rotated_rect.center)
                 << " angle=" << detection.rotated_rect.angle
                 << endl;
        }

        if (detection.class_id != kClassLeft) {
            if (debug_enabled) {
                cout << "[PostprocessDebug] obb_filter index=" << detection_index
                     << " reason=not_left_grip"
                     << endl;
            }
            continue;
        }

        const Point2f center = detection.rotated_rect.center;
        int matched_segment_index = -1;
        int match_count = 0;

        for (int i = 0; i < static_cast<int>(segments.size()); ++i) {
            if (MaskContainsPoint(segments[i].mask, center)) {
                matched_segment_index = i;
                ++match_count;
            }
        }

        if (debug_enabled) {
            cout << "[PostprocessDebug] grip_obb index=" << detection_index
                 << " segment_match_count=" << match_count
                 << " matched_segment_index=" << matched_segment_index
                 << endl;
        }

        if (match_count != 1) {
            if (debug_enabled) {
                cout << "[PostprocessDebug] obb_filter index=" << detection_index
                     << " reason=segment_match_count!=1"
                     << endl;
            }
            continue;
        }

        const SegRegion& matched_segment = segments[matched_segment_index];
        const int left_count_in_segment =
            CountClassDetectionsInsideSegment(obb_output, kClassLeft, matched_segment);
        const int big_count_in_segment =
            CountClassDetectionsInsideSegment(obb_output, kClassBig, matched_segment);
        const int small_count_in_segment =
            CountClassDetectionsInsideSegment(obb_output, kClassSmall, matched_segment);
        const int head_count_in_segment = big_count_in_segment + small_count_in_segment;
        const bool segment_structure_valid = left_count_in_segment == 1 && head_count_in_segment > 0;
        vector<const OBBDetection*> head_candidates =
            FindClassDetectionsInsideSegment(obb_output, kClassBig, matched_segment, &detection);
        const vector<const OBBDetection*> small_candidates =
            FindClassDetectionsInsideSegment(obb_output, kClassSmall, matched_segment, &detection);
        head_candidates.insert(head_candidates.end(), small_candidates.begin(), small_candidates.end());

        Point2f x_point = center;
        Point2f head_point;
        Point2f head_arrow_start;
        Point2f head_arrow_end;
        bool has_head_ray = false;
        int head_side = 0;
        int head_type_code = 0;
        bool can_grab = false;
        bool ray_intersects_head_ray = false;
        bool touches_other = false;
        int touched_segment_index = -1;
        vector<pair<Rect, Mat>> mask_collisions;
        float selected_aob_angle_deg = numeric_limits<float>::quiet_NaN();
        float selected_aob_diff_deg = numeric_limits<float>::quiet_NaN();
        const OBBDetection* head_detection =
            ResolveBestHeadDetection(head_candidates,
                                     matched_segment.center,
                                     detection.rotated_rect.center,
                                     &selected_aob_angle_deg,
                                     &selected_aob_diff_deg);
        const AcRayGeometry ac_geometry = BuildAcRayGeometry(detection, head_detection, matched_segment);
        if (ac_geometry.valid) {
            for (int i = 0; i < static_cast<int>(segments.size()); ++i) {
                if (i == matched_segment_index) {
                    continue;
                }
                if (ExtendedObbTouchesSegmentMask(ac_geometry.extended_corners,
                                                 matched_segment,
                                                 segments[i],
                                                 image_width,
                                                 image_height,
                                                 i,
                                                 debug_enabled,
                                                 &mask_collisions)) {
                    touches_other = true;
                    if (touched_segment_index < 0) {
                        touched_segment_index = i;
                    }
                }
            }
        }

        if (head_detection != nullptr) {
            const RotatedRect& head_rect = head_detection->rotated_rect;
            head_point = head_rect.center;
            head_arrow_start = matched_segment.center;
            head_arrow_end = head_point;
            head_side = ResolveHeadSideFromSegmentCenter(detection.rotated_rect.center, matched_segment.center);
            head_type_code = ResolveHeadTypeCode(head_detection->class_id, head_side);
            has_head_ray = head_type_code > 0;

            ray_intersects_head_ray = has_head_ray &&
                                      ac_geometry.valid &&
                                      LineSegmentsIntersect(ac_geometry.arrow_start,
                                                            ac_geometry.arrow_end,
                                                            head_arrow_start,
                                                            head_arrow_end);
            can_grab = has_head_ray &&
                       ac_geometry.valid &&
                       !ray_intersects_head_ray &&
                       !touches_other &&
                       segment_structure_valid;
        }

        if (debug_enabled) {
            cout << "[PostprocessDebug] target index=" << detection_index
                 << " matched_segment_index=" << matched_segment_index
                 << " head_candidate_count=" << head_candidates.size()
                 << " has_head=" << BoolText(head_detection != nullptr)
                 << " head_class_id=" << (head_detection != nullptr ? head_detection->class_id : -1)
                 << " selected_head_conf=" << (head_detection != nullptr ? head_detection->conf : 0.0f)
                 << " selected_aob_angle_deg=" << selected_aob_angle_deg
                 << " selected_aob_diff_deg=" << selected_aob_diff_deg
                 << " head_side=" << head_side
                 << " head_side_basis=left_x_vs_segment_center"
                 << " D508=" << head_type_code
                 << " left_count_in_segment=" << left_count_in_segment
                 << " big_count_in_segment=" << big_count_in_segment
                 << " small_count_in_segment=" << small_count_in_segment
                 << " has_head_ray=" << BoolText(has_head_ray)
                 << " ac_geometry_valid=" << BoolText(ac_geometry.valid)
                 << " ac_uses_long_edge=" << BoolText(ac_geometry.ac_uses_long_edge)
                 << " ac_dot=" << ac_geometry.ac_dot
                 << " O=" << PointText(ac_geometry.segment_center)
                 << " A=" << PointText(ac_geometry.grip_center)
                 << " B=" << PointText(ac_geometry.head_center)
                 << " C=" << PointText(ac_geometry.arrow_end)
                 << " arrow_start=" << PointText(ac_geometry.arrow_start)
                 << " raw_grip_long_angle_deg=" << ac_geometry.raw_grip_long_angle_rad * 180.0f / static_cast<float>(CV_PI)
                 << " target_long_angle_deg=" << ac_geometry.target_long_angle_rad * 180.0f / static_cast<float>(CV_PI)
                 << " final_long_angle_deg=" << ac_geometry.final_long_angle_rad * 180.0f / static_cast<float>(CV_PI)
                 << " ac_angle_deg=" << ComputeFinalRayAngle(ac_geometry.arrow_start, ac_geometry.arrow_end)
                 << " ray_logic=v1.0.13_seg_center_ac"
                 << " ray_intersects_head=" << BoolText(ray_intersects_head_ray)
                 << " extended_touches_other=" << BoolText(touches_other);
            if (touches_other) {
                cout << " touched_segment_index=" << touched_segment_index;
            }
            cout << " can_grab=" << BoolText(can_grab);
            if (touches_other) {
                cout << " reason=extended_touches_other_mask";
            } else if (head_detection == nullptr) {
                cout << " reason=missing_head_candidate";
            } else if (!segment_structure_valid) {
                cout << " reason=invalid_segment_obb_structure";
            } else if (!ac_geometry.valid) {
                cout << " reason=" << ac_geometry.invalid_reason;
            } else if (!has_head_ray) {
                cout << " reason=invalid_head_type";
            } else if (ray_intersects_head_ray) {
                cout << " reason=ray_intersects_head";
            } else {
                cout << " reason=can_grab";
            }
            cout << endl;
        }

        detections.push_back(BuildPoseDetection(detection,
                                                obb_class_names,
                                                image_width,
                                                image_height,
                                                matched_segment_index,
                                                matched_segment.class_name,
                                                matched_segment,
                                                matched_segment.center,
                                                x_point,
                                                head_point,
                                                head_arrow_start,
                                                head_arrow_end,
                                                has_head_ray,
                                                head_side,
                                                head_type_code,
                                                head_detection != nullptr ? head_detection->class_id : -1,
                                                std::move(mask_collisions),
                                                can_grab,
                                                ac_geometry,
                                                config.center_ray_offset_px));
    }

    return detections;
}

void LogPostprocessDebugSummary(const vector<OBBDetection>& obb_output,
                                const FrameInferenceResult& result)
{
    cout << "[PostprocessDebug] summary"
         << " obb_count=" << obb_output.size()
         << " seg_count=" << result.segments.size()
         << " detection_count=" << result.detections.size()
         << " primary_index=" << result.primary_index
         << " D506=" << result.pick_status_code
         << " D508=" << result.head_type_code
         << endl;

    for (int i = 0; i < static_cast<int>(result.segments.size()); ++i) {
        const SegRegion& segment = result.segments[i];
        cout << "[PostprocessDebug] seg index=" << i
             << " class_id=" << segment.class_id
             << " class_name=" << segment.class_name
             << " conf=" << segment.confidence
             << " bbox=" << RectText(segment.bbox)
             << " center=" << PointText(segment.center)
             << " has_min_rect_corners=" << BoolText(!segment.min_rect_corners.empty())
             << endl;
    }
}

void ResolvePrimaryDetection(FrameInferenceResult& result)
{
    float best_score = -numeric_limits<float>::infinity();
    int best_index = -1;
    int grabbable_count = 0;
    for (int i = 0; i < static_cast<int>(result.detections.size()); ++i) {
        auto& detection = result.detections[i];
        if (!detection.can_grab) {
            detection.pick_status_code = kPickStatusCannotGrab;
            continue;
        }

        ++grabbable_count;
        const float score = detection.confidence;
        if (score > best_score + kStableTieEpsilon ||
            (fabs(score - best_score) <= kStableTieEpsilon &&
             (best_index < 0 || PoseStableLess(detection, result.detections[best_index])))) {
            best_score = score;
            best_index = i;
        }
    }
    result.primary_index = best_index;

    if (grabbable_count == 0) {
        result.pick_status_code = kPickStatusCannotGrab;
        result.primary_index = -1;
        result.head_type_code = 0;
        result.head_type_text = ResolveHeadTypeText(0);
        return;
    }

    result.pick_status_code = grabbable_count == 1 ? kPickStatusCanGrab : kPickStatusMultiGrab;
    for (auto& detection : result.detections) {
        detection.pick_status_code = detection.can_grab ? result.pick_status_code : kPickStatusCannotGrab;
    }

    if (result.primary_index >= 0 && result.primary_index < static_cast<int>(result.detections.size())) {
        result.head_type_code = result.detections[result.primary_index].head_type_code;
        result.head_type_text = result.detections[result.primary_index].head_type_text;
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
                                               double seg_inference_ms,
                                               const FramePostprocessConfig& config)
{
    FrameInferenceResult result;
    result.image_width = image_width;
    result.image_height = image_height;
    result.obb_inference_ms = obb_inference_ms;
    result.seg_inference_ms = seg_inference_ms;
    result.total_inference_ms = obb_inference_ms + seg_inference_ms;
    result.raw_obb_regions = BuildRawObbRegions(obb_output, obb_class_names, image_width, image_height);
    result.segments = BuildSegRegions(seg_output, seg_class_names, image_width, image_height);
    const bool debug_enabled = config.debug_logging_enabled || IsPostprocessDebugEnabled();
    result.detections = BuildFilteredPoseDetections(obb_output,
                                                    obb_class_names,
                                                    result.segments,
                                                    image_width,
                                                    image_height,
                                                    config,
                                                    debug_enabled);

    ResolvePrimaryDetection(result);
    if (debug_enabled) {
        LogPostprocessDebugSummary(obb_output, result);
    }
    return result;
}

