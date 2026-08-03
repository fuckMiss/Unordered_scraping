# TankEye-Iris 任务历史

本文件用于记录每次任务结束后的摘要。每次任务完成时追加一条，保留结果、修改范围、验证结果和遗留问题。

## 记录格式

```markdown
## YYYY-MM-DD - 任务标题

- 目标：本次要解决什么。
- 修改：列出关键文件或模块级改动。
- 结果：说明已达成的行为。
- 验证：真实执行过的构建、测试、实机验证或未验证原因。
- 遗留：未完成事项、风险、后续建议。
```

## 2026-08-03 - 建立项目协作档案与记忆体系

- 目标：根据当前源码、CMake、配置、脚本和测试建立长期项目档案、任务历史、动态记忆和操作限制。
- 修改：新增 `AGENTS.md` 与 `docs/project-memory/` 文档体系；根目录旧交接文档归档到 `docs/project-memory/archive/`。
- 结果：后续会话可以从静态项目档案、动态记忆、历史记录、交接文件和操作限制入口继续工作。
- 验证：本次是文档重构；完成后检查 Git 状态和文档文件存在性。未运行构建或业务测试。
- 遗留：当前仍存在既有业务代码改动 `app/qt/ui/grasp_main_window.cpp`、`launch_tankeye.ps1`，本次未审查也未回滚。

## 2026-08-03 - 第一批代码清理：抽离运行日志对话框

- 目标：降低 `app/qt/ui/grasp_main_window.cpp` 的职责和体积，先从低耦合 UI 辅助功能开始拆分。
- 修改：新增 `app/qt/ui/runtime_log_dialog.h/.cpp`；`GraspMainWindow::showRuntimeLogs()` 改为调用独立函数；`CMakeLists.txt` 注册新文件；删除原函数中不可达的旧版日志查看器代码。
- 结果：运行日志查看器逻辑从主窗口大文件中独立出来，主窗口保留最小入口，后续可继续按模块拆分。
- 验证：已成功构建 `tankeye-openvino_qt_app`；已构建 Release 全部目标；`scripts/run_build_tests.ps1` 默认测试全部通过；`frame_postprocess_smoke` 单独通过；已用 `launch_tankeye.ps1 -Device CPU -SimulatePlc` 真实启动程序并生成运行日志。
- 遗留：本轮未完成自动点击“运行日志”弹窗的桌面 UI 检查；既有 `launch_tankeye.ps1` 缩放改动仍未单独审查。

## 2026-08-03 - 第二批代码清理：抽离管理员账号弹窗

- 目标：继续降低 `GraspMainWindow` UI 职责，把管理员创建、登录、重置弹窗从主窗口大文件中拆出。
- 修改：新增 `app/qt/ui/admin_auth_dialogs.h/.cpp`；主窗口保留薄包装和账号存储/校验逻辑；`CMakeLists.txt` 注册新模块；管理员弹窗样式、表单标签和复制提示迁入新模块。
- 结果：管理员弹窗 UI 与主窗口业务状态解耦，后续主窗口更适合继续按顶部栏、侧栏、目标列表等方向拆分。
- 验证：已成功构建 `tankeye-openvino_qt_app`；已构建 Release 全部目标；`scripts/run_build_tests.ps1` 默认测试全部通过；`frame_postprocess_smoke` 单独通过。
- 遗留：需要真实打开管理员创建/登录/重置弹窗，确认复制机器码、授权码失败提示、密码显示按钮和记住密码行为。

## 2026-08-03 - 第二批补充：合并管理员弹窗重复逻辑

- 目标：回应“代码能合并就合并、能减少就减少”的维护目标，减少第二批拆分后产生的重复代码。
- 修改：创建/重置管理员弹窗合并为一个内部通用授权码表单函数；提取通用表单行、密码行、网格配置；新增 `app/qt/ui/ui_scale_utils.h` 复用 UI 缩放工具，移除日志和管理员弹窗模块中的重复缩放函数。
- 结果：`admin_auth_dialogs.cpp` 从 402 行降到 364 行，`runtime_log_dialog.cpp` 从 297 行降到 280 行；重复弹窗结构收敛为一次实现、多处调用。
- 验证：已成功构建 `tankeye-openvino_qt_app`；`scripts/run_build_tests.ps1` 默认测试全部通过；`frame_postprocess_smoke` 单独通过。
- 遗留：仍需真实 UI 点检管理员弹窗与运行日志弹窗。

## 2026-08-03 - 全项目瘦身第一批：合并 OBB/SEG CLI 重复入口

