# TankEye-Iris 项目配置静态档案

更新时间：2026-08-03

本文档用于长期理解项目。内容基于当前源码、`CMakeLists.txt`、`config/tankeye.json`、脚本和测试整理；不以 README 作为事实来源。

## 项目定位

TankEye-Iris 是一个 Windows 工业视觉抓取上位机项目。核心链路是：相机采集图像，OBB/SEG 模型推理，后处理得到目标姿态与区域，坐标转换得到机械坐标，限位规则判断可抓性，最后通过 Modbus TCP 写入 PLC 寄存器。

## 技术栈

- 语言：C++17、PowerShell、Python。
- 构建：CMake 3.12+，Windows only，MSVC x64。
- GUI：Qt 5，组件使用 `Widgets`、`Network`、`Concurrent`。
- 视觉与推理：OpenCV、OpenVINO C++ Runtime。
- 工业硬件：Hikrobot MVS SDK，相机封装在 `app/qt/hardware`。
- PLC 通信：Qt Network + Modbus TCP 业务封装，寄存器合约在 workflow 层。
- 测试：CMake/CTest 注册的 C++ 可执行测试，辅助脚本为 `scripts/run_build_tests.ps1`。

## 目录结构

| 路径 | 职责 |
| --- | --- |
| `app/qt/main.cpp` | Qt 应用入口、日志初始化、窗口启动参数处理。 |
| `app/qt/ui/` | 主窗口、工程设置、运行状态展示、画面叠加、管理员授权 UI 辅助。 |
| `app/qt/workflow/` | 抓取工作流、配置服务、工程设置服务、后处理、坐标转换、限位、PLC 结果合约。 |
| `app/qt/hardware/` | Hikrobot 相机和机器人/PLC 控制封装。 |
| `app/cli/` | OBB、SEG 命令行推理入口。 |
| `core/inference/` | YOLOv11 OBB/SEG OpenVINO 推理实现与 OpenVINO 工具。 |
| `core/common/` | 部署、模型、宏和通用工具。 |
| `config/` | 默认运行配置、管理员授权密钥示例；真实 `admin_auth.key` 被忽略。 |
| `models/weights/` | OpenVINO 模型权重放置位置；实际模型文件被忽略。 |
| `scripts/` | 启动、打包、部署、测试、授权码、标定、桌面快捷方式脚本。 |
| `tests/` | C++ 单元/契约/冒烟测试。 |
| `runtime/` | 运行包使用说明素材。 |
| `docs/` | 构建、打包与协作档案。 |
| `build/`、`dist/`、`_deps/`、`vendor/hik_mvs/` | 本地生成物或私有依赖，通常不进入版本库。 |

## 构建目标

主要 CMake 目标：

- `tankeye-openvino_deploy_common`：通用部署/模型工具静态库。
- `tankeye-openvino_deploy_obb`：OBB 推理静态库。
- `tankeye-openvino_deploy_seg`：SEG 推理静态库。
- `tankeye-openvino_obb`、`tankeye-openvino_seg`：命令行推理程序。
- `tankeye-openvino_qt_app`：Qt 主程序。
- `tankeye-admin-auth-code`：管理员授权码工具。

测试目标覆盖配置读取、坐标转换、工程设置、后处理、限位过滤、PLC 合约、寄存器映射和 UI 状态展示。

## 配置与运行时输入

默认配置文件是 `config/tankeye.json`。当前配置版本为 `1`，包含：

- PLC：`host=192.168.3.205`、`port=502`、`unit_id=1`、超时、浮点字序和寄存器地址。
- PLC 寄存器：`photo_trigger=1500`、`front_back=500`、`left_right=502`、`angle=504`、`pick_status=506`、`head_type=508`。
- 机械限位：总开关、ROI 留边、前后/左右/角度上下限。
- 轴向映射：默认 `front_back_axis=machine_y`，前后/左右补偿。
- 角度校准：偏移、方向、范围模式。
- 相机：IP 与曝光。

常用环境变量由源码和脚本直接读取，包括：

- 配置与运行：`TANKEYE_CONFIG_PATH`、`TANKEYE_LOG_FILE`、`TANKEYE_WINDOW_MODE`、`TANKEYE_WINDOW_WIDTH`、`TANKEYE_WINDOW_HEIGHT`。
- 推理：`TANKEYE_OPENVINO_DEVICE`、`TANKEYE_OPENVINO_CACHE_DIR`。
- PLC：`TANKEYE_PLC_HOST`、`TANKEYE_PLC_PORT`、`TANKEYE_PLC_SIM`。
- 相机：`TANKEYE_CAMERA_IP`、`TANKEYE_CAMERA_FALLBACK`。
- 调试与 UI：`TANKEYE_DEBUG_POSTPROCESS`、`TANKEYE_UI_SCALE`。
- 管理员授权：`TANKEYE_ADMIN_AUTH_KEY_FILE`、`TANKEYE_ADMIN_AUTH_SECRET`。
- 测试覆盖：`TANKEYE_SETTINGS_INI_PATH`。

## 架构决策

- 项目仅维护 Windows 构建，非 Windows 在 CMake 配置阶段直接失败。
- GUI 与生产工作流放在 `app/qt`，推理通用能力放在 `core`，命令行入口与 Qt 主程序共享推理库。
- 现场可变参数优先走 JSON、QSettings 或环境变量覆盖，避免把现场值硬编码进业务逻辑。
- PLC 写入语义集中在结果合约和控制封装中，UI 调整不得改变寄存器含义。
- 抓取判断采用保守策略：坐标、结构、限位或目标数量不满足时，应拒绝而不是猜测。
- 构建产物、运行包、模型权重、私有 SDK、授权密钥和本地依赖不进入 Git。

## 标准验证入口

- 构建主程序：`cmake --build build --config Release --target tankeye-openvino_qt_app`
- 构建授权工具：`cmake --build build --config Release --target tankeye-admin-auth-code`
- 运行测试：`powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release`
- 打包：`powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\package_runtime.ps1 -BuildDir build -ReleaseName TankEye-Iris_1.2 -Force`

涉及多功能或多文件改动时，必须运行对应集成链路；涉及真实硬件、PLC 或 UI 的结论不能只用模拟替代。
