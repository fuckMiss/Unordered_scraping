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

float QuantizeStable(float value, float step)
{
    if (!isfinite(value) || step <= 0.0f) {
        return value;
    }
    return round(value / step) * step;
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

float NormalizeAngleDeg(float angle_deg)
{
    if (!isfinite(angle_deg)) {
        return angle_deg;
    }
    angle_deg = fmod(angle_deg, 360.0f);
    if (angle_deg < 0.0f) {
        angle_deg += 360.0f;
    }
    return angle_deg;
}

float PointLength(const Point2f& point)
{
    return std::hypot(point.x, point.y);
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

int ResolveHeadSideFromGripLocalAc(const Point2f& grip_center,
                                   const Point2f& ac_point,
                                   const Point2f& head_center)
{
    const Point2f forward = ac_point - grip_center;
    const float length = PointLength(forward);
    if (length < 1e-6f) {
        return 0;
    }

    const Point2f right_axis(forward.y / length, -forward.x / length);
    const Point2f head_vector = head_center - grip_center;
    return head_vector.dot(right_axis) >= 0.0f ? 1 : -1;
}

bool IsFinitePoint(const Point2f& point)
{
    return std::isfinite(point.x) && std::isfinite(point.y);
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

struct EdgeFootCandidate
{
    Point2f foot;
    Point2f edge_start;
    Point2f edge_end;
};

struct ScriptGeometryResult
{
    bool valid = false;
    string invalid_reason;
    Point2f segment_center;
    Point2f grip_center;
    Point2f head_center;
    Point2f arrow_start;
    Point2f arrow_end;
    vector<Point2f> base_corners;
    vector<Point2f> scaled_corners;
    vector<pair<Rect, Mat>> mask_collisions;
    float ac_dot = -numeric_limits<float>::infinity();
    float selected_aob_angle_deg = numeric_limits<float>::quiet_NaN();
    float selected_aob_diff_deg = numeric_limits<float>::quiet_NaN();
    float final_ac_deg = numeric_limits<float>::quiet_NaN();
    float grip_long_angle_deg = numeric_limits<float>::quiet_NaN();
    float max_seg_iou = 0.0f;
    int head_side = 0;
    int head_type_code = 0;
    int head_class_id = -1;
    bool collision_with_other_seg = false;
};

bool SegmentMaskValid(const SegRegion& segment, int image_width, int image_height)
{
    return !segment.mask.empty() &&
           segment.mask.type() == CV_8U &&
           segment.mask.cols == image_width &&
           segment.mask.rows == image_height;
}

vector<EdgeFootCandidate> GetPerpendicularFootPoints(const vector<Point2f>& obb_4pts,
                                                     const Point2f& center_A)
{
    vector<EdgeFootCandidate> foot_list;
    foot_list.reserve(obb_4pts.size());
    for (size_t i = 0; i < obb_4pts.size(); ++i) {
        const Point2f& p_start = obb_4pts[i];
        const Point2f& p_end = obb_4pts[(i + 1) % obb_4pts.size()];
        const Point2f foot = ClosestPointOnSegment(center_A, p_start, p_end);
        foot_list.push_back({ foot, p_start, p_end });
    }
    return foot_list;
}

void ShiftEdgeSegment(const Point2f& p1,
                      const Point2f& p2,
                      const Point2f& shift_normal,
                      float shift_dist,
                      Point2f* out_p1,
                      Point2f* out_p2)
{
    if (out_p1 != nullptr) {
        *out_p1 = p1 + shift_normal * shift_dist;
    }
    if (out_p2 != nullptr) {
        *out_p2 = p2 + shift_normal * shift_dist;
    }
}

vector<Point2f> SampleLinePoints(const Point2f& p1, const Point2f& p2, int num_sample)
{
    vector<Point2f> points;
    points.reserve(std::max(num_sample, 1));
    if (num_sample <= 1) {
        points.push_back(p1);
        return points;
    }
    for (int i = 0; i < num_sample; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(num_sample - 1);
        points.push_back(p1 * (1.0f - t) + p2 * t);
    }
    return points;
}

bool BuildCannyEdgeMap(const Mat* original_image, Mat* canny_edges)
{
    if (canny_edges == nullptr || original_image == nullptr || original_image->empty()) {
        return false;
    }

    Mat gray;
    if (original_image->channels() == 1) {
        gray = *original_image;
    } else if (original_image->channels() == 3) {
        cvtColor(*original_image, gray, COLOR_BGR2GRAY);
    } else if (original_image->channels() == 4) {
        cvtColor(*original_image, gray, COLOR_BGRA2GRAY);
    } else {
        return false;
    }

    Canny(gray, *canny_edges, 40, 120);
    return !canny_edges->empty();
}

float RayAngleDeg(const Point2f& start, const Point2f& end)
{
    const Point2f ray = end - start;
    float angle = std::atan2(ray.y, ray.x) * 180.0f / static_cast<float>(CV_PI);
    if (angle < 0.0f) {
        angle += 360.0f;
    }
    return angle;
}

bool SameEdge(const Point2f& first_start,
              const Point2f& first_end,
              const Point2f& second_start,
              const Point2f& second_end)
{
    constexpr float kEpsilon = 0.1f;
    const auto same_point = [&](const Point2f& a, const Point2f& b) {
        return std::fabs(a.x - b.x) <= kEpsilon && std::fabs(a.y - b.y) <= kEpsilon;
    };
    return (same_point(first_start, second_start) && same_point(first_end, second_end)) ||
           (same_point(first_start, second_end) && same_point(first_end, second_start));
}

bool FindBestShiftForEdge(const Point2f& edge_p1,
                          const Point2f& edge_p2,
                          const Mat& canny_binary,
                          float max_shift_px,
                          float step,
                          Point2f* best_p1,
                          Point2f* best_p2,
                          float* best_shift)
{
    const Point2f vec_seg = edge_p2 - edge_p1;
    const float seg_len = PointLength(vec_seg);
    if (seg_len < 1e-3f) {
        if (best_p1 != nullptr) {
            *best_p1 = edge_p1;
        }
        if (best_p2 != nullptr) {
            *best_p2 = edge_p2;
        }
        if (best_shift != nullptr) {
            *best_shift = 0.0f;
        }
        return false;
    }

    const Point2f seg_unit = vec_seg * (1.0f / seg_len);
    const Point2f normal_unit(-seg_unit.y, seg_unit.x);
    int best_score = -1;
    float best_shift_value = 0.0f;
    Point2f candidate_best_p1 = edge_p1;
    Point2f candidate_best_p2 = edge_p2;

    for (float shift = -max_shift_px; shift <= max_shift_px + 1e-3f; shift += step) {
        Point2f shifted_p1;
        Point2f shifted_p2;
        ShiftEdgeSegment(edge_p1, edge_p2, normal_unit, shift, &shifted_p1, &shifted_p2);
        const vector<Point2f> sample_pts = SampleLinePoints(shifted_p1, shifted_p2, 60);
        int score = 0;
        for (const Point2f& point : sample_pts) {
            const int x = std::clamp(cvRound(point.x), 0, canny_binary.cols - 1);
            const int y = std::clamp(cvRound(point.y), 0, canny_binary.rows - 1);
            if (canny_binary.at<uchar>(y, x) > 0) {
                ++score;
            }
        }
        if (score > best_score) {
            best_score = score;
            best_shift_value = shift;
            candidate_best_p1 = shifted_p1;
            candidate_best_p2 = shifted_p2;
        }
    }

    if (best_p1 != nullptr) {
        *best_p1 = candidate_best_p1;
    }
    if (best_p2 != nullptr) {
        *best_p2 = candidate_best_p2;
    }
    if (best_shift != nullptr) {
        *best_shift = best_shift_value;
    }
    return best_score >= 0;
}

bool LineIntersection(const Point2f& p1,
                      const Point2f& v1,
                      const Point2f& p2,
                      const Point2f& v2,
                      Point2f* out)
{
    const float denom = v1.x * v2.y - v1.y * v2.x;
    if (std::fabs(denom) < 1e-6f) {
        return false;
    }
    const float t = ((p2.x - p1.x) * v2.y - (p2.y - p1.y) * v2.x) / denom;
    if (out != nullptr) {
        *out = Point2f(p1.x + t * v1.x, p1.y + t * v1.y);
    }
    return true;
}

vector<Point2f> ReconstructObbCenterFoot(const Point2f& center_A,
                                         const Point2f& foot_C,
                                         const Point2f& dir_ca,
                                         float half_perp)
{
    const Point2f dir_perp(-dir_ca.y, dir_ca.x);
    const float dist_AC = PointLength(foot_C - center_A);
    const float half_ca = dist_AC;

    return {
        center_A - dir_ca * half_ca - dir_perp * half_perp,
        center_A + dir_ca * half_ca - dir_perp * half_perp,
        center_A + dir_ca * half_ca + dir_perp * half_perp,
        center_A - dir_ca * half_ca + dir_perp * half_perp,
    };
}

vector<Point2f> ScaleObbAlongPerpAC(const vector<Point2f>& obb_4pts,
                                    const Point2f& center_A,
                                    const Point2f& vec_AC,
                                    float scale_factor)
{
    const float ac_norm = PointLength(vec_AC);
    if (ac_norm < 1e-6f) {
        return obb_4pts;
    }
    const Point2f ac_unit = vec_AC * (1.0f / ac_norm);
    const Point2f perp_unit(-ac_unit.y, ac_unit.x);

    vector<Point2f> scaled;
    scaled.reserve(obb_4pts.size());
    for (const Point2f& pt : obb_4pts) {
        const Point2f shifted = pt - center_A;
        const Point2f para_comp = ac_unit * shifted.dot(ac_unit);
        const Point2f perp_comp = perp_unit * shifted.dot(perp_unit) * scale_factor;
        scaled.push_back(center_A + para_comp + perp_comp);
    }
    return scaled;
}

double PolygonMaskIou(const vector<Point2f>& poly,
                      const Mat& mask_np,
                      int image_width,
                      int image_height,
                      Rect* overlap_roi,
                      Mat* overlap_mask)
{
    if (poly.size() < 3 || mask_np.empty() || mask_np.type() != CV_8U ||
        mask_np.cols != image_width || mask_np.rows != image_height) {
        return 0.0;
    }

    Mat polygon_mask(image_height, image_width, CV_8U, Scalar(0));
    vector<Point> polygon;
    polygon.reserve(poly.size());
    for (const Point2f& point : poly) {
        polygon.emplace_back(cvRound(point.x), cvRound(point.y));
    }
    fillConvexPoly(polygon_mask, polygon, Scalar(255), LINE_8);

    Mat overlap;
    bitwise_and(polygon_mask, mask_np, overlap);
    const int overlap_pixels = countNonZero(overlap);
    if (overlap_pixels <= 0) {
        return 0.0;
    }

    Mat union_mask;
    bitwise_or(polygon_mask, mask_np, union_mask);
    const int union_pixels = countNonZero(union_mask);
    if (union_pixels <= 0) {
        return 0.0;
    }

    if (overlap_roi != nullptr || overlap_mask != nullptr) {
        vector<Point> nz;
        findNonZero(overlap, nz);
        if (!nz.empty()) {
            const Rect roi = boundingRect(nz);
            if (overlap_roi != nullptr) {
                *overlap_roi = roi;
            }
            if (overlap_mask != nullptr) {
                *overlap_mask = overlap(roi).clone();
            }
        }
    }

    return static_cast<double>(overlap_pixels) / static_cast<double>(union_pixels);
}

const OBBDetection* ResolveBestHeadDetection(const vector<const OBBDetection*>& head_candidates,
                                             const Point2f& segment_center,
                                             const Point2f& grip_center,
                                             float* best_aob_angle_deg,
                                             float* best_aob_diff_deg)
{
    const OBBDetection* best_detection = nullptr;
    float best_angle_diff = numeric_limits<float>::infinity();
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
        if (angle_diff + kTieEpsilon < best_angle_diff) {
            best_angle = angle;
            best_angle_diff = angle_diff;
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

ScriptGeometryResult BuildScriptGeometry(const OBBDetection& grip_detection,
                                         const OBBDetection& head_detection,
                                         const SegRegion& segment,
                                         const Mat* original_image,
                                         int image_width,
                                         int image_height,
                                         float selected_aob_angle_deg,
                                         float selected_aob_diff_deg)
{
    (void)image_width;
    (void)image_height;
    ScriptGeometryResult geometry;
    geometry.segment_center = segment.center;
    geometry.head_center = head_detection.rotated_rect.center;
    geometry.head_class_id = head_detection.class_id;
    geometry.selected_aob_angle_deg = selected_aob_angle_deg;
    geometry.selected_aob_diff_deg = selected_aob_diff_deg;

    if (grip_detection.corners.size() != 4) {
        geometry.invalid_reason = "invalid_obb_corners";
        return geometry;
    }
    if (!IsFinitePoint(segment.center)) {
        geometry.invalid_reason = "invalid_segment_center";
        return geometry;
    }

    const vector<Point2f>& raw_pts4 = grip_detection.corners;
    const Point2f raw_center_A = AveragePoint(raw_pts4);
    geometry.grip_center = raw_center_A;

    const Point2f oa = raw_center_A - segment.center;
    const float oa_length = PointLength(oa);
    if (oa_length < 1e-6f) {
        geometry.invalid_reason = "invalid_oa_geometry";
        return geometry;
    }
    const Point2f oa_unit = oa * (1.0f / oa_length);

    const vector<EdgeFootCandidate> foot_points = GetPerpendicularFootPoints(raw_pts4, raw_center_A);
    float best_dot = -numeric_limits<float>::infinity();
    EdgeFootCandidate best_foot;
    bool found_foot = false;
    for (const EdgeFootCandidate& foot_candidate : foot_points) {
        const Point2f vec_ac = foot_candidate.foot - raw_center_A;
        const float ac_length = PointLength(vec_ac);
        if (ac_length < 1e-6f) {
            continue;
        }
        const float dot = oa_unit.dot(vec_ac * (1.0f / ac_length));
        if (dot > best_dot) {
            best_dot = dot;
            best_foot = foot_candidate;
            found_foot = true;
        }
    }
    if (!found_foot) {
        geometry.invalid_reason = "invalid_ac_geometry";
        return geometry;
    }
    geometry.ac_dot = best_dot;

    const Point2f best_vec_AC = best_foot.foot - raw_center_A;
    vector<pair<Point2f, Point2f>> selected_edges;
    selected_edges.reserve(3);
    selected_edges.push_back({ best_foot.edge_start, best_foot.edge_end });

    for (size_t i = 0; i < raw_pts4.size(); ++i) {
        const Point2f& p1 = raw_pts4[i];
        const Point2f& p2 = raw_pts4[(i + 1) % raw_pts4.size()];
        const Point2f e_vec = p2 - p1;
        const float angle = AngleBetweenVectors(e_vec, best_vec_AC);
        const bool is_parallel = (angle < 0.15f) || (std::fabs(angle - static_cast<float>(CV_PI)) < 0.15f);
        if (is_parallel) {
            selected_edges.push_back({ p1, p2 });
        }
    }

    vector<pair<Point2f, Point2f>> unique_edges;
    for (const auto& edge : selected_edges) {
        bool duplicate = false;
        for (const auto& existing : unique_edges) {
            if (SameEdge(edge.first, edge.second, existing.first, existing.second)) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            unique_edges.push_back(edge);
        }
    }

    vector<pair<Point2f, Point2f>> original_edges;
    original_edges.reserve(raw_pts4.size());
    for (size_t i = 0; i < raw_pts4.size(); ++i) {
        original_edges.push_back({ raw_pts4[i], raw_pts4[(i + 1) % raw_pts4.size()] });
    }

    pair<Point2f, Point2f> remain_edge = original_edges.front();
    bool remain_edge_found = false;
    for (const auto& original_edge : original_edges) {
        bool is_selected = false;
        for (const auto& edge : unique_edges) {
            if (SameEdge(original_edge.first, original_edge.second, edge.first, edge.second)) {
                is_selected = true;
                break;
            }
        }
        if (!is_selected) {
            remain_edge = original_edge;
            remain_edge_found = true;
            break;
        }
    }

    vector<pair<Point2f, Point2f>> tuned_edges = unique_edges;
    if (original_image != nullptr && !original_image->empty()) {
        Mat canny_edges;
        if (BuildCannyEdgeMap(original_image, &canny_edges)) {
            tuned_edges.clear();
            tuned_edges.reserve(unique_edges.size());
            for (const auto& edge : unique_edges) {
                Point2f tuned_p1;
                Point2f tuned_p2;
                float best_shift = 0.0f;
                FindBestShiftForEdge(edge.first,
                                     edge.second,
                                     canny_edges,
                                     12.0f,
                                     0.5f,
                                     &tuned_p1,
                                     &tuned_p2,
                                     &best_shift);
                tuned_edges.push_back({ tuned_p1, tuned_p2 });
            }
        }
    }

    vector<pair<Point2f, Point2f>> all_new_edges = tuned_edges;
    if (remain_edge_found) {
        all_new_edges.push_back(remain_edge);
    }

    const Point2f ac_unit = best_vec_AC * (1.0f / PointLength(best_vec_AC));
    vector<pair<Point2f, Point2f>> para_edges;
    vector<pair<Point2f, Point2f>> perp_edges;
    for (const auto& edge : all_new_edges) {
        const Point2f e_vec = edge.second - edge.first;
        const float e_len = PointLength(e_vec);
        if (e_len < 1e-6f) {
            continue;
        }
        const Point2f e_unit = e_vec * (1.0f / e_len);
        if (std::fabs(e_unit.dot(ac_unit)) > 0.9f) {
            para_edges.push_back(edge);
        } else {
            perp_edges.push_back(edge);
        }
    }

    vector<Point2f> new_rect_4pts = raw_pts4;
    if (para_edges.size() == 2 && perp_edges.size() == 2) {
        Point2f p1;
        Point2f p2;
        Point2f p3;
        Point2f p4;
        const bool ok1 = LineIntersection(perp_edges[0].first,
                                          perp_edges[0].second - perp_edges[0].first,
                                          para_edges[0].first,
                                          para_edges[0].second - para_edges[0].first,
                                          &p1);
        const bool ok2 = LineIntersection(perp_edges[1].first,
                                          perp_edges[1].second - perp_edges[1].first,
                                          para_edges[0].first,
                                          para_edges[0].second - para_edges[0].first,
                                          &p2);
        const bool ok3 = LineIntersection(perp_edges[1].first,
                                          perp_edges[1].second - perp_edges[1].first,
                                          para_edges[1].first,
                                          para_edges[1].second - para_edges[1].first,
                                          &p3);
        const bool ok4 = LineIntersection(perp_edges[0].first,
                                          perp_edges[0].second - perp_edges[0].first,
                                          para_edges[1].first,
                                          para_edges[1].second - para_edges[1].first,
                                          &p4);
        if (ok1 && ok2 && ok3 && ok4) {
            new_rect_4pts = { p1, p2, p3, p4 };
            const Point2f rect_center = AveragePoint(new_rect_4pts);
            sort(new_rect_4pts.begin(),
                 new_rect_4pts.end(),
                 [&](const Point2f& left, const Point2f& right) {
                     return std::atan2(left.y - rect_center.y, left.x - rect_center.x) <
                            std::atan2(right.y - rect_center.y, right.x - rect_center.x);
                 });
        }
    }

    const Point2f temp_new_A = AveragePoint(new_rect_4pts);
    const vector<EdgeFootCandidate> foot_points_new = GetPerpendicularFootPoints(new_rect_4pts, temp_new_A);
    const Point2f vec_OA_new = temp_new_A - segment.center;
    const float vec_OA_new_length = PointLength(vec_OA_new);
    if (vec_OA_new_length < 1e-6f) {
        geometry.invalid_reason = "invalid_new_oa_geometry";
        return geometry;
    }
    const Point2f vec_OA_new_unit = vec_OA_new * (1.0f / vec_OA_new_length);

    float best_new_dot = -numeric_limits<float>::infinity();
    Point2f final_C = best_foot.foot;
    Point2f final_vec_AC = best_vec_AC;
    bool found_new_candidate = false;
    for (const EdgeFootCandidate& foot_candidate : foot_points_new) {
        const Point2f vec_ac_new = foot_candidate.foot - temp_new_A;
        const float vec_ac_length = PointLength(vec_ac_new);
        if (vec_ac_length < 1e-6f) {
            continue;
        }
        const float dot = vec_OA_new_unit.dot(vec_ac_new * (1.0f / vec_ac_length));
        if (dot > best_new_dot) {
            best_new_dot = dot;
            final_C = foot_candidate.foot;
            final_vec_AC = vec_ac_new;
            found_new_candidate = true;
        }
    }
    if (!found_new_candidate) {
        final_C = best_foot.foot;
        final_vec_AC = best_vec_AC;
    }

    const float final_vec_AC_length = PointLength(final_vec_AC);
    if (final_vec_AC_length < 1e-6f) {
        geometry.invalid_reason = "invalid_final_ac_geometry";
        return geometry;
    }

    const Point2f dir_ca = -final_vec_AC * (1.0f / final_vec_AC_length);
    const Point2f final_A = final_C + dir_ca * 45.0f;
    if (!IsFinitePoint(final_A) || !IsFinitePoint(final_C)) {
        geometry.invalid_reason = "non_finite_final_geometry";
        return geometry;
    }

    const Point2f perp_unit(-dir_ca.y, dir_ca.x);
    float half_perp = 0.0f;
    for (const Point2f& pt : new_rect_4pts) {
        const Point2f shifted = pt - temp_new_A;
        half_perp = std::max(half_perp, std::fabs(shifted.dot(perp_unit)));
    }

    geometry.valid = true;
    geometry.invalid_reason.clear();
    geometry.arrow_start = final_A;
    geometry.arrow_end = final_C;
    geometry.base_corners = ReconstructObbCenterFoot(final_A, final_C, dir_ca, half_perp);
    geometry.scaled_corners = ScaleObbAlongPerpAC(geometry.base_corners, final_A, final_vec_AC, 2.0f);
    geometry.final_ac_deg = RayAngleDeg(final_A, final_C);
    geometry.grip_long_angle_deg = NormalizeAngleDeg(geometry.final_ac_deg + 90.0f);
    geometry.head_side = ResolveHeadSideFromGripLocalAc(final_A, final_C, head_detection.rotated_rect.center);
    geometry.head_type_code = ResolveHeadTypeCode(head_detection.class_id, geometry.head_side);
    geometry.head_class_id = head_detection.class_id;
    geometry.collision_with_other_seg = false;
    geometry.max_seg_iou = 0.0f;

    return geometry;
}

PoseDetection BuildPoseDetection(const OBBDetection& detection,
                                 const vector<string>& obb_class_names,
                                 int image_width,
                                 int image_height,
                                 int matched_segment_index,
                                 const string& segment_class_name,
                                 const SegRegion& segment,
                                 const ScriptGeometryResult& geometry,
                                 const Point2f& head_point)
{
    PoseDetection pose;
    pose.class_id = detection.class_id;
    pose.class_name = GetClassName(obb_class_names, detection.class_id);
    pose.segment_class_name = segment_class_name;
    pose.matched_segment_index = matched_segment_index;
    pose.confidence = detection.conf;
    pose.center_x = QuantizeStable(geometry.arrow_start.x, kPoseCoordinateStepPx);
    pose.center_y = QuantizeStable(geometry.arrow_start.y, kPoseCoordinateStepPx);
    pose.angle_deg = QuantizeStable(geometry.final_ac_deg, kPoseAngleStepDeg);
    pose.pick_status_code = geometry.collision_with_other_seg ? kPickStatusCannotGrab : kPickStatusCanGrab;
    pose.head_type_code = geometry.head_type_code;
    pose.head_class_id = geometry.head_class_id;
    pose.head_type_text = ResolveHeadTypeText(pose.head_type_code);
    pose.obb_center = geometry.arrow_start;
    pose.seg_center = segment.center;
    pose.x_point = geometry.arrow_start;
    pose.small_point = head_point;
    pose.has_small_point = true;
    pose.small_arrow_start = segment.center;
    pose.small_arrow_end = head_point;
    pose.has_small_ray = true;
    pose.small_ray_quadrant = geometry.head_side;
    pose.arrow_start = geometry.arrow_start;
    pose.arrow_end = geometry.arrow_end;
    pose.grip_long_angle_deg = QuantizeStable(geometry.grip_long_angle_deg, kPoseAngleStepDeg);
    pose.corners = geometry.base_corners;
    pose.bbox = BuildPoseBoundingRect(pose.corners, image_width, image_height);
    pose.mask_collisions = geometry.mask_collisions;
    pose.can_grab = !geometry.collision_with_other_seg;
    return pose;
}

vector<PoseDetection> BuildFilteredPoseDetections(const vector<OBBDetection>& obb_output,
                                                  const vector<string>& obb_class_names,
                                                  const vector<SegRegion>& segments,
                                                  int image_width,
                                                  int image_height,
                                                  const FramePostprocessConfig& config,
                                                  bool debug_enabled,
                                                  const Mat* original_image)
{
    (void)config;
    vector<PoseDetection> detections;
    detections.reserve(segments.size());

    vector<bool> used_detections(obb_output.size(), false);

    for (int segment_index = 0; segment_index < static_cast<int>(segments.size()); ++segment_index) {
        const SegRegion& segment = segments[segment_index];
        if (!SegmentMaskValid(segment, image_width, image_height)) {
            continue;
        }

        vector<int> left_indices;
        vector<int> head_indices;
        for (int detection_index = 0; detection_index < static_cast<int>(obb_output.size()); ++detection_index) {
            if (used_detections[detection_index]) {
                continue;
            }
            const auto& detection = obb_output[detection_index];
            if (!MaskContainsPoint(segment.mask, detection.rotated_rect.center)) {
                continue;
            }
            if (detection.class_id == kClassLeft) {
                left_indices.push_back(detection_index);
                used_detections[detection_index] = true;
            } else if (detection.class_id == kClassBig || detection.class_id == kClassSmall) {
                head_indices.push_back(detection_index);
                used_detections[detection_index] = true;
            }
        }

        if (left_indices.empty() || head_indices.empty()) {
            continue;
        }

        const int left_index = left_indices.front();
        const OBBDetection& grip_detection = obb_output[left_index];
        vector<const OBBDetection*> head_candidates;
        head_candidates.reserve(head_indices.size());
        for (int head_index : head_indices) {
            head_candidates.push_back(&obb_output[head_index]);
        }

        float selected_aob_angle_deg = numeric_limits<float>::quiet_NaN();
        float selected_aob_diff_deg = numeric_limits<float>::quiet_NaN();
        const OBBDetection* head_detection =
            ResolveBestHeadDetection(head_candidates,
                                     segment.center,
                                     grip_detection.rotated_rect.center,
                                     &selected_aob_angle_deg,
                                     &selected_aob_diff_deg);
        if (head_detection == nullptr) {
            continue;
        }

        const ScriptGeometryResult geometry = BuildScriptGeometry(grip_detection,
                                                                  *head_detection,
                                                                  segment,
                                                                  original_image,
                                                                  image_width,
                                                                  image_height,
                                                                  selected_aob_angle_deg,
                                                                  selected_aob_diff_deg);
        if (!geometry.valid) {
            if (debug_enabled) {
                cout << "[PostprocessDebug] target"
                     << " segment_index=" << segment_index
                     << " left_index=" << left_index
                     << " head_index=" << (head_detection != nullptr ? static_cast<int>(head_detection - &obb_output[0]) : -1)
                     << " reason=" << geometry.invalid_reason
                     << endl;
            }
            continue;
        }

        ScriptGeometryResult final_geometry = geometry;
        for (int other_index = 0; other_index < static_cast<int>(segments.size()); ++other_index) {
            if (other_index == segment_index) {
                continue;
            }
            const SegRegion& other_segment = segments[other_index];
            if (!SegmentMaskValid(other_segment, image_width, image_height)) {
                continue;
            }
            Rect overlap_roi;
            Mat overlap_mask;
            const double iou = PolygonMaskIou(final_geometry.scaled_corners,
                                              other_segment.mask,
                                              image_width,
                                              image_height,
                                              &overlap_roi,
                                              &overlap_mask);
            final_geometry.max_seg_iou = std::max(final_geometry.max_seg_iou, static_cast<float>(iou));
            if (iou > 0.10) {
                final_geometry.collision_with_other_seg = true;
                final_geometry.mask_collisions.push_back({ overlap_roi, overlap_mask });
            }
        }

        if (debug_enabled) {
            cout << "[PostprocessDebug] target"
                 << " segment_index=" << segment_index
                 << " left_index=" << left_index
                 << " head_index=" << (head_detection != nullptr ? static_cast<int>(head_detection - &obb_output[0]) : -1)
                 << " head_class_id=" << head_detection->class_id
                 << " selected_aob_angle_deg=" << selected_aob_angle_deg
                 << " selected_aob_diff_deg=" << selected_aob_diff_deg
                 << " O=" << PointText(final_geometry.segment_center)
                 << " A=" << PointText(final_geometry.arrow_start)
                 << " B=" << PointText(final_geometry.head_center)
                 << " C=" << PointText(final_geometry.arrow_end)
                 << " ac_angle_deg=" << final_geometry.final_ac_deg
                 << " gripper_long_angle_deg=" << final_geometry.grip_long_angle_deg
                 << " max_seg_iou=" << final_geometry.max_seg_iou
                 << " collision_with_other_seg=" << BoolText(final_geometry.collision_with_other_seg)
                 << " can_grab=" << BoolText(!final_geometry.collision_with_other_seg)
                 << endl;
        }

        detections.push_back(BuildPoseDetection(grip_detection,
                                                obb_class_names,
                                                image_width,
                                                image_height,
                                                segment_index,
                                                segment.class_name,
                                                segment,
                                                final_geometry,
                                                head_detection->rotated_rect.center));
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
                                               const FramePostprocessConfig& config,
                                               const Mat* original_image)
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
                                                    debug_enabled,
                                                    original_image);

    ResolvePrimaryDetection(result);
    if (debug_enabled) {
        LogPostprocessDebugSummary(obb_output, result);
    }
    return result;
}
