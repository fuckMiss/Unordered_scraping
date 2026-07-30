#include "plc_result_contract.h"

#include <cmath>

namespace {

float NormalizeAngle(float angle_deg)
{
    float value = std::fmod(angle_deg, 360.0f);
    if (value < 0.0f) {
        value += 360.0f;
    }
    return value;
}

float ApplyAngleRange(float angle_deg, AngleRangeMode range_mode)
{
    float value = NormalizeAngle(angle_deg);
    if (range_mode == AngleRangeMode::Signed180 && value >= 180.0f) {
        value -= 360.0f;
    }
    return value;
}

} // namespace

float ApplyPlcAngleCalibration(float angle_deg, const PlcOutputConfig& config)
{
    const float direction = config.angle_reverse_direction ? -1.0f : 1.0f;
    return ApplyAngleRange(angle_deg * direction + config.angle_offset_deg, config.angle_range_mode);
}

float FrontBackValue(const PoseDetection& detection, AxisMappingMode axis_mapping_mode)
{
    if (axis_mapping_mode == AxisMappingMode::FrontBackMachineX) {
        return detection.has_machine_coords ? detection.machine_x : detection.center_x;
    }
    return detection.has_machine_coords ? detection.machine_y : detection.center_y;
}

float LeftRightValue(const PoseDetection& detection, AxisMappingMode axis_mapping_mode)
{
    if (axis_mapping_mode == AxisMappingMode::FrontBackMachineX) {
        return detection.has_machine_coords ? detection.machine_y : detection.center_y;
    }
    return detection.has_machine_coords ? detection.machine_x : detection.center_x;
}

PlcWriteResult BuildPlcWriteResult(const FrameInferenceResult& result, const PlcOutputConfig& config)
{
    PlcWriteResult write_result;
    write_result.pick_status = static_cast<float>(result.pick_status_code);
    write_result.head_type = static_cast<float>(result.head_type_code);

    if (result.primary_index >= 0 && result.primary_index < static_cast<int>(result.detections.size())) {
        const PoseDetection& target = result.detections[result.primary_index];
        write_result.image_x = target.center_x;
        write_result.image_y = target.center_y;
        write_result.has_machine_coords = target.has_machine_coords;
        write_result.x = FrontBackValue(target, config.axis_mapping_mode) + config.front_back_offset;
        write_result.y = LeftRightValue(target, config.axis_mapping_mode) + config.left_right_offset;
        write_result.angle = ApplyPlcAngleCalibration(target.angle_deg, config);
        write_result.pick_status = static_cast<float>(target.pick_status_code);
        write_result.head_type = static_cast<float>(target.head_type_code);
    }

    return write_result;
}

PlcWriteResult BuildDiagnosticPlcWriteResult(const PlcOutputConfig& config)
{
    FrameInferenceResult result;
    PoseDetection target;
    target.center_x = 123.4f;
    target.center_y = 56.7f;
    target.machine_x = 123.4f;
    target.machine_y = 56.7f;
    target.has_machine_coords = true;
    target.angle_deg = 90.0f;
    target.pick_status_code = 1;
    target.head_type_code = 4;
    result.detections.push_back(target);
    result.primary_index = 0;
    result.pick_status_code = 1;
    result.head_type_code = 4;
    return BuildPlcWriteResult(result, config);
}
