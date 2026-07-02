#pragma once

#include <openvino/openvino.hpp>
#include <opencv2/opencv.hpp>

#include <cstddef>
#include <string>
#include <vector>

std::string ShapeToString(const ov::Shape& shape);
void ValidateBatchOneTensorShape(const ov::Shape& shape,
                                 std::size_t expected_rank,
                                 const std::string& tensor_name);
void ValidateBatchOneNchwInputShape(const ov::Shape& shape, const std::string& tensor_name);
ov::CompiledModel CompileModelWithGpuFallback(ov::Core& core,
                                              const std::string& model_path,
                                              std::string* actual_device);
std::vector<float> BuildNchwLetterboxInput(cv::Mat& image,
                                           int input_w,
                                           int input_h,
                                           float* scale_ratio,
                                           float* pad_x,
                                           float* pad_y);
std::vector<float> TensorToFloatVector(const ov::Tensor& tensor);
std::vector<float> TensorToAttributeMajorVector(const ov::Tensor& tensor,
                                                int detection_attribute_size,
                                                int num_detections);
