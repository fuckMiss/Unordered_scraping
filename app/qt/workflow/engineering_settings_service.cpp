#include "engineering_settings_service.h"

#include "calibration_profile_store.h"

#include <QSettings>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>

namespace {

std::unique_ptr<QSettings> CreateSettings()
{
    const QString override_path = QString::fromLocal8Bit(qgetenv("TANKEYE_SETTINGS_INI_PATH")).trimmed();
    if (!override_path.isEmpty()) {
        return std::make_unique<QSettings>(override_path, QSettings::IniFormat);
    }
    return std::make_unique<QSettings>(QStringLiteral("TankEye"), QStringLiteral("TankEye-Iris"));
}

bool NearlyEqual(double left, double right)
{
    return std::fabs(left - right) < 0.0001;
}

bool IsLegacyDefaultLimits(double x_lower,
                           double x_upper,
                           double y_lower,
                           double y_upper,
                           double angle_lower,
                           double angle_upper)
{
    const bool legacy_xy =
        x_lower <= -900000.0 && x_upper >= 900000.0 &&
        y_lower <= -900000.0 && y_upper >= 900000.0;
    const bool legacy_angle =
        (NearlyEqual(angle_lower, -360.0) || angle_lower <= -360.0) &&
        (NearlyEqual(angle_upper, 360.0) || angle_upper >= 360.0);
    return legacy_xy && legacy_angle;
}

QString AngleRangeModeKey(AngleRangeMode mode)
{
    return mode == AngleRangeMode::Signed180 ? QStringLiteral("signed_180")
                                             : QStringLiteral("zero_to_360");
}

AngleRangeMode ParseAngleRangeMode(const QString& value)
{
    return value.trimmed().toLower() == QStringLiteral("signed_180")
               ? AngleRangeMode::Signed180
               : AngleRangeMode::ZeroTo360;
}

QString AxisMappingKey(AxisMappingMode mode)
{
    return mode == AxisMappingMode::FrontBackMachineX ? QStringLiteral("machine_x")
                                                      : QStringLiteral("machine_y");
}

AxisMappingMode ParseAxisMappingMode(const QString& value)
{
    return value.trimmed().toLower() == QStringLiteral("machine_x")
               ? AxisMappingMode::FrontBackMachineX
               : AxisMappingMode::FrontBackMachineY;
}

double ClampThreshold(double value, double fallback)
{
    if (!std::isfinite(value)) {
        return fallback;
    }
    return std::max(0.01, std::min(0.99, value));
}

} // namespace

GrabLimitConfig EngineeringSettingsService::LoadGrabLimits(const GrabLimitConfig& defaults)
{
    GrabLimitConfig limits = defaults;
    auto settings = CreateSettings();
    settings->beginGroup(QStringLiteral("grab_limits"));
    limits.enabled = settings->value(QStringLiteral("enabled"), limits.enabled).toBool();
    limits.x.lower = settings->value(QStringLiteral("x_lower"), limits.x.lower).toDouble();
    limits.x.upper = settings->value(QStringLiteral("x_upper"), limits.x.upper).toDouble();
    limits.y.lower = settings->value(QStringLiteral("y_lower"), limits.y.lower).toDouble();
    limits.y.upper = settings->value(QStringLiteral("y_upper"), limits.y.upper).toDouble();
    limits.angle.lower = settings->value(QStringLiteral("angle_lower"), limits.angle.lower).toDouble();
    limits.angle.upper = settings->value(QStringLiteral("angle_upper"), limits.angle.upper).toDouble();
    limits.roi_margin = settings->value(QStringLiteral("roi_margin"), limits.roi_margin).toDouble();
    settings->endGroup();

    if (!std::isfinite(limits.roi_margin) || limits.roi_margin < 0.0) {
        limits.roi_margin = 5.0;
    }
    if (IsLegacyDefaultLimits(limits.x.lower,
                              limits.x.upper,
                              limits.y.lower,
                              limits.y.upper,
                              limits.angle.lower,
                              limits.angle.upper)) {
        limits = defaults;
        SaveGrabLimits(limits);
    }
    return limits;
}

