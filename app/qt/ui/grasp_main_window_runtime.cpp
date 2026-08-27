#include "grasp_main_window.h"

#include "frame_overlay.h"
#include "frame_processing_service.h"
#include "grasp_main_window_common.h"
#include "grasp_main_window_scale.h"
#include "plc_trigger_coordinator.h"
#include "runtime_status_presenter.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QMessageBox>
#include <QMetaObject>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QStatusBar>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace cv;
using namespace std;

namespace {

QString DisplayOverlayModeText(DisplayOverlayMode mode)
{
    return GraspMainWindowDisplayOverlayModeText(mode);
}

QString DisplayOverlayModeFileSuffix(DisplayOverlayMode mode)
{
    return GraspMainWindowDisplayOverlayModeFileSuffix(mode);
}

bool IsAllDebugMode(DisplayOverlayMode mode)
{
    return GraspMainWindowIsAllDebugMode(mode);
}

int S(int value)
{
    return GraspS(value);
}

QMargins SM(int left, int top, int right, int bottom)
{
    return GraspSM(left, top, right, bottom);
}

void SetValueText(QLabel* label, const QString& text)
{
    if (!label) {
        return;
    }
    label->setText(text);
    label->setToolTip(text);
}

QImage MatToQImage(const Mat& image)
{
    if (image.empty()) {
        return QImage();
    }

    if (image.type() == CV_8UC1) {
        return QImage(image.data,
                      image.cols,
                      image.rows,
                      static_cast<int>(image.step),
                      QImage::Format_Grayscale8).copy();
    }

    Mat rgb;
    cvtColor(image, rgb, COLOR_BGR2RGB);
    return QImage(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGB888).copy();
}

QSize FitImageSize(int source_width, int source_height, const QSize& bounds)
{
    if (source_width <= 0 || source_height <= 0 || bounds.width() <= 0 || bounds.height() <= 0) {
        return QSize(source_width, source_height);
    }

    const double scale = qMin(bounds.width() / static_cast<double>(source_width),
                              bounds.height() / static_cast<double>(source_height));
    return QSize(qMax(1, qRound(source_width * scale)),
                 qMax(1, qRound(source_height * scale)));
}

Point2f ScalePoint(const Point2f& point, double scale_x, double scale_y, int offset_x, int offset_y)
{
    return Point2f(static_cast<float>(point.x * scale_x - offset_x),
                   static_cast<float>(point.y * scale_y - offset_y));
}

Rect ScaleRect(const Rect& rect, double scale_x, double scale_y, int offset_x, int offset_y)
{
    return Rect(qRound(rect.x * scale_x) - offset_x,
                qRound(rect.y * scale_y) - offset_y,
                qRound(rect.width * scale_x),
                qRound(rect.height * scale_y));
}

void ScalePoints(vector<Point2f>& points, double scale_x, double scale_y, int offset_x, int offset_y)
{
    for (Point2f& point : points) {
        point = ScalePoint(point, scale_x, scale_y, offset_x, offset_y);
    }
}

vector<pair<Rect, Mat>> ScaleMaskCollisionOverlays(const vector<pair<Rect, Mat>>& collisions,
                                                   double scale_x,
                                                   double scale_y,
                                                   const Size& display_size,
                                                   int offset_x,
                                                   int offset_y)
{
    vector<pair<Rect, Mat>> scaled_collisions;
    const Rect display_rect(0, 0, display_size.width, display_size.height);
    for (const auto& collision : collisions) {
        const Rect scaled_roi = ScaleRect(collision.first, scale_x, scale_y, offset_x, offset_y);
        if (scaled_roi.empty() || collision.second.empty()) {
            continue;
        }

        Mat scaled_mask;
        resize(collision.second,
               scaled_mask,
               Size(max(1, scaled_roi.width), max(1, scaled_roi.height)),
               0.0,
               0.0,
               INTER_NEAREST);

        const Rect clipped_roi = scaled_roi & display_rect;
        if (clipped_roi.empty()) {
            continue;
        }

        const Rect local_crop(clipped_roi.x - scaled_roi.x,
                              clipped_roi.y - scaled_roi.y,
                              clipped_roi.width,
                              clipped_roi.height);
        scaled_collisions.push_back({ clipped_roi, scaled_mask(local_crop).clone() });
    }
    return scaled_collisions;
}

FrameInferenceResult ScaleResultForDisplay(const FrameInferenceResult& result,
                                           double scale_x,
                                           double scale_y,
                                           const Size& scaled_size,
                                           const Size& display_size,
                                           int offset_x,
                                           int offset_y,
                                           bool include_segment_masks)
{
    FrameInferenceResult scaled = result;
    scaled.image_width = display_size.width;
    scaled.image_height = display_size.height;
    scaled.display_scale = qMin(scale_x, scale_y);

    for (PoseDetection& detection : scaled.detections) {
        detection.center_x = static_cast<float>(detection.center_x * scale_x - offset_x);
        detection.center_y = static_cast<float>(detection.center_y * scale_y - offset_y);
        detection.obb_center = ScalePoint(detection.obb_center, scale_x, scale_y, offset_x, offset_y);
        detection.seg_center = ScalePoint(detection.seg_center, scale_x, scale_y, offset_x, offset_y);
        detection.x_point = ScalePoint(detection.x_point, scale_x, scale_y, offset_x, offset_y);
        detection.small_point = ScalePoint(detection.small_point, scale_x, scale_y, offset_x, offset_y);
        detection.small_arrow_start = ScalePoint(detection.small_arrow_start, scale_x, scale_y, offset_x, offset_y);
        detection.small_arrow_end = ScalePoint(detection.small_arrow_end, scale_x, scale_y, offset_x, offset_y);
        detection.arrow_start = ScalePoint(detection.arrow_start, scale_x, scale_y, offset_x, offset_y);
        detection.arrow_end = ScalePoint(detection.arrow_end, scale_x, scale_y, offset_x, offset_y);
        detection.plc_command_center = ScalePoint(detection.plc_command_center, scale_x, scale_y, offset_x, offset_y);
        detection.plc_command_arrow_start = ScalePoint(detection.plc_command_arrow_start, scale_x, scale_y, offset_x, offset_y);
        detection.plc_command_arrow_end = ScalePoint(detection.plc_command_arrow_end, scale_x, scale_y, offset_x, offset_y);
        detection.bbox = ScaleRect(detection.bbox, scale_x, scale_y, offset_x, offset_y);
        ScalePoints(detection.corners, scale_x, scale_y, offset_x, offset_y);
        ScalePoints(detection.mechanical_gripper_corners, scale_x, scale_y, offset_x, offset_y);
        detection.mask_collisions = include_segment_masks
            ? ScaleMaskCollisionOverlays(detection.mask_collisions,
                                         scale_x,
                                         scale_y,
                                         display_size,
                                         offset_x,
                                         offset_y)
            : vector<pair<Rect, Mat>>{};
    }

    for (ObbRegion& region : scaled.raw_obb_regions) {
        region.center = ScalePoint(region.center, scale_x, scale_y, offset_x, offset_y);
        region.bbox = ScaleRect(region.bbox, scale_x, scale_y, offset_x, offset_y);
        ScalePoints(region.corners, scale_x, scale_y, offset_x, offset_y);
    }

    for (SegRegion& segment : scaled.segments) {
        segment.bbox = ScaleRect(segment.bbox, scale_x, scale_y, offset_x, offset_y);
        segment.center = ScalePoint(segment.center, scale_x, scale_y, offset_x, offset_y);
        ScalePoints(segment.contour, scale_x, scale_y, offset_x, offset_y);
        ScalePoints(segment.min_rect_corners, scale_x, scale_y, offset_x, offset_y);
        if (include_segment_masks && !segment.mask.empty()) {
            Mat resized_mask;
            resize(segment.mask, resized_mask, scaled_size, 0.0, 0.0, INTER_NEAREST);
            const Rect crop_rect(offset_x, offset_y, display_size.width, display_size.height);
            segment.mask = resized_mask(crop_rect).clone();
        } else {
            segment.mask.release();
        }
    }

    return scaled;
}

QString FormatNumber(float value)
{
    return QString::number(value, 'f', 1);
}

QString FormatMachineCoordinate(const PoseDetection& detection, bool x_axis)
{
    if (!detection.has_machine_coords) {
        return QStringLiteral("--");
    }
    return FormatNumber(x_axis ? detection.machine_x : detection.machine_y);
}

QString FormatCommandCenterCoordinate(const PoseDetection& detection, bool x_axis)
{
    if (detection.has_plc_command_pose) {
        return FormatNumber(x_axis ? detection.plc_command_center.x : detection.plc_command_center.y);
    }
    return FormatNumber(x_axis ? detection.center_x : detection.center_y);
}

QString FormatCommandMachineCoordinate(const PoseDetection& detection, bool x_axis)
{
    if (detection.has_plc_command_pose && detection.has_machine_coords) {
        return FormatNumber(x_axis ? detection.plc_command_x : detection.plc_command_y);
    }
    return FormatMachineCoordinate(detection, x_axis);
}

QString FormatHeadType(int code, const std::string& text)
{
    return code > 0
        ? QStringLiteral("%1 %2").arg(code).arg(QString::fromStdString(text))
        : QStringLiteral("--");
}

QString FormatPlcWriteResult(const PlcWriteResult& result, const PlcRegisterMap& registers)
{
    const QString machine_text = result.has_machine_coords
        ? QStringLiteral("D%1前后轴值：%2\nD%3左右轴值：%4")
              .arg(registers.front_back)
              .arg(FormatNumber(result.x))
              .arg(registers.left_right)
              .arg(FormatNumber(result.y))
        : QStringLiteral("机械X：未启用\n机械Y：未启用\n");
    return QStringLiteral("图像X：%1\n图像Y：%2\n%3D%4 前后：%5\nD%6 左右：%7\nD%8 角度：%9\nD%10 状态：%11\nD%12 类型：%13")
        .arg(FormatNumber(result.image_x))
        .arg(FormatNumber(result.image_y))
        .arg(machine_text)
        .arg(registers.front_back)
        .arg(FormatNumber(result.x))
        .arg(registers.left_right)
        .arg(FormatNumber(result.y))
        .arg(registers.angle)
        .arg(FormatNumber(result.angle))
        .arg(registers.pick_status)
        .arg(FormatNumber(result.pick_status))
        .arg(registers.head_type)
        .arg(FormatNumber(result.head_type));
}


} // namespace

