#include "image_retention.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileInfoList>

#include <algorithm>

namespace {

constexpr int kDefaultMaxSavedImages = 1000;
constexpr int kMinMaxSavedImages = 1;
constexpr int kMaxMaxSavedImages = 100000;

bool IsManagedImage(const QFileInfo& file_info)
{
    return file_info.isFile() &&
           file_info.fileName().startsWith(QStringLiteral("tankeye_plc_")) &&
           file_info.suffix().compare(QStringLiteral("png"), Qt::CaseInsensitive) == 0;
}

} // namespace

int ClampMaxSavedImages(int max_saved_images)
{
    if (max_saved_images <= 0) {
        return kDefaultMaxSavedImages;
    }
    return std::max(kMinMaxSavedImages, std::min(kMaxMaxSavedImages, max_saved_images));
}

bool EnforceImageRetention(const QString& image_directory,
                           int max_saved_images,
                           QString* warning_message)
{
    if (warning_message != nullptr) {
        warning_message->clear();
    }

    const QDir directory(image_directory);
    if (!directory.exists()) {
        return true;
    }

    QFileInfoList files = directory.entryInfoList(QStringList() << QStringLiteral("tankeye_plc_*.png"),
                                                  QDir::Files,
                                                  QDir::Name);
    files.erase(std::remove_if(files.begin(), files.end(), [](const QFileInfo& file_info) {
        return !IsManagedImage(file_info);
    }), files.end());

    const int limit = ClampMaxSavedImages(max_saved_images);
    while (files.size() > limit) {
        const QFileInfo oldest_file = files.front();
        if (!QFile::remove(oldest_file.absoluteFilePath())) {
            if (warning_message != nullptr) {
                *warning_message = QStringLiteral("旧图片清理失败：%1").arg(oldest_file.absoluteFilePath());
            }
            return false;
        }
        files.removeFirst();
    }
    return true;
}
