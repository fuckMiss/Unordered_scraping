#include "plc_result_contract.h"

#include <cmath>
#include <iostream>

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
    return ApplyPlcAngleCalibration(angle_deg, config, HeadTypeCompensation{});
}

float ApplyPlcAngleCalibration(float angle_deg,
                               const PlcOutputConfig& config,
                               const HeadTypeCompensation& type_compensation)
{
    const float direction = config.angle_reverse_direction ? -1.0f : 1.0f;
    return ApplyAngleRange(angle_deg * direction +
                               config.angle_offset_deg +
                               type_compensation.angle_offset_deg,
                           config.angle_range_mode);
}

const HeadTypeCompensation& CompensationForHeadType(const PlcOutputConfig& config, int head_type_code)
{
    static const HeadTypeCompensation empty;
    if (head_type_code < 0 ||
        head_type_code >= static_cast<int>(config.head_type_compensations.size())) {
        return empty;
    }
    return config.head_type_compensations[head_type_code];
}

void LogPlcCompensation(const PoseDetection& target,
                        const HeadTypeCompensation& type_compensation,
                        const PlcOutputConfig& config,
                        const PlcWriteResult& write_result)
{
    if (!config.debug_logging_enabled) {
        return;
    }

    std::cout << "[PLC_DEBUG] compensation"
              << " D508=" << target.head_type_code
              << " raw_machine=(" << target.machine_x << "," << target.machine_y << ")"
              << " raw_image=(" << target.center_x << "," << target.center_y << ")"
              << " raw_angle=" << target.angle_deg
              << " ac_unit_machine=(" << target.ac_unit_machine_x << "," << target.ac_unit_machine_y << ")"
              << " ac_valid=" << (target.has_machine_ac_unit ? "true" : "false")
              << " global_angle_offset=" << config.angle_offset_deg
              << " type_angle_offset=" << type_compensation.angle_offset_deg
              << " type_ac_offset_mm=" << type_compensation.ac_ray_offset_mm
              << " axis_offset=(" << config.front_back_offset << "," << config.left_right_offset << ")"
              << " final_D500=" << write_result.x
              << " final_D502=" << write_result.y
              << " final_D504=" << write_result.angle
              << " final_D506=" << write_result.pick_status
              << " final_D508=" << write_result.head_type
              << " status=" << (write_result.pick_status >= 3.0f ? "reject" : "ready")
              << (write_result.type_compensation_failed ? " reason=type_compensation_failed" : "")
              << std::endl;
}

float CalculateAngleCalibrationOffset(float current_display_angle_deg,
                                      float target_angle_deg)
{
    float offset = target_angle_deg - current_display_angle_deg;
    offset = std::fmod(offset, 360.0f);
    if (offset > 360.0f) {
        offset -= 360.0f;
    } else if (offset < -360.0f) {
        offset += 360.0f;
    }
    return offset;
}

float CalculateAngleCalibrationOffsetFromRawAngle(float raw_angle_deg,
                                                 float target_angle_deg,
                                                 bool reverse_direction,
                                                 AngleRangeMode range_mode)
{
    PlcOutputConfig zero_offset_config;
    zero_offset_config.angle_offset_deg = 0.0f;
    zero_offset_config.angle_reverse_direction = reverse_direction;
    zero_offset_config.angle_range_mode = range_mode;
    const float current_display_angle = ApplyPlcAngleCalibration(raw_angle_deg, zero_offset_config);
    return CalculateAngleCalibrationOffset(current_display_angle, target_angle_deg);
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
        const HeadTypeCompensation& type_compensation =
            CompensationForHeadType(config, target.head_type_code);
        const bool needs_ray_offset = std::fabs(type_compensation.ac_ray_offset_mm) > 1e-6f;
        write_result.image_x = target.center_x;
        write_result.image_y = target.center_y;
        write_result.has_machine_coords = target.has_machine_coords;
        write_result.pick_status = static_cast<float>(target.pick_status_code);
        write_result.head_type = static_cast<float>(target.head_type_code);
        if (needs_ray_offset && (!target.has_machine_coords || !target.has_machine_ac_unit)) {
            write_result.pick_status = 3.0f;
            write_result.type_compensation_failed = true;
            LogPlcCompensation(target, type_compensation, config, write_result);
            return write_result;
        }

        PoseDetection command_target = target;
        if (target.has_machine_coords) {
            command_target.machine_x =
                target.machine_x + target.ac_unit_machine_x * type_compensation.ac_ray_offset_mm;
            command_target.machine_y =
                target.machine_y + target.ac_unit_machine_y * type_compensation.ac_ray_offset_mm;
        }
        write_result.x = FrontBackValue(command_target, config.axis_mapping_mode) + config.front_back_offset;
        write_result.y = LeftRightValue(command_target, config.axis_mapping_mode) + config.left_right_offset;
        write_result.angle = ApplyPlcAngleCalibration(target.angle_deg, config, type_compensation);
        LogPlcCompensation(target, type_compensation, config, write_result);
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
    target.ac_unit_machine_x = 1.0f;
    target.ac_unit_machine_y = 0.0f;
    target.has_machine_ac_unit = true;
    target.angle_deg = 90.0f;
    target.pick_status_code = 1;
    target.head_type_code = 4;
    result.detections.push_back(target);
    result.primary_index = 0;
    result.pick_status_code = 1;
    result.head_type_code = 4;
    return BuildPlcWriteResult(result, config);
}
