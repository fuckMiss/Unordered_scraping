# Windows 构建与运行

本项目已在 Windows 上验证过 Qt 上位机目标。

## 环境

- Visual Studio 2022
- CUDA Toolkit 12.1
- TensorRT 8.6.1.6
- OpenCV 4.12.0
- Qt 5.15.2 `msvc2019_64`
- CMake 4.x

## 建议路径

```text
D:\work_floder\Unordered_scraping-main
D:\Tensorrt\TensorRT-8.6.1.6
D:\opencv\opencv-4.12.0\opencv\build
D:\Qt\5.15.2\msvc2019_64
C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.1
```

## 配置

```powershell
cmake -S . -B build_win_qt -G "Visual Studio 17 2022" -A x64 -DBUILD_QT_APP=ON
```

如需显式指定依赖路径，可补充：

```powershell
cmake -S . -B build_win_qt -G "Visual Studio 17 2022" -A x64 `
  -DTENSORRT_ROOT="D:/Tensorrt/TensorRT-8.6.1.6" `
  -DOpenCV_DIR="D:/opencv/opencv-4.12.0/opencv/build" `
  -DQt5_DIR="D:/Qt/5.15.2/msvc2019_64/lib/cmake/Qt5" `
  -DBUILD_QT_APP=ON
```

## 编译

```powershell
cmake --build build_win_qt --config Release --target yolov11-tensorrt_qt_app
```

命令行目标也可单独编译：

```powershell
cmake --build build_win_qt --config Release --target yolov11-tensorrt_obb yolov11-tensorrt_seg
```

## 打包运行时 DLL

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\deploy_windows.ps1
```

## 启动

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1
```

也可以手动指定 engine：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1 `
  -ObbEngine .\weights\best_obb.engine `
  -SegEngine .\weights\best_seg.engine
```

## 说明

- `launch_tankeye.ps1` 默认启动 `build_win_qt\Release\yolov11-tensorrt_qt_app.exe`
- 当前构建已关闭静态 CUDA runtime，避免 `LIBCMT` 冲突
- Qt 源码已加 `/utf-8`