- 目标：面向整个项目执行瘦身优化，优先合并低风险、高重复的命令行推理入口。
- 修改：新增 `app/cli/cli_inference_runner.h`，集中处理通用参数解析、图片/视频输入、推理计时、GUI 显示和输出保存；`main_obb.cpp`、`main_seg.cpp` 只保留模型类型、输出前缀和 SEG 专属参数差异。
- 结果：OBB/SEG CLI 重复主流程收敛为一个模板化 runner，后续修改 CLI 输入、计时、GUI 行为只需改一处。
- 验证：已构建 `tankeye-openvino_obb`、`tankeye-openvino_seg`；已构建 Release 全部目标；`scripts/run_build_tests.ps1` 默认测试全部通过；`frame_postprocess_smoke` 单独通过。
- 遗留：CLI 空参/坏参返回码符合预期，但当前工具未捕获到 stderr 文本；后续如要发布 CLI，建议补一个可自动断言 usage/参数错误输出的测试。

## 2026-08-03 - 全项目瘦身第二批：合并 CMake、管理员存储与工程设置 helper

- 目标：按“能合并就合并、能减少就减少”的原则继续全项目瘦身，处理 CMake 样板、管理员账号存储和工程设置表单构造。
- 修改：`CMakeLists.txt` 新增 `add_tankeye_test()` 统一测试目标样板；管理员账号 QSettings/哈希/记住密码逻辑并入 `admin_auth_helpers`；新增 `admin_auth_helpers_test`；工程设置弹窗提取标准 grid、列配置和分散标签 helper。
- 结果：测试目标维护入口减少；`GraspMainWindow` 不再保存管理员账号存储细节；工程设置弹窗重复 grid 构造减少。当前总 diff 约 `+431/-1234`，净减少约 803 行。
- 验证：Release 全部目标构建通过；`scripts/run_build_tests.ps1` 默认测试全部通过并包含新增 `admin_auth_helpers_test`；`frame_postprocess_smoke` 单独通过；使用 CPU + 模拟 PLC 真实启动 Qt 程序并生成新日志。
- 遗留：真实启动只确认程序拉起和日志生成，未人工点击工程设置/管理员/运行日志弹窗逐项检查；顶部栏本批只做实机启动确认，未继续改布局算法。

## 2026-08-03 - 全项目瘦身第三批：合并顶部栏与目标卡重复逻辑

- 目标：继续按小批次做真正瘦身，减少 `GraspMainWindow` 中重复 UI 初始化和目标数据显示维护点；用户指出仅合并顶部栏按钮代码量收益不够后，补充压缩目标卡片重复刷新逻辑。
- 修改：`app/qt/ui/grasp_main_window.cpp` 新增内部 `CreateIconToolButton()` 与 `RefreshIconToolButton()`，复用顶部栏按钮初始化、尺寸和静态图标刷新；新增 `FormatHeadType()`；`createTargetCard()` 改为直接填充 `TargetCard` 结构，移除 7 个 `QLabel*&` 输出参数；新增 `refreshTargetCard()` 统一目标列表字段赋值。`app/qt/ui/grasp_main_window.h` 前移 `TargetCard` 结构并简化方法签名。
- 结果：顶部栏按钮和目标列表刷新逻辑都收敛为单点维护；本轮未新增文件，属于原文件内重复代码合并。当前 `grasp_main_window.cpp/.h` 累计 diff 为 `+161/-908`。
- 验证：`cmake --build build --config Release --target tankeye-openvino_qt_app` 通过；`powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；模拟 PLC 真实启动 Qt 程序，主窗口创建、显示、进入事件循环并正常退出，日志为 `build/Release/logs/codex_target_card_smoke_20260803_123043.log`。
- 遗留：本轮只做启动冒烟，没有截图或人工点击目标列表、最小化/最大化/关闭按钮；管理员、工程设置、运行日志弹窗逐项点检仍待执行。

## 2026-08-03 - 上传 GitHub 并校准动态文档

- 目标：把当前项目源码和协作文档上传到用户确认的 GitHub V5 分支，并确认文档状态与当前仓库状态一致。
- 修改：先提交并推送瘦身与协作文档改动到 `origin/Unordered_Scraping_V5`；随后修正 `MEMORY.md`、`HANDOFF.md` 中推送前遗留的“未提交改动/脏工作区”描述。
- 结果：本地分支和 `origin/Unordered_Scraping_V5` 保持同步；project-memory 文档不再描述已过期的未提交状态。
- 验证：推送前检查 `.gitignore` 与 staged 文件，未纳入 `build/`、`dist/`、`_deps/`、`vendor/`、模型权重或真实 `config/admin_auth.key`；`git status --short --branch`、`git rev-parse HEAD`、`git rev-parse origin/Unordered_Scraping_V5`、`git diff HEAD origin/Unordered_Scraping_V5` 已确认 V5 与本地一致。
- 遗留：README 系列仍按协作规则视为可能滞后，不能作为唯一事实来源；UI 弹窗逐项点检仍待执行。

## 2026-08-03 - 上下限保护区域可视化

- 目标：在主画面可视化显示扣除 ROI 留边后的最终可抓区域，并把五角色评审流程沉淀为仓内可复用文档。
- 修改：新增机械坐标反投影能力；新增限位保护区多边形构建逻辑；`frame_overlay` 支持低透明保护区绘制；工程设置新增 `显示保护区域` 开关并持久化；新增 `frame_overlay_test`；新增 `docs/project-memory/AI_ROLE_WORKFLOW.md`。
- 结果：上下限保护启用、显示开关开启且坐标转换有效时，主图像会叠加显示最终可抓区；坐标转换无效或 ROI 留边后区域为空时不画假区域。
- 验证：`cmake --build build --config Release --target tankeye-openvino_qt_app` 通过；`cmake --build build --config Release --target tankeye-openvino_frame_overlay_test` 通过；`scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过并包含 `frame_overlay_test`；模拟 PLC 真实启动 Qt 程序通过，日志为 `build/Release/logs/codex_grab_limit_overlay_smoke_20260803_144745.log`。
- 遗留：本轮未人工加载图片或相机画面观察保护区叠层；真实现场坐标转换和保护区位置仍需结合有效九点标定人工确认。

