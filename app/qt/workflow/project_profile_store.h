#pragma once

#include "coordinate_transform.h"
#include "engineering_settings_service.h"

#include <QString>
#include <QStringList>

struct ProjectProfileSettings
{
    QString name;
    QString obb_model_path;
    QString seg_model_path;
    double camera_exposure_us = 0.0;
    ModelThresholdSettings model_thresholds;
    AngleCalibrationSettings angle_calibration;
    ObbPostprocessSettings obb_postprocess;
    UiOverlaySettings ui_overlay;
    HeadTypeCompensationSettings head_type_compensation;
    AxisMappingMode axis_mapping_mode = AxisMappingMode::FrontBackMachineY;
    AxisCompensationSettings axis_compensation;
    GrabLimitConfig grab_limits;
    CoordinateTransformConfig coordinate_transform;
};

ProjectProfileSettings CreateBlankProjectProfile(const QString& name);
QString ProjectProfilesDirectory();
QString DefaultProjectProfileName();
QString SanitizeProjectProfileName(const QString& name);
QString ProjectProfileDirectory(const QString& name);
QString ProjectProfilePath(const QString& name);
QStringList ListProjectProfileNames();
QString LoadActiveProjectProfileName();
void SaveActiveProjectProfileName(const QString& name);
bool LoadProjectProfile(const QString& name,
                        ProjectProfileSettings* profile,
                        QString* error_message);
bool SaveProjectProfile(const ProjectProfileSettings& profile,
                        QString* saved_name,
                        QString* error_message);
bool DeleteProjectProfile(const QString& name, QString* error_message);
bool EnsureDefaultProjectProfile(const ProjectProfileSettings& defaults,
                                 QString* error_message);
bool ExportProjectProfile(const QString& name,
                          const QString& destination_path,
                          bool include_models,
                          QString* error_message);
bool ImportProjectProfile(const QString& source_path,
                          bool* replaced_existing,
                          QString* imported_name,
                          QString* error_message);
QString ResolveProjectProfileModelPath(const ProjectProfileSettings& profile,
                                       const QString& model_path);
