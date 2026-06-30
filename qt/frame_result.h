#pragma once

#include <opencv2/opencv.hpp>

#include <string>
#include <vector>

struct SegRegion
{
    int class_id = -1;
    std::string class_name;
    float confidence = 0.0f;
    cv::Rect bbox;
    cv::Mat mask;
    cv::Point2f center;
    std::vector<cv::Point2f> contour;
    std::vector<cv::Point2f> min_rect_corners;
};

struct PoseDetection
{
    int class_id = -1;
    std::string class_name;
    std::string segment_class_name;
    int matched_segment_index = -1;
    float confidence = 0.0f;
    float center_x = 0.0f;
    float center_y = 0.0f;
    float angle_deg = 0.0f;
    int pick_status_code = 3;
    int head_type_code = 0;
    std::string head_type_text;
    cv::Point2f seg_center;
    cv::Point2f x_point;
    cv::Point2f small_point;
    bool has_small_point = false;
    cv::Point2f arrow_start;
    cv::Point2f arrow_end;
    cv::Rect bbox;
    std::vector<cv::Point2f> corners;
    bool can_grab = false;
};

struct FrameInferenceResult
{
    int image_width = 0;
    int image_height = 0;
    double obb_inference_ms = 0.0;
    double seg_inference_ms = 0.0;
    double total_inference_ms = 0.0;
    std::vector<PoseDetection> detections;
    std::vector<SegRegion> segments;
    int primary_index = -1;
    int pick_status_code = 3;
    int head_type_code = 0;
    std::string head_type_text;
};
