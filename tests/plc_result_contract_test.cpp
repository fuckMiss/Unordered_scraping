#include "plc_result_contract.h"

#include <cassert>
#include <cmath>
#include <iostream>

namespace {

bool NearlyEqual(float left, float right)
{
    return std::fabs(left - right) < 0.001f;
}

FrameInferenceResult MakeResult(int head_type_code = 4)
{
    FrameInferenceResult result;
    PoseDetection target;
    target.center_x = 12.0f;
    target.center_y = 34.0f;
    target.machine_x = 123.4f;
    target.machine_y = 56.7f;
    target.has_machine_coords = true;
    target.angle_deg = 270.0f;
    target.pick_status_code = 1;
    target.head_type_code = head_type_code;
    result.detections.push_back(target);
    result.primary_index = 0;
    result.pick_status_code = 1;
    result.head_type_code = head_type_code;
    return result;
}

void DefaultMappingWritesMachineYToD500AndMachineXToD502()
{
    const PlcWriteResult write = BuildPlcWriteResult(MakeResult(), {});
    assert(NearlyEqual(write.x, 56.7f));
    assert(NearlyEqual(write.y, 123.4f));
    assert(NearlyEqual(write.angle, 270.0f));
    assert(NearlyEqual(write.pick_status, 1.0f));
    assert(NearlyEqual(write.head_type, 4.0f));
    assert(write.has_machine_coords);
}

void SwappedMappingWritesMachineXToD500AndMachineYToD502()
{
    PlcOutputConfig config;
    config.axis_mapping_mode = AxisMappingMode::FrontBackMachineX;
    config.front_back_offset = 1.0f;
    config.left_right_offset = -2.0f;

    const PlcWriteResult write = BuildPlcWriteResult(MakeResult(), config);
    assert(NearlyEqual(write.x, 124.4f));
    assert(NearlyEqual(write.y, 54.7f));
}

void AngleCalibrationSupportsSignedRangeAndReverseDirection()
{
    PlcOutputConfig config;
    config.angle_offset_deg = 20.0f;
    config.angle_reverse_direction = true;
    config.angle_range_mode = AngleRangeMode::Signed180;

    const PlcWriteResult write = BuildPlcWriteResult(MakeResult(), config);
    assert(NearlyEqual(write.angle, 110.0f));
}

void AngleCalibrationUsesOneOffsetForBothRanges()
{
    PlcOutputConfig config;
    config.angle_offset_deg = 15.0f;
    config.angle_reverse_direction = false;
    config.angle_range_mode = AngleRangeMode::Signed180;
    assert(NearlyEqual(ApplyPlcAngleCalibration(350.0f, config), 5.0f));

    config.angle_range_mode = AngleRangeMode::ZeroTo360;
    assert(NearlyEqual(ApplyPlcAngleCalibration(350.0f, config), 5.0f));
}

void ReverseDirectionMultipliesRawAngleBeforeOffset()
{
    PlcOutputConfig config;
    config.angle_offset_deg = 15.0f;
    config.angle_range_mode = AngleRangeMode::ZeroTo360;
    config.angle_reverse_direction = false;
    assert(NearlyEqual(ApplyPlcAngleCalibration(30.0f, config), 45.0f));

    config.angle_reverse_direction = true;
    assert(NearlyEqual(ApplyPlcAngleCalibration(30.0f, config), 345.0f));

    config.angle_range_mode = AngleRangeMode::Signed180;
    config.angle_reverse_direction = false;
    assert(NearlyEqual(ApplyPlcAngleCalibration(30.0f, config), 45.0f));

    config.angle_reverse_direction = true;
    assert(NearlyEqual(ApplyPlcAngleCalibration(30.0f, config), -15.0f));
}

void ReverseDirectionAppliesAfterHeadTypeCompensation()
{
    PlcOutputConfig config;
    config.angle_offset_deg = 15.0f;
    config.angle_reverse_direction = true;
    config.angle_range_mode = AngleRangeMode::Signed180;

    HeadTypeCompensation type_compensation;
    type_compensation.angle_offset_deg = 5.0f;

    assert(NearlyEqual(ApplyPlcAngleCalibration(30.0f, config, type_compensation), -10.0f));
}

void NoPrimaryTargetWritesOnlyStatusContract()
{
    FrameInferenceResult result;
    result.pick_status_code = 3;
    result.head_type_code = 0;

    const PlcWriteResult write = BuildPlcWriteResult(result, {});
    assert(NearlyEqual(write.pick_status, 3.0f));
    assert(NearlyEqual(write.head_type, 0.0f));
    assert(!write.has_machine_coords);
}

void ImageCoordinatesAreFallbackWhenMachineCoordinatesAreMissing()
{
    FrameInferenceResult result = MakeResult();
    result.detections[0].has_machine_coords = false;

    const PlcWriteResult write = BuildPlcWriteResult(result, {});
    assert(NearlyEqual(write.x, 34.0f));
    assert(NearlyEqual(write.y, 12.0f));
    assert(!write.has_machine_coords);
}

void TypeCompensationAppliesForAllFourHeadTypes()
{
    const int head_types[] = { 1, 3, 4, 2 };
    for (const int head_type : head_types) {
        FrameInferenceResult result = MakeResult(head_type);
        PoseDetection& target = result.detections[0];
        target.machine_x = 100.0f;
        target.machine_y = 200.0f;
        target.angle_deg = 10.0f;
        target.ac_unit_machine_x = 0.6f;
        target.ac_unit_machine_y = 0.8f;
        target.has_machine_ac_unit = true;

        PlcOutputConfig config;
        config.angle_offset_deg = 5.0f;
        config.front_back_offset = 1.0f;
        config.left_right_offset = 2.0f;
        config.head_type_compensations[head_type].angle_offset_deg = static_cast<float>(head_type);
        config.head_type_compensations[head_type].ac_ray_offset_mm = 10.0f;

        const PlcWriteResult write = BuildPlcWriteResult(result, config);
        assert(NearlyEqual(write.x, 209.0f));
        assert(NearlyEqual(write.y, 108.0f));
        assert(NearlyEqual(write.angle, 15.0f + static_cast<float>(head_type)));
        assert(NearlyEqual(write.pick_status, 1.0f));
        assert(NearlyEqual(write.head_type, static_cast<float>(head_type)));
        assert(!write.type_compensation_failed);
    }
}

void TypeRayOffsetRequiresMachineAcDirection()
{
    FrameInferenceResult result = MakeResult(4);
    result.detections[0].has_machine_ac_unit = false;

    PlcOutputConfig config;
    config.head_type_compensations[4].ac_ray_offset_mm = 5.0f;

    const PlcWriteResult write = BuildPlcWriteResult(result, config);
    assert(NearlyEqual(write.pick_status, 3.0f));
    assert(write.type_compensation_failed);
}

void DiagnosticWriteSupportsTypeCompensation()
{
    PlcOutputConfig config;
    config.head_type_compensations[4].angle_offset_deg = 2.0f;
    config.head_type_compensations[4].ac_ray_offset_mm = 5.0f;

    const PlcWriteResult write = BuildDiagnosticPlcWriteResult(config);
    assert(NearlyEqual(write.x, 56.7f));
    assert(NearlyEqual(write.y, 128.4f));
    assert(NearlyEqual(write.angle, 92.0f));
    assert(NearlyEqual(write.pick_status, 1.0f));
    assert(!write.type_compensation_failed);
}

} // namespace

int main()
{
    DefaultMappingWritesMachineYToD500AndMachineXToD502();
    SwappedMappingWritesMachineXToD500AndMachineYToD502();
    AngleCalibrationSupportsSignedRangeAndReverseDirection();
    AngleCalibrationUsesOneOffsetForBothRanges();
    ReverseDirectionMultipliesRawAngleBeforeOffset();
    ReverseDirectionAppliesAfterHeadTypeCompensation();
    NoPrimaryTargetWritesOnlyStatusContract();
    ImageCoordinatesAreFallbackWhenMachineCoordinatesAreMissing();
    TypeCompensationAppliesForAllFourHeadTypes();
    TypeRayOffsetRequiresMachineAcDirection();
    DiagnosticWriteSupportsTypeCompensation();

    std::cout << "plc_result_contract_test passed" << std::endl;
    return 0;
}
