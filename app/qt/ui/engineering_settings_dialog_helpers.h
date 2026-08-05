#pragma once

#include "coordinate_transform.h"
#include "frame_postprocess.h"
#include "grab_limit_evaluator.h"
#include "plc_result_contract.h"

#include <QList>
#include <QSize>
#include <QString>

#include <array>
#include <initializer_list>

class QCheckBox;
class QComboBox;
class QDialog;
class QDoubleSpinBox;
class QFrame;
class QGridLayout;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QWidget;

QLabel* CreateGroupCaption(const QString& text, QWidget* parent);
QGridLayout* CreateSettingsGrid(int horizontal_spacing, int vertical_spacing);
void SetGridColumnMinimumWidths(QGridLayout* grid, std::initializer_list<int> widths);
void SetGridColumnStretches(QGridLayout* grid, std::initializer_list<int> stretches);
QWidget* CreateDistributedFieldLabel(QString text, QWidget* parent, int minimum_width);
QFrame* CreateCollapsibleSection(QWidget* parent,
                                 const QString& title,
                                 QWidget* content,
                                 bool expanded,
                                 const QString& settings_key);
QHBoxLayout* CreatePathRow(QDialog* dialog, QLineEdit* path_edit, const QString& title, const QString& dialog_title);
QDoubleSpinBox* CreateLimitSpinBox(QWidget* parent, double value);
QDoubleSpinBox* CreateExposureSpinBox(QWidget* parent, double value);
QDoubleSpinBox* CreateAngleOffsetSpinBox(QWidget* parent, double value);
QDoubleSpinBox* CreateAngleReferenceSpinBox(QWidget* parent, double value);
QDoubleSpinBox* CreateCenterRayOffsetSpinBox(QWidget* parent, double value);
QDoubleSpinBox* CreateGripperDimensionSpinBox(QWidget* parent, double value);
QDoubleSpinBox* CreateThresholdSpinBox(QWidget* parent, double value);
QDoubleSpinBox* CreateCoordinateSpinBox(QWidget* parent, double value);
double CoordinateValueAt(const QList<QDoubleSpinBox*>& edits, int index);
void SetCoordinateValue(const QList<QDoubleSpinBox*>& edits, int index, double value);
QString CoordinateTransformStatusText(const CoordinateTransformState& state);
void RefreshCoordinateStatusLabel(QLabel* label, const CoordinateTransformState& state);
QHBoxLayout* CreateCameraIpRow(QWidget* parent, QList<QLineEdit*>& ip_edits);
void PopulateIpEdits(const QString& camera_ip, const QList<QLineEdit*>& ip_edits);
bool BuildCameraIpFromEdits(const QList<QLineEdit*>& ip_edits, QString* camera_ip, QString* error_message);

struct EngineeringSettingsControls
{
    QList<QLineEdit*> camera_ip_edits;
    QDoubleSpinBox* exposure_spin = nullptr;
    QDoubleSpinBox* obb_conf_threshold_spin = nullptr;
    QDoubleSpinBox* obb_nms_threshold_spin = nullptr;
    QDoubleSpinBox* seg_conf_threshold_spin = nullptr;
    QDoubleSpinBox* seg_nms_threshold_spin = nullptr;
    QDoubleSpinBox* angle_offset_spin = nullptr;
    QDoubleSpinBox* center_ray_offset_spin = nullptr;
    QCheckBox* show_plc_center_debug_check = nullptr;
    QCheckBox* show_head_ray_debug_check = nullptr;
    QCheckBox* postprocess_debug_logging_check = nullptr;
    QDoubleSpinBox* mechanical_gripper_length_spin = nullptr;
    QDoubleSpinBox* mechanical_gripper_width_spin = nullptr;
    std::array<QDoubleSpinBox*, 5> head_type_angle_offset_spins{};
    std::array<QDoubleSpinBox*, 5> head_type_ac_ray_offset_spins{};
    QComboBox* angle_direction_combo = nullptr;
    QComboBox* angle_range_combo = nullptr;
    QComboBox* axis_mapping_combo = nullptr;
    QDoubleSpinBox* front_back_offset_spin = nullptr;
    QDoubleSpinBox* left_right_offset_spin = nullptr;
    QCheckBox* limit_enabled_check = nullptr;
    QCheckBox* show_grab_limit_overlay_check = nullptr;
    QDoubleSpinBox* x_lower_spin = nullptr;
    QDoubleSpinBox* x_upper_spin = nullptr;
    QDoubleSpinBox* y_lower_spin = nullptr;
    QDoubleSpinBox* y_upper_spin = nullptr;
    QDoubleSpinBox* angle_lower_spin = nullptr;
    QDoubleSpinBox* angle_upper_spin = nullptr;
    QDoubleSpinBox* roi_margin_spin = nullptr;
    QCheckBox* coordinate_enabled_check = nullptr;
    QList<QDoubleSpinBox*> image_x_edits;
    QList<QDoubleSpinBox*> image_y_edits;
    QList<QDoubleSpinBox*> machine_x_edits;
    QList<QDoubleSpinBox*> machine_y_edits;
    QComboBox* profile_combo = nullptr;
};

struct EngineeringSettingsDraft
{
    GrabLimitConfig grab_limits;
    QString camera_ip;
    double camera_exposure_us = 0.0;
    double obb_conf_threshold = 0.5;
    double obb_nms_threshold = 0.3;
    double seg_conf_threshold = 0.3;
    double seg_nms_threshold = 0.4;
    double angle_offset_deg = 0.0;
    double center_ray_offset_px = 0.0;
    bool show_plc_center_debug = false;
    bool show_head_ray_debug = true;
    bool postprocess_debug_logging_enabled = false;
    bool show_grab_limit_overlay = true;
    double mechanical_gripper_length = 0.0;
    double mechanical_gripper_width = 0.0;
    std::array<HeadTypeCompensation, 5> head_type_compensations{};
    bool angle_reverse_direction = false;
    AngleRangeMode angle_range_mode = AngleRangeMode::ZeroTo360;
    AxisMappingMode axis_mapping_mode = AxisMappingMode::FrontBackMachineY;
    double front_back_offset = 0.0;
    double left_right_offset = 0.0;
    CoordinateTransformConfig coordinate_transform_config;
};

struct EngineeringSettingsValidationError
{
    QString title;
    QString message;
};

bool ReadEngineeringSettingsDraft(const EngineeringSettingsControls& controls,
                                  EngineeringSettingsDraft* draft,
                                  EngineeringSettingsValidationError* error);
bool ReadCoordinateTransformConfigFromEditor(const EngineeringSettingsControls& controls,
                                             const QString& profile_name,
                                             CoordinateTransformConfig* config,
                                             QString* error_message);
