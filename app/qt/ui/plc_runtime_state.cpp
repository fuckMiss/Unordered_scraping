#include "plc_runtime_state.h"

bool PlcRuntimeState::active() const
{
    return active_;
}

bool PlcRuntimeState::pollBusy() const
{
    return poll_busy_;
}

bool PlcRuntimeState::waitingForFrame() const
{
    return waiting_for_frame_;
}

RuntimeWorkflowState PlcRuntimeState::workflowState() const
{
    return workflow_state_;
}

QString PlcRuntimeState::statusText() const
{
    return status_text_;
}

void PlcRuntimeState::stop(RuntimeWorkflowState next_state)
{
    active_ = false;
    poll_busy_ = false;
    waiting_for_frame_ = false;
    status_text_.clear();
    workflow_state_ = next_state;
}

void PlcRuntimeState::startPolling()
{
    active_ = true;
    poll_busy_ = false;
    waiting_for_frame_ = false;
    status_text_ = RuntimeStatusPresenter::PlcWaitingStatusText();
    workflow_state_ = RuntimeWorkflowState::PlcPolling;
}

void PlcRuntimeState::restoreIdleOrPolling()
{
    workflow_state_ = active_ ? RuntimeWorkflowState::PlcPolling : RuntimeWorkflowState::Idle;
}

void PlcRuntimeState::setPollBusy(bool busy)
{
    poll_busy_ = busy;
}

void PlcRuntimeState::beginWriting()
{
    waiting_for_frame_ = false;
    status_text_ = RuntimeStatusPresenter::PlcProcessingStatusText();
    workflow_state_ = RuntimeWorkflowState::PlcWriting;
}

void PlcRuntimeState::beginWaitingForFrame()
{
    waiting_for_frame_ = true;
    status_text_ = RuntimeStatusPresenter::PlcWaitingForFrameStatusText();
    workflow_state_ = RuntimeWorkflowState::PlcWriting;
}

void PlcRuntimeState::finishWaitingForFrame()
{
    waiting_for_frame_ = false;
}

void PlcRuntimeState::fail(const QString& error_message)
{
    restoreIdleOrPolling();
    status_text_ = RuntimeStatusPresenter::PlcFailedStatusText(error_message);
}

void PlcRuntimeState::reject(const QString& reject_reason,
                             int pick_status_register,
                             int pick_status_code,
                             double total_ms)
{
    restoreIdleOrPolling();
    status_text_ = RuntimeStatusPresenter::PlcRejectedStatusText(reject_reason,
                                                                 pick_status_register,
                                                                 pick_status_code,
                                                                 total_ms);
}

void PlcRuntimeState::completeMissingFrame(int pick_status_register)
{
    restoreIdleOrPolling();
    status_text_ = RuntimeStatusPresenter::PlcMissingFrameCompletedStatusText(pick_status_register);
}

void PlcRuntimeState::completeFrame(const FrameInferenceResult& result,
                                    int pick_status_register,
                                    double total_ms)
{
    restoreIdleOrPolling();
    status_text_ = RuntimeStatusPresenter::PlcFrameCompletedStatusText(result,
                                                                       pick_status_register,
                                                                       total_ms);
}
