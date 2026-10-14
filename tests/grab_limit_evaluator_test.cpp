#include "grab_limit_evaluator.h"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

FrameInferenceResult MakeResult(float machine_x, float machine_y, float angle_deg = 30.0f)
{
    FrameInferenceResult result;
    PoseDetection target;
    target.machine_x = machine_x;
    target.machine_y = machine_y;
    target.has_machine_coords = true;
    target.center_x = 10.0f;
    target.center_y = 20.0f;
    target.angle_deg = angle_deg;
    target.confidence = 0.9f;
    target.can_grab = true;
    target.pick_status_code = 1;
    target.head_type_code = 4;
    target.head_type_text = "小上右";
    result.detections.push_back(target);
    result.primary_index = 0;
    result.pick_status_code = 1;
    result.head_type_code = 4;
    return result;
}

GrabLimitConfig MakeLimits()
{
    GrabLimitConfig limits;
    limits.enabled = true;
    limits.roi_margin = 5.0;
    limits.x = { 40.0, 70.0 };
    limits.y = { 110.0, 130.0 };
    limits.angle = { 0.0, 180.0 };
    return limits;
}

GrabLimitConfig MakeImageProtectionLimits(double lower = 5.0,
                                          double upper = 95.0,
                                          double roi_margin = 0.0)
{
    GrabLimitConfig limits;
    limits.enabled = true;
    limits.roi_margin = roi_margin;
    limits.x = { lower, upper };
    limits.y = { lower, upper };
    limits.angle = { 0.0, 180.0 };
    return limits;
}

CoordinateTransformState ValidCoordinateState()
{
    CoordinateTransformState state;
    state.enabled = true;
    state.valid = true;
    return state;
}

CoordinateTransformState OverlayCoordinateState()
{
    CoordinateTransformConfig config;
    config.enabled = true;
    config.points = {
        { 0.0, 0.0, 100.0, 200.0 },
        { 10.0, 0.0, 120.0, 200.0 },
        { 0.0, 10.0, 100.0, 230.0 },
        { 10.0, 10.0, 120.0, 230.0 },
    };
    return BuildCoordinateTransformState(config);
}

CoordinateTransformState IdentityCoordinateState()
{
    CoordinateTransformConfig config;
    config.enabled = true;
    config.points = {
        { 0.0, 0.0, 0.0, 0.0 },
        { 100.0, 0.0, 100.0, 0.0 },
        { 0.0, 100.0, 0.0, 100.0 },
        { 100.0, 100.0, 100.0, 100.0 },
    };
    return BuildCoordinateTransformState(config);
}

FrameInferenceResult MakeMechanicalGripperCollisionResult()
{
    FrameInferenceResult result;
    result.image_width = 100;
    result.image_height = 100;

    SegRegion current_segment;
    current_segment.mask = cv::Mat(100, 100, CV_8U, cv::Scalar(0));
    current_segment.mask(cv::Rect(45, 45, 10, 10)).setTo(cv::Scalar(255));
    current_segment.bbox = cv::Rect(45, 45, 10, 10);

    SegRegion neighbor_segment;
    neighbor_segment.mask = cv::Mat(100, 100, CV_8U, cv::Scalar(0));
    neighbor_segment.mask(cv::Rect(60, 48, 8, 4)).setTo(cv::Scalar(255));
    neighbor_segment.bbox = cv::Rect(60, 48, 8, 4);

    result.segments.push_back(current_segment);
    result.segments.push_back(neighbor_segment);

    PoseDetection target;
    target.matched_segment_index = 0;
    target.confidence = 0.9f;
    target.can_grab = true;
    target.pick_status_code = 1;
    target.head_type_code = 4;
    target.head_type_text = "test";
    target.center_x = 50.0f;
    target.center_y = 50.0f;
    target.obb_center = { 50.0f, 50.0f };
    target.seg_center = { 0.0f, 50.0f };
    target.arrow_start = { 50.0f, 50.0f };
    target.arrow_end = { 60.0f, 50.0f };
    target.angle_deg = 0.0f;
    target.grip_long_angle_deg = 90.0f;
    result.detections.push_back(target);
    result.primary_index = 0;
    result.pick_status_code = 1;
    result.head_type_code = 4;
    result.head_type_text = "test";
    return result;
}

