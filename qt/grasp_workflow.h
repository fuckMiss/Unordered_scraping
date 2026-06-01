#pragma once

#include "YOLOv11_OBB.h"
#include "YOLOv11_SEG.h"
#include "deploy_utils.h"
#include "frame_postprocess.h"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

class GraspWorkflow
{
public:
    GraspWorkflow();
    ~GraspWorkflow();

    bool loadModels(const std::string& obb_engine_path,
                    const OBBConfig& obb_config,
                    const std::string& seg_engine_path,
                    const SEGConfig& seg_config,
                    std::string* error_message);
    bool areModelsLoaded() const;
    bool isObbModelLoaded() const;
    bool isSegModelLoaded() const;
    bool runImage(const cv::Mat& image, FrameInferenceResult& result, std::string* error_message);

    bool startCamera(std::function<void(const cv::Mat&, const FrameInferenceResult&)> on_frame,
                     std::function<void(const std::string&)> on_error);
    void stopCamera();
    bool isCameraRunning() const;

private:
    bool runFrame(const cv::Mat& image, FrameInferenceResult& result, std::string* error_message);

    mutable std::mutex model_mutex_;
    std::unique_ptr<YOLOv11_OBB> obb_model_;
    std::unique_ptr<YOLOv11_SEG> seg_model_;
    OBBConfig obb_config_;
    SEGConfig seg_config_;
    TrtLogger logger_;
    std::string obb_engine_path_;
    std::string seg_engine_path_;

    std::atomic<bool> camera_running_{ false };
    std::thread camera_thread_;
};
