#include "runtime_status_presenter.h"

#include <QApplication>
#include <QComboBox>
#include <QPushButton>

#include <cassert>
#include <iostream>

namespace {

FrameInferenceResult MakeResult(int detection_count)
{
    FrameInferenceResult result;
    result.pick_status_code = detection_count > 0 ? 1 : 3;
    for (int i = 0; i < detection_count; ++i) {
        PoseDetection detection;
        result.detections.push_back(detection);
    }
    result.segments.resize(2);
    return result;
}

void PlcStatusTextIsCentralized()
{
    assert(RuntimeStatusPresenter::PlcWaitingStatusText() == QStringLiteral("等待PLC触发"));
    assert(RuntimeStatusPresenter::PlcProcessingStatusText() == QStringLiteral("PLC处理中"));
    assert(RuntimeStatusPresenter::PlcWaitingForFrameStatusText() == QStringLiteral("PLC处理中：等待相机帧"));
    assert(RuntimeStatusPresenter::PlcFailedStatusText(QStringLiteral("timeout")) == QStringLiteral("PLC失败：timeout"));
    assert(RuntimeStatusPresenter::PlcRejectedStatusText(QStringLiteral("angle out"),
                                                         506,
                                                         3,
                                                         12.34)
               .contains(QStringLiteral("原因：angle out")));
    assert(RuntimeStatusPresenter::PlcMissingFrameCompletedStatusText(506) == QStringLiteral("PLC完成：无相机帧，D506=3"));
}

void PlcCompletionTextKeepsResultContractVisible()
{
    const QString no_target =
        RuntimeStatusPresenter::PlcFrameCompletedStatusText(MakeResult(0), 506, 12.34);
    assert(no_target.contains(QStringLiteral("无有效目标")));
    assert(no_target.contains(QStringLiteral("D506=3")));

    const QString with_target =
        RuntimeStatusPresenter::PlcFrameCompletedStatusText(MakeResult(1), 506, 12.34);
    assert(with_target.contains(QStringLiteral("目标 1")));
    assert(with_target.contains(QStringLiteral("区域 2")));
    assert(with_target.contains(QStringLiteral("D506=1")));
}

void PlcLinkButtonUsesShortLabels()
{
    DeviceStatusView view;
    QPushButton button;
    view.plc_link_button = &button;

    RuntimeStatusSnapshot snapshot;
    snapshot.models_loaded = true;
    snapshot.plc_link_active = false;
    RuntimeStatusPresenter::RefreshDeviceStatus(view, snapshot);
    assert(button.text() == QStringLiteral("开始"));

    snapshot.plc_link_active = true;
    RuntimeStatusPresenter::RefreshDeviceStatus(view, snapshot);
    assert(button.text() == QStringLiteral("关闭"));
}

void DisplayModeSelectorUsesThreeOverlayIndices()
{
    DeviceStatusView view;
    QComboBox selector;
    selector.addItem(QStringLiteral("隐藏"));
    selector.addItem(QStringLiteral("全显"));
    selector.addItem(QStringLiteral("无显示"));
    view.display_mode_selector = &selector;

    RuntimeStatusSnapshot snapshot;
    RuntimeStatusPresenter::RefreshDeviceStatus(view, snapshot);
    assert(selector.currentIndex() == 0);

    snapshot.display_overlay_mode = DisplayOverlayMode::AllDebug;
    RuntimeStatusPresenter::RefreshDeviceStatus(view, snapshot);
    assert(selector.currentIndex() == 1);

    snapshot.display_overlay_mode = DisplayOverlayMode::None;
    RuntimeStatusPresenter::RefreshDeviceStatus(view, snapshot);
    assert(selector.currentIndex() == 2);
}

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    PlcStatusTextIsCentralized();
    PlcCompletionTextKeepsResultContractVisible();
    PlcLinkButtonUsesShortLabels();
    DisplayModeSelectorUsesThreeOverlayIndices();

    std::cout << "runtime_status_presenter_test passed" << std::endl;
    return 0;
}
