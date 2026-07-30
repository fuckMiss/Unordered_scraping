#include "robot_controller.h"

#include <cassert>
#include <iostream>
#include <utility>
#include <vector>

namespace {

FrameInferenceResult MakeResult()
{
    FrameInferenceResult result;
    PoseDetection target;
    target.machine_x = 12.0f;
    target.machine_y = 34.0f;
    target.has_machine_coords = true;
    target.angle_deg = 56.0f;
    target.pick_status_code = 1;
    target.head_type_code = 4;
    result.detections.push_back(target);
    result.primary_index = 0;
    result.pick_status_code = 1;
    result.head_type_code = 4;
    return result;
}

FrameInferenceResult MakeRejectResult()
{
    FrameInferenceResult result = MakeResult();
    result.primary_index = -1;
    result.pick_status_code = 3;
    result.head_type_code = 0;
    return result;
}

PlcRegisterMap CustomRegisterMap()
{
    PlcRegisterMap registers;
    registers.photo_trigger = 1600;
    registers.front_back = 600;
    registers.left_right = 602;
    registers.angle = 604;
    registers.pick_status = 606;
    registers.head_type = 608;
    return registers;
}

std::vector<int> Addresses(const std::vector<std::pair<int, float>>& writes)
{
    std::vector<int> addresses;
    for (const auto& write : writes) {
        addresses.push_back(write.first);
    }
    return addresses;
}

void CustomRegisterMapIsUsedForFrameResult()
{
    const PlcRegisterMap registers = CustomRegisterMap();
    const auto writes = BuildPlcFrameResultRegisterWrites(MakeResult(), registers, {});

    assert((Addresses(writes) == std::vector<int>{ 600, 602, 604, 606, 608 }));
    assert(writes.at(0).second == 34.0f);
    assert(writes.at(1).second == 12.0f);
    assert(writes.at(2).second == 56.0f);
    assert(writes.at(3).second == 1.0f);
    assert(writes.at(4).second == 4.0f);
}

void RejectWritesOnlyConfiguredStatusRegister()
{
    PlcRegisterMap registers = CustomRegisterMap();
    registers.pick_status = 1666;
    const auto writes = BuildPlcRejectStatusRegisterWrite(registers);

    assert(writes.size() == 1);
    assert(writes.at(0).first == 1666);
    assert(writes.at(0).second == 3.0f);
}

void RejectedFrameResultWritesOnlyConfiguredStatusRegister()
{
    PlcRegisterMap registers;
    registers.photo_trigger = 1700;
    registers.front_back = 700;
    registers.left_right = 702;
    registers.angle = 704;
    registers.pick_status = 1706;
    registers.head_type = 708;
    const auto writes = BuildPlcFrameResultRegisterWrites(MakeRejectResult(), registers, {});

    assert(writes.size() == 1);
    assert(writes.at(0).first == 1706);
    assert(writes.at(0).second == 3.0f);
}

} // namespace

int main()
{
    CustomRegisterMapIsUsedForFrameResult();
    RejectWritesOnlyConfiguredStatusRegister();
    RejectedFrameResultWritesOnlyConfiguredStatusRegister();
    std::cout << "robot_controller_register_map_test passed" << std::endl;
    return 0;
}
