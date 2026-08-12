#include "grasp_workflow.h"

#include "hik_camera.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cctype>
#include <exception>
#include <iostream>

using namespace cv;
using namespace std;

namespace {

double MsSince(const chrono::steady_clock::time_point& start,
               const chrono::steady_clock::time_point& end)
{
    return static_cast<double>(chrono::duration_cast<chrono::microseconds>(end - start).count()) / 1000.0;
}

bool CameraFallbackEnabled()
{
    char* raw_value = nullptr;
    size_t value_size = 0;
    string text;
    if (_dupenv_s(&raw_value, &value_size, "TANKEYE_CAMERA_FALLBACK") != 0 || raw_value == nullptr) {
        return false;
    }
    text = raw_value;
    free(raw_value);
    transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
        return static_cast<char>(tolower(ch));
    });
    return text == "1" || text == "true" || text == "yes" || text == "on";
}

class CameraFrameSource
{
public:
    bool open(const string& camera_ip, double exposure_us, const string& log_prefix, string* error_message)
    {
        string camera_error;
        use_hik_camera_ = hik_camera_.open(camera_ip, &camera_error, exposure_us);
        if (use_hik_camera_) {
            cout << log_prefix << " source=Hikrobot" << endl;
            return true;
        }

        cout << log_prefix << " Hikrobot unavailable: " << camera_error << endl;
        if (!CameraFallbackEnabled()) {
            if (error_message != nullptr) {
                *error_message = "Hikrobot camera could not be opened: " + camera_error;
            }
            return false;
        }

        cout << log_prefix << " TANKEYE_CAMERA_FALLBACK=1, falling back to OpenCV camera 0" << endl;
        fallback_camera_.open(0);
        if (!fallback_camera_.isOpened()) {
            if (error_message != nullptr) {
                *error_message = "No Hikrobot camera found and OpenCV camera 0 could not be opened. Hikrobot error: " + camera_error;
            }
            return false;
        }

        fallback_camera_.set(CAP_PROP_FRAME_WIDTH, 1280);
        fallback_camera_.set(CAP_PROP_FRAME_HEIGHT, 720);
        cout << log_prefix << " source=OpenCV camera 0" << endl;
        return true;
    }

    bool grab(Mat& frame, string* error_message)
    {
        if (use_hik_camera_) {
            return hik_camera_.grab(frame, error_message);
        }
        if (!fallback_camera_.read(frame) || frame.empty()) {
            if (error_message != nullptr) {
                *error_message = "OpenCV camera 0 returned an empty frame.";
            }
            return false;
        }
        return true;
    }

    void close()
    {
        if (use_hik_camera_) {
            hik_camera_.close();
        } else {
            fallback_camera_.release();
        }
    }

private:
    HikCamera hik_camera_;
    VideoCapture fallback_camera_;
    bool use_hik_camera_ = false;
};

} // namespace

GraspWorkflow::GraspWorkflow() = default;

GraspWorkflow::~GraspWorkflow()
{
    stopCamera();
}

