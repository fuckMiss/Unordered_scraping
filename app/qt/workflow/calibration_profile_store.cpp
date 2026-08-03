#include "calibration_profile_store.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QStringList>

#include <cmath>

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

bool JsonNumberValue(const QJsonObject& object,
                     const QString& key,
                     double* value,
                     QString* error_message)
{
    const QJsonValue json_value = object.value(key);
    if (!json_value.isDouble()) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("Calibration point field is missing or not numeric: %1").arg(key);
        }
        return false;
    }

    const double number = json_value.toDouble();
    if (!std::isfinite(number)) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("Calibration point field is not finite: %1").arg(key);
        }
        return false;
    }

    *value = number;
    return true;
}

bool PointFromJson(const QJsonObject& object,
                   int row,
                   CoordinateCalibrationPoint* point,
                   QString* error_message)
{
    if (point == nullptr) {
        return false;
    }
    CoordinateCalibrationPoint next;
    if (!JsonNumberValue(object, QStringLiteral("image_x"), &next.image_x, error_message) ||
        !JsonNumberValue(object, QStringLiteral("image_y"), &next.image_y, error_message) ||
        !JsonNumberValue(object, QStringLiteral("machine_x"), &next.machine_x, error_message) ||
        !JsonNumberValue(object, QStringLiteral("machine_y"), &next.machine_y, error_message)) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("Point P%1 is invalid. %2").arg(row + 1).arg(*error_message);
        }
        return false;
    }
    *point = next;
    return true;
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

bool ParseJsonProfile(const QByteArray& data,
                      const QString& fallback_name,
                      CoordinateTransformConfig* config,
                      QString* error_message)
{
    QJsonParseError parse_error;
    const QJsonDocument document = QJsonDocument::fromJson(data, &parse_error);
    if (parse_error.error != QJsonParseError::NoError || !document.isObject()) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("Calibration profile JSON is invalid: %1").arg(parse_error.errorString());
        }
        return false;
    }

    const QJsonObject root = document.object();
    const QJsonValue points_value = root.value(QStringLiteral("points"));
    if (!points_value.isArray()) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("Calibration profile must contain a points array.");
        }
        return false;
    }

    const QJsonArray points = points_value.toArray();
    if (points.size() != 9) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("Calibration profile must contain exactly 9 points; got %1.").arg(points.size());
        }
        return false;
    }

    CoordinateTransformConfig next;
    next.profile_name = SanitizeCalibrationProfileName(root.value(QStringLiteral("name")).toString(fallback_name));
    next.enabled = root.value(QStringLiteral("enabled")).toBool(false);
    next.points.reserve(9);
    for (int row = 0; row < points.size(); ++row) {
        if (!points.at(row).isObject()) {
            if (error_message != nullptr) {
                *error_message = QStringLiteral("Calibration point P%1 must be an object.").arg(row + 1);
            }
            return false;
        }
        CoordinateCalibrationPoint point;
        if (!PointFromJson(points.at(row).toObject(), row, &point, error_message)) {
            return false;
        }
        next.points.push_back(point);
    }

    *config = next;
    return true;
}

QString NormalizeColumnName(QString value)
{
    value = value.trimmed().toLower();
    value.remove(QChar(0xfeff));
    value.remove(QChar('_'));
    value.remove(QChar('-'));
    value.remove(QChar(' '));
    value.remove(QChar(0x3000));
    return value;
}

int ColumnKind(const QString& value)
{
    const QString column = NormalizeColumnName(value);
    if (column == QStringLiteral("imagex") ||
        column == QStringLiteral("imgx") ||
        column == QStringLiteral("pixelx") ||
        column == QStringLiteral("图像x") ||
        column == QStringLiteral("像素x")) {
        return 0;
    }
    if (column == QStringLiteral("imagey") ||
        column == QStringLiteral("imgy") ||
        column == QStringLiteral("pixely") ||
        column == QStringLiteral("图像y") ||
        column == QStringLiteral("像素y")) {
        return 1;
    }
    if (column == QStringLiteral("machinex") ||
        column == QStringLiteral("robotx") ||
        column == QStringLiteral("worldx") ||
        column == QStringLiteral("机械x") ||
        column == QStringLiteral("机器x")) {
        return 2;
    }
    if (column == QStringLiteral("machiney") ||
        column == QStringLiteral("roboty") ||
        column == QStringLiteral("worldy") ||
        column == QStringLiteral("机械y") ||
        column == QStringLiteral("机器y")) {
        return 3;
    }
    return -1;
}

