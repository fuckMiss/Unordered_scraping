#include "YOLOv11_SEG.h"

#include "model_utils.h"
#include "openvino_utils.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>
#include <stdexcept>

static const std::vector<std::string> SEG_CLASS_NAMES = {
    "border"
};

static vector<string> buildDefaultSegClassNames(int num_classes) {
    if (!SEG_CLASS_NAMES.empty()) {
        return BuildDefaultClassNames(SEG_CLASS_NAMES, num_classes);
    }
    return BuildCocoClassNames(num_classes);
}

static bool SegCandidateStableLess(int left_index,
                                   int right_index,
                                   const vector<float>& confidences,
                                   const vector<int>& class_ids,
                                   const vector<Rect>& boxes)
{
    constexpr float kEpsilon = 1e-6f;
    const float left_conf = confidences[left_index];
    const float right_conf = confidences[right_index];
    if (std::fabs(left_conf - right_conf) > kEpsilon) {
        return left_conf > right_conf;
    }
    if (class_ids[left_index] != class_ids[right_index]) {
        return class_ids[left_index] < class_ids[right_index];
    }
    const Rect& left_box = boxes[left_index];
    const Rect& right_box = boxes[right_index];
    if (left_box.y != right_box.y) {
        return left_box.y < right_box.y;
    }
    if (left_box.x != right_box.x) {
        return left_box.x < right_box.x;
    }
    if (left_box.area() != right_box.area()) {
        return left_box.area() > right_box.area();
    }
    if (left_box.height != right_box.height) {
        return left_box.height > right_box.height;
    }
    return left_index < right_index;
}

YOLOv11_SEG::YOLOv11_SEG(string model_path, const SEGConfig& config)
{
    config_ = config;
    init(model_path);
}

void YOLOv11_SEG::init(const string& model_path)
{
    compiled_model_ = CompileModelWithGpuFallback(core_, model_path, &actual_device_);
    infer_request_ = compiled_model_.create_infer_request();
    initializeModelState();
}

