# TankEye-Iris 构建运行打包指南

## 适用场景

这个流程适用于你把 `build` 目录删掉之后，重新生成、编译、运行、打包项目。

## 当前环境路径

- OpenVINO CMake 配置目录：
  `D:\Anaconda\envs\cll_yolo\Lib\site-packages\openvino\cmake`
- OpenCV CMake 配置目录：
  `D:\work_floder\jiezhifa\TankEye_source_for_new_pc\_deps\opencv\opencv\build\x64\vc16\lib`

## 1. 重新生成 build

在项目根目录执行：

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64 `
  -DOpenVINO_DIR="D:\Anaconda\envs\cll_yolo\Lib\site-packages\openvino\cmake" `
  -DOpenCV_DIR="D:\work_floder\jiezhifa\TankEye_source_for_new_pc\_deps\opencv\opencv\build\x64\vc16\lib"
```

## 2. 编译程序

```powershell
cmake --build build --config Release --target tankeye-openvino_qt_app
```

编译成功后，主程序在：

```text
build\Release\tankeye-openvino_qt_app.exe
```

## 3. 运行程序

直接启动开发环境下的程序：

```powershell
.\launch_tankeye.ps1 -BuildDir build -Configuration Release -WindowMode Maximized -AutoLoadModels
```

## 4. 打包运行目录

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\package_runtime.ps1 -BuildDir build -ReleaseName TankEye-Iris_1.1 -Force
```

打包完成后，发布目录在：

```text
dist\TankEye-Iris_1.1
```

压缩包在：

```text
dist\TankEye-Iris_1.1.zip
```

## 5. 常见问题

### build 不存在

先执行第 1 步，不要直接跑 `cmake --build build`。

### OpenVINO 找不到

确认 `-DOpenVINO_DIR` 指向的是 `OpenVINOConfig.cmake` 所在目录，不是 dll 目录。

### OpenCV 找不到

确认 `-DOpenCV_DIR` 指向的是 `OpenCVConfig.cmake` 所在目录。

### 想重新来一遍

可以直接删掉 `build` 目录，然后从第 1 步重新执行。
