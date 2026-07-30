#include "plc_runtime_state.h"

#include <cassert>
#include <iostream>

namespace {

FrameInferenceResult MakeResult()
{
    FrameInferenceResult result;
    result.pick_status_code = 1;
    result.detections.push_back(PoseDetection{});
    return result;
}

void StateTransitionsKeepPlcWorkflowInOnePlace()
{
    PlcRuntimeState state;
    assert(!state.active());
    assert(state.workflowState() == RuntimeWorkflowState::Idle);

    state.startPolling();
    assert(state.active());
    assert(!state.pollBusy());
    assert(!state.waitingForFrame());
    assert(state.workflowState() == RuntimeWorkflowState::PlcPolling);
    assert(state.statusText() == QStringLiteral("等待PLC触发"));

    state.setPollBusy(true);
    assert(state.pollBusy());

    state.beginWriting();
    assert(state.workflowState() == RuntimeWorkflowState::PlcWriting);
    assert(state.statusText() == QStringLiteral("PLC处理中"));

    state.beginWaitingForFrame();
    assert(state.waitingForFrame());
    assert(state.statusText() == QStringLiteral("PLC处理中：等待相机帧"));

    state.finishWaitingForFrame();
    assert(!state.waitingForFrame());

    state.completeFrame(MakeResult(), 506, 12.0);
    assert(state.workflowState() == RuntimeWorkflowState::PlcPolling);
    assert(state.statusText().contains(QStringLiteral("D506=1")));

    state.reject(QStringLiteral("limit out"), 506, 3, 13.0);
    assert(state.workflowState() == RuntimeWorkflowState::PlcPolling);
    assert(state.statusText().contains(QStringLiteral("拒绝")));
    assert(state.statusText().contains(QStringLiteral("limit out")));

    state.stop(RuntimeWorkflowState::Stopped);
    assert(!state.active());
    assert(!state.pollBusy());
    assert(!state.waitingForFrame());
    assert(state.workflowState() == RuntimeWorkflowState::Stopped);
    assert(state.statusText().isEmpty());
}

} // namespace

int main()
{
    StateTransitionsKeepPlcWorkflowInOnePlace();

    std::cout << "plc_runtime_state_test passed" << std::endl;
    return 0;
}