void GraspMainWindow::loadImage()
{
    if (image_load_watcher_ && !image_load_watcher_->isFinished()) {
        updateStatusMessage(QStringLiteral("图片正在加载，请稍候..."), 2000);
        return;
    }

    const QString path = QFileDialog::getOpenFileName(this,
                                                      QStringLiteral("选择图片"),
                                                      currentImagePath(),
                                                      QStringLiteral("Images (*.png *.jpg *.jpeg *.bmp);;All Files (*)"));
    if (path.isEmpty()) {
        return;
    }

    workflow_.stopCamera();
    input_mode_ = InputMode::Image;
    updateStatusMessage(QStringLiteral("正在加载图片..."));
    if (load_image_button_) {
        load_image_button_->setEnabled(false);
    }
    refreshDeviceStatus();
    refreshRuntimeStrip();

    auto* watcher = new QFutureWatcher<ImageLoadResult>(this);
    image_load_watcher_ = watcher;

    connect(watcher, &QFutureWatcher<ImageLoadResult>::finished, this, [this, watcher]() {
        const ImageLoadResult result = watcher->result();
        if (image_load_watcher_ == watcher) {
            image_load_watcher_ = nullptr;
        }
        watcher->deleteLater();

        if (load_image_button_) {
            load_image_button_->setEnabled(true);
        }

        if (!result.error_message.isEmpty() || result.image.empty()) {
            cout << "[Image] load failed: " << result.path.toStdString() << endl;
            QMessageBox::critical(this, QStringLiteral("读取失败"), QStringLiteral("无法读取所选图片。"));
            refreshDeviceStatus();
            refreshRuntimeStrip();
            updateStatusMessage(QStringLiteral("图片加载失败。"), 5000);
            return;
        }

        current_frame_ = result.image;
        current_image_path_ = QFileInfo(result.path).absoluteFilePath();
        image_path_label_->setText(QFileInfo(result.path).fileName());
        image_path_label_->setToolTip(current_image_path_);
        cout << "[Image] loaded: " << result.path.toStdString()
             << " (" << current_frame_.cols << "x" << current_frame_.rows << ")" << endl;
        clearResults();
        renderCurrentFrame();
        refreshDeviceStatus();
        refreshRuntimeStrip();
        updateStatusMessage(QStringLiteral("图片已加载，点击开始检测执行视觉检测。"));
    });

    watcher->setFuture(QtConcurrent::run([path]() {
        ImageLoadResult result;
        result.path = path;
        if (path.trimmed().isEmpty()) {
            result.error_message = QStringLiteral("empty path");
            return result;
        }

        result.image = imread(path.toStdString());
        if (result.image.empty()) {
            result.error_message = QStringLiteral("read failed");
        }
        return result;
    }));
}

void GraspMainWindow::openCamera()
{
    if (plc_runtime_state_.active()) {
        updateStatusMessage(QStringLiteral("抓取运行中，相机已在采集。"), 5000);
        return;
    }

    workflow_.stopCamera();
    clearResults();

    if (!workflow_.areModelsLoaded()) {
        QMessageBox::warning(this,
                             QStringLiteral("模型未加载"),
                             QStringLiteral("请先在工程设置中加载模型。"));
        refreshDeviceStatus();
        refreshRuntimeStrip();
        return;
    }

    input_mode_ = InputMode::Camera;
    resetCameraUiCounters();
    setEmptyPreviewMessage(QStringLiteral("正在打开默认相机..."));
    const bool started = workflow_.startFrameCapture(
        [this](const Mat& frame) {
            const int seen = ++camera_ui_seen_count_;
            bool expected = false;
            if (!camera_ui_update_pending_.compare_exchange_strong(expected, true)) {
                const int skipped = ++camera_ui_skipped_count_;
                if (seen == 1 || seen % 120 == 0) {
                    cout << "[CameraCaptureUI] seen=" << seen
                         << " skipped=" << skipped
                         << " pending=1"
                         << endl;
                }
                return;
            }

            const auto queued_at = std::chrono::steady_clock::now();
            Mat frame_copy = frame.clone();
            QMetaObject::invokeMethod(
                this,
                [this, frame_copy, queued_at]() {
                    const auto ui_start = std::chrono::steady_clock::now();
                    const int rendered = camera_ui_rendered_count_.load() + 1;
                    current_frame_ = frame_copy;
                    image_path_label_->setText(QStringLiteral("相机采集中"));
                    renderCurrentFrame();
                    refreshDeviceStatus();
                    refreshRuntimeStrip();
                    const auto ui_end = std::chrono::steady_clock::now();
                    camera_ui_rendered_count_ = rendered;
                    if (rendered == 1 || rendered % 120 == 0) {
                        cout << "[CameraCaptureUI] rendered=" << rendered
                             << " seen=" << camera_ui_seen_count_.load()
                             << " skipped=" << camera_ui_skipped_count_.load()
                             << " queue_ms=" << GraspMainWindowMsSince(queued_at, ui_start)
                             << " ui_update_ms=" << GraspMainWindowMsSince(ui_start, ui_end)
                             << " image=" << frame_copy.cols << "x" << frame_copy.rows
                             << endl;
                    }
                    if (rendered == 1) {
                        updateStatusMessage(QStringLiteral("相机已打开，当前仅采集画面；点击开始检测可手动检测一次。"), 5000);
                    }
                    camera_ui_update_pending_ = false;
                },
                Qt::QueuedConnection);
        },
        [this](const string& error_message) {
            QMetaObject::invokeMethod(
                this,
                [this, error_message]() {
                    camera_ui_update_pending_ = false;
                    input_mode_ = InputMode::Idle;
                    refreshDeviceStatus();
                    refreshRuntimeStrip();
                    QMessageBox::critical(this, QStringLiteral("相机错误"), QString::fromStdString(error_message));
                    updateStatusMessage(QStringLiteral("相机采集已停止。"));
                },
                Qt::QueuedConnection);
        });

    refreshDeviceStatus();
    refreshRuntimeStrip();
    if (started) {
        updateStatusMessage(QStringLiteral("默认相机已启动，当前只采集画面，不自动检测。"));
    }
}

