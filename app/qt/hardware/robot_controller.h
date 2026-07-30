#pragma once

#include "app_config_service.h"
#include "plc_result_contract.h"

#include <QByteArray>
#include <QObject>
#include <QString>

#include <cstdint>
#include <mutex>
#include <utility>
#include <vector>
#include <string>

class RobotController : public QObject
{
public:
    explicit RobotController(QObject* parent = nullptr);
    explicit RobotController(const PlcConnectionConfig& config, QObject* parent = nullptr);

    void configurePlc(const PlcConnectionConfig& config);

    bool readPhotoTrigger(bool* triggered, std::string* error_message);
    bool clearPhotoTrigger(std::string* error_message);
    bool writeRejectStatus(std::string* error_message);
    bool writeFrameResult(const FrameInferenceResult& result, std::string* error_message);
    bool writePlcTestValues(std::string* error_message);
    void setAngleCalibration(float offset_deg, bool reverse_direction, AngleRangeMode range_mode);
    void setAxisMapping(AxisMappingMode mode);
    void setAxisCompensation(float front_back_offset, float left_right_offset);
    QString plcHost() const;
    PlcRegisterMap plcRegisterMap() const;
    bool simulationEnabled() const;

private:
    bool writeFloatRegister(int start_d_address, float value, std::string* error_message);
    bool writeFloatRegisters(const std::vector<std::pair<int, float>>& writes,
                             std::string* error_message);
    bool writeWordRegister(int start_d_address, uint16_t value, std::string* error_message);
    bool readFloatRegister(int start_d_address, float* value, std::string* error_message);
    bool readWordRegister(int start_d_address, uint16_t* value, std::string* error_message);
    bool transact(const QByteArray& request,
                  int minimum_response_size,
                  QByteArray* response,
                  std::string* error_message);
    QByteArray buildReadHoldingRegistersRequest(int start_d_address, int register_count);
    QByteArray buildWriteMultipleRegistersRequest(int start_d_address, float value);
    QByteArray buildWriteSingleRegisterRequest(int start_d_address, uint16_t value);
    uint16_t nextTransactionId();
    float decodeFloat(const QByteArray& data, int offset) const;
    PlcOutputConfig plcOutputConfig() const;

    QString plc_host_ = QStringLiteral("192.168.3.205");
    quint16 plc_port_ = 502;
    int connect_timeout_ms_ = 1500;
    int response_timeout_ms_ = 1500;
    int modbus_unit_id_ = 1;
    PlcFloatWordOrder float_word_order_ = PlcFloatWordOrder::LowWordFirst;
    PlcRegisterMap plc_registers_;
    mutable std::recursive_mutex plc_mutex_;
    bool simulation_enabled_ = false;
    bool simulated_photo_trigger_ = false;
    uint16_t transaction_id_ = 0;
    float angle_offset_deg_ = 0.0f;
    bool angle_reverse_direction_ = false;
    AngleRangeMode angle_range_mode_ = AngleRangeMode::ZeroTo360;
    AxisMappingMode axis_mapping_mode_ = AxisMappingMode::FrontBackMachineY;
    float front_back_offset_ = 0.0f;
    float left_right_offset_ = 0.0f;
};

std::vector<std::pair<int, float>> BuildPlcFrameResultRegisterWrites(
    const FrameInferenceResult& result,
    const PlcRegisterMap& registers,
    const PlcOutputConfig& output_config);
std::vector<std::pair<int, float>> BuildPlcRejectStatusRegisterWrite(const PlcRegisterMap& registers);
