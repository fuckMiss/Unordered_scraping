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
