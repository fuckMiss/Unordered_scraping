#pragma once

#include "NvInfer.h"

#include <cuda_runtime_api.h>
#include <opencv2/opencv.hpp>

#include <string>
#include <vector>

using namespace nvinfer1;
using namespace std;
using namespace cv;

struct OBBDetection
{
    float conf;
    int class_id;
    RotatedRect rotated_rect;
    vector<Point2f> corners;
};

struct OBBConfig
{
    int expected_num_classes = -1;
    float conf_threshold = 0.5f;
    float nms_threshold = 0.3f;
    bool use_fp16 = false;
    bool enable_warmup = true;
    vector<string> class_names;
};

struct OBBRuntimeState
{
    int input_w = 0;
    int input_h = 0;

    int num_detections = 0;
    int detection_attribute_size = 0;
    int num_classes = 0;

    float scale_ratio = 1.0f;
    float pad_x = 0.0f;
    float pad_y = 0.0f;
    size_t output_numel = 0;
};

class YOLOv11_OBB
{
public:
    YOLOv11_OBB(string model_path, nvinfer1::ILogger& logger, const OBBConfig& config = {});
    ~YOLOv11_OBB();

    void preprocess(Mat& image);
    void infer();
    void postprocess(vector<OBBDetection>& output, int img_w, int img_h);
    void draw(Mat& image, const vector<OBBDetection>& output, const string& output_path = "obb_result.jpg");
    const vector<string>& getClassNames() const { return config_.class_names; }

private:
    void init(std::string engine_path, nvinfer1::ILogger& logger);
    void initializeEngineState();
    void bindBuffers();
    void cleanup() noexcept;

    float computeRotatedIoU(const OBBDetection& det1, const OBBDetection& det2) const;
    void nmsRotated(vector<OBBDetection>& detections, float nms_threshold) const;

    float* gpu_buffers[2]{};
    float* cpu_output_buffer = nullptr;

    cudaStream_t stream{};
    IRuntime* runtime = nullptr;
    ICudaEngine* engine = nullptr;
    IExecutionContext* context = nullptr;
    bool preprocess_initialized_ = false;

    OBBConfig config_;
    OBBRuntimeState runtime_;

    const int MAX_IMAGE_SIZE = 4096 * 4096;

    std::string input_tensor_name;
    std::string output_tensor_name;
    int input_binding_index = 0;
    int output_binding_index = 1;

    void build(std::string onnxPath, nvinfer1::ILogger& logger);
    bool saveEngine(const std::string& filename);
};