bool NearlyEqual(float left, float right)
{
    return std::fabs(left - right) < 0.02f;
}

bool ParallelModulo180(cv::Point2f first, cv::Point2f second)
{
    const float first_length = std::hypot(first.x, first.y);
    const float second_length = std::hypot(second.x, second.y);
    return first_length >= 1e-6f &&
           second_length >= 1e-6f &&
           std::fabs(first.cross(second)) / (first_length * second_length) < 0.01f;
}

void RequireDirectionContract(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "direction contract failed: " << message << std::endl;
        std::abort();
    }
}

void MechanicalGripperCornersFollowLongAxis()
{
    const float angles[] = { 0.0f, 90.0f, 45.0f, 135.0f };
    for (const float angle : angles) {
        FrameInferenceResult result = MakeMechanicalGripperCollisionResult();
        result.segments.resize(1);
        PoseDetection& target = result.detections.front();
        target.grip_long_angle_deg = angle;
        const float ac_angle_rad = (angle + 90.0f) * static_cast<float>(CV_PI) / 180.0f;
        target.arrow_end = target.arrow_start +
                           cv::Point2f(std::cos(ac_angle_rad), std::sin(ac_angle_rad)) * 10.0f;
        target.angle_deg = angle + 90.0f;
        const MechanicalGripperCollisionConfig config{ 40.0, 20.0, 10.0, false };

        ApplyMechanicalGripperCollisionFilter(result,
                                              IdentityCoordinateState(),
                                              config,
                                              MakeImageProtectionLimits(),
                                              AxisMappingMode::FrontBackMachineX);

        RequireDirectionContract(result.detections.front().can_grab,
                                 "mechanical gripper coverage requires a grabbable target");
        const std::vector<cv::Point2f>& corners = result.detections.front().mechanical_gripper_corners;
        RequireDirectionContract(corners.size() == 4,
                                 "mechanical gripper must have four corners");
        const float angle_rad = angle * static_cast<float>(CV_PI) / 180.0f;
        const cv::Point2f expected_long_axis(std::cos(angle_rad), std::sin(angle_rad));
        const cv::Point2f actual_long_axis = corners[1] - corners[0];
        RequireDirectionContract(ParallelModulo180(actual_long_axis, expected_long_axis),
                                 "mechanical gripper long axis must match grip long angle");
    }
}

