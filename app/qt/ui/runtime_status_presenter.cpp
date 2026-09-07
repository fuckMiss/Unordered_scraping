#include "runtime_status_presenter.h"

#include <QComboBox>
#include <QLabel>
#include <QPushButton>

namespace {

void SetIndicator(QLabel* dot,
                  QLabel* text,
                  bool active,
                  const QString& active_text,
                  const QString& inactive_text,
                  bool compact_mode)
{
    if (dot != nullptr) {
        const int dot_size = compact_mode ? 16 : 22;
        dot->setFixedSize(dot_size, dot_size);
        dot->setStyleSheet(QStringLiteral("border-radius: %1px; background:%2;")
                               .arg(dot_size / 2)
                               .arg(active ? QStringLiteral("#86d779") : QStringLiteral("#d46a6a")));
    }
    if (text != nullptr) {
        text->setText(active ? active_text : inactive_text);
    }
}

QString InputModeText(RuntimeInputMode mode)
{
    if (mode == RuntimeInputMode::Camera) {
        return QStringLiteral("采集");
    }
    if (mode == RuntimeInputMode::Image) {
        return QStringLiteral("图片");
    }
    return QStringLiteral("待机");
}

QString RuntimeStateText(const RuntimeStatusSnapshot& snapshot)
{
    if (snapshot.workflow_state == RuntimeWorkflowState::Stopped) {
        return QStringLiteral("检测已停止");
    }
    if (snapshot.workflow_state == RuntimeWorkflowState::PlcPolling ||
        snapshot.workflow_state == RuntimeWorkflowState::PlcWriting ||
        snapshot.plc_link_active) {
        return snapshot.plc_runtime_status_text.isEmpty()
            ? QStringLiteral("等待PLC触发")
            : snapshot.plc_runtime_status_text;
    }
    if (!snapshot.models_loaded) {
        return QStringLiteral("模型未就绪");
    }
    if (snapshot.camera_running) {
        return snapshot.plc_link_active ? QStringLiteral("等待PLC触发") : QStringLiteral("相机采集中");
    }
    if (snapshot.has_current_frame && snapshot.result.total_inference_ms > 0.0) {
        return snapshot.result.detections.empty() && snapshot.result.segments.empty()
            ? QStringLiteral("无结果")
            : QStringLiteral("检测完成");
    }
    if (snapshot.has_current_frame) {
        return QStringLiteral("等待执行");
    }
    if (snapshot.input_mode == RuntimeInputMode::Camera) {
        return QStringLiteral("等待相机");
    }
    return QStringLiteral("等待图像输入");
}

void SetRuntimeBusy(const RuntimeStripView& view, const QString& state, const QString& tooltip)
{
    if (view.state != nullptr) {
        view.state->setText(QStringLiteral("状态：%1").arg(state));
        view.state->setToolTip(tooltip);
    }
    if (view.obb_count != nullptr) {
        view.obb_count->setText(QStringLiteral("目标：-"));
    }
    if (view.seg_count != nullptr) {
        view.seg_count->setText(QStringLiteral("区域：-"));
    }
    if (view.stage_time != nullptr) {
        view.stage_time->setText(QStringLiteral("推理耗时：-"));
    }
    if (view.total_time != nullptr) {
        view.total_time->setText(QStringLiteral("总耗时：-"));
    }
}

} // namespace

void RuntimeStatusPresenter::RefreshDeviceStatus(const DeviceStatusView& view,
                                                 const RuntimeStatusSnapshot& snapshot)
{
    if (view.display_mode_selector != nullptr) {
        int display_mode_index = 0;
        if (snapshot.display_overlay_mode == DisplayOverlayMode::AllDebug) {
            display_mode_index = 1;
        } else if (snapshot.display_overlay_mode == DisplayOverlayMode::None) {
            display_mode_index = 2;
        }
        view.display_mode_selector->setCurrentIndex(display_mode_index);
    }

    if (snapshot.models_loading) {
        SetIndicator(view.camera_dot, view.camera_text, snapshot.camera_running,
                     QStringLiteral("在线"), QStringLiteral("待机"), view.compact_status_mode);
        SetIndicator(view.model_dot, view.model_text, false,
                     QStringLiteral("加载中"), QStringLiteral("加载中"), view.compact_status_mode);
        if (view.start_button != nullptr) {
            view.start_button->setEnabled(false);
        }
        if (view.plc_link_button != nullptr) {
            view.plc_link_button->setText(snapshot.plc_link_active ? QStringLiteral("关闭")
                                                                   : QStringLiteral("开始"));
            view.plc_link_button->setEnabled(false);
        }
        if (view.plc_test_button != nullptr) {
            view.plc_test_button->setEnabled(!snapshot.plc_test_running);
        }
        if (view.stop_button != nullptr) {
            view.stop_button->setEnabled(snapshot.camera_running && !snapshot.plc_link_active);
        }
        return;
    }

    if (snapshot.image_detection_running) {
        SetIndicator(view.camera_dot, view.camera_text, snapshot.camera_running,
                     QStringLiteral("在线"), QStringLiteral("待机"), view.compact_status_mode);
        SetIndicator(view.model_dot, view.model_text, true,
                     QStringLiteral("检测中"), QStringLiteral("检测中"), view.compact_status_mode);
        if (view.start_button != nullptr) {
            view.start_button->setEnabled(false);
        }
        if (view.plc_link_button != nullptr) {
            view.plc_link_button->setText(snapshot.plc_link_active ? QStringLiteral("关闭")
                                                                   : QStringLiteral("开始"));
            view.plc_link_button->setEnabled(false);
        }
        if (view.plc_test_button != nullptr) {
            view.plc_test_button->setEnabled(!snapshot.plc_test_running);
        }
        if (view.stop_button != nullptr) {
            view.stop_button->setEnabled(false);
        }
        return;
    }

    SetIndicator(view.camera_dot, view.camera_text, snapshot.camera_running,
                 QStringLiteral("在线"), QStringLiteral("待机"), view.compact_status_mode);
    SetIndicator(view.model_dot, view.model_text, snapshot.models_loaded,
                 QStringLiteral("已加载"), QStringLiteral("加载失败"), view.compact_status_mode);
    if (view.open_camera_button != nullptr) {
        view.open_camera_button->setEnabled(!snapshot.plc_link_active);
    }
    if (view.start_button != nullptr) {
        view.start_button->setEnabled(snapshot.models_loaded && snapshot.has_input);
    }
    if (view.plc_link_button != nullptr) {
        view.plc_link_button->setText(snapshot.plc_link_active ? QStringLiteral("关闭")
                                                               : QStringLiteral("开始"));
        view.plc_link_button->setEnabled(snapshot.models_loaded);
    }
    if (view.plc_test_button != nullptr) {
        view.plc_test_button->setEnabled(!snapshot.plc_test_running);
    }
    if (view.stop_button != nullptr) {
        view.stop_button->setEnabled(snapshot.camera_running && !snapshot.plc_link_active);
    }
}

