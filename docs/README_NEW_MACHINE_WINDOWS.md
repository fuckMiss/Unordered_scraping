# Windows 新机器部署和项目梳理

这份文档用于把本项目拷贝到一台新的 Windows 机器后，从“只有代码和 `.pt` 模型”开始完成部署、模型转换、编译、测试和运行。

项目核心流程：

```text
准备环境 -> 放入 .pt 模型 -> 导出 .onnx -> 生成 .engine -> 配置类别名 -> 编译 -> 测试 -> 启动 Qt 界面
```

## 1. 项目用途

本项目是一个 YOLOv11 TensorRT C++ 推理项目，包含两个命令行测试入口和一个 Qt 上位机入口：

- `OBB`：旋转框检测，对应 `main_obb.cpp`
- `SEG`：实例分割，对应 `main_seg.cpp`
- `Qt APP`：上位机界面，同时加载 `OBB + SEG` 两个 engine，对应 `qt/main.cpp`

当前实际业务主要使用：

```text
OBB engine + SEG engine + Qt APP
```

## 2. 推荐目录

建议新机器使用和开发机器一致的目录，省掉大量路径修改：

```text
D:\work_floder\Unordered_scraping-main
D:\Tensorrt\TensorRT-8.6.1.6
D:\opencv\opencv-4.12.0\opencv\build
D:\Qt\5.15.2\msvc2019_64
C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.1
```

如果路径不同，需要在 CMake 命令、`deploy_windows.ps1` 和 `launch_tankeye.ps1` 中同步修改对应路径。

## 3. 新机器需要安装的软件

必须安装：

- Visual Studio 2022，安装时勾选 `Desktop development with C++`
- CUDA Toolkit 12.1
- TensorRT 8.6.1.6
- OpenCV 4.12.0 Windows 预编译版
- Qt 5.15.2 `msvc2019_64`
- CMake
- Anaconda 或 Python 环境

Python 环境用于把 Ultralytics `.pt` 模型导出为 `.onnx`。

安装 Python 依赖：

```powershell
D:\Anaconda\envs\cll_yolo\python.exe -m pip install ultralytics "onnx>=1.12.0,<2.0.0" "onnxslim>=0.1.71" onnxruntime
```

如果下载慢，可以使用清华源：

```powershell
D:\Anaconda\envs\cll_yolo\python.exe -m pip install -i https://pypi.tuna.tsinghua.edu.cn/simple ultralytics "onnx>=1.12.0,<2.0.0" "onnxslim>=0.1.71" onnxruntime
```

## 4. 放入模型

把传输过来的 `.pt` 模型放到：

```text
weights\best_obb.pt
weights\best_seg.pt
```

完整路径示例：

```text
D:\work_floder\Unordered_scraping-main\weights\best_obb.pt
D:\work_floder\Unordered_scraping-main\weights\best_seg.pt
```

不建议直接复用旧机器生成的 `.engine`。TensorRT engine 和显卡、CUDA、TensorRT、系统环境强绑定，新机器上应重新生成。

## 5. 导出 ONNX

进入项目目录：

```powershell
cd D:\work_floder\Unordered_scraping-main
```

导出 OBB：

```powershell
D:\Anaconda\envs\cll_yolo\python.exe -c "from ultralytics import YOLO; model=YOLO(r'D:\work_floder\Unordered_scraping-main\weights\best_obb.pt'); model.export(format='onnx')"
```

导出 SEG：

```powershell
D:\Anaconda\envs\cll_yolo\python.exe -c "from ultralytics import YOLO; model=YOLO(r'D:\work_floder\Unordered_scraping-main\weights\best_seg.pt'); model.export(format='onnx')"
```

成功后应生成：

```text
weights\best_obb.onnx
weights\best_seg.onnx
```

## 6. 生成 TensorRT Engine

OBB：