bool GraspWorkflow::loadModels(const string& obb_model_path,
                               const OBBConfig& obb_config,
                               const string& seg_model_path,
                               const SEGConfig& seg_config,
                               string* error_message)
{
    lock_guard<mutex> lock(model_mutex_);

    try {
        if (!IsFile(obb_model_path)) {
            throw runtime_error("OBB model file does not exist: " + obb_model_path);
        }
        if (!IsFile(seg_model_path)) {
            throw runtime_error("SEG model file does not exist: " + seg_model_path);
        }

        auto next_obb_model = make_unique<YOLOv11_OBB>(obb_model_path, obb_config);
        auto next_seg_model = make_unique<YOLOv11_SEG>(seg_model_path, seg_config);
        obb_model_ = std::move(next_obb_model);
        seg_model_ = std::move(next_seg_model);
        obb_config_ = obb_config;
        seg_config_ = seg_config;
        obb_model_path_ = obb_model_path;
        seg_model_path_ = seg_model_path;
        return true;
    } catch (const exception& e) {
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

string GraspWorkflow::runtimeDeviceSummary() const
{
    lock_guard<mutex> lock(model_mutex_);
    if (!obb_model_ && !seg_model_) {
        return "not loaded";
    }

    string summary;
    if (obb_model_) {
        summary += "OBB " + obb_model_->actualDevice();
    } else {
        summary += "OBB not loaded";
    }
    summary += " / ";
    if (seg_model_) {
        summary += "SEG " + seg_model_->actualDevice();
    } else {
        summary += "SEG not loaded";
    }
    return summary;
}

bool GraspWorkflow::runImage(const Mat& image, FrameInferenceResult& result, string* error_message)
{
    return runFrame(image, result, error_message);
}

void GraspWorkflow::setCenterRayOffsetPx(double offset_px)
{
    lock_guard<mutex> lock(model_mutex_);
    postprocess_config_.center_ray_offset_px = offset_px;
}

double GraspWorkflow::centerRayOffsetPx() const
{
    lock_guard<mutex> lock(model_mutex_);
    return postprocess_config_.center_ray_offset_px;
}

void GraspWorkflow::setPostprocessDebugLoggingEnabled(bool enabled)
{
    lock_guard<mutex> lock(model_mutex_);
    postprocess_config_.debug_logging_enabled = enabled;
}

bool GraspWorkflow::postprocessDebugLoggingEnabled() const
{
    lock_guard<mutex> lock(model_mutex_);
    return postprocess_config_.debug_logging_enabled;
}

void GraspWorkflow::setCameraIp(const string& camera_ip)
{
    lock_guard<mutex> lock(camera_config_mutex_);
    camera_ip_ = camera_ip;
}

string GraspWorkflow::cameraIp() const
{
    lock_guard<mutex> lock(camera_config_mutex_);
    return camera_ip_;
}

void GraspWorkflow::setCameraExposureUs(double exposure_us)
{
    lock_guard<mutex> lock(camera_config_mutex_);
    camera_exposure_us_ = exposure_us > 0.0 ? exposure_us : 0.0;
}

double GraspWorkflow::cameraExposureUs() const
{
    lock_guard<mutex> lock(camera_config_mutex_);
    return camera_exposure_us_;
}

bool GraspWorkflow::autoExposeOnce(double* exposure_us, string* error_message)
{
    const string camera_ip = cameraIp();

    HikCamera camera;
    string open_error;
    if (!camera.open(camera_ip, &open_error, 0.0)) {
        if (error_message != nullptr) {
            *error_message = "Hikrobot camera could not be opened for auto exposure: " + open_error;
        }
        return false;
    }

    double measured_exposure = 0.0;
    string exposure_error;
    const bool ok = camera.autoExposureOnce(&measured_exposure, &exposure_error);
    camera.close();
    if (!ok) {
        if (error_message != nullptr) {
            *error_message = exposure_error;
        }
        return false;
    }

    setCameraExposureUs(measured_exposure);
    if (exposure_us != nullptr) {
        *exposure_us = measured_exposure;
    }
    return true;
}

bool GraspWorkflow::startFrameCapture(function<void(const Mat&)> on_frame,
                                      function<void(const string&)> on_error)
{
    if (camera_running_) {
        return true;
    }
    if (camera_thread_.joinable()) {
        camera_thread_.join();
    }

    const string camera_ip = cameraIp();
    const double exposure_us = cameraExposureUs();
    camera_running_ = true;
    camera_thread_ = thread([this, camera_ip, exposure_us, on_frame = std::move(on_frame), on_error = std::move(on_error)]() mutable {
        try {
            CameraFrameSource frame_source;
            string open_error;
            if (!frame_source.open(camera_ip, exposure_us, "[CameraCapture]", &open_error)) {
                camera_running_ = false;
                if (on_error) {
                    on_error(open_error);
                }
                return;
            }

            int frame_index = 0;
            while (camera_running_) {
                const auto grab_start = chrono::steady_clock::now();
                Mat frame;
                string grab_error;
                if (!frame_source.grab(frame, &grab_error)) {
                    camera_running_ = false;
                    if (on_error) {
                        on_error(grab_error);
                    }
                    break;
                }
                const auto grab_end = chrono::steady_clock::now();

                ++frame_index;
                if (frame_index == 1 || frame_index % 120 == 0) {
                    cout << "[CameraCapturePerf] frame=" << frame_index
                         << " grab_ms=" << MsSince(grab_start, grab_end)
                         << " image=" << frame.cols << "x" << frame.rows
                         << endl;
                }

                if (on_frame) {
                    on_frame(frame);
                }
            }

            frame_source.close();
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
        const auto obb_pre_start = chrono::steady_clock::now();
        obb_model_->preprocess(input);
        const auto obb_pre_end = chrono::steady_clock::now();
        const auto obb_start = chrono::steady_clock::now();
        obb_model_->infer();
        const auto obb_end = chrono::steady_clock::now();
        const auto obb_post_start = chrono::steady_clock::now();
        obb_model_->postprocess(obb_output, input.cols, input.rows);
        const auto obb_post_end = chrono::steady_clock::now();

        vector<SegDetection> seg_output;
        const auto seg_pre_start = chrono::steady_clock::now();
        seg_model_->preprocess(input);
        const auto seg_pre_end = chrono::steady_clock::now();
        const auto seg_start = chrono::steady_clock::now();
        seg_model_->infer();
        const auto seg_end = chrono::steady_clock::now();
        const auto seg_post_start = chrono::steady_clock::now();
        seg_model_->postprocess(seg_output, input.cols, input.rows);
        const auto seg_post_end = chrono::steady_clock::now();

        const double obb_pre_ms = MsSince(obb_pre_start, obb_pre_end);
        const double obb_inference_ms = MsSince(obb_start, obb_end);
        const double obb_post_ms = MsSince(obb_post_start, obb_post_end);
        const double seg_pre_ms = MsSince(seg_pre_start, seg_pre_end);
        const double seg_inference_ms = MsSince(seg_start, seg_end);
        const double seg_post_ms = MsSince(seg_post_start, seg_post_end);

        const auto fuse_start = chrono::steady_clock::now();
        result = BuildFrameInferenceResult(obb_output,
                                           obb_model_->getClassNames(),
                                           seg_output,
                                           seg_model_->getClassNames(),
                                           input.cols,
                                           input.rows,
                                           obb_inference_ms,
                                           seg_inference_ms,
                                           postprocess_config_);
        const auto fuse_end = chrono::steady_clock::now();
        const double total_process_ms = MsSince(obb_pre_start, fuse_end);
        result.total_inference_ms = total_process_ms;

        static int profile_frame_index = 0;
        ++profile_frame_index;
        if (profile_frame_index == 1 || profile_frame_index % 30 == 0) {
            cout << "[DetectPerf] frame=" << profile_frame_index
                 << " obb_pre_ms=" << obb_pre_ms
                 << " obb_infer_ms=" << obb_inference_ms
                 << " obb_post_ms=" << obb_post_ms
                 << " seg_pre_ms=" << seg_pre_ms
                 << " seg_infer_ms=" << seg_inference_ms
                 << " seg_post_ms=" << seg_post_ms
                 << " fuse_ms=" << MsSince(fuse_start, fuse_end)
                 << " total_process_ms=" << result.total_inference_ms
                 << " obb_count=" << obb_output.size()
                 << " seg_count=" << seg_output.size()
                 << endl;
        }
        return true;
    } catch (const exception& e) {
        if (error_message != nullptr) {
            *error_message = e.what();
        }
        return false;
    }
}
