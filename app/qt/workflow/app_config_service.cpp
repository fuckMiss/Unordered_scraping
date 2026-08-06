#include "app_config_service.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QIODevice>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>

#include <cmath>
#include <iostream>

namespace {

bool IsFinite(double value)
{
    return std::isfinite(value);
}

QJsonObject ObjectValue(const QJsonObject& object, const char* key)
{
    const QJsonValue value = object.value(QLatin1String(key));
    return value.isObject() ? value.toObject() : QJsonObject();
}

QString StringValue(const QJsonObject& object, const char* key, const QString& fallback)
{
    const QJsonValue value = object.value(QLatin1String(key));
    if (!value.isString()) {
        return fallback;
    }
    const QString text = value.toString().trimmed();
    return text.isEmpty() ? fallback : text;
}

bool BoolValue(const QJsonObject& object, const char* key, bool fallback)
{
    const QJsonValue value = object.value(QLatin1String(key));
    return value.isBool() ? value.toBool() : fallback;
}

double DoubleValue(const QJsonObject& object, const char* key, double fallback)
{
    const QJsonValue value = object.value(QLatin1String(key));
    if (!value.isDouble() || !IsFinite(value.toDouble())) {
        return fallback;
    }
    return value.toDouble();
}

int IntValue(const QJsonObject& object, const char* key, int fallback, int minimum, int maximum)
{
    const QJsonValue value = object.value(QLatin1String(key));
    if (!value.isDouble()) {
        return fallback;
    }
    const double raw = value.toDouble();
    if (!IsFinite(raw) || raw < minimum || raw > maximum) {
        return fallback;
    }
    return static_cast<int>(raw);
}

LimitRange RangeValue(const QJsonObject& object, const char* key, const LimitRange& fallback)
{
    const QJsonObject range = ObjectValue(object, key);
    LimitRange result = fallback;
    result.lower = DoubleValue(range, "lower", result.lower);
    result.upper = DoubleValue(range, "upper", result.upper);
    return result;
}

AngleRangeMode ParseAngleRangeMode(const QString& value, AngleRangeMode fallback)
{
    const QString text = value.trimmed().toLower();
    if (text == QStringLiteral("signed_180")) {
        return AngleRangeMode::Signed180;
    }
    if (text == QStringLiteral("zero_to_360")) {
        return AngleRangeMode::ZeroTo360;
    }
    return fallback;
}

AxisMappingMode ParseAxisMappingMode(const QString& value, AxisMappingMode fallback)
{
    const QString text = value.trimmed().toLower();
    if (text == QStringLiteral("machine_x")) {
        return AxisMappingMode::FrontBackMachineX;
    }
    if (text == QStringLiteral("machine_y")) {
        return AxisMappingMode::FrontBackMachineY;
    }
    return fallback;
}

PlcFloatWordOrder ParseFloatWordOrder(const QString& value, PlcFloatWordOrder fallback)
{
    const QString text = value.trimmed().toLower();
    if (text == QStringLiteral("high_word_first")) {
        return PlcFloatWordOrder::HighWordFirst;
    }
    if (text == QStringLiteral("low_word_first")) {
        return PlcFloatWordOrder::LowWordFirst;
    }
    return fallback;
}

bool IsAppDisplayLocked()
{
    const QString value = QString::fromLocal8Bit(qgetenv("TANKEYE_LOCK_APP_DISPLAY")).trimmed().toLower();
    return value == QStringLiteral("1") ||
           value == QStringLiteral("true") ||
           value == QStringLiteral("yes") ||
           value == QStringLiteral("on");
}

void ApplyEnvironmentOverrides(AppConfig& config)
{
    const QString env_host = QString::fromLocal8Bit(qgetenv("TANKEYE_PLC_HOST")).trimmed();
    if (!env_host.isEmpty()) {
        config.plc.host = env_host;
    }

    bool ok = false;
    const int env_port = QString::fromLocal8Bit(qgetenv("TANKEYE_PLC_PORT")).trimmed().toInt(&ok);
    if (ok && env_port > 0 && env_port <= 65535) {
        config.plc.port = static_cast<quint16>(env_port);
    }

    const QString sim_env = QString::fromLocal8Bit(qgetenv("TANKEYE_PLC_SIM")).trimmed().toLower();
    if (!sim_env.isEmpty()) {
        config.plc.simulation_enabled =
            sim_env == QStringLiteral("1") ||
            sim_env == QStringLiteral("true") ||
            sim_env == QStringLiteral("yes") ||
            sim_env == QStringLiteral("on");
    }
}

void ApplyJson(AppConfig& config, const QJsonObject& root)
{
    config.version = IntValue(root, "version", config.version, 1, 999);

    if (!IsAppDisplayLocked()) {
        const QJsonObject app = ObjectValue(root, "app");
        config.app.name = StringValue(app, "name", config.app.name);
        config.app.version = StringValue(app, "version", config.app.version);
        config.app.title = StringValue(app, "title", config.app.title);
    }

    const QJsonObject plc = ObjectValue(root, "plc");
    config.plc.host = StringValue(plc, "host", config.plc.host);
    config.plc.port = static_cast<quint16>(IntValue(plc, "port", config.plc.port, 1, 65535));
    config.plc.unit_id = IntValue(plc, "unit_id", config.plc.unit_id, 1, 247);
    config.plc.connect_timeout_ms = IntValue(plc, "connect_timeout_ms", config.plc.connect_timeout_ms, 100, 60000);
    config.plc.response_timeout_ms = IntValue(plc, "response_timeout_ms", config.plc.response_timeout_ms, 100, 60000);
    config.plc.float_word_order =
        ParseFloatWordOrder(StringValue(plc, "float_word_order", QString()), config.plc.float_word_order);

    const QJsonObject registers = ObjectValue(plc, "registers");
    config.plc.registers.photo_trigger =
        IntValue(registers, "photo_trigger", config.plc.registers.photo_trigger, 0, 65535);
    config.plc.registers.front_back =
        IntValue(registers, "front_back", config.plc.registers.front_back, 0, 65535);
    config.plc.registers.left_right =
        IntValue(registers, "left_right", config.plc.registers.left_right, 0, 65535);
    config.plc.registers.angle =
        IntValue(registers, "angle", config.plc.registers.angle, 0, 65535);
    config.plc.registers.pick_status =
        IntValue(registers, "pick_status", config.plc.registers.pick_status, 0, 65535);
    config.plc.registers.head_type =
        IntValue(registers, "head_type", config.plc.registers.head_type, 0, 65535);

    const QJsonObject limits = ObjectValue(root, "machine_limits");
    config.machine_limits.enabled = BoolValue(limits, "enabled", config.machine_limits.enabled);
    config.machine_limits.roi_margin = DoubleValue(limits, "roi_margin", config.machine_limits.roi_margin);
    if (config.machine_limits.roi_margin < 0.0) {
        config.machine_limits.roi_margin = 5.0;
    }
    config.machine_limits.x = RangeValue(limits, "front_back", config.machine_limits.x);
    config.machine_limits.y = RangeValue(limits, "left_right", config.machine_limits.y);
    config.machine_limits.angle = RangeValue(limits, "angle", config.machine_limits.angle);

    const QJsonObject axis = ObjectValue(root, "axis_mapping");
    config.axis_mapping_mode =
        ParseAxisMappingMode(StringValue(axis, "front_back_axis", QString()), config.axis_mapping_mode);
    config.axis_compensation.front_back_offset =
        DoubleValue(axis, "front_back_offset", config.axis_compensation.front_back_offset);
    config.axis_compensation.left_right_offset =
        DoubleValue(axis, "left_right_offset", config.axis_compensation.left_right_offset);

    const QJsonObject angle = ObjectValue(root, "angle_calibration");
    config.angle_calibration.offset_deg =
        DoubleValue(angle, "offset_deg", config.angle_calibration.offset_deg);
    const QString direction = StringValue(angle, "direction", QStringLiteral("forward")).trimmed().toLower();
    if (direction == QStringLiteral("reverse") || direction == QStringLiteral("forward")) {
        config.angle_calibration.reverse_direction = direction == QStringLiteral("reverse");
    }
    config.angle_calibration.range_mode =
        ParseAngleRangeMode(StringValue(angle, "range_mode", QString()), config.angle_calibration.range_mode);

    const QJsonObject camera = ObjectValue(root, "camera");
    config.camera.ip = StringValue(camera, "ip", config.camera.ip);
    config.camera.exposure_us = DoubleValue(camera, "exposure_us", config.camera.exposure_us);
    if (config.camera.exposure_us < 0.0) {
        config.camera.exposure_us = 0.0;
    }
}

} // namespace

