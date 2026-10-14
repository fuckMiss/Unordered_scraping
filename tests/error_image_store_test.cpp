#include "error_image_store.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <opencv2/core.hpp>

#include <cassert>
#include <iostream>

namespace {

QImage CreateOverlayImage()
{
    QImage image(4, 3, QImage::Format_RGB888);
    image.fill(Qt::red);
    return image;
}

cv::Mat CreateRawImage()
{
    return cv::Mat(3, 4, CV_8UC3, cv::Scalar(10, 20, 30)).clone();
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    QTemporaryDir temporary_directory;
    assert(temporary_directory.isValid());

    const cv::Mat raw_image = CreateRawImage();
    const QImage overlay_image = CreateOverlayImage();
    const QString image_directory =
        temporary_directory.filePath(QStringLiteral("error_images"));
    const ErrorImageSaveResult first =
        SaveErrorImagePair(raw_image, overlay_image, image_directory);
    assert(first.success);
    assert(!first.partial);
    assert(QFile::exists(first.raw_path));
    assert(QFile::exists(first.overlay_path));
    assert(first.raw_path.endsWith(QStringLiteral("_raw.png")));
    assert(first.overlay_path.endsWith(QStringLiteral("_overlay.png")));

    const ErrorImageSaveResult second =
        SaveErrorImagePair(raw_image, overlay_image, image_directory);
    assert(second.success);
    assert(first.raw_path != second.raw_path);
    assert(first.overlay_path != second.overlay_path);
    assert(QFile::exists(second.raw_path));
    assert(QFile::exists(second.overlay_path));

    const QString protected_path = QDir(image_directory).filePath(QStringLiteral("other.png"));
    QFile protected_file(protected_path);
    assert(protected_file.open(QIODevice::WriteOnly));
    protected_file.write("protected");
    protected_file.close();
    const ErrorImageSaveResult third =
        SaveErrorImagePair(raw_image, overlay_image, image_directory);
    assert(third.success);
    assert(QFile::exists(protected_path));

    const ErrorImageSaveResult invalid =
        SaveErrorImagePair(cv::Mat(), overlay_image, image_directory);
    assert(!invalid.success);
    assert(!invalid.partial);
    assert(!invalid.error_message.isEmpty());

    const ErrorImageSaveResult invalid_overlay =
        SaveErrorImagePair(raw_image, QImage(), image_directory);
    assert(!invalid_overlay.success);
    assert(!invalid_overlay.partial);
    assert(!invalid_overlay.error_message.isEmpty());

    std::cout << "error_image_store_test passed" << std::endl;
    return 0;
}
