# YOLOv11 TensorRT Qt APP

这是一个面向工业视觉部署的 YOLOv11 TensorRT C++ 项目，包含 `DET`、`OBB`、`SEG` 三类推理入口，以及一个基于 Qt5 的联合检测应用。

当前项目重点是通过 `OBB + SEG` 联合检测完成截止阀等目标的无序抓取上料视觉流程，并在 Qt 界面中模拟机械臂抓取闭环。

## 功能概览

- `DET`：标准目标检测，对应 `main.cpp`
- `OBB`：旋转框检测，对应 `main_obb.cpp`
- `SEG`：实例分割，对应 `main_seg.cpp`
- `Qt APP`：上位机应用，同时加载 `OBB engine + SEG engine`
- Windows 部署脚本：自动复制 Qt、OpenCV、TensorRT、CUDA 运行时 DLL
- 启动脚本：自动配置运行时环境并启动 Qt 应用

Qt 应用的核心流程：

```text
读取图像/视频帧 -> OBB 检测 -> SEG 分割 -> 联合后处理 -> 显示结果 -> 模拟机械臂抓取 -> 回到原点 -> 下一轮检测
```

当前机械臂控制器是模拟实现，代码位于 `qt/robot_controller.*`。接入真实机械臂时，可以替换该控制器内部逻辑。

## 项目结构

```text
Unordered_scraping-main/
├── asset/                       # 测试图片和示例资源
├── qt/                          # Qt 上位机界面
│   ├── grasp_main_window.*       # 主窗口
│   ├── grasp_workflow.*          # OBB + SEG 联合检测流程
│   ├── frame_postprocess.*       # 联合后处理
│   ├── frame_overlay.*           # 结果绘制
│   ├── robot_controller.*        # 机械臂流程模拟控制
│   └── frame_result.h
├── src/                         # TensorRT C++ 推理核心
│   ├── YOLOv11.*                 # DET 推理
│   ├── YOLOv11_OBB.*             # OBB 推理
│   ├── YOLOv11_SEG.*             # SEG 推理
│   ├── preprocess.*              # CUDA 预处理
│   ├── trt_utils.*               # TensorRT 工具
│   └── deploy_utils.*            # 命令行和部署工具
├── weights/                      # 模型目录，本地放置 .pt/.onnx/.engine
├── main.cpp                      # DET 命令行入口
├── main_obb.cpp                  # OBB 命令行入口
├── main_seg.cpp                  # SEG 命令行入口
├── CMakeLists.txt
├── deploy_windows.ps1            # Windows DLL 部署脚本
├── launch_tankeye.ps1            # Windows Qt APP 启动脚本
└── README_*.md                   # 分模块说明文档
```

## 文档导航

- `README_WINDOWS.md`：Windows 构建、部署和启动说明
- `README_NEW_MACHINE_WINDOWS.md`：新 Windows 机器从 `.pt` 模型到 Qt APP 的完整部署流程
- `README_APP.md`：Qt 应用层结构和运行说明
- `README_OBB.md`：OBB 模型部署与推理说明
- `README_SEG.md`：SEG 模型部署与推理说明
- `README_DEPLOY.md`：部署相关补充说明

## 环境要求

Windows 已验证环境：

- Visual Studio 2022 MSVC x64
- CUDA Toolkit 12.1
- TensorRT 8.6.1.6
- OpenCV 4.12.0 Windows 预编译包
- Qt 5.15.2 `msvc2019_64`
- CMake 4.2.6
- Python / Anaconda，用于通过 Ultralytics 导出 ONNX

Linux 侧项目也保留构建入口，建议使用系统 Qt5，避免混用 Anaconda Qt。

## 模型文件

模型文件默认放在 `weights/`：

```text
weights/best_obb.pt
weights/best_seg.pt
weights/best_obb.onnx
weights/best_seg.onnx
weights/best_obb.engine
weights/best_seg.engine
```

`.pt`、`.onnx`、`.engine` 文件默认不会提交到 Git。TensorRT engine 与 GPU、CUDA、TensorRT 和系统环境强绑定，建议在目标机器上重新生成。

## Windows 快速开始

