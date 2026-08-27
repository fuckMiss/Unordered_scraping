#pragma once

#include "frame_result.h"

#include <QString>

class QLabel;
class QComboBox;
class QPushButton;

enum class RuntimeInputMode {
    Idle,
    Image,
    Camera
};

enum class RuntimeWorkflowState {
    Idle,
    Stopped,
    PlcPolling,
    PlcWriting
};

enum class DisplayOverlayMode {
    NormalHidden,
    AllDebug,
    None
};

struct DeviceStatusView
{
    QLabel* camera_dot = nullptr;
    QLabel* camera_text = nullptr;
    QLabel* model_dot = nullptr;
    QLabel* model_text = nullptr;
    QPushButton* open_camera_button = nullptr;
    QPushButton* start_button = nullptr;
    QPushButton* plc_link_button = nullptr;
    QPushButton* plc_test_button = nullptr;
    QPushButton* stop_button = nullptr;
    QComboBox* display_mode_selector = nullptr;
};

struct RuntimeStripView
{
    QLabel* mode = nullptr;
    QLabel* state = nullptr;
    QLabel* obb_count = nullptr;
    QLabel* seg_count = nullptr;
    QLabel* stage_time = nullptr;
    QLabel* total_time = nullptr;
};

struct RuntimeStatusSnapshot
{
    RuntimeInputMode input_mode = RuntimeInputMode::Idle;
    RuntimeWorkflowState workflow_state = RuntimeWorkflowState::Idle;
    FrameInferenceResult result;
    QString plc_runtime_status_text;
    bool camera_running = false;
    bool models_loaded = false;
    bool models_loading = false;
    bool image_detection_running = false;
    bool plc_link_active = false;
    bool plc_test_running = false;
    DisplayOverlayMode display_overlay_mode = DisplayOverlayMode::NormalHidden;
    bool has_input = false;
    bool has_current_frame = false;
};

class RuntimeStatusPresenter
{
public:
    static void RefreshDeviceStatus(const DeviceStatusView& view, const RuntimeStatusSnapshot& snapshot);
    static void RefreshRuntimeStrip(const RuntimeStripView& view, const RuntimeStatusSnapshot& snapshot);
    static QString PlcWaitingStatusText();
    static QString PlcProcessingStatusText();
    static QString PlcWaitingForFrameStatusText();
    static QString PlcFailedStatusText(const QString& error_message);
    static QString PlcRejectedStatusText(const QString& reject_reason,
                                         int pick_status_register,
                                         int pick_status_code,
                                         double total_ms);
    static QString PlcMissingFrameCompletedStatusText(int pick_status_register);
    static QString PlcFrameCompletedStatusText(const FrameInferenceResult& result,
                                               int pick_status_register,
                                               double total_ms);
};
