#pragma once

#include "frame_result.h"

#include <opencv2/opencv.hpp>

#include <string>
#include <vector>

struct GrabLimitOverlayView
{
    bool visible = false;
    std::string message;
    std::vector<cv::Point2f> polygon;
};

void DrawFrameOverlay(cv::Mat& image,
                      const FrameInferenceResult& result,
                      int selected_index = -1,
                      bool draw_center_reticle = true,
                      bool show_all_detections = false,
                      bool show_plc_center_debug = false,
                      bool show_head_ray_debug = true,
                      const GrabLimitOverlayView* grab_limit_overlay = nullptr);

std::vector<int> SelectNormalDisplayDetectionIndices(const FrameInferenceResult& result);