void RuntimeStatusPresenter::RefreshRuntimeStrip(const RuntimeStripView& view,
                                                 const RuntimeStatusSnapshot& snapshot)
{
    if (view.mode != nullptr) {
        view.mode->setText(QStringLiteral("模式：%1").arg(InputModeText(snapshot.input_mode)));
    }

    if (snapshot.models_loading) {
        SetRuntimeBusy(view, QStringLiteral("模型加载中"), QStringLiteral("OpenVINO 正在后台加载模型"));
        return;
    }
    if (snapshot.image_detection_running) {
        SetRuntimeBusy(view, QStringLiteral("图片检测中"), QStringLiteral("OpenVINO 正在后台执行图片检测"));
        return;
    }

    const QString state = RuntimeStateText(snapshot);
    if (view.state != nullptr) {
        view.state->setText(QStringLiteral("状态：%1").arg(state));
        view.state->setToolTip(QStringLiteral("视觉系统运行状态：%1").arg(state));
    }
    if (view.obb_count != nullptr) {
        view.obb_count->setText(QStringLiteral("目标：%1").arg(snapshot.result.detections.size()));
    }
    if (view.seg_count != nullptr) {
        view.seg_count->setText(QStringLiteral("区域：%1").arg(snapshot.result.segments.size()));
    }
    if (view.stage_time != nullptr) {
        view.stage_time->setText(
            snapshot.result.total_inference_ms > 0.0
                ? QStringLiteral("推理耗时：%1 ms")
                      .arg(QString::number(snapshot.result.obb_inference_ms + snapshot.result.seg_inference_ms, 'f', 1))
                : QStringLiteral("推理耗时：-"));
    }
    if (view.total_time != nullptr) {
        view.total_time->setText(
            snapshot.result.total_inference_ms > 0.0
                ? QStringLiteral("总耗时：%1 ms")
                      .arg(QString::number(snapshot.result.total_inference_ms, 'f', 1))
                : QStringLiteral("总耗时：-"));
    }
}

QString RuntimeStatusPresenter::PlcWaitingStatusText()
{
    return QStringLiteral("等待PLC触发");
}

QString RuntimeStatusPresenter::PlcProcessingStatusText()
{
    return QStringLiteral("PLC处理中");
}

QString RuntimeStatusPresenter::PlcWaitingForFrameStatusText()
{
    return QStringLiteral("PLC处理中：等待相机帧");
}

QString RuntimeStatusPresenter::PlcFailedStatusText(const QString& error_message)
{
    return QStringLiteral("PLC失败：%1").arg(error_message);
}

QString RuntimeStatusPresenter::PlcRejectedStatusText(const QString& reject_reason,
                                                      int pick_status_register,
                                                      int pick_status_code,
                                                      double total_ms)
{
    return QStringLiteral("PLC完成：拒绝，D%1=%2，原因：%3，用时 %4 ms")
        .arg(pick_status_register)
        .arg(pick_status_code)
        .arg(reject_reason.isEmpty() ? QStringLiteral("未记录") : reject_reason)
        .arg(QString::number(total_ms, 'f', 1));
}

QString RuntimeStatusPresenter::PlcMissingFrameCompletedStatusText(int pick_status_register)
{
    return QStringLiteral("PLC完成：无相机帧，D%1=3").arg(pick_status_register);
}

QString RuntimeStatusPresenter::PlcFrameCompletedStatusText(const FrameInferenceResult& result,
                                                            int pick_status_register,
                                                            double total_ms)
{
    if (result.detections.empty()) {
        return QStringLiteral("PLC完成：无有效目标，D%1=%2，用时 %3 ms")
            .arg(pick_status_register)
            .arg(result.pick_status_code)
            .arg(QString::number(total_ms, 'f', 1));
    }

    return QStringLiteral("PLC完成：目标 %1，区域 %2，D%3=%4，用时 %5 ms")
        .arg(result.detections.size())
        .arg(result.segments.size())
        .arg(pick_status_register)
        .arg(result.pick_status_code)
        .arg(QString::number(total_ms, 'f', 1));
}