## 2026-08-03 - 坐标转换导入与应用按钮升级

- 目标：把坐标转换方案导入和矩阵计算放回工程设置应用内；删除单独 `计算矩阵` 按钮，让 `应用` 按钮直接读取当前九点编辑区并刷新状态提示；修复 `另存为` 弹窗字体/背景反差。
- 修改：`calibration_profile_store` 新增按文件路径导入并与现有按名称加载共用严格 JSON 解析；支持 VisionMaster TXT 五列导入（图像X、图像Y、机械X、机械Y、角度；矩阵只使用前四列，角度列校验但不参与计算）；工程设置新增 `导入` 按钮；`应用` 改为计算当前编辑区并刷新原有坐标转换状态提示；下拉切换只载入已保存方案；`另存为` 改为高反差自定义命名对话框；根据截图反馈压缩点位表列间空白，并移除重复矩阵显示框；新增 `calibration_profile_store_test`。
- 结果：导入只填入编辑区，不自动保存为正式方案；点击 `应用` 后通过原有状态提示显示有效/无效和平均重投影误差；缺字段、点数错误、模糊 TXT 或非有限数会拒绝导入，避免默默变 0。
- 验证：`cmake --build build --config Release --target tankeye-openvino_calibration_profile_store_test` 通过；`cmake --build build --config Release --target tankeye-openvino_qt_app` 通过；`powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；`powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1 -Device CPU -SimulatePlc` 启动真实 Qt 程序，模拟 PLC 日志 `build/Release/logs/tankeye_20260803_153259.log` 显示主窗口创建、显示、进入事件循环并退出码 0。
- 遗留：本轮未连接真实 PLC/相机；已按用户提供的 `calibration_output/26.08.02ninepoint.txt` 格式补充 VM 五列解析测试，但仍建议现场人工在工程设置导入真实文件并确认视觉效果。

## 2026-08-03 - 延长夹爪只按真实 SEG Mask 碰撞拒抓

- 目标：废弃“延长夹爪 OBB 碰到旁边物料 bbox/minAreaRect 就拒抓”的旧规则，改为只有扫到旁边物料真实 SEG mask 白色像素时才判不可抓。
- 修改：`frame_postprocess` 使用 `ExtendedObbTouchesSegmentMask()` 按延长夹爪 OBB ROI 与旁边 `segment.mask` 做二值重叠检测，`countNonZero(overlap) > 0` 才碰撞；空 mask、尺寸异常、ROI 为空不再 fallback 到 bbox/minRect 拒抓；`PoseDetection` 新增 `mask_collisions` 保存真实重叠 ROI 小 mask；调试 overlay 在 `show_all_detections` 时用半透明红色显示重叠像素，普通主画面不显示；`ScaleResultForDisplay()` 同步缩放碰撞 mask；补充后处理和 overlay 测试。
- 结果：自身所属 segment 继续忽略；其他可抓逻辑、坐标转换、上下限保护和 PLC 写入规则不变；debug reason 从 `extended_touches_other_segment` 改为 `extended_touches_other_mask`，bbox/minRect 仅保留显示辅助含义，不参与拒抓。
- 验证：`cmake --build build --config Release --target tankeye-openvino_frame_postprocess_smoke` 通过；`cmake --build build --config Release --target tankeye-openvino_frame_overlay_test` 通过；`cmake --build build --config Release --target tankeye-openvino_qt_app` 通过；`scripts/run_build_tests.ps1 -BuildDir build -Configuration Release -Filter tankeye-openvino_frame_postprocess_smoke.exe` 通过；`scripts/run_build_tests.ps1 -BuildDir build -Configuration Release -Filter tankeye-openvino_frame_overlay_test.exe` 通过；`scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；`launch_tankeye.ps1 -Device CPU -SimulatePlc` 已真实启动 Qt 程序，日志 `build/Release/logs/tankeye_20260803_175948.log` 确认模拟 PLC 启用、主窗口创建/显示并进入 Qt event loop。
- 遗留：本轮未连接真实 PLC/真实相机，未用现场真实画面人工观察红色碰撞像素；模拟 PLC 启动因 Qt 事件循环常驻超时，随后关闭进程，未发现残留 `tankeye` 进程。