void MechanicalGripperSizeCannotChangeUpstreamBaseFrame()
{
    FrameInferenceResult short_gripper = MakeMechanicalGripperCollisionResult();
    FrameInferenceResult wide_gripper = short_gripper;
    short_gripper.segments.resize(1);
    wide_gripper.segments.resize(1);

    const std::vector<cv::Point2f> base_corners = {
        { 45.0f, 30.0f },
        { 55.0f, 30.0f },
        { 55.0f, 70.0f },
        { 45.0f, 70.0f },
    };
    short_gripper.detections.front().corners = base_corners;
    wide_gripper.detections.front().corners = base_corners;
    short_gripper.detections.front().grip_long_angle_deg = 90.0f;
    wide_gripper.detections.front().grip_long_angle_deg = 90.0f;
    short_gripper.detections.front().arrow_start = { 50.0f, 50.0f };
    short_gripper.detections.front().arrow_end = { 60.0f, 50.0f };
    wide_gripper.detections.front().arrow_start = short_gripper.detections.front().arrow_start;
    wide_gripper.detections.front().arrow_end = short_gripper.detections.front().arrow_end;
    short_gripper.detections.front().angle_deg = 0.0f;
    wide_gripper.detections.front().angle_deg = 0.0f;

    const MechanicalGripperCollisionConfig short_config{ 20.0, 10.0, 10.0, false };
    const MechanicalGripperCollisionConfig wide_config{ 60.0, 20.0, 10.0, false };
    ApplyMechanicalGripperCollisionFilter(short_gripper,
                                          IdentityCoordinateState(),
                                          short_config,
                                          MakeImageProtectionLimits(),
                                          AxisMappingMode::FrontBackMachineX);
    ApplyMechanicalGripperCollisionFilter(wide_gripper,
                                          IdentityCoordinateState(),
                                          wide_config,
                                          MakeImageProtectionLimits(),
                                          AxisMappingMode::FrontBackMachineX);

    const PoseDetection& short_target = short_gripper.detections.front();
    const PoseDetection& wide_target = wide_gripper.detections.front();
    RequireDirectionContract(short_target.can_grab && wide_target.can_grab,
                             "size independence coverage requires accepted grippers");
    RequireDirectionContract(short_target.mechanical_gripper_corners.size() == 4 &&
                                 wide_target.mechanical_gripper_corners.size() == 4,
                             "size independence coverage requires two mechanical rectangles");
    RequireDirectionContract(ParallelModulo180(short_target.mechanical_gripper_corners[1] -
                                                   short_target.mechanical_gripper_corners[0],
                                               { 0.0f, 1.0f }) &&
                                 ParallelModulo180(wide_target.mechanical_gripper_corners[1] -
                                                       wide_target.mechanical_gripper_corners[0],
                                                   { 0.0f, 1.0f }),
                             "real gripper rectangles must keep the base frame direction");
    RequireDirectionContract(NearlyEqual(short_target.arrow_start.x, wide_target.arrow_start.x) &&
                                 NearlyEqual(short_target.arrow_start.y, wide_target.arrow_start.y) &&
                                 NearlyEqual(short_target.arrow_end.x, wide_target.arrow_end.x) &&
                                 NearlyEqual(short_target.arrow_end.y, wide_target.arrow_end.y) &&
                                 NearlyEqual(short_target.angle_deg, wide_target.angle_deg),
                             "real gripper size must not change AC or angle");
    RequireDirectionContract(short_target.corners.size() == wide_target.corners.size(),
                             "real gripper size must not change the base frame shape");
    for (size_t i = 0; i < base_corners.size(); ++i) {
        RequireDirectionContract(NearlyEqual(short_target.corners[i].x, base_corners[i].x) &&
                                     NearlyEqual(short_target.corners[i].y, base_corners[i].y) &&
                                     NearlyEqual(wide_target.corners[i].x, base_corners[i].x) &&
                                     NearlyEqual(wide_target.corners[i].y, base_corners[i].y),
                                 "real gripper size must not change the base frame corners");
    }
    RequireDirectionContract(
        std::hypot(short_target.mechanical_gripper_corners[1].x -
                       short_target.mechanical_gripper_corners[0].x,
                   short_target.mechanical_gripper_corners[1].y -
                       short_target.mechanical_gripper_corners[0].y) <
            std::hypot(wide_target.mechanical_gripper_corners[1].x -
                           wide_target.mechanical_gripper_corners[0].x,
                       wide_target.mechanical_gripper_corners[1].y -
                           wide_target.mechanical_gripper_corners[0].y),
        "real gripper size must change only the real gripper rectangle");
}

void InRangeTargetIsAccepted()
{
    PlcOutputConfig plc_config;
    FrameInferenceResult result = MakeResult(120.0f, 55.0f);

    const GrabLimitDecision decision = EvaluateGrabLimits(result, MakeLimits(), plc_config);
    assert(!decision.rejected);
}

void OutOfRangeTargetIsRejected()
{
    PlcOutputConfig plc_config;
    FrameInferenceResult result = MakeResult(200.0f, 55.0f);

    const GrabLimitDecision decision = EvaluateGrabLimits(result, MakeLimits(), plc_config);
    assert(decision.rejected);
    assert(!decision.reason.empty());
}

void AxisCompensationAffectsFinalLimitDecision()
{
    PlcOutputConfig plc_config;
    plc_config.front_back_offset = 10000.0f;
    plc_config.left_right_offset = -10000.0f;
    FrameInferenceResult result = MakeResult(120.0f, 55.0f);

    const GrabLimitDecision decision = EvaluateGrabLimits(result, MakeLimits(), plc_config);
    assert(decision.rejected);
    assert(!decision.reason.empty());
}

void AngleLimitsUseCalibratedPlcAngle()
{
    PlcOutputConfig plc_config;
    plc_config.angle_offset_deg = 180.0f;
    FrameInferenceResult result = MakeResult(120.0f, 55.0f, 30.0f);

    const GrabLimitDecision decision = EvaluateGrabLimits(result, MakeLimits(), plc_config);
    assert(decision.rejected);
    assert(!decision.reason.empty());
}

