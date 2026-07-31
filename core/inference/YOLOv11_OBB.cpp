#include "YOLOv11_OBB.h"

#include "model_utils.h"
#include "openvino_utils.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>

static const std::vector<std::string> OBB_CLASS_NAMES = {
    "left", "big", "small"
};

static std::vector<std::string> buildDefaultClassNames(int num_classes) {
    return BuildDefaultClassNames(OBB_CLASS_NAMES, num_classes);
}

static float normalizeAnglePi(float angle) {
    float normalized = std::fmod(angle, static_cast<float>(CV_PI));
    if (normalized < 0.0f) {
        normalized += static_cast<float>(CV_PI);
    }
    return normalized;
}

static std::vector<Point2f> xywhrToCorners(float cx, float cy, float w, float h, float angle) {
    const float cos_a = std::cos(angle);
    const float sin_a = std::sin(angle);
    const Point2f vx((w * 0.5f) * cos_a, (w * 0.5f) * sin_a);
    const Point2f vy(-(h * 0.5f) * sin_a, (h * 0.5f) * cos_a);

    return {
        Point2f(cx, cy) + vx + vy,
        Point2f(cx, cy) + vx - vy,
        Point2f(cx, cy) - vx - vy,
        Point2f(cx, cy) - vx + vy
    };
}

static float polygonArea(const std::vector<Point2f>& polygon) {
    if (polygon.size() < 3) {
        return 0.0f;
    }
    return std::fabs(static_cast<float>(cv::contourArea(polygon)));
}

YOLOv11_OBB::YOLOv11_OBB(string model_path, const OBBConfig& config)
{
    config_ = config;
    init(model_path);
}

void YOLOv11_OBB::init(const string& model_path)
{
    compiled_model_ = CompileModelWithGpuFallback(core_, model_path, &actual_device_);
    infer_request_ = compiled_model_.create_infer_request();
    initializeModelState();
}

void YOLOv11_OBB::initializeModelState()
{
    if (compiled_model_.inputs().size() != 1 || compiled_model_.outputs().size() != 1) {
        throw runtime_error("OBB model must expose exactly 1 input tensor and 1 output tensor");
    }

    input_port_ = compiled_model_.input();
    output_port_ = compiled_model_.output();
    const ov::Shape input_shape = input_port_.get_shape();
    const ov::Shape output_shape = output_port_.get_shape();

    ValidateBatchOneNchwInputShape(input_shape, "OBB input");
    ValidateBatchOneTensorShape(output_shape, 3, "OBB output");

    runtime_.input_h = static_cast<int>(input_shape[2]);
    runtime_.input_w = static_cast<int>(input_shape[3]);

    const int candidate_a = static_cast<int>(output_shape[1]);
    const int candidate_b = static_cast<int>(output_shape[2]);
    if (candidate_a > 5 && candidate_b > 5) {
        if (candidate_a < candidate_b) {
            runtime_.detection_attribute_size = candidate_a;
            runtime_.num_detections = candidate_b;
        } else {
            runtime_.detection_attribute_size = candidate_b;
            runtime_.num_detections = candidate_a;
        }
    } else if (candidate_a > 5) {
        runtime_.detection_attribute_size = candidate_a;
        runtime_.num_detections = candidate_b;
    } else if (candidate_b > 5) {
        runtime_.detection_attribute_size = candidate_b;
        runtime_.num_detections = candidate_a;
    } else {
        throw runtime_error("Invalid OBB output layout: " + ShapeToString(output_shape));
    }

    runtime_.num_classes = runtime_.detection_attribute_size - 5;
    if (runtime_.num_classes <= 0) {
        throw runtime_error("Invalid OBB output layout: detection_attribute_size must be >= 6");
    }
    if (config_.expected_num_classes > 0 && config_.expected_num_classes != runtime_.num_classes) {
        ostringstream oss;
        oss << "Configured num_classes (" << config_.expected_num_classes
            << ") does not match model output (" << runtime_.num_classes << ")";
        throw runtime_error(oss.str());
    }
    if (config_.class_names.empty()) {
        config_.class_names = buildDefaultClassNames(runtime_.num_classes);
    } else if (static_cast<int>(config_.class_names.size()) != runtime_.num_classes) {
        ostringstream oss;
        oss << "Configured class_names size (" << config_.class_names.size()
            << ") does not match num_classes (" << runtime_.num_classes << ")";
        throw runtime_error(oss.str());
    }

    runtime_.output_numel = static_cast<size_t>(runtime_.detection_attribute_size) * runtime_.num_detections;
    input_buffer_.assign(static_cast<size_t>(3) * runtime_.input_w * runtime_.input_h, 0.0f);
    output_buffer_.assign(runtime_.output_numel, 0.0f);

    cout << "YOLOv11_OBB OpenVINO Model Info:" << endl;
    cout << "  Device: " << actual_device_ << endl;
    cout << "  Input: " << runtime_.input_w << "x" << runtime_.input_h << endl;
    cout << "  Output: " << runtime_.detection_attribute_size << " x " << runtime_.num_detections << endl;
    cout << "  Num classes: " << runtime_.num_classes << endl;

    if (config_.enable_warmup) {
        ov::Tensor input_tensor(ov::element::f32, input_port_.get_shape(), input_buffer_.data());
        infer_request_.set_input_tensor(input_tensor);
        for (int i = 0; i < 5; i++) {
            infer_request_.infer();
        }
        cout << "model warmup 5 times" << endl;
    }
}

