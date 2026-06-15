# Windows Build and Run

This project has been verified on Windows with:

- Visual Studio 2022 MSVC x64
- CUDA Toolkit 12.1
- TensorRT 8.6.1.6
- OpenCV 4.12.0 prebuilt for Windows
- Qt 5.15.2 `msvc2019_64`
- CMake 4.2.6

## Local Paths

The current machine uses these paths:

```powershell
$env:TENSORRT_ROOT = "D:\Tensorrt\TensorRT-8.6.1.6"
$env:OpenCV_DIR = "D:\opencv\opencv-4.12.0\opencv\build"
$env:Qt5_DIR = "D:\Qt\5.15.2\msvc2019_64\lib\cmake\Qt5"
```

Make sure these runtime folders are available in `Path`, or run the included launch script:

```text
D:\Qt\5.15.2\msvc2019_64\bin
D:\Tensorrt\TensorRT-8.6.1.6\bin
D:\Tensorrt\TensorRT-8.6.1.6\lib
D:\opencv\opencv-4.12.0\opencv\build\x64\vc16\bin
C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.1\bin
```

## Configure

Run from a normal PowerShell or from the VS developer shell:

```powershell
cmd.exe /s /c '"D:\Visual Studio\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 && cmake -S . -B build_win_qt -G "Visual Studio 17 2022" -A x64 -DTENSORRT_ROOT="D:/Tensorrt/TensorRT-8.6.1.6" -DOpenCV_DIR="D:/opencv/opencv-4.12.0/opencv/build" -DQt5_DIR="D:/Qt/5.15.2/msvc2019_64/lib/cmake/Qt5" -DBUILD_QT_APP=ON'
```

## Build

```powershell
cmd.exe /s /c '"D:\Visual Studio\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 && cmake --build build_win_qt --config Release --target yolov11-tensorrt_qt_app'
```

Command-line targets can also be built:

```powershell
cmd.exe /s /c '"D:\Visual Studio\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 && cmake --build build_win_qt --config Release --target yolov11-tensorrt yolov11-tensorrt_obb yolov11-tensorrt_seg'
```

## Deploy Runtime DLLs

After building, run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\deploy_windows.ps1
```

This runs `windeployqt` and copies OpenCV, TensorRT, CUDA, and cuDNN runtime DLLs next to the Qt app executable.

## Run

Put model engines here:

```text
weights\best_obb.engine
weights\best_seg.engine
```

Then run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1
```

Or pass explicit engine paths:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1 -ObbEngine .\weights\best_obb.engine -SegEngine .\weights\best_seg.engine
```

The app executable is:

```text
build_win_qt\Release\yolov11-tensorrt_qt_app.exe
```

## Notes

- Build `Release` first. The OpenCV prebuilt package does not provide a usable debug DLL in this setup.
- TensorRT engine files are tied to GPU, CUDA, TensorRT, and platform details. Rebuild engines on the target Windows machine if loading fails.
- `CMakeLists.txt` adds `/utf-8` for MSVC so the Qt source files with UTF-8 UI strings compile correctly.