void TypeCenterCompensationAffectsFinalLimitDecision()
{
    PlcOutputConfig plc_config;
    plc_config.head_type_compensations[4].ac_ray_offset_mm = 20.0f;
    FrameInferenceResult result = MakeResult(120.0f, 55.0f);
    result.detections[0].ac_unit_machine_x = 0.0f;
    result.detections[0].ac_unit_machine_y = 1.0f;
    result.detections[0].has_machine_ac_unit = true;

    const GrabLimitDecision decision = EvaluateGrabLimits(result, MakeLimits(), plc_config);
    assert(decision.rejected);
    assert(!decision.reason.empty());
}

void TypeAngleCompensationAffectsFinalLimitDecision()
{
    PlcOutputConfig plc_config;
    plc_config.head_type_compensations[4].angle_offset_deg = 160.0f;
    FrameInferenceResult result = MakeResult(120.0f, 55.0f, 30.0f);

    const GrabLimitDecision decision = EvaluateGrabLimits(result, MakeLimits(), plc_config);
    assert(decision.rejected);
    assert(!decision.reason.empty());
}

void TypeCenterCompensationRequiresMachineAcDirection()
{
    PlcOutputConfig plc_config;
    plc_config.head_type_compensations[4].ac_ray_offset_mm = 1.0f;
    FrameInferenceResult result = MakeResult(120.0f, 55.0f);

    const GrabLimitDecision decision = EvaluateGrabLimits(result, MakeLimits(), plc_config);
    assert(decision.rejected);
    assert(!decision.reason.empty());
}

void MechanicalRoiRequiresValidCoordinateTransform()
{
    FrameInferenceResult result = MakeResult(120.0f, 55.0f);
    CoordinateTransformState state;
    state.enabled = true;
    state.valid = false;

    std::string error;
    assert(!ApplyMechanicalRoiFilter(result,
                                     MakeLimits(),
                                     state,
                                     AxisMappingMode::FrontBackMachineY,
                                     &error));
    assert(!error.empty());
    assert(result.detections.empty());
    assert(result.pick_status_code == 3);
}

void MechanicalRoiFiltersOutsideTargetsAndResolvesPrimary()
{
    FrameInferenceResult result;
    result.detections.push_back(MakeResult(120.0f, 55.0f).detections[0]);
    result.detections.push_back(MakeResult(200.0f, 55.0f).detections[0]);
    result.primary_index = 0;
    result.pick_status_code = 2;

    std::string error;
    assert(ApplyMechanicalRoiFilter(result,
                                    MakeLimits(),
                                    ValidCoordinateState(),
                                    AxisMappingMode::FrontBackMachineY,
                                    &error));
    assert(result.detections.size() == 1);
    assert(result.primary_index == 0);
    assert(result.pick_status_code == 1);
}

void MechanicalGripperCollisionRejectsNeighborMaskOutsideSelf()
{
    FrameInferenceResult result = MakeMechanicalGripperCollisionResult();
    const MechanicalGripperCollisionConfig config{ 40.0, 20.0, 10.0, false };

    ApplyMechanicalGripperCollisionFilter(result,
                                          IdentityCoordinateState(),
                                          config,
                                          MakeImageProtectionLimits(),
                                          AxisMappingMode::FrontBackMachineX);

    RequireDirectionContract(result.detections.size() == 1, "MechanicalGripperCollisionRejectsNeighborMaskOutsideSelf");
    RequireDirectionContract(!result.detections.front().can_grab, "MechanicalGripperCollisionRejectsNeighborMaskOutsideSelf");
    RequireDirectionContract(result.detections.front().pick_status_code == 3, "MechanicalGripperCollisionRejectsNeighborMaskOutsideSelf");
    RequireDirectionContract(result.detections.front().mechanical_gripper_corners.size() == 4, "MechanicalGripperCollisionRejectsNeighborMaskOutsideSelf");
    RequireDirectionContract(result.detections.front().mask_collisions.size() == 1, "MechanicalGripperCollisionRejectsNeighborMaskOutsideSelf");
    RequireDirectionContract(result.primary_index == -1, "MechanicalGripperCollisionRejectsNeighborMaskOutsideSelf");
    RequireDirectionContract(result.pick_status_code == 3, "MechanicalGripperCollisionRejectsNeighborMaskOutsideSelf");
}

