#pragma once

#include <openvino/openvino.hpp>
#include <opencv2/opencv.hpp>

#include <string>
#include <vector>

using namespace std;
using namespace cv;

struct SegDetection
{
    float conf;
    int class_id;
    Rect bbox;
    Mat mask;
};

struct SEGConfig
{
    int expected_num_classes = -1;
    float conf_threshold = 0.3f;
    float nms_threshold = 0.4f;
    float mask_threshold = 0.5f;
    float mask_alpha = 0.45f;
    bool use_fp16 = false;
    bool enable_warmup = false;
    vector<string> class_names;
};

struct SEGRuntimeState
{
    int input_w = 0;
    int input_h = 0;

    int num_detections = 0;
    int detection_attribute_size = 0;
    int num_classes = 0;

    int mask_dim = 0;
    int mask_h = 0;
    int mask_w = 0;

    float scale_ratio = 1.0f;
    float pad_x = 0.0f;
    float pad_y = 0.0f;

    size_t det_output_numel = 0;
    size_t mask_output_numel = 0;
};

class YOLOv11_SEG
{
public:
    YOLOv11_SEG(string model_path, const SEGConfig& config = {});

    void preprocess(Mat& image);
    void infer();
    void postprocess(vector<SegDetection>& output, int img_w, int img_h);
    void draw(Mat& image, const vector<SegDetection>& output, const string& output_path = "seg_result.jpg");
    const vector<string>& getClassNames() const { return config_.class_names; }
    const string& actualDevice() const { return actual_device_; }

private:
    void init(const string& model_path);
    void initializeModelState();

    Mat decodeMask(const vector<float>& coeffs, const Rect& box, int img_w, int img_h) const;

    ov::Core core_;
    ov::CompiledModel compiled_model_;
    ov::InferRequest infer_request_;
    ov::Output<const ov::Node> input_port_;
    ov::Output<const ov::Node> det_port_;
    ov::Output<const ov::Node> mask_port_;
    int det_output_index_ = 0;
    int mask_output_index_ = 1;
    vector<float> input_buffer_;
    vector<float> det_output_buffer_;
    vector<float> mask_output_buffer_;
    string actual_device_;

    SEGConfig config_;
    SEGRuntimeState runtime_;
};

