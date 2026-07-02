#include "robot_controller.h"

#include <QDataStream>
#include <QIODevice>
#include <QTcpSocket>

#include <cmath>
#include <cstring>
#include <iostream>
#include <utility>

namespace {

constexpr int kModbusUnitId = 1;
constexpr int kReadHoldingRegisters = 0x03;
constexpr int kWriteSingleRegister = 0x06;
constexpr int kWriteMultipleRegisters = 0x10;

constexpr int kAddressX = 500;
constexpr int kAddressY = 502;
constexpr int kAddressAngle = 504;
constexpr int kAddressPickStatus = 506;
constexpr int kAddressHeadType = 508;
constexpr int kAddressPhotoTrigger = 1500;

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

}  // namespace

RobotController::RobotController(QObject* parent)
    : QObject(parent)
{
}

bool RobotController::isBusy() const
{
    return busy_;
}

void RobotController::grabAsync(const PoseDetection& target, DoneCallback on_returned_home)
{
    busy_ = true;
    ++generation_;

    std::string error_message;
    const bool ok = writeTargetToPlc(target, &error_message);
    busy_ = false;

    if (ok) {
        std::cout << "[PLC] target written: D500=" << target.center_x
                  << ", D502=" << target.center_y
                  << ", D504=" << target.angle_deg
                  << ", D506=" << target.pick_status_code
                  << ", D508=" << target.head_type_code
                  << std::endl;
    } else {
        std::cout << "[PLC] write target failed: " << error_message << std::endl;
    }

    if (ok && on_returned_home) {
        on_returned_home();
    }
}

void RobotController::cancel()
{
    if (busy_) {
        std::cout << "[PLC] operation cancelled" << std::endl;
    }
    busy_ = false;
    ++generation_;
}

bool RobotController::readPhotoTrigger(bool* triggered, std::string* error_message)
{
    if (triggered == nullptr) {
        if (error_message != nullptr) {
            *error_message = "triggered pointer is null";
        }
        return false;
    }

    uint16_t word_value = 0;
    std::string word_error;
    if (readWordRegister(kAddressPhotoTrigger, &word_value, &word_error)) {
        if (word_value == 1) {
            *triggered = true;
            return true;
        }
    }

    float value = 0.0f;
    std::string float_error;
    if (readFloatRegister(kAddressPhotoTrigger, &value, &float_error) &&
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
    return writeWordRegister(kAddressPhotoTrigger, 0, error_message);
}

bool RobotController::writeFrameResult(const FrameInferenceResult& result, std::string* error_message)
{
    const PlcWriteResult write_result = buildWriteResult(result);
    if (!writeFloatRegister(kAddressX, write_result.x, error_message)) {
        return false;
    }
    if (!writeFloatRegister(kAddressY, write_result.y, error_message)) {
        return false;
    }
    if (!writeFloatRegister(kAddressAngle, write_result.angle, error_message)) {
        return false;
    }
    if (!writeFloatRegister(kAddressPickStatus, write_result.pick_status, error_message)) {
        return false;
    }
    if (!writeFloatRegister(kAddressHeadType, write_result.head_type, error_message)) {
        return false;
    }
    return true;
}

bool RobotController::writePlcTestValues(std::string* error_message)
{
    if (!writeFloatRegister(kAddressX, 123.4f, error_message)) {
        return false;
    }
    if (!writeFloatRegister(kAddressY, 56.7f, error_message)) {
        return false;
    }
    if (!writeFloatRegister(kAddressAngle, 90.0f, error_message)) {
        return false;
    }
    if (!writeFloatRegister(kAddressPickStatus, 1.0f, error_message)) {
        return false;
    }
    if (!writeFloatRegister(kAddressHeadType, 4.0f, error_message)) {
        return false;
    }
    return true;
}

QString RobotController::plcHost() const
{
    return plc_host_;
}

bool RobotController::writeTargetToPlc(const PoseDetection& target, std::string* error_message)
{
    FrameInferenceResult result;
    result.detections.push_back(target);
    result.primary_index = 0;
    result.pick_status_code = target.pick_status_code;
    result.head_type_code = target.head_type_code;
    return writeFrameResult(result, error_message);
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
    stream << static_cast<uint8_t>(kModbusUnitId);
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

    const uint16_t first_word = float_word_order_ == FloatWordOrder::HighWordFirst ? high_word : low_word;
    const uint16_t second_word = float_word_order_ == FloatWordOrder::HighWordFirst ? low_word : high_word;

    QByteArray request;
    QDataStream stream(&request, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);
    stream << nextTransactionId();
    stream << static_cast<uint16_t>(0);
    stream << static_cast<uint16_t>(11);
    stream << static_cast<uint8_t>(kModbusUnitId);
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
    stream << static_cast<uint8_t>(kModbusUnitId);
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
    const uint16_t high_word = float_word_order_ == FloatWordOrder::HighWordFirst ? first_word : second_word;
    const uint16_t low_word = float_word_order_ == FloatWordOrder::HighWordFirst ? second_word : first_word;
    return BitsToFloat((static_cast<uint32_t>(high_word) << 16) | low_word);
}

PlcWriteResult RobotController::buildWriteResult(const FrameInferenceResult& result) const
{
    PlcWriteResult write_result;
    write_result.pick_status = static_cast<float>(result.pick_status_code);
    write_result.head_type = static_cast<float>(result.head_type_code);

    if (result.primary_index >= 0 && result.primary_index < static_cast<int>(result.detections.size())) {
        const PoseDetection& target = result.detections[result.primary_index];
        write_result.x = target.center_x;
        write_result.y = target.center_y;
        write_result.angle = target.angle_deg;
        write_result.pick_status = static_cast<float>(target.pick_status_code);
        write_result.head_type = static_cast<float>(target.head_type_code);
    }

    return write_result;
}
