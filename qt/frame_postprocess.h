#pragma once

#include "YOLOv11_OBB.h"
#include "YOLOv11_SEG.h"
#include "frame_result.h"

#include <string>
#include <vector>

FrameInferenceResult BuildFrameInferenceResult(const std::vector<OBBDetection>& obb_output,
                                               const std::vector<std::string>& obb_class_names,
                                               const std::vector<SegDetection>& seg_output,
                                               const std::vector<std::string>& seg_class_names,
                                               int image_width,
                                               int image_height,
                                               double obb_inference_ms,
                                               double seg_inference_ms);
