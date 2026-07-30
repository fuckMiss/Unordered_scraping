#include "robot_controller.h"

#include <QDataStream>
#include <QIODevice>
#include <QTcpSocket>
#include <QtGlobal>

#include <cmath>
#include <cstring>
#include <iostream>
#include <mutex>
#include <utility>

namespace {

constexpr int kReadHoldingRegisters = 0x03;
constexpr int kWriteSingleRegister = 0x06;
constexpr int kWriteMultipleRegisters = 0x10;


uint32_t FloatToBits(float value)
{
    uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value), "float must be 32-bit");
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

float BitsToFloat(uint32_t bits)
{
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

void LogPlcWriteResult(const char* prefix, const PlcWriteResult& result, const PlcRegisterMap& registers)
{
    std::cout << prefix
              << " D" << registers.front_back << "(front_back)=" << result.x
              << ", D" << registers.left_right << "(left_right)=" << result.y
              << ", source=" << (result.has_machine_coords ? "machine" : "image")
              << ", image=(" << result.image_x << "," << result.image_y << ")"
              << ", D" << registers.angle << "(angle)=" << result.angle
              << ", D" << registers.pick_status << "(pick_status)=" << result.pick_status
              << ", D" << registers.head_type << "(head_type)=" << result.head_type
              << std::endl;
}

}  // namespace

RobotController::RobotController(QObject* parent)
    : RobotController(AppConfigService::Load().plc, parent)
{
}

RobotController::RobotController(const PlcConnectionConfig& config, QObject* parent)
    : QObject(parent)
{
    configurePlc(config);
}

std::vector<std::pair<int, float>> BuildPlcRejectStatusRegisterWrite(const PlcRegisterMap& registers)
{
    return {
        { registers.pick_status, 3.0f },
    };
}

std::vector<std::pair<int, float>> BuildPlcFrameResultRegisterWrites(
    const FrameInferenceResult& result,
    const PlcRegisterMap& registers,
    const PlcOutputConfig& output_config)
{
    const PlcWriteResult write_result = BuildPlcWriteResult(result, output_config);
    if (write_result.pick_status >= 3.0f ||
        result.primary_index < 0 ||
        result.primary_index >= static_cast<int>(result.detections.size())) {
        return BuildPlcRejectStatusRegisterWrite(registers);
    }

    return {
        { registers.front_back, write_result.x },
        { registers.left_right, write_result.y },
        { registers.angle, write_result.angle },
        { registers.pick_status, write_result.pick_status },
        { registers.head_type, write_result.head_type },
    };
}

void RobotController::configurePlc(const PlcConnectionConfig& config)
{
    std::lock_guard<std::recursive_mutex> lock(plc_mutex_);
    plc_host_ = config.host.trimmed().isEmpty() ? QStringLiteral("192.168.3.205") : config.host.trimmed();
    plc_port_ = config.port;
    modbus_unit_id_ = config.unit_id;
    connect_timeout_ms_ = config.connect_timeout_ms;
    response_timeout_ms_ = config.response_timeout_ms;
    float_word_order_ = config.float_word_order;
    simulation_enabled_ = config.simulation_enabled;
    plc_registers_ = config.registers;
    if (simulation_enabled_) {
        std::cout << "[PLC_SIM] enabled. PLC reads/writes are simulated in-process." << std::endl;
    }
}

bool RobotController::readPhotoTrigger(bool* triggered, std::string* error_message)
{
    std::lock_guard<std::recursive_mutex> lock(plc_mutex_);

    if (triggered == nullptr) {
        if (error_message != nullptr) {
            *error_message = "triggered pointer is null";
        }
        return false;
    }

    if (simulation_enabled_) {
        *triggered = simulated_photo_trigger_;
        if (*triggered) {
            std::cout << "[PLC_SIM] read D" << plc_registers_.photo_trigger << "=1" << std::endl;
        }
        return true;
    }

    uint16_t word_value = 0;
    std::string word_error;
    if (readWordRegister(plc_registers_.photo_trigger, &word_value, &word_error)) {
        if (word_value == 1) {
            *triggered = true;
            return true;
        }
    }

    float value = 0.0f;
    std::string float_error;
    if (readFloatRegister(plc_registers_.photo_trigger, &value, &float_error) &&
        std::fabs(value - 1.0f) < 0.001f) {
        *triggered = true;
        return true;
    }

    if (!word_error.empty() && !float_error.empty()) {
        if (error_message != nullptr) {
            *error_message = "trigger word read failed: " + word_error +
                             "; trigger float read failed: " + float_error;
        }
        return false;
    }

    *triggered = false;
    return true;
}

