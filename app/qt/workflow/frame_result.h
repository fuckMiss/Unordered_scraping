#pragma once

#include <opencv2/opencv.hpp>

#include <string>
#include <utility>
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
    float machine_x = 0.0f;
    float machine_y = 0.0f;
    bool has_machine_coords = false;
    float ac_unit_machine_x = 0.0f;
    float ac_unit_machine_y = 0.0f;
    bool has_machine_ac_unit = false;
    float angle_deg = 0.0f;
    float display_angle_deg = 0.0f;
    bool has_display_angle = false;
    float display_geometry_angle_offset_deg = 0.0f;
    bool has_display_geometry_angle_offset = false;
    float plc_command_x = 0.0f;
    float plc_command_y = 0.0f;
    float plc_command_angle_deg = 0.0f;
    cv::Point2f plc_command_center;
    cv::Point2f plc_command_arrow_start;
    cv::Point2f plc_command_arrow_end;
    bool has_plc_command_pose = false;
    int pick_status_code = 3;
    int head_type_code = 0;
    int head_class_id = -1;
    std::string head_type_text;
    cv::Point2f obb_center;
    cv::Point2f seg_center;
    cv::Point2f x_point;
    cv::Point2f small_point;
    bool has_small_point = false;
    cv::Point2f small_arrow_start;
    cv::Point2f small_arrow_end;
    bool has_small_ray = false;
    int small_ray_quadrant = 0;
    cv::Point2f arrow_start;
    cv::Point2f arrow_end;
    float grip_long_angle_deg = 0.0f;
    cv::Rect bbox;
    std::vector<cv::Point2f> corners;
    std::vector<cv::Point2f> mechanical_gripper_corners;
    std::vector<std::pair<cv::Rect, cv::Mat>> mask_collisions;
    bool can_grab = false;
};

struct ObbRegion
{
    int class_id = -1;
    std::string class_name;
    float confidence = 0.0f;
    cv::Point2f center;
    cv::Rect bbox;
    std::vector<cv::Point2f> corners;
};

struct FrameInferenceResult
{
    int image_width = 0;
    int image_height = 0;
    double obb_inference_ms = 0.0;
    double seg_inference_ms = 0.0;
    double total_inference_ms = 0.0;
    double display_scale = 1.0;
    std::vector<PoseDetection> detections;
    std::vector<SegRegion> segments;
    std::vector<ObbRegion> raw_obb_regions;
    int primary_index = -1;
    int pick_status_code = 3;
    int head_type_code = 0;
    std::string head_type_text;
};
