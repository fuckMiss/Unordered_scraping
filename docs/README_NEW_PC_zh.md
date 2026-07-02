# 新电脑从零安装操作流程

这份文档给刚装好系统的新电脑使用。按顺序做，不跳步。遇到报错先看每一步下面的检查项。

## 0. 先准备这些东西

请先把下面文件或安装包准备好，放到 U 盘或共享盘里：

- 本项目源码文件夹：`Unordered_scraping-unit`
- 模型文件：
  - `weights\best_obb.xml`
  - `weights\best_obb.bin`
  - `weights\best_seg.xml`
  - `weights\best_seg.bin`
- Qt 5.15.2 `msvc2019_64` 安装包或已安装目录
- OpenCV 4.x Windows 版本
- Visual Studio 2022 安装器
- Anaconda 或 Miniconda 安装器
- Intel 显卡驱动安装包，或者能联网更新驱动

推荐把项目放在：

```text
D:\work_floder\Unordered_scraping-unit
```

后面的命令都按这个路径写。如果你放在别的路径，命令里的路径也要一起改。

## 1. 安装 Visual Studio 2022

1. 打开 Visual Studio Installer。
2. 勾选 `Desktop development with C++`。
3. 确认包含 MSVC 编译工具和 Windows SDK。
4. 安装完成后重启电脑。

检查方法：

1. 打开开始菜单。
2. 找到 `x64 Native Tools Command Prompt for VS 2022`。
3. 能打开就说明 C++ 编译环境基本可用。

## 2. 安装 CMake

安装 CMake，并勾选加入 PATH。

检查命令：

```bat
cmake --version
```

能看到版本号就可以。

## 3. 安装 Qt

安装 Qt 5.15.2，并选择 `msvc2019_64` 组件。

推荐安装路径：

```text
D:\Qt\5.15.2\msvc2019_64
```

检查这个文件是否存在：

```text
D:\Qt\5.15.2\msvc2019_64\bin\windeployqt.exe
```

如果没有这个文件，说明 Qt 路径或组件不对。

## 4. 安装 OpenCV

解压或安装 OpenCV 4.x。

推荐路径：

```text
D:\opencv\opencv-4.12.0\opencv\build
```

检查这个目录是否存在：

```text
D:\opencv\opencv-4.12.0\opencv\build\x64\vc16\bin
```

如果 OpenCV 版本不是 4.12.0，也可以用，但后面命令里的 `OpenCV_DIR` 要改成你的实际路径。

## 5. 安装 Anaconda 和 OpenVINO

安装 Anaconda 或 Miniconda。建议创建一个专用环境：

```bat
conda create -n cll_yolo python=3.10 -y
conda activate cll_yolo
pip install -U openvino openvino-dev
```

检查 OpenVINO：

```bat
python -c "import openvino as ov; print(ov.__version__)"
```

能打印版本号就可以。

本项目默认常用路径是：

```text
D:\Anaconda\envs\cll_yolo\Lib\site-packages\openvino
```

如果你的 Anaconda 装在别处，后面 `openvino_DIR` 要改成实际路径。

## 6. 更新显卡驱动

如果使用 Intel 集成显卡跑 OpenVINO GPU，请安装或更新 Intel 显卡驱动。

最简单的方法：

1. 打开设备管理器。
2. 找到显示适配器。
3. 确认能看到 Intel UHD / Intel Iris / Intel Arc 等设备。
4. 安装厂家或 Intel 官方驱动。

如果 GPU 后面跑不起来，先用 CPU 跑，程序仍然可以运行。

## 7. 放好项目和模型

把项目放到：

```text
D:\work_floder\Unordered_scraping-unit
```

确认模型文件在：

```text
D:\work_floder\Unordered_scraping-unit\weights\best_obb.xml
D:\work_floder\Unordered_scraping-unit\weights\best_obb.bin
D:\work_floder\Unordered_scraping-unit\weights\best_seg.xml
D:\work_floder\Unordered_scraping-unit\weights\best_seg.bin
```

如果只有 ONNX 文件，则放到：

```text
D:\work_floder\Unordered_scraping-unit\weights\best_obb.onnx
D:\work_floder\Unordered_scraping-unit\weights\best_seg.onnx
```

然后在项目目录运行：

```bat
python .\export.py
```

正常后会生成 `.xml` 和 `.bin` 文件。

## 8. 打开正确的命令行

推荐用 `x64 Native Tools Command Prompt for VS 2022`。

打开后进入项目目录：

```bat
D:
cd D:\work_floder\Unordered_scraping-unit
conda activate cll_yolo
```

## 9. 配置 CMake

在项目目录运行：

```bat
set openvino_DIR=D:\Anaconda\envs\cll_yolo\Lib\site-packages\openvino\cmake
set OpenCV_DIR=D:\opencv\opencv-4.12.0\opencv\build

cmake -S . -B build_win_unit -G "Visual Studio 17 2022" -A x64 -DBUILD_QT_APP=ON
```

如果 `openvino_DIR` 或 `OpenCV_DIR` 路径不同，改成新电脑上的实际路径。

成功标志：

- 没有红色错误。
- 项目下出现 `build_win_unit` 文件夹。

## 10. 编译程序

继续运行：

```bat
cmake --build build_win_unit --config Release --target tankeye-openvino_qt_app
```

成功后会出现：

```text
build_win_unit\Release\tankeye-openvino_qt_app.exe
```

如果编译失败，先检查：