void YOLOv11_OBB::preprocess(Mat& image)
{
    input_buffer_ = BuildNchwLetterboxInput(image,
                                            runtime_.input_w,
                                            runtime_.input_h,
                                            &runtime_.scale_ratio,
                                            &runtime_.pad_x,
                                            &runtime_.pad_y);
    ov::Tensor input_tensor(ov::element::f32, input_port_.get_shape(), input_buffer_.data());
    infer_request_.set_input_tensor(input_tensor);
}

void YOLOv11_OBB::infer()
{
    infer_request_.infer();
}

void YOLOv11_OBB::postprocess(vector<OBBDetection>& output, int img_w, int img_h)
{
    output_buffer_ = TensorToAttributeMajorVector(infer_request_.get_output_tensor(0),
                                                  runtime_.detection_attribute_size,
                                                  runtime_.num_detections);
    if (output_buffer_.size() != runtime_.output_numel) {
        throw runtime_error("OBB output size mismatch");
    }

    output.clear();

    const Mat det_output(runtime_.detection_attribute_size, runtime_.num_detections, CV_32F, output_buffer_.data());
    const float inv_scale = runtime_.scale_ratio > 0.0f ? (1.0f / runtime_.scale_ratio) : 0.0f;

    for (int i = 0; i < det_output.cols; ++i) {
        const int angle_channel = 4 + runtime_.num_classes;
        const Mat classes_scores = det_output.col(i).rowRange(4, 4 + runtime_.num_classes);
        Point class_id_point;
        double score;
        minMaxLoc(classes_scores, nullptr, &score, nullptr, &class_id_point);

        if (score > config_.conf_threshold) {
            float cx = det_output.at<float>(0, i);
            float cy = det_output.at<float>(1, i);
            float ow = det_output.at<float>(2, i);
            float oh = det_output.at<float>(3, i);
            float angle = det_output.at<float>(angle_channel, i);

            cx = (cx - runtime_.pad_x) * inv_scale;
            cy = (cy - runtime_.pad_y) * inv_scale;
            ow *= inv_scale;
            oh *= inv_scale;

            if (ow <= 1.0f || oh <= 1.0f) {
                continue;
            }

            angle = normalizeAnglePi(angle);
            if (ow < oh) {
                std::swap(ow, oh);
                angle = normalizeAnglePi(angle + CV_PI * 0.5f);
            }

            std::vector<Point2f> corners = xywhrToCorners(cx, cy, ow, oh, angle);
            for (Point2f& point : corners) {
                point.x = std::clamp(point.x, 0.0f, static_cast<float>(img_w - 1));
                point.y = std::clamp(point.y, 0.0f, static_cast<float>(img_h - 1));
            }

            OBBDetection det;
            det.conf = static_cast<float>(score);
            det.class_id = class_id_point.y;
            det.corners = corners;
            det.rotated_rect = RotatedRect(
                Point2f(std::clamp(cx, 0.0f, static_cast<float>(img_w - 1)),
                        std::clamp(cy, 0.0f, static_cast<float>(img_h - 1))),
                Size2f(ow, oh),
                angle * 180.0f / static_cast<float>(CV_PI));
            output.push_back(std::move(det));
        }
    }

    nmsRotated(output, config_.nms_threshold);
}

