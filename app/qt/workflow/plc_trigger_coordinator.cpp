#include "plc_trigger_coordinator.h"

#include "frame_processing_service.h"

#include <QDateTime>
#include <QTextStream>

#include <iostream>

namespace {

double MsSince(const std::chrono::steady_clock::time_point& start,
               const std::chrono::steady_clock::time_point& end)
{
    return static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()) / 1000.0;
}

struct RejectWriteOutcome
{
    QString error_message;
    bool wrote_reject = false;
    bool trigger_cleared = false;
};

RejectWriteOutcome RejectAndMaybeClear(RobotController& robot_controller, bool clear_trigger)
{
    RejectWriteOutcome outcome;
    std::string error_message;
    if (!robot_controller.writeRejectStatus(&error_message)) {
        outcome.error_message = QStringLiteral("write reject failed: %1").arg(QString::fromStdString(error_message));
        return outcome;
    }
    outcome.wrote_reject = true;
    if (clear_trigger && !robot_controller.clearPhotoTrigger(&error_message)) {
        outcome.error_message = QStringLiteral("clear trigger failed: %1").arg(QString::fromStdString(error_message));
        return outcome;
    }
    outcome.trigger_cleared = clear_trigger;
    return outcome;
}

PlcWriteResult BuildRejectWriteResult()
{
    PlcWriteResult result;
    result.pick_status = 3.0f;
    result.head_type = 0.0f;
    return result;
}

QString JsonEscape(const QString& text)
{
    QString escaped;
    escaped.reserve(text.size() + 8);
    for (const QChar ch : text) {
        switch (ch.unicode()) {
        case '"': escaped += QStringLiteral("\\\""); break;
        case '\\': escaped += QStringLiteral("\\\\"); break;
        case '\b': escaped += QStringLiteral("\\b"); break;
        case '\f': escaped += QStringLiteral("\\f"); break;
        case '\n': escaped += QStringLiteral("\\n"); break;
        case '\r': escaped += QStringLiteral("\\r"); break;
        case '\t': escaped += QStringLiteral("\\t"); break;
        default:
            if (ch.unicode() < 0x20) {
                escaped += QStringLiteral("\\u%1").arg(ch.unicode(), 4, 16, QLatin1Char('0'));
            } else {
                escaped += ch;
            }
            break;
        }
    }
    return escaped;
}

QString JsonString(const QString& text)
{
    return QStringLiteral("\"%1\"").arg(JsonEscape(text));
}

QString FormatFloat(double value)
{
    return QString::number(value, 'f', 3);
}

QString BuildPlcTriggerLogLine(const PlcTriggerProcessResult& result)
{
    const FrameInferenceResult& frame = result.frame_result;
    const PlcWriteResult& plc = result.plc_write_result;
    const PlcRegisterMap& registers = result.plc_registers;
    const QString status = result.error_message.isEmpty() ? QStringLiteral("ok") : QStringLiteral("error");
    const QString write_mode = result.wrote_result
        ? QStringLiteral("result")
        : (result.wrote_reject ? QStringLiteral("reject") : QStringLiteral("none"));
    const QString reject_reason = result.reject_reason.isEmpty() ? QStringLiteral("") : result.reject_reason;
    const QString error_message = result.error_message.isEmpty() ? QStringLiteral("") : result.error_message;

    QString line;
    QTextStream stream(&line);
    stream << "[PLC_TRIGGER] {"
           << "\"ts\":" << JsonString(QDateTime::currentDateTime().toString(Qt::ISODateWithMs)) << ","
           << "\"status\":" << JsonString(status) << ","
           << "\"had_frame\":" << (result.had_frame ? "true" : "false") << ","
           << "\"input\":{\"trigger\":{\"D" << registers.photo_trigger << "\":1}"
           << ",\"width\":" << frame.image_width
           << ",\"height\":" << frame.image_height << "},"
           << "\"vision\":{\"detections\":" << static_cast<int>(frame.detections.size())
           << ",\"segments\":" << static_cast<int>(frame.segments.size())
           << ",\"primary_index\":" << frame.primary_index
           << ",\"pick_status\":" << frame.pick_status_code
           << ",\"head_type\":" << frame.head_type_code << "},"
           << "\"plc\":{\"write_mode\":" << JsonString(write_mode)
           << ",\"trigger_cleared\":" << (result.trigger_cleared ? "true" : "false")
           << ",\"D" << registers.front_back << "\":" << FormatFloat(plc.x)
           << ",\"D" << registers.left_right << "\":" << FormatFloat(plc.y)
           << ",\"D" << registers.angle << "\":" << FormatFloat(plc.angle)
           << ",\"D" << registers.pick_status << "\":" << FormatFloat(plc.pick_status)
           << ",\"D" << registers.head_type << "\":" << FormatFloat(plc.head_type)
           << ",\"source\":" << JsonString(plc.has_machine_coords ? QStringLiteral("machine") : QStringLiteral("image"))
           << ",\"image_x\":" << FormatFloat(plc.image_x)
           << ",\"image_y\":" << FormatFloat(plc.image_y) << "},"
           << "\"reject_reason\":" << JsonString(reject_reason) << ","
           << "\"error\":" << JsonString(error_message) << ","
           << "\"timing_ms\":{\"detect\":" << FormatFloat(result.detect_ms)
           << ",\"write_and_clear\":" << FormatFloat(result.write_and_trigger_clear_ms)
           << ",\"trigger_to_signal\":" << FormatFloat(result.trigger_to_signal_ms)
           << "}}";
    return line;
}