void MechanicalGripperInvalidSizeRejectsConservatively()
{
    FrameInferenceResult result = MakeMechanicalGripperCollisionResult();
    const MechanicalGripperCollisionConfig config{ 0.0, 20.0, 10.0, false };

    ApplyMechanicalGripperCollisionFilter(result,
                                          IdentityCoordinateState(),
                                          config,
                                          MakeImageProtectionLimits(),
                                          AxisMappingMode::FrontBackMachineX);

    RequireDirectionContract(!result.detections.front().can_grab, "MechanicalGripperInvalidSizeRejectsConservatively");
    RequireDirectionContract(result.detections.front().mechanical_gripper_corners.empty(), "MechanicalGripperInvalidSizeRejectsConservatively");
    RequireDirectionContract(result.primary_index == -1, "MechanicalGripperInvalidSizeRejectsConservatively");
    RequireDirectionContract(result.pick_status_code == 3, "MechanicalGripperInvalidSizeRejectsConservatively");
}

void MechanicalGripperInvalidCoordinateRejectsConservatively()
{
    FrameInferenceResult result = MakeMechanicalGripperCollisionResult();
    CoordinateTransformState state;
    state.enabled = true;
    state.valid = false;
    const MechanicalGripperCollisionConfig config{ 40.0, 20.0, 10.0, false };

    ApplyMechanicalGripperCollisionFilter(result,
                                          state,
                                          config,
                                          MakeImageProtectionLimits(),
                                          AxisMappingMode::FrontBackMachineX);

    RequireDirectionContract(!result.detections.front().can_grab, "MechanicalGripperInvalidCoordinateRejectsConservatively");
    RequireDirectionContract(result.detections.front().mechanical_gripper_corners.empty(), "MechanicalGripperInvalidCoordinateRejectsConservatively");
    RequireDirectionContract(result.primary_index == -1, "MechanicalGripperInvalidCoordinateRejectsConservatively");
    RequireDirectionContract(result.pick_status_code == 3, "MechanicalGripperInvalidCoordinateRejectsConservatively");
}

void MechanicalGripperPreservesAcPoseAndOffsetsCenter()
{
    FrameInferenceResult result = MakeMechanicalGripperCollisionResult();
    result.segments[1].mask.setTo(cv::Scalar(0));
    const MechanicalGripperCollisionConfig config{ 40.0, 20.0, 10.0, false };

    ApplyMechanicalGripperCollisionFilter(result,
                                          IdentityCoordinateState(),
                                          config,
                                          MakeImageProtectionLimits(),
                                          AxisMappingMode::FrontBackMachineX);

    RequireDirectionContract(result.detections.front().can_grab, "MechanicalGripperPreservesAcPoseAndOffsetsCenter");
    RequireDirectionContract(NearlyEqual(result.detections.front().arrow_start.x, 50.0f), "MechanicalGripperPreservesAcPoseAndOffsetsCenter");
    RequireDirectionContract(NearlyEqual(result.detections.front().arrow_start.y, 50.0f), "MechanicalGripperPreservesAcPoseAndOffsetsCenter");
    RequireDirectionContract(NearlyEqual(result.detections.front().arrow_end.x, 60.0f), "MechanicalGripperPreservesAcPoseAndOffsetsCenter");
    RequireDirectionContract(NearlyEqual(result.detections.front().arrow_end.y, 50.0f), "MechanicalGripperPreservesAcPoseAndOffsetsCenter");
    RequireDirectionContract(NearlyEqual(result.detections.front().angle_deg, 0.0f), "MechanicalGripperPreservesAcPoseAndOffsetsCenter");
    RequireDirectionContract(NearlyEqual(result.detections.front().center_x, 60.0f), "MechanicalGripperPreservesAcPoseAndOffsetsCenter");
    RequireDirectionContract(NearlyEqual(result.detections.front().center_y, 50.0f), "MechanicalGripperPreservesAcPoseAndOffsetsCenter");
    const std::vector<cv::Point2f>& corners = result.detections.front().mechanical_gripper_corners;
    RequireDirectionContract(corners.size() == 4, "MechanicalGripperPreservesAcPoseAndOffsetsCenter");
    RequireDirectionContract(NearlyEqual(std::hypot(corners[1].x - corners[0].x, corners[1].y - corners[0].y), 40.0f), "MechanicalGripperPreservesAcPoseAndOffsetsCenter");
    RequireDirectionContract(NearlyEqual(std::hypot(corners[3].x - corners[0].x, corners[3].y - corners[0].y), 20.0f), "MechanicalGripperPreservesAcPoseAndOffsetsCenter");
    RequireDirectionContract(NearlyEqual(corners[0].x, 60.0f), "MechanicalGripperPreservesAcPoseAndOffsetsCenter");
    RequireDirectionContract(NearlyEqual(corners[1].x, 60.0f), "MechanicalGripperPreservesAcPoseAndOffsetsCenter");
    RequireDirectionContract(NearlyEqual(corners[0].y, 30.0f), "MechanicalGripperPreservesAcPoseAndOffsetsCenter");
    RequireDirectionContract(NearlyEqual(corners[1].y, 70.0f), "MechanicalGripperPreservesAcPoseAndOffsetsCenter");
    RequireDirectionContract(result.primary_index == 0, "MechanicalGripperPreservesAcPoseAndOffsetsCenter");
    RequireDirectionContract(result.pick_status_code == 1, "MechanicalGripperPreservesAcPoseAndOffsetsCenter");
}