QStringList SplitTxtColumns(const QString& line)
{
    return line.split(QRegularExpression(QStringLiteral("[,;\\t ]+")), Qt::SkipEmptyParts);
}

bool ParseFiniteDouble(const QString& text, double* value)
{
    bool ok = false;
    const double number = text.trimmed().toDouble(&ok);
    if (!ok || !std::isfinite(number)) {
        return false;
    }
    *value = number;
    return true;
}

bool ParseHeaderTableTxt(const QStringList& lines, CoordinateTransformConfig* config)
{
    for (int header_index = 0; header_index < lines.size(); ++header_index) {
        const QStringList headers = SplitTxtColumns(lines.at(header_index));
        int column_for_kind[4] = { -1, -1, -1, -1 };
        for (int column = 0; column < headers.size(); ++column) {
            const int kind = ColumnKind(headers.at(column));
            if (kind >= 0 && column_for_kind[kind] < 0) {
                column_for_kind[kind] = column;
            }
        }
        if (column_for_kind[0] < 0 || column_for_kind[1] < 0 ||
            column_for_kind[2] < 0 || column_for_kind[3] < 0) {
            continue;
        }

        CoordinateTransformConfig next;
        next.enabled = true;
        for (int line_index = header_index + 1; line_index < lines.size(); ++line_index) {
            const QStringList columns = SplitTxtColumns(lines.at(line_index));
            int max_column = 0;
            for (const int column : column_for_kind) {
                max_column = qMax(max_column, column);
            }
            if (columns.size() <= max_column) {
                continue;
            }

            CoordinateCalibrationPoint point;
            if (!ParseFiniteDouble(columns.at(column_for_kind[0]), &point.image_x) ||
                !ParseFiniteDouble(columns.at(column_for_kind[1]), &point.image_y) ||
                !ParseFiniteDouble(columns.at(column_for_kind[2]), &point.machine_x) ||
                !ParseFiniteDouble(columns.at(column_for_kind[3]), &point.machine_y)) {
                continue;
            }
            next.points.push_back(point);
        }
        if (next.points.size() == 9) {
            *config = next;
            return true;
        }
    }
    return false;
}

bool IsZeroPointRow(const CoordinateCalibrationPoint& point, double angle)
{
    return std::fabs(point.image_x) < 1e-9 &&
           std::fabs(point.image_y) < 1e-9 &&
           std::fabs(point.machine_x) < 1e-9 &&
           std::fabs(point.machine_y) < 1e-9 &&
           std::fabs(angle) < 1e-9;
}

bool ParseVisionMasterFiveColumnTxt(const QStringList& lines, CoordinateTransformConfig* config)
{
    CoordinateTransformConfig next;
    next.enabled = true;
    bool saw_trailing_zero = false;

    for (const QString& line : lines) {
        const QStringList columns = SplitTxtColumns(line);
        if (columns.size() != 5) {
            return false;
        }

        CoordinateCalibrationPoint point;
        double angle = 0.0;
        if (!ParseFiniteDouble(columns.at(0), &point.image_x) ||
            !ParseFiniteDouble(columns.at(1), &point.image_y) ||
            !ParseFiniteDouble(columns.at(2), &point.machine_x) ||
            !ParseFiniteDouble(columns.at(3), &point.machine_y) ||
            !ParseFiniteDouble(columns.at(4), &angle)) {
            return false;
        }

        // VisionMaster exports image X/Y, machine X/Y, and angle. The homography uses only the two XY pairs.
        if (IsZeroPointRow(point, angle)) {
            saw_trailing_zero = true;
            continue;
        }
        if (saw_trailing_zero) {
            return false;
        }
        if (next.points.size() >= 9) {
            return false;
        }
        next.points.push_back(point);
    }

    if (next.points.size() != 9) {
        return false;
    }
    *config = next;
    return true;
}