void FinalizeAndLog(PlcTriggerProcessResult& result)
{
    result.structured_log_line = BuildPlcTriggerLogLine(result);
    std::cout << result.structured_log_line.toStdString() << std::endl;
}

} // namespace

PlcTriggerReadResult ReadPlcPhotoTrigger(RobotController& robot_controller)
{
    PlcTriggerReadResult result;
    std::string error_message;
    if (!robot_controller.readPhotoTrigger(&result.triggered, &error_message)) {
        result.error_message = QString::fromStdString(error_message);
    }
    return result;
}

PlcTriggerProcessResult ProcessPlcTriggeredFrame(GraspWorkflow& workflow,
                                                 RobotController& robot_controller,
                                                 const cv::Mat& frame,
                                                 const GrabLimitConfig& limits,
                                                 const CoordinateTransformState& coordinate_state,
                                                 const PlcOutputConfig& plc_config,
                                                 const std::chrono::steady_clock::time_point& plc_start)
{
    PlcTriggerProcessResult process_result;
    process_result.had_frame = true;
    process_result.plc_registers = robot_controller.plcRegisterMap();

    const auto detect_start = std::chrono::steady_clock::now();
    const FrameProcessingResult frame_process =
        ProcessVisionFrame(workflow,
                           frame,
                           limits,
                           coordinate_state,
                           plc_config.axis_mapping_mode,
                           true);
    process_result.frame_result = frame_process.frame_result;
    if (!frame_process.error_message.isEmpty()) {
        if (frame_process.error_stage == FrameProcessingErrorStage::RoiOrCoordinate) {
            process_result.reject_reason = frame_process.error_message;
            std::cout << "[PLC] reject grab: " << process_result.reject_reason.toStdString() << std::endl;
            const auto detect_end = std::chrono::steady_clock::now();
            const auto write_start = std::chrono::steady_clock::now();
            const RejectWriteOutcome reject_outcome = RejectAndMaybeClear(robot_controller, true);
            process_result.error_message = reject_outcome.error_message;
            process_result.wrote_reject = reject_outcome.wrote_reject;
            process_result.trigger_cleared = reject_outcome.trigger_cleared;
            process_result.plc_write_result = BuildRejectWriteResult();
            const auto write_end = std::chrono::steady_clock::now();
            process_result.detect_ms = MsSince(detect_start, detect_end);
            process_result.write_and_trigger_clear_ms = MsSince(write_start, write_end);
            process_result.trigger_to_signal_ms = MsSince(plc_start, write_end);
            FinalizeAndLog(process_result);
            return process_result;
        }
        process_result.error_message = frame_process.error_stage == FrameProcessingErrorStage::Inference
            ? QStringLiteral("detect failed: %1").arg(frame_process.error_message)
            : frame_process.error_message;
        const auto error_end = std::chrono::steady_clock::now();
        process_result.detect_ms = MsSince(detect_start, error_end);
        process_result.trigger_to_signal_ms = MsSince(plc_start, error_end);
        FinalizeAndLog(process_result);
        return process_result;
    }
    const auto detect_end = std::chrono::steady_clock::now();

    const auto write_start = std::chrono::steady_clock::now();
    std::string error_message;
    const GrabLimitDecision limit_decision =
        EvaluateGrabLimits(process_result.frame_result, limits, plc_config);
    process_result.plc_write_result = BuildPlcWriteResult(process_result.frame_result, plc_config);
    if (limit_decision.rejected) {
        process_result.reject_reason = QString::fromStdString(limit_decision.reason);
        std::cout << "[PLC] reject grab: " << limit_decision.reason << std::endl;
        const RejectWriteOutcome reject_outcome = RejectAndMaybeClear(robot_controller, false);
        process_result.error_message = reject_outcome.error_message;
        process_result.wrote_reject = reject_outcome.wrote_reject;
        process_result.trigger_cleared = reject_outcome.trigger_cleared;
        if (process_result.wrote_reject) {
            process_result.plc_write_result = BuildRejectWriteResult();
        }
        if (!process_result.error_message.isEmpty()) {
            const auto error_end = std::chrono::steady_clock::now();
            process_result.detect_ms = MsSince(detect_start, detect_end);
            process_result.write_and_trigger_clear_ms = MsSince(write_start, error_end);
            process_result.trigger_to_signal_ms = MsSince(plc_start, error_end);
            FinalizeAndLog(process_result);
            return process_result;
        }
    } else if (!robot_controller.writeFrameResult(process_result.frame_result, &error_message)) {
        process_result.error_message =
            QStringLiteral("write failed: %1").arg(QString::fromStdString(error_message));
        const auto error_end = std::chrono::steady_clock::now();
        process_result.detect_ms = MsSince(detect_start, detect_end);
        process_result.write_and_trigger_clear_ms = MsSince(write_start, error_end);
        process_result.trigger_to_signal_ms = MsSince(plc_start, error_end);
        FinalizeAndLog(process_result);
        return process_result;
    } else {
        process_result.wrote_result = true;
    }

    if (!robot_controller.clearPhotoTrigger(&error_message)) {
        process_result.error_message =
            QStringLiteral("clear trigger failed: %1").arg(QString::fromStdString(error_message));
        const auto error_end = std::chrono::steady_clock::now();
        process_result.detect_ms = MsSince(detect_start, detect_end);
        process_result.write_and_trigger_clear_ms = MsSince(write_start, error_end);
        process_result.trigger_to_signal_ms = MsSince(plc_start, error_end);
        FinalizeAndLog(process_result);
        return process_result;
    }
    process_result.trigger_cleared = true;
    const auto write_end = std::chrono::steady_clock::now();

    process_result.detect_ms = MsSince(detect_start, detect_end);
    process_result.write_and_trigger_clear_ms = MsSince(write_start, write_end);
    process_result.trigger_to_signal_ms = MsSince(plc_start, write_end);
    FinalizeAndLog(process_result);
    return process_result;
}

