#include "project_profile_store.h"

#include <QtGui/private/qzipreader_p.h>
#include <QtGui/private/qzipwriter_p.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSettings>
#include <QTemporaryDir>

#include <cmath>

namespace {

constexpr int kProjectProfileVersion = 1;

std::unique_ptr<QSettings> CreateSettings()
{
    const QString override_path = QString::fromLocal8Bit(qgetenv("TANKEYE_SETTINGS_INI_PATH")).trimmed();
    if (!override_path.isEmpty()) {
        return std::make_unique<QSettings>(override_path, QSettings::IniFormat);
    }
    return std::make_unique<QSettings>(QStringLiteral("TankEye"), QStringLiteral("TankEye-Iris"));
}

QString AppDir()
{
    return QCoreApplication::applicationDirPath();
}

QString SourceOrPackageRoot()
{
    QDir dir(AppDir());
    if (QFileInfo(dir.filePath(QStringLiteral("models/DG_8_weights/best_obb.xml"))).exists()) {
        return dir.absolutePath();
    }
    QDir source_candidate = dir;
    if (source_candidate.cdUp() && source_candidate.cdUp() &&
        QFileInfo(source_candidate.filePath(QStringLiteral("models/DG_8_weights/best_obb.xml"))).exists()) {
        return source_candidate.absolutePath();
    }
    if (QFileInfo(dir.filePath(QStringLiteral("config/tankeye.json"))).exists()) {
        return dir.absolutePath();
    }
    return AppDir();
}

bool EnsureDir(const QString& path, QString* error_message)
{
    QDir dir(path);
    if (dir.exists() || dir.mkpath(QStringLiteral("."))) {
        return true;
    }
    if (error_message != nullptr) {
        *error_message = QStringLiteral("无法创建目录：%1").arg(dir.absolutePath());
    }
    return false;
}

QString RelativeToRoot(const QString& path)
{
    const QString trimmed = path.trimmed();
    if (trimmed.isEmpty()) {
        return QString();
    }
    QFileInfo info(trimmed);
    if (!info.isAbsolute()) {
        return QDir::fromNativeSeparators(trimmed);
    }
    const QString root = SourceOrPackageRoot();
    const QString relative = QDir(root).relativeFilePath(info.absoluteFilePath());
    if (!relative.startsWith(QStringLiteral(".."))) {
        return QDir::fromNativeSeparators(relative);
    }
    return QDir::fromNativeSeparators(info.absoluteFilePath());
}

QString PathToStore(const QString& path, const QString& profile_name, bool copy_models)
{
    if (!copy_models) {
        return RelativeToRoot(path);
    }
    const QString file_name = QFileInfo(path).fileName();
    return QStringLiteral("models/%1").arg(file_name);
}

QJsonObject LimitRangeToJson(const LimitRange& range)
{
    QJsonObject object;
    object.insert(QStringLiteral("lower"), range.lower);
    object.insert(QStringLiteral("upper"), range.upper);
    return object;
}

LimitRange LimitRangeFromJson(const QJsonObject& object, const LimitRange& fallback)
{
    LimitRange range = fallback;
    range.lower = object.value(QStringLiteral("lower")).toDouble(range.lower);
    range.upper = object.value(QStringLiteral("upper")).toDouble(range.upper);
    return range;
}

QString AngleRangeKey(AngleRangeMode mode)
{
    return mode == AngleRangeMode::Signed180 ? QStringLiteral("signed_180")
                                             : QStringLiteral("zero_to_360");
}

AngleRangeMode ParseAngleRange(const QString& key)
{
    return key.trimmed().toLower() == QStringLiteral("signed_180")
        ? AngleRangeMode::Signed180
        : AngleRangeMode::ZeroTo360;
}

QString AxisMappingKey(AxisMappingMode mode)
{
    return mode == AxisMappingMode::FrontBackMachineX ? QStringLiteral("machine_x")
                                                      : QStringLiteral("machine_y");
}

AxisMappingMode ParseAxisMapping(const QString& key)
{
    return key.trimmed().toLower() == QStringLiteral("machine_x")
        ? AxisMappingMode::FrontBackMachineX
        : AxisMappingMode::FrontBackMachineY;
}

QJsonArray CoordinatePointsToJson(const CoordinateTransformConfig& config)
{
    QJsonArray points;
    for (const CoordinateCalibrationPoint& point : config.points) {
        QJsonObject object;
        object.insert(QStringLiteral("image_x"), point.image_x);
        object.insert(QStringLiteral("image_y"), point.image_y);
        object.insert(QStringLiteral("machine_x"), point.machine_x);
        object.insert(QStringLiteral("machine_y"), point.machine_y);
        points.append(object);
    }
    return points;
}

std::vector<CoordinateCalibrationPoint> CoordinatePointsFromJson(const QJsonArray& array)
{
    std::vector<CoordinateCalibrationPoint> points;
    points.reserve(9);
    for (const QJsonValue& value : array) {
        const QJsonObject object = value.toObject();
        points.push_back({
            object.value(QStringLiteral("image_x")).toDouble(),
            object.value(QStringLiteral("image_y")).toDouble(),
            object.value(QStringLiteral("machine_x")).toDouble(),
            object.value(QStringLiteral("machine_y")).toDouble(),
        });
    }
    if (points.size() < 9) {
        points.resize(9);
    }
    if (points.size() > 9) {
        points.resize(9);
    }
    return points;
}

QJsonObject ProfileToJson(ProjectProfileSettings profile, bool copy_models)
{
    profile.name = SanitizeProjectProfileName(profile.name);
    QJsonObject root;
    root.insert(QStringLiteral("version"), kProjectProfileVersion);
    root.insert(QStringLiteral("name"), profile.name);
    QJsonObject models;
    models.insert(QStringLiteral("obb"), PathToStore(profile.obb_model_path, profile.name, copy_models));
    models.insert(QStringLiteral("seg"), PathToStore(profile.seg_model_path, profile.name, copy_models));
    root.insert(QStringLiteral("models"), models);

    QJsonObject thresholds;
    thresholds.insert(QStringLiteral("obb_conf_threshold"), profile.model_thresholds.obb_conf_threshold);
    thresholds.insert(QStringLiteral("obb_nms_threshold"), profile.model_thresholds.obb_nms_threshold);
    thresholds.insert(QStringLiteral("seg_conf_threshold"), profile.model_thresholds.seg_conf_threshold);
    thresholds.insert(QStringLiteral("seg_nms_threshold"), profile.model_thresholds.seg_nms_threshold);
    root.insert(QStringLiteral("model_thresholds"), thresholds);

    QJsonObject camera;
    camera.insert(QStringLiteral("exposure_us"), profile.camera_exposure_us);
    root.insert(QStringLiteral("camera"), camera);

    QJsonObject angle;
    angle.insert(QStringLiteral("offset_deg"), profile.angle_calibration.offset_deg);
    angle.insert(QStringLiteral("direction"), profile.angle_calibration.reverse_direction ? QStringLiteral("reverse") : QStringLiteral("forward"));
    angle.insert(QStringLiteral("range_mode"), AngleRangeKey(profile.angle_calibration.range_mode));
    root.insert(QStringLiteral("angle_calibration"), angle);

    QJsonObject postprocess;
    postprocess.insert(QStringLiteral("center_ray_offset_px"), profile.obb_postprocess.center_ray_offset_px);
    postprocess.insert(QStringLiteral("show_head_type_adjusted_geometry"),
                       profile.obb_postprocess.show_head_type_adjusted_geometry);
    postprocess.insert(QStringLiteral("show_plc_center_debug"), profile.obb_postprocess.show_plc_center_debug);
    postprocess.insert(QStringLiteral("show_head_ray_debug"), profile.obb_postprocess.show_head_ray_debug);
    postprocess.insert(QStringLiteral("debug_logging_enabled"), profile.obb_postprocess.debug_logging_enabled);
    root.insert(QStringLiteral("obb_postprocess"), postprocess);

    QJsonObject overlay;
    overlay.insert(QStringLiteral("show_grab_limit_overlay"), profile.ui_overlay.show_grab_limit_overlay);
    overlay.insert(QStringLiteral("mechanical_gripper_length"), profile.ui_overlay.mechanical_gripper_length);
    overlay.insert(QStringLiteral("mechanical_gripper_width"), profile.ui_overlay.mechanical_gripper_width);
    root.insert(QStringLiteral("ui_overlay"), overlay);

    QJsonArray head_types;
    for (int head_type = 1; head_type < static_cast<int>(profile.head_type_compensation.types.size()); ++head_type) {
        QJsonObject object;
        object.insert(QStringLiteral("code"), head_type);
        object.insert(QStringLiteral("angle_offset_deg"), profile.head_type_compensation.types[head_type].angle_offset_deg);
        object.insert(QStringLiteral("ac_ray_offset_mm"), profile.head_type_compensation.types[head_type].ac_ray_offset_mm);
        head_types.append(object);
    }
    root.insert(QStringLiteral("head_type_compensation"), head_types);

    QJsonObject axis;
    axis.insert(QStringLiteral("front_back_axis"), AxisMappingKey(profile.axis_mapping_mode));
    axis.insert(QStringLiteral("front_back_offset"), profile.axis_compensation.front_back_offset);
    axis.insert(QStringLiteral("left_right_offset"), profile.axis_compensation.left_right_offset);
    root.insert(QStringLiteral("axis_mapping"), axis);

    QJsonObject limits;
    limits.insert(QStringLiteral("enabled"), profile.grab_limits.enabled);
    limits.insert(QStringLiteral("roi_margin"), profile.grab_limits.roi_margin);
    limits.insert(QStringLiteral("x"), LimitRangeToJson(profile.grab_limits.x));
    limits.insert(QStringLiteral("y"), LimitRangeToJson(profile.grab_limits.y));
    limits.insert(QStringLiteral("angle"), LimitRangeToJson(profile.grab_limits.angle));
    root.insert(QStringLiteral("grab_limits"), limits);

    QJsonObject coordinate;
    coordinate.insert(QStringLiteral("enabled"), profile.coordinate_transform.enabled);
    coordinate.insert(QStringLiteral("profile_name"), profile.coordinate_transform.profile_name);
    coordinate.insert(QStringLiteral("points"), CoordinatePointsToJson(profile.coordinate_transform));
    root.insert(QStringLiteral("coordinate_transform"), coordinate);
    return root;
}

bool ProfileFromJson(const QByteArray& data,
                     const QString& fallback_name,
                     ProjectProfileSettings* profile,
                     QString* error_message)
{
    QJsonParseError parse_error;
    const QJsonDocument document = QJsonDocument::fromJson(data, &parse_error);
    if (parse_error.error != QJsonParseError::NoError || !document.isObject()) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("工程方案 JSON 无效：%1").arg(parse_error.errorString());
        }
        return false;
    }

    const QJsonObject root = document.object();
    ProjectProfileSettings next;
    next.name = SanitizeProjectProfileName(root.value(QStringLiteral("name")).toString(fallback_name));
    if (next.name.isEmpty()) {
        next.name = DefaultProjectProfileName();
    }

    const QJsonObject models = root.value(QStringLiteral("models")).toObject();
    next.obb_model_path = models.value(QStringLiteral("obb")).toString();
    next.seg_model_path = models.value(QStringLiteral("seg")).toString();
    const QJsonObject thresholds = root.value(QStringLiteral("model_thresholds")).toObject();
    next.model_thresholds.obb_conf_threshold = thresholds.value(QStringLiteral("obb_conf_threshold")).toDouble(0.5);
    next.model_thresholds.obb_nms_threshold = thresholds.value(QStringLiteral("obb_nms_threshold")).toDouble(0.3);
    next.model_thresholds.seg_conf_threshold = thresholds.value(QStringLiteral("seg_conf_threshold")).toDouble(0.3);
    next.model_thresholds.seg_nms_threshold = thresholds.value(QStringLiteral("seg_nms_threshold")).toDouble(0.4);

    next.camera_exposure_us = root.value(QStringLiteral("camera")).toObject().value(QStringLiteral("exposure_us")).toDouble(0.0);

    const QJsonObject angle = root.value(QStringLiteral("angle_calibration")).toObject();
    const double legacy_offset = angle.value(QStringLiteral("offset_deg")).toDouble(0.0);
    next.angle_calibration.reverse_direction = angle.value(QStringLiteral("direction")).toString() == QStringLiteral("reverse");
    next.angle_calibration.range_mode = ParseAngleRange(angle.value(QStringLiteral("range_mode")).toString());
    next.angle_calibration.offset_deg = legacy_offset;

    const QJsonObject postprocess = root.value(QStringLiteral("obb_postprocess")).toObject();
    next.obb_postprocess.center_ray_offset_px = postprocess.value(QStringLiteral("center_ray_offset_px")).toDouble(10.0);
    next.obb_postprocess.show_head_type_adjusted_geometry =
        postprocess.value(QStringLiteral("show_head_type_adjusted_geometry")).toBool(true);
    next.obb_postprocess.show_plc_center_debug = postprocess.value(QStringLiteral("show_plc_center_debug")).toBool(false);
    next.obb_postprocess.show_head_ray_debug = postprocess.value(QStringLiteral("show_head_ray_debug")).toBool(true);
    next.obb_postprocess.debug_logging_enabled = postprocess.value(QStringLiteral("debug_logging_enabled")).toBool(false);

    const QJsonObject overlay = root.value(QStringLiteral("ui_overlay")).toObject();
    next.ui_overlay.show_grab_limit_overlay = overlay.value(QStringLiteral("show_grab_limit_overlay")).toBool(true);
    next.ui_overlay.mechanical_gripper_length = overlay.value(QStringLiteral("mechanical_gripper_length")).toDouble(0.0);
    next.ui_overlay.mechanical_gripper_width = overlay.value(QStringLiteral("mechanical_gripper_width")).toDouble(0.0);

    for (const QJsonValue& value : root.value(QStringLiteral("head_type_compensation")).toArray()) {
        const QJsonObject object = value.toObject();
        const int code = object.value(QStringLiteral("code")).toInt(0);
        if (code > 0 && code < static_cast<int>(next.head_type_compensation.types.size())) {
            next.head_type_compensation.types[code].angle_offset_deg =
                static_cast<float>(object.value(QStringLiteral("angle_offset_deg")).toDouble(0.0));
            next.head_type_compensation.types[code].ac_ray_offset_mm =
                static_cast<float>(object.value(QStringLiteral("ac_ray_offset_mm")).toDouble(0.0));
        }
    }

    const QJsonObject axis = root.value(QStringLiteral("axis_mapping")).toObject();
    next.axis_mapping_mode = ParseAxisMapping(axis.value(QStringLiteral("front_back_axis")).toString());
    next.axis_compensation.front_back_offset = axis.value(QStringLiteral("front_back_offset")).toDouble(0.0);
    next.axis_compensation.left_right_offset = axis.value(QStringLiteral("left_right_offset")).toDouble(0.0);

    const QJsonObject limits = root.value(QStringLiteral("grab_limits")).toObject();
    next.grab_limits.enabled = limits.value(QStringLiteral("enabled")).toBool(true);
    next.grab_limits.roi_margin = limits.value(QStringLiteral("roi_margin")).toDouble(5.0);
    next.grab_limits.x = LimitRangeFromJson(limits.value(QStringLiteral("x")).toObject(), next.grab_limits.x);
    next.grab_limits.y = LimitRangeFromJson(limits.value(QStringLiteral("y")).toObject(), next.grab_limits.y);
    next.grab_limits.angle = LimitRangeFromJson(limits.value(QStringLiteral("angle")).toObject(), next.grab_limits.angle);

    const QJsonObject coordinate = root.value(QStringLiteral("coordinate_transform")).toObject();
    next.coordinate_transform.enabled = coordinate.value(QStringLiteral("enabled")).toBool(false);
    next.coordinate_transform.profile_name = coordinate.value(QStringLiteral("profile_name")).toString(DefaultProjectProfileName());
    next.coordinate_transform.points = CoordinatePointsFromJson(coordinate.value(QStringLiteral("points")).toArray());

    *profile = next;
    return true;
}

