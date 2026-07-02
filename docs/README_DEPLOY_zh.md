# Windows 部署

本文档说明 TankEye-Iris 的 Windows-only 部署流程，内容按当前 `CMakeLists.txt`、`launch_tankeye.ps1`、`deploy_windows.ps1` 和 OpenVINO 推理代码整理。

## 1. 安装依赖

使用 Anaconda Prompt 或 Developer Command Prompt。

Python 工具：

```bat
pip install -U openvino openvino-dev
```

原生工具：

- Visual Studio 2022，安装 Desktop development with C++
- CMake
- Qt 5.15.2 `msvc2019_64`
- OpenCV 4.x Windows 构建
- Intel 显卡驱动

脚本中已知的本地路径：

```text
D:\Qt\5.15.2\msvc2019_64
D:\opencv\opencv-4.12.0\opencv\build
D:\Anaconda\envs\cll_yolo\Lib\site-packages\openvino
```

## 2. 转换模型

如果需要重新生成 OpenVINO IR，输入 ONNX 文件为：

```text
weights\best_obb.onnx
weights\best_seg.onnx
```

运行：

```bat
python .\export.py
```

生成后，运行时默认使用的 OpenVINO IR 文件为：

```text
weights\best_obb.xml
weights\best_obb.bin
weights\best_seg.xml
weights\best_seg.bin
```

## 3. 配置并构建

```bat
set openvino_DIR=D:\Anaconda\envs\cll_yolo\Lib\site-packages\openvino\cmake
set OpenCV_DIR=D:\opencv\opencv-4.12.0\opencv\build

cmake -S . -B build_win_unit -G "Visual Studio 17 2022" -A x64 -DBUILD_QT_APP=ON
cmake --build build_win_unit --config Release --target tankeye-openvino_qt_app
```

可选的命令行测试目标：

```bat
cmake --build build_win_unit --config Release --target tankeye-openvino_obb
cmake --build build_win_unit --config Release --target tankeye-openvino_seg
```

## 4. 运行

```bat
powershell -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1
```

设备选项：

```bat
powershell -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1 -Device AUTO
powershell -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1 -Device GPU
powershell -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1 -Device CPU
```

`AUTO` 是项目代码里的设备选择逻辑：先调用 OpenVINO `GPU` 编译模型，失败后再调用 `CPU`。这里不是直接使用 OpenVINO 的 `AUTO` 插件名。

## 5. 验证 Intel UHD 是否被使用

打开最新日志目录：

```text
build_win_unit\Release\logs
```

或运行：

```bat
findstr /i "OpenVINO Requested Available Selected Device GPU CPU" build_win_unit\Release\logs\tankeye_*.log
```

日志含义：

- `Available devices: CPU GPU`：OpenVINO 可以识别 CPU 和 GPU 设备。
- `Selected device: GPU`：OpenVINO GPU 插件已启用。在只有 Intel UHD 的机器上，这表示集成显卡正在被使用。
- `Selected device: CPU`：GPU 失败，或用户显式指定了 CPU。

## 6. 打包运行时 DLL

```bat
powershell -NoProfile -ExecutionPolicy Bypass -File .\deploy_windows.ps1
```

脚本会复制：

- Qt 运行时文件
- OpenCV DLL
- 首个找到的 OpenVINO `libs` 或 runtime bin 目录中的 DLL
- Qt image/platform 插件
- `qt\assets\app_logo_cutout.png`
- `qt\assets\app_icon.ico`

部署后需要确认运行目录或 PATH 中存在所需 OpenVINO 插件 DLL，例如 CPU/GPU 插件。脚本会从当前环境候选路径中选择第一个存在的 OpenVINO DLL 目录，不会合并复制多个 OpenVINO 目录。

## 7. Intel GPU Kernel 诊断

如果出现带有 CISA 或 kernel header 信息的 `kernel.errors.txt`，这是 OpenVINO GPU 插件使用的 Intel GPU 编译器生成的文件。

建议检查：

1. 使用 `-Device CPU` 运行。
2. 如果 CPU 正常而 GPU 失败，更新 Intel 显卡驱动。
3. 确认 `openvino_intel_gpu_plugin.dll` 位于运行目录或 PATH 中。
4. 如果驱动已经是最新版本，尝试其他 OpenVINO Runtime 版本。
