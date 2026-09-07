# TankEye-Iris 2.1.6 编译、启动、打包流程

本文档按当前项目事实整理。项目事实以源码、CMake、配置、脚本和测试为准，不依赖 README 系列文件。后续读写本文档必须使用 UTF-8，避免中文乱码。

当前约定统一使用 `build` 目录；Visual Studio 生成器是多配置构建，Release 主程序位于：

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

如果 `build` 已经存在且配置正确，可以跳过本步。重新从零配置时先确认是否需要清理旧 build；清理目录属于高风险操作，按仓库规则需要先得到用户明确同意。

```powershell
cmake -S . -B build `
  -DBUILD_QT_APP=ON `
  -DOpenVINO_DIR="D:\Anaconda\envs\cll_yolo\Lib\site-packages\openvino\cmake" `
  -DOpenCV_DIR="D:\work_floder\jiezhifa\TankEye_source_for_new_pc\_deps\opencv\opencv\build\x64\vc16\lib" `
  -DQt5_DIR="D:\Qt\5.15.2\msvc2019_64\lib\cmake\Qt5"
```

配置成功后，`build` 目录中会有 CMake 生成的 Visual Studio 工程文件。

## 3. 编译 Release 程序

```powershell
cmake --build build --config Release --target tankeye-openvino_qt_app
cmake --build build --config Release --target tankeye-admin-auth-code
```

检查主程序是否存在：

```powershell
Test-Path .\build\Release\tankeye-openvino_qt_app.exe
```

返回 `True` 表示主程序已经编译完成。

## 4. 运行关键测试

只验证抓取后处理逻辑：

```powershell
cmake --build build --config Release --target tankeye-openvino_frame_postprocess_smoke
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release -Filter "*frame_postprocess_smoke*.exe"
```

运行全部默认测试：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release
```

看到以下输出表示默认测试全部通过：

```text
[TankEyeTests] All tests passed.
```

## 5. 从源码目录启动程序

常用调试启动命令：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1 -BuildDir build -Configuration Release -WindowMode Maximized -AutoLoadModels -DebugPostprocess -SimulatePlc
```

参数说明：

- `-BuildDir build`：使用统一的 `build` 编译目录。
- `-Configuration Release`：启动 `build\Release` 下的程序。
- `-WindowMode Maximized`：最大化窗口。
- `-AutoLoadModels`：自动加载当前工程方案默认的 OBB 和 SEG 模型，通常是 `models\DG_8_weights`。
- `-DebugPostprocess`：打开后处理调试日志，便于分析可抓/不可抓原因。
- `-SimulatePlc`：PLC 模拟模式，不真实写 PLC。

如需连接真实 PLC，不要加 `-SimulatePlc`，并确认 `config\tankeye.json` 中的 PLC 地址配置正确。连接真实 PLC、真实相机或真实设备动作必须先得到用户明确同意。

## 6. 可编辑配置

主配置文件：

```text
config\tankeye.json
```

顶部显示文案由 `app` 节控制：

```json
"app": {
  "name": "TankEye-Iris",
  "version": "2.1.6",
  "title": "截止阀抓取上料系统"
}
```

- 左上角显示：`name + V + version`，例如 `TankEye-Iris V2.1.6`。
- 顶部中间标题显示：`title`。
- 运行包内也有自己的 `config\tankeye.json`，重新打包或手动同步配置后才会在运行包中生效。
- 正式运行包的启动脚本会设置 `TANKEYE_LOCK_APP_DISPLAY=1`，普通现场测试只改运行包 `config\tankeye.json` 不会改变顶部显示名、版本号或标题；这些显示值由程序内置默认值控制。

## 7. 管理员授权

正式包必须使用 `config\admin_auth.key` 签发管理员授权文件。该文件不应提交到 GitHub；正式打包前必须存在，打包脚本会复制进运行包。

新机器首次创建管理员账号时，让现场在管理员授权弹窗中点击“保存授权申请”，选择保存位置和文件名。软件会生成类似下面的授权申请文件：

```text
TankEye_admin_license_request_TK-B57A-B3ED-5AA9-3F90.json
```