bool ExtractLabeledNumber(const QString& line,
                          const QStringList& labels,
                          double* value)
{
    static const QString number_pattern =
        QStringLiteral("([-+]?(?:\\d+(?:\\.\\d*)?|\\.\\d+)(?:[eE][-+]?\\d+)?)");
    for (const QString& label : labels) {
        const QRegularExpression expression(
            QRegularExpression::escape(label) + QStringLiteral("\\s*[:=：,，\\s]+") + number_pattern,
            QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch match = expression.match(line);
        if (match.hasMatch() && ParseFiniteDouble(match.captured(1), value)) {
            return true;
        }
    }
    return false;
}

bool ParseLabeledLinesTxt(const QStringList& lines, CoordinateTransformConfig* config)
{
    const QStringList image_x_labels = {
        QStringLiteral("image_x"), QStringLiteral("image x"), QStringLiteral("imagex"),
        QStringLiteral("img_x"), QStringLiteral("图像X"), QStringLiteral("像素X")
    };
    const QStringList image_y_labels = {
        QStringLiteral("image_y"), QStringLiteral("image y"), QStringLiteral("imagey"),
        QStringLiteral("img_y"), QStringLiteral("图像Y"), QStringLiteral("像素Y")
    };
    const QStringList machine_x_labels = {
        QStringLiteral("machine_x"), QStringLiteral("machine x"), QStringLiteral("machinex"),
        QStringLiteral("robot_x"), QStringLiteral("机械X"), QStringLiteral("机器X")
    };
    const QStringList machine_y_labels = {
        QStringLiteral("machine_y"), QStringLiteral("machine y"), QStringLiteral("machiney"),
        QStringLiteral("robot_y"), QStringLiteral("机械Y"), QStringLiteral("机器Y")
    };

    CoordinateTransformConfig next;
    next.enabled = true;
    for (const QString& line : lines) {
        CoordinateCalibrationPoint point;
        if (ExtractLabeledNumber(line, image_x_labels, &point.image_x) &&
            ExtractLabeledNumber(line, image_y_labels, &point.image_y) &&
            ExtractLabeledNumber(line, machine_x_labels, &point.machine_x) &&
            ExtractLabeledNumber(line, machine_y_labels, &point.machine_y)) {
            next.points.push_back(point);
        }
    }
    if (next.points.size() != 9) {
        return false;
    }
    *config = next;
    return true;
}

bool ParseTxtProfile(const QByteArray& data,
                     CoordinateTransformConfig* config,
                     QString* error_message)
{
    const QString text = QString::fromUtf8(data);
    QStringList lines = text.split(QRegularExpression(QStringLiteral("[\\r\\n]+")), Qt::SkipEmptyParts);
    for (QString& line : lines) {
        line = line.trimmed();
    }
    lines.removeAll(QString());

    CoordinateTransformConfig next;
    if (!ParseHeaderTableTxt(lines, &next) &&
        !ParseLabeledLinesTxt(lines, &next) &&
        !ParseVisionMasterFiveColumnTxt(lines, &next)) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("TXT import requires explicit image_x/image_y/machine_x/machine_y columns or VisionMaster 5-column rows: image_x image_y machine_x machine_y angle.");
        }
        return false;
    }

    *config = next;
    return true;
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
    return LoadCalibrationProfileFromFile(ExistingCalibrationProfilePath(name), config, error_message);
}

bool LoadCalibrationProfileFromFile(const QString& path,
                                    CoordinateTransformConfig* config,
                                    QString* error_message)
{
    if (config == nullptr) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("Calibration profile output is null.");
        }
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("Unable to open calibration profile: %1").arg(file.fileName());
        }
        return false;
    }

    const QByteArray data = file.readAll();
    const QString fallback_name = QFileInfo(file).completeBaseName();
    CoordinateTransformConfig next;
    const QString suffix = QFileInfo(file).suffix().toLower();
    const bool loaded = suffix == QStringLiteral("txt")
                            ? ParseTxtProfile(data, &next, error_message)
                            : ParseJsonProfile(data, fallback_name, &next, error_message);
    if (!loaded) {
        return false;
    }

    next.profile_name = SanitizeCalibrationProfileName(next.profile_name.isEmpty() ? fallback_name : next.profile_name);
    if (next.points.size() != 9) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("Calibration profile must contain exactly 9 points; got %1.")
                                 .arg(static_cast<int>(next.points.size()));
        }
        return false;
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