float YOLOv11_OBB::computeRotatedIoU(const OBBDetection& det1, const OBBDetection& det2) const
{
    std::vector<Point2f> intersection;
    const float inter_area = cv::intersectConvexConvex(det1.corners, det2.corners, intersection, true);
    if (inter_area <= 1e-6f) {
        return 0.0f;
    }

    const float area1 = polygonArea(det1.corners);
    const float area2 = polygonArea(det2.corners);
    float union_area = area1 + area2 - inter_area;

    if (union_area < 1e-6f) {
        return 0.0f;
    }

    return inter_area / union_area;
}

void YOLOv11_OBB::nmsRotated(vector<OBBDetection>& detections, float nms_threshold) const
{
    if (detections.empty()) {
        return;
    }

    std::stable_sort(detections.begin(), detections.end(), [](const OBBDetection& a, const OBBDetection& b) {
        return a.conf > b.conf;
    });

    std::vector<OBBDetection> result;
    for (int class_id = 0; class_id < runtime_.num_classes; ++class_id) {
        std::vector<bool> suppressed(detections.size(), false);
        for (size_t i = 0; i < detections.size(); ++i) {
            if (suppressed[i] || detections[i].class_id != class_id) {
                continue;
            }
            result.push_back(detections[i]);
            for (size_t j = i + 1; j < detections.size(); ++j) {
                if (suppressed[j] || detections[j].class_id != class_id) {
                    continue;
                }
                if (computeRotatedIoU(detections[i], detections[j]) > nms_threshold) {
                    suppressed[j] = true;
                }
            }
        }
    }

    std::stable_sort(result.begin(), result.end(), [](const OBBDetection& a, const OBBDetection& b) {
        return a.conf > b.conf;
    });
    detections = result;
}

void YOLOv11_OBB::draw(Mat& image, const vector<OBBDetection>& output, const string& output_path)
{
    Mat display = image.clone();

    for (size_t i = 0; i < output.size(); i++)
    {
        auto detection = output[i];
        auto rrect = detection.rotated_rect;
        int class_id = detection.class_id;
        float conf = detection.conf;

        const string class_name = GetClassName(config_.class_names, class_id);
        const cv::Scalar color = GetClassColor(class_id);

        Point2f corners[4];
        if (detection.corners.size() == 4) {
            for (int j = 0; j < 4; ++j) corners[j] = detection.corners[j];
        } else {
            rrect.points(corners);
        }

        for (int j = 0; j < 4; j++) {
            line(display, corners[j], corners[(j + 1) % 4], color, 2);
        }

        string label = class_name + " " + to_string(conf).substr(0, 4);
        int baseLine = 0;
        Size labelSize = getTextSize(label, FONT_HERSHEY_SIMPLEX, 0.6, 1, &baseLine);

        Point labelOrigin(static_cast<int>(corners[0].x), static_cast<int>(corners[0].y) - 5);
        if (labelOrigin.y < labelSize.height + 5) {
            labelOrigin.y = static_cast<int>(corners[0].y) + labelSize.height + 10;
        }
        if (labelOrigin.x < 0) {
            labelOrigin.x = 0;
        }
        if (labelOrigin.x + labelSize.width > display.cols) {
            labelOrigin.x = std::max(0, display.cols - labelSize.width);
        }
        if (labelOrigin.y >= display.rows) {
            labelOrigin.y = std::max(labelSize.height + 5, display.rows - 1);
        }

        rectangle(display,
                  Point(labelOrigin.x, labelOrigin.y - labelSize.height - 5),
                  Point(labelOrigin.x + labelSize.width, labelOrigin.y + baseLine),
                  color, -1);
        putText(display, label, Point(labelOrigin.x, labelOrigin.y),
                FONT_HERSHEY_SIMPLEX, 0.6, Scalar(255, 255, 255), 1);
    }

    image = display;
    if (!output_path.empty()) {
        imwrite(output_path, display);
        cout << "Saved result to: " << output_path << endl;
    }
}
