#include "engineering_settings_dialog_helpers.h"

#include "calibration_profile_store.h"
#include "ui_scale_utils.h"

#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QSettings>
#include <QSizePolicy>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtGlobal>

#include <cmath>

namespace {

int S(int value)
{
    return ScalePx(value);
}

QLineEdit* CreateIpOctetEdit(QWidget* parent)
{
    auto* edit = new QLineEdit(parent);
    edit->setValidator(new QIntValidator(0, 255, edit));
    edit->setMaxLength(3);
    edit->setAlignment(Qt::AlignCenter);
    edit->setFixedWidth(S(52));
    edit->setPlaceholderText(QStringLiteral("0"));
    return edit;
}

QDoubleSpinBox* CreateSpinBox(QWidget* parent,
                              double value,
                              double minimum,
                              double maximum,
                              int decimals,
                              double single_step,
                              int fixed_width,
                              const QString& suffix = QString(),
                              const QString& special_value_text = QString())
{
    auto* spin_box = new QDoubleSpinBox(parent);
    spin_box->setRange(minimum, maximum);
    spin_box->setDecimals(decimals);
    spin_box->setSingleStep(single_step);
    if (!suffix.isEmpty()) {
        spin_box->setSuffix(suffix);
    }
    if (!special_value_text.isEmpty()) {
        spin_box->setSpecialValueText(special_value_text);
    }
    spin_box->setValue(value);
    spin_box->setFixedWidth(S(fixed_width));
    return spin_box;
}

} // namespace

QLabel* CreateGroupCaption(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setObjectName("groupCaption");
    return label;
}

QGridLayout* CreateSettingsGrid(int horizontal_spacing, int vertical_spacing)
{
    auto* grid = new QGridLayout();
    grid->setHorizontalSpacing(S(horizontal_spacing));
    grid->setVerticalSpacing(S(vertical_spacing));
    return grid;
}

void SetGridColumnMinimumWidths(QGridLayout* grid, std::initializer_list<int> widths)
{
    int column = 0;
    for (const int width : widths) {
        grid->setColumnMinimumWidth(column++, S(width));
    }
}

void SetGridColumnStretches(QGridLayout* grid, std::initializer_list<int> stretches)
{
    int column = 0;
    for (const int stretch : stretches) {
        grid->setColumnStretch(column++, stretch);
    }
}

QWidget* CreateDistributedFieldLabel(QString text, QWidget* parent, int minimum_width)
{
    text.remove(QLatin1Char(' '));
    text.remove(QChar(0x3000));
    text.remove(QStringLiteral("："));
    text.remove(QLatin1Char(':'));

    auto* container = new QWidget(parent);
    container->setMinimumWidth(S(minimum_width));
    container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto* layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    for (int i = 0; i < text.size(); ++i) {
        auto* char_label = new QLabel(QString(text.at(i)), container);
        char_label->setAlignment(Qt::AlignCenter);
        layout->addWidget(char_label, 0);
        if (i + 1 < text.size()) {
            layout->addStretch(1);
        }
    }

    auto* colon_label = new QLabel(QStringLiteral("："), container);
    colon_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    layout->addWidget(colon_label, 0);
    return container;
}

QFrame* CreateCollapsibleSection(QWidget* parent,
                                 const QString& title,
                                 QWidget* content,
                                 bool expanded,
                                 const QString& settings_key)
{
    auto* section = new QFrame(parent);
    section->setObjectName("collapsibleSection");
    auto* layout = new QVBoxLayout(section);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(S(8));

    auto* header = new QToolButton(section);
    header->setObjectName("collapsibleHeader");
    header->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    header->setCheckable(true);
    header->setChecked(expanded);
    header->setText(title);
    header->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
    header->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    header->setAutoRaise(false);

    content->setVisible(expanded);
    layout->addWidget(header);
    layout->addWidget(content);

    QObject::connect(header, &QToolButton::toggled, section, [header, content, settings_key](bool checked) {
        header->setArrowType(checked ? Qt::DownArrow : Qt::RightArrow);
        content->setVisible(checked);
        if (!settings_key.isEmpty()) {
            QSettings settings(QStringLiteral("TankEye"), QStringLiteral("TankEye-Iris"));
            settings.beginGroup(QStringLiteral("engineering_ui"));
            settings.setValue(settings_key, checked);
            settings.endGroup();
        }
    });

    return section;
}

