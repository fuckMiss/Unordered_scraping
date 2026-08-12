#pragma once

#include <openvino/openvino.hpp>
#include <opencv2/opencv.hpp>

#include <string>
#include <vector>

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
    bool enable_warmup = false;
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
    YOLOv11_OBB(string model_path, const OBBConfig& config = {});

    void preprocess(Mat& image);
    void infer();
    void postprocess(vector<OBBDetection>& output, int img_w, int img_h);
    void draw(Mat& image, const vector<OBBDetection>& output, const string& output_path = "obb_result.jpg");
    const vector<string>& getClassNames() const { return config_.class_names; }
    const string& actualDevice() const { return actual_device_; }

private:
    void init(const string& model_path);
    void initializeModelState();

    float computeRotatedIoU(const OBBDetection& det1, const OBBDetection& det2) const;
    void nmsRotated(vector<OBBDetection>& detections, float nms_threshold) const;

    ov::Core core_;
    ov::CompiledModel compiled_model_;
    ov::InferRequest infer_request_;
    ov::Output<const ov::Node> input_port_;
    ov::Output<const ov::Node> output_port_;
    vector<float> input_buffer_;
    vector<float> output_buffer_;
    string actual_device_;

    OBBConfig config_;
    OBBRuntimeState runtime_;
};

