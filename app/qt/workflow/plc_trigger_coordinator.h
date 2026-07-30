#pragma once

#include "coordinate_transform.h"
#include "grab_limit_evaluator.h"
#include "grasp_workflow.h"
#include "plc_result_contract.h"
#include "robot_controller.h"

#include <opencv2/opencv.hpp>

#include <QString>

#include <chrono>

struct PlcTriggerProcessResult
{
    QString error_message;
    QString reject_reason;
    QString structured_log_line;
    FrameInferenceResult frame_result;
    PlcWriteResult plc_write_result;
    PlcRegisterMap plc_registers;
    bool had_frame = true;
    bool wrote_result = false;
    bool wrote_reject = false;
    bool trigger_cleared = false;
    double detect_ms = 0.0;
    double write_and_trigger_clear_ms = 0.0;
    double trigger_to_signal_ms = 0.0;
};

struct PlcManualWriteResult
{
    QString error_message;
};

struct PlcTriggerReadResult
{
    QString error_message;
    bool triggered = false;
};

PlcTriggerReadResult ReadPlcPhotoTrigger(RobotController& robot_controller);

PlcTriggerProcessResult ProcessPlcTriggeredFrame(GraspWorkflow& workflow,
                                                 RobotController& robot_controller,
                                                 const cv::Mat& frame,
                                                 const GrabLimitConfig& limits,
                                                 const CoordinateTransformState& coordinate_state,
                                                 const PlcOutputConfig& plc_config,
                                                 const std::chrono::steady_clock::time_point& plc_start);

PlcTriggerProcessResult ProcessPlcTriggeredMissingFrame(RobotController& robot_controller,
                                                        const PlcOutputConfig& plc_config,
                                                        const std::chrono::steady_clock::time_point& plc_start);

PlcManualWriteResult WriteFrameResultToPlc(RobotController& robot_controller,
                                           const FrameInferenceResult& result,
                                           const GrabLimitConfig& limits,
                                           const CoordinateTransformState& coordinate_state,
                                           const PlcOutputConfig& plc_config,
                                           bool clear_trigger);
