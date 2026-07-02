#include "openvino_utils.h"

#include "model_utils.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace cv;
using namespace std;

string ShapeToString(const ov::Shape& shape)
{
    string result = "[";
    for (size_t i = 0; i < shape.size(); ++i) {
        if (i > 0) {
            result += ", ";
        }
        result += to_string(shape[i]);
    }
    result += "]";
    return result;
}

void ValidateBatchOneTensorShape(const ov::Shape& shape,
                                 size_t expected_rank,
                                 const string& tensor_name)
{
    if (shape.size() != expected_rank) {
        throw runtime_error("Unsupported " + tensor_name + " tensor rank: expected " +
                            to_string(expected_rank) + ", got " + to_string(shape.size()) +
                            " " + ShapeToString(shape));
    }
    if (shape.empty() || shape[0] != 1) {
        throw runtime_error("Unsupported " + tensor_name + " batch size: expected 1, got " +
                            (shape.empty() ? string("empty") : to_string(shape[0])) +
                            " " + ShapeToString(shape));
    }
    for (size_t i = 0; i < shape.size(); ++i) {
        if (shape[i] == 0) {
            throw runtime_error("Invalid " + tensor_name + " tensor dimension at axis " +
                                to_string(i) + ": " + ShapeToString(shape));
        }
    }
}

void ValidateBatchOneNchwInputShape(const ov::Shape& shape, const string& tensor_name)
{
    ValidateBatchOneTensorShape(shape, 4, tensor_name);
    if (shape[1] != 3) {
        throw runtime_error("Unsupported " + tensor_name +
                            " channel count/layout: expected [1, 3, H, W], got " +
                            ShapeToString(shape));
    }
}

ov::CompiledModel CompileModelWithGpuFallback(ov::Core& core,
                                              const string& model_path,
                                              string* actual_device)
{
    shared_ptr<ov::Model> model = core.read_model(model_path);
    string requested_device = "AUTO";
    char* requested_device_env = nullptr;
    size_t requested_device_env_size = 0;
    if (_dupenv_s(&requested_device_env, &requested_device_env_size, "TANKEYE_OPENVINO_DEVICE") == 0 &&
        requested_device_env != nullptr &&
        requested_device_env[0] != '\0') {
        requested_device = requested_device_env;
        transform(requested_device.begin(), requested_device.end(), requested_device.begin(), ::toupper);
    }
    free(requested_device_env);

    cout << "[OpenVINO] Loading model: " << model_path << endl;
    cout << "[OpenVINO] Requested device: " << requested_device << endl;
    try {
        vector<string> devices = core.get_available_devices();
        cout << "[OpenVINO] Available devices:";
        if (devices.empty()) {
            cout << " none";
        }
        for (const string& device : devices) {
            cout << " " << device;
        }
        cout << endl;
    } catch (const exception& device_error) {
        cerr << "[OpenVINO] Failed to query available devices: " << device_error.what() << endl;
    }

    if (requested_device == "CPU") {
        cout << "[OpenVINO] Trying device: CPU" << endl;
        ov::CompiledModel compiled = core.compile_model(model, "CPU");
        if (actual_device != nullptr) {
            *actual_device = "CPU";
        }
        cout << "[OpenVINO] Selected device: CPU" << endl;
        return compiled;
    }

    if (requested_device == "GPU") {
        cout << "[OpenVINO] Trying device: GPU" << endl;
        ov::CompiledModel compiled = core.compile_model(model, "GPU");
        if (actual_device != nullptr) {
            *actual_device = "GPU";
        }
        cout << "[OpenVINO] Selected device: GPU" << endl;
        return compiled;
    }

    try {
        cout << "[OpenVINO] Trying device: GPU" << endl;
        ov::CompiledModel compiled = core.compile_model(model, "GPU");
        if (actual_device != nullptr) {
            *actual_device = "GPU";
        }
        cout << "[OpenVINO] Selected device: GPU" << endl;
        return compiled;
    } catch (const exception& gpu_error) {
        cerr << "[OpenVINO] GPU compile failed, falling back to CPU: " << gpu_error.what() << endl;
        ov::CompiledModel compiled = core.compile_model(model, "CPU");
        if (actual_device != nullptr) {
            *actual_device = "CPU";
        }
        cout << "[OpenVINO] Selected device: CPU" << endl;
        return compiled;
    }
}