QHBoxLayout* CreatePathRow(QDialog* dialog, QLineEdit* path_edit, const QString& title, const QString& dialog_title)
{
    auto* row = new QHBoxLayout();
    auto* title_label = new QLabel(title, dialog);
    title_label->setMinimumWidth(S(90));
    auto* browse_button = new QPushButton(QStringLiteral("浏览"), dialog);
    row->addWidget(title_label);
    row->addWidget(path_edit, 1);
    row->addWidget(browse_button);

    QObject::connect(browse_button, &QPushButton::clicked, dialog, [dialog, path_edit, dialog_title]() {
        const QString path = QFileDialog::getOpenFileName(dialog,
                                                          dialog_title,
                                                          path_edit->text(),
                                                          QStringLiteral("OpenVINO/ONNX Model (*.xml *.onnx);;All Files (*)"));
        if (!path.isEmpty()) {
            path_edit->setText(path);
        }
    });

    return row;
}

QDoubleSpinBox* CreateLimitSpinBox(QWidget* parent, double value)
{
    return CreateSpinBox(parent, value, -999999.0, 999999.0, 2, 1.0, 104);
}

QDoubleSpinBox* CreateExposureSpinBox(QWidget* parent, double value)
{
    auto* spin_box = new QDoubleSpinBox(parent);
    spin_box->setRange(0.0, 10000000.0);
    spin_box->setDecimals(1);
    spin_box->setSingleStep(100.0);
    spin_box->setSuffix(QStringLiteral(" us"));
    spin_box->setSpecialValueText(QStringLiteral("未设置"));
    spin_box->setValue(value > 0.0 ? value : 0.0);
    spin_box->setFixedWidth(S(150));
    return spin_box;
}

QDoubleSpinBox* CreateAngleOffsetSpinBox(QWidget* parent, double value)
{
    return CreateSpinBox(parent, value, -360.0, 360.0, 2, 1.0, 150, QStringLiteral(" deg"));
}

QDoubleSpinBox* CreateAngleReferenceSpinBox(QWidget* parent, double value)
{
    return CreateSpinBox(parent, value, -3600.0, 3600.0, 2, 1.0, 125, QStringLiteral(" deg"));
}

QDoubleSpinBox* CreateCenterRayOffsetSpinBox(QWidget* parent, double value)
{
    return CreateSpinBox(parent, value, -100.0, 100.0, 2, 1.0, 150, QStringLiteral(" px"));
}

QDoubleSpinBox* CreateGripperDimensionSpinBox(QWidget* parent, double value)
{
    return CreateSpinBox(parent, value, 0.0, 999999.0, 2, 1.0, 150);
}

QDoubleSpinBox* CreateThresholdSpinBox(QWidget* parent, double value)
{
    return CreateSpinBox(parent, qBound(0.01, value, 0.99), 0.01, 0.99, 2, 0.05, 96);
}

QDoubleSpinBox* CreateCoordinateSpinBox(QWidget* parent, double value)
{
    return CreateSpinBox(parent, value, -9999999.0, 9999999.0, 3, 1.0, 96);
}

double CoordinateValueAt(const QList<QDoubleSpinBox*>& edits, int index)
{
    return (index >= 0 && index < edits.size() && edits[index]) ? edits[index]->value() : 0.0;
}

void SetCoordinateValue(const QList<QDoubleSpinBox*>& edits, int index, double value)
{
    if (index >= 0 && index < edits.size() && edits[index]) {
        edits[index]->setValue(value);
    }
}

QString CoordinateTransformStatusText(const CoordinateTransformState& state)
{
    if (!state.enabled) {
        return QStringLiteral("坐标转换未启用。");
    }
    if (state.valid) {
        return QStringLiteral("坐标转换已启用，矩阵有效，平均重投影误差 %1，PLC 将写入机械坐标。")
            .arg(QString::number(state.mean_reprojection_error, 'f', 3));
    }
    return QStringLiteral("坐标转换无效：%1").arg(QString::fromStdString(state.error_message));
}

void RefreshCoordinateStatusLabel(QLabel* label, const CoordinateTransformState& state)
{
    if (label == nullptr) {
        return;
    }

    label->setText(CoordinateTransformStatusText(state));
    if (!state.enabled) {
        label->setStyleSheet(QString(
            "QLabel { color: #4b5563; background: transparent; border: none; padding: %1px 0px; font-size: %2px; font-weight: 700; }")
            .arg(S(8))
            .arg(S(15)));
        return;
    }
    if (state.valid) {
        label->setStyleSheet(QString(
            "QLabel { color: #047857; background: transparent; border: none; padding: %1px 0px; font-size: %2px; font-weight: 800; }")
            .arg(S(8))
            .arg(S(16)));
        return;
    }
    label->setStyleSheet(QString(
        "QLabel { color: #be123c; background: transparent; border: none; padding: %1px 0px; font-size: %2px; font-weight: 800; }")
        .arg(S(8))
        .arg(S(15)));
}