void EngineeringSettingsService::SaveGrabLimits(const GrabLimitConfig& config)
{
    auto settings = CreateSettings();
    settings->beginGroup(QStringLiteral("grab_limits"));
    settings->setValue(QStringLiteral("enabled"), config.enabled);
    settings->setValue(QStringLiteral("x_lower"), config.x.lower);
    settings->setValue(QStringLiteral("x_upper"), config.x.upper);
    settings->setValue(QStringLiteral("y_lower"), config.y.lower);
    settings->setValue(QStringLiteral("y_upper"), config.y.upper);
    settings->setValue(QStringLiteral("angle_lower"), config.angle.lower);
    settings->setValue(QStringLiteral("angle_upper"), config.angle.upper);
    settings->setValue(QStringLiteral("roi_margin"), config.roi_margin);
    settings->endGroup();
    settings->sync();
}

CameraSettings EngineeringSettingsService::LoadCameraSettings()
{
    return LoadCameraSettings(CameraSettings{});
}

CameraSettings EngineeringSettingsService::LoadCameraSettings(const CameraSettings& defaults)
{
    CameraSettings camera = defaults;
    auto settings = CreateSettings();
    settings->beginGroup(QStringLiteral("camera"));
    camera.ip = settings->value(QStringLiteral("ip"), camera.ip).toString().trimmed();
    camera.exposure_us = settings->value(QStringLiteral("exposure_us"), camera.exposure_us).toDouble();
    settings->endGroup();
    return camera;
}

void EngineeringSettingsService::SaveCameraSettings(const CameraSettings& camera)
{
    auto settings = CreateSettings();
    settings->beginGroup(QStringLiteral("camera"));
    settings->setValue(QStringLiteral("ip"), camera.ip.trimmed());
    settings->setValue(QStringLiteral("exposure_us"), camera.exposure_us > 0.0 ? camera.exposure_us : 0.0);
    settings->endGroup();
    settings->sync();
}

AngleCalibrationSettings EngineeringSettingsService::LoadAngleCalibrationSettings()
{
    return LoadAngleCalibrationSettings(AngleCalibrationSettings{});
}

AngleCalibrationSettings EngineeringSettingsService::LoadAngleCalibrationSettings(const AngleCalibrationSettings& defaults)
{
    AngleCalibrationSettings angle = defaults;
    auto settings = CreateSettings();
    settings->beginGroup(QStringLiteral("angle_calibration"));
    angle.offset_deg = settings->value(QStringLiteral("offset_deg"), angle.offset_deg).toDouble();
    const QString direction =
        settings->value(QStringLiteral("direction"), angle.reverse_direction ? QStringLiteral("reverse")
                                                                             : QStringLiteral("forward"))
            .toString()
            .trimmed()
            .toLower();
    if (direction == QStringLiteral("reverse") || direction == QStringLiteral("forward")) {
        angle.reverse_direction = direction == QStringLiteral("reverse");
    }
    angle.range_mode = ParseAngleRangeMode(
        settings->value(QStringLiteral("range_mode"), AngleRangeModeKey(angle.range_mode)).toString());
    settings->endGroup();
    return angle;
}

void EngineeringSettingsService::SaveAngleCalibrationSettings(const AngleCalibrationSettings& angle)
{
    auto settings = CreateSettings();
    settings->beginGroup(QStringLiteral("angle_calibration"));
    settings->setValue(QStringLiteral("offset_deg"), angle.offset_deg);
    settings->setValue(QStringLiteral("direction"), angle.reverse_direction ? QStringLiteral("reverse")
                                                                           : QStringLiteral("forward"));
    settings->setValue(QStringLiteral("range_mode"), AngleRangeModeKey(angle.range_mode));
    settings->endGroup();
    settings->sync();
}

ObbPostprocessSettings EngineeringSettingsService::LoadObbPostprocessSettings()
{
    ObbPostprocessSettings postprocess;
    auto settings = CreateSettings();
    settings->beginGroup(QStringLiteral("obb_postprocess"));
    postprocess.center_ray_offset_px = settings->value(QStringLiteral("center_ray_offset_px"), 10.0).toDouble();
    postprocess.show_plc_center_debug = settings->value(QStringLiteral("show_plc_center_debug"), false).toBool();
    postprocess.show_head_ray_debug = settings->value(QStringLiteral("show_head_ray_debug"), true).toBool();
    postprocess.debug_logging_enabled = settings->value(QStringLiteral("debug_logging_enabled"), false).toBool();
    settings->endGroup();

    if (!std::isfinite(postprocess.center_ray_offset_px)) {
        postprocess.center_ray_offset_px = 10.0;
    }
    postprocess.center_ray_offset_px = std::max(-100.0, std::min(100.0, postprocess.center_ray_offset_px));
    return postprocess;
}

