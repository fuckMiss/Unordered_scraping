# TankEye-Iris 编译、启动、打包流程

本文档按当前项目约定整理：以后统一只使用 `build` 目录。

不要再使用 `build_qt_codex`、`build_repackage` 这类临时目录。Visual Studio 生成器是多配置构建，所以 Release 程序会生成在：

```text
build\Release\tankeye-openvino_qt_app.exe
```

不是：

```text
build\tankeye-openvino_qt_app.exe
```

## 1. 进入项目目录

```powershell
cd D:\work_floder\jiezhifa\TankEye_source_for_new_pc
```

## 2. 首次配置 build

如果 `build` 已经存在且配置正确，可以跳过本步骤。重新从零开始时，先删除旧的 `build`，然后执行：

```powershell
cmake -S . -B build `
  -DBUILD_QT_APP=ON `
  -DOpenVINO_DIR="D:\Anaconda\envs\cll_yolo\Lib\site-packages\openvino\cmake" `
  -DOpenCV_DIR="D:\work_floder\jiezhifa\TankEye_source_for_new_pc\_deps\opencv\opencv\build\x64\vc16\lib" `
  -DQt5_DIR="D:\Qt\5.15.2\msvc2019_64\lib\cmake\Qt5"
```

配置成功后，`build` 目录里会有 CMake 生成的 Visual Studio 工程文件。

## 3. 编译 Release 程序

```powershell
cmake --build build --config Release --target tankeye-openvino_qt_app
```

编译完成后检查程序是否存在：

```powershell
Test-Path .\build\Release\tankeye-openvino_qt_app.exe
```

返回 `True` 就说明主程序已经编译出来了。

## 4. 编译并运行关键测试

如果只想验证这次抓取后处理逻辑：

```powershell
cmake --build build --config Release --target tankeye-openvino_frame_postprocess_smoke
```

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release -Filter "*frame_postprocess_smoke*.exe"
```

如果想跑全部测试：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release
```

看到：

```text
[TankEyeTests] All tests passed.
```

就说明测试通过。

## 5. 从源码目录启动程序

常用调试启动命令：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1 -BuildDir build -Configuration Release -WindowMode Maximized -AutoLoadModels -DebugPostprocess -SimulatePlc
```

参数说明：

- `-BuildDir build`：使用统一的 `build` 编译目录。
- `-Configuration Release`：启动 `build\Release` 下的程序。
- `-WindowMode Maximized`：最大化窗口。
- `-AutoLoadModels`：自动加载 `models\weights` 下的 OBB 和 SEG 模型。
- `-DebugPostprocess`：打开后处理调试日志，方便看为什么可抓或不可抓。
- `-SimulatePlc`：PLC 模拟模式，不真实写 PLC。

如果要连真实 PLC，就不要加 `-SimulatePlc`，并确认 `config\tankeye.json` 里的 PLC 地址配置正确。

## 6. 打包运行包

打包命令：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\package_runtime.ps1 -BuildDir build -ReleaseName TankEye-Iris_1.1 -Force
```

脚本会自动从下面这些位置查找主程序：

```text
build\tankeye-openvino_qt_app.exe
build\Release\tankeye-openvino_qt_app.exe
build\RelWithDebInfo\tankeye-openvino_qt_app.exe
build\MinSizeRel\tankeye-openvino_qt_app.exe
```

当前项目一般会命中：

```text
build\Release\tankeye-openvino_qt_app.exe
```

## 7. 打包结果

成功后会生成：

```text
dist\TankEye-Iris_1.1
dist\TankEye-Iris_1.1.zip
```

`dist\TankEye-Iris_1.1` 是可直接运行的文件夹，`dist\TankEye-Iris_1.1.zip` 是给新电脑拷贝用的压缩包。

## 8. 验证打包结果

检查关键文件：

```powershell
Test-Path .\dist\TankEye-Iris_1.1\tankeye-openvino_qt_app.exe
Test-Path .\dist\TankEye-Iris_1.1\platforms\qwindows.dll
Test-Path .\dist\TankEye-Iris_1.1\models\weights\best_obb.xml
Test-Path .\dist\TankEye-Iris_1.1\models\weights\best_seg.xml
Test-Path .\dist\TankEye-Iris_1.1\openvino_intel_cpu_plugin.dll
Test-Path .\dist\TankEye-Iris_1.1\openvino_intel_gpu_plugin.dll
```

