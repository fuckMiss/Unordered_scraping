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

struct SegmentMaskAnalysis
{
    Point2f center;
    vector<Point2f> contour;
    vector<Point2f> min_rect_corners;
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

Size2f BuildExtendedObbSize(const RotatedRect& rect)
{
    const float width = rect.size.width;
    const float height = rect.size.height;

    Size2f extended_size = rect.size;
    if (width > height) {
        extended_size.width *= kExtendedObbScale;
    } else {
        extended_size.height *= kExtendedObbScale;
    }
    return extended_size;
}

vector<Point2f> BuildExtendedObbCorners(const OBBDetection& detection)
{
    const RotatedRect& rect = detection.rotated_rect;
    const Size2f extended_size = BuildExtendedObbSize(rect);

    Point2f points[4];
    RotatedRect(rect.center, extended_size, rect.angle).points(points);
    return { points[0], points[1], points[2], points[3] };
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

float SquaredDistanceToForwardRaySegment(const Point2f& ray_start,
                                         const Point2f& ray_end,
                                         const Point2f& point,
                                         float max_scale)
{
    const Point2f ray = ray_end - ray_start;
    const float length_squared = ray.dot(ray);
    if (length_squared < 1e-6f) {
        const Point2f delta = point - ray_start;
        return delta.dot(delta);
    }

    const Point2f delta = point - ray_start;
    const float projection_scale = delta.dot(ray) / length_squared;
    if (projection_scale < 0.0f || projection_scale > max_scale) {
        return numeric_limits<float>::infinity();
    }

    const Point2f closest = ray_start + ray * projection_scale;
    const Point2f distance = point - closest;
    return distance.dot(distance);
}

bool ForwardRaySegmentPassesNearPoint(const Point2f& ray_start,
                                      const Point2f& ray_end,
                                      const Point2f& point,
                                      float max_scale,
                                      float tolerance_px)
{
    const float distance_squared = SquaredDistanceToForwardRaySegment(ray_start, ray_end, point, max_scale);
    return distance_squared <= tolerance_px * tolerance_px;
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

Point2f ComputeKeepPointForRect(const Point2f& center,
                                const Size2f& size,
                                float angle_deg,
                                int class_id,
                                const Point2f& segment_center)
{
    const float angle_rad = angle_deg * static_cast<float>(CV_PI) / 180.0f;
    float dx = 0.0f;
    float dy = 0.0f;
    if (size.width > size.height) {
        dy = size.height * 0.5f;
    } else {
        dx = size.width * 0.5f;
    }

    const float c = std::cos(angle_rad);
    const float s = std::sin(angle_rad);
    const Point2f offset(dx * c - dy * s, dx * s + dy * c);
    const Point2f point_a = center + offset;
    const Point2f point_b = center - offset;

    bool use_point_a = true;
    if (class_id == kClassLeft && point_b.x < point_a.x) {
        use_point_a = false;
    }

    if (class_id == kClassLeft) {
        const Point2f keep_point = use_point_a ? point_a : point_b;
        const Point2f opposite_point = use_point_a ? point_b : point_a;
        constexpr float kDisplayRayScale = 4.0f;
        const float tolerance_px = std::min(size.width, size.height) * 0.5f;
        if (ForwardRaySegmentPassesNearPoint(center,
                                             keep_point,
                                             segment_center,
                                             kDisplayRayScale,
                                             tolerance_px)) {
            use_point_a = !use_point_a;
        }
    }
    return use_point_a ? point_a : point_b;
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

bool PointInsideConvexPolygon(const Point2f& point, const vector<Point2f>& polygon)
{
    if (polygon.size() < 3) {
        return false;
    }

    bool has_positive = false;
    bool has_negative = false;
    constexpr float kEpsilon = 1e-4f;
    for (size_t i = 0; i < polygon.size(); ++i) {
        const Point2f start = polygon[i];
        const Point2f end = polygon[(i + 1) % polygon.size()];
        const float cross = CrossProduct(end - start, point - start);
        if (cross > kEpsilon) {
            has_positive = true;
        } else if (cross < -kEpsilon) {
            has_negative = true;
        }
        if (has_positive && has_negative) {
            return false;
        }
    }
    return true;
}

bool ConvexPolygonsIntersect(const vector<Point2f>& first, const vector<Point2f>& second)
{
    if (first.size() < 3 || second.size() < 3) {
        return false;
    }

    for (size_t i = 0; i < first.size(); ++i) {
        const Point2f first_start = first[i];
        const Point2f first_end = first[(i + 1) % first.size()];
        for (size_t j = 0; j < second.size(); ++j) {
            if (LineSegmentsIntersect(first_start,
                                      first_end,
                                      second[j],
                                      second[(j + 1) % second.size()])) {
                return true;
            }
        }
    }

    return PointInsideConvexPolygon(first.front(), second) ||
           PointInsideConvexPolygon(second.front(), first);
}

int ResolveHeadSide(const Point2f& left_center,
                    const Point2f& keep_point,
                    const Point2f& head_center)
{
    const Point2f forward = keep_point - left_center;
    const float length = std::hypot(forward.x, forward.y);
    if (length < 1e-6f) {
        return 1;
    }

    const Point2f forward_unit(forward.x / length, forward.y / length);
    const Point2f right_axis(forward_unit.y, -forward_unit.x);
    const Point2f head_vector = head_center - left_center;
    return head_vector.dot(right_axis) >= 0.0f ? 1 : -1;
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
                                 const Point2f& keep_point,
                                 double center_ray_offset_px)
{
    const auto& rect = detection.rotated_rect;
    const Point2f original_center = rect.center;
    const Point2f shifted_center = OffsetPointAlongRay(original_center,
                                                       keep_point,
                                                       center_ray_offset_px);
    const float ray_angle_deg = ComputeFinalRayAngle(original_center, keep_point);

    PoseDetection pose;
    pose.class_id = detection.class_id;
    pose.class_name = GetClassName(obb_class_names, detection.class_id);
    pose.segment_class_name = segment_class_name;
    pose.matched_segment_index = matched_segment_index;
    pose.confidence = detection.conf;
    pose.center_x = shifted_center.x;
    pose.center_y = shifted_center.y;
    pose.angle_deg = ray_angle_deg;
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
    pose.arrow_start = original_center;
    pose.arrow_end = keep_point;
    pose.corners = detection.corners;
    pose.extended_corners = BuildExtendedObbCorners(detection);
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

const OBBDetection* ResolveBestHeadDetection(const vector<const OBBDetection*>& head_candidates)
{
    const OBBDetection* best_detection = nullptr;
    float best_confidence = -numeric_limits<float>::infinity();
    for (const OBBDetection* candidate : head_candidates) {
        if (candidate == nullptr) {
            continue;
        }
        if (candidate->conf > best_confidence) {
            best_confidence = candidate->conf;
            best_detection = candidate;
        }
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

        const vector<Point2f> extended_corners = BuildExtendedObbCorners(detection);
        const SegRegion& matched_segment = segments[matched_segment_index];
        bool touches_other = false;
        int touched_segment_index = -1;
        vector<pair<Rect, Mat>> mask_collisions;
        for (int i = 0; i < static_cast<int>(segments.size()); ++i) {
            if (i == matched_segment_index) {
                continue;
            }
            if (ExtendedObbTouchesSegmentMask(extended_corners,
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

        const int left_count_in_segment =
            CountClassDetectionsInsideSegment(obb_output, kClassLeft, matched_segment);
        const int big_count_in_segment =
            CountClassDetectionsInsideSegment(obb_output, kClassBig, matched_segment);
        const int small_count_in_segment =
            CountClassDetectionsInsideSegment(obb_output, kClassSmall, matched_segment);
        const int head_count_in_segment = big_count_in_segment + small_count_in_segment;
        const bool segment_structure_valid =
            left_count_in_segment == 1 &&
            head_count_in_segment == 1 &&
            !(big_count_in_segment > 0 && small_count_in_segment > 0);
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
        const Point2f keep_point = ComputeKeepPointForRect(detection.rotated_rect.center,
                                                           detection.rotated_rect.size,
                                                           detection.rotated_rect.angle,
                                                           detection.class_id,
                                                           matched_segment.center);
        const OBBDetection* head_detection = ResolveBestHeadDetection(head_candidates);
        if (head_detection != nullptr) {
            const RotatedRect& head_rect = head_detection->rotated_rect;
            head_point = head_rect.center;
            head_arrow_start = matched_segment.center;
            head_arrow_end = head_point;
            head_side = ResolveHeadSide(detection.rotated_rect.center, keep_point, head_rect.center);
            head_type_code = ResolveHeadTypeCode(head_detection->class_id, head_side);
            has_head_ray = head_type_code > 0;

            ray_intersects_head_ray = has_head_ray &&
                                      LineSegmentsIntersect(detection.rotated_rect.center,
                                                            keep_point,
                                                            head_arrow_start,
                                                            head_arrow_end);
            can_grab = has_head_ray &&
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
                 << " head_side=" << head_side
                 << " head_side_basis=left_local_keep"
                 << " D508=" << head_type_code
                 << " left_count_in_segment=" << left_count_in_segment
                 << " big_count_in_segment=" << big_count_in_segment
                 << " small_count_in_segment=" << small_count_in_segment
                 << " has_head_ray=" << BoolText(has_head_ray)
                 << " ray_logic=v1.0.6_raw_obb_oa"
                 << " ray_intersects_head=" << BoolText(ray_intersects_head_ray)
                 << " extended_touches_other=" << BoolText(touches_other);
            if (touches_other) {
                cout << " touched_segment_index=" << touched_segment_index;
            }
            cout << " can_grab=" << BoolText(can_grab);
            if (touches_other) {
                cout << " reason=extended_touches_other_mask";
            } else if (!segment_structure_valid) {
                cout << " reason=invalid_segment_obb_structure";
            } else if (head_detection == nullptr) {
                cout << " reason=missing_big_or_small";
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
                                                keep_point,
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
    int grabbable_count = 0;
    for (int i = 0; i < static_cast<int>(result.detections.size()); ++i) {
        auto& detection = result.detections[i];
        if (!detection.can_grab) {
            detection.pick_status_code = kPickStatusCannotGrab;
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