- Visual Studio 是否安装了 C++ 桌面开发。
- Qt 路径是否是 `msvc2019_64`。
- OpenCV 路径是否正确。
- OpenVINO 是否安装在当前 conda 环境。

## 11. 第一次运行

在项目目录运行：

```bat
powershell -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1
```

如果想强制 CPU：

```bat
powershell -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1 -Device CPU
```

如果想强制 GPU：

```bat
powershell -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1 -Device GPU
```

第一次建议先用默认命令。如果 GPU 不稳定，再用 `-Device CPU`。

## 12. 看日志

运行日志在：

```text
build_win_unit\Release\logs
```

检查实际用了 CPU 还是 GPU：

```bat
findstr /i "OpenVINO Requested Available Selected Device GPU CPU" build_win_unit\Release\logs\tankeye_*.log
```

常见结果：

- `Selected device: GPU`：正在用 OpenVINO GPU 插件。
- `Selected device: CPU`：正在用 CPU，或者 GPU 失败后回退到 CPU。
- `Available devices: CPU GPU`：OpenVINO 能看到 CPU 和 GPU。

## 13. 打包运行目录

确认程序能打开后，运行部署脚本：

```bat
powershell -NoProfile -ExecutionPolicy Bypass -File .\deploy_windows.ps1
```

脚本会把 Qt、OpenCV、OpenVINO 等运行 DLL 复制到：

```text
build_win_unit\Release
```

以后现场运行，优先用：

```bat
powershell -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1
```

不要直接双击 exe，直接双击可能找不到 DLL 或模型路径。

如果要交给别人拷走，建议保持下面这个目录结构：

```text
TankEye-Package\
  launch_tankeye.ps1
  deploy_windows.ps1
  weights\
    best_obb.xml
    best_obb.bin
    best_seg.xml
    best_seg.bin
  build_win_unit\
    Release\
      tankeye-openvino_qt_app.exe
      *.dll
      platforms\
      imageformats\
      qt\
        assets\
      logs\
  docs\
```

至少要带走这些东西：

- `build_win_unit\Release\`
- `weights\best_obb.xml`
- `weights\best_obb.bin`
- `weights\best_seg.xml`
- `weights\best_seg.bin`
- `launch_tankeye.ps1`
- `deploy_windows.ps1`
- 本项目的说明文档

最省事的做法是：先运行 `deploy_windows.ps1`，再把整个项目文件夹拷走。这样路径最不容易错。

如果你想压成一个单独的 zip，可以在项目目录执行：

```bat
powershell -NoProfile -Command "Compress-Archive -Path .\launch_tankeye.ps1,.\deploy_windows.ps1,.\weights,.\build_win_unit\Release,.\docs -DestinationPath .\tankeye_release.zip -Force"
```

压完后，把 `tankeye_release.zip` 拷到目标电脑，解压后从解压目录运行：

```bat
powershell -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1
```

注意：不要只拷 `tankeye-openvino_qt_app.exe`。只拷 exe 通常会缺 DLL、缺模型或缺 Qt 插件。

## 14. PLC 联动前检查

默认 PLC 地址：

```text
192.168.3.205:502
```

接线和网络检查：

1. 电脑网线接到 PLC 网络。
2. 电脑 IP 和 PLC 在同一网段，例如 `192.168.3.xxx`。
3. 能 ping 通 PLC：

```bat
ping 192.168.3.205
```

PLC 寄存器：

```text
D500   center_x
D502   center_y
D504   angle_deg
D506   pick_status
D508   head_type
D1500  trigger
```

触发逻辑：

```text
PLC 写 D1500 = 1
软件检测一帧
软件写 D500/D502/D504/D506/D508
软件清 D1500
```

## 15. 最常见问题

问题：打开程序提示找不到 exe。

处理：先编译，确认这个文件存在：

```text
build_win_unit\Release\tankeye-openvino_qt_app.exe
```

问题：提示找不到模型。

处理：确认这四个文件存在：

```text
weights\best_obb.xml
weights\best_obb.bin
weights\best_seg.xml
weights\best_seg.bin
```

问题：提示找不到 Qt DLL。

处理：运行：

```bat
powershell -NoProfile -ExecutionPolicy Bypass -File .\deploy_windows.ps1
```

问题：GPU 运行失败。

处理：先用 CPU 跑：

```bat
powershell -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1 -Device CPU
```

然后更新 Intel 显卡驱动。

问题：生成了 `kernel.errors.txt`。

处理：先看程序是否能正常推理。如果日志显示 `Selected device: GPU` 且结果正常，这可能只是 Intel GPU 编译器诊断。若不正常，用 `-Device CPU` 测试。

问题：PLC 没反应。

处理：

1. 检查 `ping 192.168.3.205` 是否通。
2. 检查 PLC 的 Modbus TCP 是否开启。
3. 检查电脑 IP 是否在同一网段。
4. 检查 PLC 寄存器地址是否仍是 D500、D502、D504、D506、D508、D1500。

## 16. 给现场人员的一句话流程

```text
装 VS2022 C++ -> 装 CMake -> 装 Qt -> 装 OpenCV -> 装 Anaconda/OpenVINO -> 放项目和模型 -> cmake 配置 -> cmake 编译 -> launch_tankeye.ps1 启动 -> deploy_windows.ps1 打包运行 DLL -> 压 zip -> 看 logs 确认 CPU/GPU -> 连 PLC 测 D1500
```
