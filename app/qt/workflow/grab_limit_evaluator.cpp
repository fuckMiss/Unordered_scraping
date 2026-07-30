#include "grab_limit_evaluator.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <sstream>
#include <utility>

namespace {

std::string HeadTypeText(int head_type_code)
{
    switch (head_type_code) {
    case 1: return "大上右";
    case 2: return "小上左";
    case 3: return "大上左";
    case 4: return "小上右";
    default: return "未知";
    }
}

bool IsValueWithinRange(float value, const LimitRange& range)
{
    return value >= range.lower && value <= range.upper;
}

std::string FormatLimitReason(const char* label, float value, const LimitRange& range)
{
    std::ostringstream message;
    message.setf(std::ios::fixed);
    message.precision(1);
    message << label << "超限：" << value << " 不在 ["
            << range.lower << ", " << range.upper << "]";
    return message.str();
}

const char* FrontBackAxisLabel(AxisMappingMode mode)
{
    return mode == AxisMappingMode::FrontBackMachineX ? "前后 / 机械X" : "前后 / 机械Y";
}

const char* LeftRightAxisLabel(AxisMappingMode mode)
{
    return mode == AxisMappingMode::FrontBackMachineX ? "左右 / 机械Y" : "左右 / 机械X";
}

} // namespace

void ResolveResultAfterFiltering(FrameInferenceResult& result)
{
    result.primary_index = -1;
    result.pick_status_code = 3;
    result.head_type_code = 0;
    result.head_type_text = HeadTypeText(0);

    float best_score = -std::numeric_limits<float>::infinity();
    int grabbable_count = 0;
    for (int i = 0; i < static_cast<int>(result.detections.size()); ++i) {
        PoseDetection& detection = result.detections[i];
        if (!detection.can_grab) {
            detection.pick_status_code = 3;
            continue;
        }

        ++grabbable_count;
        const float score = detection.confidence;
        if (score > best_score) {
            best_score = score;
            result.primary_index = i;
        }
    }

    if (grabbable_count == 0) {
        return;
    }

    result.pick_status_code = grabbable_count == 1 ? 1 : 2;
    for (PoseDetection& detection : result.detections) {
        detection.pick_status_code = detection.can_grab ? result.pick_status_code : 3;
    }

    if (result.primary_index >= 0 && result.primary_index < static_cast<int>(result.detections.size())) {
        result.head_type_code = result.detections[result.primary_index].head_type_code;
        result.head_type_text = result.detections[result.primary_index].head_type_text;
    }
}

bool ApplyMechanicalRoiFilter(FrameInferenceResult& result,
                              const GrabLimitConfig& limits,
                              const CoordinateTransformState& coordinate_state,
                              AxisMappingMode axis_mapping_mode,
                              std::string* error_message)
{
    if (!limits.enabled) {
        return true;
    }

    if (!coordinate_state.enabled || !coordinate_state.valid) {
        if (error_message != nullptr) {
            *error_message = "机械 ROI 已启用，请先启用有效的九点坐标转换。";
        }
        result.detections.clear();
        ResolveResultAfterFiltering(result);
        return false;
    }

    const double front_back_lower = limits.x.lower + limits.roi_margin;
    const double front_back_upper = limits.x.upper - limits.roi_margin;
    const double left_right_lower = limits.y.lower + limits.roi_margin;
    const double left_right_upper = limits.y.upper - limits.roi_margin;
    if (front_back_lower > front_back_upper || left_right_lower > left_right_upper) {
        if (error_message != nullptr) {
            *error_message = "ROI 留边过大，内缩后的机械 ROI 为空。";
        }
        result.detections.clear();
        ResolveResultAfterFiltering(result);
        return false;
    }

    const int total_count = static_cast<int>(result.detections.size());
    std::vector<PoseDetection> filtered;
    filtered.reserve(result.detections.size());
    for (const PoseDetection& detection : result.detections) {
        if (!detection.has_machine_coords) {
            continue;
        }

        const float front_back = axis_mapping_mode == AxisMappingMode::FrontBackMachineX
                                     ? detection.machine_x
                                     : detection.machine_y;
        const float left_right = axis_mapping_mode == AxisMappingMode::FrontBackMachineX
                                     ? detection.machine_y
                                     : detection.machine_x;
        if (front_back >= front_back_lower &&
            front_back <= front_back_upper &&
            left_right >= left_right_lower &&
            left_right <= left_right_upper) {
            filtered.push_back(detection);
        }
    }

    result.detections = std::move(filtered);
    ResolveResultAfterFiltering(result);
    std::cout << "[ROI] total=" << total_count
              << " inside=" << result.detections.size()
              << " margin=" << limits.roi_margin
              << " front_back=[" << front_back_lower << "," << front_back_upper << "]"
              << " left_right=[" << left_right_lower << "," << left_right_upper << "]"
              << " mapping=" << (axis_mapping_mode == AxisMappingMode::FrontBackMachineX
                                      ? "front_back=machine_x,left_right=machine_y"
                                      : "front_back=machine_y,left_right=machine_x")
              << std::endl;
    return true;
}

GrabLimitDecision EvaluateGrabLimits(const FrameInferenceResult& result,
                                     const GrabLimitConfig& limits,
                                     const PlcOutputConfig& plc_config)
{
    GrabLimitDecision decision;
    if (!limits.enabled) {
        return decision;
    }
    if (result.primary_index < 0 || result.primary_index >= static_cast<int>(result.detections.size())) {
        decision.rejected = true;
        decision.reason = "无有效主目标";
        return decision;
    }

    const PoseDetection& target = result.detections[result.primary_index];
    const float active_front_back = FrontBackValue(target, plc_config.axis_mapping_mode);
    const float active_left_right = LeftRightValue(target, plc_config.axis_mapping_mode);
    if (!IsValueWithinRange(active_front_back, limits.x)) {
        decision.rejected = true;
        decision.reason = FormatLimitReason(FrontBackAxisLabel(plc_config.axis_mapping_mode),
                                            active_front_back,
                                            limits.x);
        return decision;
    }
    if (!IsValueWithinRange(active_left_right, limits.y)) {
        decision.rejected = true;
        decision.reason = FormatLimitReason(LeftRightAxisLabel(plc_config.axis_mapping_mode),
                                            active_left_right,
                                            limits.y);
        return decision;
    }

    const float active_angle = ApplyPlcAngleCalibration(target.angle_deg, plc_config);
    if (!IsValueWithinRange(active_angle, limits.angle)) {
        decision.rejected = true;
        decision.reason = FormatLimitReason("旋转角度", active_angle, limits.angle);
        return decision;
    }
    return decision;
}
