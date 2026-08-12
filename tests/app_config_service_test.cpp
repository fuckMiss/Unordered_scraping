#include "app_config_service.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <cassert>
#include <cmath>
#include <iostream>

namespace {

bool NearlyEqual(double left, double right)
{
    return std::fabs(left - right) < 0.001;
}

void WriteTextFile(const QString& path, const QByteArray& content)
{
    QFile file(path);
    assert(file.open(QIODevice::WriteOnly | QIODevice::Text));
    assert(file.write(content) == content.size());
}

void FullJsonIsParsed()
{
    QTemporaryDir directory;
    assert(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("tankeye.json"));
    WriteTextFile(path, R"json(
{
  "version": 2,
  "app": {
    "name": "FieldEye",
    "version": "9.8.7",
    "title": "Field Test System"
  },
  "plc": {
    "host": "10.0.0.10",
    "port": 1502,
    "unit_id": 5,
    "connect_timeout_ms": 2500,
    "response_timeout_ms": 2600,
    "float_word_order": "high_word_first",
    "registers": {
      "photo_trigger": 2000,
      "front_back": 600,
      "left_right": 602,
      "angle": 604,
      "pick_status": 606,
      "head_type": 608
    }
  },
  "machine_limits": {
    "enabled": true,
    "roi_margin": 4.5,
    "front_back": { "lower": 1.0, "upper": 2.0 },
    "left_right": { "lower": 3.0, "upper": 4.0 },
    "angle": { "lower": 5.0, "upper": 6.0 }
  },
  "axis_mapping": {
    "front_back_axis": "machine_x",
    "front_back_offset": 1.5,
    "left_right_offset": -2.5
  },
  "angle_calibration": {
    "offset_deg": 12.0,
    "direction": "reverse",
    "range_mode": "signed_180"
  },
  "camera": {
    "ip": "192.168.1.10",
    "exposure_us": 1234.0
  }
}
)json");

    const AppConfig config = AppConfigService::LoadFromPath(path);
    assert(config.version == 2);
    assert(config.app.name == QStringLiteral("FieldEye"));
    assert(config.app.version == QStringLiteral("9.8.7"));
    assert(config.app.title == QStringLiteral("Field Test System"));
    assert(config.plc.host == QStringLiteral("10.0.0.10"));
    assert(config.plc.port == 1502);
    assert(config.plc.unit_id == 5);
    assert(config.plc.float_word_order == PlcFloatWordOrder::HighWordFirst);
    assert(config.plc.registers.photo_trigger == 2000);
    assert(config.plc.registers.pick_status == 606);
    assert(NearlyEqual(config.machine_limits.roi_margin, 4.5));
    assert(NearlyEqual(config.machine_limits.x.lower, 1.0));
    assert(config.axis_mapping_mode == AxisMappingMode::FrontBackMachineX);
    assert(NearlyEqual(config.axis_compensation.left_right_offset, -2.5));
    assert(config.angle_calibration.reverse_direction);
    assert(config.angle_calibration.range_mode == AngleRangeMode::Signed180);
    assert(NearlyEqual(config.angle_calibration.offset_deg, 12.0));
    assert(config.camera.ip == QStringLiteral("192.168.1.10"));
    assert(NearlyEqual(config.camera.exposure_us, 1234.0));
}

void MissingAndInvalidValuesFallBack()
{
    const AppConfig missing = AppConfigService::LoadFromPath(QStringLiteral("Z:/missing/tankeye.json"));
    assert(missing.app.name == QStringLiteral("TankEye-Iris"));
    assert(missing.app.version == QStringLiteral("1.4.3"));
    assert(missing.app.title == QStringLiteral("截止阀抓取上料系统"));
    assert(missing.plc.registers.photo_trigger == 1500);
    assert(missing.plc.registers.pick_status == 506);

    QTemporaryDir directory;
    assert(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("invalid.json"));
    WriteTextFile(path, R"json(
{
  "plc": {
    "port": -1,
    "unit_id": 999,
    "float_word_order": "not_valid",
    "registers": { "photo_trigger": -10 }
  },
  "machine_limits": { "roi_margin": -2.0 }
}
)json");

    const AppConfig invalid = AppConfigService::LoadFromPath(path);
    assert(invalid.plc.port == 502);
    assert(invalid.plc.unit_id == 1);
    assert(invalid.plc.float_word_order == PlcFloatWordOrder::LowWordFirst);
    assert(invalid.plc.registers.photo_trigger == 1500);
    assert(NearlyEqual(invalid.machine_limits.roi_margin, 5.0));
}

void AppDisplayCanBeLocked()
{
    QTemporaryDir directory;
    assert(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("tankeye.json"));
    WriteTextFile(path, R"json(
{
  "app": {
    "name": "EditedName",
    "version": "0.0.1",
    "title": "Edited Title"
  }
}
)json");

    qputenv("TANKEYE_LOCK_APP_DISPLAY", "1");
    const AppConfig locked = AppConfigService::LoadFromPath(path);
    qunsetenv("TANKEYE_LOCK_APP_DISPLAY");

    assert(locked.app.name == QStringLiteral("TankEye-Iris"));
    assert(locked.app.version == QStringLiteral("1.4.3"));
    assert(locked.app.title == QStringLiteral("截止阀抓取上料系统"));
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    FullJsonIsParsed();
    MissingAndInvalidValuesFallBack();
    AppDisplayCanBeLocked();
    std::cout << "app_config_service_test passed" << std::endl;
    return 0;
}
