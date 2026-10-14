#include "openvino_utils.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {

void AcceptsOneAndThreeChannelInputShapes()
{
    ValidateBatchOneNchwInputShape({ 1, 1, 4, 4 }, "test input");
    ValidateBatchOneNchwInputShape({ 1, 3, 4, 4 }, "test input");
}

void RejectsUnsupportedInputChannelCount()
{
    bool rejected = false;
    try {
        ValidateBatchOneNchwInputShape({ 1, 2, 4, 4 }, "test input");
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    assert(rejected);
}

void BuildsOneChannelTensorBuffer()
{
    cv::Mat image(1, 1, CV_8UC3, cv::Scalar(30, 20, 10));
    const std::vector<float> input =
        BuildNchwLetterboxInput(image, 2, 2, 1, nullptr, nullptr, nullptr);

    assert(input.size() == 4);
    assert(std::fabs(input[0] - input[1]) < 1e-6f);
    assert(std::fabs(input[1] - input[2]) < 1e-6f);
    assert(std::fabs(input[2] - input[3]) < 1e-6f);
    assert(input[0] > 0.0f);
    assert(input[0] < 1.0f);
}

void BuildsThreeChannelTensorBuffer()
{
    cv::Mat image(1, 1, CV_8UC3, cv::Scalar(30, 20, 10));
    const std::vector<float> input =
        BuildNchwLetterboxInput(image, 2, 2, 3, nullptr, nullptr, nullptr);

    assert(input.size() == 12);
    assert(std::fabs(input[0] - 10.0f / 255.0f) < 1e-6f);
    assert(std::fabs(input[4] - 20.0f / 255.0f) < 1e-6f);
    assert(std::fabs(input[8] - 30.0f / 255.0f) < 1e-6f);
}

} // namespace

int main()
{
    AcceptsOneAndThreeChannelInputShapes();
    RejectsUnsupportedInputChannelCount();
    BuildsOneChannelTensorBuffer();
    BuildsThreeChannelTensorBuffer();
    std::cout << "openvino input tests passed" << std::endl;
    return 0;
}
