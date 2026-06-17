# 从模型到打包运行完整流程

这份文档按“照着一步一步做”的方式写，适合新机器第一次部署。

## 一句话说明

GitHub 上只放源码、脚本、文档和少量测试图。下面这些东西不要上传 GitHub：

```text
build/
build_win_qt/
dist/
weights/*.pt
weights/*.onnx
weights/*.engine
*.dll
*.exe
*.so
```

新机器使用时，推荐流程是：

```text
1. 拉代码
2. 安装依赖
3. 放入 .pt 模型
4. 导出 .onnx
5. 生成 .engine
6. 编译程序
7. 打包运行时
8. 启动测试
```

## Windows 完整流程

下面按新电脑第一次部署来写。路径如果和你的电脑不同，要把命令里的路径一起改掉。

推荐目录：

```text
D:\work_floder\Unordered_scraping-main
D:\Tensorrt\TensorRT-8.6.1.6
D:\opencv\opencv-4.12.0\opencv\build
D:\Qt\5.15.2\msvc2019_64
C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.1
```

### 1. 安装软件

需要安装：

```text
Visual Studio 2022
CUDA Toolkit 12.1
TensorRT 8.6.1.6
OpenCV 4.12.0
Qt 5.15.2 msvc2019_64
CMake
Anaconda 或 Python
```

Visual Studio 安装时要勾选：

```text
Desktop development with C++
```

### 2. 准备 Python 环境

打开 PowerShell，安装导出 ONNX 需要的包：

```powershell
D:\Anaconda\envs\cll_yolo\python.exe -m pip install ultralytics "onnx>=1.12.0,<2.0.0" "onnxslim>=0.1.71" onnxruntime
```

如果下载慢，用清华源：

```powershell
D:\Anaconda\envs\cll_yolo\python.exe -m pip install -i https://pypi.tuna.tsinghua.edu.cn/simple ultralytics "onnx>=1.12.0,<2.0.0" "onnxslim>=0.1.71" onnxruntime
```

### 3. 放入模型

在项目根目录新建 `weights` 文件夹，然后放入两个 `.pt` 模型：

```text
weights\best_obb.pt
weights\best_seg.pt
```

完整路径示例：

```text
D:\work_floder\Unordered_scraping-main\weights\best_obb.pt
D:\work_floder\Unordered_scraping-main\weights\best_seg.pt
```

### 4. 导出 ONNX

在 PowerShell 里进入项目目录：

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

执行完后检查有没有这两个文件：

```text
weights\best_obb.onnx
weights\best_seg.onnx
```

### 5. 生成 TensorRT Engine

继续在项目目录执行：

```powershell
D:\Tensorrt\TensorRT-8.6.1.6\bin\trtexec.exe --onnx=D:\work_floder\Unordered_scraping-main\weights\best_obb.onnx --saveEngine=D:\work_floder\Unordered_scraping-main\weights\best_obb.engine --fp16
```

```powershell
D:\Tensorrt\TensorRT-8.6.1.6\bin\trtexec.exe --onnx=D:\work_floder\Unordered_scraping-main\weights\best_seg.onnx --saveEngine=D:\work_floder\Unordered_scraping-main\weights\best_seg.engine --fp16
```

执行完后检查有没有这两个文件：

```text
weights\best_obb.engine
weights\best_seg.engine
```

如果 `--fp16` 报错，去掉 `--fp16` 再执行一次，生成 FP32 engine。

### 6. 配置 CMake

继续在项目目录执行：

```powershell
cmd.exe /s /c '"D:\Visual Studio\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 && cmake -S . -B build_win_qt -G "Visual Studio 17 2022" -A x64 -DTENSORRT_ROOT="D:/Tensorrt/TensorRT-8.6.1.6" -DOpenCV_DIR="D:/opencv/opencv-4.12.0/opencv/build" -DQt5_DIR="D:/Qt/5.15.2/msvc2019_64/lib/cmake/Qt5" -DBUILD_QT_APP=ON'
```

执行成功后，会出现：

```text
build_win_qt\
```

### 7. 编译程序

继续执行：

```powershell
cmd.exe /s /c '"D:\Visual Studio\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 && cmake --build build_win_qt --config Release --target yolov11-tensorrt_qt_app yolov11-tensorrt_obb yolov11-tensorrt_seg'
```

执行成功后，检查这三个文件：

```text
build_win_qt\Release\yolov11-tensorrt_qt_app.exe
build_win_qt\Release\yolov11-tensorrt_obb.exe
build_win_qt\Release\yolov11-tensorrt_seg.exe
```

### 8. 打包运行时

确认 `.engine` 和 `.exe` 都有以后，再运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\deploy_windows.ps1
```

这个脚本会复制运行需要的文件，例如：

```text
Qt DLL
OpenCV DLL
TensorRT DLL
CUDA 运行时 DLL
Qt platforms 插件
程序 exe
启动脚本
engine 模型
```

如果这一步报错，优先检查：

```text
build_win_qt\Release\yolov11-tensorrt_qt_app.exe 是否存在
weights\best_obb.engine 是否存在
weights\best_seg.engine 是否存在
deploy_windows.ps1 里的路径是否和本机安装路径一致
```

### 9. 启动程序

打包完成后，在项目根目录执行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1
```

也可以手动指定 engine：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1 -ObbEngine .\weights\best_obb.engine -SegEngine .\weights\best_seg.engine
```

程序打开后：

```text
1. 如果启动时没自动加载模型，点“工程设置”
2. 选择 OBB engine
3. 选择 SEG engine
4. 点“加载双模型”
5. 点“加载图片”或“打开相机”
6. 点“开始检测”
7. 需要循环模拟抓取时，点“自动抓取”
```

## Windows 搬到另一台电脑

如果另一台电脑要运行源码工程，推荐重新按上面的完整流程做一遍。

如果只是临时演示，可以复制打包后的运行目录。但要注意：

```text
1. 目标电脑也需要 NVIDIA 显卡和兼容驱动
2. TensorRT engine 强绑定 GPU / CUDA / TensorRT 版本
3. 换电脑后 engine 可能不能用，不能用就要在目标电脑重新生成
```

## Ubuntu 快速流程

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

单独编译 OBB / SEG 测试程序：

```bash
cmake --build build --target yolov11-tensorrt_obb --config Release
cmake --build build --target yolov11-tensorrt_seg --config Release
```

## 常见问题

### 直接运行 deploy_windows.ps1 可以打包吗

不可以直接跳到这一步。必须先有：

```text
weights\best_obb.engine
weights\best_seg.engine
build_win_qt\Release\yolov11-tensorrt_qt_app.exe
```

有了这些，再运行 `deploy_windows.ps1` 才是完整打包。

### TensorRT engine 加载失败

优先在当前机器重新生成 `.engine`。不要直接复用另一台机器生成的 engine。

### 找不到 DLL

先运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\deploy_windows.ps1
```

再通过 `launch_tankeye.ps1` 启动。

### PowerShell 不让运行脚本

使用这种方式启动：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1
```
