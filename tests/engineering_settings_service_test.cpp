#include "engineering_settings_service.h"
#include "app_startup_manager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
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

void ExplicitSettingsIniPathIsUsed(const QString& settings_path)
{
    EngineeringSettingsService::SaveCameraSettings({ QStringLiteral("10.20.30.40"), 456.0 });
    assert(QFile::exists(settings_path));

    QSettings settings(settings_path, QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("camera"));
    assert(settings.value(QStringLiteral("ip")).toString() == QStringLiteral("10.20.30.40"));
    assert(NearlyEqual(settings.value(QStringLiteral("exposure_us")).toDouble(), 456.0));
    settings.endGroup();
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

    const HeadTypeCompensationSettings default_head_compensation =
        EngineeringSettingsService::LoadHeadTypeCompensationSettings();
    for (int head_type = 1; head_type < static_cast<int>(default_head_compensation.types.size()); ++head_type) {
        assert(NearlyEqual(default_head_compensation.types[head_type].angle_offset_deg, 0.0));
        assert(NearlyEqual(default_head_compensation.types[head_type].ac_ray_offset_mm, 0.0));
    }

    HeadTypeCompensationSettings head_compensation;
    const int head_types[] = { 1, 3, 4, 2 };
    for (const int head_type : head_types) {
        head_compensation.types[head_type].angle_offset_deg = head_type * 1.5;
        head_compensation.types[head_type].ac_ray_offset_mm = -head_type * 2.0;
    }
    EngineeringSettingsService::SaveHeadTypeCompensationSettings(head_compensation);
    const HeadTypeCompensationSettings loaded_head_compensation =
        EngineeringSettingsService::LoadHeadTypeCompensationSettings();
    for (const int head_type : head_types) {
        assert(NearlyEqual(loaded_head_compensation.types[head_type].angle_offset_deg,
                           head_compensation.types[head_type].angle_offset_deg));
        assert(NearlyEqual(loaded_head_compensation.types[head_type].ac_ray_offset_mm,
                           head_compensation.types[head_type].ac_ray_offset_mm));
    }

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

    assert(EngineeringSettingsService::LoadUiOverlaySettings().show_grab_limit_overlay);
    EngineeringSettingsService::SaveUiOverlaySettings({ false, 88.5, 12.25 });
    const UiOverlaySettings loaded_overlay = EngineeringSettingsService::LoadUiOverlaySettings();
    assert(!loaded_overlay.show_grab_limit_overlay);
    assert(NearlyEqual(loaded_overlay.mechanical_gripper_length, 88.5));
    assert(NearlyEqual(loaded_overlay.mechanical_gripper_width, 12.25));

    const StartupLaunchSettings default_startup = EngineeringSettingsService::LoadStartupLaunchSettings();
    assert(default_startup.auto_start_enabled);
    assert(default_startup.delay_seconds == 0);
    EngineeringSettingsService::SaveStartupLaunchSettings({ false, 45 });
    const StartupLaunchSettings loaded_startup = EngineeringSettingsService::LoadStartupLaunchSettings();
    assert(!loaded_startup.auto_start_enabled);
    assert(loaded_startup.delay_seconds == 45);
    EngineeringSettingsService::SaveStartupLaunchSettings({ true, 999 });
    assert(EngineeringSettingsService::LoadStartupLaunchSettings().delay_seconds == 600);

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

void StartupShortcutSyncUsesRequestedDirectory()
{
    assert(BuildStartupLaunchArguments(15) ==
           QStringLiteral("-StartupProfile AutoStart -StartupDelaySeconds 15"));
    assert(BuildStartupLaunchArguments(999).contains(QStringLiteral("-StartupDelaySeconds 600")));

    const QString temp_root = QDir::temp().absoluteFilePath(QStringLiteral("tankeye_startup_shortcut_test"));
    QDir(temp_root).removeRecursively();
    QDir().mkpath(temp_root);
    const QString app_dir = QDir(temp_root).absoluteFilePath(QStringLiteral("app"));
    const QString startup_dir = QDir(temp_root).absoluteFilePath(QStringLiteral("Startup"));
    QDir().mkpath(app_dir);
    QFile launch_script(QDir(app_dir).absoluteFilePath(QStringLiteral("launch_tankeye.ps1")));
    assert(launch_script.open(QIODevice::WriteOnly | QIODevice::Text));
    launch_script.write("param()\n");
    launch_script.close();

    QFile legacy_startup_entry(QDir(startup_dir).absoluteFilePath(QStringLiteral("TankEye-Iris.vbs")));
    assert(legacy_startup_entry.open(QIODevice::WriteOnly | QIODevice::Text));
    legacy_startup_entry.write("legacy entry\r\n");
    legacy_startup_entry.close();
    QFile legacy_lnk_entry(QDir(startup_dir).absoluteFilePath(QStringLiteral("TankEye-Iris.lnk")));
    assert(legacy_lnk_entry.open(QIODevice::WriteOnly | QIODevice::Text));
    legacy_lnk_entry.write("legacy shortcut\r\n");
    legacy_lnk_entry.close();

    const StartupShortcutSyncResult enabled =
        SyncStartupShortcut({ true, 15 }, app_dir, startup_dir);
    assert(enabled.success);
    assert(QFile::exists(enabled.startup_entry_path));

    QFile startup_entry(enabled.startup_entry_path);
    assert(startup_entry.open(QIODevice::ReadOnly));
    const QByteArray content = startup_entry.readAll();
    assert(content.contains("-StartupDelaySeconds 15"));
    assert(content.contains("-StartupProfile AutoStart"));
    assert(!content.contains("legacy entry"));
    assert(!QFile::exists(QDir(startup_dir).absoluteFilePath(QStringLiteral("TankEye-Iris.lnk"))));
    assert(content.contains("launch_tankeye.ps1"));
    assert(!content.contains("_\r\n\r\n"));
    assert(!content.contains("_\n\n"));
    assert(content.contains(
        "command = \"powershell.exe -NoProfile -ExecutionPolicy Bypass -File \" & _\r\n"
        "          \"\"\"\" & launchScript & \"\"\"\" & \" -StartupProfile AutoStart -StartupDelaySeconds 15\"\r\n"));

    const StartupShortcutSyncResult disabled =
        SyncStartupShortcut({ false, 15 }, app_dir, startup_dir);
    assert(disabled.success);
    assert(!QFile::exists(enabled.startup_entry_path));
}

} // namespace

int main(int argc, char** argv)
{
    const QString settings_path = TemporarySettingsPath();
    PrepareTemporarySettingsPath(settings_path);
    QCoreApplication app(argc, argv);

    ExplicitSettingsIniPathIsUsed(settings_path);
    CameraSettingsRoundTrip();
    PlcRelatedSettingsRoundTrip();
    StartupShortcutSyncUsesRequestedDirectory();
    GrabLimitLegacyDefaultsAreSanitized();
    PrepareTemporarySettingsPath(settings_path);
    SettingsOverrideProvidedDefaults();

    std::cout << "engineering_settings_service_test passed" << std::endl;
    return 0;
}
