#include <string>

#include "YOLOv11_SEG.h"
#include "cli_inference_runner.h"

int main(int argc, char** argv)
{
    return RunInferenceCli<YOLOv11_SEG, SegDetection>(
        argc,
        argv,
        "[--num-classes=N] [--conf=T] [--nms=T] [--mask-thres=T] [--alpha=T] [--labels=PATH] [--fp16] [--no-warmup]",
        "seg_result_",
        SEGConfig{},
        [](const std::string& arg, SEGConfig& config) {
            if (StartsWith(arg, "--mask-thres=")) {
                config.mask_threshold = std::stof(arg.substr(std::string("--mask-thres=").size()));
                return true;
            }
            if (StartsWith(arg, "--alpha=")) {
                config.mask_alpha = std::stof(arg.substr(std::string("--alpha=").size()));
                return true;
            }
            return false;
        });
}