都返回 `True`，说明运行包的核心文件齐了。

也可以校验打包出来的 exe 是否就是本次 build 的 exe：

```powershell
(Get-FileHash .\build\Release\tankeye-openvino_qt_app.exe).Hash -eq (Get-FileHash .\dist\TankEye-Iris_1.1\tankeye-openvino_qt_app.exe).Hash
```

返回 `True` 表示一致。

## 9. 启动打包后的程序

进入运行包目录：

```powershell
cd .\dist\TankEye-Iris_1.1
```

正常启动：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1
```

模拟 PLC 启动：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1 -Device CPU -SimulatePlc
```

也可以双击：

```text
launch_tankeye_main_only.vbs
```

## 10. 常见问题

### 报错：Qt app executable not found

先检查 exe 是否存在：

```powershell
Test-Path .\build\Release\tankeye-openvino_qt_app.exe
```

如果返回 `False`，说明还没编译主程序，先执行：

```powershell
cmake --build build --config Release --target tankeye-openvino_qt_app
```

如果返回 `True` 但打包仍报错，确认你运行的是新版脚本：

```powershell
Select-String -Path .\scripts\package_runtime.ps1 -Pattern "Build output"
```

能搜到 `[Package] Build output` 就是新版脚本。

### 报错：禁止运行脚本

不要直接执行：

```powershell
.\scripts\package_runtime.ps1
```

改用：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\package_runtime.ps1 -BuildDir build -ReleaseName TankEye-Iris_1.1 -Force
```

### 提示：VCINSTALLDIR is not set

这是 `windeployqt` 的警告，不一定是失败。只要最后出现：

```text
[Package] Release directory: ...
[Package] Release zip: ...
```

并且 `dist\TankEye-Iris_1.1.zip` 已生成，就说明打包成功。

### 图片中文路径导致加载慢或闪退

当前已知风险：Windows 下 OpenCV 直接读取中文路径可能不稳定。临时规避方法是把测试图片放到纯英文路径，并把图片文件名改成英文或数字。

### 打包后加载模型变慢

运行包第一次加载模型可能会慢，这是正常现象。OpenVINO 第一次加载 GPU/CPU 模型时，会做模型编译并生成缓存。

运行包会把缓存保存在：

```text
dist\TankEye-Iris_1.1\openvino_cache
```

只要不删除这个目录，第二次启动、第二次加载同一套模型，通常会比第一次快。

之前的旧打包脚本有一个问题：运行包每次启动都会删除 `openvino_cache`，等于每次都强制重新编译模型，所以会感觉“加载模型、打开相机、加载图片都变慢”。现在已经改成默认保留缓存。

如果现场确实需要手动清理 OpenVINO 缓存，可以显式加参数：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1 -ClearOpenVinoCache
```

正常使用不要加这个参数。

### 打包后 Device AUTO 的行为

当前运行包会把 `-Device AUTO` 原样传给程序，不再由启动脚本强行先尝试 GPU。

常用启动：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1 -Device AUTO
```

强制 CPU：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1 -Device CPU
```

强制 GPU：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1 -Device GPU
```

如果怀疑模型加载慢，优先查看最新日志里的这些字段：

```text
[OpenVINO] Cache dir:
[OpenVINO] Requested device:
[OpenVINO] Selected device:
[OpenVINO] Compile model ms:
[ImagePerf]
```

其中 `Compile model ms` 如果第一次很大、第二次明显变小，说明缓存正在正常生效。

## 11. 最常用的一套命令

日常修改代码后，直接按顺序执行：

```powershell
cd D:\work_floder\jiezhifa\TankEye_source_for_new_pc
cmake --build build --config Release --target tankeye-openvino_qt_app tankeye-openvino_frame_postprocess_smoke
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release -Filter "*frame_postprocess_smoke*.exe"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\package_runtime.ps1 -BuildDir build -ReleaseName TankEye-Iris_1.1 -Force
```