bool RobotController::clearPhotoTrigger(std::string* error_message)
{
    std::lock_guard<std::recursive_mutex> lock(plc_mutex_);

    if (simulation_enabled_) {
        simulated_photo_trigger_ = false;
        std::cout << "[PLC_SIM] clear D" << plc_registers_.photo_trigger
                  << "=0; result registers are kept for PLC/module cleanup" << std::endl;
        return true;
    }
    if (!writeWordRegister(plc_registers_.photo_trigger, 0, error_message)) {
        return false;
    }
    std::cout << "[PLC] cleared D" << plc_registers_.photo_trigger
              << "=0; result registers are kept for PLC/module cleanup" << std::endl;
    return true;
}

bool RobotController::writeFrameResult(const FrameInferenceResult& result, std::string* error_message)
{
    std::lock_guard<std::recursive_mutex> lock(plc_mutex_);

    const PlcWriteResult write_result = BuildPlcWriteResult(result, plcOutputConfig());
    if (write_result.pick_status >= 3.0f ||
        result.primary_index < 0 ||
        result.primary_index >= static_cast<int>(result.detections.size())) {
        return writeRejectStatus(error_message);
    }

    if (simulation_enabled_) {
        LogPlcWriteResult("[PLC_SIM] write result:", write_result, plc_registers_);
        return true;
    }

    if (!writeFloatRegisters(
            BuildPlcFrameResultRegisterWrites(result, plc_registers_, plcOutputConfig()),
            error_message)) {
        return false;
    }
    LogPlcWriteResult("[PLC] write result:", write_result, plc_registers_);
    return true;
}

bool RobotController::writeRejectStatus(std::string* error_message)
{
    std::lock_guard<std::recursive_mutex> lock(plc_mutex_);

    if (simulation_enabled_) {
        std::cout << "[PLC_SIM] write reject: D" << plc_registers_.pick_status
                  << "=3 only; D" << plc_registers_.front_back
                  << "/D" << plc_registers_.left_right
                  << "/D" << plc_registers_.angle
                  << "/D" << plc_registers_.head_type
                  << " are not written" << std::endl;
        return true;
    }

    for (const auto& write : BuildPlcRejectStatusRegisterWrite(plc_registers_)) {
        if (!writeFloatRegister(write.first, write.second, error_message)) {
            return false;
        }
    }
    std::cout << "[PLC] write reject: D" << plc_registers_.pick_status
              << "=3 only; D" << plc_registers_.front_back
              << "/D" << plc_registers_.left_right
              << "/D" << plc_registers_.angle
              << "/D" << plc_registers_.head_type
              << " are not written" << std::endl;
    return true;
}

bool RobotController::writePlcTestValues(std::string* error_message)
{
    std::lock_guard<std::recursive_mutex> lock(plc_mutex_);

    const PlcWriteResult test_result = BuildDiagnosticPlcWriteResult(plcOutputConfig());
    if (simulation_enabled_) {
        simulated_photo_trigger_ = true;
        std::cout << "[PLC_SIM] manual trigger: D" << plc_registers_.photo_trigger << "=1" << std::endl;
        return true;
    }

    if (!writeFloatRegisters({
            { plc_registers_.front_back, test_result.x },
            { plc_registers_.left_right, test_result.y },
            { plc_registers_.angle, test_result.angle },
            { plc_registers_.pick_status, 1.0f },
            { plc_registers_.head_type, 4.0f },
        },
        error_message)) {
        return false;
    }
    std::cout << "[PLC] test values written; result registers are not cleared by vision" << std::endl;
    return true;
}

void RobotController::setAngleCalibration(float offset_deg, bool reverse_direction, AngleRangeMode range_mode)
{
    std::lock_guard<std::recursive_mutex> lock(plc_mutex_);
    angle_offset_deg_ = offset_deg;
    angle_reverse_direction_ = reverse_direction;
    angle_range_mode_ = range_mode;
}

void RobotController::setAxisMapping(AxisMappingMode mode)
{
    std::lock_guard<std::recursive_mutex> lock(plc_mutex_);
    axis_mapping_mode_ = mode;
}

void RobotController::setAxisCompensation(float front_back_offset, float left_right_offset)
{
    std::lock_guard<std::recursive_mutex> lock(plc_mutex_);
    front_back_offset_ = front_back_offset;
    left_right_offset_ = left_right_offset;
}

QString RobotController::plcHost() const
{
    std::lock_guard<std::recursive_mutex> lock(plc_mutex_);

    if (simulation_enabled_) {
        return QStringLiteral("SIMULATED PLC");
    }
    return QStringLiteral("%1:%2").arg(plc_host_).arg(plc_port_);
}

PlcRegisterMap RobotController::plcRegisterMap() const
{
    std::lock_guard<std::recursive_mutex> lock(plc_mutex_);
    return plc_registers_;
}

