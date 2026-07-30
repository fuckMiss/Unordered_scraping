#pragma once

#include "frame_result.h"
#include "runtime_status_presenter.h"

#include <QString>

class PlcRuntimeState
{
public:
    bool active() const;
    bool pollBusy() const;
    bool waitingForFrame() const;
    RuntimeWorkflowState workflowState() const;
    QString statusText() const;

    void stop(RuntimeWorkflowState next_state);
    void startPolling();
    void restoreIdleOrPolling();
    void setPollBusy(bool busy);
    void beginWriting();
    void beginWaitingForFrame();
    void finishWaitingForFrame();
    void fail(const QString& error_message);
    void reject(const QString& reject_reason,
                int pick_status_register,
                int pick_status_code,
                double total_ms);
    void completeMissingFrame(int pick_status_register);
    void completeFrame(const FrameInferenceResult& result,
                       int pick_status_register,
                       double total_ms);

private:
    bool active_ = false;
    bool poll_busy_ = false;
    bool waiting_for_frame_ = false;
    RuntimeWorkflowState workflow_state_ = RuntimeWorkflowState::Idle;
    QString status_text_;
};
