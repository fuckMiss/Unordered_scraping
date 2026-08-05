#include "grab_limit_evaluator.h"

#include <cassert>
#include <cmath>
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

void AxisCompensationDoesNotAffectLimitDecision()
{
    PlcOutputConfig plc_config;
    plc_config.front_back_offset = 10000.0f;
    plc_config.left_right_offset = -10000.0f;
    FrameInferenceResult result = MakeResult(120.0f, 55.0f);

    const GrabLimitDecision decision = EvaluateGrabLimits(result, MakeLimits(), plc_config);
    assert(!decision.rejected);
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

    ApplyMechanicalGripperCollisionFilter(result, IdentityCoordinateState(), config);

    assert(result.detections.size() == 1);
    assert(!result.detections.front().can_grab);
    assert(result.detections.front().pick_status_code == 3);
    assert(result.detections.front().mechanical_gripper_corners.size() == 4);
    assert(result.detections.front().mask_collisions.size() == 1);
    assert(result.primary_index == -1);
    assert(result.pick_status_code == 3);
}

void MechanicalGripperInvalidSizeRejectsConservatively()
{
    FrameInferenceResult result = MakeMechanicalGripperCollisionResult();
    const MechanicalGripperCollisionConfig config{ 0.0, 20.0, 10.0, false };

    ApplyMechanicalGripperCollisionFilter(result, IdentityCoordinateState(), config);

    assert(!result.detections.front().can_grab);
    assert(result.detections.front().mechanical_gripper_corners.empty());
    assert(result.primary_index == -1);
    assert(result.pick_status_code == 3);
}

void MechanicalGripperInvalidCoordinateRejectsConservatively()
{
    FrameInferenceResult result = MakeMechanicalGripperCollisionResult();
    CoordinateTransformState state;
    state.enabled = true;
    state.valid = false;
    const MechanicalGripperCollisionConfig config{ 40.0, 20.0, 10.0, false };

    ApplyMechanicalGripperCollisionFilter(result, state, config);

    assert(!result.detections.front().can_grab);
    assert(result.detections.front().mechanical_gripper_corners.empty());
    assert(result.primary_index == -1);
    assert(result.pick_status_code == 3);
}

void MechanicalGripperFinalizesPoseFromRealFrameBoundary()
{
    FrameInferenceResult result = MakeMechanicalGripperCollisionResult();
    result.segments[1].mask.setTo(cv::Scalar(0));
    const MechanicalGripperCollisionConfig config{ 40.0, 20.0, 10.0, false };

    ApplyMechanicalGripperCollisionFilter(result, IdentityCoordinateState(), config);

    assert(result.detections.front().can_grab);
    assert(NearlyEqual(result.detections.front().arrow_start.x, 50.0f));
    assert(NearlyEqual(result.detections.front().arrow_start.y, 50.0f));
    assert(NearlyEqual(result.detections.front().arrow_end.x, 60.0f));
    assert(NearlyEqual(result.detections.front().arrow_end.y, 50.0f));
    assert(NearlyEqual(result.detections.front().angle_deg, 0.0f));
    assert(NearlyEqual(result.detections.front().center_x, 60.0f));
    assert(NearlyEqual(result.detections.front().center_y, 50.0f));
    const std::vector<cv::Point2f>& corners = result.detections.front().mechanical_gripper_corners;
    assert(corners.size() == 4);
    assert(NearlyEqual(std::hypot(corners[1].x - corners[0].x, corners[1].y - corners[0].y), 40.0f));
    assert(NearlyEqual(std::hypot(corners[3].x - corners[0].x, corners[3].y - corners[0].y), 20.0f));
    assert(NearlyEqual(corners[0].x, 60.0f));
    assert(NearlyEqual(corners[1].x, 60.0f));
    assert(NearlyEqual(corners[0].y, 30.0f));
    assert(NearlyEqual(corners[1].y, 70.0f));
    assert(result.primary_index == 0);
    assert(result.pick_status_code == 1);
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
    AxisCompensationDoesNotAffectLimitDecision();
    AngleLimitsUseCalibratedPlcAngle();
    MechanicalRoiRequiresValidCoordinateTransform();
    MechanicalRoiFiltersOutsideTargetsAndResolvesPrimary();
    MechanicalGripperCollisionRejectsNeighborMaskOutsideSelf();
    MechanicalGripperInvalidSizeRejectsConservatively();
    MechanicalGripperInvalidCoordinateRejectsConservatively();
    MechanicalGripperFinalizesPoseFromRealFrameBoundary();
    OverlayPolygonUsesInnerRoiAndDefaultAxisMapping();
    OverlayPolygonUsesFrontBackMachineXMapping();
    OverlayPolygonRejectsEmptyInnerRoi();

    std::cout << "grab_limit_evaluator_test passed" << std::endl;
    return 0;
}
