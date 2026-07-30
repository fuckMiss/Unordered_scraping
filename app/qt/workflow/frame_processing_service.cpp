#include "frame_processing_service.h"

FrameProcessingResult ProcessVisionFrame(GraspWorkflow& workflow,
                                         const cv::Mat& frame,
                                         const GrabLimitConfig& limits,
                                         const CoordinateTransformState& coordinate_state,
                                         AxisMappingMode axis_mapping_mode,
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
    std::string roi_error;
    if (!ApplyMechanicalRoiFilter(process_result.frame_result,
                                  limits,
                                  coordinate_state,
                                  axis_mapping_mode,
                                  &roi_error)) {
        process_result.error_message = QString::fromStdString(roi_error);
        process_result.error_stage = FrameProcessingErrorStage::RoiOrCoordinate;
        return process_result;
    }

    if (reject_invalid_coordinate_state_when_limits_disabled &&
        !limits.enabled &&
        coordinate_state.enabled &&
        !coordinate_state.valid) {
        process_result.error_message = QString::fromStdString(coordinate_state.error_message);
        process_result.error_stage = FrameProcessingErrorStage::InvalidCoordinateState;
    }
    return process_result;
}