bool WriteJsonFile(const QString& path, const QJsonObject& object, QString* error_message)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("无法写入文件：%1").arg(path);
        }
        return false;
    }
    file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
    return true;
}

bool ReadFile(const QString& path, QByteArray* data, QString* error_message)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("无法读取文件：%1").arg(path);
        }
        return false;
    }
    *data = file.readAll();
    return true;
}

bool CopyModelFile(const QString& source_model_path,
                   const QString& profile_name,
                   const QString& target_file_name,
                   QString* error_message)
{
    const QString source_path = QFileInfo(source_model_path).absoluteFilePath();
    if (!QFileInfo::exists(source_path)) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("模型文件不存在：%1").arg(source_path);
        }
        return false;
    }
    const QString target_dir = QDir(ProjectProfileDirectory(profile_name)).filePath(QStringLiteral("models"));
    if (!EnsureDir(target_dir, error_message)) {
        return false;
    }
    const QString target_path = QDir(target_dir).filePath(target_file_name);
    if (QFileInfo(source_path).absoluteFilePath() == QFileInfo(target_path).absoluteFilePath()) {
        return true;
    }
    QFile::remove(target_path);
    if (!QFile::copy(source_path, target_path)) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("无法复制模型文件：%1").arg(source_path);
        }
        return false;
    }
    return true;
}

