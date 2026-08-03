#include "engineering_settings_dialog_controller.h"

#include "grasp_main_window.h"

#include "admin_auth_helpers.h"
#include "calibration_profile_store.h"
#include "engineering_settings_dialog_helpers.h"
#include "engineering_settings_service.h"
#include "ui_scale_utils.h"

#include <QtConcurrent/QtConcurrent>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QFutureWatcher>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QList>
#include <QMessageBox>
#include <QPoint>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QStringList>
#include <QTimer>
#include <QToolButton>
#include <QtGlobal>
#include <QVBoxLayout>
#include <QWidget>

#include <cmath>
#include <memory>
#include <string>

namespace {

int S(int value)
{
    return ScalePx(value);
}

QSize SS(int width, int height)
{
    return ScaleSize(width, height);
}

QMargins SM(int left, int top, int right, int bottom)
{
    return ScaleMargins(left, top, right, bottom);
}

QString BuildModelStatusText(const GraspWorkflow& workflow)
{
    return QStringLiteral("模型状态：%1")
        .arg(workflow.areModelsLoaded() ? QStringLiteral("已加载") : QStringLiteral("加载失败"));
}

} // namespace

EngineeringSettingsDialogController::EngineeringSettingsDialogController(GraspMainWindow& window)
    : window_(window)
{
}