void YOLOv11_SEG::initializeModelState()
{
    if (compiled_model_.inputs().size() != 1 || compiled_model_.outputs().size() != 2) {
        throw runtime_error("SEG model must expose exactly 1 input tensor and 2 output tensors");
    }

    input_port_ = compiled_model_.input();
    const ov::Shape input_shape = input_port_.get_shape();
    ValidateBatchOneNchwInputShape(input_shape, "SEG input");
    runtime_.input_h = static_cast<int>(input_shape[2]);
    runtime_.input_w = static_cast<int>(input_shape[3]);

    const ov::Output<const ov::Node> output_a = compiled_model_.output(0);
    const ov::Output<const ov::Node> output_b = compiled_model_.output(1);
    const ov::Shape shape_a = output_a.get_shape();
    const ov::Shape shape_b = output_b.get_shape();

    if (shape_a.size() == 3 && shape_b.size() == 4) {
        det_port_ = output_a;
        mask_port_ = output_b;
        det_output_index_ = 0;
        mask_output_index_ = 1;
    } else if (shape_a.size() == 4 && shape_b.size() == 3) {
        det_port_ = output_b;
        mask_port_ = output_a;
        det_output_index_ = 1;
        mask_output_index_ = 0;
    } else {
        det_port_ = output_a;
        mask_port_ = output_b;
        det_output_index_ = 0;
        mask_output_index_ = 1;
    }

    const ov::Shape det_shape = det_port_.get_shape();
    const ov::Shape mask_shape = mask_port_.get_shape();
    ValidateBatchOneTensorShape(det_shape, 3, "SEG detection output");
    ValidateBatchOneTensorShape(mask_shape, 4, "SEG mask output");

    runtime_.mask_dim = static_cast<int>(mask_shape[1]);
    runtime_.mask_h = static_cast<int>(mask_shape[2]);
    runtime_.mask_w = static_cast<int>(mask_shape[3]);
    if (runtime_.mask_dim <= 0 || runtime_.mask_h <= 0 || runtime_.mask_w <= 0) {
        ostringstream oss;
        oss << "Invalid SEG mask prototype output shape: " << ShapeToString(mask_shape);
        throw runtime_error(oss.str());
    }

    const DetectionOutputLayout output_layout =
        ParseDetectionOutputLayout(det_shape,
                                   4,
                                   runtime_.mask_dim,
                                   "SEG",
                                   "proto=" + ShapeToString(mask_shape) +
                                       ", expected detection_attribute_size = 4 + num_classes + mask_dim");
    runtime_.detection_attribute_size = output_layout.detection_attribute_size;
    runtime_.num_detections = output_layout.num_detections;
    runtime_.num_classes = output_layout.num_classes;
    if (config_.expected_num_classes > 0 && config_.expected_num_classes != runtime_.num_classes) {
        ostringstream oss;
        oss << "Configured num_classes (" << config_.expected_num_classes
            << ") does not match model output (" << runtime_.num_classes << ")";
        throw runtime_error(oss.str());
    }

    if (config_.class_names.empty()) {
        config_.class_names = buildDefaultSegClassNames(runtime_.num_classes);
    } else if (static_cast<int>(config_.class_names.size()) != runtime_.num_classes) {
        ostringstream oss;
        oss << "Configured class_names size (" << config_.class_names.size()
            << ") does not match num_classes (" << runtime_.num_classes << ")";
        throw runtime_error(oss.str());
    }

    runtime_.det_output_numel =
        static_cast<size_t>(runtime_.detection_attribute_size) * runtime_.num_detections;
    runtime_.mask_output_numel =
        static_cast<size_t>(runtime_.mask_dim) * runtime_.mask_h * runtime_.mask_w;

    input_buffer_.assign(static_cast<size_t>(3) * runtime_.input_w * runtime_.input_h, 0.0f);
    det_output_buffer_.assign(runtime_.det_output_numel, 0.0f);
    mask_output_buffer_.assign(runtime_.mask_output_numel, 0.0f);

    cout << "YOLOv11_SEG OpenVINO Model Info:" << endl;
    cout << "  Device: " << actual_device_ << endl;
    cout << "  Input: " << runtime_.input_w << "x" << runtime_.input_h << endl;
    cout << "  Raw Detection Tensor: " << ShapeToString(det_shape) << endl;
    cout << "  Raw Proto Tensor: " << ShapeToString(mask_shape) << endl;
    cout << "  Detection Output: " << runtime_.detection_attribute_size << " x " << runtime_.num_detections << endl;
    cout << "  Proto Output: " << runtime_.mask_dim << " x " << runtime_.mask_h << " x " << runtime_.mask_w << endl;
    cout << "  Num classes: " << runtime_.num_classes << endl;

    if (config_.enable_warmup) {
        WarmupInferRequest(infer_request_, input_port_, input_buffer_);
    }
}

void YOLOv11_SEG::preprocess(Mat& image)
{
    PrepareNchwLetterboxInput(image,
                              runtime_.input_w,
                              runtime_.input_h,
                              input_port_,
                              infer_request_,
                              input_buffer_,
                              &runtime_.scale_ratio,
                              &runtime_.pad_x,
                              &runtime_.pad_y);
}

void YOLOv11_SEG::infer()
{
    infer_request_.infer();
}