bool RobotController::simulationEnabled() const
{
    std::lock_guard<std::recursive_mutex> lock(plc_mutex_);

    return simulation_enabled_;
}

bool RobotController::writeFloatRegister(int start_d_address, float value, std::string* error_message)
{
    QByteArray response;
    const QByteArray request = buildWriteMultipleRegistersRequest(start_d_address, value);
    if (!transact(request, 12, &response, error_message)) {
        return false;
    }

    const quint8 function_code = static_cast<quint8>(response.at(7));
    if ((function_code & 0x80) != 0) {
        if (error_message != nullptr) {
            const int exception_code = response.size() > 8 ? static_cast<quint8>(response.at(8)) : -1;
            *error_message = "PLC write exception: " + std::to_string(exception_code);
        }
        return false;
    }
    if (function_code != kWriteMultipleRegisters) {
        if (error_message != nullptr) {
            *error_message = "unexpected write function code: " + std::to_string(function_code);
        }
        return false;
    }
    return true;
}

bool RobotController::writeFloatRegisters(const std::vector<std::pair<int, float>>& writes,
                                          std::string* error_message)
{
    for (const auto& write : writes) {
        if (!writeFloatRegister(write.first, write.second, error_message)) {
            return false;
        }
    }
    return true;
}

bool RobotController::writeWordRegister(int start_d_address, uint16_t value, std::string* error_message)
{
    QByteArray response;
    const QByteArray request = buildWriteSingleRegisterRequest(start_d_address, value);
    if (!transact(request, 12, &response, error_message)) {
        return false;
    }

    const quint8 function_code = static_cast<quint8>(response.at(7));
    if ((function_code & 0x80) != 0) {
        if (error_message != nullptr) {
            const int exception_code = response.size() > 8 ? static_cast<quint8>(response.at(8)) : -1;
            *error_message = "PLC word write exception: " + std::to_string(exception_code);
        }
        return false;
    }
    if (function_code != kWriteSingleRegister) {
        if (error_message != nullptr) {
            *error_message = "unexpected word write function code: " + std::to_string(function_code);
        }
        return false;
    }
    return true;
}

bool RobotController::readFloatRegister(int start_d_address, float* value, std::string* error_message)
{
    QByteArray response;
    const QByteArray request = buildReadHoldingRegistersRequest(start_d_address, 2);
    if (!transact(request, 13, &response, error_message)) {
        return false;
    }

    const quint8 function_code = static_cast<quint8>(response.at(7));
    if ((function_code & 0x80) != 0) {
        if (error_message != nullptr) {
            const int exception_code = response.size() > 8 ? static_cast<quint8>(response.at(8)) : -1;
            *error_message = "PLC read exception: " + std::to_string(exception_code);
        }
        return false;
    }
    if (function_code != kReadHoldingRegisters || static_cast<quint8>(response.at(8)) < 4) {
        if (error_message != nullptr) {
            *error_message = "unexpected read response";
        }
        return false;
    }

    *value = decodeFloat(response, 9);
    return true;
}

bool RobotController::readWordRegister(int start_d_address, uint16_t* value, std::string* error_message)
{
    if (value == nullptr) {
        if (error_message != nullptr) {
            *error_message = "word output pointer is null";
        }
        return false;
    }

    QByteArray response;
    const QByteArray request = buildReadHoldingRegistersRequest(start_d_address, 1);
    if (!transact(request, 11, &response, error_message)) {
        return false;
    }

    const quint8 function_code = static_cast<quint8>(response.at(7));
    if ((function_code & 0x80) != 0) {
        if (error_message != nullptr) {
            const int exception_code = response.size() > 8 ? static_cast<quint8>(response.at(8)) : -1;
            *error_message = "PLC word read exception: " + std::to_string(exception_code);
        }
        return false;
    }
    if (function_code != kReadHoldingRegisters || static_cast<quint8>(response.at(8)) < 2) {
        if (error_message != nullptr) {
            *error_message = "unexpected word read response";
        }
        return false;
    }

    *value = (static_cast<uint16_t>(static_cast<quint8>(response.at(9))) << 8)
             | static_cast<uint16_t>(static_cast<quint8>(response.at(10)));
    return true;
}