```powershell
D:\Tensorrt\TensorRT-8.6.1.6\bin\trtexec.exe `
  --onnx=D:\work_floder\Unordered_scraping-main\weights\best_obb.onnx `
  --saveEngine=D:\work_floder\Unordered_scraping-main\weights\best_obb.engine `
  --fp16
```

SEG：

```powershell
D:\Tensorrt\TensorRT-8.6.1.6\bin\trtexec.exe `
  --onnx=D:\work_floder\Unordered_scraping-main\weights\best_seg.onnx `
  --saveEngine=D:\work_floder\Unordered_scraping-main\weights\best_seg.engine `
  --fp16
```

成功后应生成：

```text
weights\best_obb.engine
weights\best_seg.engine
```

如果 `--fp16` 报错，可以先去掉 `--fp16` 生成 FP32 engine。

## 7. 配置类别名

C++ 推理代码不会自动读取 `.pt` 里的类别名。类别名必须在代码里配置，或者运行命令时通过 `--labels=xxx.txt` 指定。

### 7.1 SEG 类别名

如果分割模型只有一个类别 `border`，修改：

```text
src\YOLOv11_SEG.cpp
```

找到：

```cpp
static const std::vector<std::string> SEG_CLASS_NAMES = {
};
```

改成：

```cpp
static const std::vector<std::string> SEG_CLASS_NAMES = {
    "border"
};
```

否则结果图会显示 `class_0`。

### 7.2 OBB 类别名

修改：

```text
src\YOLOv11_OBB.cpp
```

找到：

```cpp
static const std::vector<std::string> OBB_CLASS_NAMES = {
    ...
};
```

类别数量必须和 OBB 模型真实类别数一致。例如模型是 2 类，就必须写 2 个名称：

```cpp
static const std::vector<std::string> OBB_CLASS_NAMES = {
    "class_name_1",
    "class_name_2"
};
```

如果数量不一致，程序会退回显示 `class_0`、`class_1`。

## 8. 配置 CMake

进入项目目录：

```powershell
cd D:\work_floder\Unordered_scraping-main
```

执行：

```powershell
cmd.exe /s /c '"D:\Visual Studio\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 && cmake -S . -B build_win_qt -G "Visual Studio 17 2022" -A x64 -DTENSORRT_ROOT="D:/Tensorrt/TensorRT-8.6.1.6" -DOpenCV_DIR="D:/opencv/opencv-4.12.0/opencv/build" -DQt5_DIR="D:/Qt/5.15.2/msvc2019_64/lib/cmake/Qt5" -DBUILD_QT_APP=ON'
```

## 9. 编译

编译 Qt 界面和两个命令行测试程序：

```powershell
cmd.exe /s /c '"D:\Visual Studio\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 && cmake --build build_win_qt --config Release --target yolov11-tensorrt_qt_app yolov11-tensorrt_obb yolov11-tensorrt_seg'
```

成功后应生成：

```text
build_win_qt\Release\yolov11-tensorrt_qt_app.exe
build_win_qt\Release\yolov11-tensorrt_obb.exe
build_win_qt\Release\yolov11-tensorrt_seg.exe
```

## 10. 拷贝运行时 DLL

编译完成后执行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\deploy_windows.ps1
```

这个脚本会把 Qt、OpenCV、TensorRT、CUDA 等运行时 DLL 拷贝到：

```text
build_win_qt\Release
```

## 11. 命令行测试

### 11.1 测试 OBB

```powershell
.\build_win_qt\Release\yolov11-tensorrt_obb.exe .\weights\best_obb.engine .\asset\20260331_090339_757.jpg --no-warmup
```

成功后会生成：

```text
obb_result_20260331_090339_757.jpg
```

### 11.2 测试 SEG

```powershell
.\build_win_qt\Release\yolov11-tensorrt_seg.exe .\weights\best_seg.engine .\asset\20260331_090339_757.jpg --no-warmup
```

成功后会生成：

```text
seg_result_20260331_090339_757.jpg
```

如果类别名显示为 `class_0`，回到第 7 步检查类别名配置。

## 12. 启动 Qt 界面

默认读取：

```text
weights\best_obb.engine
weights\best_seg.engine
```

启动：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1
```

