#include "engineering_settings_dialog_controller.h"

#include "grasp_main_window.h"

#include "admin_auth_helpers.h"
#include "calibration_profile_store.h"
#include "engineering_settings_dialog_helpers.h"
#include "engineering_settings_service.h"
#include "project_profile_store.h"
#include "ui_scale_utils.h"

#include <QtConcurrent/QtConcurrent>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QFutureWatcher>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
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
#include <QSpacerItem>
#include <QSpinBox>
#include <QStringList>
#include <QTimer>
#include <QToolButton>
#include <QtGlobal>
#include <QVBoxLayout>
#include <QWidget>

#include <cmath>
#include <functional>
#include <memory>
#include <string>
#include <array>

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

double NormalizeOldAngleOffset(double angle_deg, AngleRangeMode range_mode)
{
    double value = std::fmod(angle_deg, 360.0);
    if (range_mode == AngleRangeMode::Signed180) {
        if (value < -180.0) {
            value += 360.0;
        } else if (value >= 180.0) {
            value -= 360.0;
        }
    }
    return value;
}

bool PromptNamedValue(QWidget* parent,
                      const QString& title,
                      const QString& label_text,
                      const QString& default_name,
                      QString* value)
{
    if (value == nullptr) {
        return false;
    }

    QDialog dialog(parent);
    dialog.setWindowTitle(title);
    dialog.setStyleSheet(QString(
        "QDialog { background: #f4f6f8; }"
        "QLabel { color: #111111; font-size: %1px; }"
        "QLineEdit { background: #ffffff; color: #111111; border: 1px solid #8fa0ae; border-radius: %2px; padding: %3px %4px; font-size: %1px; min-height: %5px; }"
        "QPushButton { background: #ffffff; color: #111111; border: 1px solid #9aa9b7; border-radius: %2px; padding: %3px %6px; font-size: %1px; font-weight: 600; min-height: %5px; }"
        "QPushButton:hover { background: #e9eef3; }")
        .arg(S(15))
        .arg(S(8))
        .arg(S(6))
        .arg(S(8))
        .arg(S(32))
        .arg(S(14)));

    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(SM(16, 16, 16, 16));
    layout->setSpacing(S(10));
    auto* label = new QLabel(label_text, &dialog);
    auto* name_edit = new QLineEdit(default_name, &dialog);
    name_edit->selectAll();
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("确定"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    layout->addWidget(label);
    layout->addWidget(name_edit);
    layout->addWidget(buttons);

    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    *value = name_edit->text().trimmed();
    return !value->isEmpty();
}

bool PromptCalibrationProfileName(QWidget* parent, const QString& default_name, QString* profile_name)
{
    return PromptNamedValue(parent,
                            QStringLiteral("另存为标定方案"),
                            QStringLiteral("方案名称"),
                            default_name,
                            profile_name);
}

bool PromptProjectProfileName(QWidget* parent,
                              const QString& title,
                              const QString& default_name,
                              QString* profile_name)
{
    return PromptNamedValue(parent,
                            title,
                            QStringLiteral("工程方案名称"),
                            default_name,
                            profile_name);
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
    const bool startup_expanded = ui_settings.value(QStringLiteral("startup_expanded"), true).toBool();
    const bool debug_expanded = ui_settings.value(QStringLiteral("debug_expanded"), false).toBool();
    const bool limits_expanded = ui_settings.value(QStringLiteral("limits_expanded"), false).toBool();
    const bool coordinate_expanded = ui_settings.value(QStringLiteral("coordinate_expanded"), false).toBool();
    const bool admin_expanded = ui_settings.value(QStringLiteral("admin_expanded"), false).toBool();
    ui_settings.endGroup();

    auto* obb_path_edit = new QLineEdit(owner->obb_model_path_, dialog);
    obb_path_edit->setPlaceholderText(QStringLiteral("选择模型文件 A"));
    auto* seg_path_edit = new QLineEdit(owner->seg_model_path_, dialog);
    seg_path_edit->setPlaceholderText(QStringLiteral("选择模型文件 B"));
    auto* select_model_folder_button = new QPushButton(QStringLiteral("选择模型文件夹"), dialog);
    auto* model_folder_row = new QHBoxLayout();
    model_folder_row->setSpacing(S(8));
    model_folder_row->addWidget(new QLabel(QStringLiteral("快捷选择"), dialog));
    model_folder_row->addWidget(select_model_folder_button);
    model_folder_row->addStretch(1);
    QObject::connect(select_model_folder_button, &QPushButton::clicked, dialog,
                     [owner, dialog, obb_path_edit, seg_path_edit]() {
                         const QString folder_path = QFileDialog::getExistingDirectory(
                             dialog,
                             QStringLiteral("选择模型文件夹"),
                             QFileInfo(obb_path_edit->text()).absolutePath());
                         if (folder_path.isEmpty()) {
                             return;
                         }
                         const StandardModelFolderSelection selection =
                             DetectStandardModelFolder(folder_path);
                         if (!selection.complete()) {
                             QMessageBox::warning(
                                 dialog,
                                 QStringLiteral("模型文件夹不完整"),
                                 QStringLiteral("未找到完整的标准模型文件：\n%1")
                                     .arg(selection.missing_files.join(QStringLiteral("\n"))));
                             return;
                         }
                         obb_path_edit->setText(selection.obb_xml_path);
                         seg_path_edit->setText(selection.seg_xml_path);
                         owner->updateStatusMessage(
                             QStringLiteral("已识别模型 A/B，请保存设置后应用方案。"), 5000);
                     });
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
    auto* angle_direction_combo = CreateClickFocusedComboBox(dialog);
    angle_direction_combo->addItem(QStringLiteral("正向"), false);
    angle_direction_combo->addItem(QStringLiteral("反向"), true);
    angle_direction_combo->setCurrentIndex(owner->angle_reverse_direction_ ? 1 : 0);
    angle_direction_combo->setFixedWidth(S(170));
    auto* angle_range_combo = CreateClickFocusedComboBox(dialog);
    angle_range_combo->addItem(QStringLiteral("0~360"), static_cast<int>(AngleRangeMode::ZeroTo360));
    angle_range_combo->addItem(QStringLiteral("-180~180"), static_cast<int>(AngleRangeMode::Signed180));
    angle_range_combo->setCurrentIndex(owner->angle_range_mode_ == AngleRangeMode::Signed180 ? 1 : 0);
    angle_range_combo->setFixedWidth(S(170));
    auto* axis_mapping_combo = CreateClickFocusedComboBox(dialog);
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
    auto* capture_current_angle_button = new QPushButton(QStringLiteral("取当前角度"), dialog);
    auto* calculate_angle_offset_button = new QPushButton(QStringLiteral("计算校准"), dialog);
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
    postprocess_debug_logging_check->setToolTip(QStringLiteral("在日志中输出 [PostprocessDebug] 明细，包含 O/A/B/C、AC角度、B点选择和可抓/拒抓原因。"));
    auto* show_head_type_adjusted_geometry_check =
        new QCheckBox(QStringLiteral("显示类型补偿后的夹取框"), dialog);
    show_head_type_adjusted_geometry_check->setChecked(owner->show_head_type_adjusted_geometry_);
    show_head_type_adjusted_geometry_check->setToolTip(QStringLiteral("关闭后主画面的夹取框、检测框和 AC 箭头保持原始视觉检测方向；类型角度补偿仍只作用于 PLC 输出。"));
    auto* auto_start_check = new QCheckBox(QStringLiteral("开机自启"), dialog);
    auto_start_check->setChecked(owner->auto_start_enabled_);
    auto_start_check->setToolTip(QStringLiteral("在当前用户 Startup 文件夹中创建或移除 TankEye-Iris 启动入口。"));
    auto* startup_delay_spin = CreateStartupDelaySpinBox(dialog, owner->startup_delay_seconds_);
    startup_delay_spin->setToolTip(QStringLiteral("仅开机自启时生效；等待现场网络、相机和 PLC 服务完成初始化后再启动。"));
    auto* mechanical_gripper_length_spin = CreateGripperDimensionSpinBox(dialog, owner->mechanical_gripper_length_);
    mechanical_gripper_length_spin->setToolTip(QStringLiteral("真实夹爪机械长度；为 0 时保守判为不可抓。"));
    auto* mechanical_gripper_width_spin = CreateGripperDimensionSpinBox(dialog, owner->mechanical_gripper_width_);
    mechanical_gripper_width_spin->setToolTip(QStringLiteral("真实夹爪机械宽度；为 0 时保守判为不可抓。"));
    struct HeadTypeRow
    {
        int code;
        const char* name;
    };
    const std::array<HeadTypeRow, 4> head_type_rows = {{
        { 1, "大上右" },
        { 3, "大上左" },
        { 4, "小上右" },
        { 2, "小上左" },
    }};
    std::array<QDoubleSpinBox*, 5> head_type_angle_offset_spins{};
    std::array<QDoubleSpinBox*, 5> head_type_ac_ray_offset_spins{};
    auto* head_type_compensation_grid = CreateSettingsGrid(10, 10);
    SetGridColumnMinimumWidths(head_type_compensation_grid, { 120, 90, 130, 140 });
    SetGridColumnStretches(head_type_compensation_grid, { 0, 0, 0, 0, 1 });
    head_type_compensation_grid->addWidget(new QLabel(QStringLiteral("类型"), dialog), 0, 0);
    head_type_compensation_grid->addWidget(new QLabel(QStringLiteral("类型码"), dialog), 0, 1);
    head_type_compensation_grid->addWidget(new QLabel(QStringLiteral("角度补偿"), dialog), 0, 2);
    head_type_compensation_grid->addWidget(new QLabel(QStringLiteral("AC 偏移"), dialog), 0, 3);
    for (int row = 0; row < static_cast<int>(head_type_rows.size()); ++row) {
        const HeadTypeRow row_info = head_type_rows[static_cast<size_t>(row)];
        head_type_compensation_grid->addWidget(new QLabel(QString::fromUtf8(row_info.name), dialog), row + 1, 0);
        head_type_compensation_grid->addWidget(new QLabel(QString::number(row_info.code), dialog), row + 1, 1);
        auto* angle_spin = CreateAngleOffsetSpinBox(
            dialog, owner->head_type_compensation_settings_.types[row_info.code].angle_offset_deg);
        angle_spin->setToolTip(QStringLiteral("该类型额外角度补偿，叠加全局角度校准。"));
        auto* ac_offset_spin = CreateLimitSpinBox(
            dialog, owner->head_type_compensation_settings_.types[row_info.code].ac_ray_offset_mm);
        ac_offset_spin->setToolTip(QStringLiteral("沿 A->C 方向的机械毫米偏移，正值朝 C。"));
        head_type_angle_offset_spins[row_info.code] = angle_spin;
        head_type_ac_ray_offset_spins[row_info.code] = ac_offset_spin;
        head_type_compensation_grid->addWidget(angle_spin, row + 1, 2, Qt::AlignLeft);
        head_type_compensation_grid->addWidget(ac_offset_spin, row + 1, 3, Qt::AlignLeft);
    }
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
    angle_debug_grid->addWidget(create_angle_label(QStringLiteral("当前角度：")), 0, 0);
    angle_debug_grid->addWidget(angle_reference_current_spin, 0, 1);
    angle_debug_grid->addWidget(capture_current_angle_button, 0, 2, Qt::AlignLeft);
    angle_debug_grid->addWidget(create_angle_label(QStringLiteral("机械目标角度：")), 1, 0);
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
    auto* show_grab_limit_overlay_check = new QCheckBox(QStringLiteral("显示保护区域"), dialog);
    show_grab_limit_overlay_check->setChecked(owner->show_grab_limit_overlay_);
    show_grab_limit_overlay_check->setToolTip(QStringLiteral("在主画面叠加显示扣除 ROI 留边后的最终可抓区域；不影响 PLC 或过滤逻辑。"));
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
    auto* profile_combo = CreateClickFocusedComboBox(dialog);
    profile_combo->setFixedWidth(S(170));
    auto* apply_profile_button = new QPushButton(QStringLiteral("应用"), dialog);
    auto* import_profile_button = new QPushButton(QStringLiteral("导入"), dialog);
    auto* save_as_profile_button = new QPushButton(QStringLiteral("另存为"), dialog);
    auto* delete_profile_button = new QPushButton(QStringLiteral("删除"), dialog);
    QList<QDoubleSpinBox*> image_x_edits;
    QList<QDoubleSpinBox*> image_y_edits;
    QList<QDoubleSpinBox*> machine_x_edits;
    QList<QDoubleSpinBox*> machine_y_edits;
    auto* coordinate_card = new QWidget(dialog);
    coordinate_card->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    coordinate_card->setMaximumWidth(S(660));
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
    profile_row->addWidget(import_profile_button);
    profile_row->addWidget(save_as_profile_button);
    profile_row->addWidget(delete_profile_button);
    profile_row->addStretch(1);
    coordinate_card_layout->addLayout(profile_row);

    auto* coordinate_grid = CreateSettingsGrid(8, 8);
    SetGridColumnMinimumWidths(coordinate_grid, { 42 });
    SetGridColumnStretches(coordinate_grid, { 0, 0, 0, 0, 0, 1 });
    coordinate_grid->addWidget(new QLabel(QStringLiteral("点位"), dialog), 0, 0);
    coordinate_grid->addWidget(new QLabel(QStringLiteral("图像X"), dialog), 0, 1);
    coordinate_grid->addWidget(new QLabel(QStringLiteral("图像Y"), dialog), 0, 2);
    coordinate_grid->addWidget(new QLabel(QStringLiteral("机械X"), dialog), 0, 3);
    coordinate_grid->addWidget(new QLabel(QStringLiteral("机械Y"), dialog), 0, 4);
    coordinate_grid->addItem(new QSpacerItem(S(1), S(1), QSizePolicy::Expanding, QSizePolicy::Minimum), 0, 5, 10, 1);

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

    const auto populate_coordinate_fields = [](const CoordinateTransformConfig& config,
                                               const QList<QDoubleSpinBox*>& image_x_edits,
                                               const QList<QDoubleSpinBox*>& image_y_edits,
                                               const QList<QDoubleSpinBox*>& machine_x_edits,
                                               const QList<QDoubleSpinBox*>& machine_y_edits) {
        for (int row = 0; row < 9; ++row) {
            const CoordinateCalibrationPoint point =
                row < static_cast<int>(config.points.size())
                    ? config.points[row]
                    : CoordinateCalibrationPoint{};
            SetCoordinateValue(image_x_edits, row, point.image_x);
            SetCoordinateValue(image_y_edits, row, point.image_y);
            SetCoordinateValue(machine_x_edits, row, point.machine_x);
            SetCoordinateValue(machine_y_edits, row, point.machine_y);
        }
    };
    populate_coordinate_fields(owner->coordinate_transform_config_,
                               image_x_edits,
                               image_y_edits,
                               machine_x_edits,
                               machine_y_edits);
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
    settings_controls.obb_model_path_edit = obb_path_edit;
    settings_controls.seg_model_path_edit = seg_path_edit;
    settings_controls.exposure_spin = exposure_spin;
    settings_controls.obb_conf_threshold_spin = obb_conf_threshold_spin;
    settings_controls.obb_nms_threshold_spin = obb_nms_threshold_spin;
    settings_controls.seg_conf_threshold_spin = seg_conf_threshold_spin;
    settings_controls.seg_nms_threshold_spin = seg_nms_threshold_spin;
    settings_controls.angle_offset_spin = angle_offset_spin;
    settings_controls.center_ray_offset_spin = center_ray_offset_spin;
    settings_controls.show_head_type_adjusted_geometry_check = show_head_type_adjusted_geometry_check;
    settings_controls.show_plc_center_debug_check = show_plc_center_debug_check;
    settings_controls.show_head_ray_debug_check = show_head_ray_debug_check;
    settings_controls.postprocess_debug_logging_check = postprocess_debug_logging_check;
    settings_controls.auto_start_check = auto_start_check;
    settings_controls.startup_delay_spin = startup_delay_spin;
    settings_controls.mechanical_gripper_length_spin = mechanical_gripper_length_spin;
    settings_controls.mechanical_gripper_width_spin = mechanical_gripper_width_spin;
    settings_controls.head_type_angle_offset_spins = head_type_angle_offset_spins;
    settings_controls.head_type_ac_ray_offset_spins = head_type_ac_ray_offset_spins;
    settings_controls.angle_direction_combo = angle_direction_combo;
    settings_controls.angle_range_combo = angle_range_combo;
    settings_controls.axis_mapping_combo = axis_mapping_combo;
    settings_controls.front_back_offset_spin = front_back_offset_spin;
    settings_controls.left_right_offset_spin = left_right_offset_spin;
    settings_controls.limit_enabled_check = limit_enabled_check;
    settings_controls.show_grab_limit_overlay_check = show_grab_limit_overlay_check;
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
        populate_coordinate_fields(owner->coordinate_transform_config_,
                                   image_x_edits,
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
    auto* save_settings_button = new QPushButton(QStringLiteral("保存设置"), dialog);

    auto* project_profile_combo = CreateClickFocusedComboBox(dialog);
    project_profile_combo->setFixedWidth(S(190));
    auto* new_project_profile_button = new QPushButton(QStringLiteral("新建"), dialog);
    auto* save_as_project_profile_button = new QPushButton(QStringLiteral("另存为"), dialog);
    auto* delete_project_profile_button = new QPushButton(QStringLiteral("删除"), dialog);
    auto* import_project_profile_button = new QPushButton(QStringLiteral("导入"), dialog);
    auto* export_project_profile_button = new QPushButton(QStringLiteral("导出"), dialog);
    auto* apply_project_profile_button = new QPushButton(QStringLiteral("应用方案"), dialog);
    auto selected_project_profile_name =
        std::make_shared<QString>(owner->current_project_profile_name_);
    auto refresh_project_profile_combo = [owner, project_profile_combo, selected_project_profile_name]() {
        QSignalBlocker blocker(project_profile_combo);
        project_profile_combo->clear();
        project_profile_combo->addItems(ListProjectProfileNames());
        const int index = project_profile_combo->findText(*selected_project_profile_name);
        if (index >= 0) {
            project_profile_combo->setCurrentIndex(index);
        }
    };
    refresh_project_profile_combo();

    auto* project_profile_row = new QHBoxLayout();
    project_profile_row->setSpacing(S(8));
    project_profile_row->addWidget(new QLabel(QStringLiteral("当前工程方案"), dialog));
    project_profile_row->addWidget(project_profile_combo);
    project_profile_row->addWidget(new_project_profile_button);
    project_profile_row->addWidget(save_as_project_profile_button);
    project_profile_row->addWidget(delete_project_profile_button);
    project_profile_row->addWidget(import_project_profile_button);
    project_profile_row->addWidget(export_project_profile_button);
    project_profile_row->addWidget(apply_project_profile_button);
    project_profile_row->addStretch(1);

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
    auto* project_profile_content = new QWidget(dialog);
    auto* project_profile_layout = new QVBoxLayout(project_profile_content);
    project_profile_layout->setContentsMargins(0, 0, 0, 0);
    project_profile_layout->addLayout(project_profile_row);
    scroll_layout->insertWidget(0,
                                 CreateCollapsibleSection(dialog,
                                                          QStringLiteral("物料工程方案"),
                                                          project_profile_content,
                                                          true,
                                                          QStringLiteral("project_profile_expanded")));

    auto* basic_content = new QWidget(dialog);
    auto* basic_content_layout = new QVBoxLayout(basic_content);
    basic_content_layout->setContentsMargins(0, 0, 0, 0);
    basic_content_layout->setSpacing(S(8));
    basic_content_layout->addLayout(model_folder_row);
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

    auto* startup_content = new QWidget(dialog);
    auto* startup_content_layout = new QVBoxLayout(startup_content);
    startup_content_layout->setContentsMargins(0, 0, 0, 0);
    startup_content_layout->setSpacing(S(8));
    startup_content_layout->addWidget(auto_start_check);
    auto* startup_grid = CreateSettingsGrid(10, 8);
    SetGridColumnMinimumWidths(startup_grid, { 120, 170 });
    SetGridColumnStretches(startup_grid, { 0, 0, 1 });
    startup_grid->addWidget(new QLabel(QStringLiteral("延迟启动"), dialog), 0, 0);
    startup_grid->addWidget(startup_delay_spin, 0, 1, Qt::AlignLeft);
    startup_content_layout->addLayout(startup_grid);
    scroll_layout->addWidget(CreateCollapsibleSection(dialog,
                                                      QStringLiteral("启动设置"),
                                                      startup_content,
                                                      startup_expanded,
                                                      QStringLiteral("startup_expanded")));

    scroll_layout->addSpacing(S(4));

    auto* debug_content = new QWidget(dialog);
    auto* debug_content_layout = new QVBoxLayout(debug_content);
    debug_content_layout->setContentsMargins(0, 0, 0, 0);
    debug_content_layout->setSpacing(S(8));
    debug_content_layout->addWidget(show_head_type_adjusted_geometry_check);
    debug_content_layout->addWidget(show_head_ray_debug_check);
    debug_content_layout->addWidget(postprocess_debug_logging_check);
    auto* gripper_dimension_grid = CreateSettingsGrid(10, 10);
    SetGridColumnMinimumWidths(gripper_dimension_grid, { 120, 170 });
    SetGridColumnStretches(gripper_dimension_grid, { 0, 0, 0, 1 });
    gripper_dimension_grid->addWidget(new QLabel(QStringLiteral("夹爪长度"), dialog), 0, 0);
    gripper_dimension_grid->addWidget(mechanical_gripper_length_spin, 0, 1, Qt::AlignLeft);
    gripper_dimension_grid->addWidget(new QLabel(QStringLiteral("夹爪宽度"), dialog), 1, 0);
    gripper_dimension_grid->addWidget(mechanical_gripper_width_spin, 1, 1, Qt::AlignLeft);
    debug_content_layout->addLayout(gripper_dimension_grid);
    debug_content_layout->addWidget(CreateGroupCaption(QStringLiteral("类型补偿"), dialog));
    debug_content_layout->addLayout(head_type_compensation_grid);
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
    limit_content_layout->addWidget(show_grab_limit_overlay_check);

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
    bottom_row->addWidget(save_settings_button);
    bottom_row->addWidget(load_button);
    layout->addLayout(bottom_row);

    auto populate_project_profile_controls =
        [selected_project_profile_name,
         project_profile_combo,
         obb_path_edit,
         seg_path_edit,
         exposure_spin,
         obb_conf_threshold_spin,
         obb_nms_threshold_spin,
         seg_conf_threshold_spin,
         seg_nms_threshold_spin,
         angle_offset_spin,
         angle_direction_combo,
         angle_range_combo,
         center_ray_offset_spin,
         show_head_type_adjusted_geometry_check,
         show_plc_center_debug_check,
         show_head_ray_debug_check,
         postprocess_debug_logging_check,
         axis_mapping_combo,
         front_back_offset_spin,
         left_right_offset_spin,
         limit_enabled_check,
         show_grab_limit_overlay_check,
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
          coordinate_status_label,
          normalize_transform_config,
          populate_coordinate_fields,
          head_type_angle_offset_spins,
          head_type_ac_ray_offset_spins](const ProjectProfileSettings& profile) {
        QSignalBlocker offset_blocker(angle_offset_spin);
        QSignalBlocker direction_blocker(angle_direction_combo);
        QSignalBlocker range_blocker(angle_range_combo);
        *selected_project_profile_name = profile.name;
        {
            QSignalBlocker blocker(project_profile_combo);
            const int index = project_profile_combo->findText(profile.name);
            if (index >= 0) {
                project_profile_combo->setCurrentIndex(index);
            }
        }
        obb_path_edit->setText(profile.obb_model_path);
        seg_path_edit->setText(profile.seg_model_path);
        exposure_spin->setValue(profile.camera_exposure_us);
        obb_conf_threshold_spin->setValue(profile.model_thresholds.obb_conf_threshold);
        obb_nms_threshold_spin->setValue(profile.model_thresholds.obb_nms_threshold);
        seg_conf_threshold_spin->setValue(profile.model_thresholds.seg_conf_threshold);
        seg_nms_threshold_spin->setValue(profile.model_thresholds.seg_nms_threshold);
        angle_offset_spin->setValue(profile.angle_calibration.offset_deg);
        angle_direction_combo->setCurrentIndex(profile.angle_calibration.reverse_direction ? 1 : 0);
        angle_range_combo->setCurrentIndex(
            profile.angle_calibration.range_mode == AngleRangeMode::Signed180 ? 1 : 0);
        center_ray_offset_spin->setValue(profile.obb_postprocess.center_ray_offset_px);
        show_head_type_adjusted_geometry_check->setChecked(
            profile.obb_postprocess.show_head_type_adjusted_geometry);
        show_plc_center_debug_check->setChecked(profile.obb_postprocess.show_plc_center_debug);
        show_head_ray_debug_check->setChecked(profile.obb_postprocess.show_head_ray_debug);
        postprocess_debug_logging_check->setChecked(profile.obb_postprocess.debug_logging_enabled);
        axis_mapping_combo->setCurrentIndex(
            profile.axis_mapping_mode == AxisMappingMode::FrontBackMachineX ? 1 : 0);
        front_back_offset_spin->setValue(profile.axis_compensation.front_back_offset);
        left_right_offset_spin->setValue(profile.axis_compensation.left_right_offset);
        limit_enabled_check->setChecked(profile.grab_limits.enabled);
        show_grab_limit_overlay_check->setChecked(profile.ui_overlay.show_grab_limit_overlay);
        x_lower_spin->setValue(profile.grab_limits.x.lower);
        x_upper_spin->setValue(profile.grab_limits.x.upper);
        y_lower_spin->setValue(profile.grab_limits.y.lower);
        y_upper_spin->setValue(profile.grab_limits.y.upper);
        angle_lower_spin->setValue(profile.grab_limits.angle.lower);
        angle_upper_spin->setValue(profile.grab_limits.angle.upper);
        roi_margin_spin->setValue(profile.grab_limits.roi_margin);
        for (int head_type = 1;
             head_type < static_cast<int>(profile.head_type_compensation.types.size());
             ++head_type) {
            head_type_angle_offset_spins[head_type]->setValue(
                profile.head_type_compensation.types[head_type].angle_offset_deg);
            head_type_ac_ray_offset_spins[head_type]->setValue(
                profile.head_type_compensation.types[head_type].ac_ray_offset_mm);
        }
        coordinate_enabled_check->setChecked(profile.coordinate_transform.enabled);
        CoordinateTransformConfig coordinate = profile.coordinate_transform;
        normalize_transform_config(&coordinate);
        populate_coordinate_fields(coordinate,
                                   image_x_edits,
                                   image_y_edits,
                                   machine_x_edits,
                                   machine_y_edits);
        coordinate_status_label->setText(
            coordinate.enabled ? QStringLiteral("已载入坐标转换配置，点击“应用方案”时校验。")
                                : QStringLiteral("坐标转换未启用。"));
    };
    auto load_project_profile_draft = [owner, populate_project_profile_controls](
                                          const QString& profile_name) {
        ProjectProfileSettings profile;
        QString error_message;
        if (!LoadProjectProfile(profile_name, &profile, &error_message)) {
            QMessageBox::warning(owner, QStringLiteral("读取工程方案失败"), error_message);
            return false;
        }
        populate_project_profile_controls(profile);
        return true;
    };
    load_project_profile_draft(*selected_project_profile_name);

    QObject::connect(capture_current_angle_button, &QPushButton::clicked, dialog, [owner,
                                                                           angle_reference_current_spin,
                                                                           angle_direction_combo,
                                                                           angle_range_combo]() {
        const int primary_index = owner->current_result_.primary_index;
        if (primary_index < 0 || primary_index >= static_cast<int>(owner->current_result_.detections.size())) {
            QMessageBox::warning(owner,
                                 QStringLiteral("无当前角度"),
                                 QStringLiteral("当前没有有效主目标，请先完成一次识别后再取当前角度。"));
            return;
        }
        PlcOutputConfig config;
        config.angle_offset_deg = 0.0f;
        config.angle_reverse_direction = angle_direction_combo->currentData().toBool();
        config.angle_range_mode = static_cast<AngleRangeMode>(angle_range_combo->currentData().toInt());
        const double display_angle = ApplyPlcAngleCalibration(
            owner->current_result_.detections[primary_index].angle_deg,
            config);
        angle_reference_current_spin->setValue(display_angle);
        owner->updateStatusMessage(QStringLiteral("已取当前角度：%1 deg")
                                       .arg(QString::number(display_angle, 'f', 2)),
                                   5000);
    });

    QObject::connect(calculate_angle_offset_button, &QPushButton::clicked, dialog, [owner,
                                                                            angle_reference_current_spin,
                                                                            angle_reference_target_spin,
                                                                            angle_offset_spin,
                                                                            angle_range_combo]() {
        const auto range_mode = static_cast<AngleRangeMode>(angle_range_combo->currentData().toInt());
        const double offset = NormalizeOldAngleOffset(
            angle_reference_target_spin->value() - angle_reference_current_spin->value(),
            range_mode);
        angle_offset_spin->setValue(offset);
        owner->updateStatusMessage(QStringLiteral("角度校准已计算：角度偏移 %1 deg")
                                       .arg(QString::number(offset, 'f', 2)),
                                   5000);
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
                                                                  refresh_profile_combo,
                                                                  project_profile_combo,
                                                                  refresh_project_profile_combo,
                                                                  selected_project_profile_name]() {
        EngineeringSettingsDraft draft;
        EngineeringSettingsValidationError validation_error;
        if (!ReadEngineeringSettingsDraft(settings_controls, &draft, &validation_error)) {
            QMessageBox::warning(owner, validation_error.title, validation_error.message);
            return;
        }
        if (owner->models_loading_) {
            QMessageBox::warning(owner,
                                 QStringLiteral("模型正在加载"),
                                 QStringLiteral("模型加载完成后再保存工程方案。"));
            return;
        }

        QString save_error;
        const QString profile_name = selected_project_profile_name->trimmed();
        const ProjectProfileSettings profile_to_save =
            owner->projectProfileSettingsFromDraft(
                draft,
                profile_name);
        const bool is_active_profile =
            profile_name == owner->active_project_profile_name_.trimmed();
        if (profile_name.isEmpty() ||
            (is_active_profile &&
             !owner->validateProjectProfileModels(profile_to_save, &save_error)) ||
            !SaveProjectProfile(profile_to_save, nullptr, &save_error)) {
            QMessageBox::warning(owner,
                                 QStringLiteral("保存失败"),
                                 save_error);
            return;
        }
        owner->camera_ip_ = draft.camera_ip;
        owner->auto_start_enabled_ = draft.auto_start_enabled;
        owner->startup_delay_seconds_ = draft.startup_delay_seconds;
        owner->workflow_.setCameraIp(owner->camera_ip_.toStdString());
        EngineeringSettingsService::SaveCameraIp(owner->camera_ip_);
        QString startup_error;
        if (!owner->saveStartupLaunchSettings(&startup_error)) {
            QMessageBox::warning(owner, QStringLiteral("开机自启设置失败"), startup_error);
            return;
        }
        QString apply_error;
        if (is_active_profile &&
            !owner->applySavedEngineeringSettings(
                draft,
                profile_name,
                &apply_error)) {
            owner->updateStatusMessage(
                QStringLiteral("方案已保存，但当前运行方案未改变：%1").arg(apply_error),
                6000);
            QMessageBox::warning(owner,
                                 QStringLiteral("保存后应用失败"),
                                 QStringLiteral("方案文件已保存，但当前运行参数未改变。\n%1")
                                     .arg(apply_error));
            refresh_project_profile_combo();
            refresh_profile_combo();
            return;
        }
        refresh_project_profile_combo();
        refresh_profile_combo();
        const QString status_message = is_active_profile
            ? QStringLiteral("方案已保存，下一次开始检测/开始抓取时生效。\n当前运行仍使用旧参数，当前画面和检测结果未改变。")
            : QStringLiteral("工程方案已保存：%1，当前运行方案未改变。").arg(profile_name);
        owner->updateStatusMessage(status_message, 5000);
        QMessageBox::information(owner,
                                 QStringLiteral("保存成功"),
                                 status_message);
    });

    QObject::connect(project_profile_combo,
                     QOverload<int>::of(&QComboBox::activated),
                     dialog,
                     [project_profile_combo, apply_project_profile_button, load_project_profile_draft](int) {
        apply_project_profile_button->setEnabled(project_profile_combo->currentIndex() >= 0);
        if (project_profile_combo->currentIndex() >= 0) {
            load_project_profile_draft(project_profile_combo->currentText());
        }
    });

    QObject::connect(apply_project_profile_button, &QPushButton::clicked, dialog, [owner,
                                                                                     dialog,
                                                                                     project_profile_combo,
                                                                                     settings_controls]() {
        EngineeringSettingsDraft draft;
        EngineeringSettingsValidationError validation_error;
        if (!ReadEngineeringSettingsDraft(settings_controls, &draft, &validation_error)) {
            QMessageBox::warning(owner, validation_error.title, validation_error.message);
            return;
        }
        const QString profile_name = project_profile_combo->currentText().trimmed();
        QString save_error;
        if (!SaveProjectProfile(owner->projectProfileSettingsFromDraft(
                                    draft,
                                    profile_name),
                                nullptr,
                                &save_error)) {
            QMessageBox::warning(owner, QStringLiteral("保存方案失败"), save_error);
            return;
        }
        QString error_message;
        if (!owner->switchProjectProfile(profile_name, true, &error_message)) {
            QMessageBox::warning(owner, QStringLiteral("应用方案失败"), error_message);
            return;
        }
        dialog->close();
    });

    QObject::connect(save_as_project_profile_button, &QPushButton::clicked, dialog, [owner,
                                                                                       project_profile_combo,
                                                                                       settings_controls,
                                                                                       refresh_project_profile_combo,
                                                                                       selected_project_profile_name,
                                                                                       populate_project_profile_controls]() {
        EngineeringSettingsDraft draft;
        EngineeringSettingsValidationError validation_error;
        if (!ReadEngineeringSettingsDraft(settings_controls, &draft, &validation_error)) {
            QMessageBox::warning(owner, validation_error.title, validation_error.message);
            return;
        }
        QString profile_name;
        if (!PromptProjectProfileName(owner,
                                      QStringLiteral("另存为工程方案"),
                                      project_profile_combo->currentText(),
                                      &profile_name)) {
            return;
        }
        QString error_message;
        QString saved_name;
        if (!SaveProjectProfile(owner->projectProfileSettingsFromDraft(
                                    draft,
                                    profile_name),
                                &saved_name,
                                &error_message)) {
            QMessageBox::warning(owner, QStringLiteral("另存为失败"), error_message);
            return;
        }
        *selected_project_profile_name = saved_name;
        refresh_project_profile_combo();
        ProjectProfileSettings saved_profile;
        if (LoadProjectProfile(saved_name, &saved_profile, &error_message)) {
            populate_project_profile_controls(saved_profile);
        }
        owner->updateStatusMessage(QStringLiteral("已另存为工程方案：%1").arg(saved_name), 5000);
    });

    QObject::connect(new_project_profile_button, &QPushButton::clicked, dialog, [owner,
                                                                                   refresh_project_profile_combo,
                                                                                   selected_project_profile_name,
                                                                                   populate_project_profile_controls]() {
        QString profile_name;
        if (!PromptProjectProfileName(owner,
                                      QStringLiteral("新建工程方案"),
                                      QStringLiteral("DG_10"),
                                      &profile_name)) {
            return;
        }
        const QString safe_name = SanitizeProjectProfileName(profile_name);
        if (ListProjectProfileNames().contains(safe_name)) {
            QMessageBox::warning(owner, QStringLiteral("方案已存在"), QStringLiteral("请使用新的方案名称。"));
            return;
        }
        QString error_message;
        QString saved_name;
        if (!SaveProjectProfile(CreateBlankProjectProfile(safe_name), &saved_name, &error_message)) {
            QMessageBox::warning(owner, QStringLiteral("新建失败"), error_message);
            return;
        }
        *selected_project_profile_name = saved_name;
        refresh_project_profile_combo();
        ProjectProfileSettings blank_profile;
        if (!LoadProjectProfile(saved_name, &blank_profile, &error_message)) {
            QMessageBox::warning(owner, QStringLiteral("读取新方案失败"), error_message);
            return;
        }
        populate_project_profile_controls(blank_profile);
        owner->updateStatusMessage(
            QStringLiteral("已新建空白方案 %1，请配置模型和参数后应用。").arg(saved_name),
            6000);
    });

    QObject::connect(delete_project_profile_button, &QPushButton::clicked, dialog, [owner,
                                                                                      project_profile_combo,
                                                                                      refresh_project_profile_combo,
                                                                                      selected_project_profile_name,
                                                                                      load_project_profile_draft]() {
        const QString profile_name = project_profile_combo->currentText().trimmed();
        if (profile_name.isEmpty() || profile_name == DefaultProjectProfileName()) {
            QMessageBox::warning(owner, QStringLiteral("无法删除"), QStringLiteral("默认 DG_8 工程方案不能删除。"));
            return;
        }
        if (QMessageBox::question(owner,
                                  QStringLiteral("删除工程方案"),
                                  QStringLiteral("确定删除方案“%1”吗？").arg(profile_name),
                                  QMessageBox::Yes | QMessageBox::No,
                                  QMessageBox::No) != QMessageBox::Yes) {
            return;
        }
        QString error_message;
        if (!DeleteProjectProfile(profile_name, &error_message)) {
            QMessageBox::warning(owner, QStringLiteral("删除失败"), error_message);
            return;
        }
        refresh_project_profile_combo();
        const QString next_name = project_profile_combo->currentText().trimmed();
        *selected_project_profile_name = next_name;
        if (!next_name.isEmpty()) {
            load_project_profile_draft(next_name);
        }
        owner->updateStatusMessage(QStringLiteral("已删除工程方案：%1").arg(profile_name), 5000);
    });

    QObject::connect(import_project_profile_button, &QPushButton::clicked, dialog, [owner,
                                                                                      dialog,
                                                                                      project_profile_combo,
                                                                                      refresh_project_profile_combo,
                                                                                      selected_project_profile_name,
                                                                                      load_project_profile_draft]() {
        const QString path = QFileDialog::getOpenFileName(
            dialog,
            QStringLiteral("导入工程方案"),
            QString(),
            QStringLiteral("工程方案 (*.json *.zip);;JSON (*.json);;ZIP (*.zip);;All Files (*)"));
        if (path.isEmpty()) {
            return;
        }
        bool replaced = false;
        QString imported_name;
        QString error_message;
        if (!ImportProjectProfile(path, &replaced, &imported_name, &error_message)) {
            QMessageBox::warning(owner, QStringLiteral("导入失败"), error_message);
            return;
        }
        refresh_project_profile_combo();
        project_profile_combo->setCurrentText(imported_name);
        *selected_project_profile_name = imported_name;
        load_project_profile_draft(imported_name);
        owner->updateStatusMessage(QStringLiteral("已导入工程方案：%1，请点击“应用方案”。").arg(imported_name), 6000);
    });

    QObject::connect(export_project_profile_button, &QPushButton::clicked, dialog, [owner,
                                                                                      dialog,
                                                                                      project_profile_combo]() {
        const QString profile_name = project_profile_combo->currentText().trimmed();
        if (profile_name.isEmpty()) {
            return;
        }
        const QString path = QFileDialog::getSaveFileName(
            dialog,
            QStringLiteral("导出工程方案"),
            profile_name + QStringLiteral(".zip"),
            QStringLiteral("工程方案 ZIP (*.zip);;工程方案 JSON (*.json)"));
        if (path.isEmpty()) {
            return;
        }
        bool include_models = false;
        if (path.toLower().endsWith(QStringLiteral(".zip"))) {
            include_models = QMessageBox::question(
                                 owner,
                                 QStringLiteral("导出模型"),
                                 QStringLiteral("是否把 OBB/SEG 模型文件一起导出？"),
                                 QMessageBox::Yes | QMessageBox::No,
                                 QMessageBox::Yes) == QMessageBox::Yes;
        }
        QString error_message;
        if (!ExportProjectProfile(profile_name, path, include_models, &error_message)) {
            QMessageBox::warning(owner, QStringLiteral("导出失败"), error_message);
            return;
        }
        owner->updateStatusMessage(QStringLiteral("工程方案已导出：%1").arg(QFileInfo(path).fileName()), 6000);
    });

    QObject::connect(apply_profile_button, &QPushButton::clicked, dialog, [owner,
                                                                  profile_combo,
                                                                  coordinate_status_label,
                                                                  read_transform_config_from_editor,
                                                                  refresh_profile_combo]() {
        const QString profile_name = profile_combo->currentText().trimmed();
        if (profile_name.isEmpty()) {
            QMessageBox::warning(owner, QStringLiteral("方案为空"), QStringLiteral("请选择一个标定方案。"));
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

        EngineeringSettingsService::SaveLastCoordinateProfile(profile_name);

        refresh_profile_combo();
        owner->updateStatusMessage(QStringLiteral("已应用当前坐标方案并计算矩阵：%1").arg(profile_name), 5000);
    });

    QObject::connect(profile_combo,
            static_cast<void (QComboBox::*)(int)>(&QComboBox::activated),
            dialog,
            [owner,
             profile_combo,
             coordinate_enabled_check,
             image_x_edits,
             image_y_edits,
             machine_x_edits,
             machine_y_edits,
             coordinate_status_label,
             populate_coordinate_fields](int) {
        const QString profile_name = profile_combo->currentText().trimmed();
        if (profile_name.isEmpty()) {
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
        populate_coordinate_fields(owner->coordinate_transform_config_,
                                   image_x_edits,
                                   image_y_edits,
                                   machine_x_edits,
                                   machine_y_edits);
        RefreshCoordinateStatusLabel(coordinate_status_label, owner->coordinate_transform_state_);
        EngineeringSettingsService::SaveLastCoordinateProfile(profile_name);
        owner->updateStatusMessage(QStringLiteral("已载入标定方案：%1").arg(profile_name), 5000);
    });

    QObject::connect(import_profile_button, &QPushButton::clicked, dialog, [owner,
                                                                   dialog,
                                                                   profile_combo,
                                                                   coordinate_enabled_check,
                                                                   image_x_edits,
                                                                   image_y_edits,
                                                                   machine_x_edits,
                                                                   machine_y_edits,
                                                                   coordinate_status_label,
                                                                   normalize_transform_config,
                                                                   populate_coordinate_fields]() {
        const QString path = QFileDialog::getOpenFileName(dialog,
                                                          QStringLiteral("导入坐标转换方案"),
                                                          QString(),
                                                          QStringLiteral("Calibration Profile (*.json *.txt);;JSON Profile (*.json);;VisionMaster TXT (*.txt);;All Files (*)"));
        if (path.isEmpty()) {
            return;
        }

        CoordinateTransformConfig imported_config;
        QString import_error;
        if (!LoadCalibrationProfileFromFile(path, &imported_config, &import_error)) {
            QMessageBox::warning(owner, QStringLiteral("导入失败"), import_error);
            return;
        }

        imported_config.profile_name = QFileInfo(path).completeBaseName();
        normalize_transform_config(&imported_config);
        coordinate_enabled_check->setChecked(imported_config.enabled);
        populate_coordinate_fields(imported_config,
                                   image_x_edits,
                                   image_y_edits,
                                   machine_x_edits,
                                   machine_y_edits);
        {
            QSignalBlocker blocker(profile_combo);
            if (profile_combo->findText(imported_config.profile_name) < 0) {
                profile_combo->insertItem(0, imported_config.profile_name);
            }
            profile_combo->setCurrentText(imported_config.profile_name);
        }
        coordinate_status_label->setText(QStringLiteral("已导入坐标方案，尚未应用；点击“应用”后计算矩阵。"));
        owner->updateStatusMessage(QStringLiteral("已导入坐标方案，点击“应用”计算矩阵：%1").arg(QFileInfo(path).fileName()), 6000);
    });

    QObject::connect(save_as_profile_button, &QPushButton::clicked, dialog, [owner,
                                                                    profile_combo,
                                                                    coordinate_status_label,
                                                                    read_transform_config_from_editor,
                                                                    refresh_profile_combo]() {
        const QString default_name = profile_combo->currentText().trimmed().isEmpty()
                                          ? DefaultCalibrationProfileName()
                                          : profile_combo->currentText().trimmed();
        QString profile_name;
        if (!PromptCalibrationProfileName(owner, default_name, &profile_name)) {
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
