# TankEye-Iris

TankEye-Iris 是一套 Windows 工业视觉抓取上位机。它要解决的是把现场的一次抓取决策稳定地闭环：

```text
物料图像 -> 目标与姿态 -> 图像坐标 -> 机械坐标 -> 抓取合法性 -> PLC 寄存器
```

因此项目的核心约束只有几件事：相机必须给出可信图像，模型必须给出可信目标，坐标转换必须把像素落到机械坐标系，限位规则必须拒绝危险目标，PLC 合约必须保持稳定。

## 系统边界

输入：

- 海康 Hikrobot 工业相机图像，调试时可用 OpenCV camera 0 回退。
- OBB 模型：`models/weights/best_obb.xml` 和 `best_obb.bin`。
- SEG 模型：`models/weights/best_seg.xml` 和 `best_seg.bin`。
- 现场配置：`config/tankeye.json`、工程设置窗口保存的 `QSettings`、九点标定配置。
- PLC 拍照触发寄存器。

输出：

- 主界面叠加显示目标、分割区域、角度、头部类型、抓取状态和运行状态。
- PLC Modbus TCP 寄存器写入：前后、左右、角度、抓取状态、头部类型。
- 运行日志：默认写到运行目录 `logs/tankeye_*.log`。

## 主运行链路

1. 启动 `launch_tankeye.ps1` 或 `launch_tankeye_main_only.vbs`。
2. 启动脚本准备 DLL 搜索路径、OpenVINO 设备、模型路径、日志路径、PLC 覆盖参数、窗口参数和相机 IP。
3. `tankeye-openvino_qt_app.exe` 启动 Qt 主界面，读取 `config/tankeye.json`。
4. `GraspWorkflow` 加载 OBB/SEG OpenVINO 模型。
5. 相机线程从 Hikrobot 相机取帧；设置 `TANKEYE_CAMERA_FALLBACK=1` 时，Hikrobot 不可用会回退到 OpenCV camera 0。
6. 每帧依次执行 OBB 推理、SEG 推理和后处理。
7. 后处理只把结构上可解释的目标交给抓取逻辑：OBB 的 `left` 是夹取位，`big/small` 是头部辅助点，SEG 提供物料区域。
8. 九点标定把主目标中心从图像 `X/Y` 转成机械 `X/Y`。
9. 机械 ROI、上下限和角度规则过滤不可抓目标。
10. 抓取模式下，PLC 触发拍照后，程序写入结果寄存器并清除拍照触发。

## 第一性原则

- **坐标系是核心事实。** 模型输出的是像素，机械执行需要机器坐标。九点标定生成单应矩阵，至少需要 4 组有效点，建议现场使用完整 9 点。
- **PLC 合约比界面更稳定。** UI 可以调整显示方式，但 `D500/D502/D504/D506/D508` 的语义必须与 PLC 程序一致。
- **拒绝优先于误抓。** 没有唯一主目标、没有有效坐标、超出 ROI/限位、结构不合法时，系统应写拒绝状态，而不是猜一个位置。
- **配置是现场事实，代码是默认值。** `config/tankeye.json` 是默认配置层；工程设置窗口保存的值会覆盖可调项；环境变量只用于临时调试或部署覆盖。
- **运行包要自洽。** 目标 PC 不应依赖源码；运行包需要带齐 EXE、模型、配置、Qt/OpenCV/OpenVINO/Hikrobot 运行时和必要工具。

## PLC 合约

默认映射来自 `config/tankeye.json`。现场改地址后，以 JSON 为准。

| 地址 | 含义 |
| --- | --- |
| `D1500` | 拍照触发。PLC 写 `1`，视觉处理完成后清 `0`。 |
| `D500` | 前后轴。默认写机械 `Y`，可切换为机械 `X`。 |
| `D502` | 左右轴。默认写机械 `X`，可切换为机械 `Y`。 |
| `D504` | 校准后的机械旋转角度。 |
| `D506` | 抓取状态：`1` 单个可抓，`2` 多个可抓，`3` 不可抓/拒绝。 |
| `D508` | 头部类型：`0` 未知，`1..4` 为识别类型。 |

拒绝目标时，程序只写 `D506=3`，不会覆盖前后、左右、角度和头部类型寄存器。

## 配置层级

程序默认读取运行目录：

```text
config/tankeye.json
```

可用 `TANKEYE_CONFIG_PATH` 指向另一份 JSON。常用环境变量：

| 变量 | 作用 |
| --- | --- |
| `TANKEYE_CONFIG_PATH` | 覆盖配置文件路径。 |
| `TANKEYE_PLC_HOST` | 覆盖 PLC IP。 |
| `TANKEYE_PLC_PORT` | 覆盖 PLC 端口。 |
| `TANKEYE_PLC_SIM` | `1/true/yes/on` 启用 PLC 模拟。 |
| `TANKEYE_OPENVINO_DEVICE` | `AUTO`、`GPU` 或 `CPU`。 |
| `TANKEYE_DEBUG_POSTPROCESS` | `1` 打开后处理调试日志。 |
| `TANKEYE_LOG_FILE` | 指定日志文件。 |
| `TANKEYE_CAMERA_FALLBACK` | `1` 允许相机回退到 OpenCV camera 0。 |

`tankeye.json` 是默认层；工程设置窗口保存的 `QSettings` 会覆盖相机、限位、轴向映射、补偿和角度等现场可调项；环境变量适合临时启动和排障。

## 工程设置