bool AddFileToZip(QZipWriter* writer,
                  const QString& zip_path,
                  const QString& file_path,
                  QString* error_message)
{
    QFile file(file_path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("无法读取模型文件：%1").arg(file_path);
        }
        return false;
    }
    writer->addFile(zip_path, file.readAll());
    return true;
}

bool ContainsUnsafeZipPath(const QString& path)
{
    const QString normalized = QDir::cleanPath(path);
    return normalized.startsWith(QStringLiteral("../")) ||
           normalized.contains(QStringLiteral("/../")) ||
           QFileInfo(normalized).isAbsolute();
}

} // namespace

QString ProjectProfilesDirectory()
{
    const QString settings_path = QString::fromLocal8Bit(qgetenv("TANKEYE_SETTINGS_INI_PATH")).trimmed();
    if (!settings_path.isEmpty()) {
        return QDir(QFileInfo(settings_path).absolutePath()).filePath(QStringLiteral("project_profiles"));
    }
    return QDir(SourceOrPackageRoot()).filePath(QStringLiteral("config/project_profiles"));
}

QString DefaultProjectProfileName()
{
    return QStringLiteral("DG_8");
}

ProjectProfileSettings CreateBlankProjectProfile(const QString& name)
{
    ProjectProfileSettings profile;
    profile.name = SanitizeProjectProfileName(name);
    profile.camera_exposure_us = 0.0;
    profile.model_thresholds = ModelThresholdSettings{};
    profile.angle_calibration = AngleCalibrationSettings{};
    profile.obb_postprocess = ObbPostprocessSettings{};
    profile.ui_overlay = UiOverlaySettings{};
    profile.ui_overlay.show_grab_limit_overlay = false;
    profile.head_type_compensation = HeadTypeCompensationSettings{};
    profile.axis_mapping_mode = AxisMappingMode::FrontBackMachineY;
    profile.axis_compensation = AxisCompensationSettings{};
    profile.grab_limits = GrabLimitConfig{};
    profile.grab_limits.enabled = false;
    profile.grab_limits.roi_margin = 5.0;
    profile.coordinate_transform = CoordinateTransformConfig{};
    profile.coordinate_transform.enabled = false;
    profile.coordinate_transform.profile_name = DefaultProjectProfileName();
    profile.coordinate_transform.points.resize(9);
    return profile;
}