void GraspMainWindow::closeCamera()
{
    if (plc_runtime_state_.active()) {
        updateStatusMessage(QStringLiteral("抓取运行中，请使用“关闭”退出 PLC 抓取。"), 5000);
        return;
    }

    if (!workflow_.isCameraRunning()) {
        updateStatusMessage(QStringLiteral("相机未打开。"), 3000);
        refreshDeviceStatus();
        refreshRuntimeStrip();
        return;
    }

    workflow_.stopCamera();
    camera_ui_update_pending_ = false;
    if (input_mode_ == InputMode::Camera) {
        input_mode_ = current_frame_.empty() ? InputMode::Idle : InputMode::Image;
    }
    image_path_label_->setText(QStringLiteral("相机已关闭"));
    refreshDeviceStatus();
    refreshRuntimeStrip();
    updateStatusMessage(QStringLiteral("相机已关闭。"), 4000);
}

void GraspMainWindow::startDetection()
{
    if (image_detection_running_) {
        updateStatusMessage(QStringLiteral("图片检测中，请稍候..."), 2000);
        return;
    }

    QString pending_error;
    const PendingActivationResult pending_result =
        activatePendingEngineeringSettingsForStart(PendingStartAction::ImageDetection,
                                                   &pending_error);
    if (pending_result == PendingActivationResult::Failed) {
        QMessageBox::warning(this,
                             QStringLiteral("应用保存方案失败"),
                             pending_error);
        return;
    }
    if (pending_result == PendingActivationResult::Loading) {
        updateStatusMessage(QStringLiteral("正在加载保存后的模型，完成后开始图片检测。"), 5000);
        return;
    }

    if (!workflow_.areModelsLoaded()) {
        QMessageBox::warning(this,
                             QStringLiteral("模型未加载"),
                             QStringLiteral("请先在工程设置中加载模型。"));
        return;
    }

    if (input_mode_ == InputMode::Camera) {
        if (current_frame_.empty()) {
            if (!workflow_.isCameraRunning()) {
                openCamera();
            } else {
                updateStatusMessage(QStringLiteral("相机采集中，等待第一帧..."), 2000);
            }
            return;
        }
    } else if (current_frame_.empty()) {
        QMessageBox::information(this, QStringLiteral("没有输入"), QStringLiteral("请先加载图片，或打开相机。"));
        return;
    }

    image_detection_running_ = true;
    refreshDeviceStatus();
    refreshRuntimeStrip();
    updateStatusMessage(QStringLiteral("图片检测中..."));

    const auto frame = std::make_shared<Mat>(current_frame_.clone());
    const auto result = std::make_shared<FrameInferenceResult>();
    const GrabLimitConfig limits = grab_limits_;
    const auto image_detect_start = std::chrono::steady_clock::now();
    auto* watcher = new QFutureWatcher<QString>(this);
    image_detection_watcher_ = watcher;

    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher, frame, result, image_detect_start]() {
        const auto finished_start = std::chrono::steady_clock::now();
        const QString error_message = watcher->result();
        if (image_detection_watcher_ == watcher) {
            image_detection_watcher_ = nullptr;
        }
        watcher->deleteLater();
        image_detection_running_ = false;

        if (!error_message.isEmpty()) {
            refreshDeviceStatus();
            refreshRuntimeStrip();
            updateStatusMessage(QStringLiteral("图片检测失败。"), 5000);
            QMessageBox::critical(this, QStringLiteral("检测失败"), error_message);
            return;
        }

        const auto ui_start = std::chrono::steady_clock::now();
        setFrameAndResult(*frame, *result);
        const auto ui_end = std::chrono::steady_clock::now();
        cout << "[ImagePerf] wall_to_finished_ms=" << GraspMainWindowMsSince(image_detect_start, finished_start)
             << " ui_update_ms=" << GraspMainWindowMsSince(ui_start, ui_end)
             << " wall_to_display_ms=" << GraspMainWindowMsSince(image_detect_start, ui_end)
             << " image=" << frame->cols << "x" << frame->rows
             << " total_process_ms=" << result->total_inference_ms
             << endl;
        refreshDeviceStatus();
        refreshRuntimeStrip();
        updateStatusMessage(QStringLiteral("图片检测完成。"), 3000);
        const FrameInferenceResult result_for_plc = *result;
        QTimer::singleShot(0, this, [this, result_for_plc]() {
            writeCurrentResultToPlc(QStringLiteral("图片联合检测"), false, result_for_plc);
        });
    });

    const CoordinateTransformState coordinate_state = coordinate_transform_state_;
    const PlcOutputConfig plc_config = plcOutputConfig();
    const MechanicalGripperCollisionConfig mechanical_gripper{
        mechanical_gripper_length_,
        mechanical_gripper_width_,
        center_ray_offset_px_,
        postprocess_debug_logging_enabled_
    };
    watcher->setFuture(QtConcurrent::run([this, frame, result, coordinate_state, limits, plc_config, mechanical_gripper]() -> QString {
        const FrameProcessingResult process_result =
            ProcessVisionFrame(workflow_,
                               *frame,
                               limits,
                               mechanical_gripper,
                               coordinate_state,
                               plc_config,
                               false);
        *result = process_result.frame_result;
        return process_result.error_message;
    }));
}
void GraspMainWindow::stopDetection()
{
    if (plc_runtime_state_.workflowState() == RuntimeWorkflowState::PlcPolling ||
        plc_runtime_state_.workflowState() == RuntimeWorkflowState::PlcWriting) {
        stopPlcLinkState(RuntimeWorkflowState::Stopped);
    } else {
        stopPlcLinkState(plc_runtime_state_.workflowState());
    }
    workflow_.stopCamera();
    camera_ui_update_pending_ = false;
    refreshDeviceStatus();
    refreshRuntimeStrip();
    updateStatusMessage(QStringLiteral("检测已停止。"), 4000);
}

