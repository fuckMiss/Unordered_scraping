#include "grasp_main_window.h"

#include "app_startup_manager.h"
#include "engineering_settings_dialog_helpers.h"
#include "engineering_settings_service.h"

#include <QtGlobal>

#include <iostream>

namespace {

const char* HeadTypeName(int head_type_code)
{
    switch (head_type_code) {
    case 1: return "large_upper_right";
    case 2: return "small_upper_left";
    case 3: return "large_upper_left";
    case 4: return "small_upper_right";
    default: return "unknown";
    }
}

void LogHeadTypeCompensationSettings(const char* action,
                                     const HeadTypeCompensationSettings& settings)
{
    std::cout << "[PLC_DEBUG] head_type_compensation_" << action;
    for (int head_type_code : {1, 3, 4, 2}) {
        const HeadTypeCompensation& compensation = settings.types[head_type_code];
        std::cout << " [" << HeadTypeName(head_type_code)
                  << ",D508=" << head_type_code
                  << ",angle_offset_deg=" << compensation.angle_offset_deg
                  << ",ac_ray_offset_mm=" << compensation.ac_ray_offset_mm << "]";
    }
    std::cout << std::endl;
}

} // namespace

void GraspMainWindow::loadLimitSettings()
{
    grab_limits_ = EngineeringSettingsService::LoadGrabLimits(app_config_.machine_limits);
}

void GraspMainWindow::loadCameraSettings()
{
    camera_ip_ = EngineeringSettingsService::LoadCameraIp(app_config_.camera.ip);
    if (camera_ip_.isEmpty()) {
        camera_ip_ = QString::fromLocal8Bit(qgetenv("TANKEYE_CAMERA_IP")).trimmed();
    }
    workflow_.setCameraIp(camera_ip_.toStdString());
}

void GraspMainWindow::loadModelThresholdSettings()
{
    const ModelThresholdSettings thresholds = EngineeringSettingsService::LoadModelThresholdSettings();
    obb_conf_threshold_ = thresholds.obb_conf_threshold;
    obb_nms_threshold_ = thresholds.obb_nms_threshold;
    seg_conf_threshold_ = thresholds.seg_conf_threshold;
    seg_nms_threshold_ = thresholds.seg_nms_threshold;
}

void GraspMainWindow::loadAngleCalibrationSettings()
{
    const AngleCalibrationSettings angle =
        EngineeringSettingsService::LoadAngleCalibrationSettings(app_config_.angle_calibration);
    angle_offset_deg_ = angle.offset_deg;
    angle_reverse_direction_ = angle.reverse_direction;
    angle_range_mode_ = angle.range_mode;

    robot_controller_.setAngleCalibration(static_cast<float>(angle_offset_deg_),
                                          angle_reverse_direction_,
                                          angle_range_mode_);
}

void GraspMainWindow::loadObbPostprocessSettings()
{
    const ObbPostprocessSettings postprocess = EngineeringSettingsService::LoadObbPostprocessSettings();
    center_ray_offset_px_ = postprocess.center_ray_offset_px;
    show_head_type_adjusted_geometry_ = postprocess.show_head_type_adjusted_geometry;
    show_plc_center_debug_ = postprocess.show_plc_center_debug;
    show_head_ray_debug_ = postprocess.show_head_ray_debug;
    postprocess_debug_logging_enabled_ = postprocess.debug_logging_enabled;
    if (qEnvironmentVariableIntValue("TANKEYE_DEBUG_POSTPROCESS") == 1) {
        postprocess_debug_logging_enabled_ = true;
    }
    workflow_.setCenterRayOffsetPx(center_ray_offset_px_);
    workflow_.setPostprocessDebugLoggingEnabled(postprocess_debug_logging_enabled_);
    robot_controller_.setDebugLoggingEnabled(postprocess_debug_logging_enabled_);
}

