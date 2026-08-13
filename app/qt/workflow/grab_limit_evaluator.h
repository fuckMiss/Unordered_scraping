#pragma once

#include "coordinate_transform.h"
#include "plc_result_contract.h"

#include <string>
#include <vector>

struct LimitRange
{
    double lower = 0.0;
    double upper = 0.0;
};

struct GrabLimitConfig
{
    bool enabled = true;
    double roi_margin = 5.0;
    LimitRange x{ 0.0, 0.0 };
    LimitRange y{ 0.0, 0.0 };
    LimitRange angle{ 0.0, 0.0 };
};

struct GrabLimitDecision
{
    bool rejected = false;
    bool failed = false;
    std::string reason;
};

struct GrabLimitOverlayPolygon
{
    bool visible = false;
    std::string reason;
    std::vector<cv::Point2f> image_points;
};

struct MechanicalGripperCollisionConfig
{
    double length = 0.0;
    double width = 0.0;
    double center_ray_offset_px = 10.0;
    bool debug_logging_enabled = false;
};

void ResolveResultAfterFiltering(FrameInferenceResult& result);
GrabLimitOverlayPolygon BuildGrabLimitOverlayPolygon(const GrabLimitConfig& limits,
                                                     const CoordinateTransformState& coordinate_state,
                                                     AxisMappingMode axis_mapping_mode);
std::vector<cv::Point2f> BuildMechanicalGripperCorners(const PoseDetection& detection,
                                                       const CoordinateTransformState& coordinate_state,
                                                       const MechanicalGripperCollisionConfig& config,
                                                       int detection_index);
void ApplyMechanicalGripperCollisionFilter(FrameInferenceResult& result,
                                           const CoordinateTransformState& coordinate_state,
                                           const MechanicalGripperCollisionConfig& config,
                                           const GrabLimitConfig& limits,
                                           AxisMappingMode axis_mapping_mode);
bool ApplyMechanicalRoiFilter(FrameInferenceResult& result,
                              const GrabLimitConfig& limits,
                              const CoordinateTransformState& coordinate_state,
                              AxisMappingMode axis_mapping_mode,
                              std::string* error_message);
GrabLimitDecision EvaluateGrabLimits(const FrameInferenceResult& result,
                                     const GrabLimitConfig& limits,
                                     const PlcOutputConfig& plc_config);