QHBoxLayout* CreateCameraIpRow(QWidget* parent, QList<QLineEdit*>& ip_edits)
{
    auto* row = new QHBoxLayout();
    auto* label = new QLabel(QStringLiteral("相机 IP"), parent);
    label->setMinimumWidth(S(90));
    row->addWidget(label);

    auto* ip_container = new QHBoxLayout();
    ip_container->setSpacing(S(4));
    for (int i = 0; i < 4; ++i) {
        auto* edit = CreateIpOctetEdit(parent);
        ip_edits.append(edit);
        ip_container->addWidget(edit);
        if (i < 3) {
            auto* dot = new QLabel(QStringLiteral("."), parent);
            dot->setAlignment(Qt::AlignCenter);
            ip_container->addWidget(dot);
        }
    }
    ip_container->addStretch(1);
    row->addLayout(ip_container, 1);
    return row;
}

void PopulateIpEdits(const QString& camera_ip, const QList<QLineEdit*>& ip_edits)
{
    const QStringList parts = camera_ip.trimmed().split('.');
    if (parts.size() != 4 || ip_edits.size() != 4) {
        return;
    }

    for (int i = 0; i < 4; ++i) {
        bool ok = false;
        const int value = parts.at(i).toInt(&ok);
        if (!ok || value < 0 || value > 255) {
            return;
        }
    }

    for (int i = 0; i < 4; ++i) {
        ip_edits.at(i)->setText(parts.at(i));
    }
}

bool BuildCameraIpFromEdits(const QList<QLineEdit*>& ip_edits, QString* camera_ip, QString* error_message)
{
    QStringList parts;
    bool any_filled = false;
    bool all_filled = true;

    for (QLineEdit* edit : ip_edits) {
        const QString text = edit->text().trimmed();
        any_filled = any_filled || !text.isEmpty();
        all_filled = all_filled && !text.isEmpty();
        parts.append(text);
    }

    if (!any_filled) {
        *camera_ip = QString();
        return true;
    }

    if (!all_filled || parts.size() != 4) {
        *error_message = QStringLiteral("相机 IP 必须填写完整四段，或四段全部留空。");
        return false;
    }

    for (const QString& part : parts) {
        bool ok = false;
        const int value = part.toInt(&ok);
        if (!ok || value < 0 || value > 255) {
            *error_message = QStringLiteral("相机 IP 每一段必须是 0 到 255。");
            return false;
        }
    }

    *camera_ip = parts.join('.');
    return true;
}