void GraspMainWindow::loadStartupLaunchSettings()
{
    const StartupLaunchSettings startup = EngineeringSettingsService::LoadStartupLaunchSettings();
    auto_start_enabled_ = startup.auto_start_enabled;
    startup_delay_seconds_ = ClampStartupDelaySeconds(startup.delay_seconds);
}

bool GraspMainWindow::saveStartupLaunchSettings(QString* error_message) const
{
    const StartupLaunchSettings startup{
        auto_start_enabled_,
        ClampStartupDelaySeconds(startup_delay_seconds_),
    };
    EngineeringSettingsService::SaveStartupLaunchSettings(startup);

    const StartupShortcutSyncResult sync_result = SyncStartupShortcut(startup);
    if (!sync_result.success) {
        if (error_message != nullptr) {
            *error_message = sync_result.error_message;
        }
        std::cerr << "[Startup] sync failed: "
                  << sync_result.error_message.toStdString() << std::endl;
        return false;
    }
    std::cout << "[Startup] "
              << (startup.auto_start_enabled ? "enabled" : "disabled")
              << " entry=" << sync_result.startup_entry_path.toStdString()
              << " delay_seconds=" << startup.delay_seconds << std::endl;
    return true;
}

void GraspMainWindow::loadUiOverlaySettings()
{
    const UiOverlaySettings overlays = EngineeringSettingsService::LoadUiOverlaySettings();
    show_grab_limit_overlay_ = overlays.show_grab_limit_overlay;
    mechanical_gripper_length_ = overlays.mechanical_gripper_length;
    mechanical_gripper_width_ = overlays.mechanical_gripper_width;
}

void GraspMainWindow::loadHeadTypeCompensationSettings()
{
    head_type_compensation_settings_ = EngineeringSettingsService::LoadHeadTypeCompensationSettings();
    robot_controller_.setHeadTypeCompensations(head_type_compensation_settings_.types);
    if (postprocess_debug_logging_enabled_) {
        LogHeadTypeCompensationSettings("loaded", head_type_compensation_settings_);
    }
}

void GraspMainWindow::loadAxisMappingSettings()
{
    axis_mapping_mode_ = EngineeringSettingsService::LoadAxisMappingMode(app_config_.axis_mapping_mode);
    robot_controller_.setAxisMapping(axis_mapping_mode_);
}

void GraspMainWindow::loadAxisCompensationSettings()
{
    const AxisCompensationSettings compensation =
        EngineeringSettingsService::LoadAxisCompensationSettings(app_config_.axis_compensation);
    front_back_offset_ = compensation.front_back_offset;
    left_right_offset_ = compensation.left_right_offset;

    robot_controller_.setAxisCompensation(static_cast<float>(front_back_offset_),
                                          static_cast<float>(left_right_offset_));
}

void GraspMainWindow::loadCoordinateTransformSettings()
{
    coordinate_transform_config_ = EngineeringSettingsService::LoadCoordinateTransformSettings();
    rebuildCoordinateTransformState();
}

ProjectProfileSettings GraspMainWindow::legacyProjectProfileDefaults() const
{
    ProjectProfileSettings profile;
    profile.name = DefaultProjectProfileName();
    profile.obb_model_path = obb_model_path_;
    profile.seg_model_path = seg_model_path_;
    profile.camera_exposure_us = camera_exposure_us_;
    profile.model_thresholds = EngineeringSettingsService::LoadModelThresholdSettings();
    profile.angle_calibration = EngineeringSettingsService::LoadAngleCalibrationSettings(app_config_.angle_calibration);
    profile.obb_postprocess = EngineeringSettingsService::LoadObbPostprocessSettings();
    profile.ui_overlay = EngineeringSettingsService::LoadUiOverlaySettings();
    profile.head_type_compensation =
        EngineeringSettingsService::LoadHeadTypeCompensationSettings();
    profile.axis_mapping_mode =
        EngineeringSettingsService::LoadAxisMappingMode(app_config_.axis_mapping_mode);
    profile.axis_compensation =
        EngineeringSettingsService::LoadAxisCompensationSettings(app_config_.axis_compensation);
    profile.grab_limits = EngineeringSettingsService::LoadGrabLimits(app_config_.machine_limits);
    profile.coordinate_transform = EngineeringSettingsService::LoadCoordinateTransformSettings();
    return profile;
}

