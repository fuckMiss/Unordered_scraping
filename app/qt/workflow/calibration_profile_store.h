#pragma once

#include "coordinate_transform.h"

#include <QString>
#include <QStringList>

QString CalibrationProfilesDirectory();
QString SanitizeCalibrationProfileName(const QString& name);
QString DefaultCalibrationProfileName();
QString CalibrationProfilePath(const QString& name);
QStringList ListCalibrationProfileNames();
bool LoadCalibrationProfile(const QString& name,
                            CoordinateTransformConfig* config,
                            QString* error_message);
bool SaveCalibrationProfile(const CoordinateTransformConfig& config,
                            QString* saved_name,
                            QString* error_message);
bool DeleteCalibrationProfile(const QString& name, QString* error_message);