QString SanitizeProjectProfileName(const QString& name)
{
    QString safe = name.trimmed();
    safe.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("_"));
    safe.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral("_"));
    return safe.left(64);
}

QString ProjectProfileDirectory(const QString& name)
{
    return QDir(ProjectProfilesDirectory()).filePath(SanitizeProjectProfileName(name));
}

QString ProjectProfilePath(const QString& name)
{
    return QDir(ProjectProfileDirectory(name)).filePath(QStringLiteral("profile.json"));
}

QStringList ListProjectProfileNames()
{
    QDir dir(ProjectProfilesDirectory());
    QStringList names;
    for (const QFileInfo& entry : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        if (QFileInfo(QDir(entry.absoluteFilePath()).filePath(QStringLiteral("profile.json"))).exists()) {
            names.append(entry.fileName());
        }
    }
    return names;
}

QString LoadActiveProjectProfileName()
{
    auto settings = CreateSettings();
    settings->beginGroup(QStringLiteral("project_profile"));
    QString name = settings->value(QStringLiteral("active"), DefaultProjectProfileName()).toString();
    settings->endGroup();
    name = SanitizeProjectProfileName(name);
    return name.isEmpty() ? DefaultProjectProfileName() : name;
}

void SaveActiveProjectProfileName(const QString& name)
{
    auto settings = CreateSettings();
    settings->beginGroup(QStringLiteral("project_profile"));
    settings->setValue(QStringLiteral("active"), SanitizeProjectProfileName(name));
    settings->endGroup();
    settings->sync();
}