Mat YOLOv11_SEG::decodeMask(const vector<float>& coeffs, const Rect& box, int img_w, int img_h) const
{
    Mat protos(runtime_.mask_dim, runtime_.mask_h * runtime_.mask_w, CV_32F, const_cast<float*>(mask_output_buffer_.data()));
    Mat coeff_mat(1, runtime_.mask_dim, CV_32F, const_cast<float*>(coeffs.data()));
    Mat mask_flat = coeff_mat * protos;
    Mat mask = mask_flat.reshape(1, runtime_.mask_h);

    Mat mask_sigmoid;
    exp(-mask, mask_sigmoid);
    mask_sigmoid = 1.0 / (1.0 + mask_sigmoid);

    const float proto_to_input_x = runtime_.input_w / static_cast<float>(runtime_.mask_w);
    const float proto_to_input_y = runtime_.input_h / static_cast<float>(runtime_.mask_h);

    const int x0 = max(0, min(runtime_.mask_w, static_cast<int>(floor((box.x * runtime_.scale_ratio + runtime_.pad_x) / proto_to_input_x))));
    const int y0 = max(0, min(runtime_.mask_h, static_cast<int>(floor((box.y * runtime_.scale_ratio + runtime_.pad_y) / proto_to_input_y))));
    const int x1 = max(0, min(runtime_.mask_w, static_cast<int>(ceil(((box.x + box.width) * runtime_.scale_ratio + runtime_.pad_x) / proto_to_input_x))));
    const int y1 = max(0, min(runtime_.mask_h, static_cast<int>(ceil(((box.y + box.height) * runtime_.scale_ratio + runtime_.pad_y) / proto_to_input_y))));

    if (x1 <= x0 || y1 <= y0) {
        return Mat::zeros(img_h, img_w, CV_8U);
    }

    Mat crop = mask_sigmoid(Range(y0, y1), Range(x0, x1)).clone();
    if (crop.empty()) {
        return Mat::zeros(img_h, img_w, CV_8U);
    }

    Rect clipped_box = box & Rect(0, 0, img_w, img_h);
    if (clipped_box.width <= 0 || clipped_box.height <= 0) {
        return Mat::zeros(img_h, img_w, CV_8U);
    }

    Mat resized_crop;
    resize(crop, resized_crop, clipped_box.size(), 0.0, 0.0, INTER_LINEAR);

    Mat binary_crop;
    threshold(resized_crop, binary_crop, config_.mask_threshold, 255.0, THRESH_BINARY);
    binary_crop.convertTo(binary_crop, CV_8U);

    Mat full_mask = Mat::zeros(img_h, img_w, CV_8U);
    binary_crop.copyTo(full_mask(clipped_box));
    return full_mask;
}