void EngineeringSettingsService::SaveObbPostprocessSettings(const ObbPostprocessSettings& postprocess)
{
    auto settings = CreateSettings();
    settings->beginGroup(QStringLiteral("obb_postprocess"));
    settings->setValue(QStringLiteral("center_ray_offset_px"), postprocess.center_ray_offset_px);
    settings->setValue(QStringLiteral("show_plc_center_debug"), postprocess.show_plc_center_debug);
    settings->setValue(QStringLiteral("show_head_ray_debug"), postprocess.show_head_ray_debug);
    settings->setValue(QStringLiteral("debug_logging_enabled"), postprocess.debug_logging_enabled);
    settings->endGroup();
    settings->sync();
}

ModelThresholdSettings EngineeringSettingsService::LoadModelThresholdSettings()
{
    ModelThresholdSettings thresholds;
    auto settings = CreateSettings();
    settings->beginGroup(QStringLiteral("model_thresholds"));
    thresholds.obb_conf_threshold =
        settings->value(QStringLiteral("obb_conf_threshold"), thresholds.obb_conf_threshold).toDouble();
    thresholds.obb_nms_threshold =
        settings->value(QStringLiteral("obb_nms_threshold"), thresholds.obb_nms_threshold).toDouble();
    thresholds.seg_conf_threshold =
        settings->value(QStringLiteral("seg_conf_threshold"), thresholds.seg_conf_threshold).toDouble();
    thresholds.seg_nms_threshold =
        settings->value(QStringLiteral("seg_nms_threshold"), thresholds.seg_nms_threshold).toDouble();
    settings->endGroup();

    thresholds.obb_conf_threshold = ClampThreshold(thresholds.obb_conf_threshold, 0.5);
    thresholds.obb_nms_threshold = ClampThreshold(thresholds.obb_nms_threshold, 0.3);
    thresholds.seg_conf_threshold = ClampThreshold(thresholds.seg_conf_threshold, 0.3);
    thresholds.seg_nms_threshold = ClampThreshold(thresholds.seg_nms_threshold, 0.4);
    return thresholds;
}

void EngineeringSettingsService::SaveModelThresholdSettings(const ModelThresholdSettings& thresholds)
{
    auto settings = CreateSettings();
    settings->beginGroup(QStringLiteral("model_thresholds"));
    settings->setValue(QStringLiteral("obb_conf_threshold"), ClampThreshold(thresholds.obb_conf_threshold, 0.5));
    settings->setValue(QStringLiteral("obb_nms_threshold"), ClampThreshold(thresholds.obb_nms_threshold, 0.3));
    settings->setValue(QStringLiteral("seg_conf_threshold"), ClampThreshold(thresholds.seg_conf_threshold, 0.3));
    settings->setValue(QStringLiteral("seg_nms_threshold"), ClampThreshold(thresholds.seg_nms_threshold, 0.4));
    settings->endGroup();
    settings->sync();
}

AxisMappingMode EngineeringSettingsService::LoadAxisMappingMode()
{
    return LoadAxisMappingMode(AxisMappingMode::FrontBackMachineY);
}

AxisMappingMode EngineeringSettingsService::LoadAxisMappingMode(AxisMappingMode default_mode)
{
    auto settings = CreateSettings();
    settings->beginGroup(QStringLiteral("axis_mapping"));
    const QString front_back_axis =
        settings->value(QStringLiteral("front_back_axis"), AxisMappingKey(default_mode)).toString();
    settings->endGroup();
    const QString normalized = front_back_axis.trimmed().toLower();
    return (normalized == QStringLiteral("machine_x") || normalized == QStringLiteral("machine_y"))
        ? ParseAxisMappingMode(normalized)
        : default_mode;
}

void EngineeringSettingsService::SaveAxisMappingMode(AxisMappingMode mode)
{
    auto settings = CreateSettings();
    settings->beginGroup(QStringLiteral("axis_mapping"));
    settings->setValue(QStringLiteral("front_back_axis"), AxisMappingKey(mode));
    settings->endGroup();
    settings->sync();
}