void GraspMainWindow::applyProjectProfileSettings(const ProjectProfileSettings& profile)
{
    current_project_profile_name_ = profile.name;
    obb_model_path_ = profile.obb_model_path;
    seg_model_path_ = profile.seg_model_path;
    camera_exposure_us_ = profile.camera_exposure_us;
    obb_conf_threshold_ = profile.model_thresholds.obb_conf_threshold;
    obb_nms_threshold_ = profile.model_thresholds.obb_nms_threshold;
    seg_conf_threshold_ = profile.model_thresholds.seg_conf_threshold;
    seg_nms_threshold_ = profile.model_thresholds.seg_nms_threshold;
    angle_offset_deg_ = profile.angle_calibration.offset_deg;
    angle_reverse_direction_ = profile.angle_calibration.reverse_direction;
    angle_range_mode_ = profile.angle_calibration.range_mode;
    center_ray_offset_px_ = profile.obb_postprocess.center_ray_offset_px;
    show_head_type_adjusted_geometry_ = profile.obb_postprocess.show_head_type_adjusted_geometry;
    show_plc_center_debug_ = profile.obb_postprocess.show_plc_center_debug;
    show_head_ray_debug_ = profile.obb_postprocess.show_head_ray_debug;
    postprocess_debug_logging_enabled_ = profile.obb_postprocess.debug_logging_enabled;
    if (qEnvironmentVariableIntValue("TANKEYE_DEBUG_POSTPROCESS") == 1) {
        postprocess_debug_logging_enabled_ = true;
    }
    show_grab_limit_overlay_ = profile.ui_overlay.show_grab_limit_overlay;
    mechanical_gripper_length_ = profile.ui_overlay.mechanical_gripper_length;
    mechanical_gripper_width_ = profile.ui_overlay.mechanical_gripper_width;
    head_type_compensation_settings_ = profile.head_type_compensation;
    axis_mapping_mode_ = profile.axis_mapping_mode;
    front_back_offset_ = profile.axis_compensation.front_back_offset;
    left_right_offset_ = profile.axis_compensation.left_right_offset;
    grab_limits_ = profile.grab_limits;
    coordinate_transform_config_ = profile.coordinate_transform;
    rebuildCoordinateTransformState();

    workflow_.setCameraIp(camera_ip_.toStdString());
    workflow_.setCameraExposureUs(camera_exposure_us_);
    workflow_.setCenterRayOffsetPx(center_ray_offset_px_);
    workflow_.setPostprocessDebugLoggingEnabled(postprocess_debug_logging_enabled_);
    robot_controller_.setAngleCalibration(static_cast<float>(angle_offset_deg_),
                                          angle_reverse_direction_,
                                          angle_range_mode_);
    robot_controller_.setAxisMapping(axis_mapping_mode_);
    robot_controller_.setAxisCompensation(static_cast<float>(front_back_offset_),
                                          static_cast<float>(left_right_offset_));
    robot_controller_.setHeadTypeCompensations(head_type_compensation_settings_.types);
    robot_controller_.setDebugLoggingEnabled(postprocess_debug_logging_enabled_);
    if (postprocess_debug_logging_enabled_) {
        LogHeadTypeCompensationSettings("loaded", head_type_compensation_settings_);
    }
}

