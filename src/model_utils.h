#pragma once

#include "common.h"
#include "NvInfer.h"

#include <algorithm>
#include <opencv2/opencv.hpp>
#include <stdexcept>
#include <string>
#include <vector>

inline std::string DimsToString(const nvinfer1::Dims& dims)
{
    std::string result = "[";
    for (int i = 0; i < dims.nbDims; ++i) {
        if (i > 0) {
            result += ", ";
        }
        result += std::to_string(dims.d[i]);
    }
    result += "]";
    return result;
}

inline std::vector<std::string> BuildDefaultClassNames(const std::vector<std::string>& default_names,
                                                       int num_classes)
{
    if (num_classes == static_cast<int>(default_names.size())) {
        return default_names;
    }

    std::vector<std::string> names;
    names.reserve(std::max(num_classes, 0));
    for (int i = 0; i < num_classes; ++i) {
        names.push_back("class_" + std::to_string(i));
    }
    return names;
}

inline std::vector<std::string> BuildCocoClassNames(int num_classes)
{
    return BuildDefaultClassNames(CLASS_NAMES, num_classes);
}

inline void ValidateInputImage(cv::Mat& image, int max_image_size)
{
    if (image.empty()) {
        throw std::invalid_argument("Input image is empty");
    }
    if (image.depth() != CV_8U) {
        throw std::invalid_argument("Input image must use 8-bit channels");
    }
    if (image.channels() == 1) {
        cv::cvtColor(image, image, cv::COLOR_GRAY2BGR);
    } else if (image.channels() == 4) {
        cv::cvtColor(image, image, cv::COLOR_BGRA2BGR);
    } else if (image.channels() != 3) {
        throw std::invalid_argument("Input image must have 1, 3, or 4 channels");
    }
    if (!image.isContinuous()) {
        image = image.clone();
    }
    const int64_t pixel_count = static_cast<int64_t>(image.cols) * static_cast<int64_t>(image.rows);
    if (pixel_count > static_cast<int64_t>(max_image_size)) {
        throw std::invalid_argument("Input image is larger than preprocess buffer limit");
    }
}

inline void ValidateStaticTensorDims(const nvinfer1::Dims& dims, int expected_rank, const std::string& tensor_name)
{
    if (dims.nbDims != expected_rank) {
        throw std::runtime_error(
            "Unsupported " + tensor_name + " tensor rank: expected " + std::to_string(expected_rank) +
            ", got " + std::to_string(dims.nbDims) + " " + DimsToString(dims));
    }

    for (int i = 0; i < dims.nbDims; ++i) {
        if (dims.d[i] <= 0) {
            throw std::runtime_error(
                "Dynamic or invalid " + tensor_name + " tensor dimension at axis " + std::to_string(i) +
                ": " + DimsToString(dims));
        }
    }
}

inline void ValidateBatchOneTensorDims(const nvinfer1::Dims& dims,
                                       int expected_rank,
                                       const std::string& tensor_name)
{
    ValidateStaticTensorDims(dims, expected_rank, tensor_name);
    if (dims.d[0] != 1) {
        throw std::runtime_error(
            "Unsupported " + tensor_name + " batch size: expected 1, got " +
            std::to_string(dims.d[0]) + " " + DimsToString(dims));
    }
}

inline void ValidateBatchOneNchwInputDims(const nvinfer1::Dims& dims, const std::string& tensor_name)
{
    ValidateBatchOneTensorDims(dims, 4, tensor_name);
    if (dims.d[1] != 3) {
        throw std::runtime_error(
            "Unsupported " + tensor_name + " channel count/layout: expected [1, 3, H, W], got " +
            DimsToString(dims));
    }
}

inline cv::Scalar GetClassColor(int class_id)
{
    if (class_id >= 0 && class_id < static_cast<int>(COLORS.size())) {
        return cv::Scalar(COLORS[class_id][0], COLORS[class_id][1], COLORS[class_id][2]);
    }
    return cv::Scalar(0, 255, 0);
}

inline std::string GetClassName(const std::vector<std::string>& class_names, int class_id)
{
    if (class_id >= 0 && class_id < static_cast<int>(class_names.size())) {
        return class_names[class_id];
    }
    return "class_" + std::to_string(class_id);
}
