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
struct DetectionOutputLayout
{
    int detection_attribute_size = 0;
    int num_detections = 0;
    int num_classes = 0;
};
DetectionOutputLayout ParseDetectionOutputLayout(const ov::Shape& shape,
                                                 int fixed_attribute_count,
                                                 int extra_attribute_count,
                                                 const std::string& tensor_name,
                                                 const std::string& detail = "");
std::vector<float> BuildNchwLetterboxInput(cv::Mat& image,
                                           int input_w,
                                           int input_h,
                                           float* scale_ratio,
                                           float* pad_x,
                                           float* pad_y);
void PrepareNchwLetterboxInput(cv::Mat& image,
                               int input_w,
                               int input_h,
                               const ov::Output<const ov::Node>& input_port,
                               ov::InferRequest& infer_request,
                               std::vector<float>& input_buffer,
                               float* scale_ratio,
                               float* pad_x,
                               float* pad_y);
void WarmupInferRequest(ov::InferRequest& infer_request,
                        const ov::Output<const ov::Node>& input_port,
                        std::vector<float>& input_buffer,
                        int iterations = 5);
std::vector<float> TensorToFloatVector(const ov::Tensor& tensor);
std::vector<float> TensorToAttributeMajorVector(const ov::Tensor& tensor,
                                                int detection_attribute_size,
                                                int num_detections);