### 1. 导出 ONNX

```powershell
cd D:\work_floder\Unordered_scraping-main

D:\Anaconda\envs\cll_yolo\python.exe -c "from ultralytics import YOLO; model=YOLO(r'D:\work_floder\Unordered_scraping-main\weights\best_obb.pt'); model.export(format='onnx')"

D:\Anaconda\envs\cll_yolo\python.exe -c "from ultralytics import YOLO; model=YOLO(r'D:\work_floder\Unordered_scraping-main\weights\best_seg.pt'); model.export(format='onnx')"
```

### 2. 生成 TensorRT Engine

```powershell
D:\Tensorrt\TensorRT-8.6.1.6\bin\trtexec.exe `
  --onnx=D:\work_floder\Unordered_scraping-main\weights\best_obb.onnx `
  --saveEngine=D:\work_floder\Unordered_scraping-main\weights\best_obb.engine `
  --fp16

D:\Tensorrt\TensorRT-8.6.1.6\bin\trtexec.exe `
  --onnx=D:\work_floder\Unordered_scraping-main\weights\best_seg.onnx `
  --saveEngine=D:\work_floder\Unordered_scraping-main\weights\best_seg.engine `
  --fp16
```

如果 `--fp16` 报错，可以先去掉 `--fp16` 生成 FP32 engine。

### 3. 配置 CMake

```powershell
cmd.exe /s /c '"D:\Visual Studio\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 && cmake -S . -B build_win_qt -G "Visual Studio 17 2022" -A x64 -DTENSORRT_ROOT="D:/Tensorrt/TensorRT-8.6.1.6" -DOpenCV_DIR="D:/opencv/opencv-4.12.0/opencv/build" -DQt5_DIR="D:/Qt/5.15.2/msvc2019_64/lib/cmake/Qt5" -DBUILD_QT_APP=ON'
```

### 4. 编译

```powershell
cmd.exe /s /c '"D:\Visual Studio\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 && cmake --build build_win_qt --config Release --target yolov11-tensorrt_qt_app yolov11-tensorrt_obb yolov11-tensorrt_seg'
```

### 5. 部署运行时 DLL

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\deploy_windows.ps1
```

### 6. 启动 Qt APP

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1
```

也可以显式指定 engine：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1 `
  -ObbEngine .\weights\best_obb.engine `
  -SegEngine .\weights\best_seg.engine
```

## 命令行测试

OBB：

```powershell
.\build_win_qt\Release\yolov11-tensorrt_obb.exe .\weights\best_obb.engine .\asset\20260331_090339_757.jpg --no-warmup
```

SEG：

```powershell
.\build_win_qt\Release\yolov11-tensorrt_seg.exe .\weights\best_seg.engine .\asset\20260331_090339_757.jpg --no-warmup
```

## Linux 构建

安装 Qt5：

```bash
sudo apt update
sudo apt install qtbase5-dev qtbase5-dev-tools qt5-qmake
```

配置和编译：

```bash
cmake -S . -B build
cmake --build build --config Release
```

单独编译目标：

```bash
cmake --build build --target yolov11-tensorrt --config Release
cmake --build build --target yolov11-tensorrt_obb --config Release
cmake --build build --target yolov11-tensorrt_seg --config Release
cmake --build build --target yolov11-tensorrt_qt_app --config Release
```

Linux 上如果通过 apt 安装 TensorRT，`trtexec` 通常位于：

```bash
/usr/src/tensorrt/bin/trtexec
```

## 常见问题

### TensorRT engine 加载失败

优先在当前机器重新生成 `.engine`。不要直接复用另一台机器生成的 engine。

### 结果显示为 `class_0`

说明 C++ 端类别名没有配置，或类别数量和模型输出不一致。检查：

```text
src/YOLOv11_OBB.cpp
src/YOLOv11_SEG.cpp
```

也可以通过命令行参数 `--labels=xxx.txt` 指定类别名文件。

### 找不到 DLL

先运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\deploy_windows.ps1
```

再通过 `launch_tankeye.ps1` 启动 Qt 应用。

## License

本项目基于仓库内 `LICENSE` 文件发布。
