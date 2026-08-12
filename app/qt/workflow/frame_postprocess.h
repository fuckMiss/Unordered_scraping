#pragma once

#include "frame_result.h"

#include <string>
#include <vector>

struct OBBDetection;
struct SegDetection;

struct FramePostprocessConfig
{
    double center_ray_offset_px = 10.0;
    bool debug_logging_enabled = false;
};

FrameInferenceResult BuildFrameInferenceResult(const std::vector<OBBDetection>& obb_output,
                                               const std::vector<std::string>& obb_class_names,
                                               const std::vector<SegDetection>& seg_output,
                                               const std::vector<std::string>& seg_class_names,
                                               int image_width,
                                               int image_height,
                                               double obb_inference_ms,
                                               double seg_inference_ms,
                                               const FramePostprocessConfig& config = {});
