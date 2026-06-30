#pragma once

#include "frame_result.h"

#include <QByteArray>
#include <QObject>
#include <QString>

#include <cstdint>
#include <functional>
#include <string>

struct PlcWriteResult
{
    float x = 0.0f;
    float y = 0.0f;
    float angle = 0.0f;
    float pick_status = 3.0f;
    float head_type = 0.0f;
};

class RobotController : public QObject
{
public:
    using DoneCallback = std::function<void()>;

    explicit RobotController(QObject* parent = nullptr);

    bool isBusy() const;
    void grabAsync(const PoseDetection& target, DoneCallback on_returned_home);
    void cancel();
    bool readPhotoTrigger(bool* triggered, std::string* error_message);
    bool clearPhotoTrigger(std::string* error_message);
    bool writeFrameResult(const FrameInferenceResult& result, std::string* error_message);
    bool writePlcTestValues(std::string* error_message);
    QString plcHost() const;

private:
    enum class FloatWordOrder {
        HighWordFirst,
        LowWordFirst
    };

    bool writeTargetToPlc(const PoseDetection& target, std::string* error_message);
    bool writeFloatRegister(int start_d_address, float value, std::string* error_message);
    bool readFloatRegister(int start_d_address, float* value, std::string* error_message);
    bool readWordRegister(int start_d_address, uint16_t* value, std::string* error_message);
    bool transact(const QByteArray& request,
                  int minimum_response_size,
                  QByteArray* response,
                  std::string* error_message);
    QByteArray buildReadHoldingRegistersRequest(int start_d_address, int register_count);
    QByteArray buildWriteMultipleRegistersRequest(int start_d_address, float value);
    uint16_t nextTransactionId();
    float decodeFloat(const QByteArray& data, int offset) const;
    PlcWriteResult buildWriteResult(const FrameInferenceResult& result) const;

    QString plc_host_ = QStringLiteral("192.168.3.205");
    quint16 plc_port_ = 502;
    int connect_timeout_ms_ = 1500;
    int response_timeout_ms_ = 1500;
    FloatWordOrder float_word_order_ = FloatWordOrder::HighWordFirst;
    bool busy_ = false;
    int generation_ = 0;
    uint16_t transaction_id_ = 0;
};
