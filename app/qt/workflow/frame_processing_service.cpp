#include "frame_processing_service.h"

#include <cmath>

namespace {

cv::Point2f MachinePointFromPlcWrite(const PlcWriteResult& write_result,
                                     AxisMappingMode axis_mapping_mode)
{
    return axis_mapping_mode == AxisMappingMode::FrontBackMachineX
        ? cv::Point2f(write_result.x, write_result.y)
        : cv::Point2f(write_result.y, write_result.x);
}

float NormalizeAngleDiffDeg(float target_angle_deg, float source_angle_deg)
{
    float diff = std::fmod(target_angle_deg - source_angle_deg + 180.0f, 360.0f);
    if (diff < 0.0f) {
        diff += 360.0f;
    }
    return diff - 180.0f;
}

cv::Point2f RotateVector(const cv::Point2f& vector, float angle_deg)
{
    const float angle_rad = static_cast<float>(angle_deg * CV_PI / 180.0);
    const float cos_angle = std::cos(angle_rad);
    const float sin_angle = std::sin(angle_rad);
    return {
        vector.x * cos_angle - vector.y * sin_angle,
        vector.x * sin_angle + vector.y * cos_angle,
    };
}

cv::Point2f BuildCommandArrowEnd(const PoseDetection& detection,
                                 const cv::Point2f& command_center,
                                 float command_angle_deg)
{
    const cv::Point2f raw_vector = detection.arrow_end - detection.arrow_start;
    if (std::hypot(raw_vector.x, raw_vector.y) < 1e-6f) {
        return command_center;
    }
    const float angle_delta_deg = NormalizeAngleDiffDeg(command_angle_deg, detection.angle_deg);
    return command_center + RotateVector(raw_vector, angle_delta_deg);
}

void ApplyPlcCommandPose(FrameInferenceResult& result,
                         const CoordinateTransformState& coordinate_state,
                         const PlcOutputConfig& plc_config)
{
    for (int i = 0; i < static_cast<int>(result.detections.size()); ++i) {
        PoseDetection& detection = result.detections[i];
        detection.has_plc_command_pose = false;
        detection.plc_command_x = 0.0f;
        detection.plc_command_y = 0.0f;
        detection.plc_command_angle_deg = 0.0f;
        detection.plc_command_center = {};
        detection.plc_command_arrow_start = {};
        detection.plc_command_arrow_end = {};

        FrameInferenceResult single;
        single.image_width = result.image_width;
        single.image_height = result.image_height;
        single.detections = { detection };
        single.primary_index = 0;
        single.pick_status_code = detection.pick_status_code;
        single.head_type_code = detection.head_type_code;
        single.head_type_text = detection.head_type_text;
        const PlcWriteResult write_result = BuildPlcWriteResult(single, plc_config);
        if (write_result.pick_status >= 3.0f || write_result.type_compensation_failed) {
            continue;
        }

        const cv::Point2f command_machine =
            MachinePointFromPlcWrite(write_result, plc_config.axis_mapping_mode);
        detection.plc_command_x = command_machine.x;
        detection.plc_command_y = command_machine.y;
        detection.plc_command_angle_deg = write_result.angle;
        cv::Point2f command_image;
        if (TransformMachinePointToImage(coordinate_state, command_machine, &command_image)) {
            detection.plc_command_center = command_image;
            detection.plc_command_arrow_start = command_image;
            detection.plc_command_arrow_end = BuildCommandArrowEnd(detection, command_image, write_result.angle);
            detection.has_plc_command_pose = true;
        } else if (!detection.has_machine_coords) {
            detection.plc_command_center = { write_result.image_x, write_result.image_y };
            detection.plc_command_arrow_start = detection.plc_command_center;
            detection.plc_command_arrow_end = BuildCommandArrowEnd(
                detection, detection.plc_command_center, write_result.angle);
            detection.has_plc_command_pose = true;
        }
    }
}

} // namespace

FrameProcessingResult ProcessVisionFrame(GraspWorkflow& workflow,
                                         const cv::Mat& frame,
                                         const GrabLimitConfig& limits,
                                         const MechanicalGripperCollisionConfig& mechanical_gripper,
                                         const CoordinateTransformState& coordinate_state,
                                         const PlcOutputConfig& plc_config,
                                         bool reject_invalid_coordinate_state_when_limits_disabled)
{
    FrameProcessingResult process_result;

    std::string error_message;
    if (!workflow.runImage(frame, process_result.frame_result, &error_message)) {
        process_result.error_message = QString::fromStdString(error_message);
        process_result.error_stage = FrameProcessingErrorStage::Inference;
        return process_result;
    }

    ApplyCoordinateTransform(process_result.frame_result, coordinate_state);
    ApplyMechanicalGripperCollisionFilter(process_result.frame_result,
                                          coordinate_state,
                                          mechanical_gripper);
    ApplyCoordinateTransform(process_result.frame_result, coordinate_state);
    std::string roi_error;
    if (!ApplyMechanicalRoiFilter(process_result.frame_result,
                                  limits,
                                  coordinate_state,
                                  plc_config.axis_mapping_mode,
                                  &roi_error)) {
        process_result.error_message = QString::fromStdString(roi_error);
        process_result.error_stage = FrameProcessingErrorStage::RoiOrCoordinate;
        return process_result;
    }

    ApplyPlcCommandPose(process_result.frame_result, coordinate_state, plc_config);

    if (reject_invalid_coordinate_state_when_limits_disabled &&
        !limits.enabled &&
        coordinate_state.enabled &&
        !coordinate_state.valid) {
        process_result.error_message = QString::fromStdString(coordinate_state.error_message);
        process_result.error_stage = FrameProcessingErrorStage::InvalidCoordinateState;
    }
    return process_result;
}