## 2026-08-03 - 放大夹取 OBB 碰撞归属修正

- 目标：保持当前夹取 OBB 中心放大形状和倍率不变，修正误判为“只有当前目标放大夹取 OBB 内存在旁边物料自身 mask 外露像素时，才拒当前目标”。
- 修改：`ExtendedObbTouchesSegmentMask()` 新增 matched segment mask 参数，先计算 `当前放大 OBB mask ∩ 旁边 segment.mask`，再扣除 `matched_segment.mask`，最终 overlap 非空才写入 `mask_collisions` 并拒抓；不改 `BuildExtendedObbCorners()`，不引入新 UI 开关。
- 结果：旁边物料 bbox/minAreaRect/显示框碰到当前目标 mask 不再导致当前目标不可抓；旁边 mask 与当前 mask 重叠但都落在当前自身 mask 内时不拒抓；A 的放大框扫到 B 的外露 mask 时只拒 A，不连带拒 B。
- 验证：`cmake --build build --config Release --target tankeye-openvino_frame_postprocess_smoke` 通过；`scripts/run_build_tests.ps1 -BuildDir build -Configuration Release -Filter tankeye-openvino_frame_postprocess_smoke.exe` 通过；`cmake --build build --config Release --target tankeye-openvino_frame_overlay_test` 通过；`cmake --build build --config Release --target tankeye-openvino_qt_app` 通过；`scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；`launch_tankeye.ps1 -Device CPU -SimulatePlc` 已真实启动 Qt 程序，日志 `build/Release/logs/tankeye_20260803_182210.log` 确认模拟 PLC 启用、主窗口创建/显示并进入 Qt event loop。
- 遗留：本轮未连接真实 PLC/真实相机，未在真实现场画面中人工复核用户截图里的中间目标；模拟 PLC 启动因 Qt 事件循环常驻超时，验证后关闭进程，未发现残留 `tankeye` 进程。

## 2026-08-03 - 普通显示按 SEG 去重 OBB 框

- 目标：面向现场展示时隐藏同一 SEG 中的重复误检 OBB，只让普通主画面和普通目标列表每个 SEG 显示一个代表目标；调试全显仍保留全部误检框用于排查。
- 修改：`frame_overlay` 新增 `SelectNormalDisplayDetectionIndices()`，按 `matched_segment_index` 做显示级去重，代表目标优先级为主目标、可抓目标、置信度最高；普通 overlay 使用该选择结果绘制，`show_all_detections=true` 分支继续绘制全部 detections 和 raw OBB；`GraspMainWindow::refreshTargetTable()` 普通模式复用同一选择函数，全显模式仍显示全部 detection。
- 结果：不改变后处理安全逻辑、PLC 写入、主目标选择、坐标转换或上下限保护；同一 SEG 里实际存在多个 left/头部时仍可按结构异常拒抓，但普通画面不再把多个误检框都展示给现场用户。
- 验证：`cmake --build build --config Release --target tankeye-openvino_frame_overlay_test` 通过；`scripts/run_build_tests.ps1 -BuildDir build -Configuration Release -Filter tankeye-openvino_frame_overlay_test.exe` 通过；`cmake --build build --config Release --target tankeye-openvino_qt_app` 通过；`scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；`launch_tankeye.ps1 -Device CPU -SimulatePlc` 已真实启动 Qt 程序，日志 `build/Release/logs/tankeye_20260803_184719.log` 确认模拟 PLC 启用、主窗口创建/显示并进入 Qt event loop。
- 遗留：本轮未连接真实 PLC/真实相机，未在现场图片上人工切换普通/全显确认视觉差异；模拟 PLC 启动因 Qt 事件循环常驻超时，验证后关闭进程，未发现残留 `tankeye` 进程。
