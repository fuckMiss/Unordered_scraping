# 新 Windows 机器部署

这份文档面向“新电脑第一次把项目跑起来”。

## 推荐顺序

1. 安装 Visual Studio 2022 和 C++ 桌面开发组件
2. 安装 CUDA Toolkit 12.1
3. 安装 TensorRT 8.6.1.6
4. 安装 OpenCV 4.12.0
5. 安装 Qt 5.15.2 `msvc2019_64`
6. 准备 `weights/best_obb.pt` 和 `weights/best_seg.pt`
7. 导出 ONNX
8. 生成 TensorRT engine
9. 配置 CMake
10. 编译
11. 打包运行时 DLL
12. 启动 Qt 界面

## 需要的目录

```text
D:\work_floder\Unordered_scraping-main
D:\Tensorrt\TensorRT-8.6.1.6
D:\opencv\opencv-4.12.0\opencv\build
D:\Qt\5.15.2\msvc2019_64
```

## 建议模型文件

```text
weights\best_obb.pt
weights\best_seg.pt
weights\best_obb.onnx
weights\best_seg.onnx
weights\best_obb.engine
weights\best_seg.engine
```

## 最后要跑的命令

```powershell
cmake -S . -B build_win_qt -G "Visual Studio 17 2022" -A x64 -DBUILD_QT_APP=ON
cmake --build build_win_qt --config Release --target yolov11-tensorrt_qt_app
powershell -NoProfile -ExecutionPolicy Bypass -File .\deploy_windows.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1
```

## 说明

- 如果换机器，TensorRT engine 需要重新生成
- `launch_tankeye.ps1` 默认读取 `weights\best_obb.engine` 和 `weights\best_seg.engine`
- 这套流程适合直接交付给现场电脑