bool LoadProjectProfile(const QString& name, ProjectProfileSettings* profile, QString* error_message)
{
    QByteArray data;
    if (!ReadFile(ProjectProfilePath(name), &data, error_message)) {
        return false;
    }
    if (!ProfileFromJson(data, name, profile, error_message)) {
        return false;
    }
    profile->name = SanitizeProjectProfileName(profile->name);
    profile->obb_model_path = ResolveProjectProfileModelPath(*profile, profile->obb_model_path);
    profile->seg_model_path = ResolveProjectProfileModelPath(*profile, profile->seg_model_path);
    return true;
}

bool SaveProjectProfile(const ProjectProfileSettings& profile,
                        QString* saved_name,
                        QString* error_message)
{
    ProjectProfileSettings next = profile;
    next.name = SanitizeProjectProfileName(next.name);
    if (next.name.isEmpty()) {
        next.name = DefaultProjectProfileName();
    }
    if (!EnsureDir(ProjectProfileDirectory(next.name), error_message)) {
        return false;
    }
    const QJsonObject json = ProfileToJson(next, false);
    if (!WriteJsonFile(ProjectProfilePath(next.name), json, error_message)) {
        return false;
    }
    if (saved_name != nullptr) {
        *saved_name = next.name;
    }
    return true;
}