工程师拿到授权申请 JSON 后，在项目根目录生成本机 `admin_license.json`：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\generate_admin_auth_code.ps1 -RequestFile .\TankEye_admin_license_request_TK-B57A-B3ED-5AA9-3F90.json -Output .\config\admin_license.json
```

把生成的 `admin_license.json` 放入现场运行包 `config` 目录。普通检测不依赖管理员授权文件；没有有效授权文件时，不能创建管理员、不能登录管理员，也不能进入工程设置。

创建管理员账号时必须设置恢复问题和恢复答案。忘记管理员密码时，软件仍会检查本机 `admin_license.json`，并要求正确回答恢复问题；通过后才能重置账号密码。重置时 `新恢复问题` 和 `新恢复答案` 可以不填，不填会继续沿用原恢复问题和恢复答案；如果要更换恢复问题，必须两个都填。

## 8. 打包运行包

打包命令：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\package_runtime.ps1 -BuildDir build -ReleaseName TankEye-Iris_2.1.6 -Force
```

脚本会自动查找主程序，当前项目通常命中：

```text
build\Release\tankeye-openvino_qt_app.exe
```

成功后生成：

```text
dist\TankEye-Iris_2.1.6
dist\TankEye-Iris_2.1.6.zip
```

`dist\TankEye-Iris_2.1.6` 是可直接运行的文件夹，`dist\TankEye-Iris_2.1.6.zip` 是给新电脑拷贝使用的压缩包。运行包产物不纳入 GitHub。

## 9. 验证打包结果

检查关键文件：

```powershell
Test-Path .\dist\TankEye-Iris_2.1.6\tankeye-openvino_qt_app.exe
Test-Path .\dist\TankEye-Iris_2.1.6\tankeye-openvino_device_probe.exe
Test-Path .\dist\TankEye-Iris_2.1.6\platforms\qwindows.dll
Test-Path .\dist\TankEye-Iris_2.1.6\models\DG_8_weights\best_obb.xml
Test-Path .\dist\TankEye-Iris_2.1.6\models\DG_8_weights\best_seg.xml
Test-Path .\dist\TankEye-Iris_2.1.6\openvino_intel_cpu_plugin.dll
Test-Path .\dist\TankEye-Iris_2.1.6\openvino_intel_gpu_plugin.dll
Test-Path .\dist\TankEye-Iris_2.1.6\config\admin_auth.key
Test-Path .\dist\TankEye-Iris_2.1.6\USAGE_GUIDE.txt
Test-Path .\dist\TankEye-Iris_2.1.6\docs
Test-Path .\dist\TankEye-Iris_2.1.6\AGENTS.md
```

前 9 项按实际包内容应返回 `True`；`docs` 和 `AGENTS.md` 必须返回 `False`，表示运行包不包含源码文档和协作规则。

校验打包出的 exe 是否就是本次 build 的 exe：

```powershell
(Get-FileHash .\build\Release\tankeye-openvino_qt_app.exe).Hash -eq (Get-FileHash .\dist\TankEye-Iris_2.1.6\tankeye-openvino_qt_app.exe).Hash
(Get-FileHash .\build\Release\tankeye-openvino_device_probe.exe).Hash -eq (Get-FileHash .\dist\TankEye-Iris_2.1.6\tankeye-openvino_device_probe.exe).Hash
```

返回 `True` 表示一致。

## 10. 启动运行包

进入运行包目录：

```powershell
cd .\dist\TankEye-Iris_2.1.6
```

正常启动：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1
```

默认 `-Device AUTO` 会先运行 `tankeye-openvino_device_probe.exe` 预检 GPU；预检失败、超时或崩溃时，主程序使用 CPU。

运行包启动脚本会设置：

```powershell
$env:TANKEYE_SETTINGS_INI_PATH = .\config\engineering_settings.ini
```

因此工程设置保存在当前运行包目录内，不同版本运行包互不共用。

模拟 PLC + CPU 启动：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1 -Device CPU -SimulatePlc
```

也可以双击：

```text
launch_tankeye_main_only.vbs
```

## 11. 当前 2.1.6 关键行为