void GraspMainWindow::loadProjectProfileSettings()
{
    QString error_message;
    if (!EnsureDefaultProjectProfile(legacyProjectProfileDefaults(), &error_message)) {
        std::cerr << "[ProjectProfile] ensure default failed: "
                  << error_message.toStdString() << std::endl;
    }

    QString profile_name = LoadActiveProjectProfileName();
    ProjectProfileSettings profile;
    if (!LoadProjectProfile(profile_name, &profile, &error_message)) {
        std::cerr << "[ProjectProfile] load active failed: "
                  << error_message.toStdString() << std::endl;
        profile_name = DefaultProjectProfileName();
        if (!LoadProjectProfile(profile_name, &profile, &error_message)) {
            profile = legacyProjectProfileDefaults();
            profile.name = profile_name;
        }
    }
    applyProjectProfileSettings(profile);
    active_project_profile_name_ = current_project_profile_name_;
    SaveActiveProjectProfileName(active_project_profile_name_);
}

ProjectProfileSettings GraspMainWindow::currentProjectProfileSettings(const QString& profile_name) const
{
    ProjectProfileSettings profile;
    profile.name = profile_name.trimmed().isEmpty() ? current_project_profile_name_ : profile_name;
    profile.obb_model_path = obb_model_path_;
    profile.seg_model_path = seg_model_path_;
    profile.camera_exposure_us = camera_exposure_us_;
    profile.model_thresholds = {
        obb_conf_threshold_,
        obb_nms_threshold_,
        seg_conf_threshold_,
        seg_nms_threshold_,
    };
    profile.angle_calibration = {
        angle_offset_deg_,
        angle_reverse_direction_,
        angle_range_mode_,
    };
    profile.obb_postprocess = {
        center_ray_offset_px_,
        show_head_type_adjusted_geometry_,
        show_plc_center_debug_,
        show_head_ray_debug_,
        postprocess_debug_logging_enabled_,
    };
    profile.ui_overlay = {
        show_grab_limit_overlay_,
        mechanical_gripper_length_,
        mechanical_gripper_width_,
    };
    profile.head_type_compensation = head_type_compensation_settings_;
    profile.axis_mapping_mode = axis_mapping_mode_;
    profile.axis_compensation = {
        front_back_offset_,
        left_right_offset_,
    };
    profile.grab_limits = grab_limits_;
    profile.coordinate_transform = coordinate_transform_config_;
    return profile;
}

ProjectProfileSettings GraspMainWindow::projectProfileSettingsFromDraft(
    const EngineeringSettingsDraft& draft,
    const QString& profile_name) const
{
    ProjectProfileSettings profile;
    profile.name = profile_name;
    profile.obb_model_path = draft.obb_model_path;
    profile.seg_model_path = draft.seg_model_path;
    profile.camera_exposure_us = draft.camera_exposure_us;
    profile.model_thresholds = {
        draft.obb_conf_threshold,
        draft.obb_nms_threshold,
        draft.seg_conf_threshold,
        draft.seg_nms_threshold,
    };
    profile.angle_calibration = {
        draft.angle_offset_deg,
        draft.angle_reverse_direction,
        draft.angle_range_mode,
    };
    profile.obb_postprocess = {
        draft.center_ray_offset_px,
        draft.show_head_type_adjusted_geometry,
        draft.show_plc_center_debug,
        draft.show_head_ray_debug,
        draft.postprocess_debug_logging_enabled,
    };
    profile.ui_overlay = {
        draft.show_grab_limit_overlay,
        draft.mechanical_gripper_length,
        draft.mechanical_gripper_width,
    };
    profile.head_type_compensation.types = draft.head_type_compensations;
    profile.axis_mapping_mode = draft.axis_mapping_mode;
    profile.axis_compensation = {
        draft.front_back_offset,
        draft.left_right_offset,
    };
    profile.grab_limits = draft.grab_limits;
    profile.coordinate_transform = draft.coordinate_transform_config;
    return profile;
}

bool GraspMainWindow::saveProjectProfileSettings(QString* error_message) const
{
    QString saved_name;
    if (!SaveProjectProfile(currentProjectProfileSettings(current_project_profile_name_), &saved_name, error_message)) {
        return false;
    }
    SaveActiveProjectProfileName(saved_name);
    return true;
}

