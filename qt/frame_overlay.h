#pragma once

#include "frame_result.h"

#include <opencv2/opencv.hpp>

void DrawFrameOverlay(cv::Mat& image,
                      const FrameInferenceResult& result,
                      int selected_index = -1,
                      bool draw_center_reticle = true,
                      bool show_all_detections = false);