void MechanicalGripperDoesNotReplaceDownwardAcWithSideBoundary()
{
    FrameInferenceResult result = MakeMechanicalGripperCollisionResult();
    result.segments[1].mask.setTo(cv::Scalar(0));
    PoseDetection& target = result.detections.front();
    target.arrow_start = { 50.0f, 50.0f };
    target.arrow_end = { 50.0f, 60.0f };
    target.angle_deg = 90.0f;
    target.grip_long_angle_deg = 90.0f;
    const MechanicalGripperCollisionConfig config{ 40.0, 20.0, 10.0, false };

    ApplyMechanicalGripperCollisionFilter(result,
                                          IdentityCoordinateState(),
                                          config,
                                          MakeImageProtectionLimits(),
                                          AxisMappingMode::FrontBackMachineX);

    RequireDirectionContract(result.detections.front().can_grab, "MechanicalGripperDoesNotReplaceDownwardAcWithSideBoundary");
    RequireDirectionContract(NearlyEqual(result.detections.front().arrow_start.x, 50.0f), "MechanicalGripperDoesNotReplaceDownwardAcWithSideBoundary");
    RequireDirectionContract(NearlyEqual(result.detections.front().arrow_start.y, 50.0f), "MechanicalGripperDoesNotReplaceDownwardAcWithSideBoundary");
    RequireDirectionContract(NearlyEqual(result.detections.front().arrow_end.x, 50.0f), "MechanicalGripperDoesNotReplaceDownwardAcWithSideBoundary");
    RequireDirectionContract(NearlyEqual(result.detections.front().arrow_end.y, 60.0f), "MechanicalGripperDoesNotReplaceDownwardAcWithSideBoundary");
    RequireDirectionContract(NearlyEqual(result.detections.front().angle_deg, 90.0f), "MechanicalGripperDoesNotReplaceDownwardAcWithSideBoundary");
    RequireDirectionContract(NearlyEqual(result.detections.front().center_x, 50.0f), "MechanicalGripperDoesNotReplaceDownwardAcWithSideBoundary");
    RequireDirectionContract(NearlyEqual(result.detections.front().center_y, 60.0f), "MechanicalGripperDoesNotReplaceDownwardAcWithSideBoundary");
    RequireDirectionContract(result.primary_index == 0, "MechanicalGripperDoesNotReplaceDownwardAcWithSideBoundary");
    RequireDirectionContract(result.pick_status_code == 1, "MechanicalGripperDoesNotReplaceDownwardAcWithSideBoundary");
}

void MechanicalGripperInsideProtectionPolygonIsAccepted()
{
    FrameInferenceResult result = MakeMechanicalGripperCollisionResult();
    result.segments[1].mask.setTo(cv::Scalar(0));
    const MechanicalGripperCollisionConfig config{ 40.0, 20.0, 10.0, false };

    ApplyMechanicalGripperCollisionFilter(result,
                                          IdentityCoordinateState(),
                                          config,
                                          MakeImageProtectionLimits(),
                                          AxisMappingMode::FrontBackMachineX);

    RequireDirectionContract(result.detections.front().can_grab, "MechanicalGripperInsideProtectionPolygonIsAccepted");
    RequireDirectionContract(result.primary_index == 0, "MechanicalGripperInsideProtectionPolygonIsAccepted");
    RequireDirectionContract(result.pick_status_code == 1, "MechanicalGripperInsideProtectionPolygonIsAccepted");
}

