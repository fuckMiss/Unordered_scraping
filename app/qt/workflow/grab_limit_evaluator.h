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

void ResolveResultAfterFiltering(FrameInferenceResult& result);
GrabLimitOverlayPolygon BuildGrabLimitOverlayPolygon(const GrabLimitConfig& limits,
                                                     const CoordinateTransformState& coordinate_state,
                                                     AxisMappingMode axis_mapping_mode);
bool ApplyMechanicalRoiFilter(FrameInferenceResult& result,
                              const GrabLimitConfig& limits,
                              const CoordinateTransformState& coordinate_state,
                              AxisMappingMode axis_mapping_mode,
                              std::string* error_message);
GrabLimitDecision EvaluateGrabLimits(const FrameInferenceResult& result,
                                     const GrabLimitConfig& limits,
                                     const PlcOutputConfig& plc_config);
