#pragma once

#include "engineering_settings_service.h"

#include <QString>
#include <QtGlobal>

struct PlcRegisterMap
{
    int photo_trigger = 1500;
    int front_back = 500;
    int left_right = 502;
    int angle = 504;
    int pick_status = 506;
    int head_type = 508;
};

enum class PlcFloatWordOrder {
    HighWordFirst,
    LowWordFirst
};

struct PlcConnectionConfig
{
    QString host = QStringLiteral("192.168.3.205");
    quint16 port = 502;
    int unit_id = 1;
    int connect_timeout_ms = 1500;
    int response_timeout_ms = 1500;
    PlcFloatWordOrder float_word_order = PlcFloatWordOrder::LowWordFirst;
    bool simulation_enabled = false;
    PlcRegisterMap registers;
};

struct AppDisplayConfig
{
    QString name = QStringLiteral("TankEye-Iris");
    QString version = QStringLiteral("2.1.6");
    QString title = QStringLiteral("截止阀抓取上料系统");
};

struct AppConfig
{
    int version = 1;
    AppDisplayConfig app;
    PlcConnectionConfig plc;
    GrabLimitConfig machine_limits;
    AxisMappingMode axis_mapping_mode = AxisMappingMode::FrontBackMachineY;
    AxisCompensationSettings axis_compensation;
    AngleCalibrationSettings angle_calibration;
    CameraSettings camera;
};

class AppConfigService
{
public:
    static AppConfig Load();
    static AppConfig LoadFromPath(const QString& path);
    static QString DefaultConfigPath();
};