- 真实夹爪框已完全接管旧延长 OBB 框，不恢复 `extended_corners` / `ExtendedObb` / `ScaleObbLongEdge`。
- 真实夹爪长轴使用 1.0.14 语义：垂直 AC，宽度沿 AC。
- D508 左右判定以夹取 OBB 本地方向为基准，使用 A->C 作为本地前向。
- 四类型补偿已加入：大上右、小上左、大上左、小上右分别支持角度补偿和沿 AC 机械偏移。
- 普通显示补偿后姿态，全显保留 raw/原始对照。
- 主界面左侧图像区约 75%，右侧控制栏约 25%。
- 图像完整显示，允许边缘留白，不裁剪。
- 图片读取使用后台线程。
- OpenVINO 缓存默认保留；如需清理，启动时显式添加 `-ClearOpenVinoCache`。
- 运行日志默认带时间戳，可在程序内打开“运行日志”查看。
- 运行包不包含源码文档目录 `docs\`，也不包含协作规则文件 `AGENTS.md`。
- 运行包启动脚本会锁定顶部显示名、版本号和标题，避免现场通过普通配置文件改显示。

## 12. 开机自启合同

当前开机自启合同为：

```text
-StartupProfile AutoStart -StartupDelaySeconds N
```

含义：

- 自动加载模型。
- 模型加载完成后请求进入 PLC 抓取联动。
- 启动入口统一使用当前用户 Startup 文件夹中的 `TankEye-Iris.vbs`。
- 不使用注册表、服务或计划任务。
- 工程设置中可以关闭开机自启并调整延迟秒数。

源码手动启动如需加载默认模型，可用：

```powershell
.\launch_tankeye.ps1 -BuildDir build -Configuration Release -WindowMode Maximized -AutoLoadModels
```

`-AutoLoadModels` 只用于手动加载默认模型，不等价于开机自启，也不会自动进入抓取。

## 13. 常见问题

### 报错：Qt app executable not found

先检查 exe 是否存在：

```powershell
Test-Path .\build\Release\tankeye-openvino_qt_app.exe
```

如果返回 `False`，说明还没有编译主程序，先执行：

```powershell
cmake --build build --config Release --target tankeye-openvino_qt_app
```

### 报错：禁止运行脚本

不要直接执行：

```powershell
.\scripts\package_runtime.ps1
```

改用：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\package_runtime.ps1 -BuildDir build -ReleaseName TankEye-Iris_2.1.6 -Force
```

### 提示：VCINSTALLDIR is not set

这是 `windeployqt` 的常见警告，不一定是失败。只要最后出现 release directory、release zip，并且 zip 已生成，就说明打包成功。

### 中文路径导致图片加载失败或异常

Windows 下 OpenCV 直接读取中文路径可能不稳定。临时规避方法是把测试图片放到纯英文路径，并把图片文件名改成英文或数字。

### 管理员授权无效

确认 `config\admin_license.json` 存在、属于本机、由同一份 `config\admin_auth.key` 生成；MachineGuid、系统盘序列号和 Qt machineUniqueId 三项稳定身份必须匹配，BIOS 序列号如果授权申请中非空也必须匹配，MAC 只作辅助记录，不因网卡状态变化直接判整机不符。忘记密码时还需要回答创建管理员时设置的恢复问题，不能只凭授权文件直接重置。

### 打包后加载模型变慢

运行包第一次加载模型可能较慢，这是正常现象。OpenVINO 第一次加载 GPU/CPU 模型时会做模型编译并生成缓存。

缓存目录：

```text
dist\TankEye-Iris_2.1.6\openvino_cache
```

只要不删除该目录，第二次启动或第二次加载同一套模型通常会更快。

如现场确实需要手动清理 OpenVINO 缓存，可显式加参数：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1 -ClearOpenVinoCache
```

正常使用不要加这个参数。

## 14. 常用命令

日常修改代码后，常用验证顺序：

```powershell
cd D:\work_floder\jiezhifa\TankEye_source_for_new_pc
cmake --build build --config Release --target tankeye-openvino_qt_app tankeye-admin-auth-code tankeye-openvino_frame_postprocess_smoke
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release -Filter "*frame_postprocess_smoke*.exe"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release
```

需要打包时再执行：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\package_runtime.ps1 -BuildDir build -ReleaseName TankEye-Iris_2.1.6 -Force
```