bool DeleteProjectProfile(const QString& name, QString* error_message)
{
    const QString safe_name = SanitizeProjectProfileName(name);
    if (safe_name == DefaultProjectProfileName()) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("默认 DG_8 工程方案不能删除。");
        }
        return false;
    }
    QDir dir(ProjectProfileDirectory(safe_name));
    if (!dir.exists() || dir.removeRecursively()) {
        return true;
    }
    if (error_message != nullptr) {
        *error_message = QStringLiteral("无法删除工程方案：%1").arg(safe_name);
    }
    return false;
}

bool EnsureDefaultProjectProfile(const ProjectProfileSettings& defaults, QString* error_message)
{
    if (QFileInfo::exists(ProjectProfilePath(DefaultProjectProfileName()))) {
        return true;
    }
    ProjectProfileSettings next = defaults;
    next.name = DefaultProjectProfileName();
    next.obb_model_path = QDir(SourceOrPackageRoot()).filePath(QStringLiteral("models/DG_8_weights/best_obb.xml"));
    next.seg_model_path = QDir(SourceOrPackageRoot()).filePath(QStringLiteral("models/DG_8_weights/best_seg.xml"));
    QString saved_name;
    if (!SaveProjectProfile(next, &saved_name, error_message)) {
        return false;
    }
    SaveActiveProjectProfileName(saved_name);
    return true;
}

bool ExportProjectProfile(const QString& name,
                          const QString& destination_path,
                          bool include_models,
                          QString* error_message)
{
    ProjectProfileSettings profile;
    if (!LoadProjectProfile(name, &profile, error_message)) {
        return false;
    }
    const QString lower = destination_path.trimmed().toLower();
    if (lower.endsWith(QStringLiteral(".json"))) {
        return WriteJsonFile(destination_path, ProfileToJson(profile, false), error_message);
    }
    QZipWriter writer(destination_path);
    if (!writer.isWritable()) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("无法创建导出包：%1").arg(destination_path);
        }
        return false;
    }
    writer.setCompressionPolicy(QZipWriter::AutoCompress);
    writer.addFile(QStringLiteral("profile.json"), QJsonDocument(ProfileToJson(profile, include_models)).toJson(QJsonDocument::Indented));
    if (include_models) {
        const QString obb_directory = QFileInfo(profile.obb_model_path).absolutePath();
        const QString seg_directory = QFileInfo(profile.seg_model_path).absolutePath();
        if (!AddFileToZip(&writer, QStringLiteral("models/best_obb.xml"), profile.obb_model_path, error_message) ||
            !AddFileToZip(&writer, QStringLiteral("models/best_obb.bin"), QDir(obb_directory).filePath(QStringLiteral("best_obb.bin")), error_message) ||
            !AddFileToZip(&writer, QStringLiteral("models/best_seg.xml"), profile.seg_model_path, error_message) ||
            !AddFileToZip(&writer, QStringLiteral("models/best_seg.bin"), QDir(seg_directory).filePath(QStringLiteral("best_seg.bin")), error_message)) {
            writer.close();
            QFile::remove(destination_path);
            return false;
        }
    }
    writer.close();
    if (writer.status() != QZipWriter::NoError) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("导出 ZIP 失败。");
        }
        return false;
    }
    return true;
}

