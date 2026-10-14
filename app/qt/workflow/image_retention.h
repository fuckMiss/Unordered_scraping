#pragma once

#include <QString>

struct ImageRetentionSettings
{
    int max_saved_images = 1000;
};

int ClampMaxSavedImages(int max_saved_images);

bool EnforceImageRetention(const QString& image_directory,
                           int max_saved_images,
                           QString* warning_message = nullptr);
