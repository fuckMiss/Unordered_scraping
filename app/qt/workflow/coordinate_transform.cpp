#include "coordinate_transform.h"

#include <cmath>
#include <sstream>

using namespace cv;
using namespace std;

namespace {

bool HasFiniteCalibrationPoint(const CoordinateCalibrationPoint& point)
{
    return isfinite(point.image_x) &&
           isfinite(point.image_y) &&
           isfinite(point.machine_x) &&
           isfinite(point.machine_y);
}

} // namespace

CoordinateTransformState BuildCoordinateTransformState(const CoordinateTransformConfig& config)
{
    CoordinateTransformState state;
    state.enabled = config.enabled;
    state.mean_reprojection_error = 0.0;
    state.points = config.points;

    if (!state.enabled) {
        state.valid = false;
        state.error_message = "coordinate transform is disabled";
        return state;
    }

    vector<Point2f> image_points;
    vector<Point2f> machine_points;
    image_points.reserve(config.points.size());
    machine_points.reserve(config.points.size());

    for (const CoordinateCalibrationPoint& point : config.points) {
        if (!HasFiniteCalibrationPoint(point)) {
            state.error_message = "calibration point contains non-finite values";
            return state;
        }
        image_points.emplace_back(static_cast<float>(point.image_x),
                                  static_cast<float>(point.image_y));
        machine_points.emplace_back(static_cast<float>(point.machine_x),
                                    static_cast<float>(point.machine_y));
    }

    if (image_points.size() < 4 || machine_points.size() < 4) {
        state.error_message = "at least 4 calibration point pairs are required";
        return state;
    }

    state.homography = findHomography(image_points, machine_points, 0);
    if (state.homography.empty() ||
        state.homography.rows != 3 ||
        state.homography.cols != 3) {
        state.error_message = "homography calculation failed; check duplicated or collinear points";
        return state;
    }

    state.homography.convertTo(state.homography, CV_64F);
    const double det = determinant(state.homography);
    if (!isfinite(det) || fabs(det) < 1e-12) {
        state.homography.release();
        state.error_message = "homography matrix is singular";
        return state;
    }

    vector<Point2f> projected_points;
    perspectiveTransform(image_points, projected_points, state.homography);
    double error_sum = 0.0;
    for (size_t i = 0; i < projected_points.size(); ++i) {
        const double dx = static_cast<double>(projected_points[i].x) - machine_points[i].x;
        const double dy = static_cast<double>(projected_points[i].y) - machine_points[i].y;
        error_sum += sqrt(dx * dx + dy * dy);
    }
    if (!projected_points.empty()) {
        state.mean_reprojection_error = error_sum / static_cast<double>(projected_points.size());
    }

    state.valid = true;
    state.error_message.clear();
    return state;
}

bool TransformImagePointToMachine(const CoordinateTransformState& state,
                                  const Point2f& image_point,
                                  Point2f* machine_point)
{
    if (machine_point == nullptr || !state.enabled || !state.valid || state.homography.empty()) {
        return false;
    }

    vector<Point2f> src = { image_point };
    vector<Point2f> dst;
    perspectiveTransform(src, dst, state.homography);
    if (dst.empty() || !isfinite(dst[0].x) || !isfinite(dst[0].y)) {
        return false;
    }

    *machine_point = dst[0];
    return true;
}

void ApplyCoordinateTransform(FrameInferenceResult& result, const CoordinateTransformState& state)
{
    for (PoseDetection& detection : result.detections) {
        detection.has_machine_coords = false;
        detection.machine_x = 0.0f;
        detection.machine_y = 0.0f;

        Point2f machine_point;
        if (TransformImagePointToMachine(state,
                                         Point2f(detection.center_x, detection.center_y),
                                         &machine_point)) {
            detection.machine_x = machine_point.x;
            detection.machine_y = machine_point.y;
            detection.has_machine_coords = true;
        }
    }
}
