# YOLOv11 TensorRT Qt APP

这是一个面向工业视觉部署的 `OBB + SEG` 联合检测 Qt 上位机项目，用于截止阀等目标的无序抓取上料视觉流程。

核心流程：

```text
读取图像/相机帧 -> OBB 检测 -> SEG 分割 -> 联合后处理 -> 显示结果 -> 模拟机械臂抓取 -> 回原点 -> 下一轮检测
```

当前机械臂控制器是模拟实现，代码位于 `qt/robot_controller.*`。接入真实机械臂时，可以保留 `grabAsync(...)` 接口，替换内部通信逻辑。

## 项目结构

```text
Unordered_scraping-main/
├── qt/                          # Qt 上位机界面与抓取业务流程
│   ├── main.cpp                  # Qt 应用入口
│   ├── grasp_main_window.*       # 主窗口
│   ├── grasp_workflow.*          # OBB + SEG 联合检测流程
│   ├── frame_postprocess.*       # 联合后处理
│   ├── frame_overlay.*           # 结果绘制
│   ├── robot_controller.*        # 机械臂流程模拟控制
│   ├── frame_result.h            # 一帧联合检测结果结构
│   └── assets/                   # Qt 图标与界面资源
├── src/                          # TensorRT C++ 推理核心
│   ├── YOLOv11_OBB.*             # OBB 推理
│   ├── YOLOv11_SEG.*             # SEG 推理
│   ├── preprocess.*              # CUDA 预处理
│   ├── trt_utils.*               # TensorRT 工具
│   └── deploy_utils.*            # 部署工具函数
├── asset/                        # 抓取流程测试图片
├── docs/                         # Ubuntu / Windows / 应用说明
├── main_obb.cpp                  # OBB 命令行测试入口
├── main_seg.cpp                  # SEG 命令行测试入口
├── deploy_windows.ps1            # Windows DLL 部署脚本
├── launch_tankeye.ps1            # Windows Qt APP 启动脚本
├── launch_tankeye.sh             # Ubuntu Qt APP 启动脚本
└── CMakeLists.txt
```

## 文档导航

- `docs/README_STEP_BY_STEP.md`：从模型到编译、打包、运行的傻瓜式完整流程
- `docs/README_WINDOWS.md`：Windows 构建、部署和启动说明
- `docs/README_NEW_MACHINE_WINDOWS.md`：新 Windows 机器从 `.pt` 模型到 Qt APP 的完整部署流程
- `docs/README_APP.md`：Qt 应用层结构和运行说明
- `docs/README_OBB.md`：OBB 模型部署与命令行测试说明
- `docs/README_SEG.md`：SEG 模型部署与命令行测试说明
- `docs/README_DEPLOY.md`：Ubuntu 部署相关补充说明

## 环境要求

Windows 已验证环境：

- Visual Studio 2022 MSVC x64
- CUDA Toolkit 12.1
- TensorRT 8.6.1.6
- OpenCV 4.12.0 Windows 预编译包
- Qt 5.15.2 `msvc2019_64`
- CMake 4.2.6
- Python / Anaconda，用于通过 Ultralytics 导出 ONNX

Ubuntu 侧建议使用系统 Qt5，避免混用 Anaconda Qt。

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

导出 ONNX：

```powershell
D:\Anaconda\envs\cll_yolo\python.exe -c "from ultralytics import YOLO; model=YOLO(r'D:\work_floder\Unordered_scraping-main\weights\best_obb.pt'); model.export(format='onnx')"
D:\Anaconda\envs\cll_yolo\python.exe -c "from ultralytics import YOLO; model=YOLO(r'D:\work_floder\Unordered_scraping-main\weights\best_seg.pt'); model.export(format='onnx')"
```

生成 TensorRT Engine：

```powershell
D:\Tensorrt\TensorRT-8.6.1.6\bin\trtexec.exe --onnx=D:\work_floder\Unordered_scraping-main\weights\best_obb.onnx --saveEngine=D:\work_floder\Unordered_scraping-main\weights\best_obb.engine --fp16
D:\Tensorrt\TensorRT-8.6.1.6\bin\trtexec.exe --onnx=D:\work_floder\Unordered_scraping-main\weights\best_seg.onnx --saveEngine=D:\work_floder\Unordered_scraping-main\weights\best_seg.engine --fp16
```

配置和编译：

```powershell
cmd.exe /s /c '"D:\Visual Studio\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 && cmake -S . -B build_win_qt -G "Visual Studio 17 2022" -A x64 -DTENSORRT_ROOT="D:/Tensorrt/TensorRT-8.6.1.6" -DOpenCV_DIR="D:/opencv/opencv-4.12.0/opencv/build" -DQt5_DIR="D:/Qt/5.15.2/msvc2019_64/lib/cmake/Qt5" -DBUILD_QT_APP=ON'
cmd.exe /s /c '"D:\Visual Studio\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 && cmake --build build_win_qt --config Release --target yolov11-tensorrt_qt_app yolov11-tensorrt_obb yolov11-tensorrt_seg'
```

部署和启动：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\deploy_windows.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1
```

## Ubuntu 快速开始

安装 Qt5：

```bash
sudo apt update
sudo apt install qtbase5-dev qtbase5-dev-tools qt5-qmake
```

配置和编译：

```bash
cmake -S . -B build
cmake --build build --target yolov11-tensorrt_qt_app --config Release
```

启动：

```bash
./launch_tankeye.sh
```

## 命令行测试

保留 OBB 和 SEG 两个命令行测试入口，便于单独验证 engine：

```bash
cmake --build build --target yolov11-tensorrt_obb --config Release
cmake --build build --target yolov11-tensorrt_seg --config Release
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