bool ImportProjectProfile(const QString& source_path,
                          bool* replaced_existing,
                          QString* imported_name,
                          QString* error_message)
{
    const QString lower = source_path.trimmed().toLower();
    QByteArray profile_data;
    QTemporaryDir temp_dir;
    QString extracted_root;
    if (lower.endsWith(QStringLiteral(".zip"))) {
        QZipReader reader(source_path);
        if (!reader.exists() || !reader.isReadable()) {
            if (error_message != nullptr) {
                *error_message = QStringLiteral("无法读取工程方案 ZIP：%1").arg(source_path);
            }
            return false;
        }
        for (const QZipReader::FileInfo& info : reader.fileInfoList()) {
            if (ContainsUnsafeZipPath(info.filePath)) {
                if (error_message != nullptr) {
                    *error_message = QStringLiteral("工程方案包包含不安全路径：%1").arg(info.filePath);
                }
                return false;
            }
        }
        if (!reader.extractAll(temp_dir.path())) {
            if (error_message != nullptr) {
                *error_message = QStringLiteral("无法解压工程方案 ZIP。");
            }
            return false;
        }
        extracted_root = temp_dir.path();
        if (!ReadFile(QDir(extracted_root).filePath(QStringLiteral("profile.json")), &profile_data, error_message)) {
            return false;
        }
    } else {
        if (!ReadFile(source_path, &profile_data, error_message)) {
            return false;
        }
        extracted_root = QFileInfo(source_path).absolutePath();
    }

    ProjectProfileSettings profile;
    if (!ProfileFromJson(profile_data, QFileInfo(source_path).completeBaseName(), &profile, error_message)) {
        return false;
    }
    profile.name = SanitizeProjectProfileName(profile.name);
    if (profile.name.isEmpty()) {
        profile.name = QFileInfo(source_path).completeBaseName();
    }
    const bool existed = QFileInfo::exists(ProjectProfilePath(profile.name));
    if (replaced_existing != nullptr) {
        *replaced_existing = existed;
    }

    QString saved_name;
    if (!SaveProjectProfile(profile, &saved_name, error_message)) {
        return false;
    }
    const QString imported_model_dir = QDir(extracted_root).filePath(QStringLiteral("models"));
    if (QDir(imported_model_dir).exists()) {
        for (const QString& model_file : { QStringLiteral("best_obb.xml"), QStringLiteral("best_obb.bin"),
                                           QStringLiteral("best_seg.xml"), QStringLiteral("best_seg.bin") }) {
            const QString source_model = QDir(imported_model_dir).filePath(model_file);
            if (QFileInfo::exists(source_model)) {
                if (!CopyModelFile(source_model, saved_name, model_file, error_message)) {
                    return false;
                }
            }
        }
        ProjectProfileSettings reloaded;
        if (LoadProjectProfile(saved_name, &reloaded, error_message)) {
            reloaded.obb_model_path = QStringLiteral("models/best_obb.xml");
            reloaded.seg_model_path = QStringLiteral("models/best_seg.xml");
            SaveProjectProfile(reloaded, &saved_name, error_message);
        }
    }
    if (imported_name != nullptr) {
        *imported_name = saved_name;
    }
    return true;
}

QString ResolveProjectProfileModelPath(const ProjectProfileSettings& profile, const QString& model_path)
{
    const QString trimmed = model_path.trimmed();
    if (trimmed.isEmpty()) {
        return QString();
    }
    QFileInfo info(trimmed);
    if (info.isAbsolute()) {
        return info.absoluteFilePath();
    }
    const QString profile_relative = QDir(ProjectProfileDirectory(profile.name)).filePath(trimmed);
    if (QFileInfo::exists(profile_relative)) {
        return QFileInfo(profile_relative).absoluteFilePath();
    }
    return QFileInfo(QDir(SourceOrPackageRoot()).filePath(trimmed)).absoluteFilePath();
}