也可以显式指定 engine：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1 `
  -ObbEngine .\weights\best_obb.engine `
  -SegEngine .\weights\best_seg.engine
```

## 13. 项目结构梳理

```text
asset\
```

测试图片和示例资源。命令行测试默认可以使用这里的图片。

```text
weights\
```

模型目录。放置 `.pt`、`.onnx`、`.engine`：

```text
best_obb.pt
best_obb.onnx
best_obb.engine
best_seg.pt
best_seg.onnx
best_seg.engine
```

```text
src\
```

TensorRT C++ 推理核心代码：

- `YOLOv11_OBB.cpp/.h`：OBB 推理
- `YOLOv11_SEG.cpp/.h`：SEG 推理
- `preprocess.cu/.h`：CUDA 预处理
- `trt_utils.cpp/.h`：TensorRT engine 加载、ONNX 构建、序列化
- `deploy_utils.cpp/.h`：命令行工具、图片收集、标签读取
- `model_utils.h`：类别名、图像校验等通用工具
- `runtime_utils.h`：资源释放辅助

```text
qt\
```

Qt 上位机界面：

- `main.cpp`：Qt APP 入口
- `grasp_main_window.*`：主界面
- `grasp_workflow.*`：同时调用 OBB + SEG 的流程
- `frame_postprocess.*`：联合后处理
- `frame_overlay.*`：结果绘制
- `robot_controller.*`：机械臂流程模拟控制

```text
main_obb.cpp
main_seg.cpp
```

两个命令行测试入口。

```text
CMakeLists.txt
```

项目构建入口，定义 OBB、SEG、Qt APP 三个目标。

```text
deploy_windows.ps1
```

Windows 运行时 DLL 部署脚本。

```text
launch_tankeye.ps1
```

Windows Qt APP 启动脚本，会自动设置运行时 `Path` 并传入默认 engine。

## 14. 常见问题

### 14.1 engine 加载失败

优先在新机器重新生成 `.engine`。不要直接复用旧机器的 engine。

### 14.2 结果显示 class_0

说明 C++ 端类别名没配置，或类别数量和模型输出类别数不一致。检查：

```text
src\YOLOv11_SEG.cpp
src\YOLOv11_OBB.cpp
```

### 14.3 找不到 DLL

先运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\deploy_windows.ps1
```

再用 `launch_tankeye.ps1` 启动。

### 14.4 CMake 找不到 TensorRT、OpenCV、Qt

检查第 2 步路径是否一致。如果安装路径不同，修改第 8 步 CMake 命令里的：

```text
-DTENSORRT_ROOT=
-DOpenCV_DIR=
-DQt5_DIR=
```

### 14.5 trtexec 不存在

检查 TensorRT 是否安装完整，确认文件存在：

```text
D:\Tensorrt\TensorRT-8.6.1.6\bin\trtexec.exe
```

## 15. 最短操作清单

```text
1. 安装 VS2022、CUDA、TensorRT、OpenCV、Qt、CMake、Python
2. 拷贝项目到 D:\work_floder\Unordered_scraping-main
3. 放入 weights\best_obb.pt 和 weights\best_seg.pt
4. 用 Ultralytics 导出 best_obb.onnx 和 best_seg.onnx
5. 用 trtexec 生成 best_obb.engine 和 best_seg.engine
6. 配置 src\YOLOv11_SEG.cpp 和 src\YOLOv11_OBB.cpp 类别名
7. CMake configure
8. CMake build Release
9. 运行 deploy_windows.ps1
10. 命令行测试 OBB/SEG
11. 运行 launch_tankeye.ps1 启动界面
```
