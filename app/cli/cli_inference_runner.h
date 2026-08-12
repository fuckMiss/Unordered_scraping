#pragma once

#include "deploy_utils.h"

#include <opencv2/opencv.hpp>

#include <chrono>
#include <iostream>
#include <string>
#include <vector>

template <typename Config>
bool ParseCommonInferenceArg(const std::string& arg, Config& config)
{
    if (StartsWith(arg, "--num-classes=")) {
        config.expected_num_classes = std::stoi(arg.substr(std::string("--num-classes=").size()));
    } else if (StartsWith(arg, "--conf=")) {
        config.conf_threshold = std::stof(arg.substr(std::string("--conf=").size()));
    } else if (StartsWith(arg, "--nms=")) {
        config.nms_threshold = std::stof(arg.substr(std::string("--nms=").size()));
    } else if (StartsWith(arg, "--labels=")) {
        config.class_names = LoadClassNames(arg.substr(std::string("--labels=").size()));
    } else if (arg == "--fp16") {
        config.use_fp16 = true;
    } else if (arg == "--no-warmup") {
        config.enable_warmup = false;
    } else {
        return false;
    }
    return true;
}

inline double InferenceMsSince(const std::chrono::system_clock::time_point& start,
                               const std::chrono::system_clock::time_point& end)
{
    return static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()) / 1000.0;
}

template <typename Model, typename Detection, typename Config, typename ExtraArgParser>
int RunInferenceCli(int argc,
                    char** argv,
                    const std::string& usage_options,
                    const std::string& output_prefix,
                    Config config,
                    ExtraArgParser parse_extra_arg)
{
    try {
        if (argc < 3) {
            std::cerr << "Usage: " << argv[0] << " <openvino_model.xml|onnx_file> <image/video/folder> "
                      << usage_options << std::endl;
            return -1;
        }

        const std::string model_file_path{ argv[1] };
        const std::string path{ argv[2] };
        const bool show_gui = IsGuiAvailable();

        for (int i = 3; i < argc; ++i) {
            const std::string arg = argv[i];
            if (ParseCommonInferenceArg(arg, config) || parse_extra_arg(arg, config)) {
                continue;
            }
            std::cerr << "Unknown argument: " << arg << std::endl;
            return -1;
        }

        std::vector<std::string> image_path_list;
        bool is_video{ false };
        image_path_list = CollectImagePaths(path, is_video);

        Model model(model_file_path, config);

        if (is_video) {
            cv::VideoCapture cap(path);
            if (!cap.isOpened()) {
                std::cerr << "Failed to open video: " << path << std::endl;
                return -1;
            }

            int frame_count = 0;
            while (true) {
                cv::Mat image;
                cap >> image;
                if (image.empty()) {
                    break;
                }

                std::vector<Detection> objects;
                model.preprocess(image);

                const auto start = std::chrono::system_clock::now();
                model.infer();
                const auto end = std::chrono::system_clock::now();

                model.postprocess(objects, image.cols, image.rows);
                model.draw(image, objects, "");
                printf("Frame %d cost %2.4lf ms\n", frame_count++, InferenceMsSince(start, end));

                if (show_gui) {
                    cv::imshow("prediction", image);
                    if (cv::waitKey(1) == 27) {
                        break;
                    }
                }
            }

            if (show_gui) {
                cv::destroyAllWindows();
            }
            cap.release();
            return 0;
        }

        if (image_path_list.empty()) {
            std::cerr << "No images found in: " << path << std::endl;
            return -1;
        }

        for (const auto& image_path : image_path_list) {
            cv::Mat image = cv::imread(image_path);
            if (image.empty()) {
                std::cerr << "Error reading image: " << image_path << std::endl;
                continue;
            }

            std::vector<Detection> objects;
            model.preprocess(image);

            const auto start = std::chrono::system_clock::now();
            model.infer();
            const auto end = std::chrono::system_clock::now();

            model.postprocess(objects, image.cols, image.rows);

            const std::string output_path = output_prefix + image_path.substr(image_path.find_last_of("/\\") + 1);
            model.draw(image, objects, output_path);
            printf("%s cost %2.4lf ms\n", image_path.c_str(), InferenceMsSince(start, end));

            if (show_gui) {
                cv::imshow("Result", image);
                cv::waitKey(0);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return -1;
    }

    return 0;
}