void EngineeringSettingsDialogController::show()
{
    GraspMainWindow* owner = &window_;

    if (owner->engineering_settings_dialog_) {
        owner->engineering_settings_dialog_->show();
        owner->engineering_settings_dialog_->raise();
        owner->engineering_settings_dialog_->activateWindow();
        return;
    }

    auto* dialog = new QDialog(owner);
    const PlcRegisterMap registers = owner->robot_controller_.plcRegisterMap();
    owner->engineering_settings_dialog_ = dialog;
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(QStringLiteral("工程设置"));
    dialog->setWindowFlags(Qt::Window | Qt::Dialog);
    dialog->resize(SS(760, 460));
    dialog->setStyleSheet(QString(
        "QDialog { background: #f4f6f8; }"
        "QLabel { color: #111111; font-size: %1px; }"
        "QLabel#engineeringSectionTitle { color: #0f1720; font-size: %2px; font-weight: 700; }"
        "QLineEdit { background: #ffffff; color: #111111; border: 1px solid #9aa9b7; border-radius: %3px; padding: %4px %5px; font-size: %1px; min-height: %6px; }"
        "QDoubleSpinBox { background: #ffffff; color: #111111; border: 1px solid #8fa0ae; border-radius: %3px; padding: %7px %8px; font-size: %1px; min-height: %6px; }"
        "QDoubleSpinBox::up-button, QDoubleSpinBox::down-button { width: %9px; }"
        "QComboBox { background: #ffffff; color: #111111; border: 1px solid #8fa0ae; border-radius: %3px; padding: %7px %8px; font-size: %1px; min-height: %6px; }"
        "QCheckBox { color: #111111; font-size: %2px; font-weight: 700; spacing: %10px; }"
        "QCheckBox::indicator { width: %11px; height: %11px; }"
        "QPushButton { background: #ffffff; color: #111111; border: 1px solid #9aa9b7; border-radius: %3px; padding: %7px %12px; font-size: %1px; font-weight: 600; min-height: %6px; }"
        "QPushButton:hover { background: #e9eef3; }"
        "QFrame#settingsCard { background: #ffffff; border: 1px solid #d5dde5; border-radius: %3px; }"
        "QFrame#collapsibleSection { background: transparent; border: none; }"
        "QToolButton#collapsibleHeader { background: transparent; color: #111111; border: none; padding: %7px 0px; font-size: %2px; font-weight: 700; text-align: left; }"
        "QToolButton#collapsibleHeader:hover { background: transparent; }")
        .arg(S(15))
        .arg(S(17))
        .arg(S(8))
        .arg(S(8))
        .arg(S(10))
        .arg(S(32))
        .arg(S(6))
        .arg(S(8))
        .arg(S(18))
        .arg(S(8))
        .arg(S(18))
        .arg(S(14)));

    QObject::connect(dialog, &QObject::destroyed, owner, [owner]() {
        owner->engineering_settings_dialog_ = nullptr;
    });

    auto* layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(SM(16, 16, 16, 16));
    layout->setSpacing(S(12));

    auto* scroll_area = new QScrollArea(dialog);
    scroll_area->setWidgetResizable(true);
    scroll_area->setFrameShape(QFrame::NoFrame);
    scroll_area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll_area->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    auto* scroll_content = new QWidget(scroll_area);
    auto* scroll_layout = new QVBoxLayout(scroll_content);
    scroll_layout->setContentsMargins(0, 0, 0, 0);
    scroll_layout->setSpacing(S(12));
    scroll_layout->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    scroll_area->setWidget(scroll_content);
    QSettings ui_settings(QStringLiteral("TankEye"), QStringLiteral("TankEye-Iris"));
    ui_settings.beginGroup(QStringLiteral("engineering_ui"));
    const bool basic_expanded = ui_settings.value(QStringLiteral("basic_expanded"), true).toBool();
    const bool angle_debug_expanded = ui_settings.value(QStringLiteral("angle_debug_expanded"), true).toBool();
    const bool debug_expanded = ui_settings.value(QStringLiteral("debug_expanded"), false).toBool();
    const bool limits_expanded = ui_settings.value(QStringLiteral("limits_expanded"), false).toBool();
    const bool coordinate_expanded = ui_settings.value(QStringLiteral("coordinate_expanded"), false).toBool();
    const bool admin_expanded = ui_settings.value(QStringLiteral("admin_expanded"), false).toBool();
    ui_settings.endGroup();

    auto* obb_path_edit = new QLineEdit(owner->obb_model_path_, dialog);
    obb_path_edit->setPlaceholderText(QStringLiteral("选择模型文件 A"));
    auto* seg_path_edit = new QLineEdit(owner->seg_model_path_, dialog);
    seg_path_edit->setPlaceholderText(QStringLiteral("选择模型文件 B"));
    QList<QLineEdit*> camera_ip_edits;
    auto* camera_ip_row = CreateCameraIpRow(dialog, camera_ip_edits);
    PopulateIpEdits(owner->camera_ip_, camera_ip_edits);
    auto* exposure_spin = CreateExposureSpinBox(dialog, owner->camera_exposure_us_);
    auto* obb_conf_threshold_spin = CreateThresholdSpinBox(dialog, owner->obb_conf_threshold_);
    auto* obb_nms_threshold_spin = CreateThresholdSpinBox(dialog, owner->obb_nms_threshold_);
    auto* seg_conf_threshold_spin = CreateThresholdSpinBox(dialog, owner->seg_conf_threshold_);
    auto* seg_nms_threshold_spin = CreateThresholdSpinBox(dialog, owner->seg_nms_threshold_);
    obb_conf_threshold_spin->setToolTip(QStringLiteral("模型A判断阈值：调高可减少误检，但可能增加漏检。"));
    obb_nms_threshold_spin->setToolTip(QStringLiteral("模型A过滤阈值：调低会更积极合并重叠框。"));
    seg_conf_threshold_spin->setToolTip(QStringLiteral("模型B判断阈值：调高可减少误检，但可能增加漏检。"));
    seg_nms_threshold_spin->setToolTip(QStringLiteral("模型B过滤阈值：调低会更积极合并重叠区域。"));
    auto* model_threshold_grid = CreateSettingsGrid(10, 8);
    SetGridColumnMinimumWidths(model_threshold_grid, { 170, 100, 170, 100 });
    SetGridColumnStretches(model_threshold_grid, { 0, 0, 0, 0, 1 });
    model_threshold_grid->addWidget(new QLabel(QStringLiteral("模型A判断阈值"), dialog), 0, 0);
    model_threshold_grid->addWidget(obb_conf_threshold_spin, 0, 1, Qt::AlignLeft);
    model_threshold_grid->addWidget(new QLabel(QStringLiteral("模型A过滤阈值"), dialog), 0, 2);
    model_threshold_grid->addWidget(obb_nms_threshold_spin, 0, 3, Qt::AlignLeft);
    model_threshold_grid->addWidget(new QLabel(QStringLiteral("模型B判断阈值"), dialog), 1, 0);
    model_threshold_grid->addWidget(seg_conf_threshold_spin, 1, 1, Qt::AlignLeft);
    model_threshold_grid->addWidget(new QLabel(QStringLiteral("模型B过滤阈值"), dialog), 1, 2);
    model_threshold_grid->addWidget(seg_nms_threshold_spin, 1, 3, Qt::AlignLeft);
    auto* auto_exposure_button = new QPushButton(QStringLiteral("单次自动曝光"), dialog);
    auto* exposure_row = new QHBoxLayout();
    exposure_row->setSpacing(S(8));
    auto* exposure_label = new QLabel(QStringLiteral("曝光"), dialog);
    exposure_label->setMinimumWidth(S(90));
    exposure_row->addWidget(exposure_label);
    exposure_row->addWidget(exposure_spin);
    exposure_row->addWidget(auto_exposure_button);
    exposure_row->addStretch(1);
    auto* angle_offset_spin = CreateAngleOffsetSpinBox(dialog, owner->angle_offset_deg_);
    auto* angle_direction_combo = new QComboBox(dialog);
    angle_direction_combo->addItem(QStringLiteral("正向"), false);
    angle_direction_combo->addItem(QStringLiteral("反向"), true);
    angle_direction_combo->setCurrentIndex(owner->angle_reverse_direction_ ? 1 : 0);
    angle_direction_combo->setFixedWidth(S(170));
    auto* angle_range_combo = new QComboBox(dialog);
    angle_range_combo->addItem(QStringLiteral("0~360"), static_cast<int>(AngleRangeMode::ZeroTo360));
    angle_range_combo->addItem(QStringLiteral("-180~180"), static_cast<int>(AngleRangeMode::Signed180));
    angle_range_combo->setCurrentIndex(owner->angle_range_mode_ == AngleRangeMode::Signed180 ? 1 : 0);
    angle_range_combo->setFixedWidth(S(170));
    auto* axis_mapping_combo = new QComboBox(dialog);
    axis_mapping_combo->addItem(QStringLiteral("前后=机械Y，左右=机械X"),
                                static_cast<int>(AxisMappingMode::FrontBackMachineY));
    axis_mapping_combo->addItem(QStringLiteral("前后=机械X，左右=机械Y"),
                                static_cast<int>(AxisMappingMode::FrontBackMachineX));
    axis_mapping_combo->setCurrentIndex(owner->axis_mapping_mode_ == AxisMappingMode::FrontBackMachineX ? 1 : 0);
    axis_mapping_combo->setFixedWidth(S(230));
    auto* front_back_offset_spin = CreateLimitSpinBox(dialog, owner->front_back_offset_);
    front_back_offset_spin->setToolTip(
        QStringLiteral("只补偿最终写入 PLC 的 D%1 前后轴，不影响机械 ROI。").arg(registers.front_back));
    auto* left_right_offset_spin = CreateLimitSpinBox(dialog, owner->left_right_offset_);
    left_right_offset_spin->setToolTip(
        QStringLiteral("只补偿最终写入 PLC 的 D%1 左右轴，不影响机械 ROI。").arg(registers.left_right));
    auto* angle_reference_current_spin = CreateAngleReferenceSpinBox(dialog, 0.0);
    auto* angle_reference_target_spin = CreateAngleReferenceSpinBox(dialog, 0.0);
    auto* center_ray_offset_spin = CreateCenterRayOffsetSpinBox(dialog, owner->center_ray_offset_px_);
    center_ray_offset_spin->setToolTip(QStringLiteral("沿当前 OBB 射线方向平移中心点；影响显示、坐标转换和 PLC 坐标。"));
    const int angle_control_width = S(170);
    angle_reference_current_spin->setFixedWidth(angle_control_width);
    angle_reference_target_spin->setFixedWidth(angle_control_width);
    angle_offset_spin->setFixedWidth(angle_control_width);
    center_ray_offset_spin->setFixedWidth(angle_control_width);
    auto* show_plc_center_debug_check = new QCheckBox(QStringLiteral("显示PLC中心点偏移"), dialog);
    show_plc_center_debug_check->setChecked(owner->show_plc_center_debug_);
    show_plc_center_debug_check->setToolTip(QStringLiteral("只在可抓主目标上显示原始 OBB 中心和实际写入 PLC 前使用的偏移后中心。"));
    auto* show_head_ray_debug_check = new QCheckBox(QStringLiteral("显示头部射线"), dialog);
    show_head_ray_debug_check->setChecked(owner->show_head_ray_debug_);
    show_head_ray_debug_check->setToolTip(QStringLiteral("显示 SEG 中心到头部辅助点的射线；大头为黄色，小头为青色。"));
    auto* postprocess_debug_logging_check = new QCheckBox(QStringLiteral("启用后处理调试日志"), dialog);
    postprocess_debug_logging_check->setChecked(owner->postprocess_debug_logging_enabled_);
    postprocess_debug_logging_check->setToolTip(QStringLiteral("在日志中输出 [PostprocessDebug] 明细，用于排查目标为何可抓或不可抓。"));
    auto* capture_current_angle_button = new QPushButton(QStringLiteral("取当前角度"), dialog);
    auto* calculate_angle_offset_button = new QPushButton(QStringLiteral("计算校准"), dialog);

    auto* admin_content = new QWidget(dialog);
    auto* admin_layout = new QVBoxLayout(admin_content);
    admin_layout->setContentsMargins(0, 0, 0, 0);
    admin_layout->setSpacing(S(8));
    auto* admin_card = new QFrame(admin_content);
    admin_card->setObjectName("settingsCard");
    auto* admin_card_layout = new QVBoxLayout(admin_card);
    admin_card_layout->setContentsMargins(SM(12, 12, 12, 12));
    admin_card_layout->setSpacing(S(8));
    auto* admin_form = new QFormLayout();
    admin_form->setHorizontalSpacing(S(12));
    admin_form->setVerticalSpacing(S(8));
    auto* admin_current_password_edit = new QLineEdit(admin_card);
    auto* admin_username_edit = new QLineEdit(owner->adminUsername(), admin_card);
    auto* admin_new_password_edit = new QLineEdit(admin_card);
    auto* admin_confirm_password_edit = new QLineEdit(admin_card);
    admin_form->addRow(QStringLiteral("当前密码"), CreatePasswordFieldWithVisibilityButton(admin_current_password_edit, admin_card));
    admin_form->addRow(QStringLiteral("管理员账号"), admin_username_edit);
    admin_form->addRow(QStringLiteral("新密码"), CreatePasswordFieldWithVisibilityButton(admin_new_password_edit, admin_card));
    admin_form->addRow(QStringLiteral("确认新密码"), CreatePasswordFieldWithVisibilityButton(admin_confirm_password_edit, admin_card));
    admin_card_layout->addLayout(admin_form);
    auto* save_admin_button = new QPushButton(QStringLiteral("保存管理员账号"), admin_card);
    auto* admin_button_row = new QHBoxLayout();
    admin_button_row->addStretch(1);
    admin_button_row->addWidget(save_admin_button);
    admin_card_layout->addLayout(admin_button_row);
    admin_layout->addWidget(admin_card);
    auto* angle_debug_grid = CreateSettingsGrid(10, 10);
    angle_debug_grid->setColumnMinimumWidth(0, S(120));
    angle_debug_grid->setColumnMinimumWidth(1, angle_control_width);
    angle_debug_grid->setColumnMinimumWidth(2, S(120));
    SetGridColumnStretches(angle_debug_grid, { 0, 0, 0, 1 });
    const auto create_angle_label = [dialog](const QString& text) {
        return CreateDistributedFieldLabel(text, dialog, 132);
    };
    angle_debug_grid->addWidget(create_angle_label(QStringLiteral("当前显示角度：")), 0, 0);
    angle_debug_grid->addWidget(angle_reference_current_spin, 0, 1);
    angle_debug_grid->addWidget(capture_current_angle_button, 0, 2, Qt::AlignLeft);
    angle_debug_grid->addWidget(create_angle_label(QStringLiteral("机器目标角度：")), 1, 0);
    angle_debug_grid->addWidget(angle_reference_target_spin, 1, 1);
    angle_debug_grid->addWidget(calculate_angle_offset_button, 1, 2, Qt::AlignLeft);
    angle_debug_grid->addWidget(create_angle_label(QStringLiteral("角度偏移：")), 2, 0);
    angle_debug_grid->addWidget(angle_offset_spin, 2, 1);
    angle_debug_grid->addWidget(create_angle_label(QStringLiteral("正向/反向：")), 3, 0);
    angle_debug_grid->addWidget(angle_direction_combo, 3, 1);
    angle_debug_grid->addWidget(create_angle_label(QStringLiteral("角度范围：")), 4, 0);
    angle_debug_grid->addWidget(angle_range_combo, 4, 1);
    angle_debug_grid->addWidget(create_angle_label(QStringLiteral("中心偏移：")), 5, 0);
    angle_debug_grid->addWidget(center_ray_offset_spin, 5, 1);
    angle_debug_grid->addWidget(show_plc_center_debug_check, 6, 1, 1, 2, Qt::AlignLeft);
    auto* axis_mapping_row = CreateSettingsGrid(10, 10);
    SetGridColumnMinimumWidths(axis_mapping_row, { 120, 170 });
    SetGridColumnStretches(axis_mapping_row, { 0, 0, 0, 1 });
    axis_mapping_row->addWidget(new QLabel(QStringLiteral("轴向映射"), dialog), 0, 0);
    axis_mapping_row->addWidget(axis_mapping_combo, 0, 1, 1, 2, Qt::AlignLeft);
    axis_mapping_row->addWidget(new QLabel(QStringLiteral("前后补偿"), dialog), 1, 0);
    axis_mapping_row->addWidget(front_back_offset_spin, 1, 1, Qt::AlignLeft);
    axis_mapping_row->addWidget(new QLabel(QStringLiteral("左右补偿"), dialog), 2, 0);
    axis_mapping_row->addWidget(left_right_offset_spin, 2, 1, Qt::AlignLeft);
    auto* limit_enabled_check = new QCheckBox(QStringLiteral("启用上下限保护"), dialog);
    limit_enabled_check->setChecked(owner->grab_limits_.enabled);
    auto* x_lower_spin = CreateLimitSpinBox(dialog, owner->grab_limits_.x.lower);
    auto* x_upper_spin = CreateLimitSpinBox(dialog, owner->grab_limits_.x.upper);
    auto* y_lower_spin = CreateLimitSpinBox(dialog, owner->grab_limits_.y.lower);
    auto* y_upper_spin = CreateLimitSpinBox(dialog, owner->grab_limits_.y.upper);
    auto* angle_lower_spin = CreateLimitSpinBox(dialog, owner->grab_limits_.angle.lower);
    auto* angle_upper_spin = CreateLimitSpinBox(dialog, owner->grab_limits_.angle.upper);
    auto* roi_margin_spin = CreateLimitSpinBox(dialog, owner->grab_limits_.roi_margin);
    roi_margin_spin->setRange(0.0, 999999.0);
    auto* coordinate_enabled_check = new QCheckBox(QStringLiteral("启用坐标转换"), dialog);
    coordinate_enabled_check->setChecked(owner->coordinate_transform_config_.enabled);
    auto* profile_combo = new QComboBox(dialog);
    profile_combo->setFixedWidth(S(190));
    auto* apply_profile_button = new QPushButton(QStringLiteral("应用"), dialog);
    auto* save_as_profile_button = new QPushButton(QStringLiteral("另存为"), dialog);
    auto* delete_profile_button = new QPushButton(QStringLiteral("删除"), dialog);
    QList<QDoubleSpinBox*> image_x_edits;
    QList<QDoubleSpinBox*> image_y_edits;
    QList<QDoubleSpinBox*> machine_x_edits;
    QList<QDoubleSpinBox*> machine_y_edits;
    auto* coordinate_card = new QWidget(dialog);
    coordinate_card->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    coordinate_card->setMaximumWidth(S(620));
    auto* coordinate_card_layout = new QVBoxLayout(coordinate_card);
    coordinate_card_layout->setContentsMargins(0, 0, 0, 0);
    coordinate_card_layout->setSpacing(S(8));
    coordinate_card_layout->setSizeConstraint(QLayout::SetFixedSize);
    coordinate_card_layout->addWidget(CreateGroupCaption(QStringLiteral("点位参数"), coordinate_card));
    auto* profile_row = new QHBoxLayout();
    profile_row->setSpacing(S(6));
    profile_row->addWidget(new QLabel(QStringLiteral("方案"), coordinate_card));
    profile_row->addWidget(profile_combo);
    profile_row->addWidget(apply_profile_button);
    profile_row->addWidget(save_as_profile_button);
    profile_row->addWidget(delete_profile_button);
    profile_row->addStretch(1);
    coordinate_card_layout->addLayout(profile_row);

    auto* coordinate_grid = CreateSettingsGrid(8, 8);
    SetGridColumnMinimumWidths(coordinate_grid, { 42 });
    SetGridColumnStretches(coordinate_grid, { 0, 0, 0, 0, 0 });
    coordinate_grid->addWidget(new QLabel(QStringLiteral("点位"), dialog), 0, 0);
    coordinate_grid->addWidget(new QLabel(QStringLiteral("图像X"), dialog), 0, 1);
    coordinate_grid->addWidget(new QLabel(QStringLiteral("图像Y"), dialog), 0, 2);
    coordinate_grid->addWidget(new QLabel(QStringLiteral("机械X"), dialog), 0, 3);
    coordinate_grid->addWidget(new QLabel(QStringLiteral("机械Y"), dialog), 0, 4);

    image_x_edits.reserve(9);
    image_y_edits.reserve(9);
    machine_x_edits.reserve(9);
    machine_y_edits.reserve(9);
    for (int row = 0; row < 9; ++row) {
        coordinate_grid->addWidget(new QLabel(QStringLiteral("P%1").arg(row + 1), dialog), row + 1, 0);

        auto* image_x_edit = CreateCoordinateSpinBox(dialog, 0.0);
        auto* image_y_edit = CreateCoordinateSpinBox(dialog, 0.0);
        auto* machine_x_edit = CreateCoordinateSpinBox(dialog, 0.0);
        auto* machine_y_edit = CreateCoordinateSpinBox(dialog, 0.0);
        image_x_edits.append(image_x_edit);
        image_y_edits.append(image_y_edit);
        machine_x_edits.append(machine_x_edit);
        machine_y_edits.append(machine_y_edit);
        coordinate_grid->addWidget(image_x_edit, row + 1, 1);
        coordinate_grid->addWidget(image_y_edit, row + 1, 2);
        coordinate_grid->addWidget(machine_x_edit, row + 1, 3);
        coordinate_grid->addWidget(machine_y_edit, row + 1, 4);
    }

    const auto populate_coordinate_fields = [owner](const QList<QDoubleSpinBox*>& image_x_edits,
                                                    const QList<QDoubleSpinBox*>& image_y_edits,
                                                    const QList<QDoubleSpinBox*>& machine_x_edits,
                                                    const QList<QDoubleSpinBox*>& machine_y_edits) {
        for (int row = 0; row < 9; ++row) {
            const CoordinateCalibrationPoint point =
                row < static_cast<int>(owner->coordinate_transform_config_.points.size())
                    ? owner->coordinate_transform_config_.points[row]
                    : CoordinateCalibrationPoint{};
            SetCoordinateValue(image_x_edits, row, point.image_x);
            SetCoordinateValue(image_y_edits, row, point.image_y);
            SetCoordinateValue(machine_x_edits, row, point.machine_x);
            SetCoordinateValue(machine_y_edits, row, point.machine_y);
        }
    };
    populate_coordinate_fields(image_x_edits, image_y_edits, machine_x_edits, machine_y_edits);
    coordinate_card_layout->addLayout(coordinate_grid);

    auto* status_label = new QLabel(owner->models_loading_
                                        ? QStringLiteral("模型正在后台加载，请稍候...")
                                        : BuildModelStatusText(owner->workflow_),
                                    dialog);
    auto* coordinate_status_label = new QLabel(CoordinateTransformStatusText(owner->coordinate_transform_state_), dialog);
    coordinate_status_label->setWordWrap(true);
    RefreshCoordinateStatusLabel(coordinate_status_label, owner->coordinate_transform_state_);
    auto* angle_range_hint_label = new QLabel(dialog);
    angle_range_hint_label->setObjectName("pathHintLabel");
    angle_range_hint_label->setWordWrap(true);
    angle_range_hint_label->setStyleSheet(QString(
        "QLabel { color: #374151; background: transparent; border: none; padding: %1px 0px; font-size: %2px; font-weight: 600; }")
        .arg(S(6))
        .arg(S(15)));
    auto refresh_angle_range_hint = [angle_range_combo, angle_range_hint_label]() {
        const auto mode = static_cast<AngleRangeMode>(angle_range_combo->currentData().toInt());
        angle_range_hint_label->setText(
            mode == AngleRangeMode::Signed180
                ? QStringLiteral("当前角度范围：-180~180。角度上下限也请按 -180~180 填写。")
                : QStringLiteral("当前角度范围：0~360。角度上下限也请按 0~360 填写。"));
    };
    refresh_angle_range_hint();
    QObject::connect(angle_range_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), dialog, [refresh_angle_range_hint]() {
        refresh_angle_range_hint();
    });
    EngineeringSettingsControls settings_controls;
    settings_controls.camera_ip_edits = camera_ip_edits;
    settings_controls.exposure_spin = exposure_spin;
    settings_controls.obb_conf_threshold_spin = obb_conf_threshold_spin;
    settings_controls.obb_nms_threshold_spin = obb_nms_threshold_spin;
    settings_controls.seg_conf_threshold_spin = seg_conf_threshold_spin;
    settings_controls.seg_nms_threshold_spin = seg_nms_threshold_spin;
    settings_controls.angle_offset_spin = angle_offset_spin;
    settings_controls.center_ray_offset_spin = center_ray_offset_spin;
    settings_controls.show_plc_center_debug_check = show_plc_center_debug_check;
    settings_controls.show_head_ray_debug_check = show_head_ray_debug_check;
    settings_controls.postprocess_debug_logging_check = postprocess_debug_logging_check;
    settings_controls.angle_direction_combo = angle_direction_combo;
    settings_controls.angle_range_combo = angle_range_combo;
    settings_controls.axis_mapping_combo = axis_mapping_combo;
    settings_controls.front_back_offset_spin = front_back_offset_spin;
    settings_controls.left_right_offset_spin = left_right_offset_spin;
    settings_controls.limit_enabled_check = limit_enabled_check;
    settings_controls.x_lower_spin = x_lower_spin;
    settings_controls.x_upper_spin = x_upper_spin;
    settings_controls.y_lower_spin = y_lower_spin;
    settings_controls.y_upper_spin = y_upper_spin;
    settings_controls.angle_lower_spin = angle_lower_spin;
    settings_controls.angle_upper_spin = angle_upper_spin;
    settings_controls.roi_margin_spin = roi_margin_spin;
    settings_controls.coordinate_enabled_check = coordinate_enabled_check;
    settings_controls.image_x_edits = image_x_edits;
    settings_controls.image_y_edits = image_y_edits;
    settings_controls.machine_x_edits = machine_x_edits;
    settings_controls.machine_y_edits = machine_y_edits;
    settings_controls.profile_combo = profile_combo;
    auto normalize_transform_config = [](CoordinateTransformConfig* config) {
        if (config == nullptr) {
            return;
        }
        config->profile_name = SanitizeCalibrationProfileName(config->profile_name);
        if (config->profile_name.isEmpty()) {
            config->profile_name = DefaultCalibrationProfileName();
        }
        if (config->points.size() < 9) {
            config->points.resize(9);
        }
        if (config->points.size() > 9) {
            config->points.resize(9);
        }
    };
    auto refresh_profile_combo = [owner, profile_combo, normalize_transform_config]() {
        QSignalBlocker blocker(profile_combo);
        profile_combo->clear();
        QStringList profile_names = ListCalibrationProfileNames();
        CoordinateTransformConfig current_config = owner->coordinate_transform_config_;
        normalize_transform_config(&current_config);
        if (!profile_names.contains(current_config.profile_name)) {
            profile_names.prepend(current_config.profile_name);
        }
        profile_names.removeDuplicates();
        profile_combo->addItems(profile_names);
        const int index = profile_combo->findText(current_config.profile_name);
        if (index >= 0) {
            profile_combo->setCurrentIndex(index);
        }
    };
    auto apply_transform_config_to_editor = [owner,
                                             coordinate_enabled_check,
                                             image_x_edits,
                                             image_y_edits,
                                             machine_x_edits,
                                             machine_y_edits,
                                             coordinate_status_label,
                                             normalize_transform_config,
                                             refresh_profile_combo,
                                             populate_coordinate_fields](CoordinateTransformConfig config) {
        normalize_transform_config(&config);
        owner->coordinate_transform_config_ = config;
        owner->rebuildCoordinateTransformState();
        coordinate_enabled_check->setChecked(owner->coordinate_transform_config_.enabled);
        populate_coordinate_fields(image_x_edits,
                                   image_y_edits,
                                   machine_x_edits,
                                   machine_y_edits);
        RefreshCoordinateStatusLabel(coordinate_status_label, owner->coordinate_transform_state_);
        refresh_profile_combo();
    };
    auto read_transform_config_from_editor = [settings_controls](const QString& profile_name,
                                                                 CoordinateTransformConfig* config,
                                                                 QString* error_message) {
        return ReadCoordinateTransformConfigFromEditor(settings_controls, profile_name, config, error_message);
    };
    refresh_profile_combo();
    auto* load_button = new QPushButton(QStringLiteral("加载模型"), dialog);
    auto* validate_transform_button = new QPushButton(QStringLiteral("计算矩阵"), dialog);
    auto* save_settings_button = new QPushButton(QStringLiteral("保存设置"), dialog);

    auto refresh_model_state = [owner, status_label, load_button]() {
        if (owner->models_loading_) {
            status_label->setText(QStringLiteral("模型正在后台加载，请稍候..."));
            load_button->setText(QStringLiteral("加载中..."));
            load_button->setEnabled(false);
            return;
        }

        status_label->setText(BuildModelStatusText(owner->workflow_));
        load_button->setText(QStringLiteral("加载模型"));
        load_button->setEnabled(true);
    };
    refresh_model_state();

    auto* model_state_timer = new QTimer(dialog);
    model_state_timer->setInterval(200);
    QObject::connect(model_state_timer, &QTimer::timeout, dialog, refresh_model_state);
    model_state_timer->start();

    scroll_layout->addWidget(CreateCollapsibleSection(dialog,
                                                      QStringLiteral("管理员账户"),
                                                      admin_content,
                                                      admin_expanded,
                                                      QStringLiteral("admin_expanded")));

    auto* basic_content = new QWidget(dialog);
    auto* basic_content_layout = new QVBoxLayout(basic_content);
    basic_content_layout->setContentsMargins(0, 0, 0, 0);
    basic_content_layout->setSpacing(S(8));
    basic_content_layout->addLayout(CreatePathRow(dialog, obb_path_edit, QStringLiteral("模型文件 A"), QStringLiteral("选择模型文件 A")));
    basic_content_layout->addLayout(CreatePathRow(dialog, seg_path_edit, QStringLiteral("模型文件 B"), QStringLiteral("选择模型文件 B")));
    basic_content_layout->addLayout(model_threshold_grid);
    basic_content_layout->addLayout(camera_ip_row);
    basic_content_layout->addLayout(exposure_row);
    scroll_layout->addWidget(CreateCollapsibleSection(dialog,
                                                      QStringLiteral("基础设置"),
                                                      basic_content,
                                                      basic_expanded,
                                                      QStringLiteral("basic_expanded")));

    auto* advanced_angle_content = new QWidget(dialog);
    auto* advanced_angle_content_layout = new QVBoxLayout(advanced_angle_content);
    advanced_angle_content_layout->setContentsMargins(0, 0, 0, 0);
    advanced_angle_content_layout->setSpacing(S(8));
    advanced_angle_content_layout->addLayout(angle_debug_grid);
    scroll_layout->addWidget(CreateCollapsibleSection(dialog,
                                                      QStringLiteral("角度调试"),
                                                      advanced_angle_content,
                                                      angle_debug_expanded,
                                                      QStringLiteral("angle_debug_expanded")));

    scroll_layout->addSpacing(S(4));

    auto* debug_content = new QWidget(dialog);
    auto* debug_content_layout = new QVBoxLayout(debug_content);
    debug_content_layout->setContentsMargins(0, 0, 0, 0);
    debug_content_layout->setSpacing(S(8));
    debug_content_layout->addWidget(show_head_ray_debug_check);
    debug_content_layout->addWidget(postprocess_debug_logging_check);
    scroll_layout->addWidget(CreateCollapsibleSection(dialog,
                                                      QStringLiteral("调试设置"),
                                                      debug_content,
                                                      debug_expanded,
                                                      QStringLiteral("debug_expanded")));

    scroll_layout->addSpacing(S(4));

    auto* limit_content = new QWidget(dialog);
    auto* limit_content_layout = new QVBoxLayout(limit_content);
    limit_content_layout->setContentsMargins(0, 0, 0, 0);
    limit_content_layout->setSpacing(S(8));
    limit_content_layout->addLayout(axis_mapping_row);
    limit_content_layout->addWidget(limit_enabled_check);

    auto* limit_grid = CreateSettingsGrid(8, 8);
    SetGridColumnMinimumWidths(limit_grid, { 78 });
    SetGridColumnStretches(limit_grid, { 0, 0, 0, 1 });
    limit_grid->addWidget(new QLabel(QStringLiteral("轴向"), dialog), 0, 0);
    limit_grid->addWidget(new QLabel(QStringLiteral("下限"), dialog), 0, 1);
    limit_grid->addWidget(new QLabel(QStringLiteral("上限"), dialog), 0, 2);
    auto* front_back_limit_label = new QLabel(owner->frontBackAxisLabel(), dialog);
    auto* left_right_limit_label = new QLabel(owner->leftRightAxisLabel(), dialog);
    limit_grid->addWidget(front_back_limit_label, 1, 0);
    limit_grid->addWidget(x_lower_spin, 1, 1, Qt::AlignLeft);
    limit_grid->addWidget(x_upper_spin, 1, 2, Qt::AlignLeft);
    limit_grid->addWidget(left_right_limit_label, 2, 0);
    limit_grid->addWidget(y_lower_spin, 2, 1, Qt::AlignLeft);
    limit_grid->addWidget(y_upper_spin, 2, 2, Qt::AlignLeft);
    limit_grid->addWidget(new QLabel(QStringLiteral("旋转角度"), dialog), 3, 0);
    limit_grid->addWidget(angle_lower_spin, 3, 1, Qt::AlignLeft);
    limit_grid->addWidget(angle_upper_spin, 3, 2, Qt::AlignLeft);
    limit_grid->addWidget(new QLabel(QStringLiteral("ROI 留边"), dialog), 4, 0);
    limit_grid->addWidget(roi_margin_spin, 4, 1, Qt::AlignLeft);
    limit_grid->addWidget(new QLabel(QStringLiteral("机械坐标单位，边界向内收缩"), dialog), 4, 2, 1, 2);
    auto refresh_axis_mapping_labels = [axis_mapping_combo, front_back_limit_label, left_right_limit_label]() {
        const auto mode = static_cast<AxisMappingMode>(axis_mapping_combo->currentData().toInt());
        front_back_limit_label->setText(mode == AxisMappingMode::FrontBackMachineX
                                            ? QStringLiteral("前后 / 机械X")
                                            : QStringLiteral("前后 / 机械Y"));
        left_right_limit_label->setText(mode == AxisMappingMode::FrontBackMachineX
                                            ? QStringLiteral("左右 / 机械Y")
                                            : QStringLiteral("左右 / 机械X"));
    };
    refresh_axis_mapping_labels();
    QObject::connect(axis_mapping_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), dialog, [refresh_axis_mapping_labels]() {
        refresh_axis_mapping_labels();
    });
    limit_content_layout->addLayout(limit_grid);
    limit_content_layout->addWidget(angle_range_hint_label);
    scroll_layout->addWidget(CreateCollapsibleSection(dialog,
                                                      QStringLiteral("上下限保护"),
                                                      limit_content,
                                                      limits_expanded,
                                                      QStringLiteral("limits_expanded")));

    auto* coordinate_content = new QWidget(dialog);
    auto* coordinate_content_layout = new QVBoxLayout(coordinate_content);
    coordinate_content_layout->setContentsMargins(0, 0, 0, 0);
    coordinate_content_layout->setSpacing(S(8));
    coordinate_content_layout->addWidget(coordinate_enabled_check);
    coordinate_content_layout->addWidget(coordinate_card);
    coordinate_content_layout->addWidget(coordinate_status_label);
    scroll_layout->addWidget(CreateCollapsibleSection(dialog,
                                                      QStringLiteral("坐标转换"),
                                                      coordinate_content,
                                                      coordinate_expanded,
                                                      QStringLiteral("coordinate_expanded")));

    layout->addWidget(scroll_area, 1);
    scroll_content->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    scroll_area->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto* bottom_row = new QHBoxLayout();
    bottom_row->addWidget(status_label, 1);
    bottom_row->addWidget(validate_transform_button);
    bottom_row->addWidget(save_settings_button);
    bottom_row->addWidget(load_button);
    layout->addLayout(bottom_row);

    auto calibration_base_offset = std::make_shared<double>(angle_offset_spin->value());
    QObject::connect(angle_reference_current_spin,
                     QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                     dialog,
                     [angle_offset_spin, calibration_base_offset](double) {
        *calibration_base_offset = angle_offset_spin->value();
    });

    QObject::connect(capture_current_angle_button, &QPushButton::clicked, dialog, [owner,
                                                                          angle_reference_current_spin,
                                                                          angle_offset_spin,
                                                                          angle_direction_combo,
                                                                          angle_range_combo,
                                                                          calibration_base_offset]() {
        const int primary_index = owner->current_result_.primary_index;
        if (primary_index < 0 || primary_index >= static_cast<int>(owner->current_result_.detections.size())) {
            QMessageBox::warning(owner,
                                 QStringLiteral("无当前角度"),
                                 QStringLiteral("当前没有有效主目标，请先完成一次识别后再取当前角度。"));
            return;
        }
        const double raw_angle = owner->current_result_.detections[primary_index].angle_deg;
        const auto range_mode = static_cast<AngleRangeMode>(angle_range_combo->currentData().toInt());
        PlcOutputConfig config = owner->plcOutputConfig();
        config.angle_offset_deg = static_cast<float>(angle_offset_spin->value());
        config.angle_reverse_direction = angle_direction_combo->currentData().toBool();
        config.angle_range_mode = range_mode;
        const double display_angle = ApplyPlcAngleCalibration(static_cast<float>(raw_angle), config);
        *calibration_base_offset = angle_offset_spin->value();
        angle_reference_current_spin->setValue(display_angle);
        owner->updateStatusMessage(QStringLiteral("已取当前显示角度：%1 deg").arg(QString::number(display_angle, 'f', 2)), 5000);
    });

    QObject::connect(calculate_angle_offset_button, &QPushButton::clicked, dialog, [owner,
                                                                           angle_reference_current_spin,
                                                                           angle_reference_target_spin,
                                                                           angle_offset_spin,
                                                                           angle_direction_combo,
                                                                           angle_range_combo,
                                                                           calibration_base_offset]() {
        const double current_display_angle = angle_reference_current_spin->value();
        const double target_angle = angle_reference_target_spin->value();
        double offset = *calibration_base_offset + target_angle - current_display_angle;
        offset = std::fmod(offset, 360.0);
        if (offset > 360.0) {
            offset -= 360.0;
        } else if (offset < -360.0) {
            offset += 360.0;
        }
        angle_offset_spin->setValue(offset);

        const auto range_mode = static_cast<AngleRangeMode>(angle_range_combo->currentData().toInt());
        const bool reverse_direction = angle_direction_combo->currentData().toBool();
        float final_angle = static_cast<float>(target_angle);
        const int primary_index = owner->current_result_.primary_index;
        if (primary_index >= 0 && primary_index < static_cast<int>(owner->current_result_.detections.size())) {
            PlcOutputConfig config = owner->plcOutputConfig();
            config.angle_offset_deg = static_cast<float>(offset);
            config.angle_reverse_direction = reverse_direction;
            config.angle_range_mode = range_mode;
            final_angle = ApplyPlcAngleCalibration(owner->current_result_.detections[primary_index].angle_deg, config);
        }
        owner->updateStatusMessage(QStringLiteral("角度校准已计算：偏移 %1 deg，D%2 将从 %3 修正到 %4 deg")
                                .arg(QString::number(offset, 'f', 2))
                                .arg(owner->robot_controller_.plcRegisterMap().angle)
                                .arg(QString::number(current_display_angle, 'f', 2))
                                .arg(QString::number(final_angle, 'f', 2)),
                            7000);
    });

    QObject::connect(save_admin_button, &QPushButton::clicked, dialog, [owner,
                                                               admin_current_password_edit,
                                                               admin_username_edit,
                                                               admin_new_password_edit,
                                                               admin_confirm_password_edit]() {
        if (admin_new_password_edit->text() != admin_confirm_password_edit->text()) {
            QMessageBox::warning(owner,
                                 QStringLiteral("保存失败"),
                                 QStringLiteral("两次输入的新密码不一致。"));
            return;
        }

        QString error_message;
        if (!owner->changeAdminCredentials(admin_current_password_edit->text(),
                                           admin_username_edit->text(),
                                           admin_new_password_edit->text(),
                                           &error_message)) {
            QMessageBox::warning(owner, QStringLiteral("保存失败"), error_message);
            return;
        }

        owner->saveRememberedAdminPassword(false, QString());
        admin_current_password_edit->clear();
        admin_new_password_edit->clear();
        admin_confirm_password_edit->clear();
        owner->updateStatusMessage(QStringLiteral("管理员账号已更新，请使用新密码登录。"), 5000);
        QMessageBox::information(owner,
                                 QStringLiteral("保存成功"),
                                 QStringLiteral("管理员账号已更新。"));
    });

    QObject::connect(auto_exposure_button, &QPushButton::clicked, dialog, [owner,
                                                                  dialog,
                                                                  camera_ip_edits,
                                                                  exposure_spin,
                                                                  auto_exposure_button,
                                                                  status_label]() {
        if (owner->plc_runtime_state_.active()) {
            QMessageBox::warning(owner,
                                 QStringLiteral("抓取运行中"),
                                 QStringLiteral("请先停止 PLC 抓取，再执行单次自动曝光。"));
            return;
        }

        QString camera_ip;
        QString camera_ip_error;
        if (!BuildCameraIpFromEdits(camera_ip_edits, &camera_ip, &camera_ip_error)) {
            QMessageBox::warning(owner,
                                 QStringLiteral("相机 IP 错误"),
                                 camera_ip_error);
            return;
        }

        const bool restore_camera = owner->workflow_.isCameraRunning();
        if (restore_camera) {
            owner->workflow_.stopCamera();
            owner->camera_ui_update_pending_ = false;
            owner->refreshDeviceStatus();
            owner->refreshRuntimeStrip();
        }

        owner->camera_ip_ = camera_ip;
        owner->workflow_.setCameraIp(owner->camera_ip_.toStdString());
        auto_exposure_button->setEnabled(false);
        status_label->setText(QStringLiteral("正在执行单次自动曝光..."));
        owner->updateStatusMessage(QStringLiteral("正在执行海康相机单次自动曝光..."), 5000);

        auto* watcher = new QFutureWatcher<QString>(owner);
        auto measured_exposure = std::make_shared<double>(0.0);
        const QPointer<QDialog> safe_dialog(dialog);
        const QPointer<QDoubleSpinBox> safe_exposure_spin(exposure_spin);
        const QPointer<QPushButton> safe_auto_exposure_button(auto_exposure_button);
        const QPointer<QLabel> safe_status_label(status_label);
        QObject::connect(watcher, &QFutureWatcher<QString>::finished, owner, [owner,
                                                                    watcher,
                                                                    measured_exposure,
                                                                    restore_camera,
                                                                    safe_dialog,
                                                                    safe_exposure_spin,
                                                                    safe_auto_exposure_button,
                                                                    safe_status_label]() {
            const QString error_message = watcher->result();
            watcher->deleteLater();
            if (safe_auto_exposure_button) {
                safe_auto_exposure_button->setEnabled(true);
            }
            if (!safe_dialog) {
                return;
            }

            if (!error_message.isEmpty()) {
                if (safe_status_label) {
                    safe_status_label->setText(BuildModelStatusText(owner->workflow_));
                }
                QMessageBox::critical(owner, QStringLiteral("单次自动曝光失败"), error_message);
                if (restore_camera) {
                    owner->openCamera();
                }
                return;
            }

            owner->camera_exposure_us_ = *measured_exposure;
            owner->workflow_.setCameraExposureUs(owner->camera_exposure_us_);
            owner->saveCameraSettings();
            if (safe_exposure_spin) {
                safe_exposure_spin->setValue(owner->camera_exposure_us_);
            }
            if (safe_status_label) {
                safe_status_label->setText(QStringLiteral("单次自动曝光完成：%1 us")
                                               .arg(QString::number(owner->camera_exposure_us_, 'f', 1)));
            }
            owner->updateStatusMessage(QStringLiteral("单次自动曝光完成，曝光值已保存：%1 us")
                                    .arg(QString::number(owner->camera_exposure_us_, 'f', 1)),
                                6000);
            if (restore_camera) {
                owner->openCamera();
            }
        });
        watcher->setFuture(QtConcurrent::run([owner, measured_exposure]() -> QString {
            std::string error_message;
            double exposure_us = 0.0;
            if (!owner->workflow_.autoExposeOnce(&exposure_us, &error_message)) {
                return QString::fromStdString(error_message);
            }
            *measured_exposure = exposure_us;
            return QString();
        }));
    });

    QObject::connect(save_settings_button, &QPushButton::clicked, dialog, [owner,
                                                                  camera_ip_edits,
                                                                  exposure_spin,
                                                                  angle_offset_spin,
                                                                  center_ray_offset_spin,
                                                                  show_plc_center_debug_check,
                                                                  postprocess_debug_logging_check,
                                                                  angle_direction_combo,
                                                                  angle_range_combo,
                                                                  axis_mapping_combo,
                                                                  front_back_offset_spin,
                                                                  left_right_offset_spin,
                                                                  limit_enabled_check,
                                                                  x_lower_spin,
                                                                  x_upper_spin,
                                                                  y_lower_spin,
                                                                  y_upper_spin,
                                                                  angle_lower_spin,
                                                                  angle_upper_spin,
                                                                  roi_margin_spin,
                                                                  coordinate_enabled_check,
                                                                  image_x_edits,
                                                                  image_y_edits,
                                                                  machine_x_edits,
                                                                  machine_y_edits,
                                                                  settings_controls,
                                                                  coordinate_status_label,
                                                                  refresh_profile_combo]() {
        EngineeringSettingsDraft draft;
        EngineeringSettingsValidationError validation_error;
        if (!ReadEngineeringSettingsDraft(settings_controls, &draft, &validation_error)) {
            QMessageBox::warning(owner, validation_error.title, validation_error.message);
            return;
        }

        owner->coordinate_transform_config_ = draft.coordinate_transform_config;
        owner->rebuildCoordinateTransformState();
        RefreshCoordinateStatusLabel(coordinate_status_label, owner->coordinate_transform_state_);
        if (owner->coordinate_transform_state_.enabled && !owner->coordinate_transform_state_.valid) {
            QMessageBox::warning(owner,
                                 QStringLiteral("坐标转换无效"),
                                 QString::fromStdString(owner->coordinate_transform_state_.error_message));
            return;
        }

        owner->applyEngineeringSettingsDraft(draft);
        owner->saveLimitSettings();
        owner->saveCameraSettings();
        owner->saveModelThresholdSettings();
        owner->saveAngleCalibrationSettings();
        owner->saveObbPostprocessSettings();
        owner->saveAxisMappingSettings();
        owner->saveAxisCompensationSettings();
        QString save_error;
        if (!owner->saveCoordinateTransformSettings(&save_error)) {
            QMessageBox::warning(owner,
                                 QStringLiteral("保存失败"),
                                 save_error);
            return;
        }
        refresh_profile_combo();
        owner->refreshResultPresentation(true);
        owner->updateStatusMessage(limit_enabled_check->isChecked()
                                ? QStringLiteral("设置已保存，上下限保护已启用；模型阈值重新加载模型后生效。")
                                : QStringLiteral("设置已保存，上下限保护已关闭；模型阈值重新加载模型后生效。"),
                            5000);
        QMessageBox::information(owner,
                                 QStringLiteral("保存成功"),
                                 QStringLiteral("工程设置已保存。模型阈值需要重新加载模型后生效。"));
    });

    QObject::connect(validate_transform_button, &QPushButton::clicked, dialog, [owner,
                                                                       profile_combo,
                                                                       coordinate_status_label,
                                                                       read_transform_config_from_editor]() {
        CoordinateTransformConfig next_transform_config;
        QString calibration_error;
        const QString profile_name = profile_combo->currentText().trimmed();
        if (!read_transform_config_from_editor(profile_name, &next_transform_config, &calibration_error)) {
            QMessageBox::warning(owner, QStringLiteral("坐标转换错误"), calibration_error);
            return;
        }

        owner->coordinate_transform_config_ = next_transform_config;
        owner->rebuildCoordinateTransformState();
        RefreshCoordinateStatusLabel(coordinate_status_label, owner->coordinate_transform_state_);
        if (owner->coordinate_transform_state_.enabled && !owner->coordinate_transform_state_.valid) {
            QMessageBox::warning(owner,
                                 QStringLiteral("坐标转换无效"),
                                 QString::fromStdString(owner->coordinate_transform_state_.error_message));
        } else {
            owner->updateStatusMessage(CoordinateTransformStatusText(owner->coordinate_transform_state_), 5000);
        }
    });

    QObject::connect(apply_profile_button, &QPushButton::clicked, dialog, [owner,
                                                                  profile_combo,
                                                                  coordinate_enabled_check,
                                                                  image_x_edits,
                                                                  image_y_edits,
                                                                  machine_x_edits,
                                                                  machine_y_edits,
                                                                  coordinate_status_label,
                                                                  refresh_profile_combo,
                                                                  populate_coordinate_fields]() {
        const QString profile_name = profile_combo->currentText().trimmed();
        if (profile_name.isEmpty()) {
            QMessageBox::warning(owner, QStringLiteral("方案为空"), QStringLiteral("请选择一个标定方案。"));
            return;
        }

        CoordinateTransformConfig loaded_config;
        QString load_error;
        if (!LoadCalibrationProfile(profile_name, &loaded_config, &load_error)) {
            QMessageBox::warning(owner, QStringLiteral("加载失败"), load_error);
            return;
        }

        owner->coordinate_transform_config_ = loaded_config;
        owner->rebuildCoordinateTransformState();
        coordinate_enabled_check->setChecked(owner->coordinate_transform_config_.enabled);
        populate_coordinate_fields(image_x_edits, image_y_edits, machine_x_edits, machine_y_edits);
        RefreshCoordinateStatusLabel(coordinate_status_label, owner->coordinate_transform_state_);

        EngineeringSettingsService::SaveLastCoordinateProfile(profile_name);

        refresh_profile_combo();
        owner->updateStatusMessage(QStringLiteral("已应用标定方案：%1").arg(profile_name), 5000);
    });

    QObject::connect(profile_combo,
            static_cast<void (QComboBox::*)(int)>(&QComboBox::activated),
            dialog,
            [apply_profile_button](int) {
        if (apply_profile_button != nullptr) {
            apply_profile_button->click();
        }
    });

    QObject::connect(save_as_profile_button, &QPushButton::clicked, dialog, [owner,
                                                                    profile_combo,
                                                                    coordinate_status_label,
                                                                    read_transform_config_from_editor,
                                                                    refresh_profile_combo]() {
        bool ok = false;
        const QString default_name = profile_combo->currentText().trimmed().isEmpty()
                                          ? DefaultCalibrationProfileName()
                                          : profile_combo->currentText().trimmed();
        const QString profile_name = QInputDialog::getText(owner,
                                                           QStringLiteral("另存为标定方案"),
                                                           QStringLiteral("方案名称"),
                                                           QLineEdit::Normal,
                                                           default_name,
                                                           &ok).trimmed();
        if (!ok || profile_name.isEmpty()) {
            return;
        }

        CoordinateTransformConfig next_transform_config;
        QString calibration_error;
        if (!read_transform_config_from_editor(profile_name, &next_transform_config, &calibration_error)) {
            QMessageBox::warning(owner, QStringLiteral("坐标转换错误"), calibration_error);
            return;
        }

        owner->coordinate_transform_config_ = next_transform_config;
        owner->rebuildCoordinateTransformState();
        RefreshCoordinateStatusLabel(coordinate_status_label, owner->coordinate_transform_state_);
        if (owner->coordinate_transform_state_.enabled && !owner->coordinate_transform_state_.valid) {
            QMessageBox::warning(owner,
                                 QStringLiteral("坐标转换无效"),
                                 QString::fromStdString(owner->coordinate_transform_state_.error_message));
            return;
        }

        QString save_error;
        if (!owner->saveCoordinateTransformSettings(&save_error)) {
            QMessageBox::warning(owner, QStringLiteral("保存失败"), save_error);
            return;
        }
        refresh_profile_combo();
        owner->updateStatusMessage(QStringLiteral("已保存为标定方案：%1").arg(profile_name), 5000);
    });

    QObject::connect(delete_profile_button, &QPushButton::clicked, dialog, [owner,
                                                                   profile_combo,
                                                                   coordinate_enabled_check,
                                                                   image_x_edits,
                                                                   image_y_edits,
                                                                   machine_x_edits,
                                                                   machine_y_edits,
                                                                   coordinate_status_label,
                                                                   apply_transform_config_to_editor]() {
        const QString profile_name = profile_combo->currentText().trimmed();
        if (profile_name.isEmpty()) {
            return;
        }

        const auto choice = QMessageBox::question(owner,
                                                  QStringLiteral("删除标定方案"),
                                                  QStringLiteral("确定删除方案“%1”吗？").arg(profile_name),
                                                  QMessageBox::Yes | QMessageBox::No,
                                                  QMessageBox::No);
        if (choice != QMessageBox::Yes) {
            return;
        }

        QString delete_error;
        if (!DeleteCalibrationProfile(profile_name, &delete_error)) {
            QMessageBox::warning(owner, QStringLiteral("删除失败"), delete_error);
            return;
        }

        const QStringList profiles = ListCalibrationProfileNames();
        if (!profiles.isEmpty()) {
            CoordinateTransformConfig loaded_config;
            QString load_error;
            const QString next_profile = profiles.first();
            if (LoadCalibrationProfile(next_profile, &loaded_config, &load_error)) {
                apply_transform_config_to_editor(loaded_config);
                EngineeringSettingsService::SaveLastCoordinateProfile(next_profile);
            }
        } else {
            CoordinateTransformConfig blank_config;
            blank_config.profile_name = DefaultCalibrationProfileName();
            blank_config.points.resize(9);
            apply_transform_config_to_editor(blank_config);
            EngineeringSettingsService::SaveLastCoordinateProfile(blank_config.profile_name);
        }
        owner->updateStatusMessage(QStringLiteral("已删除标定方案：%1").arg(profile_name), 5000);
    });

    QObject::connect(load_button, &QPushButton::clicked, dialog, [owner, obb_path_edit, seg_path_edit]() {
        owner->loadModelsFromPathsAsync(obb_path_edit->text(),
                                 seg_path_edit->text(),
                                 true,
                                 true);
    });

    const QPoint center = owner->geometry().center() - QRect(QPoint(0, 0), dialog->size()).center();
    dialog->move(center);
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

