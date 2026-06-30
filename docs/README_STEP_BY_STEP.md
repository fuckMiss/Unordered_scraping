# 从源码到运行

这是给第一次接触项目的人看的最短路径。

## 步骤

1. 拉取代码
2. 安装 Visual Studio / CUDA / TensorRT / OpenCV / Qt
3. 放入 `weights/best_obb.pt` 和 `weights/best_seg.pt`
4. 导出 ONNX
5. 生成 TensorRT engine
6. 编译 `yolov11-tensorrt_qt_app`
7. 运行 `deploy_windows.ps1`
8. 运行 `launch_tankeye.ps1`

## Windows 命令

```powershell
cmake -S . -B build_win_qt -G "Visual Studio 17 2022" -A x64 -DBUILD_QT_APP=ON
cmake --build build_win_qt --config Release --target yolov11-tensorrt_qt_app
powershell -NoProfile -ExecutionPolicy Bypass -File .\deploy_windows.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1
```

## 打包前检查

- `build_win_qt\Release\yolov11-tensorrt_qt_app.exe`
- `weights\best_obb.engine`
- `weights\best_seg.engine`

## 常见卡点

- engine 不是本机生成的
- TensorRT / CUDA / OpenCV 路径没写对
- 没先编译 Qt 程序