void YOLOv11_SEG::postprocess(vector<SegDetection>& output, int img_w, int img_h)
{
    det_output_buffer_ = TensorToAttributeMajorVector(infer_request_.get_output_tensor(det_output_index_),
                                                      runtime_.detection_attribute_size,
                                                      runtime_.num_detections);
    mask_output_buffer_ = TensorToFloatVector(infer_request_.get_output_tensor(mask_output_index_));
    if (det_output_buffer_.size() != runtime_.det_output_numel ||
        mask_output_buffer_.size() != runtime_.mask_output_numel) {
        throw runtime_error("SEG output size mismatch");
    }

    output.clear();

    vector<Rect> boxes;
    vector<int> class_ids;
    vector<float> confidences;
    vector<vector<float>> mask_coeffs;

    const Mat det_output(runtime_.detection_attribute_size, runtime_.num_detections, CV_32F, det_output_buffer_.data());
    const float inv_scale = runtime_.scale_ratio > 0.0f ? (1.0f / runtime_.scale_ratio) : 0.0f;

    for (int i = 0; i < det_output.cols; ++i) {
        const Mat classes_scores = det_output.col(i).rowRange(4, 4 + runtime_.num_classes);
        Point class_id_point;
        double score;
        minMaxLoc(classes_scores, nullptr, &score, nullptr, &class_id_point);

        float conf = static_cast<float>(score);

        float cx = det_output.at<float>(0, i);
        float cy = det_output.at<float>(1, i);
        float ow = det_output.at<float>(2, i);
        float oh = det_output.at<float>(3, i);

        if (conf <= config_.conf_threshold) {
            continue;
        }

        float left = (cx - 0.5f * ow - runtime_.pad_x) * inv_scale;
        float top = (cy - 0.5f * oh - runtime_.pad_y) * inv_scale;
        float right = (cx + 0.5f * ow - runtime_.pad_x) * inv_scale;
        float bottom = (cy + 0.5f * oh - runtime_.pad_y) * inv_scale;

        left = clamp(left, 0.0f, static_cast<float>(img_w - 1));
        top = clamp(top, 0.0f, static_cast<float>(img_h - 1));
        right = clamp(right, 0.0f, static_cast<float>(img_w - 1));
        bottom = clamp(bottom, 0.0f, static_cast<float>(img_h - 1));

        Rect box(static_cast<int>(left),
                 static_cast<int>(top),
                 static_cast<int>(right - left),
                 static_cast<int>(bottom - top));

        if (box.width <= 1 || box.height <= 1) {
            continue;
        }

        vector<float> coeff(runtime_.mask_dim);
        for (int j = 0; j < runtime_.mask_dim; ++j) {
            coeff[j] = det_output.at<float>(4 + runtime_.num_classes + j, i);
        }

        boxes.push_back(box);
        class_ids.push_back(class_id_point.y);
        confidences.push_back(conf);
        mask_coeffs.push_back(std::move(coeff));
    }

    vector<int> nms_result;
    dnn::NMSBoxes(boxes, confidences, config_.conf_threshold, config_.nms_threshold, nms_result);
    std::stable_sort(nms_result.begin(), nms_result.end(), [&](int left, int right) {
        return SegCandidateStableLess(left, right, confidences, class_ids, boxes);
    });

    for (int idx : nms_result) {
        SegDetection result;
        result.class_id = class_ids[idx];
        result.conf = confidences[idx];
        result.bbox = boxes[idx];
        result.mask = decodeMask(mask_coeffs[idx], result.bbox, img_w, img_h);
        output.push_back(std::move(result));
    }
}

void YOLOv11_SEG::draw(Mat& image, const vector<SegDetection>& output, const string& output_path)
{
    Mat overlay = image.clone();

    for (const auto& detection : output) {
        const int class_id = detection.class_id;
        const Scalar color = class_id < static_cast<int>(COLORS.size())
            ? Scalar(COLORS[class_id][0], COLORS[class_id][1], COLORS[class_id][2])
            : Scalar(0, 255, 0);

        if (!detection.mask.empty()) {
            overlay.setTo(color, detection.mask);
        }
    }

    addWeighted(overlay, config_.mask_alpha, image, 1.0f - config_.mask_alpha, 0.0, image);

    for (const auto& detection : output) {
        const int class_id = detection.class_id;
        const Scalar color = class_id < static_cast<int>(COLORS.size())
            ? Scalar(COLORS[class_id][0], COLORS[class_id][1], COLORS[class_id][2])
            : Scalar(0, 255, 0);

        rectangle(image, detection.bbox, color, 2);

        const string& class_name =
            (class_id >= 0 && class_id < static_cast<int>(config_.class_names.size()))
                ? config_.class_names[class_id]
                : string("unknown");
        string label = class_name + " " + to_string(detection.conf).substr(0, 4);
        Size text_size = getTextSize(label, FONT_HERSHEY_DUPLEX, 0.8, 1, 0);
        Rect text_rect(detection.bbox.x,
                       max(0, detection.bbox.y - text_size.height - 12),
                       text_size.width + 10,
                       text_size.height + 10);
        rectangle(image, text_rect, color, FILLED);
        putText(image, label,
                Point(text_rect.x + 5, text_rect.y + text_rect.height - 4),
                FONT_HERSHEY_DUPLEX, 0.8, Scalar(0, 0, 0), 1, 0);
    }

    if (!output_path.empty()) {
        imwrite(output_path, image);
        cout << "Seg result saved to " << output_path << endl;
    }
}
