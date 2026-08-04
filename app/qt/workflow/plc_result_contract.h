#pragma once

#include "frame_result.h"

struct PlcWriteResult
{
    float image_x = 0.0f;
    float image_y = 0.0f;
    float x = 0.0f;
    float y = 0.0f;
    float angle = 0.0f;
    float pick_status = 3.0f;
    float head_type = 0.0f;
    bool has_machine_coords = false;
};

enum class AngleRangeMode {
    ZeroTo360,
    Signed180
};

enum class AxisMappingMode {
    FrontBackMachineY,
    FrontBackMachineX
};

struct PlcOutputConfig
{
    float angle_offset_deg = 0.0f;
    bool angle_reverse_direction = false;
    AngleRangeMode angle_range_mode = AngleRangeMode::ZeroTo360;
    AxisMappingMode axis_mapping_mode = AxisMappingMode::FrontBackMachineY;
    float front_back_offset = 0.0f;
    float left_right_offset = 0.0f;
};

float ApplyPlcAngleCalibration(float angle_deg, const PlcOutputConfig& config);
float CalculateAngleCalibrationOffset(float current_display_angle_deg,
                                      float target_angle_deg);
float CalculateAngleCalibrationOffsetFromRawAngle(float raw_angle_deg,
                                                 float target_angle_deg,
                                                 bool reverse_direction,
                                                 AngleRangeMode range_mode);
float FrontBackValue(const PoseDetection& detection, AxisMappingMode axis_mapping_mode);
float LeftRightValue(const PoseDetection& detection, AxisMappingMode axis_mapping_mode);
PlcWriteResult BuildPlcWriteResult(const FrameInferenceResult& result, const PlcOutputConfig& config);
PlcWriteResult BuildDiagnosticPlcWriteResult(const PlcOutputConfig& config);
