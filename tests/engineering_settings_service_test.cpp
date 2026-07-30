#include "engineering_settings_service.h"

#include <QCoreApplication>
#include <QDir>
#include <QSettings>

#include <cassert>
#include <cmath>
#include <iostream>

namespace {

bool NearlyEqual(double left, double right)
{
    return std::fabs(left - right) < 0.001;
}

QString TemporarySettingsPath()
{
    const QString temp_path = QDir::temp().absoluteFilePath(QStringLiteral("tankeye_settings_service_test"));
    QDir(temp_path).removeRecursively();
    QDir().mkpath(temp_path);
    return QDir(temp_path).absoluteFilePath(QStringLiteral("TankEye-Iris.ini"));
}

void PrepareTemporarySettingsPath(const QString& settings_path)
{
    qputenv("TANKEYE_SETTINGS_INI_PATH", settings_path.toLocal8Bit());
    QSettings settings(settings_path, QSettings::IniFormat);
    settings.clear();
    settings.sync();
}

void CameraSettingsRoundTrip()
{
    CameraSettings camera;
    camera.ip = QStringLiteral("192.168.0.233");
    camera.exposure_us = 1234.5;

    EngineeringSettingsService::SaveCameraSettings(camera);
    const CameraSettings loaded = EngineeringSettingsService::LoadCameraSettings();
    assert(loaded.ip == camera.ip);
    assert(NearlyEqual(loaded.exposure_us, camera.exposure_us));
}

void PlcRelatedSettingsRoundTrip()
{
    AngleCalibrationSettings angle;
    angle.offset_deg = -12.5;
    angle.reverse_direction = true;
    angle.range_mode = AngleRangeMode::Signed180;
    EngineeringSettingsService::SaveAngleCalibrationSettings(angle);

    const AngleCalibrationSettings loaded_angle =
        EngineeringSettingsService::LoadAngleCalibrationSettings();
    assert(NearlyEqual(loaded_angle.offset_deg, angle.offset_deg));
    assert(loaded_angle.reverse_direction);
    assert(loaded_angle.range_mode == AngleRangeMode::Signed180);

    EngineeringSettingsService::SaveAxisMappingMode(AxisMappingMode::FrontBackMachineX);
    assert(EngineeringSettingsService::LoadAxisMappingMode() == AxisMappingMode::FrontBackMachineX);

    AxisCompensationSettings compensation;
    compensation.front_back_offset = 1.25;
    compensation.left_right_offset = -2.5;
    EngineeringSettingsService::SaveAxisCompensationSettings(compensation);
    const AxisCompensationSettings loaded_compensation =
        EngineeringSettingsService::LoadAxisCompensationSettings();
    assert(NearlyEqual(loaded_compensation.front_back_offset, compensation.front_back_offset));
    assert(NearlyEqual(loaded_compensation.left_right_offset, compensation.left_right_offset));

    ObbPostprocessSettings postprocess;
    postprocess.center_ray_offset_px = 12.0;
    postprocess.show_plc_center_debug = true;
    postprocess.show_head_ray_debug = false;
    postprocess.debug_logging_enabled = true;
    EngineeringSettingsService::SaveObbPostprocessSettings(postprocess);
    const ObbPostprocessSettings loaded_postprocess =
        EngineeringSettingsService::LoadObbPostprocessSettings();
    assert(NearlyEqual(loaded_postprocess.center_ray_offset_px, postprocess.center_ray_offset_px));
    assert(loaded_postprocess.show_plc_center_debug);
    assert(!loaded_postprocess.show_head_ray_debug);
    assert(loaded_postprocess.debug_logging_enabled);

    ModelThresholdSettings thresholds;
    thresholds.obb_conf_threshold = 0.62;
    thresholds.obb_nms_threshold = 0.35;
    thresholds.seg_conf_threshold = 0.44;
    thresholds.seg_nms_threshold = 0.50;
    EngineeringSettingsService::SaveModelThresholdSettings(thresholds);
    const ModelThresholdSettings loaded_thresholds =
        EngineeringSettingsService::LoadModelThresholdSettings();
    assert(NearlyEqual(loaded_thresholds.obb_conf_threshold, thresholds.obb_conf_threshold));
    assert(NearlyEqual(loaded_thresholds.obb_nms_threshold, thresholds.obb_nms_threshold));
    assert(NearlyEqual(loaded_thresholds.seg_conf_threshold, thresholds.seg_conf_threshold));
    assert(NearlyEqual(loaded_thresholds.seg_nms_threshold, thresholds.seg_nms_threshold));
}

void GrabLimitLegacyDefaultsAreSanitized()
{
    QSettings settings(QString::fromLocal8Bit(qgetenv("TANKEYE_SETTINGS_INI_PATH")), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("grab_limits"));
    settings.setValue(QStringLiteral("x_lower"), -999999.0);
    settings.setValue(QStringLiteral("x_upper"), 999999.0);
    settings.setValue(QStringLiteral("y_lower"), -999999.0);
    settings.setValue(QStringLiteral("y_upper"), 999999.0);
    settings.setValue(QStringLiteral("angle_lower"), -360.0);
    settings.setValue(QStringLiteral("angle_upper"), 360.0);
    settings.setValue(QStringLiteral("roi_margin"), -1.0);
    settings.endGroup();

    const GrabLimitConfig loaded = EngineeringSettingsService::LoadGrabLimits({});
    assert(NearlyEqual(loaded.x.lower, 0.0));
    assert(NearlyEqual(loaded.x.upper, 0.0));
    assert(NearlyEqual(loaded.y.lower, 0.0));
    assert(NearlyEqual(loaded.y.upper, 0.0));
    assert(NearlyEqual(loaded.angle.lower, 0.0));
    assert(NearlyEqual(loaded.angle.upper, 0.0));
    assert(NearlyEqual(loaded.roi_margin, 5.0));
}

void SettingsOverrideProvidedDefaults()
{
    GrabLimitConfig limit_defaults;
    limit_defaults.enabled = true;
    limit_defaults.roi_margin = 7.0;
    limit_defaults.x = { 10.0, 20.0 };
    limit_defaults.y = { 30.0, 40.0 };
    limit_defaults.angle = { 50.0, 60.0 };

    const GrabLimitConfig loaded_limits = EngineeringSettingsService::LoadGrabLimits(limit_defaults);
    assert(NearlyEqual(loaded_limits.roi_margin, 7.0));
    assert(NearlyEqual(loaded_limits.x.lower, 10.0));

    CameraSettings camera_defaults;
    camera_defaults.ip = QStringLiteral("192.168.8.8");
    camera_defaults.exposure_us = 456.0;
    const CameraSettings loaded_camera = EngineeringSettingsService::LoadCameraSettings(camera_defaults);
    assert(loaded_camera.ip == camera_defaults.ip);
    assert(NearlyEqual(loaded_camera.exposure_us, camera_defaults.exposure_us));

    EngineeringSettingsService::SaveCameraSettings({ QStringLiteral("192.168.9.9"), 789.0 });
    const CameraSettings overridden_camera = EngineeringSettingsService::LoadCameraSettings(camera_defaults);
    assert(overridden_camera.ip == QStringLiteral("192.168.9.9"));
    assert(NearlyEqual(overridden_camera.exposure_us, 789.0));
}

} // namespace

int main(int argc, char** argv)
{
    const QString settings_path = TemporarySettingsPath();
    PrepareTemporarySettingsPath(settings_path);
    QCoreApplication app(argc, argv);

    CameraSettingsRoundTrip();
    PlcRelatedSettingsRoundTrip();
    GrabLimitLegacyDefaultsAreSanitized();
    PrepareTemporarySettingsPath(settings_path);
    SettingsOverrideProvidedDefaults();

    std::cout << "engineering_settings_service_test passed" << std::endl;
    return 0;
}
