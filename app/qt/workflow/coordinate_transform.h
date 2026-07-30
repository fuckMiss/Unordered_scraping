#pragma once

#include "frame_result.h"

#include <opencv2/opencv.hpp>

#include <QString>

#include <string>
#include <vector>

struct CoordinateCalibrationPoint
{
    double image_x = 0.0;
    double image_y = 0.0;
    double machine_x = 0.0;
    double machine_y = 0.0;
};

struct CoordinateTransformConfig
{
    bool enabled = false;
    QString profile_name;
    std::vector<CoordinateCalibrationPoint> points;
};

struct CoordinateCalibrationProfile
{
    QString name;
    CoordinateTransformConfig config;
};

struct CoordinateTransformState
{
    bool enabled = false;
    bool valid = false;
    double mean_reprojection_error = 0.0;
    std::string error_message;
    std::vector<CoordinateCalibrationPoint> points;
    cv::Mat homography;
};

CoordinateTransformState BuildCoordinateTransformState(const CoordinateTransformConfig& config);
bool TransformImagePointToMachine(const CoordinateTransformState& state,
                                  const cv::Point2f& image_point,
                                  cv::Point2f* machine_point);
void ApplyCoordinateTransform(FrameInferenceResult& result, const CoordinateTransformState& state);