void GraspMainWindow::saveCameraSettings() const
{
    EngineeringSettingsService::SaveCameraSettings({ camera_ip_, camera_exposure_us_ });
}

void GraspMainWindow::saveModelThresholdSettings() const
{
    EngineeringSettingsService::SaveModelThresholdSettings({
        obb_conf_threshold_,
        obb_nms_threshold_,
        seg_conf_threshold_,
        seg_nms_threshold_,
    });
}

void GraspMainWindow::saveAngleCalibrationSettings() const
{
    EngineeringSettingsService::SaveAngleCalibrationSettings({
        angle_offset_deg_,
        angle_reverse_direction_,
        angle_range_mode_,
    });
}

void GraspMainWindow::saveObbPostprocessSettings() const
{
    EngineeringSettingsService::SaveObbPostprocessSettings({
        center_ray_offset_px_,
        show_head_type_adjusted_geometry_,
        show_plc_center_debug_,
        show_head_ray_debug_,
        postprocess_debug_logging_enabled_,
    });
}

void GraspMainWindow::saveUiOverlaySettings() const
{
    EngineeringSettingsService::SaveUiOverlaySettings({
        show_grab_limit_overlay_,
        mechanical_gripper_length_,
        mechanical_gripper_width_,
    });
}

void GraspMainWindow::saveHeadTypeCompensationSettings() const
{
    EngineeringSettingsService::SaveHeadTypeCompensationSettings(head_type_compensation_settings_);
    if (postprocess_debug_logging_enabled_) {
        LogHeadTypeCompensationSettings("saved", head_type_compensation_settings_);
    }
}

void GraspMainWindow::saveAxisMappingSettings() const
{
    EngineeringSettingsService::SaveAxisMappingMode(axis_mapping_mode_);
}

void GraspMainWindow::saveAxisCompensationSettings() const
{
    EngineeringSettingsService::SaveAxisCompensationSettings({
        front_back_offset_,
        left_right_offset_,
    });
}

bool GraspMainWindow::saveCoordinateTransformSettings(QString* error_message) const
{
    return EngineeringSettingsService::SaveCoordinateTransformSettings(coordinate_transform_config_,
                                                                       error_message);
}

void GraspMainWindow::rebuildCoordinateTransformState()
{
    coordinate_transform_state_ = BuildCoordinateTransformState(coordinate_transform_config_);
}

void GraspMainWindow::saveLimitSettings() const
{
    EngineeringSettingsService::SaveGrabLimits(grab_limits_);
}

float GraspMainWindow::calibratedAngle(float angle_deg) const
{
    return ApplyPlcAngleCalibration(angle_deg, plcOutputConfig());
}

PlcOutputConfig GraspMainWindow::plcOutputConfig() const
{
    PlcOutputConfig config;
    config.angle_offset_deg = static_cast<float>(angle_offset_deg_);
    config.angle_reverse_direction = angle_reverse_direction_;
    config.angle_range_mode = angle_range_mode_;
    config.axis_mapping_mode = axis_mapping_mode_;
    config.front_back_offset = static_cast<float>(front_back_offset_);
    config.left_right_offset = static_cast<float>(left_right_offset_);
    config.head_type_compensations = head_type_compensation_settings_.types;
    config.debug_logging_enabled = postprocess_debug_logging_enabled_;
    return config;
}

QString GraspMainWindow::frontBackAxisLabel() const
{
    return axis_mapping_mode_ == AxisMappingMode::FrontBackMachineX
        ? QStringLiteral("前后 / 机械X")
        : QStringLiteral("前后 / 机械Y");
}

QString GraspMainWindow::leftRightAxisLabel() const
{
    return axis_mapping_mode_ == AxisMappingMode::FrontBackMachineX
        ? QStringLiteral("左右 / 机械Y")
        : QStringLiteral("左右 / 机械X");
}
