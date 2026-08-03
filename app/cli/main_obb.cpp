#include <string>

#include "YOLOv11_OBB.h"
#include "cli_inference_runner.h"

int main(int argc, char** argv)
{
    return RunInferenceCli<YOLOv11_OBB, OBBDetection>(
        argc,
        argv,
        "[--num-classes=N] [--conf=T] [--nms=T] [--labels=PATH] [--fp16] [--no-warmup]",
        "obb_result_",
        OBBConfig{},
        [](const std::string&, OBBConfig&) { return false; });
}