AxisCompensationSettings EngineeringSettingsService::LoadAxisCompensationSettings()
{
    return LoadAxisCompensationSettings(AxisCompensationSettings{});
}

AxisCompensationSettings EngineeringSettingsService::LoadAxisCompensationSettings(const AxisCompensationSettings& defaults)
{
    AxisCompensationSettings compensation = defaults;
    auto settings = CreateSettings();
    settings->beginGroup(QStringLiteral("axis_mapping"));
    compensation.front_back_offset =
        settings->value(QStringLiteral("front_back_offset"), compensation.front_back_offset).toDouble();
    compensation.left_right_offset =
        settings->value(QStringLiteral("left_right_offset"), compensation.left_right_offset).toDouble();
    settings->endGroup();
    return compensation;
}

void EngineeringSettingsService::SaveAxisCompensationSettings(const AxisCompensationSettings& compensation)
{
    auto settings = CreateSettings();
    settings->beginGroup(QStringLiteral("axis_mapping"));
    settings->setValue(QStringLiteral("front_back_offset"), compensation.front_back_offset);
    settings->setValue(QStringLiteral("left_right_offset"), compensation.left_right_offset);
    settings->endGroup();
    settings->sync();
}

UiOverlaySettings EngineeringSettingsService::LoadUiOverlaySettings()
{
    UiOverlaySettings overlays;
    auto settings = CreateSettings();
    settings->beginGroup(QStringLiteral("ui_overlay"));
    overlays.show_grab_limit_overlay =
        settings->value(QStringLiteral("show_grab_limit_overlay"), overlays.show_grab_limit_overlay).toBool();
    overlays.mechanical_gripper_length =
        settings->value(QStringLiteral("mechanical_gripper_length"), overlays.mechanical_gripper_length).toDouble();
    overlays.mechanical_gripper_width =
        settings->value(QStringLiteral("mechanical_gripper_width"), overlays.mechanical_gripper_width).toDouble();
    settings->endGroup();
    if (!std::isfinite(overlays.mechanical_gripper_length) || overlays.mechanical_gripper_length < 0.0) {
        overlays.mechanical_gripper_length = 0.0;
    }
    if (!std::isfinite(overlays.mechanical_gripper_width) || overlays.mechanical_gripper_width < 0.0) {
        overlays.mechanical_gripper_width = 0.0;
    }
    return overlays;
}

void EngineeringSettingsService::SaveUiOverlaySettings(const UiOverlaySettings& overlays)
{
    auto settings = CreateSettings();
    settings->beginGroup(QStringLiteral("ui_overlay"));
    settings->setValue(QStringLiteral("show_grab_limit_overlay"), overlays.show_grab_limit_overlay);
    settings->setValue(QStringLiteral("mechanical_gripper_length"),
                       overlays.mechanical_gripper_length > 0.0 ? overlays.mechanical_gripper_length : 0.0);
    settings->setValue(QStringLiteral("mechanical_gripper_width"),
                       overlays.mechanical_gripper_width > 0.0 ? overlays.mechanical_gripper_width : 0.0);
    settings->endGroup();
    settings->sync();
}

CoordinateTransformConfig EngineeringSettingsService::LoadCoordinateTransformSettings()
{
    auto settings = CreateSettings();
    settings->beginGroup(QStringLiteral("coordinate_transform"));
    QString selected_profile = settings->value(QStringLiteral("last_profile")).toString().trimmed();
    settings->endGroup();

    MigrateLegacyCoordinateTransformSettings();

    QStringList profile_names = ListCalibrationProfileNames();
    if (selected_profile.isEmpty()) {
        selected_profile = DefaultCalibrationProfileName();
    }
    if (!profile_names.contains(selected_profile) && !profile_names.isEmpty()) {
        selected_profile = profile_names.first();
    }

    CoordinateTransformConfig config;
    QString load_error;
    bool loaded = false;
    if (!profile_names.isEmpty()) {
        loaded = LoadCalibrationProfile(selected_profile, &config, &load_error);
        if (!loaded && selected_profile != profile_names.first()) {
            load_error.clear();
            selected_profile = profile_names.first();
            loaded = LoadCalibrationProfile(selected_profile, &config, &load_error);
        }
    }

    if (!loaded) {
        config = CoordinateTransformConfig{};
        config.profile_name = selected_profile.isEmpty() ? DefaultCalibrationProfileName() : selected_profile;
        config.points.resize(9);
    }

    if (config.profile_name.trimmed().isEmpty()) {
        config.profile_name = selected_profile.isEmpty() ? DefaultCalibrationProfileName() : selected_profile;
    }
    if (config.points.size() < 9) {
        config.points.resize(9);
    }
    if (config.points.size() > 9) {
        config.points.resize(9);
    }
    return config;
}

