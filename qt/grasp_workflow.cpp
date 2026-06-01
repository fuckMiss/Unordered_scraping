#include "grasp_workflow.h"

#include <chrono>
#include <exception>

using namespace cv;
using namespace std;

GraspWorkflow::GraspWorkflow() = default;

GraspWorkflow::~GraspWorkflow()
{
    stopCamera();
}

bool GraspWorkflow::loadModels(const string& obb_engine_path,
                               const OBBConfig& obb_config,
                               const string& seg_engine_path,
                               const SEGConfig& seg_config,
                               string* error_message)
{
    lock_guard<mutex> lock(model_mutex_);

    try {
        if (!IsFile(obb_engine_path)) {
            throw runtime_error("OBB engine file does not exist: " + obb_engine_path);
        }
        if (!IsFile(seg_engine_path)) {
            throw runtime_error("SEG engine file does not exist: " + seg_engine_path);
        }

        obb_config_ = obb_config;
        seg_config_ = seg_config;
        obb_engine_path_ = obb_engine_path;
        seg_engine_path_ = seg_engine_path;
        obb_model_ = make_unique<YOLOv11_OBB>(obb_engine_path, logger_, obb_config_);
        seg_model_ = make_unique<YOLOv11_SEG>(seg_engine_path, logger_, seg_config_);
        return true;
    } catch (const exception& e) {
        obb_model_.reset();
        seg_model_.reset();
        if (error_message != nullptr) {
            *error_message = e.what();
        }
        return false;
    }
}

bool GraspWorkflow::areModelsLoaded() const
{
    lock_guard<mutex> lock(model_mutex_);
    return static_cast<bool>(obb_model_) && static_cast<bool>(seg_model_);
}

bool GraspWorkflow::isObbModelLoaded() const
{
    lock_guard<mutex> lock(model_mutex_);
    return static_cast<bool>(obb_model_);
}

bool GraspWorkflow::isSegModelLoaded() const
{
    lock_guard<mutex> lock(model_mutex_);
    return static_cast<bool>(seg_model_);
}

bool GraspWorkflow::runImage(const Mat& image, FrameInferenceResult& result, string* error_message)
{
    return runFrame(image, result, error_message);
}

bool GraspWorkflow::startCamera(function<void(const Mat&, const FrameInferenceResult&)> on_frame,
                                function<void(const string&)> on_error)
{
    if (camera_running_) {
        return true;
    }
    if (camera_thread_.joinable()) {
        camera_thread_.join();
    }
    if (!areModelsLoaded()) {
        if (on_error) {
            on_error("请先同时加载 OBB 和 SEG engine。");
        }
        return false;
    }

    camera_running_ = true;
    camera_thread_ = thread([this, on_frame = std::move(on_frame), on_error = std::move(on_error)]() mutable {
        try {
            VideoCapture cap(0);
            if (!cap.isOpened()) {
                camera_running_ = false;
                if (on_error) {
                    on_error("无法打开默认相机 VideoCapture(0)。");
                }
                return;
            }

            while (camera_running_) {
                Mat frame;
                cap >> frame;
                if (frame.empty()) {
                    continue;
                }

                FrameInferenceResult result;
                string error_message;
                if (!runFrame(frame, result, &error_message)) {
                    camera_running_ = false;
                    if (on_error) {
                        on_error(error_message);
                    }
                    break;
                }

                if (on_frame) {
                    on_frame(frame, result);
                }
            }

            cap.release();
        } catch (const exception& e) {
            camera_running_ = false;
            if (on_error) {
                on_error(e.what());
            }
        }
    });
    return true;
}

void GraspWorkflow::stopCamera()
{
    if (!camera_running_) {
        if (camera_thread_.joinable()) {
            camera_thread_.join();
        }
        return;
    }

    camera_running_ = false;
    if (camera_thread_.joinable()) {
        camera_thread_.join();
    }
}

bool GraspWorkflow::isCameraRunning() const
{
    return camera_running_;
}

bool GraspWorkflow::runFrame(const Mat& image, FrameInferenceResult& result, string* error_message)
{
    lock_guard<mutex> lock(model_mutex_);

    try {
        if (!obb_model_ || !seg_model_) {
            throw runtime_error("OBB and SEG models must both be loaded before inference.");
        }

        Mat input = image.clone();

        vector<OBBDetection> obb_output;
        obb_model_->preprocess(input);
        const auto obb_start = chrono::steady_clock::now();
        obb_model_->infer();
        const auto obb_end = chrono::steady_clock::now();
        obb_model_->postprocess(obb_output, input.cols, input.rows);

        vector<SegDetection> seg_output;
        seg_model_->preprocess(input);
        const auto seg_start = chrono::steady_clock::now();
        seg_model_->infer();
        const auto seg_end = chrono::steady_clock::now();
        seg_model_->postprocess(seg_output, input.cols, input.rows);

        const double obb_inference_ms =
            static_cast<double>(chrono::duration_cast<chrono::microseconds>(obb_end - obb_start).count()) / 1000.0;
        const double seg_inference_ms =
            static_cast<double>(chrono::duration_cast<chrono::microseconds>(seg_end - seg_start).count()) / 1000.0;

        result = BuildFrameInferenceResult(obb_output,
                                           obb_model_->getClassNames(),
                                           seg_output,
                                           seg_model_->getClassNames(),
                                           input.cols,
                                           input.rows,
                                           obb_inference_ms,
                                           seg_inference_ms);
        return true;
    } catch (const exception& e) {
        if (error_message != nullptr) {
            *error_message = e.what();
        }
        return false;
    }
}
