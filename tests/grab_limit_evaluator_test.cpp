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
    OverlayPolygonUsesInnerRoiAndDefaultAxisMapping();
    OverlayPolygonUsesFrontBackMachineXMapping();
    OverlayPolygonRejectsEmptyInnerRoi();

    std::cout << "grab_limit_evaluator_test passed" << std::endl;
    return 0;
}
