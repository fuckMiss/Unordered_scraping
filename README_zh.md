# TankEye-Iris

TankEye-Iris 是一个面向 Windows 的工业视觉无序抓取应用。项目使用 OpenVINO 运行 YOLOv11 OBB 和 YOLOv11 SEG 模型，保留 Qt 操作界面，并延续现有的抓取位姿后处理和 PLC 输出流程。

本文档按当前代码整理，主要对应 `CMakeLists.txt`、`launch_tankeye.ps1`、`export.py`、`src/openvino_utils.cpp`、`qt/grasp_workflow.*` 和 `qt/robot_controller.*`。

当前运行时仅支持 OpenVINO。CUDA、TensorRT、DirectML、Linux 启动脚本和 `.engine` 文件不属于这个 Windows 包。

## 运行环境

- 操作系统：Windows x64
- 界面：Qt 5.15.2 `msvc2019_64`
- 推理：OpenVINO Runtime
- 视觉依赖：OpenCV 4.x
- 默认设备：`AUTO`，由项目代码先尝试 OpenVINO `GPU`，失败后回退到 `CPU`

当日志显示 `Selected device: GPU` 时，说明 OpenVINO 正在使用 GPU 插件。如果机器只有 Intel UHD Graphics，那么这里的 GPU 就是 Intel 集成显卡。

## 模型文件

运行时默认加载的模型文件：

```text
weights\best_obb.xml
weights\best_obb.bin
weights\best_seg.xml
weights\best_seg.bin
```

如果需要从 ONNX 重新转换，`export.py` 期望输入：

```text
weights\best_obb.onnx
weights\best_seg.onnx
```

将 ONNX 转换为 OpenVINO IR：

```bat
python .\export.py
```

## 构建

```bat
set openvino_DIR=D:\Anaconda\envs\cll_yolo\Lib\site-packages\openvino\cmake
set OpenCV_DIR=D:\opencv\opencv-4.12.0\opencv\build

cmake -S . -B build_win_unit -G "Visual Studio 17 2022" -A x64 -DBUILD_QT_APP=ON
cmake --build build_win_unit --config Release --target tankeye-openvino_qt_app
```

## 运行

使用默认设备选择：

```bat
powershell -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1
```

强制使用 Intel GPU：

```bat
powershell -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1 -Device GPU
```

强制使用 CPU：

```bat
powershell -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1 -Device CPU
```

运行日志会写入：

```text
build_win_unit\Release\logs\tankeye_*.log
```

查看 OpenVINO 实际选择的设备：

```bat
findstr /i "OpenVINO Requested Available Selected Device GPU CPU" build_win_unit\Release\logs\tankeye_*.log
```

## Intel GPU Kernel 诊断

Intel GPU 驱动或编译器诊断可能会生成 `kernel.errors.txt`，里面包含 CISA 或 kernel header 相关信息。该文件由 Intel OpenVINO GPU 栈在为集成显卡编译 kernel 时生成，不是 Qt、PLC 或抓取后处理错误。

如果推理结果正确，并且日志仍然显示 `Selected device: GPU`，该文件可能只是非致命的编译器诊断。如果 GPU 推理失败或不稳定，可以用 CPU 测试：

```bat
powershell -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1 -Device CPU
```

如果 CPU 正常但 GPU 失败，请更新 Intel 显卡驱动，或使用其他 OpenVINO Runtime 版本验证。

## 部署

```bat
powershell -NoProfile -ExecutionPolicy Bypass -File .\deploy_windows.ps1
```

完整 Windows 部署清单见 [docs/README_DEPLOY_zh.md](docs/README_DEPLOY_zh.md)。

如果是在一台刚装好系统的新电脑上部署，请按 [docs/README_NEW_PC_zh.md](docs/README_NEW_PC_zh.md) 从零安装。