bool RobotController::transact(const QByteArray& request,
                               int minimum_response_size,
                               QByteArray* response,
                               std::string* error_message)
{
    QTcpSocket socket;
    socket.connectToHost(plc_host_, plc_port_);
    if (!socket.waitForConnected(connect_timeout_ms_)) {
        if (error_message != nullptr) {
            *error_message = "connect failed: " + socket.errorString().toStdString();
        }
        return false;
    }

    if (socket.write(request) != request.size() || !socket.waitForBytesWritten(response_timeout_ms_)) {
        if (error_message != nullptr) {
            *error_message = "send failed: " + socket.errorString().toStdString();
        }
        socket.disconnectFromHost();
        return false;
    }

    if (!socket.waitForReadyRead(response_timeout_ms_)) {
        if (error_message != nullptr) {
            *error_message = "response timeout: " + socket.errorString().toStdString();
        }
        socket.disconnectFromHost();
        return false;
    }

    QByteArray response_data = socket.readAll();
    const int deadline_step_ms = 20;
    int waited_ms = 0;
    while (response_data.size() < minimum_response_size && waited_ms < response_timeout_ms_) {
        if (!socket.waitForReadyRead(deadline_step_ms)) {
            waited_ms += deadline_step_ms;
            continue;
        }
        response_data += socket.readAll();
    }

    if (response_data.size() < minimum_response_size) {
        if (error_message != nullptr) {
            *error_message = "short response from PLC";
        }
        socket.disconnectFromHost();
        return false;
    }

    if (response != nullptr) {
        *response = std::move(response_data);
    }
    socket.disconnectFromHost();
    return true;
}

QByteArray RobotController::buildReadHoldingRegistersRequest(int start_d_address, int register_count)
{
    QByteArray request;
    QDataStream stream(&request, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);
    stream << nextTransactionId();
    stream << static_cast<uint16_t>(0);
    stream << static_cast<uint16_t>(6);
    stream << static_cast<uint8_t>(modbus_unit_id_);
    stream << static_cast<uint8_t>(kReadHoldingRegisters);
    stream << static_cast<uint16_t>(start_d_address);
    stream << static_cast<uint16_t>(register_count);
    return request;
}

QByteArray RobotController::buildWriteMultipleRegistersRequest(int start_d_address, float value)
{
    const uint32_t bits = FloatToBits(value);
    const uint16_t high_word = static_cast<uint16_t>((bits >> 16) & 0xFFFF);
    const uint16_t low_word = static_cast<uint16_t>(bits & 0xFFFF);

    const uint16_t first_word = float_word_order_ == PlcFloatWordOrder::HighWordFirst ? high_word : low_word;
    const uint16_t second_word = float_word_order_ == PlcFloatWordOrder::HighWordFirst ? low_word : high_word;

    QByteArray request;
    QDataStream stream(&request, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);
    stream << nextTransactionId();
    stream << static_cast<uint16_t>(0);
    stream << static_cast<uint16_t>(11);
    stream << static_cast<uint8_t>(modbus_unit_id_);
    stream << static_cast<uint8_t>(kWriteMultipleRegisters);
    stream << static_cast<uint16_t>(start_d_address);
    stream << static_cast<uint16_t>(2);
    stream << static_cast<uint8_t>(4);
    stream << first_word;
    stream << second_word;
    return request;
}

QByteArray RobotController::buildWriteSingleRegisterRequest(int start_d_address, uint16_t value)
{
    QByteArray request;
    QDataStream stream(&request, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);
    stream << nextTransactionId();
    stream << static_cast<uint16_t>(0);
    stream << static_cast<uint16_t>(6);
    stream << static_cast<uint8_t>(modbus_unit_id_);
    stream << static_cast<uint8_t>(kWriteSingleRegister);
    stream << static_cast<uint16_t>(start_d_address);
    stream << value;
    return request;
}

uint16_t RobotController::nextTransactionId()
{
    ++transaction_id_;
    if (transaction_id_ == 0) {
        ++transaction_id_;
    }
    return transaction_id_;
}

float RobotController::decodeFloat(const QByteArray& data, int offset) const
{
    const uint16_t first_word =
        (static_cast<uint16_t>(static_cast<quint8>(data.at(offset))) << 8)
        | static_cast<uint16_t>(static_cast<quint8>(data.at(offset + 1)));
    const uint16_t second_word =
        (static_cast<uint16_t>(static_cast<quint8>(data.at(offset + 2))) << 8)
        | static_cast<uint16_t>(static_cast<quint8>(data.at(offset + 3)));
    const uint16_t high_word = float_word_order_ == PlcFloatWordOrder::HighWordFirst ? first_word : second_word;
    const uint16_t low_word = float_word_order_ == PlcFloatWordOrder::HighWordFirst ? second_word : first_word;
    return BitsToFloat((static_cast<uint32_t>(high_word) << 16) | low_word);
}

PlcOutputConfig RobotController::plcOutputConfig() const
{
    PlcOutputConfig config;
    config.angle_offset_deg = angle_offset_deg_;
    config.angle_reverse_direction = angle_reverse_direction_;
    config.angle_range_mode = angle_range_mode_;
    config.axis_mapping_mode = axis_mapping_mode_;
    config.front_back_offset = front_back_offset_;
    config.left_right_offset = left_right_offset_;
    return config;
}