void MechanicalGripperCornerOutsideProtectionPolygonIsRejected()
{
    FrameInferenceResult result = MakeMechanicalGripperCollisionResult();
    result.segments[1].mask.setTo(cv::Scalar(0));
    const MechanicalGripperCollisionConfig config{ 40.0, 20.0, 10.0, false };

    ApplyMechanicalGripperCollisionFilter(result,
                                          IdentityCoordinateState(),
                                          config,
                                          MakeImageProtectionLimits(35.0, 95.0),
                                          AxisMappingMode::FrontBackMachineX);

    RequireDirectionContract(!result.detections.front().can_grab, "MechanicalGripperCornerOutsideProtectionPolygonIsRejected");
    RequireDirectionContract(result.detections.front().pick_status_code == 3, "MechanicalGripperCornerOutsideProtectionPolygonIsRejected");
    RequireDirectionContract(result.primary_index == -1, "MechanicalGripperCornerOutsideProtectionPolygonIsRejected");
    RequireDirectionContract(result.pick_status_code == 3, "MechanicalGripperCornerOutsideProtectionPolygonIsRejected");
}

void MechanicalGripperTouchingProtectionBoundaryIsRejected()
{
    FrameInferenceResult result = MakeMechanicalGripperCollisionResult();
    result.segments[1].mask.setTo(cv::Scalar(0));
    const MechanicalGripperCollisionConfig config{ 40.0, 20.0, 10.0, false };

    ApplyMechanicalGripperCollisionFilter(result,
                                          IdentityCoordinateState(),
                                          config,
                                          MakeImageProtectionLimits(30.0, 95.0),
                                          AxisMappingMode::FrontBackMachineX);

    RequireDirectionContract(!result.detections.front().can_grab, "MechanicalGripperTouchingProtectionBoundaryIsRejected");
    RequireDirectionContract(result.detections.front().pick_status_code == 3, "MechanicalGripperTouchingProtectionBoundaryIsRejected");
    RequireDirectionContract(result.primary_index == -1, "MechanicalGripperTouchingProtectionBoundaryIsRejected");
    RequireDirectionContract(result.pick_status_code == 3, "MechanicalGripperTouchingProtectionBoundaryIsRejected");
}

void MechanicalGripperPartlyOutsideProtectionPolygonIsRejected()
{
    FrameInferenceResult result = MakeMechanicalGripperCollisionResult();
    result.segments[1].mask.setTo(cv::Scalar(0));
    const MechanicalGripperCollisionConfig config{ 80.0, 20.0, 10.0, false };

    ApplyMechanicalGripperCollisionFilter(result,
                                          IdentityCoordinateState(),
                                          config,
                                          MakeImageProtectionLimits(5.0, 70.0),
                                          AxisMappingMode::FrontBackMachineX);

    RequireDirectionContract(!result.detections.front().can_grab, "MechanicalGripperPartlyOutsideProtectionPolygonIsRejected");
    RequireDirectionContract(result.detections.front().pick_status_code == 3, "MechanicalGripperPartlyOutsideProtectionPolygonIsRejected");
    RequireDirectionContract(result.primary_index == -1, "MechanicalGripperPartlyOutsideProtectionPolygonIsRejected");
    RequireDirectionContract(result.pick_status_code == 3, "MechanicalGripperPartlyOutsideProtectionPolygonIsRejected");
}

void MechanicalGripperInvalidProtectionPolygonRejectsConservatively()
{
    FrameInferenceResult result = MakeMechanicalGripperCollisionResult();
    result.segments[1].mask.setTo(cv::Scalar(0));
    const MechanicalGripperCollisionConfig config{ 40.0, 20.0, 10.0, false };

    ApplyMechanicalGripperCollisionFilter(result,
                                          IdentityCoordinateState(),
                                          config,
                                          MakeImageProtectionLimits(5.0, 95.0, 1000.0),
                                          AxisMappingMode::FrontBackMachineX);

    RequireDirectionContract(!result.detections.front().can_grab, "MechanicalGripperInvalidProtectionPolygonRejectsConservatively");
    RequireDirectionContract(result.detections.front().pick_status_code == 3, "MechanicalGripperInvalidProtectionPolygonRejectsConservatively");
    RequireDirectionContract(result.primary_index == -1, "MechanicalGripperInvalidProtectionPolygonRejectsConservatively");
    RequireDirectionContract(result.pick_status_code == 3, "MechanicalGripperInvalidProtectionPolygonRejectsConservatively");
}