bool ReadEngineeringSettingsDraft(const EngineeringSettingsControls& controls,
                                  EngineeringSettingsDraft* draft,
                                  EngineeringSettingsValidationError* error)
{
    if (draft == nullptr) {
        return false;
    }

    if (controls.x_lower_spin->value() > controls.x_upper_spin->value() ||
        controls.y_lower_spin->value() > controls.y_upper_spin->value() ||
        controls.angle_lower_spin->value() > controls.angle_upper_spin->value()) {
        if (error != nullptr) {
            error->title = QStringLiteral("限制参数错误");
            error->message = QStringLiteral("每一组限制的下限必须小于或等于上限。");
        }
        return false;
    }

    const double roi_margin = controls.roi_margin_spin->value();
    if (controls.limit_enabled_check->isChecked() &&
        (controls.x_lower_spin->value() + roi_margin > controls.x_upper_spin->value() - roi_margin ||
         controls.y_lower_spin->value() + roi_margin > controls.y_upper_spin->value() - roi_margin)) {
        if (error != nullptr) {
            error->title = QStringLiteral("ROI 留边错误");
            error->message = QStringLiteral("ROI 留边过大，内缩后的前后/Y 或左右/X 区域为空。");
        }
        return false;
    }

    QString camera_ip;
    QString camera_ip_error;
    if (!BuildCameraIpFromEdits(controls.camera_ip_edits, &camera_ip, &camera_ip_error)) {
        if (error != nullptr) {
            error->title = QStringLiteral("相机 IP 错误");
            error->message = camera_ip_error;
        }
        return false;
    }

    CoordinateTransformConfig next_config;
    QString calibration_error;
    if (!ReadCoordinateTransformConfigFromEditor(controls,
                                                 controls.profile_combo->currentText().trimmed(),
                                                 &next_config,
                                                 &calibration_error)) {
        if (error != nullptr) {
            error->title = QStringLiteral("坐标转换错误");
            error->message = calibration_error;
        }
        return false;
    }

    EngineeringSettingsDraft next_draft;
    next_draft.grab_limits.enabled = controls.limit_enabled_check->isChecked();
    next_draft.grab_limits.x = { controls.x_lower_spin->value(), controls.x_upper_spin->value() };
    next_draft.grab_limits.y = { controls.y_lower_spin->value(), controls.y_upper_spin->value() };
    next_draft.grab_limits.angle = { controls.angle_lower_spin->value(), controls.angle_upper_spin->value() };
    next_draft.grab_limits.roi_margin = controls.roi_margin_spin->value();
    next_draft.camera_ip = camera_ip;
    next_draft.camera_exposure_us = controls.exposure_spin->value() > 0.0 ? controls.exposure_spin->value() : 0.0;
    next_draft.obb_conf_threshold = controls.obb_conf_threshold_spin->value();
    next_draft.obb_nms_threshold = controls.obb_nms_threshold_spin->value();
    next_draft.seg_conf_threshold = controls.seg_conf_threshold_spin->value();
    next_draft.seg_nms_threshold = controls.seg_nms_threshold_spin->value();
    next_draft.angle_offset_deg = controls.angle_offset_spin->value();
    next_draft.center_ray_offset_px = controls.center_ray_offset_spin->value();
    next_draft.show_plc_center_debug = controls.show_plc_center_debug_check->isChecked();
    next_draft.show_head_ray_debug = controls.show_head_ray_debug_check->isChecked();
    next_draft.postprocess_debug_logging_enabled = controls.postprocess_debug_logging_check->isChecked();
    next_draft.auto_start_enabled = controls.auto_start_check->isChecked();
    next_draft.startup_delay_seconds = controls.startup_delay_spin->value();
    next_draft.show_grab_limit_overlay = controls.show_grab_limit_overlay_check->isChecked();
    next_draft.mechanical_gripper_length = controls.mechanical_gripper_length_spin->value();
    next_draft.mechanical_gripper_width = controls.mechanical_gripper_width_spin->value();
    for (int head_type = 1; head_type < static_cast<int>(next_draft.head_type_compensations.size()); ++head_type) {
        next_draft.head_type_compensations[head_type].angle_offset_deg =
            controls.head_type_angle_offset_spins[head_type] != nullptr
                ? controls.head_type_angle_offset_spins[head_type]->value()
                : 0.0;
        next_draft.head_type_compensations[head_type].ac_ray_offset_mm =
            controls.head_type_ac_ray_offset_spins[head_type] != nullptr
                ? controls.head_type_ac_ray_offset_spins[head_type]->value()
                : 0.0;
    }
    next_draft.angle_reverse_direction = controls.angle_direction_combo->currentData().toBool();
    next_draft.angle_range_mode = static_cast<AngleRangeMode>(controls.angle_range_combo->currentData().toInt());
    next_draft.axis_mapping_mode = static_cast<AxisMappingMode>(controls.axis_mapping_combo->currentData().toInt());
    next_draft.front_back_offset = controls.front_back_offset_spin->value();
    next_draft.left_right_offset = controls.left_right_offset_spin->value();
    next_draft.coordinate_transform_config = next_config;
    *draft = next_draft;
    return true;
}

bool ReadCoordinateTransformConfigFromEditor(const EngineeringSettingsControls& controls,
                                             const QString& profile_name,
                                             CoordinateTransformConfig* config,
                                             QString* error_message)
{
    if (config == nullptr) {
        return false;
    }

    CoordinateTransformConfig next_config;
    next_config.profile_name = SanitizeCalibrationProfileName(profile_name);
    next_config.enabled = controls.coordinate_enabled_check->isChecked();
    next_config.points.reserve(9);
    for (int row = 0; row < 9; ++row) {
        const double image_x = CoordinateValueAt(controls.image_x_edits, row);
        const double image_y = CoordinateValueAt(controls.image_y_edits, row);
        const double machine_x = CoordinateValueAt(controls.machine_x_edits, row);
        const double machine_y = CoordinateValueAt(controls.machine_y_edits, row);
        next_config.points.push_back({ image_x, image_y, machine_x, machine_y });
        if (next_config.enabled &&
            (!std::isfinite(image_x) || !std::isfinite(image_y) ||
             !std::isfinite(machine_x) || !std::isfinite(machine_y))) {
            if (error_message != nullptr) {
                *error_message = QStringLiteral("第 %1 个点位含有非法数值。").arg(row + 1);
            }
            return false;
        }
    }

    if (next_config.profile_name.isEmpty()) {
        next_config.profile_name = DefaultCalibrationProfileName();
    }
    if (next_config.points.size() < 9) {
        next_config.points.resize(9);
    }
    if (next_config.points.size() > 9) {
        next_config.points.resize(9);
    }

    *config = next_config;
    return true;
}
