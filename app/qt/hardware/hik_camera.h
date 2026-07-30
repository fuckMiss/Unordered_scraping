#pragma once

#include "MvCameraControl.h"

#include <opencv2/opencv.hpp>

#include <string>

class HikCamera
{
public:
    HikCamera();
    ~HikCamera();

    HikCamera(const HikCamera&) = delete;
    HikCamera& operator=(const HikCamera&) = delete;

    bool open(const std::string& preferred_ip, std::string* error_message, double exposure_us = 0.0);
    bool grab(cv::Mat& frame, std::string* error_message);
    bool setManualExposure(double exposure_us, std::string* error_message);
    bool getExposure(double* exposure_us, std::string* error_message) const;
    bool autoExposureOnce(double* exposure_us, std::string* error_message);
    void close();
    bool isOpen() const;

private:
    bool convertFrameToBgr(const MV_FRAME_OUT& sdk_frame,
                           cv::Mat& frame,
                           std::string* error_message);

    void* handle_ = nullptr;
    bool sdk_initialized_ = false;
    bool grabbing_ = false;
};
