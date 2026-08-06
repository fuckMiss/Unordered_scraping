#include <openvino/openvino.hpp>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

std::string ToUpper(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    return value;
}

double MsSince(const std::chrono::steady_clock::time_point& start,
               const std::chrono::steady_clock::time_point& end)
{
    return static_cast<double>(
        std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()) / 1000.0;
}

bool CompileOne(ov::Core& core, const std::string& model_path, const std::string& device)
{
    std::cout << "[DeviceProbe] Loading model: " << model_path << std::endl;
    const auto read_start = std::chrono::steady_clock::now();
    std::shared_ptr<ov::Model> model = core.read_model(model_path);
    const auto read_end = std::chrono::steady_clock::now();
    std::cout << "[DeviceProbe] Read model ms: " << MsSince(read_start, read_end) << std::endl;

    std::cout << "[DeviceProbe] Trying device: " << device << std::endl;
    const auto compile_start = std::chrono::steady_clock::now();
    ov::CompiledModel compiled = core.compile_model(model, device);
    const auto compile_end = std::chrono::steady_clock::now();
    std::cout << "[DeviceProbe] Selected device: " << device << std::endl;
    std::cout << "[DeviceProbe] Compile model ms: " << MsSince(compile_start, compile_end) << std::endl;
    return static_cast<bool>(compiled);
}

} // namespace

int main(int argc, char** argv)
{
    try {
        if (argc < 3) {
            std::cerr << "Usage: " << argv[0]
                      << " <obb_model.xml> <seg_model.xml> [--device=GPU] [--cache-dir=PATH]"
                      << std::endl;
            return 2;
        }

        const std::string obb_model = argv[1];
        const std::string seg_model = argv[2];
        std::string device = "GPU";
        std::string cache_dir;

        for (int i = 3; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg.rfind("--device=", 0) == 0) {
                device = ToUpper(arg.substr(std::string("--device=").size()));
            } else if (arg.rfind("--cache-dir=", 0) == 0) {
                cache_dir = arg.substr(std::string("--cache-dir=").size());
            } else {
                std::cerr << "Unknown argument: " << arg << std::endl;
                return 2;
            }
        }

        ov::Core core;
        if (!cache_dir.empty()) {
            core.set_property(ov::cache_dir(cache_dir));
            std::cout << "[DeviceProbe] Cache dir: " << cache_dir << std::endl;
        }

        std::cout << "[DeviceProbe] Requested device: " << device << std::endl;
        try {
            const std::vector<std::string> devices = core.get_available_devices();
            std::cout << "[DeviceProbe] Available devices:";
            if (devices.empty()) {
                std::cout << " none";
            }
            for (const std::string& available_device : devices) {
                std::cout << " " << available_device;
            }
            std::cout << std::endl;
        } catch (const std::exception& error) {
            std::cerr << "[DeviceProbe] Failed to query devices: " << error.what() << std::endl;
        }

        if (!CompileOne(core, obb_model, device)) {
            return 1;
        }
        if (!CompileOne(core, seg_model, device)) {
            return 1;
        }

        std::cout << "[DeviceProbe] Probe succeeded." << std::endl;
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "[DeviceProbe] Probe failed: " << error.what() << std::endl;
        return 1;
    }
}