void GraspMainWindow::togglePlcLinkMode()
{
    if (plc_runtime_state_.active()) {
        stopPlcLinkState(RuntimeWorkflowState::Idle);
        refreshDeviceStatus();
        refreshRuntimeStrip();
        updateStatusMessage(QStringLiteral("抓取已停止。"), 4000);
        return;
    }

    QString pending_error;
    const PendingActivationResult pending_result =
        activatePendingEngineeringSettingsForStart(PendingStartAction::PlcGrasp,
                                                   &pending_error);
    if (pending_result == PendingActivationResult::Failed) {
        QMessageBox::warning(this,
                             QStringLiteral("应用保存方案失败"),
                             pending_error);
        return;
    }
    if (pending_result == PendingActivationResult::Loading) {
        updateStatusMessage(QStringLiteral("正在加载保存后的模型，完成后开始 PLC 抓取。"), 5000);
        return;
    }

    if (!workflow_.areModelsLoaded()) {
        QMessageBox::warning(this,
                             QStringLiteral("模型未加载"),
                             QStringLiteral("请先在工程设置中加载模型。"));
        return;
    }

    if (workflow_.isCameraRunning()) {
        workflow_.stopCamera();
        camera_ui_update_pending_ = false;
    }

    if (!workflow_.isCameraRunning()) {
        input_mode_ = InputMode::Camera;
        resetCameraUiCounters();
        clearResults();
        setEmptyPreviewMessage(QStringLiteral("相机正在打开，等待接收信号。"));

        const bool started = workflow_.startFrameCapture(
            [this](const Mat& frame) {
                const int seen = ++camera_ui_seen_count_;
                bool expected = false;
                if (!camera_ui_update_pending_.compare_exchange_strong(expected, true)) {
                    ++camera_ui_skipped_count_;
                    return;
                }

                Mat frame_copy = frame.clone();
                QMetaObject::invokeMethod(
                    this,
                    [this, frame_copy, seen]() {
                        current_frame_ = frame_copy;
                        image_path_label_->setText(QStringLiteral("PLC触发采集中"));
                        refreshDeviceStatus();
                        refreshRuntimeStrip();
                        if (seen == 1 || seen % 120 == 0) {
                            cout << "[CameraCaptureUI] cached=" << seen
                                 << " seen=" << seen
                                 << " skipped=" << camera_ui_skipped_count_.load()
                                 << " image=" << frame_copy.cols << "x" << frame_copy.rows
                                 << endl;
                        }
                        camera_ui_update_pending_ = false;
                    },
                    Qt::QueuedConnection);
            },
            [this](const string& error_message) {
                QMetaObject::invokeMethod(
                    this,
                    [this, error_message]() {
                        camera_ui_update_pending_ = false;
                        stopPlcLinkState(RuntimeWorkflowState::Idle);
                        input_mode_ = InputMode::Idle;
                        refreshDeviceStatus();
                        refreshRuntimeStrip();
                        QMessageBox::critical(this, QStringLiteral("相机错误"), QString::fromStdString(error_message));
                        updateStatusMessage(QStringLiteral("抓取已停止：相机采集失败。"), 5000);
                    },
                    Qt::QueuedConnection);
            });

        if (!started || !workflow_.isCameraRunning()) {
            stopPlcLinkState(RuntimeWorkflowState::Idle);
            refreshDeviceStatus();
            refreshRuntimeStrip();
            updateStatusMessage(QStringLiteral("抓取未启动：相机未成功打开。"), 4000);
            return;
        }
    }

    startPlcPollingState();
    refreshDeviceStatus();
    refreshRuntimeStrip();
    updateStatusMessage(QStringLiteral("抓取已启动，等待 D1500=1。"), 5000);
}

void GraspMainWindow::pollPlcTrigger()
{
    if (!plc_runtime_state_.active() ||
        plc_runtime_state_.pollBusy() ||
        plc_runtime_state_.waitingForFrame() ||
        (plc_poll_watcher_ && !plc_poll_watcher_->isFinished())) {
        return;
    }

    plc_runtime_state_.setPollBusy(true);
    const auto triggered = std::make_shared<bool>(false);
    auto* watcher = new QFutureWatcher<QString>(this);
    plc_poll_watcher_ = watcher;

    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher, triggered]() {
        finishPlcTriggerPoll(watcher, triggered);
    });

    watcher->setFuture(QtConcurrent::run([this, triggered]() -> QString {
        const PlcTriggerReadResult read_result = ReadPlcPhotoTrigger(robot_controller_);
        *triggered = read_result.triggered;
        return read_result.error_message;
    }));
}

void GraspMainWindow::finishPlcTriggerPoll(QFutureWatcher<QString>* watcher,
                                           const std::shared_ptr<bool>& triggered)
{
    const QString error_message = watcher->result();
    if (plc_poll_watcher_ == watcher) {
        plc_poll_watcher_ = nullptr;
    }
    watcher->deleteLater();

    if (!error_message.isEmpty()) {
        plc_runtime_state_.setPollBusy(false);
        updateStatusMessage(QStringLiteral("PLC read failed: %1").arg(error_message), 3000);
        return;
    }

    if (!*triggered) {
        plc_runtime_state_.setPollBusy(false);
        return;
    }

    plc_runtime_state_.beginWriting();
    refreshRuntimeStrip();
    processPlcTriggeredFrame();
}

void GraspMainWindow::processPlcTriggeredFrame()
{
    if (!workflow_.areModelsLoaded()) {
        plc_runtime_state_.finishWaitingForFrame();
        plc_runtime_state_.restoreIdleOrPolling();
        refreshRuntimeStrip();
        updateStatusMessage(QStringLiteral("PLC触发被忽略：模型未加载。"), 3000);
        return;
    }

    if (current_frame_.empty()) {
        if (workflow_.isCameraRunning()) {
            if (!plc_runtime_state_.waitingForFrame()) {
                plc_runtime_state_.beginWaitingForFrame();
                cout << "[PLCPerf] trigger received before first camera frame; waiting for frame" << endl;
                refreshRuntimeStrip();
                updateStatusMessage(QStringLiteral("已收到 PLC 触发，等待相机第一帧。"), 2000);
            }
            QTimer::singleShot(50, this, [this]() {
                if (!plc_runtime_state_.active() || !plc_runtime_state_.waitingForFrame()) {
                    return;
                }
                processPlcTriggeredFrame();
            });
            return;
        }

        const auto plc_start = std::chrono::steady_clock::now();
        const PlcTriggerProcessResult missing_frame_result =
            ProcessPlcTriggeredMissingFrame(robot_controller_, plcOutputConfig(), plc_start);
        plc_runtime_state_.finishWaitingForFrame();
        if (!missing_frame_result.error_message.isEmpty()) {
            plc_runtime_state_.restoreIdleOrPolling();
            refreshRuntimeStrip();
            updateStatusMessage(QStringLiteral("PLC trigger failed: %1").arg(missing_frame_result.error_message), 5000);
            return;
        }
        const int pick_status_register = robot_controller_.plcRegisterMap().pick_status;
        last_plc_reject_reason_ = missing_frame_result.reject_reason;
        plc_runtime_state_.completeMissingFrame(pick_status_register);
        refreshRuntimeStrip();
        updateStatusMessage(QStringLiteral("PLC触发时没有相机帧，已写 D%1=3。拒绝原因：%2")
                                .arg(pick_status_register)
                                .arg(last_plc_reject_reason_),
                            8000);
        if (plc_test_result_dialog_pending_) {
            plc_test_result_dialog_pending_ = false;
            showPlcTestResultDialog(missing_frame_result.frame_result);
        }
        return;
    }

    plc_runtime_state_.beginWriting();
    refreshRuntimeStrip();

    const auto plc_start = std::chrono::steady_clock::now();
    const auto frame = std::make_shared<Mat>(current_frame_.clone());
    const auto result = std::make_shared<FrameInferenceResult>();
    const auto plc_process_result = std::make_shared<PlcTriggerProcessResult>();
    const GrabLimitConfig limits = grab_limits_;
    auto* watcher = new QFutureWatcher<QString>(this);
    plc_detection_watcher_ = watcher;

    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher, frame, result, plc_process_result, plc_start]() {
        finishPlcTriggeredFrame(watcher, frame, result, plc_process_result, plc_start);
    });

    const CoordinateTransformState coordinate_state = coordinate_transform_state_;
    const PlcOutputConfig plc_config = plcOutputConfig();
    const MechanicalGripperCollisionConfig mechanical_gripper{
        mechanical_gripper_length_,
        mechanical_gripper_width_,
        center_ray_offset_px_,
        postprocess_debug_logging_enabled_
    };
    watcher->setFuture(QtConcurrent::run([this, frame, result, plc_process_result, plc_start, limits, coordinate_state, plc_config, mechanical_gripper]() -> QString {
        *plc_process_result =
            ProcessPlcTriggeredFrame(workflow_,
                                     robot_controller_,
                                     *frame,
                                     limits,
                                     mechanical_gripper,
                                     coordinate_state,
                                     plc_config,
                                     plc_start);
        *result = plc_process_result->frame_result;
        return plc_process_result->error_message;
    }));
}