QString AppConfigService::DefaultConfigPath()
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("config/tankeye.json"));
}

AppConfig AppConfigService::Load()
{
    const QString override_path = QString::fromLocal8Bit(qgetenv("TANKEYE_CONFIG_PATH")).trimmed();
    return LoadFromPath(override_path.isEmpty() ? DefaultConfigPath() : override_path);
}

AppConfig AppConfigService::LoadFromPath(const QString& path)
{
    AppConfig config;

    QFile file(path);
    if (!file.exists()) {
        std::cout << "[Config] file not found, using built-in defaults: "
                  << path.toStdString() << std::endl;
        ApplyEnvironmentOverrides(config);
        return config;
    }
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        std::cout << "[Config] failed to open, using built-in defaults: "
                  << path.toStdString() << std::endl;
        ApplyEnvironmentOverrides(config);
        return config;
    }

    QJsonParseError parse_error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parse_error);
    if (parse_error.error != QJsonParseError::NoError || !document.isObject()) {
        std::cout << "[Config] invalid JSON, using built-in defaults: "
                  << path.toStdString()
                  << " error=" << parse_error.errorString().toStdString()
                  << std::endl;
        ApplyEnvironmentOverrides(config);
        return config;
    }

    ApplyJson(config, document.object());
    ApplyEnvironmentOverrides(config);
    std::cout << "[Config] loaded: " << path.toStdString() << std::endl;
    return config;
}
