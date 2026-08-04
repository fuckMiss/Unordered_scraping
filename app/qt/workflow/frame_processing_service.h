#pragma once

#include "coordinate_transform.h"
#include "grab_limit_evaluator.h"
#include "grasp_workflow.h"
#include "plc_result_contract.h"

#include <opencv2/opencv.hpp>

#include <QString>

enum class FrameProcessingErrorStage {
    None,
    Inference,
    RoiOrCoordinate,
    InvalidCoordinateState
};

struct FrameProcessingResult
{
    QString error_message;
    FrameProcessingErrorStage error_stage = FrameProcessingErrorStage::None;
    FrameInferenceResult frame_result;
};

FrameProcessingResult ProcessVisionFrame(GraspWorkflow& workflow,
                                         const cv::Mat& frame,
                                         const GrabLimitConfig& limits,
                                         const MechanicalGripperCollisionConfig& mechanical_gripper,
                                         const CoordinateTransformState& coordinate_state,
                                         AxisMappingMode axis_mapping_mode,
                                         bool reject_invalid_coordinate_state_when_limits_disabled);