void GraspMainWindow::finishPlcTriggeredFrame(QFutureWatcher<QString>* watcher,
                                              const std::shared_ptr<Mat>& frame,
                                              const std::shared_ptr<FrameInferenceResult>& result,
                                              const std::shared_ptr<PlcTriggerProcessResult>& plc_process_result,
                                              const std::chrono::steady_clock::time_point& plc_start)
{
    const QString error_message = watcher->result();
    if (plc_detection_watcher_ == watcher) {
        plc_detection_watcher_ = nullptr;
    }
    watcher->deleteLater();
    plc_runtime_state_.setPollBusy(false);

    if (!error_message.isEmpty()) {
        plc_runtime_state_.fail(error_message);
        refreshDeviceStatus();
        refreshRuntimeStrip();
        updateStatusMessage(QStringLiteral("PLC trigger failed: %1").arg(error_message), 6000);
        return;
    }

    const auto ui_start = std::chrono::steady_clock::now();
    setFrameAndResult(*frame, *result);
    const auto ui_end = std::chrono::steady_clock::now();
    cout << "[PLCPerf] ui_update_ms=" << GraspMainWindowMsSince(ui_start, ui_end)
         << " trigger_to_display_ms=" << GraspMainWindowMsSince(plc_start, ui_end)
         << endl;
    QString saved_image_path;
    QString save_error;
    const bool image_saved = image_save_active_ &&
        savePlcFrameImage(*frame, *result, &saved_image_path, &save_error);
    if (image_save_active_) {
        if (image_saved) {
            cout << "[ImageSave] saved PLC frame: "
                 << saved_image_path.toStdString()
                 << endl;
        } else {
            cerr << "[ImageSave] failed to save PLC frame: "
                 << save_error.toStdString()
                 << endl;
        }
    }
    const double total_ms = GraspMainWindowMsSince(plc_start, ui_end);
    const int pick_status_register = robot_controller_.plcRegisterMap().pick_status;
    if (plc_process_result && !plc_process_result->reject_reason.isEmpty()) {
        last_plc_reject_reason_ = plc_process_result->reject_reason;
        plc_runtime_state_.reject(last_plc_reject_reason_,
                                  pick_status_register,
                                  result->pick_status_code,
                                  total_ms);
        updateStatusMessage(QStringLiteral("PLC拒绝：%1").arg(last_plc_reject_reason_), 8000);
    } else {
        plc_runtime_state_.completeFrame(*result, pick_status_register, total_ms);
    }
    refreshDeviceStatus();
    refreshRuntimeStrip();
    if (image_save_active_) {
        updateStatusMessage(image_saved
                                ? QStringLiteral("PLC图片已保存：%1").arg(saved_image_path)
                                : QStringLiteral("PLC图片保存失败：%1").arg(save_error),
                            image_saved ? 5000 : 8000);
    }
    if (plc_test_result_dialog_pending_) {
        plc_test_result_dialog_pending_ = false;
        showPlcTestResultDialog(*result);
    }
}

void GraspMainWindow::writeCurrentResultToPlc(const QString& context,
                                              bool clear_trigger,
                                              const FrameInferenceResult& result)
{
    const GrabLimitConfig limits = grab_limits_;
    const CoordinateTransformState coordinate_state = coordinate_transform_state_;
    auto* watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher, context]() {
        const QString error_message = watcher->result();
        watcher->deleteLater();
        if (!error_message.isEmpty()) {
            updateStatusMessage(QStringLiteral("%1完成，但 PLC 写入失败：%2")
                                    .arg(context, error_message),
                                6000);
            return;
        }

        updateStatusMessage(QStringLiteral("%1完成，已写入 PLC。").arg(context), 5000);
    });

    const PlcOutputConfig plc_config = plcOutputConfig();
    watcher->setFuture(QtConcurrent::run([this, result, clear_trigger, limits, coordinate_state, plc_config]() -> QString {
        const PlcManualWriteResult write_result =
            WriteFrameResultToPlc(robot_controller_,
                                  result,
                                  limits,
                                  coordinate_state,
                                  plc_config,
                                  clear_trigger);
        return write_result.error_message;
    }));
}

void GraspMainWindow::showPlcTestResultDialog(const FrameInferenceResult& result)
{
    const PlcWriteResult write_result = BuildPlcWriteResult(result, plcOutputConfig());
    const QString message = QStringLiteral("测试成功，结果已写回。\n\n%1")
                                .arg(FormatPlcWriteResult(write_result, robot_controller_.plcRegisterMap()));
    const PlcRegisterMap registers = robot_controller_.plcRegisterMap();
    updateStatusMessage(QStringLiteral("PLC test succeeded: D%1=%2, D%3=%4")
                            .arg(registers.pick_status)
                            .arg(FormatNumber(write_result.pick_status))
                            .arg(registers.head_type)
                            .arg(FormatNumber(write_result.head_type)),
                        6000);
    QMessageBox::information(this, QStringLiteral("测试成功"), message);
}

void GraspMainWindow::writePlcTestValues()
{
    if (plc_test_watcher_ && !plc_test_watcher_->isFinished()) {
        updateStatusMessage(QStringLiteral("PLC test is already running."), 3000);
        return;
    }

    auto* watcher = new QFutureWatcher<QString>(this);
    plc_test_watcher_ = watcher;
    plc_test_button_->setEnabled(false);
    updateStatusMessage(QStringLiteral("PLC test is sending..."), 3000);

    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher]() {
        const QString error_message = watcher->result();
        if (plc_test_watcher_ == watcher) {
            plc_test_watcher_ = nullptr;
        }
        watcher->deleteLater();
        refreshDeviceStatus();

        if (error_message.isEmpty()) {
            if (robot_controller_.simulationEnabled() && plc_runtime_state_.active()) {
                plc_test_result_dialog_pending_ = true;
                updateStatusMessage(QStringLiteral("PLC test sent. Waiting for result..."), 5000);
            } else if (robot_controller_.simulationEnabled()) {
                updateStatusMessage(QStringLiteral("PLC test sent, but PLC link is not active."), 5000);
                QMessageBox::information(this,
                                         QStringLiteral("测试"),
                                         QStringLiteral("模拟触发已发送。请先点击“开始”，再点击“测试”，才能看到检测返回结果。"));
            } else {
                const PlcWriteResult test_result = BuildDiagnosticPlcWriteResult(plcOutputConfig());
                const PlcRegisterMap registers = robot_controller_.plcRegisterMap();
                const QString message = QStringLiteral("测试成功，诊断值已写入。\n\n"
                                                      "D%1 前后：%2\n"
                                                      "D%3 左右：%4\n"
                                                      "D%5 角度：%6\n"
                                                      "D%7 状态：1.0\n"
                                                      "D%8 类型：4.0")
                                           .arg(registers.front_back)
                                           .arg(FormatNumber(test_result.x))
                                           .arg(registers.left_right)
                                           .arg(FormatNumber(test_result.y))
                                           .arg(registers.angle)
                                           .arg(FormatNumber(test_result.angle))
                                           .arg(registers.pick_status)
                                           .arg(registers.head_type);
                updateStatusMessage(QStringLiteral("PLC test succeeded."), 5000);
                QMessageBox::information(this, QStringLiteral("测试成功"), message);
            }
        } else {
            updateStatusMessage(QStringLiteral("PLC test failed: %1").arg(error_message), 6000);
            QMessageBox::warning(this,
                                 QStringLiteral("测试失败"),
                                 QStringLiteral("测试失败：%1").arg(error_message));
        }
    });

    watcher->setFuture(QtConcurrent::run([this]() -> QString {
        std::string error_message;
        if (robot_controller_.writePlcTestValues(&error_message)) {
            return QString();
        }
        return QString::fromStdString(error_message);
    }));
}