void OverlayPolygonUsesInnerRoiAndDefaultAxisMapping()
{
    GrabLimitConfig limits;
    limits.enabled = true;
    limits.roi_margin = 5.0;
    limits.x = { 200.0, 230.0 };
    limits.y = { 100.0, 120.0 };

    const GrabLimitOverlayPolygon polygon =
        BuildGrabLimitOverlayPolygon(limits, OverlayCoordinateState(), AxisMappingMode::FrontBackMachineY);
    assert(polygon.visible);
    assert(polygon.image_points.size() == 4);
    assert(NearlyEqual(polygon.image_points[0].x, 2.5f));
    assert(NearlyEqual(polygon.image_points[0].y, 1.67f));
    assert(NearlyEqual(polygon.image_points[2].x, 7.5f));
    assert(NearlyEqual(polygon.image_points[2].y, 8.33f));
}

void OverlayPolygonUsesFrontBackMachineXMapping()
{
    GrabLimitConfig limits;
    limits.enabled = true;
    limits.roi_margin = 5.0;
    limits.x = { 100.0, 120.0 };
    limits.y = { 200.0, 230.0 };

    const GrabLimitOverlayPolygon polygon =
        BuildGrabLimitOverlayPolygon(limits, OverlayCoordinateState(), AxisMappingMode::FrontBackMachineX);
    assert(polygon.visible);
    assert(polygon.image_points.size() == 4);
    assert(NearlyEqual(polygon.image_points[0].x, 2.5f));
    assert(NearlyEqual(polygon.image_points[0].y, 1.67f));
    assert(NearlyEqual(polygon.image_points[2].x, 7.5f));
    assert(NearlyEqual(polygon.image_points[2].y, 8.33f));
}

void OverlayPolygonRejectsEmptyInnerRoi()
{
    GrabLimitConfig limits = MakeLimits();
    limits.roi_margin = 1000.0;

    const GrabLimitOverlayPolygon polygon =
        BuildGrabLimitOverlayPolygon(limits, OverlayCoordinateState(), AxisMappingMode::FrontBackMachineY);
    assert(!polygon.visible);
    assert(polygon.image_points.empty());
    assert(!polygon.reason.empty());
}

} // namespace

int main()
{
    InRangeTargetIsAccepted();
    OutOfRangeTargetIsRejected();
    AxisCompensationAffectsFinalLimitDecision();
    AngleLimitsUseCalibratedPlcAngle();
    TypeCenterCompensationAffectsFinalLimitDecision();
    TypeAngleCompensationAffectsFinalLimitDecision();
    TypeCenterCompensationRequiresMachineAcDirection();
    MechanicalRoiRequiresValidCoordinateTransform();
    MechanicalRoiFiltersOutsideTargetsAndResolvesPrimary();
    MechanicalGripperCollisionRejectsNeighborMaskOutsideSelf();
    MechanicalGripperInvalidSizeRejectsConservatively();
    MechanicalGripperInvalidCoordinateRejectsConservatively();
    MechanicalGripperCornersFollowLongAxis();
    MechanicalGripperSizeCannotChangeUpstreamBaseFrame();
    MechanicalGripperPreservesAcPoseAndOffsetsCenter();
    MechanicalGripperDoesNotReplaceDownwardAcWithSideBoundary();
    MechanicalGripperInsideProtectionPolygonIsAccepted();
    MechanicalGripperCornerOutsideProtectionPolygonIsRejected();
    MechanicalGripperTouchingProtectionBoundaryIsRejected();
    MechanicalGripperPartlyOutsideProtectionPolygonIsRejected();
    MechanicalGripperInvalidProtectionPolygonRejectsConservatively();
    OverlayPolygonUsesInnerRoiAndDefaultAxisMapping();
    OverlayPolygonUsesFrontBackMachineXMapping();
    OverlayPolygonRejectsEmptyInnerRoi();

    std::cout << "grab_limit_evaluator_test passed" << std::endl;
    return 0;
}
