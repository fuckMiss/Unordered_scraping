#pragma once

#include <QImage>
#include <QString>

#include <opencv2/core/mat.hpp>

struct ErrorImageSaveResult
{
    bool success = false;
    bool partial = false;
    QString raw_path;
    QString overlay_path;
    QString error_message;
};

ErrorImageSaveResult SaveErrorImagePair(const cv::Mat& raw_image,
                                        const QImage& overlay_image,
                                        const QString& directory_path);