void GraspMainWindow::setFrameAndResult(const Mat& frame, const FrameInferenceResult& result)
{
    current_frame_ = frame.clone();
    current_result_ = result;

    renderCurrentFrame();
    refreshInfoPanel();
    refreshTargetTable();
    refreshDeviceStatus();
    refreshRuntimeStrip();
}

bool GraspMainWindow::loadImageFromPath(const QString& path, bool notify)
{
    if (path.trimmed().isEmpty()) {
        cout << "[Image] load rejected: empty path" << endl;
        return false;
    }

    workflow_.stopCamera();
    input_mode_ = InputMode::Image;

    Mat image = imread(path.toStdString());
    if (image.empty()) {
        cout << "[Image] load failed: " << path.toStdString() << endl;
        if (notify) {
            QMessageBox::critical(this, QStringLiteral("读取失败"), QStringLiteral("无法读取所选图片。"));
        }
        return false;
    }

    current_frame_ = image;
    current_image_path_ = QFileInfo(path).absoluteFilePath();
    image_path_label_->setText(QFileInfo(path).fileName());
    image_path_label_->setToolTip(current_image_path_);
    cout << "[Image] loaded: " << path.toStdString()
         << " (" << image.cols << "x" << image.rows << ")" << endl;
    clearResults();
    renderCurrentFrame();
    refreshDeviceStatus();
    refreshRuntimeStrip();
    if (notify) {
        updateStatusMessage(QStringLiteral("图片已加载，点击开始检测执行视觉检测。"));
    }
    return true;
}

void GraspMainWindow::renderCurrentFrame()
{
    if (current_frame_.empty()) {
        return;
    }

    const QSize viewport_size = image_label_->size();
    if (viewport_size.width() <= 0 || viewport_size.height() <= 0) {
        return;
    }
    const QImage image = renderFrameImage(current_frame_, current_result_, viewport_size, display_overlay_mode_);
    if (image.isNull()) {
        image_label_->setText(QStringLiteral("图像渲染失败"));
        return;
    }

    const QPixmap pixmap = QPixmap::fromImage(image);
    image_label_->setPixmap(pixmap);
}

QImage GraspMainWindow::renderFrameImage(const Mat& frame,
                                         const FrameInferenceResult& result,
                                         const QSize& bounds,
                                         DisplayOverlayMode mode) const
{
    if (frame.empty() || bounds.width() <= 0 || bounds.height() <= 0) {
        return QImage();
    }

    const QSize display_qsize = FitImageSize(frame.cols,
                                             frame.rows,
                                             bounds);
    Mat display;
    if (display_qsize.width() != frame.cols || display_qsize.height() != frame.rows) {
        cv::resize(frame,
                   display,
                   Size(display_qsize.width(), display_qsize.height()),
                   0.0,
                   0.0,
                   display_qsize.width() < frame.cols ? INTER_AREA : INTER_LINEAR);
    } else {
        display = frame.clone();
    }

    const double scale_x = display.cols / static_cast<double>(frame.cols);
    const double scale_y = display.rows / static_cast<double>(frame.rows);
    const bool show_all_detections = IsAllDebugMode(mode);
    const FrameInferenceResult display_result = ScaleResultForDisplay(result,
                                                                      scale_x,
                                                                      scale_y,
                                                                      display.size(),
                                                                      display.size(),
                                                                      0,
                                                                      0,
                                                                      show_all_detections);
    GrabLimitOverlayView grab_limit_overlay;
    const GrabLimitOverlayView* grab_limit_overlay_view = nullptr;
    QString grab_limit_overlay_label;
    QPoint grab_limit_overlay_label_pos(12, 28);
    if (mode != DisplayOverlayMode::None && show_grab_limit_overlay_ && grab_limits_.enabled) {
        const GrabLimitOverlayPolygon polygon =
            BuildGrabLimitOverlayPolygon(grab_limits_, coordinate_transform_state_, axis_mapping_mode_);
        grab_limit_overlay.visible = polygon.visible;
        grab_limit_overlay.polygon.reserve(polygon.image_points.size());
        for (const Point2f& point : polygon.image_points) {
            grab_limit_overlay.polygon.emplace_back(static_cast<float>(point.x * scale_x),
                                                    static_cast<float>(point.y * scale_y));
        }
        if (grab_limit_overlay.visible && !grab_limit_overlay.polygon.empty()) {
            const Point2f anchor = grab_limit_overlay.polygon.front();
            grab_limit_overlay_label = QStringLiteral("允许抓取区");
            grab_limit_overlay_label_pos = QPoint(qBound(8, qRound(anchor.x) + 8, qMax(8, display.cols - 96)),
                                                  qBound(22, qRound(anchor.y) - 8, qMax(22, display.rows - 10)));
        } else {
            grab_limit_overlay.message = "ROI unavailable";
            grab_limit_overlay_label = QStringLiteral("保护区域无法显示");
        }
        grab_limit_overlay_view = &grab_limit_overlay;
    }
    if (mode != DisplayOverlayMode::None) {
        DrawFrameOverlay(display,
                         display_result,
                         -1,
                         false,
                         show_all_detections,
                         show_plc_center_debug_,
                         show_head_ray_debug_,
                         grab_limit_overlay_view,
                         show_head_type_adjusted_geometry_);
    }

    QImage image = MatToQImage(display);
    if (image.isNull()) {
        return QImage();
    }
    if (!grab_limit_overlay_label.isEmpty()) {
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing, true);
        QFont label_font = painter.font();
        label_font.setPointSize(qMax(10, qRound(11 * display_result.display_scale)));
        label_font.setBold(true);
        painter.setFont(label_font);
        const QFontMetrics metrics(label_font);
        const QRect text_rect = metrics.boundingRect(grab_limit_overlay_label).adjusted(-6, -4, 6, 4);
        QRect background_rect(grab_limit_overlay_label_pos.x(),
                              grab_limit_overlay_label_pos.y() - text_rect.height() + 4,
                              text_rect.width(),
                              text_rect.height());
        background_rect.moveLeft(qBound(4, background_rect.left(), qMax(4, image.width() - background_rect.width() - 4)));
        background_rect.moveTop(qBound(4, background_rect.top(), qMax(4, image.height() - background_rect.height() - 4)));
        painter.fillRect(background_rect, QColor(0, 0, 0, 135));
        painter.setPen(grab_limit_overlay.visible ? QColor(80, 245, 100) : QColor(255, 220, 80));
        painter.drawText(background_rect.adjusted(6, 2, -6, -2), Qt::AlignCenter, grab_limit_overlay_label);
    }

    return image;
}

bool GraspMainWindow::savePlcFrameImage(const Mat& frame,
                                        const FrameInferenceResult& result,
                                        QString* saved_path,
                                        QString* error_message) const
{
    if (frame.empty()) {
        if (error_message) {
            *error_message = QStringLiteral("当前没有可保存图像。");
        }
        return false;
    }

    QDir image_dir(QCoreApplication::applicationDirPath());
    if (!image_dir.exists(QStringLiteral("images")) && !image_dir.mkpath(QStringLiteral("images"))) {
        if (error_message) {
            *error_message = QStringLiteral("无法创建图片保存目录。");
        }
        return false;
    }
    image_dir.cd(QStringLiteral("images"));

    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz"));
    const QString file_name = QStringLiteral("tankeye_plc_%1_%2.png")
                                  .arg(stamp, DisplayOverlayModeFileSuffix(image_save_overlay_mode_));
    const QString file_path = image_dir.filePath(file_name);
    const QImage image = renderFrameImage(frame,
                                          result,
                                          QSize(frame.cols, frame.rows),
                                          image_save_overlay_mode_);
    if (image.isNull() || !image.save(file_path, "PNG")) {
        if (error_message) {
            *error_message = QStringLiteral("图片保存失败。");
        }
        return false;
    }

    if (saved_path) {
        *saved_path = QFileInfo(file_path).absoluteFilePath();
    }
    return true;
}