- 相机 IP、曝光值、九点坐标转换、限位、轴向映射、补偿和角度校准会保存，下次打开复用。
- `曝光 us` 可手动填写，也可使用 `单次自动曝光`。
- `上下限保护` 使用机械坐标过滤目标；`ROI 留边` 会把边界向内收缩，避免抓取边界附近物料。
- `轴向映射` 可选择 `前后=机械Y，左右=机械X` 或 `前后=机械X，左右=机械Y`，同时影响 PLC `D500/D502`、机械 ROI 和限位检查。
- `前后补偿`、`左右补偿` 只叠加到最终 PLC 输出和 PLC 测试显示，不影响九点矩阵、机械 ROI 或限位判断。
- 角度校准公式：

```text
mechanical_angle = image_angle * direction + offset
```

角度范围可选择 `0~360` 或 `-180~180`。例如在 `-180~180` 下，`270` 会输出为 `-90`。

## 九点标定

运行包内双击：

```text
nine_point_circle_picker.exe
```

从运行包目录启动时，输出到：

```text
calibration_output/calibration_image_points.txt
```

点位按成功点击顺序写入：第 1 个成功点是 `P1`，第 2 个是 `P2`。只有点到黑色圆圈内部或足够贴近圆圈才算成功；点工具条或空白区域不会占用编号；可用顶部 `撤销` 删除上一个点。

## 构建

项目仅维护 Windows 构建。主要依赖：

- CMake 3.12+
- MSVC x64
- Qt 5.15.2 msvc2019_64
- OpenCV
- OpenVINO C++ Runtime/Dev
- Hikrobot MVS SDK。源码包若本地存在 `vendor/hik_mvs` 会优先使用，但该厂商 SDK 不建议提交到公开 Git 仓库。

常用构建命令：

```powershell
cmake -S . -B build
cmake --build build --config Release
```

从源码启动：

```powershell
.\launch_tankeye.ps1 -Configuration Release -BuildDir build -WindowMode Maximized
```

源码路径含中文或非 ASCII 字符时，启动脚本默认不自动传模型参数；需要自动加载模型时，优先使用 ASCII-only 路径或运行包。

## 测试

构建后运行测试：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release
```

当前 CMake 注册的测试覆盖：

- 配置读取：`app_config_service_test`
- 九点坐标转换：`coordinate_transform_test`
- 工程设置：`engineering_settings_service_test`
- 后处理冒烟：`frame_postprocess_smoke`
- 限位过滤：`grab_limit_evaluator_test`
- PLC 输出合约：`plc_result_contract_test`
- PLC 寄存器映射：`robot_controller_register_map_test`
- UI 状态展示：`runtime_status_presenter_test`、`plc_runtime_state_test`

## 部署与打包

把构建产物部署到 `build/Release`：

```powershell
.\scripts\deploy_windows.ps1 -Configuration Release -BuildDir build
```

生成独立运行包：

```powershell
.\scripts\package_runtime.ps1 -BuildDir build -ReleaseName TankEye-Iris_1.1 -Force
```

输出目录：

```text
dist/TankEye-Iris_1.1
dist/TankEye-Iris_1.1.zip
```

运行包包含主程序、模型、配置、标定输出、示例图、运行时 DLL、启动脚本、桌面图标脚本和使用说明。它不应包含源码、CMake 工程、Python 脚本、测试、`.lib`、`.pdb`、`.pt` 等开发产物。

上传 GitHub 时，建议不要提交 `dist/`、`_deps/`、`vendor/hik_mvs/`、`samples/Data/` 和 `models/weights/` 中的实际模型文件；模型文件可通过 GitHub Releases、Git LFS 或私有部署渠道分发。

创建桌面图标：

```powershell
.\scripts\create_desktop_shortcut.ps1
```

如果运行包文件夹改名或移动位置，需要重新创建桌面图标。

## 目录地图

| 路径 | 职责 |
| --- | --- |
| `app/qt/main.cpp` | Qt 程序入口、日志初始化、窗口启动。 |
| `app/qt/ui/` | 主界面、工程设置窗口、状态展示和画面叠加。 |
| `app/qt/hardware/` | Hikrobot 相机封装、PLC Modbus TCP 控制。 |
| `app/qt/workflow/` | 抓取工作流、配置服务、坐标转换、限位、PLC 输出合约和后处理。 |
| `app/cli/` | OBB/SEG 命令行推理入口。 |
| `core/inference/` | YOLOv11 OBB/SEG OpenVINO 推理封装。 |
| `core/common/` | 模型、部署和通用工具。 |
| `config/` | 默认现场配置。 |
| `models/weights/` | OpenVINO 模型权重。 |
| `scripts/` | 启动、部署、打包、测试、标定和桌面图标脚本。 |
| `tests/` | C++ 测试。 |
| `runtime/` | 运行包使用说明素材。 |
| `vendor/hik_mvs/` | 随仓库携带的 Hikrobot MVS 头文件、库和运行时。 |
| `dist/` | 已生成的运行包。 |

## 排障入口

- 程序打不开：先看 `logs/tankeye_*.log`。
- 找不到模型：确认 `models/weights/best_obb.xml` 和 `models/weights/best_seg.xml` 是否存在。
- 找不到相机：检查 MVS 驱动、相机供电、网段、相机 IP；调试时可设置 `TANKEYE_CAMERA_FALLBACK=1`。
- GPU 启动失败：先用 `-Device CPU` 验证主链路，再检查 Intel GPU 驱动和 OpenVINO 运行时。
- PLC 不联机：先用 `-SimulatePlc` 验证视觉链路，再检查 PLC IP、端口、寄存器地址和网线。
- 目标坐标异常：优先检查九点标定点顺序、机械坐标录入、单应矩阵有效性和 ROI/轴向映射。
