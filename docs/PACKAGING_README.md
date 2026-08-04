# TankEye-Iris 1.4 编译、启动、打包流程

本文档按当前项目约定整理。以后统一使用 `build` 目录，不再使用 `build_qt_codex`、`build_repackage` 等临时目录。Visual Studio 生成器是多配置构建，Release 程序生成在：

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

如果 `build` 已经存在且配置正确，可以跳过本步骤。重新从零开始时，先清理旧 build，然后执行：

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
cmake --build build --config Release --target tankeye-admin-auth-code
```

检查程序是否存在：

```powershell
Test-Path .\build\Release\tankeye-openvino_qt_app.exe
```

返回 `True` 说明主程序已经编译出来。

## 4. 编译并运行关键测试

只验证抓取后处理逻辑：

```powershell
cmake --build build --config Release --target tankeye-openvino_frame_postprocess_smoke
```

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release -Filter "*frame_postprocess_smoke*.exe"
```

运行全部测试：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release
```

看到：

```text
[TankEyeTests] All tests passed.
```

说明测试通过。

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

如果要连接真实 PLC，不要加 `-SimulatePlc`，并确认 `config\tankeye.json` 里的 PLC 地址配置正确。

## 6. 管理员授权码

正式包使用 `config\admin_auth.key` 验证管理员授权码。该文件不会提交到 GitHub，但如果存在，打包脚本会复制进运行包。

新机器首次创建管理员账号时，让对方复制软件显示的机器码，然后生成 `INIT` 授权码：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\generate_admin_auth_code.ps1 -MachineCode "TK-客户机器码" -Purpose INIT
```

忘记管理员密码时，用同一机器码生成 `RESET` 重置码：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\generate_admin_auth_code.ps1 -MachineCode "TK-客户机器码" -Purpose RESET
```

如果脚本提示使用开发默认密钥，说明没有找到 `config\admin_auth.key`，正式打包前需要先补齐密钥文件。

## 7. 打包运行包

打包命令：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\package_runtime.ps1 -BuildDir build -ReleaseName TankEye-Iris_1.4 -Force
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

## 8. 打包结果

成功后生成：

```text
dist\TankEye-Iris_1.4
dist\TankEye-Iris_1.4.zip
```

`dist\TankEye-Iris_1.4` 是可直接运行的文件夹，`dist\TankEye-Iris_1.4.zip` 是给新电脑拷贝用的压缩包。

## 9. 验证打包结果

检查关键文件：

```powershell
Test-Path .\dist\TankEye-Iris_1.4\tankeye-openvino_qt_app.exe
Test-Path .\dist\TankEye-Iris_1.4\platforms\qwindows.dll
Test-Path .\dist\TankEye-Iris_1.4\models\weights\best_obb.xml
Test-Path .\dist\TankEye-Iris_1.4\models\weights\best_seg.xml
Test-Path .\dist\TankEye-Iris_1.4\openvino_intel_cpu_plugin.dll
Test-Path .\dist\TankEye-Iris_1.4\openvino_intel_gpu_plugin.dll
Test-Path .\dist\TankEye-Iris_1.4\config\admin_auth.key
Test-Path .\dist\TankEye-Iris_1.4\USAGE_GUIDE.txt
Test-Path .\dist\TankEye-Iris_1.4\docs
Test-Path .\dist\TankEye-Iris_1.4\AGENTS.md
```

前 8 项应返回 `True`；`docs` 和 `AGENTS.md` 两项必须返回 `False`，说明运行包未包含源码文档和协作规则文件。

校验打包出的 exe 是否就是本次 build 的 exe：

```powershell
(Get-FileHash .\build\Release\tankeye-openvino_qt_app.exe).Hash -eq (Get-FileHash .\dist\TankEye-Iris_1.4\tankeye-openvino_qt_app.exe).Hash
```

返回 `True` 表示一致。

## 10. 启动打包后的程序

进入运行包目录：

```powershell
cd .\dist\TankEye-Iris_1.4
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

## 11. 当前 1.4 行为说明

- 主界面比例为左侧图像区约 75%、右侧控制栏约 25%。
- 右侧栏采用双列布局，并随窗口尺寸自适应。
- 图像完整显示，允许边缘留白，不使用居中裁剪。
- “加载图片”使用后台线程读取。
- 当前目标 X/Y 有机械坐标时优先显示机械坐标；PLC 写入仍使用机械坐标。
- 真实夹爪框由工程设置中的夹爪长度/宽度和九点标定换算得到，负责可抓/不可抓状态显示、碰撞拒抓、抓取射线 C 点和中心偏移。
- 旧 3 倍延长 OBB 框已从运行逻辑和画面显示中移除。
- 运行日志全部带时间戳。
- 界面内“运行日志”支持最新日志、自动刷新、搜索、级别过滤、时间过滤和分页。
- 普通模式只显示目标列表；管理员模式登录后显示隐藏/全显、工程设置、运行日志等调试入口。
- 首次创建管理员需要 `INIT` 授权码；忘记密码重置需要 `RESET` 重置码。
- OpenVINO 缓存目录默认为运行包内 `openvino_cache`，正常启动不会删除缓存。

## 12. 常见问题

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
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\package_runtime.ps1 -BuildDir build -ReleaseName TankEye-Iris_1.4 -Force
```

### 提示：VCINSTALLDIR is not set

这是 `windeployqt` 的警告，不一定是失败。只要最后出现：

```text
[Package] Release directory: ...
[Package] Release zip: ...
```

并且 `dist\TankEye-Iris_1.4.zip` 已生成，就说明打包成功。

### 图片中文路径导致加载失败或异常

Windows 下 OpenCV 直接读取中文路径可能不稳定。临时规避方法是把测试图片放到纯英文路径，并把图片文件名改成英文或数字。

### 管理员授权码无效

确认机器码是从软件里复制的完整机器码；首次创建使用 `-Purpose INIT`，忘记密码重置使用 `-Purpose RESET`；生成码的电脑和打包运行包使用同一个 `config\admin_auth.key`。

### 打包后加载模型变慢

运行包第一次加载模型可能会慢，这是正常现象。OpenVINO 第一次加载 GPU/CPU 模型时，会做模型编译并生成缓存。

运行包会把缓存保存在：

```text
dist\TankEye-Iris_1.4\openvino_cache
```

只要不删除这个目录，第二次启动、第二次加载同一套模型，通常会比第一次快。

如果现场确实需要手动清理 OpenVINO 缓存，可以显式加参数：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1 -ClearOpenVinoCache
```

正常使用不要加这个参数。

### 首次加载图片仍然感觉卡

图片读取已经改为后台线程；如果刚启动程序就加载图片，后台 OpenVINO 模型编译可能正在占用 CPU/GPU 资源，导致首次图片显示、缩放和渲染体感变慢。模型编译完成或缓存命中后会明显好转。

### 打包后 Device AUTO 的行为

当前运行包会把 `-Device AUTO` 原样传给程序。

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
```

其中 `Compile model ms` 如果第一次很大、第二次明显变小，说明缓存正在正常生效。

## 13. 最常用的一套命令

日常修改代码后，直接按顺序执行：

```powershell
cd D:\work_floder\jiezhifa\TankEye_source_for_new_pc
cmake --build build --config Release --target tankeye-openvino_qt_app tankeye-admin-auth-code tankeye-openvino_frame_postprocess_smoke
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release -Filter "*frame_postprocess_smoke*.exe"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\package_runtime.ps1 -BuildDir build -ReleaseName TankEye-Iris_1.4 -Force
```