PlcTriggerProcessResult ProcessPlcTriggeredMissingFrame(RobotController& robot_controller,
                                                        const PlcOutputConfig&,
                                                        const std::chrono::steady_clock::time_point& plc_start)
{
    PlcTriggerProcessResult process_result;
    process_result.had_frame = false;
    process_result.plc_registers = robot_controller.plcRegisterMap();
    process_result.reject_reason = QStringLiteral("no_camera_frame");
    process_result.frame_result.pick_status_code = 3;
    process_result.frame_result.head_type_code = 0;
    process_result.plc_write_result = BuildRejectWriteResult();

    const auto write_start = std::chrono::steady_clock::now();
    std::string error_message;
    if (!robot_controller.writeFrameResult(process_result.frame_result, &error_message)) {
        process_result.error_message =
            QStringLiteral("write failed: %1").arg(QString::fromStdString(error_message));
        const auto error_end = std::chrono::steady_clock::now();
        process_result.write_and_trigger_clear_ms = MsSince(write_start, error_end);
        process_result.trigger_to_signal_ms = MsSince(plc_start, error_end);
        FinalizeAndLog(process_result);
        return process_result;
    }
    process_result.wrote_reject = true;

    if (!robot_controller.clearPhotoTrigger(&error_message)) {
        process_result.error_message =
            QStringLiteral("clear trigger failed: %1").arg(QString::fromStdString(error_message));
        const auto error_end = std::chrono::steady_clock::now();
        process_result.write_and_trigger_clear_ms = MsSince(write_start, error_end);
        process_result.trigger_to_signal_ms = MsSince(plc_start, error_end);
        FinalizeAndLog(process_result);
        return process_result;
    }
    process_result.trigger_cleared = true;

    const auto write_end = std::chrono::steady_clock::now();
    process_result.write_and_trigger_clear_ms = MsSince(write_start, write_end);
    process_result.trigger_to_signal_ms = MsSince(plc_start, write_end);
    FinalizeAndLog(process_result);
    return process_result;
}

PlcManualWriteResult WriteFrameResultToPlc(RobotController& robot_controller,
                                           const FrameInferenceResult& result,
                                           const GrabLimitConfig& limits,
                                           const CoordinateTransformState& coordinate_state,
                                           const PlcOutputConfig& plc_config,
                                           bool clear_trigger)
{
    PlcManualWriteResult write_result;
    std::string error_message;

    if (coordinate_state.enabled && !coordinate_state.valid) {
        write_result.error_message = QString::fromStdString(coordinate_state.error_message);
        return write_result;
    }

    const GrabLimitDecision limit_decision = EvaluateGrabLimits(result, limits, plc_config);
    if (limit_decision.rejected) {
        std::cout << "[PLC] reject grab: " << limit_decision.reason << std::endl;
        if (!robot_controller.writeRejectStatus(&error_message)) {
            write_result.error_message = QString::fromStdString(error_message);
            return write_result;
        }
    } else if (!robot_controller.writeFrameResult(result, &error_message)) {
        write_result.error_message = QString::fromStdString(error_message);
        return write_result;
    }

    if (clear_trigger && !robot_controller.clearPhotoTrigger(&error_message)) {
        write_result.error_message =
            QStringLiteral("D%1 clear trigger failed: %2")
                .arg(robot_controller.plcRegisterMap().photo_trigger)
                .arg(QString::fromStdString(error_message));
    }
    return write_result;
}