bool EngineeringSettingsService::SaveCoordinateTransformSettings(const CoordinateTransformConfig& input_config,
                                                                 QString* error_message)
{
    CoordinateTransformConfig config = input_config;
    config.profile_name = SanitizeCalibrationProfileName(config.profile_name);
    if (config.profile_name.isEmpty()) {
        config.profile_name = DefaultCalibrationProfileName();
    }

    QString saved_name;
    QString save_error;
    if (!SaveCalibrationProfile(config, &saved_name, &save_error)) {
        if (error_message != nullptr) {
            *error_message = save_error;
        }
        std::cerr << "[Calibration] save profile failed: "
                  << save_error.toStdString() << std::endl;
        return false;
    }

    SaveLastCoordinateProfile(saved_name);
    auto settings = CreateSettings();
    settings->beginGroup(QStringLiteral("coordinate_transform"));
    settings->setValue(QStringLiteral("legacy_migrated"), true);
    settings->endGroup();
    settings->sync();
    return true;
}

void EngineeringSettingsService::SaveLastCoordinateProfile(const QString& profile_name)
{
    auto settings = CreateSettings();
    settings->beginGroup(QStringLiteral("coordinate_transform"));
    settings->setValue(QStringLiteral("last_profile"), profile_name);
    settings->endGroup();
    settings->sync();
}

void EngineeringSettingsService::MigrateLegacyCoordinateTransformSettings()
{
    auto settings = CreateSettings();
    settings->beginGroup(QStringLiteral("coordinate_transform"));
    if (settings->value(QStringLiteral("legacy_migrated"), false).toBool()) {
        settings->endGroup();
        return;
    }

    const bool has_legacy_values = settings->contains(QStringLiteral("enabled")) ||
                                   settings->contains(QStringLiteral("point_count")) ||
                                   settings->contains(QStringLiteral("point_0_image_x")) ||
                                   settings->contains(QStringLiteral("point_0_machine_x"));
    if (!has_legacy_values) {
        settings->setValue(QStringLiteral("legacy_migrated"), true);
        settings->endGroup();
        return;
    }

    CoordinateTransformConfig legacy_config;
    legacy_config.profile_name = DefaultCalibrationProfileName();
    legacy_config.enabled = settings->value(QStringLiteral("enabled"), false).toBool();
    const int point_count = settings->value(QStringLiteral("point_count"), 9).toInt();
    legacy_config.points.clear();
    legacy_config.points.reserve(9);
    for (int i = 0; i < 9; ++i) {
        CoordinateCalibrationPoint point;
        const QString prefix = QStringLiteral("point_%1").arg(i);
        point.image_x = settings->value(prefix + QStringLiteral("_image_x"), 0.0).toDouble();
        point.image_y = settings->value(prefix + QStringLiteral("_image_y"), 0.0).toDouble();
        point.machine_x = settings->value(prefix + QStringLiteral("_machine_x"), 0.0).toDouble();
        point.machine_y = settings->value(prefix + QStringLiteral("_machine_y"), 0.0).toDouble();
        legacy_config.points.push_back(point);
    }
    if (point_count > 0 && point_count < 9) {
        legacy_config.enabled = false;
    }

    QString saved_name;
    QString save_error;
    if (SaveCalibrationProfile(legacy_config, &saved_name, &save_error)) {
        settings->setValue(QStringLiteral("last_profile"), saved_name);
        settings->setValue(QStringLiteral("legacy_migrated"), true);
    } else {
        std::cerr << "[Calibration] legacy migration failed: "
                  << save_error.toStdString() << std::endl;
    }
    settings->endGroup();
}
