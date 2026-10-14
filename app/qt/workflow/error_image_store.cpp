#include "error_image_store.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>

#include <opencv2/imgcodecs.hpp>

namespace {

QString ErrorImageBaseName(const QString& stamp, int sequence)
{
    return QStringLiteral("tankeye_error_%1%2")
        .arg(stamp, sequence == 0 ? QString() : QStringLiteral("_%1").arg(sequence));
}

} // namespace

ErrorImageSaveResult SaveErrorImagePair(const cv::Mat& raw_image,
                                        const QImage& overlay_image,
                                        const QString& directory_path)
{
    ErrorImageSaveResult result;
    if (raw_image.empty()) {
        result.error_message = QStringLiteral("当前没有可保存的 PLC 检测帧。");
        return result;
    }
    if (overlay_image.isNull()) {
        result.error_message = QStringLiteral("错误图片 overlay 生成失败。");
        return result;
    }

    QDir directory(directory_path);
    if (!directory.exists() && !QDir().mkpath(directory_path)) {
        result.error_message = QStringLiteral("无法创建错误图片目录：%1").arg(directory_path);
        return result;
    }

    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz"));
    QString raw_path;
    QString overlay_path;
    for (int sequence = 0; sequence < 100000; ++sequence) {
        const QString base_name = ErrorImageBaseName(stamp, sequence);
        const QString candidate_raw_path = directory.filePath(base_name + QStringLiteral("_raw.png"));
        const QString candidate_overlay_path = directory.filePath(base_name + QStringLiteral("_overlay.png"));
        if (!QFileInfo::exists(candidate_raw_path) && !QFileInfo::exists(candidate_overlay_path)) {
            raw_path = candidate_raw_path;
            overlay_path = candidate_overlay_path;
            break;
        }
    }
    if (raw_path.isEmpty() || overlay_path.isEmpty()) {
        result.error_message = QStringLiteral("无法生成唯一的错误图片文件名。");
        return result;
    }

    try {
        if (!cv::imwrite(raw_path.toStdString(), raw_image)) {
            result.error_message = QStringLiteral("错误图片原图保存失败。");
            return result;
        }
    } catch (const cv::Exception& exception) {
        result.error_message = QStringLiteral("错误图片原图保存失败：%1")
            .arg(QString::fromLocal8Bit(exception.what()));
        return result;
    }

    result.raw_path = QFileInfo(raw_path).absoluteFilePath();
    if (!overlay_image.save(overlay_path, "PNG")) {
        result.partial = true;
        result.error_message = QStringLiteral("原图已保存，但错误图片 overlay 保存失败。");
        return result;
    }

    result.success = true;
    result.overlay_path = QFileInfo(overlay_path).absoluteFilePath();
    return result;
}
