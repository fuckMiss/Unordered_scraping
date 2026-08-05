#pragma once

#include "coordinate_transform.h"
#include "frame_postprocess.h"
#include "grab_limit_evaluator.h"
#include "plc_result_contract.h"

#include <array>

#include <QString>

struct CameraSettings
{
    QString ip;
    double exposure_us = 0.0;
};

struct AngleCalibrationSettings
{
    double offset_deg = 0.0;
    bool reverse_direction = false;
    AngleRangeMode range_mode = AngleRangeMode::ZeroTo360;
};

struct ObbPostprocessSettings
{
    double center_ray_offset_px = 10.0;
    bool show_plc_center_debug = false;
    bool show_head_ray_debug = true;
    bool debug_logging_enabled = false;
};

struct ModelThresholdSettings
{
    double obb_conf_threshold = 0.5;
    double obb_nms_threshold = 0.3;
    double seg_conf_threshold = 0.3;
    double seg_nms_threshold = 0.4;
};

struct AxisCompensationSettings
{
    double front_back_offset = 0.0;
    double left_right_offset = 0.0;
};

struct UiOverlaySettings
{
    bool show_grab_limit_overlay = true;
    double mechanical_gripper_length = 0.0;
    double mechanical_gripper_width = 0.0;
};

struct HeadTypeCompensationSettings
{
    std::array<HeadTypeCompensation, 5> types{};
};

class EngineeringSettingsService
{
public:
    static GrabLimitConfig LoadGrabLimits(const GrabLimitConfig& defaults);
    static void SaveGrabLimits(const GrabLimitConfig& config);

    static CameraSettings LoadCameraSettings();
    static CameraSettings LoadCameraSettings(const CameraSettings& defaults);
    static void SaveCameraSettings(const CameraSettings& settings);

    static AngleCalibrationSettings LoadAngleCalibrationSettings();
    static AngleCalibrationSettings LoadAngleCalibrationSettings(const AngleCalibrationSettings& defaults);
    static void SaveAngleCalibrationSettings(const AngleCalibrationSettings& settings);

    static ObbPostprocessSettings LoadObbPostprocessSettings();
    static void SaveObbPostprocessSettings(const ObbPostprocessSettings& settings);

    static ModelThresholdSettings LoadModelThresholdSettings();
    static void SaveModelThresholdSettings(const ModelThresholdSettings& settings);

    static AxisMappingMode LoadAxisMappingMode();
    static AxisMappingMode LoadAxisMappingMode(AxisMappingMode default_mode);
    static void SaveAxisMappingMode(AxisMappingMode mode);

    static AxisCompensationSettings LoadAxisCompensationSettings();
    static AxisCompensationSettings LoadAxisCompensationSettings(const AxisCompensationSettings& defaults);
    static void SaveAxisCompensationSettings(const AxisCompensationSettings& settings);

    static UiOverlaySettings LoadUiOverlaySettings();
    static void SaveUiOverlaySettings(const UiOverlaySettings& settings);

    static HeadTypeCompensationSettings LoadHeadTypeCompensationSettings();
    static HeadTypeCompensationSettings LoadHeadTypeCompensationSettings(const HeadTypeCompensationSettings& defaults);
    static void SaveHeadTypeCompensationSettings(const HeadTypeCompensationSettings& settings);

    static CoordinateTransformConfig LoadCoordinateTransformSettings();
    static bool SaveCoordinateTransformSettings(const CoordinateTransformConfig& config,
                                                QString* error_message = nullptr);
    static void SaveLastCoordinateProfile(const QString& profile_name);

private:
    static void MigrateLegacyCoordinateTransformSettings();
};