void GraspMainWindow::refreshInfoPanel()
{
    const int resolved_index = current_result_.primary_index;

    if (resolved_index < 0 || resolved_index >= static_cast<int>(current_result_.detections.size())) {
        SetValueText(machine_x_value_label_, QStringLiteral("--"));
        SetValueText(machine_y_value_label_, QStringLiteral("--"));
        SetValueText(angle_value_label_, QStringLiteral("--"));
        SetValueText(pick_status_value_label_, QString::number(current_result_.pick_status_code));
        SetValueText(head_type_value_label_, FormatHeadType(current_result_.head_type_code, current_result_.head_type_text));
        return;
    }

    const auto& detection = current_result_.detections[resolved_index];
    SetValueText(machine_x_value_label_, FormatCommandMachineCoordinate(detection, true));
    SetValueText(machine_y_value_label_, FormatCommandMachineCoordinate(detection, false));
    SetValueText(angle_value_label_,
                 FormatNumber(detection.has_display_angle
                                  ? detection.display_angle_deg
                                  : calibratedAngle(detection.angle_deg)) + QStringLiteral(" deg"));
    SetValueText(pick_status_value_label_, QString::number(detection.pick_status_code));
    SetValueText(head_type_value_label_, FormatHeadType(detection.head_type_code, detection.head_type_text));
}

void GraspMainWindow::createTargetCard(TargetCard& target_card)
{
    const PlcRegisterMap registers = robot_controller_.plcRegisterMap();
    auto* card = new QFrame(target_list_content_);
    card->setObjectName("infoCard");

    auto* card_layout = new QVBoxLayout(card);
    card_layout->setContentsMargins(SM(12, 12, 12, 12));
    card_layout->setSpacing(S(8));

    target_card.title_label = new QLabel(card);
    target_card.title_label->setObjectName("statusStackName");
    card_layout->addWidget(target_card.title_label);

    auto* metrics_layout = new QHBoxLayout();
    metrics_layout->setSpacing(S(8));

    auto create_metric = [card, metrics_layout](const QString& name, QLabel*& value_label) {
        auto* metric_card = new QFrame(card);
        metric_card->setObjectName("compactMetricCell");

        auto* metric_layout = new QVBoxLayout(metric_card);
        metric_layout->setContentsMargins(SM(10, 8, 10, 8));
        metric_layout->setSpacing(S(3));

        auto* name_label = new QLabel(name, metric_card);
        name_label->setObjectName("compactMetricName");
        value_label = new QLabel(QStringLiteral("--"), metric_card);
        value_label->setObjectName("compactMetricValue");
        value_label->setWordWrap(true);

        metric_layout->addWidget(name_label);
        metric_layout->addWidget(value_label);
        metrics_layout->addWidget(metric_card);
    };

    create_metric(QStringLiteral("图像X"), target_card.image_x_label);
    create_metric(QStringLiteral("图像Y"), target_card.image_y_label);
    create_metric(QStringLiteral("机械X"), target_card.machine_x_label);
    create_metric(QStringLiteral("机械Y"), target_card.machine_y_label);
    create_metric(QStringLiteral("角度"), target_card.angle_label);
    create_metric(QStringLiteral("D%1").arg(registers.pick_status), target_card.pick_status_label);
    create_metric(QStringLiteral("D%1").arg(registers.head_type), target_card.head_type_label);

    card_layout->addLayout(metrics_layout);
    target_card.card = card;
}

void GraspMainWindow::refreshTargetCard(TargetCard& target_card, const PoseDetection& detection, int detection_index)
{
    target_card.title_label->setText(
        display_overlay_mode_ == DisplayOverlayMode::AllDebug
            ? QStringLiteral("序号 %1").arg(detection_index + 1)
            : (detection_index == current_result_.primary_index
                   ? QStringLiteral("主目标")
                   : QStringLiteral("目标 %1").arg(detection_index + 1)));
    SetValueText(target_card.image_x_label, FormatCommandCenterCoordinate(detection, true));
    SetValueText(target_card.image_y_label, FormatCommandCenterCoordinate(detection, false));
    SetValueText(target_card.machine_x_label, FormatCommandMachineCoordinate(detection, true));
    SetValueText(target_card.machine_y_label, FormatCommandMachineCoordinate(detection, false));
    SetValueText(target_card.angle_label,
                 FormatNumber(detection.has_display_angle
                                  ? detection.display_angle_deg
                                  : calibratedAngle(detection.angle_deg)) + QStringLiteral(" deg"));
    SetValueText(target_card.pick_status_label, QString::number(detection.pick_status_code));
    SetValueText(target_card.head_type_label, FormatHeadType(detection.head_type_code, detection.head_type_text));
}

void GraspMainWindow::refreshTargetTable()
{
    if (!target_list_content_layout_ || !target_list_content_) {
        return;
    }

    if (!target_empty_label_) {
        target_empty_label_ = new QLabel(QStringLiteral("当前无检测结果"), target_list_content_);
        target_empty_label_->setObjectName("pathHintLabel");
        target_empty_label_->setWordWrap(true);
        target_list_content_layout_->addWidget(target_empty_label_);
    }

    QList<int> visible_indices;
    if (display_overlay_mode_ == DisplayOverlayMode::AllDebug) {
        for (int i = 0; i < static_cast<int>(current_result_.detections.size()); ++i) {
            visible_indices.append(i);
        }
    } else {
        const vector<int> normal_indices = SelectNormalDisplayDetectionIndices(current_result_);
        for (const int index : normal_indices) {
            visible_indices.append(index);
        }
    }

    const int required_cards = visible_indices.size();
    while (target_cards_.size() < required_cards) {
        TargetCard target_card;
        createTargetCard(target_card);
        int insert_index = target_list_content_layout_->count();
        if (insert_index > 0 &&
            target_list_content_layout_->itemAt(insert_index - 1)->spacerItem() != nullptr) {
            --insert_index;
        }
        target_list_content_layout_->insertWidget(insert_index, target_card.card);
        target_cards_.append(target_card);
    }

    target_empty_label_->setVisible(required_cards == 0);
    for (int i = 0; i < target_cards_.size(); ++i) {
        TargetCard& target_card = target_cards_[i];
        const bool visible = i < required_cards;
        target_card.card->setVisible(visible);
        if (!visible) {
            continue;
        }

        const int detection_index = visible_indices[i];
        refreshTargetCard(target_card, current_result_.detections[detection_index], detection_index);
    }

    if (target_list_content_layout_->count() == 0 ||
        target_list_content_layout_->itemAt(target_list_content_layout_->count() - 1)->spacerItem() == nullptr) {
        target_list_content_layout_->addStretch();
    }
}

void GraspMainWindow::showDetailPage()
{
    if (side_pages_ && detail_page_) {
        side_pages_->setCurrentWidget(detail_page_);
    }
}

void GraspMainWindow::showTargetListPage()
{
    if (side_pages_ && target_list_page_) {
        side_pages_->setCurrentWidget(target_list_page_);
    }
}

void GraspMainWindow::refreshRuntimePresentation()
{
    refreshDeviceStatus();
    refreshRuntimeStrip();
}

void GraspMainWindow::refreshResultPresentation(bool render_frame)
{
    if (render_frame) {
        renderCurrentFrame();
    }
    refreshInfoPanel();
    refreshTargetTable();
    refreshRuntimePresentation();
}

