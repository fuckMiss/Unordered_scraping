#include "calibration_profile_store.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {

constexpr int kProfileVersion = 1;

QJsonObject PointToJson(const CoordinateCalibrationPoint& point)
{
    QJsonObject object;
    object.insert(QStringLiteral("image_x"), point.image_x);
    object.insert(QStringLiteral("image_y"), point.image_y);
    object.insert(QStringLiteral("machine_x"), point.machine_x);
    object.insert(QStringLiteral("machine_y"), point.machine_y);
    return object;
}

CoordinateCalibrationPoint PointFromJson(const QJsonObject& object)
{
    CoordinateCalibrationPoint point;
    point.image_x = object.value(QStringLiteral("image_x")).toDouble();
    point.image_y = object.value(QStringLiteral("image_y")).toDouble();
    point.machine_x = object.value(QStringLiteral("machine_x")).toDouble();
    point.machine_y = object.value(QStringLiteral("machine_y")).toDouble();
    return point;
}

bool EnsureProfilesDirectory(QString* error_message)
{
    QDir dir(CalibrationProfilesDirectory());
    if (dir.exists()) {
        return true;
    }
    if (dir.mkpath(QStringLiteral("."))) {
        return true;
    }
    if (error_message != nullptr) {
        *error_message = QStringLiteral("Unable to create calibration profile directory: %1").arg(dir.absolutePath());
    }
    return false;
}

QString ExistingCalibrationProfilePath(const QString& name)
{
    const QString safe_name = SanitizeCalibrationProfileName(name);
    const QString current_path = QDir(CalibrationProfilesDirectory()).filePath(safe_name + QStringLiteral(".json"));
    if (QFile::exists(current_path)) {
        return current_path;
    }

    return current_path;
}

} // namespace

QString CalibrationProfilesDirectory()
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("calibration_profiles"));
}

QString SanitizeCalibrationProfileName(const QString& name)
{
    QString result = name.trimmed();
    if (result.isEmpty()) {
        result = DefaultCalibrationProfileName();
    }

    const QString invalid = QStringLiteral("\\/:*?\"<>|");
    for (const QChar ch : invalid) {
        result.replace(ch, QChar('_'));
    }
    result.replace(QChar('\n'), QChar('_'));
    result.replace(QChar('\r'), QChar('_'));
    return result.left(80).trimmed();
}

QString DefaultCalibrationProfileName()
{
    return QStringLiteral("默认方案");
}

QString CalibrationProfilePath(const QString& name)
{
    const QString safe_name = SanitizeCalibrationProfileName(name);
    return QDir(CalibrationProfilesDirectory()).filePath(safe_name + QStringLiteral(".json"));
}

QStringList ListCalibrationProfileNames()
{
    QStringList names;
    QDir dir(CalibrationProfilesDirectory());
    if (!dir.exists()) {
        return names;
    }

    const QFileInfoList files = dir.entryInfoList({ QStringLiteral("*.json") }, QDir::Files, QDir::Name);
    for (const QFileInfo& file : files) {
        names.append(file.completeBaseName());
    }

    names.removeDuplicates();
    names.sort(Qt::CaseInsensitive);
    return names;
}

bool LoadCalibrationProfile(const QString& name,
                            CoordinateTransformConfig* config,
                            QString* error_message)
{
    if (config == nullptr) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("Calibration profile output is null.");
        }
        return false;
    }

    QFile file(ExistingCalibrationProfilePath(name));
    if (!file.open(QIODevice::ReadOnly)) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("Unable to open calibration profile: %1").arg(file.fileName());
        }
        return false;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("Calibration profile format is invalid: %1").arg(file.fileName());
        }
        return false;
    }

    const QJsonObject root = document.object();
    CoordinateTransformConfig next;
    next.profile_name = root.value(QStringLiteral("name")).toString(SanitizeCalibrationProfileName(name));
    next.enabled = root.value(QStringLiteral("enabled")).toBool(false);

    const QJsonArray points = root.value(QStringLiteral("points")).toArray();
    for (const QJsonValue& value : points) {
        if (value.isObject()) {
            next.points.push_back(PointFromJson(value.toObject()));
        }
    }
    while (next.points.size() < 9) {
        next.points.push_back({});
    }
    if (next.points.size() > 9) {
        next.points.resize(9);
    }

    *config = next;
    return true;
}

bool SaveCalibrationProfile(const CoordinateTransformConfig& config,
                            QString* saved_name,
                            QString* error_message)
{
    if (!EnsureProfilesDirectory(error_message)) {
        return false;
    }

    const QString name = SanitizeCalibrationProfileName(config.profile_name);
    QJsonObject root;
    root.insert(QStringLiteral("version"), kProfileVersion);
    root.insert(QStringLiteral("name"), name);
    root.insert(QStringLiteral("enabled"), config.enabled);

    QJsonArray points;
    for (int i = 0; i < 9; ++i) {
        const CoordinateCalibrationPoint point =
            i < static_cast<int>(config.points.size()) ? config.points[i] : CoordinateCalibrationPoint{};
        points.append(PointToJson(point));
    }
    root.insert(QStringLiteral("points"), points);

    QFile file(CalibrationProfilePath(name));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("Unable to save calibration profile: %1").arg(file.fileName());
        }
        return false;
    }

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (saved_name != nullptr) {
        *saved_name = name;
    }
    return true;
}

bool DeleteCalibrationProfile(const QString& name, QString* error_message)
{
    QFile file(CalibrationProfilePath(name));
    if (!file.exists()) {
        return true;
    }
    if (file.remove()) {
        return true;
    }
    if (error_message != nullptr) {
        *error_message = QStringLiteral("Unable to delete calibration profile: %1").arg(file.fileName());
    }
    return false;
}
