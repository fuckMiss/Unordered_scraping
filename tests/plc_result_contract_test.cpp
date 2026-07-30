#include "plc_result_contract.h"

#include <cassert>
#include <cmath>
#include <iostream>

namespace {

bool NearlyEqual(float left, float right)
{
    return std::fabs(left - right) < 0.001f;
}

FrameInferenceResult MakeResult()
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
    target.head_type_code = 4;
    result.detections.push_back(target);
    result.primary_index = 0;
    result.pick_status_code = 1;
    result.head_type_code = 4;
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

} // namespace

int main()
{
    DefaultMappingWritesMachineYToD500AndMachineXToD502();
    SwappedMappingWritesMachineXToD500AndMachineYToD502();
    AngleCalibrationSupportsSignedRangeAndReverseDirection();
    NoPrimaryTargetWritesOnlyStatusContract();
    ImageCoordinatesAreFallbackWhenMachineCoordinatesAreMissing();

    std::cout << "plc_result_contract_test passed" << std::endl;
    return 0;
}