RuntimeStatusSnapshot GraspMainWindow::runtimeStatusSnapshot() const
{
    RuntimeStatusSnapshot snapshot;
    snapshot.input_mode = input_mode_ == InputMode::Camera
        ? RuntimeInputMode::Camera
        : (input_mode_ == InputMode::Image ? RuntimeInputMode::Image : RuntimeInputMode::Idle);
    snapshot.workflow_state = plc_runtime_state_.workflowState();
    snapshot.result = current_result_;
    snapshot.plc_runtime_status_text = plc_runtime_state_.statusText();
    snapshot.camera_running = workflow_.isCameraRunning();
    snapshot.models_loaded = workflow_.areModelsLoaded();
    snapshot.models_loading = models_loading_;
    snapshot.image_detection_running = image_detection_running_;
    snapshot.plc_link_active = plc_runtime_state_.active();
    snapshot.plc_test_running = plc_test_watcher_ && !plc_test_watcher_->isFinished();
    snapshot.display_overlay_mode = display_overlay_mode_;
    snapshot.has_input = !current_frame_.empty() || input_mode_ == InputMode::Camera;
    snapshot.has_current_frame = !current_frame_.empty();
    return snapshot;
}

void GraspMainWindow::refreshDeviceStatus()
{
    const RuntimeStatusSnapshot snapshot = runtimeStatusSnapshot();
    DeviceStatusView view;
    view.camera_dot = camera_status_dot_;
    view.camera_text = camera_status_value_;
    view.model_dot = obb_status_dot_;
    view.model_text = obb_status_value_;
    view.open_camera_button = open_camera_button_;
    view.start_button = start_button_;
    view.plc_link_button = plc_link_button_;
    view.plc_test_button = plc_test_button_;
    view.stop_button = stop_button_;
    view.display_mode_selector = display_mode_selector_;
    RuntimeStatusPresenter::RefreshDeviceStatus(view, snapshot);
}

void GraspMainWindow::refreshRuntimeStrip()
{
    const RuntimeStatusSnapshot snapshot = runtimeStatusSnapshot();
    RuntimeStripView view;
    view.mode = runtime_mode_label_;
    view.state = runtime_state_label_;
    view.obb_count = runtime_obb_count_label_;
    view.seg_count = runtime_seg_count_label_;
    view.stage_time = runtime_stage_time_label_;
    view.total_time = runtime_total_time_label_;
    RuntimeStatusPresenter::RefreshRuntimeStrip(view, snapshot);
}

void GraspMainWindow::updateStatusMessage(const QString& message, int timeout_ms)
{
    statusBar()->showMessage(message, timeout_ms);
}

void GraspMainWindow::clearResults()
{
    current_result_ = FrameInferenceResult{};
    camera_ui_update_pending_ = false;
    refreshInfoPanel();
    refreshTargetTable();
    refreshRuntimeStrip();
}

void GraspMainWindow::setEmptyPreviewMessage(const QString& message)
{
    current_frame_.release();
    camera_ui_update_pending_ = false;
    image_label_->setPixmap(QPixmap());
    image_label_->setText(message);
    refreshRuntimeStrip();
}

void GraspMainWindow::setModelLoadingState(bool loading)
{
    models_loading_ = loading;
    refreshProjectProfileSelector();
    refreshDeviceStatus();
    refreshRuntimeStrip();
}

void GraspMainWindow::maybeStartAutoGrasp()
{
    if (!auto_start_grasp_requested_ || auto_start_grasp_attempted_) {
        return;
    }
    if (!auto_start_enabled_) {
        auto_start_grasp_attempted_ = true;
        cout << "[Startup] auto start grasp skipped: startup auto-start disabled in settings." << endl;
        return;
    }
    if (models_loading_ || !workflow_.areModelsLoaded()) {
        return;
    }
    if (plc_runtime_state_.active()) {
        auto_start_grasp_attempted_ = true;
        return;
    }

    auto_start_grasp_attempted_ = true;
    cout << "[Startup] auto start grasp requested; entering PLC grasp link." << endl;
    QTimer::singleShot(0, this, [this]() {
        if (!plc_runtime_state_.active() && workflow_.areModelsLoaded()) {
            togglePlcLinkMode();
        }
    });
}

void GraspMainWindow::loadModelsFromPathsAsync(const QString& obb_model_path,
                                               const QString& seg_model_path,
                                               bool show_error_dialog,
                                               bool notify_success,
                                               std::function<void(bool, const QString&)> completion)
{
    if (models_loading_) {
        return;
    }

    const QString trimmed_obb_path = obb_model_path.trimmed();
    const QString trimmed_seg_path = seg_model_path.trimmed();
    if (trimmed_obb_path.isEmpty() || trimmed_seg_path.isEmpty()) {
        if (show_error_dialog) {
            QMessageBox::warning(this,
                                 QStringLiteral("缺少模型"),
                                 QStringLiteral("请同时选择完整的模型文件。"));
        }
        return;
    }

    workflow_.stopCamera();
    setModelLoadingState(true);
    updateStatusMessage(QStringLiteral("正在加载模型..."));

    auto* watcher = new QFutureWatcher<QString>(this);
    model_load_watcher_ = watcher;
    connect(watcher, &QFutureWatcher<QString>::finished, this, [this,
                                                                  watcher,
                                                                  trimmed_obb_path,
                                                                  trimmed_seg_path,
                                                                  show_error_dialog,
                                                                  notify_success,
                                                                  completion]() {
        const QString error_message = watcher->result();
        const bool ok = error_message.isEmpty();
        if (model_load_watcher_ == watcher) {
            model_load_watcher_ = nullptr;
        }
        watcher->deleteLater();
        setModelLoadingState(false);

        if (ok) {
            obb_model_path_ = trimmed_obb_path;
            seg_model_path_ = trimmed_seg_path;
            if (!pending_project_profile_name_.isEmpty()) {
                active_project_profile_name_ = pending_project_profile_name_;
                SaveActiveProjectProfileName(active_project_profile_name_);
                pending_project_profile_name_.clear();
            }
            has_pending_previous_project_profile_settings_ = false;
            if (notify_success) {
                updateStatusMessage(QStringLiteral("模型已加载。"), 5000);
            }
        } else {
            if (has_pending_previous_project_profile_settings_) {
                applyProjectProfileSettings(pending_previous_project_profile_settings_);
            }
            pending_project_profile_name_.clear();
            has_pending_previous_project_profile_settings_ = false;
            if (show_error_dialog) {
                QMessageBox::critical(this, QStringLiteral("模型加载失败"), error_message);
            }
            updateStatusMessage(QStringLiteral("模型加载失败，请检查模型路径和运行环境。"), 6000);
        }

        refreshDeviceStatus();
        refreshRuntimeStrip();
        refreshProjectProfileSelector();
        maybeStartAutoGrasp();
        if (close_after_model_load_) {
            close_after_model_load_ = false;
            close();
        }
        if (completion) {
            completion(ok, error_message);
        }
    });

    watcher->setFuture(QtConcurrent::run([this, trimmed_obb_path, trimmed_seg_path]() -> QString {
        OBBConfig obb_config;
        SEGConfig seg_config;
        obb_config.conf_threshold = static_cast<float>(obb_conf_threshold_);
        obb_config.nms_threshold = static_cast<float>(obb_nms_threshold_);
        seg_config.conf_threshold = static_cast<float>(seg_conf_threshold_);
        seg_config.nms_threshold = static_cast<float>(seg_nms_threshold_);
        obb_config.enable_warmup = true;
        seg_config.enable_warmup = true;
        std::string error_message;
        const bool ok = workflow_.loadModels(trimmed_obb_path.toStdString(),
                                             obb_config,
                                             trimmed_seg_path.toStdString(),
                                             seg_config,
                                             &error_message);
        if (!ok) {
            std::cerr << "[OpenVINO] model load failed: " << error_message << std::endl;
            return QString::fromStdString(error_message);
        }
        return QString();
    }));
}

QString GraspMainWindow::currentImagePath() const
{
    const QString path = current_image_path_.trimmed();
    if (!path.isEmpty()) {
        const QFileInfo info(path);
        if (info.exists()) {
            return info.absoluteFilePath();
        }
    }
    return QStringLiteral("./samples/images");
}