vector<float> BuildNchwLetterboxInput(Mat& image,
                                      int input_w,
                                      int input_h,
                                      float* scale_ratio,
                                      float* pad_x,
                                      float* pad_y)
{
    ValidateInputImage(image, 4096 * 4096);

    const float scale = min(input_h / static_cast<float>(image.rows),
                            input_w / static_cast<float>(image.cols));
    const float pad_left = (input_w - scale * image.cols) * 0.5f;
    const float pad_top = (input_h - scale * image.rows) * 0.5f;

    if (scale_ratio != nullptr) {
        *scale_ratio = scale;
    }
    if (pad_x != nullptr) {
        *pad_x = pad_left;
    }
    if (pad_y != nullptr) {
        *pad_y = pad_top;
    }

    const int resized_w = max(1, min(input_w, static_cast<int>(round(image.cols * scale))));
    const int resized_h = max(1, min(input_h, static_cast<int>(round(image.rows * scale))));
    const int left = max(0, min(input_w - resized_w, static_cast<int>(round(pad_left))));
    const int top = max(0, min(input_h - resized_h, static_cast<int>(round(pad_top))));

    Mat canvas(input_h, input_w, CV_8UC3, Scalar(128, 128, 128));
    Mat resized;
    resize(image, resized, Size(resized_w, resized_h), 0.0, 0.0, INTER_LINEAR);
    resized.copyTo(canvas(Rect(left, top, resized_w, resized_h)));

    Mat rgb;
    cvtColor(canvas, rgb, COLOR_BGR2RGB);

    vector<float> input(static_cast<size_t>(3) * input_w * input_h);
    const int area = input_w * input_h;
    for (int y = 0; y < input_h; ++y) {
        const Vec3b* row = rgb.ptr<Vec3b>(y);
        for (int x = 0; x < input_w; ++x) {
            const int index = y * input_w + x;
            input[index] = row[x][0] / 255.0f;
            input[area + index] = row[x][1] / 255.0f;
            input[2 * area + index] = row[x][2] / 255.0f;
        }
    }
    return input;
}

vector<float> TensorToFloatVector(const ov::Tensor& tensor)
{
    const size_t count = tensor.get_size();
    vector<float> values(count);
    const ov::element::Type type = tensor.get_element_type();
    if (type == ov::element::f32) {
        const float* data = tensor.data<const float>();
        copy(data, data + count, values.begin());
        return values;
    }
    if (type == ov::element::f16) {
        const ov::float16* data = tensor.data<const ov::float16>();
        for (size_t i = 0; i < count; ++i) {
            values[i] = static_cast<float>(data[i]);
        }
        return values;
    }
    throw runtime_error("Unsupported OpenVINO output tensor type: " + type.get_type_name());
}

vector<float> TensorToAttributeMajorVector(const ov::Tensor& tensor,
                                           int detection_attribute_size,
                                           int num_detections)
{
    vector<float> raw = TensorToFloatVector(tensor);
    const ov::Shape shape = tensor.get_shape();
    const size_t expected = static_cast<size_t>(detection_attribute_size) * num_detections;
    if (raw.size() != expected || shape.size() != 3) {
        throw runtime_error("Unexpected detection tensor shape: " + ShapeToString(shape));
    }

    if (static_cast<int>(shape[1]) == detection_attribute_size &&
        static_cast<int>(shape[2]) == num_detections) {
        return raw;
    }

    if (static_cast<int>(shape[1]) == num_detections &&
        static_cast<int>(shape[2]) == detection_attribute_size) {
        vector<float> transposed(expected);
        for (int det = 0; det < num_detections; ++det) {
            for (int attr = 0; attr < detection_attribute_size; ++attr) {
                transposed[static_cast<size_t>(attr) * num_detections + det] =
                    raw[static_cast<size_t>(det) * detection_attribute_size + attr];
            }
        }
        return transposed;
    }

    throw runtime_error("Detection tensor shape does not match parsed layout: " + ShapeToString(shape));
}
