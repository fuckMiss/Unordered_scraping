#include "grasp_main_window.h"

#include "engineering_settings_service.h"

#include <QtGlobal>

void GraspMainWindow::loadLimitSettings()
{
    grab_limits_ = EngineeringSettingsService::LoadGrabLimits(app_config_.machine_limits);
}

void GraspMainWindow::loadCameraSettings()
{
    const CameraSettings camera = EngineeringSettingsService::LoadCameraSettings(app_config_.camera);
    camera_ip_ = camera.ip;
    camera_exposure_us_ = camera.exposure_us;
    if (camera_ip_.isEmpty()) {
        camera_ip_ = QString::fromLocal8Bit(qgetenv("TANKEYE_CAMERA_IP")).trimmed();
    }
    workflow_.setCameraIp(camera_ip_.toStdString());
    workflow_.setCameraExposureUs(camera_exposure_us_);
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
    show_plc_center_debug_ = postprocess.show_plc_center_debug;
    show_head_ray_debug_ = postprocess.show_head_ray_debug;
    postprocess_debug_logging_enabled_ = postprocess.debug_logging_enabled;
    workflow_.setCenterRayOffsetPx(center_ray_offset_px_);
    workflow_.setPostprocessDebugLoggingEnabled(postprocess_debug_logging_enabled_);
}

void GraspMainWindow::loadUiOverlaySettings()
{
    const UiOverlaySettings overlays = EngineeringSettingsService::LoadUiOverlaySettings();
    show_grab_limit_overlay_ = overlays.show_grab_limit_overlay;
    mechanical_gripper_length_ = overlays.mechanical_gripper_length;
    mechanical_gripper_width_ = overlays.mechanical_gripper_width;
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
