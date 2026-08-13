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

## 2026-08-04 - 回到 GitHub V5 基线

- 目标：按用户要求放弃 2026-08-04 本地 AC/绿色框/工程设置角度校准试改，重新从 GitHub 拉回之前上传的 V5 版本，然后后续在干净 V5 基线上重新改。
- 修改：执行 `git fetch origin` 后，将当前分支硬回退到 `origin/Unordered_Scraping_V5`，当前 HEAD 为 `8698d85a33b9c1bdb18ff9b790b91265429504fe`；未跟踪参考脚本 `models/推理v1.0.13.py` 未删除。
- 结果：已跟踪源码回到 GitHub V5；按协作规则，本记录和 `MEMORY.md` 作为新的项目记忆改动保留。
- 验证：`cmake --build build --config Release --target tankeye-openvino_frame_postprocess_smoke` 通过；`cmake --build build --config Release --target tankeye-openvino_qt_app` 通过；`scripts/run_build_tests.ps1 -BuildDir build -Configuration Release -Filter tankeye-openvino_frame_postprocess_smoke.exe` 通过；`scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；`launch_tankeye.ps1 -BuildDir build -Device CPU -SimulatePlc` 已启动 Qt 程序并生成 `build/Release/logs/tankeye_20260804_140811.log`，因 GUI 事件循环常驻导致命令超时，随后关闭本次启动进程并确认无残留 `tankeye` 进程。
- 遗留：未连接真实 PLC/真实相机；下一步角度修正需要重新按五角色流程从 V5 基线审查和实施。

## 2026-08-04 - 新增提交信息说明规则

- 目标：按用户要求，为后续每次提交增加明确说明规则，避免提交注释看不出相对上一版发生了什么变化。
- 修改：`AGENTS.md` 和 `docs/project-memory/OPERATING_LIMITS.md` 新增提交信息规范；`MEMORY.md` 同步记录该长期协作规则。
- 结果：后续提交信息必须说明本次解决的问题、主要改动范围和关键验证结果，禁止只写笼统的 `update`、`fix`、`change`。
- 验证：文档改动已检查目标文件可读；未运行构建，因本轮不改源码。
- 遗留：无。

## 2026-08-04 - 按 AC 射线替换抓取角度定义

- 目标：根据 `models/推理v1.0.13.py` 的角度思路，在 V5 基线上把最终抓取角度从旧红色 A 框自身 OBB keep-point 射线改为 `SEG 中心 O -> A -> AC` 夹爪射线，减少 OBB 长短边和方向翻转造成的角度误判。
- 修改：`app/qt/workflow/frame_postprocess.cpp` 新增 AC 几何求解：同一 SEG 内 big/small B 候选按 `|∠AOB - 90°|` 最小择优，置信度只作并列 tie-break；红色 A 框按最佳 B 长边/短边中旋转幅度更小的方向对齐，再沿长边保持现有 3.0 倍放大；最终角度、overlay 箭头、PLC 中心偏移和真实 mask 碰撞检测都使用该对齐放大框；无 B、无有效 SEG minRect、OA/AC 退化时拒抓且不回退旧角度。`tests/frame_postprocess_smoke.cpp` 更新旧 keep-point 断言为 AC 语义，覆盖多 B 择优、缺 B 拒抓、AC 中心偏移和保守交叉拒抓。
- 结果：PLC D504 仍走现有角度校准/范围/反向链路，但输入视觉角度已改为 AC 射线；D508 左右改为按 A.x 与 SEG 中心 O.x 判定；碰撞安全逻辑保留真实 SEG mask 重叠规则，只是碰撞框与新 AC 对齐方向一致。
- 验证：`cmake --build build --config Release --target tankeye-openvino_frame_postprocess_smoke` 通过；`cmake --build build --config Release --target tankeye-openvino_frame_overlay_test` 通过；`cmake --build build --config Release --target tankeye-openvino_qt_app` 通过；`powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release -Filter tankeye-openvino_frame_postprocess_smoke.exe` 通过；`powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；`powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1 -BuildDir build -Device CPU -SimulatePlc` 已启动真实 Qt 程序并生成 `build/Release/logs/tankeye_20260804_143746.log`，日志确认模拟 PLC 启用、主窗口创建/显示并进入 Qt event loop；命令因 GUI 常驻超时后确认无残留 `tankeye` 进程。
- 遗留：未连接真实 PLC/真实相机，未在现场真实画面人工复核 AC 箭头、D504 角度和 D508 类型；Release 测试脚本可执行链路已通过，但现有 C++ smoke 主要沿用 `assert` 风格，后续若要强化自动断言建议补充非 `NDEBUG` 受影响的测试宏。

## 2026-08-04 - 修正 AC 抓取框显示与安全框候选方向

- 目标：解决用户指出的两个问题：普通画面原始抓取 OBB 没有跟随 3 倍夹取安全框一起变化，以及右上角类似目标的绿色可抓取安全框方向明显不正常。
- 修改：`app/qt/workflow/frame_postprocess.cpp` 中 `PoseDetection::corners` 改为保存 AC 对齐后的基础抓取 OBB，模型 raw OBB 继续只在全显/调试 raw overlay 使用；AC 对齐候选从“B 长边/B 短边中旋转幅度最小”改为两种方案都试算，优先选择 AC 垂足落在长边上的夹爪几何，再用 AC 点积和旋转幅度兜底；PostprocessDebug 增加 `ac_uses_long_edge`、`ac_dot`、raw/target/final 角度和 `ac_angle_deg` 字段，便于区分显示混源和角度算法问题。
- 结果：普通画面基础抓取框、3 倍安全框、AC 箭头、D504 视觉角度、中心偏移和真实 mask 碰撞检测现在来自同一套 AC 几何；右上角这类目标不再仅因“最小旋转”把安全框长轴选到不正常方向。
- 验证：`cmake --build build --config Release --target tankeye-openvino_frame_postprocess_smoke` 通过；`powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release -Filter tankeye-openvino_frame_postprocess_smoke.exe` 通过；`cmake --build build --config Release --target tankeye-openvino_frame_overlay_test` 通过；`cmake --build build --config Release --target tankeye-openvino_qt_app` 通过；`powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；`powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1 -BuildDir build -Device CPU -SimulatePlc` 已启动真实 Qt 程序并生成 `build/Release/logs/tankeye_20260804_150600.log`，日志确认模拟 PLC 启用、主窗口创建/显示并进入 Qt event loop；命令因 GUI 常驻超时后确认无残留 `tankeye` 进程。
- 遗留：未连接真实 PLC/真实相机；本轮未自动重新加载用户截图对应现场图片做视觉截图复核，仍需人工重新加载 `20260802163109_337_188.bmp` 或同源图片确认右上角绿色安全框、D504 和 D508。

## 2026-08-04 - 接入 AC 角度排查日志到工程设置调试开关

- 目标：按用户要求，让工程设置中打开“启用后处理调试日志”后，日志能显示 AC 角度排查所需字段，便于现场测试判断框歪来自 B 点选择、候选轴选择还是 AC 方向。
- 修改：`app/qt/workflow/frame_postprocess.cpp` 在 `[PostprocessDebug] target` 行补充 O/A/B/C、arrow_start、B 点 `∠AOB` 与 90°差值、AC 是否使用长边、AC 点积、raw/target/final 长边角度和最终 `ac_angle_deg`；`app/qt/ui/engineering_settings_dialog.cpp` 更新“启用后处理调试日志”提示，说明会输出 O/A/B/C、AC角度、B点选择和可抓/拒抓原因。
- 结果：无需新增开关；用户只要在工程设置打开现有后处理调试日志，重新加载图片后即可在运行日志中查看新增 AC 诊断字段。
- 验证：`cmake --build build --config Release --target tankeye-openvino_frame_postprocess_smoke` 通过；`cmake --build build --config Release --target tankeye-openvino_qt_app` 通过；`powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release -Filter tankeye-openvino_frame_postprocess_smoke.exe` 通过；`powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；`powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1 -BuildDir build -Device CPU -SimulatePlc` 已启动真实 Qt 程序并生成 `build/Release/logs/tankeye_20260804_151451.log`，日志确认模拟 PLC 启用、主窗口创建/显示并进入 Qt event loop；命令因 GUI 常驻超时后确认无残留 `tankeye` 进程。
- 遗留：本轮启动验证未加载真实图片；新增字段需用户打开调试开关并加载现场图后在运行日志中复核。

## 2026-08-04 - 工程设置角度校准幂等与检测稳态

- 目标：修复工程设置中“取当前角度 -> 输入机器目标角度 -> 计算校准”连续点击会继续改变角度偏移的问题；同时在不缓存同图结果的前提下减少同一图片重复检测时角度、X、Y 的小波动。
- 修改：`engineering_settings_dialog.cpp` 新增角度校准快照，程序写入计算结果时不刷新基准偏移；`plc_result_contract` 新增可测的 `CalculateAngleCalibrationOffset()`；`YOLOv11_OBB.cpp`、`YOLOv11_SEG.cpp`、`frame_postprocess.cpp` 增加稳定 tie-break 和最终位姿中心/角度 0.01 精度量化；`frame_postprocess_smoke`、`plc_result_contract_test` 增加幂等和稳态断言。
- 结果：同一组当前显示角、机器目标角、方向和范围下，重复点击“计算校准”会得到相同偏移；重复检测仍真实重跑模型，但 OBB/SEG 输出、B 候选、主目标选择和最终 UI/PLC 位姿值对近似并列结果更稳定。
- 验证：`cmake --build build --config Release --target tankeye-openvino_plc_result_contract_test` 通过；`cmake --build build --config Release --target tankeye-openvino_frame_postprocess_smoke` 通过；`powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release -Filter tankeye-openvino_plc_result_contract_test.exe` 通过；`powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release -Filter tankeye-openvino_frame_postprocess_smoke.exe` 通过；`cmake --build build --config Release --target tankeye-openvino_frame_overlay_test` 通过；`cmake --build build --config Release --target tankeye-openvino_obb` 通过；`cmake --build build --config Release --target tankeye-openvino_seg` 通过；`cmake --build build --config Release --target tankeye-openvino_qt_app` 通过；`powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；`powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1 -BuildDir build -Device CPU -SimulatePlc` 已真实启动 Qt 程序并生成 `build/Release/logs/tankeye_20260804_153721.log`，命令因 GUI 常驻事件循环在 30 秒超时，检查无残留 `tankeye` 进程。
- 遗留：未连接真实 PLC/真实相机；重复检测仍受模型后端数值差异影响，本轮只减少近似并列排序和浮点尾差造成的可见抖动，现场图片仍建议由用户复测确认。

## 2026-08-04 - 角度校准改为零偏移绝对重算

- 目标：按用户澄清修正工程设置角度校准语义，重新计算时旧角度偏移必须视为 0，偏移值应等于“机器目标角度 - 当前零偏移显示角度”。
- 修改：`engineering_settings_dialog.cpp` 的“取当前角度”改为用原始视觉角、当前反向/范围和 `angle_offset_deg = 0` 生成当前角度；方向或范围变化时，如当前角来自一次抓取结果，会按原始视觉角和零偏移刷新显示角；`CalculateAngleCalibrationOffset()` 改为只接收当前角和目标角，不再接收旧偏移；`plc_result_contract_test` 更新为 `102.25 -> 90.00` 得到 `-12.25`，并覆盖归一化。
- 结果：同一当前角和目标角连点“计算校准”结果不变；修改当前角、目标角、反向或范围后会按零偏移绝对公式重算，不会把上一次偏移继续叠加进去；最终 PLC 写入仍沿用现有 `ApplyPlcAngleCalibration()`。
- 验证：`cmake --build build --config Release --target tankeye-openvino_plc_result_contract_test` 通过；`cmake --build build --config Release --target tankeye-openvino_qt_app` 通过；`powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release -Filter tankeye-openvino_plc_result_contract_test.exe` 通过；`powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；`powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1 -BuildDir build -Device CPU -SimulatePlc` 已启动 Qt 程序并生成 `build/Release/logs/tankeye_20260804_155627.log`，GUI 常驻导致 30 秒超时，检查无残留 `tankeye` 进程。
- 遗留：未连接真实 PLC/真实相机；本轮未人工点击工程设置对话框验证，需要用户在 UI 中按现场流程复测“取当前角度/计算校准”。

## 2026-08-04 - 修正正向/反向校准偏移不变化

- 目标：修复用户发现的“正向计算一个角度偏移后，切到反向重新计算，偏移值仍然一样”的问题；正反向应基于当前模式下的零偏移视觉角分别求偏移。
- 修改：`plc_result_contract` 新增 `CalculateAngleCalibrationOffsetFromRawAngle()`，用原始视觉角、反向开关、角度范围和零偏移先求当前显示角，再计算目标差值；`engineering_settings_dialog.cpp` 的计算按钮改为优先使用最近一次“取当前角度”的原始视觉角重新计算，计算时会同步刷新当前角度框；`plc_result_contract_test` 新增 raw=30、target=90 时正向偏移 60、反向偏移 -240 的断言。
- 结果：正向/反向不再只是依赖当前角度输入框是否刷新，而是直接参与偏移计算；除 0/180 等反向后等价角度外，切换正反向重新计算偏移会变化。
- 验证：`cmake --build build --config Release --target tankeye-openvino_plc_result_contract_test` 通过；`cmake --build build --config Release --target tankeye-openvino_qt_app` 通过；`powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；`powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1 -BuildDir build -Device CPU -SimulatePlc` 已启动 Qt 程序并生成 `build/Release/logs/tankeye_20260804_162431.log`，GUI 常驻导致 30 秒超时，检查无残留 `tankeye` 进程。
- 遗留：未连接真实 PLC/真实相机；仍需用户在工程设置界面用现场流程复测“正向取角/计算、切反向/再计算”的实际显示值。

## 2026-08-04 - 校准输入框保持手动值但计算尊重正反向

- 目标：按用户确认的第二种交互方式修正工程设置角度校准：手动输入当前角度后，切换正向/反向或点击计算时，当前角度输入框不应自动从 30 变成 -30，但偏移计算仍要按正反向生效。
- 修改：`engineering_settings_dialog.cpp` 移除正反向/范围切换时自动刷新当前角度框的逻辑，计算按钮不再回写当前角度框；内部仍使用原始视觉角或手动输入角作为 raw angle，结合当前正反向和范围调用 `CalculateAngleCalibrationOffsetFromRawAngle()`；`plc_result_contract_test` 增加截图口径用例，验证 raw=30、target=95、`-180~180` 下正向偏移 65、反向偏移 125。
- 结果：界面显示不打扰用户输入，计算结果仍能体现顺时针/逆时针方向切换；用户手动输入 30 后切反向，框里保持 30，但点击计算会按内部 -30 参与计算并得到 125。
- 验证：`cmake --build build --config Release --target tankeye-openvino_plc_result_contract_test` 通过；`cmake --build build --config Release --target tankeye-openvino_qt_app` 通过；`powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；`powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1 -BuildDir build -Device CPU -SimulatePlc` 已启动 Qt 程序并生成 `build/Release/logs/tankeye_20260804_163707.log`，GUI 常驻导致 30 秒超时，检查无残留 `tankeye` 进程。
- 遗留：未连接真实 PLC/真实相机；仍需用户在工程设置界面人工确认输入框不跳变和偏移值变化符合现场预期。

## 2026-08-04 - OpenVINO OBB/SEG 推理公共逻辑瘦身

- 目标：按“真正瘦身优化”要求清洗重复代码，减少 OBB/SEG 两套推理类里相同的 OpenVINO 输入预处理、warmup 和输出布局解析维护点，不改变检测、PLC、UI 行为。
- 修改：`core/inference/openvino_utils.h/.cpp` 新增 `PrepareNchwLetterboxInput()` 统一完成 letterbox NCHW buffer 构建和 `InferRequest` 输入 Tensor 绑定；新增 `WarmupInferRequest()` 统一模型 warmup；新增 `ParseDetectionOutputLayout()` 统一解析 YOLO 输出张量中“属性维/候选维/类别数”的布局。`YOLOv11_OBB.cpp` 和 `YOLOv11_SEG.cpp` 删除各自重复实现，改为调用这些公共 helper。
- 结果：OBB/SEG 以后不再各维护一份相同预处理和 warmup 代码；输出布局判断从两处重复分支收敛到一处，后续模型输出形状兼容规则改动只需要看 `openvino_utils`。本轮没有新增功能开关，没有改模型后处理语义，没有连接真实 PLC/相机。
- 验证：`cmake --build build --config Release --target tankeye-openvino_obb` 通过；`cmake --build build --config Release --target tankeye-openvino_seg` 通过；`cmake --build build --config Release --target tankeye-openvino_qt_app` 通过；`powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；`powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1 -BuildDir build -Device CPU -SimulatePlc` 已启动 Qt 程序并生成 `build/Release/logs/tankeye_20260804_164915.log`，GUI 常驻导致 30 秒超时，随后确认无残留 `tankeye` 进程。
- 遗留：本轮是低风险推理公共代码清洗，只做模拟 PLC 启动和自动化测试；未连接真实 PLC、真实相机，未加载现场实时画面做人工视觉复核。

## 2026-08-04 - GraspMainWindow 工程设置职责拆分

- 目标：回应 `app/qt/ui/grasp_main_window.cpp` 超过三千行、维护困难的问题，先把与主窗口绘制弱相关的工程设置加载/保存职责拆出，降低主文件体量和阅读噪音；本轮不改变 UI、检测、PLC 语义。
- 修改：新增 `app/qt/ui/grasp_main_window_settings.cpp`，承接 `load*Settings()`、`save*Settings()`、`plcOutputConfig()`、轴向标签和坐标转换状态重建等工程设置相关成员函数；`grasp_main_window.cpp` 删除对应实现并清理部分已不用的 Qt include；`CMakeLists.txt` 将新文件加入 Qt app 源列表。尝试拆管理员 UI 时发现其依赖主窗口运行时缩放函数 `UiScale()/SC()`，为避免复制缩放逻辑和改变 UI 表现，本轮未拆管理员块。
- 结果：`grasp_main_window.cpp` 从约 3198 行降到约 3038 行，工程设置持久化细节集中到 155 行的新文件；这是一批低风险职责拆分，不是最终清洗，后续仍应继续处理 PLC/检测后台流程和响应式缩放耦合。
- 验证：`cmake --build build --config Release --target tankeye-openvino_qt_app` 通过；`powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；`powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1 -BuildDir build -Device CPU -SimulatePlc` 已启动 Qt 程序并生成 `build/Release/logs/tankeye_20260804_170243.log`，GUI 常驻导致 30 秒超时，随后确认无残留 `tankeye` 进程。
- 遗留：未人工点击工程设置界面，未连接真实 PLC/真实相机；下一批建议先把主窗口运行时缩放状态正式抽成共享 helper，再拆管理员 UI 或顶部/侧栏布局，避免复制 `UiScale()/SC()`。

## 2026-08-04 - 汇总当前角度修正、校准稳态与瘦身改动并上传 GitHub

- 目标：按用户要求把当前已验证改动整理成可复用提示词口径，并上传到 GitHub `origin/Unordered_Scraping_V5`。
- 修改：本记录补充本次上传范围；提交范围包括 AC 抓取角度修正、工程设置角度校准幂等/零偏移/正反向口径、重复检测稳态、OpenVINO OBB/SEG 公共逻辑瘦身、`GraspMainWindow` 工程设置职责拆分、项目记忆更新，以及参考脚本 `models/推理v1.0.13.py`。
- 验证：上传前已完成 `tankeye-openvino_obb`、`tankeye-openvino_seg`、`tankeye-openvino_qt_app` Release 构建，默认测试脚本全部通过，模拟 PLC 启动生成 `build/Release/logs/tankeye_20260804_170243.log` 且无残留 `tankeye` 进程；`git diff --check` 无空白错误，仅提示工作区文件下次 Git 操作会按 CRLF 处理。
- 遗留：本次上传不包含被 `.gitignore` 排除的 `build/`、模型权重和 `config/admin_auth.key`；未连接真实 PLC/真实相机。

## 2026-08-04 - 工程设置新增真实夹爪长宽可视化

- 目标：按用户确认的第一阶段需求，只在主画面绘制真实夹爪机械长宽换算后的图像矩形，用于观察效果；不改变现有延长 OBB 判断、真实 SEG mask 碰撞拒抓、可抓状态或 PLC 写入。
- 修改：`EngineeringSettingsService` 的 `UiOverlaySettings` 新增 `mechanical_gripper_length` 和 `mechanical_gripper_width` 持久化；工程设置“调试设置”新增“夹爪长度/夹爪宽度”输入框；后处理在 `PoseDetection::grip_long_angle_deg` 显式输出与延长 OBB 同源的 AC 对齐抓取长轴角度；`GraspMainWindow` 在显示刷新时根据当前检测的原始夹取 OBB 中心、`grip_long_angle_deg` 和现有九点坐标转换的局部机械/图像比例生成 `mechanical_gripper_corners`，不依赖未来可能删除的绿色延长框或从框边反推角度；`frame_overlay` 用区别于原有延长框的蓝青色额外绘制真实夹爪框；打开现有“启用后处理调试日志”后，会输出 `[PostprocessDebug] mechanical_gripper` 或 `mechanical_gripper_skip` 行，包含长宽、中心、检测角度、抓取长轴角度、局部比例、像素长宽、四角点或跳过原因。旧 `extended_corners`、mask 碰撞和后处理拒抓逻辑未改。
- 结果：夹爪长宽默认 0 时不绘制；启用有效坐标转换并填写正数长宽后，主画面会在当前角度方向上叠加真实夹爪尺寸对应的图像矩形，中心仍取原始夹取 OBB 中心。
- 验证：`cmake --build build --config Release --target tankeye-openvino_engineering_settings_service_test` 通过；`cmake --build build --config Release --target tankeye-openvino_frame_overlay_test` 通过；`cmake --build build --config Release --target tankeye-openvino_qt_app` 通过；补充日志和修正方向来源后再次构建 `tankeye-openvino_frame_postprocess_smoke`、`tankeye-openvino_qt_app` 和 `tankeye-openvino_frame_overlay_test` 通过；`scripts/run_build_tests.ps1 -BuildDir build -Configuration Release -Filter tankeye-openvino_frame_postprocess_smoke.exe` 通过；`powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；`powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1 -BuildDir build -Device CPU -SimulatePlc` 启动真实 Qt 程序并生成 `build/Release/logs/tankeye_20260804_175624.log`，GUI 常驻导致 45 秒超时，随后确认无残留 `tankeye` 进程；`git diff --check` 无空白错误，仅提示 CRLF。
- 遗留：未连接真实 PLC/真实相机；未人工加载现场图片观察真实夹爪框效果。下一步需要用户在工程设置输入真实夹爪长宽、启用有效坐标转换后，用现场图或相机画面确认蓝青色夹爪框尺寸与方向是否匹配真实夹具。

## 2026-08-04 - 真实夹爪框接管碰撞拒抓

- 目标：用工程设置中的真实夹爪机械长宽生成图像矩形，替代旧 3 倍 `extended_corners` 作为碰撞拒抓范围；旧延长框仅保留为全显/调试对比几何。
- 修改：`frame_postprocess` 移除旧延长 OBB mask 碰撞对 `can_grab` 的影响，只继续输出 AC 姿态、`grip_long_angle_deg` 和 `extended_corners`；`grab_limit_evaluator` 新增真实夹爪角点生成和 mask 碰撞过滤，按 `机械夹爪框 ∩ 旁边 SEG mask ∩ 非当前自身 mask` 写入 `mask_collisions` 并统一重算主目标/抓取状态；`frame_processing_service` 在 `ApplyCoordinateTransform()` 后执行真实夹爪过滤；图片检测和 PLC 触发路径传入夹爪长宽与调试日志开关；普通 overlay 不再画旧延长框，全显/调试模式继续画旧框对比。
- 结果：夹爪长宽为 0、坐标转换禁用/无效、局部比例或自身 mask 无效时保守判不可抓；真实夹爪框生成参数、跳过/拒抓原因、碰撞 segment、像素长宽和四角点会在启用后处理调试日志时打印。
- 验证：`cmake --build build --config Release --target tankeye-openvino_frame_postprocess_smoke`、`tankeye-openvino_frame_overlay_test`、`tankeye-openvino_grab_limit_evaluator_test`、`tankeye-openvino_qt_app` 均通过；`powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；`powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1 -BuildDir build -Device CPU -SimulatePlc` 启动真实 Qt 程序并生成 `build/Release/logs/tankeye_20260804_182109.log`，命令因 GUI 常驻 45 秒超时，随后确认无残留 `tankeye` 进程。
- 遗留：未连接真实 PLC/真实相机；仍需用户用现场图片或相机画面复核真实夹爪框尺寸、方向和拒抓效果是否符合实际夹具。

## 2026-08-04 - 真实夹爪框状态颜色修正

- 目标：修正真实夹爪框接管旧延长框拒抓职责后，overlay 颜色没有继承原“是否可抓/主目标”状态色的问题。
- 修改：`frame_overlay` 将真实夹爪框颜色从固定蓝青色改为复用 `DetectionStateColor()`；旧延长框在全显/调试模式下也继续使用同一状态色；`frame_overlay_test` 增加真实夹爪框可抓和不可抓状态颜色断言。
- 结果：真实夹爪框现在按原状态语义显示：主目标使用主目标色，可抓显示绿色，不可抓显示红色。
- 验证：`cmake --build build --config Release --target tankeye-openvino_frame_overlay_test` 通过；`cmake --build build --config Release --target tankeye-openvino_qt_app` 通过；`powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过。
- 遗留：未连接真实 PLC/真实相机，需用户在现场画面确认颜色观感与原延长框一致。

## 2026-08-04 - 真实夹爪框完全接管旧延长框

- 目标：删除旧 3 倍延长 OBB 框的字段、生成、缩放、显示、调试日志和测试残留，让真实夹爪框完全接管碰撞拒抓、overlay 状态框和 AC 射线 C 点/中心偏移。
- 修改：`PoseDetection` 删除 `extended_corners`；`frame_postprocess` 删除 `kExtendedObbScale`、`ScaleObbLongEdge()` 和基于旧延长框求 C 的逻辑，后处理只保留 AC 基础姿态、基础抓取框和 `grip_long_angle_deg`；`grab_limit_evaluator` 在真实夹爪框生成后按 O->A 方向求真实夹爪边界 C 点，更新 `arrow_start/arrow_end/angle_deg/center_x/center_y`，再执行真实夹爪 mask 碰撞拒抓；`frame_processing_service` 在真实夹爪最终化后重新应用坐标转换，保证 PLC 机器坐标来自最终中心。
- 结果：代码和测试中已搜不到 `extended_corners`、`ExtendedObb`、`kExtendedObbScale`、`ScaleObbLongEdge`、`draw_extended_debug` 等旧延长框符号；普通/全显 overlay 都只画真实夹爪框、基础抓取框和射线。
- 验证：`cmake --build build --config Release --target tankeye-openvino_frame_postprocess_smoke`、`tankeye-openvino_grab_limit_evaluator_test`、`tankeye-openvino_frame_overlay_test`、`tankeye-openvino_qt_app` 均通过；`powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；`powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1 -BuildDir build -Device CPU -SimulatePlc` 启动真实 Qt 程序并生成 `build/Release/logs/tankeye_20260804_184839.log`，GUI 常驻 45 秒超时后确认无残留 `tankeye` 进程。
- 遗留：未连接真实 PLC/真实相机；需要用户用现场图片或相机画面复核真实夹爪框、射线 C 点、中心偏移和拒抓效果。

## 2026-08-04 - 打包 TankEye-Iris 1.4 运行包
- 目标：按用户要求读取并更新 `docs/PACKAGING_README.md` 的使用说明，生成版本号 1.4 的运行包，并确保运行包不包含 `docs/` 和 `AGENTS.md`。
- 修改：`docs/PACKAGING_README.md` 更新为 1.4 路径、命令、验证步骤和真实夹爪框行为说明；`runtime/USAGE_GUIDE.txt` 更新运行包使用说明和真实夹爪框说明；`scripts/package_runtime.ps1` 默认发布名、运行包 README、manifest 版本更新为 1.4，并增加 staging 检查，若 `docs/` 或 `AGENTS.md` 进入运行包则失败。
- 结果：已生成 `dist\TankEye-Iris_1.4` 和 `dist\TankEye-Iris_1.4.zip`；运行包 `RELEASE_MANIFEST.json` 显示 `name=TankEye-Iris_1.4`、`version=1.4`、`runtime_only=true`；运行包保留主程序、Qt/OpenVINO/Hikrobot 运行依赖、模型、配置和使用说明。
- 验证：执行 `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\package_runtime.ps1 -BuildDir build -ReleaseName TankEye-Iris_1.4 -Force` 成功，期间仅有已知 `VCINSTALLDIR is not set` 警告；检查 `tankeye-openvino_qt_app.exe`、`platforms\qwindows.dll`、`models\weights\best_obb.xml`、`models\weights\best_seg.xml`、`USAGE_GUIDE.txt` 和 zip 均存在；`dist\TankEye-Iris_1.4\docs` 与 `dist\TankEye-Iris_1.4\AGENTS.md` 均返回 `False`；zip 条目检查确认不包含 `docs/` 和 `AGENTS.md`；运行包内 `launch_tankeye.ps1 -Device CPU -SimulatePlc` 启动真实 Qt 程序，生成 `dist\TankEye-Iris_1.4\logs\tankeye_20260804_190721.log`，日志确认模拟 PLC、主窗口创建、OBB/SEG 模型按 CPU 加载成功，45 秒 GUI 常驻超时后确认无残留 `tankeye` 进程；`git diff --check` 无空白错误，仅有 CRLF 换行提示。
- 遗留：本次未连接真实 PLC/真实相机；打包脚本提示未找到有效启用的九点方案，因此运行包内机械保护限制保持 `machine_limits.enabled=false`，现场使用前仍需确认目标机器配置。

## 2026-08-04 - 上传真实夹爪框与 1.4 打包改动到 GitHub
- 目标：按用户要求将当前已验证改动上传到 GitHub `origin/Unordered_Scraping_V5`。
- 修改：先提交 26 个已跟踪文件，范围包含真实夹爪框完全接管旧延长框、相关测试、1.4 打包脚本与使用说明、项目记忆更新；随后补充本次上传记录。
- 结果：主提交 `55f9b46` 已推送到 `origin/Unordered_Scraping_V5`，远端从 `998dfff` 更新到 `55f9b46`。
- 验证：推送前确认暂存范围不包含 `dist/`、`build/`、模型权重或 `config/admin_auth.key`；`git push origin Unordered_Scraping_V5` 在提权网络环境下成功返回 `998dfff..55f9b46`。
- 遗留：运行包产物 `dist\TankEye-Iris_1.4` 和 `dist\TankEye-Iris_1.4.zip` 留在本地且未纳入 Git；本次未连接真实 PLC/真实相机。

## 2026-08-05 - 将真实夹爪方向演进到 Python 1.0.14 AC 垂直语义
- 目标：按用户确认的 `推理v1.0.13.py` 到 `推理v1.0.14(1).py` 几何变化，将 C++ 后处理从“夹爪长轴沿对齐红框长边”演进为“先确定 AC 射线，真实夹爪长轴垂直 AC、宽度沿 AC”，不恢复旧 2 倍/3 倍延长框。
- 修改：`frame_postprocess` 保持 AC 选择仍基于 `SEG 中心 O -> A -> C`，但 `PoseDetection::grip_long_angle_deg` 改为 `AC angle + 90°` 并归一化，用于真实夹爪长轴；后处理调试日志 `ray_logic` 更新为 `v1.0.14_ac_perp_gripper` 并输出新的 `gripper_long_angle_deg`。`grab_limit_evaluator` 继续使用真实夹爪长宽和九点坐标转换生成 `mechanical_gripper_corners`，碰撞规则不变。
- 结果：PLC/overlay 的 AC 箭头角度仍来自 `A -> C`，真实夹爪框长边改为垂直 AC，C 点和中心偏移在真实夹爪框边界上按 AC 方向求得；旧 `extended_corners`、`ExtendedObb`、`ScaleObbLongEdge` 等延长框逻辑未恢复。
- 验证：`cmake --build build --config Release --target tankeye-openvino_frame_postprocess_smoke`、`tankeye-openvino_grab_limit_evaluator_test`、`tankeye-openvino_frame_overlay_test`、`tankeye-openvino_qt_app` 均通过；`powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；`powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release -Filter tankeye-openvino_frame_postprocess_smoke.exe` 通过；`powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1 -BuildDir build -Device CPU -SimulatePlc` 启动真实 Qt 程序并生成 `build/Release/logs/tankeye_20260805_152102.log`，日志确认 `[PLC_SIM] enabled`、主窗口创建并进入 Qt 事件循环，GUI 常驻导致 60 秒超时后确认无残留 `tankeye` 进程；`git diff --check` 无空白错误，仅有 CRLF 提示。
- 遗留：未连接真实 PLC/真实相机；仍需用户用现场图片或相机画面复核真实夹爪框长轴垂直 AC 后的方向、C 点、中心偏移、D504 角度和拒抓效果是否符合实际夹具。

## 2026-08-05 - 补充上传 Python 1.0.14 参考脚本
- 目标：按用户要求把 `models/推理v1.0.14(1).py` 也纳入 Git/GitHub，作为 1.0.14 参考实现保留在仓库中；`models/推理v1.0.13.py` 继续保留不变。
- 修改：本轮仅新增未跟踪的 `models/推理v1.0.14(1).py` 到版本库暂存/提交范围，未改动 C++ 源码、测试或其他项目文件。
- 结果：仓库中同时保留 `推理v1.0.13.py` 和 `推理v1.0.14(1).py`，便于后续继续对照 Python 参考语义；未触碰现有 C++ 实现。
- 验证：这次只涉及参考脚本入库，没有新的编译目标或运行时行为变化；沿用上一轮已通过的 Release 构建、默认测试和模拟 PLC Qt 启动结果作为当前基线。
- 遗留：后续如继续对照 Python 版本演进，仍需优先以当前 C++ 实现和现场验证为准，不可再次把旧临时放大框语义误带回实现。

## 2026-08-05 - 四类型角度补偿与沿 AC 中心偏移
- 目标：在当前 1.0.14 AC 几何语义上，为 D508 类型码 `1=大上右`、`3=大上左`、`4=小上右`、`2=小上左` 增加独立角度补偿和沿 A->C 的机械毫米中心偏移，并让最终 PLC 命令进入限位复核。
- 修改：`PlcOutputConfig` 增加四类型补偿数组；`BuildPlcWriteResult()` 叠加全局角度/轴补偿与类型补偿，AC 偏移在机械坐标中沿单位向量计算；`coordinate_transform` 保存机械 AC 单位向量；`RobotController` 同步持有类型补偿，真实 PLC 写入和诊断写入共用同一输出合同；无有效 AC 方向或坐标时有非零类型中心偏移则保守拒抓。
- 修改：工程设置增加四类型补偿表，支持角度补偿、AC 偏移(mm)、默认 0 和旧配置兼容；普通信息面板/目标卡片优先显示补偿后的中心、机械坐标和 PLC 角度，全显仍可通过原始 OBB 中心与 AC 射线对照。
- 修改：限位判断改为复核最终 PLC 命令，补偿后前后轴、左右轴或角度越界时拒抓；补充 PLC 合同、设置持久化和限位测试。
- 验证：`cmake --build build --config Release --target tankeye-openvino_plc_result_contract_test`、`tankeye-openvino_engineering_settings_service_test`、`tankeye-openvino_grab_limit_evaluator_test`、`tankeye-openvino_frame_postprocess_smoke`、`tankeye-openvino_frame_overlay_test`、`tankeye-openvino_qt_app` 均通过；默认 `scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；单独 `frame_postprocess_smoke` 通过；`launch_tankeye.ps1 -BuildDir build -Device CPU -SimulatePlc` 启动日志 `build/Release/logs/tankeye_20260805_165752.log` 确认模拟 PLC、主窗口创建并进入 Qt 事件循环，随后已清理无残留进程；`git diff --check` 无空白错误，仅有 CRLF 换行提示。
- 遗留：未连接真实 PLC、真实相机或真实设备；仍需现场用四类真实目标复核补偿正负方向、D500/D502/D504 最终值、ROI 拒抓和工程设置界面可读性。

## 2026-08-05 - 增加四类型补偿调试日志

- 目标：根据 `tankeye_20260805_172705.log` 无法确认四类型补偿是否作用的问题，在不改变关闭调试时日志量的前提下，补充从配置到最终 PLC 命令的诊断链路。
- 修改：扩展 `PlcOutputConfig` 的调试开关并同步到 `RobotController`；`BuildPlcWriteResult()` 在调试开启时输出 D508、原始位姿、AC 单位向量、全局/类型补偿、轴补偿和最终 D500/D502/D504/D506/D508；工程设置加载/保存及图片/PLC 触发限位复核增加 `[PLC_DEBUG]` 日志。
- 结果：现场可区分参数未加载、D508 未匹配、AC 偏移无法计算、补偿后限位拒抓和最终命令值等情况；关闭“启用后处理调试日志”时新增日志全部关闭。
- 验证：`cmake --build build --config Release` 通过；`scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` 全部测试通过；`launch_tankeye.ps1 -BuildDir build -Device CPU -SimulatePlc -DebugPostprocess` 启动成功，生成 `build/Release/logs/tankeye_20260805_174628.log`，确认模拟 PLC、`[PLC_DEBUG] head_type_compensation_loaded`、调试日志开关和 Qt 事件循环正常，随后清理无残留进程。
- 遗留：尚未连接真实 PLC/真实相机；需要现场开启调试日志并实际执行一次图片写 PLC或模拟触发，检查 `[PLC_DEBUG] compensation` 中的最终 D500/D502/D504。

## 2026-08-05 - 普通画面显示补偿后命令姿态并补齐写入日志

- 目标：解决四类型补偿已计算但普通画面仍显示原始夹爪框/AC 射线，导致现场观感像“没效果”的问题；同时让图片写 PLC 的成功/失败进入调试日志。
- 修改：`ScaleResultForDisplay()` 同步缩放 `plc_command_center` 和命令 AC 射线；`frame_overlay` 普通模式使用补偿后的命令中心/射线，并按命令中心偏移真实夹爪框，全显模式继续画原始姿态用于对照；`WriteFrameResultToPlc()` 在调试开启时输出 `manual_write` 成功、拒抓或失败日志；主窗口图片写 PLC 回调删除 `return` 后死代码。
- 结果：普通显示与最终 PLC 命令值对齐，全显仍保留补偿前几何；调试日志可直接看到图片写入最终 D500/D502/D504 是否成功写出或被拒抓。
- 验证：`cmake --build build --config Release --target tankeye-openvino_frame_overlay_test` 通过；`cmake --build build --config Release --target tankeye-openvino_qt_app` 通过；`scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；`launch_tankeye.ps1 -BuildDir build -Device CPU -SimulatePlc -DebugPostprocess` 启动成功，生成 `build/Release/logs/tankeye_20260805_180224.log`，确认模拟 PLC、补偿参数加载日志和 Qt 事件循环，随后清理无残留进程。
- 遗留：未连接真实 PLC/真实相机；需要现场开启调试日志，用非零四类型参数重新检测并确认普通 overlay 与 `[PLC_DEBUG] manual_write/compensation` 的最终命令一致。

## 2026-08-05 - 修正角度补偿后的普通画面旋转显示

- 目标：根据 `tankeye_20260805_180512.log` 和现场观察，解决角度补偿已进入最终 D504 但普通画面框/箭头方向仍像原始角度、甚至出现原始框与补偿框同时显示的问题。
- 修改：`ApplyPlcCommandPose()` 按最终 PLC D504 相对原始 AC 角度的差值生成命令箭头；`frame_overlay` 普通模式按同一角度差旋转真实夹爪框并平移到补偿后中心，且在有有效补偿命令姿态和真实夹爪框时不再画原始基础 OBB；`frame_overlay_test` 增加补偿命令姿态旋转方向和全显保留原始姿态覆盖。
- 结果：普通画面只保留补偿后的旋转夹爪框和补偿后的 AC 箭头；原始 OBB/原始几何仍保留在全显/调试模式用于对照排查。
- 验证：`cmake --build build --config Release --target tankeye-openvino_frame_overlay_test` 通过；`cmake --build build --config Release --target tankeye-openvino_qt_app` 通过；`scripts/run_build_tests.ps1 -BuildDir build -Configuration Release -Filter tankeye-openvino_frame_overlay_test.exe` 通过；默认 `scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；`launch_tankeye.ps1 -BuildDir build -Device CPU -SimulatePlc -DebugPostprocess` 启动成功，生成 `build/Release/logs/tankeye_20260805_181435.log`，确认模拟 PLC、补偿参数加载和 Qt 事件循环，随后确认无残留 `tankeye` 进程。
- 遗留：未连接真实 PLC、真实相机或真实设备；仍需现场用非零类型角度补偿图片复核普通画面的框方向与最终 D504 一致。

## 2026-08-05 - 恢复 D508 左右为夹取 OBB 本地 AC 判定

- 目标：修正“大上左”现场物料被程序判成“大上右”，导致四类型补偿调错行的问题；恢复用户原始需求中的“以夹取 OBB 为基准，看大头/小头在夹取 OBB 哪边”。
- 修改：`frame_postprocess` 将左右来源从 `A.x vs SEG center.x` 改为 `A->C` 本地坐标：以 AC 射线为前向，按头部 B 在右轴侧或左轴侧输出 D508；调试日志 `head_side_basis` 改为 `grip_local_ac`；`frame_postprocess_smoke` 增加 A 在 SEG 右侧但 B 位于本地左侧时仍输出 `D508=3 大上左` 的回归。
- 结果：D508 编码和 UI/PLC 含义不变，四类型补偿按修正后的现场左右语义匹配；物料朝上、朝下或旋转时不再依赖图像全局左右判定。
- 验证：`cmake --build build --config Release --target tankeye-openvino_frame_postprocess_smoke` 通过；`cmake --build build --config Release --target tankeye-openvino_qt_app` 通过；`scripts/run_build_tests.ps1 -BuildDir build -Configuration Release -Filter tankeye-openvino_frame_postprocess_smoke.exe`、`tankeye-openvino_plc_result_contract_test.exe`、`tankeye-openvino_frame_overlay_test.exe` 均通过；默认 `scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；`launch_tankeye.ps1 -BuildDir build -Device CPU -SimulatePlc -DebugPostprocess` 启动成功，日志 `build/Release/logs/tankeye_20260805_184746.log` 确认模拟 PLC、补偿参数加载和 Qt 事件循环，随后确认无残留进程。
- 遗留：未连接真实 PLC、真实相机或真实设备；仍需用户用现场图片人工确认四类名称和对应补偿行已恢复一致。

## 2026-08-05 - 恢复普通/全显显示原始 OBB 与补偿姿态

- 目标：修正普通模式原始 OBB 被隐藏、全显模式补偿后的真实夹爪框和射线不显示的问题。
- 修改：`frame_overlay` 改为始终绘制基础 `detection.corners`；普通和全显在存在 `plc_command_pose` 时都额外绘制补偿后的真实夹爪框与 AC 射线；`frame_overlay_test` 更新为同时断言原始 OBB、补偿框和补偿射线可见。
- 结果：普通模式同时显示检测基础 OBB 和最终补偿姿态；全显模式在 raw OBB、SEG、碰撞 mask 等调试层之外，也显示最终补偿姿态。
- 验证：`cmake --build build --config Release --target tankeye-openvino_frame_overlay_test` 通过；`cmake --build build --config Release --target tankeye-openvino_qt_app` 通过；`scripts/run_build_tests.ps1 -BuildDir build -Configuration Release -Filter tankeye-openvino_frame_overlay_test.exe` 通过；默认 `scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过。
- 遗留：未连接真实 PLC、真实相机或真实设备；仍需现场人工确认普通/全显叠层视觉是否清楚。

## 2026-08-05 - 修正 overlay 原始 OBB 去重与补偿同步

- 目标：修正全显模式出现两个原始 OBB 框，以及普通模式基础 OBB 不随补偿后夹爪姿态变化的问题。
- 修改：普通模式下 detection 基础 OBB 与真实夹爪框使用同一 `plc_command_pose` 旋转/平移；全显模式由 `raw_obb_regions` 显示原始模型 OBB，不再从 detection 重复画基础 OBB，但仍显示补偿后的真实夹爪框和 AC 射线；`frame_overlay_test` 增加普通旧位置清空、新补偿位置可见、全显 raw+command 姿态可见断言。
- 结果：普通模式的基础 OBB 会跟随补偿姿态；全显模式不再出现两层原始 OBB 重叠，同时保留最终补偿姿态。
- 验证：`cmake --build build --config Release --target tankeye-openvino_frame_overlay_test` 通过；`cmake --build build --config Release --target tankeye-openvino_qt_app` 通过；`scripts/run_build_tests.ps1 -BuildDir build -Configuration Release -Filter tankeye-openvino_frame_overlay_test.exe` 通过；默认 `scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过。
- 遗留：未连接真实 PLC、真实相机或真实设备；仍需现场切换普通/全显人工确认叠层观感。

## 2026-08-05 - 长时间运行风险扫查与 TankEye-Iris 1.4.1 打包
- 目标：按用户要求大扫当前 C++/Qt 维护状态，重点检查明显 bug、内存泄漏和长时间使用风险；若无阻塞问题则生成 1.4.1 运行包。
- 修改：`ApplyPlcCommandPose()` 不再为每个 detection 复制完整 `FrameInferenceResult`，改为构造只含当前目标和 PLC 合同字段的最小结果，减少长时间检测循环中的无意义容器/`cv::Mat` 引用计数拷贝；`scripts/package_runtime.ps1`、`runtime/USAGE_GUIDE.txt`、`docs/PACKAGING_README.md` 版本和路径更新为 1.4.1。
- 结果：静态扫查未发现新的确定性内存泄漏；Qt 对象基本由父对象或 `deleteLater()` 管理，相机线程通过原子标志停止并 join。已生成 `dist\TankEye-Iris_1.4.1` 和 `dist\TankEye-Iris_1.4.1.zip`，manifest 显示 `name=TankEye-Iris_1.4.1`、`version=1.4.1`、`runtime_only=true`。
- 验证：`cmake --build build --config Release --target tankeye-openvino_qt_app` 通过；`powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；`git diff --check` 无空白错误，仅有 CRLF 提示；打包命令成功，只有已知 `VCINSTALLDIR is not set` 警告；包内关键文件、zip、manifest、exe 哈希、禁止 `docs/`/`AGENTS.md` 检查通过；包内 `launch_tankeye.ps1 -Device CPU -SimulatePlc` 启动真实 Qt 程序并正常退出，日志 `dist\TankEye-Iris_1.4.1\logs\tankeye_20260805_192737.log` 确认模拟 PLC、主窗口创建、OBB/SEG CPU 模型加载成功，退出后无残留 `tankeye` 进程。
- 遗留：本轮未连接真实 PLC、真实相机或真实设备；仍建议现场长时间挂机观察日志增长、内存占用和相机连续采集稳定性。
## 2026-08-05 - 增加开机自启与启动延迟设置

- 目标：按用户要求，生成 TankEye-Iris 桌面图标后默认创建当前用户开机自启入口，并在工程设置中提供“开机自启”和“延迟启动秒数”控制；关闭开机自启后不再随 Windows 登录启动。
- 修改：新增 `app_startup_manager`，通过当前用户 Startup 文件夹内的 `TankEye-Iris.vbs`/`TankEye-Iris.lnk` 管理自启入口，不使用注册表、服务或计划任务；`EngineeringSettingsService` 新增 `startup/auto_start_enabled` 和 `startup/delay_seconds`，默认开机自启开启、延迟 0 秒，延迟范围限制为 0~600 秒；工程设置新增“启动设置”折叠区；保存工程设置时同步创建或移除当前用户启动入口。
- 修改：`launch_tankeye.ps1` 和运行包内启动脚本新增 `-StartupDelaySeconds`；`scripts/create_desktop_shortcut.ps1` 以及运行包生成的 `create_desktop_shortcut.ps1` 默认同步创建 Startup 快捷方式，默认延迟 0 秒，可用 `-NoAutoStart` 跳过；`runtime/USAGE_GUIDE.txt`、`docs/PACKAGING_README.md` 和 `scripts/package_runtime.ps1` 内运行包说明已同步。
- 结果：已重新生成本地运行包 `dist\TankEye-Iris_1.4.1` 和 `dist\TankEye-Iris_1.4.1.zip`，包内 `create_desktop_shortcut.ps1` 默认会创建开机自启入口，包内 manifest 仍为 `name=TankEye-Iris_1.4.1`、`version=1.4.1`、`runtime_only=true`、`files=129`。
- 验证：`cmake --build build --config Release --target tankeye-openvino_engineering_settings_service_test` 通过；`cmake --build build --config Release --target tankeye-openvino_qt_app` 通过；`scripts/run_build_tests.ps1 -BuildDir build -Configuration Release -Filter tankeye-openvino_engineering_settings_service_test.exe` 通过；默认 `scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；`git diff --check` 无空白错误，仅有 CRLF 提示；`launch_tankeye.ps1`、`scripts/package_runtime.ps1`、源码与包内 `create_desktop_shortcut.ps1` PowerShell 语法检查通过；打包成功，仅有已知 `VCINSTALLDIR is not set` 警告。
- 遗留：本轮未实际执行会写入桌面或 Startup 文件夹的快捷方式脚本，也未真实重启 Windows 验证开机自启；新增单元测试只在临时目录验证启动入口创建/删除，不触碰真实系统启动项。工作区仍有用户既有未跟踪样张 `samples/images/*.bmp`，本次未修改。
## 2026-08-05 - 修正普通显示基础 OBB 与真实夹爪框角度不一致

- 目标：修复同一张图中部分目标的普通画面基础 OBB 框与真实夹爪框角度一致、部分目标相差 90 度的问题；用户期望普通画面中这两层使用同一个最终夹爪长边方向。
- 根因：后处理会在 `head_long_angle` 与 `head_long_angle+90` 两个候选中选择 AC 几何最优方向，导致 `pose.corners` 的长边有时等于 AC、有时等于 AC+90；真实夹爪框始终按 `grip_long_angle_deg=AC+90` 生成。此前 overlay 只统一应用 PLC 命令姿态旋转/平移，没有统一二者初始长边方向。
- 修改：`frame_overlay` 在普通模式绘制基础 detection corners 时，先基于真实夹爪框长边角度对基础 OBB corners 做一次显示层旋转对齐；该改动只影响普通 overlay 的基础 OBB 显示，不修改 CV 后处理、真实夹爪框生成、碰撞判定、PLC 输出、D504 或全显 raw OBB 调试层。
- 验证：`tankeye-openvino_frame_overlay_test` Release 构建通过并新增“普通模式基础 OBB 对齐真实夹爪框”回归；`tankeye-openvino_qt_app` Release 构建通过；默认 `scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；`git diff --check` 无空白错误，仅有 CRLF 提示；已重新生成 `dist\TankEye-Iris_1.4.1` 和 zip，manifest 仍为 `version=1.4.1`、`files=129`。
- 遗留：未用用户现场图片重新启动 GUI 做截图肉眼复核；需现场确认普通画面基础 OBB 与真实夹爪框现在稳定同向，全显/调试模式仍可看到 raw OBB 与 AC 射线用于排查。
## 2026-08-06 - 修正开机自启 VBS 语法导致的启动报错

- 问题：用户重启后在 Windows Script Host 中看到 `TankEye-Iris.vbs` 第 18 行语法错误 `800A03EA`，新电脑测试人员也反馈“闪退”。
- 处理：把 `app_startup_manager` 生成的 VBS 改成二进制原样写入，避免文本模式对已含 CRLF 的续行脚本二次换行；同时给 `engineering_settings_service_test` 增加原始字节级回归，确认不存在 `_` 后空白行。
- 结果：`tankeye-openvino_engineering_settings_service_test`、`tankeye-openvino_qt_app` 构建通过，默认构建测试脚本全部通过。
- 备注：当前问题根因在自启动脚本，不是 Qt 主程序闪退本体；真实 Startup 目录未被修改。

## 2026-08-06 - 将生成图标后的开机自启默认延迟改为 0 秒并打包

- 目标：落实用户口径，生成桌面图标后默认开机自启，默认延迟 0 秒；如不需要自启，再由管理员进入工程设置关闭。
- 修改：`StartupLaunchSettings`、工程设置草稿、源码 `create_desktop_shortcut.ps1` 和运行包内嵌 `create_desktop_shortcut.ps1` 的默认 `StartupDelaySeconds` 均改为 0；相关文档同步说明默认 0 秒。
- 验证：`cmake --build build --config Release --target tankeye-openvino_engineering_settings_service_test` 通过；`tankeye-openvino_qt_app` Release 构建通过；默认 `scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过。
- 打包：执行 `scripts/package_runtime.ps1 -BuildDir build -ReleaseName TankEye-Iris_1.4.1 -Force` 成功，产物为 `dist\TankEye-Iris_1.4.1` 和 `dist\TankEye-Iris_1.4.1.zip`；包内 `create_desktop_shortcut.ps1` 已确认默认 `StartupDelaySeconds = 0`。
## 2026-08-06 - 寮€鏈鸿嚜鍚鍔犲叆鑷姩鍔犺浇妯″瀷

- 鐩爣锛氳ˉ瓒宠繖娆℃敼鍔ㄧ殑鏈€鍚庝竴姝ワ紝璁╁紑鏈鸿嚜鍚笉鍙槸鎵撳紑绋嬪簭锛岃€屾槸鐩存帴鍔犺浇 OBB/SEG 妯″瀷銆?
- 淇敼锛歚app_startup_manager` 鍜?`scripts/create_desktop_shortcut.ps1` 鐨勫惎鍔ㄥ弬鏁扮粺涓?`-AutoLoadModels`锛涘紑鏈鸿剼鏈?`launch_tankeye.ps1` 浼氭牴鎹凡浼樺厛鐨勬ā鍨嬭矾寰勮嚜鍔ㄤ紶鍏?OBB/SEG 鍙傛暟锛岀劧鍚庡啀鍚姩 Qt 绋嬪簭銆?
- 缁撴灉锛氬紑鏈鸿嚜鍚幇鍦哄紑鍚悗浼氳绋嬪簭鐩存帴鍑虹幇鍦ㄥ凡鍔犺浇妯″瀷鐨勭姸鎬侊紝涓嶅啀闇€瑕佹墜鍔ㄩ€夋ā鍨嬨€?
- 楠岃瘉锛歚cmake --build build --config Release --target tankeye-openvino_engineering_settings_service_test tankeye-openvino_qt_app` 閫氳繃锛涢粯璁?`scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` 閫氳繃锛涘皻鏈噸鏂扮敓鎴愭柊鐗堣繍琛屽寘锛屾墍浠?dist` 1.4.1 浠嶆槸涓婁竴杞殑鐗堟湰銆?
## 2026-08-06 - 开机自启联动升级为“自动进入抓取”

- 目标：把开机自启从“只打开程序 + 自动加载模型”升级为“模型加载完成后自动进入 PLC 抓取联动”，同时把主按钮文案收敛为“开始 / 关闭”。
- 修改：`app/qt/main.cpp` 读取 `TANKEYE_AUTO_START_GRASP` 并把开机自启意图传入 `GraspMainWindow`；`GraspMainWindow` 继续只在模型加载完成后触发一次自动进入联动；`plc_link_button_` 初始文案改为“开始”，相关提示语同步改为“关闭/开始”；`RuntimeStatusPresenter` 保持按钮短标签。
- 修改：`launch_tankeye.ps1`、`app_startup_manager.cpp`、`scripts/create_desktop_shortcut.ps1` 与 `scripts/package_runtime.ps1` 的启动入口都同步带上 `-AutoLoadModels -AutoStartGrasp`。
- 修改：`tests/engineering_settings_service_test.cpp` 补了启动入口参数断言；`tests/runtime_status_presenter_test.cpp` 补了按钮文案断言，并改成 `QApplication` 入口。
- 结果：Release 构建通过，`tankeye-openvino_engineering_settings_service_test`、`tankeye-openvino_runtime_status_presenter_test` 和默认 `scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过。
- 备注：这轮没有实际打开带自动抓取的 GUI 冒烟，因为自动抓取会进入相机链路，按当前约束先不碰真实相机。

## 2026-08-06 - 开机自启链路自愈与运行包脚本修正

- 目标：修正现场自启入口仍用旧参数、运行包脚本不接收 `-AutoLoadModels -AutoStartGrasp` 导致自启后不加载模型/不进入抓取的问题。
- 修改：`main.cpp` 启动时读取工程设置并调用 `SyncStartupShortcut()`，让旧 `TankEye-Iris.vbs/.lnk` 自动重写或在关闭自启时移除；`app_startup_manager` 新增 `BuildStartupLaunchArguments()` 作为自启参数单一合同；运行包 `launch_tankeye.ps1` 模板新增 `-AutoLoadModels/-AutoStartGrasp/-StartupDelaySeconds` 参数并设置 `TANKEYE_AUTO_START_GRASP`；文档同步说明自启会加载模型并请求进入 PLC 抓取联动。
- 结果：重新生成 `dist\TankEye-Iris_1.4.1` 和 `dist\TankEye-Iris_1.4.1.zip`；包内脚本和当前用户 Startup 入口均确认带 `-StartupDelaySeconds 0 -AutoLoadModels -AutoStartGrasp`。
- 验证：`cmake --build build --config Release --target tankeye-openvino_qt_app tankeye-openvino_engineering_settings_service_test tankeye-openvino_runtime_status_presenter_test` 通过；默认 `scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；打包成功，仅有已知 `VCINSTALLDIR is not set` 警告；包目录/zip 均不含 `docs/` 或 `AGENTS.md`；包内 `launch_tankeye.ps1 -Device CPU -SimulatePlc` 已真实启动 Qt 程序，日志 `dist\TankEye-Iris_1.4.1\logs\tankeye_20260806_141949.log` 确认模拟 PLC、OBB/SEG 模型按 CPU 加载成功。
- 遗留：未运行带 `-AutoStartGrasp` 的真实 GUI 冒烟，因为该路径会尝试打开真实相机；需用户允许真实相机动作后再做最终开机联动实测。

## 2026-08-06 - 重构启动补丁为 AutoStart 启动画像

- 目标：把前几轮为修自启问题叠出的长串参数、`.lnk/.vbs` 双入口和重复拼接收敛成更清楚的启动合同。
- 修改：自启入口统一为 `TankEye-Iris.vbs`，旧 `TankEye-Iris.lnk` 仅作为迁移残留清理；自启参数从 `-Device AUTO -WindowMode Maximized -StartupDelaySeconds ... -AutoLoadModels -AutoStartGrasp` 收敛为 `-StartupProfile AutoStart -StartupDelaySeconds ...`；源码和运行包 `launch_tankeye.ps1` 负责把 `AutoStart` 画像展开为 `Device=AUTO`、最大化、自动加载模型和自动开始抓取意图。
- 修改：`SyncStartupShortcut()` 先比较现有 VBS 内容，内容一致时不再重写，只清理旧 `.lnk`；`scripts/create_desktop_shortcut.ps1` 和运行包内同名脚本改为生成 Startup VBS，不再生成 Startup LNK；测试改为断言启动画像合同和旧 LNK 清理。
- 结果：当前用户 `Startup\TankEye-Iris.vbs` 已确认指向 `dist\TankEye-Iris_1.4.1\launch_tankeye.ps1`，并只包含 `-StartupProfile AutoStart -StartupDelaySeconds 0`。
- 验证：`tankeye-openvino_qt_app`、`tankeye-openvino_engineering_settings_service_test`、`tankeye-openvino_runtime_status_presenter_test` Release 构建通过；默认 `scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；三份 PowerShell 脚本语法解析通过；重新打包 `dist\TankEye-Iris_1.4.1` 和 zip 成功；包内脚本检查确认自启入口使用 `StartupProfile`；包内 `launch_tankeye.ps1 -Device CPU -SimulatePlc` 真实启动成功，日志 `dist\TankEye-Iris_1.4.1\logs\tankeye_20260806_144008.log` 确认模型加载。
- 遗留：`launch_tankeye.ps1` 仍保留旧 `-AutoLoadModels/-AutoStartGrasp` 参数作为兼容旧手动命令和旧入口的迁移口，不再作为新自启入口合同；未运行真实相机自动抓取冒烟。

## 2026-08-06 - 删除启动画像重构后的兼容尾巴

- 目标：回应“重构补丁后代码反而更多”的问题，删除上一轮为兼容旧入口保留的旧自动开关参数，避免 `StartupProfile` 与 `AutoLoadModels/AutoStartGrasp` 两套口径并存。
- 修改：源码 `launch_tankeye.ps1` 和运行包模板内 `launch_tankeye.ps1` 移除 `-AutoLoadModels`、`-AutoStartGrasp` 参数；自动加载模型与自动开始抓取只由 `-StartupProfile AutoStart` 控制；测试删除对旧开关不存在性的额外断言。
- 结果：新自启入口和包内脚本只保留 `StartupProfile` 作为自动启动合同，旧自动开关不再出现在启动脚本参数面上。
- 验证：三份 PowerShell 脚本语法解析通过；`tankeye-openvino_qt_app`、`tankeye-openvino_engineering_settings_service_test`、`tankeye-openvino_runtime_status_presenter_test` Release 构建通过；默认 `scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；重新打包 `dist\TankEye-Iris_1.4.1` 和 zip 成功；包内脚本 grep 确认只剩 `StartupProfile`；包内模拟 PLC 启动日志 `dist\TankEye-Iris_1.4.1\logs\tankeye_20260806_145501.log` 确认模型加载。
- 遗留：仍未运行真实相机自动抓取冒烟。

## 2026-08-06 - 生成项目架构图 PDF

- 目标：按用户要求把项目架构和思维导图整理成可直接查看的 PDF。
- 修改：新增 `docs/architecture/TankEye-Iris_Architecture_Map.html` 作为可编辑图源，包含项目架构思维导图、核心运行链路图和关键边界说明；使用 Edge headless 生成 `docs/architecture/TankEye-Iris_Architecture_Map.pdf`。
- 结果：PDF 已生成在项目目录内，可直接发给现场或测试人员查看。
- 验证：确认 HTML 中包含“TankEye-Iris 项目架构思维导图”“核心运行链路架构图”和 `StartupProfile AutoStart` 说明；确认 PDF 文件存在且文件头为 `%PDF-1.4`，大小约 169 KB。
- 遗留：本轮为文档/图示产物，未运行代码构建；未打开 PDF 做人工视觉复核。

## 2026-08-06 - Restore manual AutoLoadModels launch behavior

- Goal: Explain and fix why `launch_tankeye.ps1 -AutoLoadModels` printed default model paths but started Qt with empty OBB/SEG arguments.
- Change: Restored `-AutoLoadModels` in source-tree `launch_tankeye.ps1` as a manual model-loading switch. `-StartupProfile AutoStart` remains the only startup profile that also requests auto grasp; `-AutoLoadModels` does not request grasp.
- Result: Manual source-tree launches can again use `-AutoLoadModels` to pass `models\weights\best_obb.xml` and `best_seg.xml` to the Qt app.
- Verification: PowerShell parser check passed; `cmake --build build --config Release --target tankeye-openvino_qt_app` passed; `scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` passed; simulated PLC GUI launch with `-AutoLoadModels -Device CPU -SimulatePlc` produced `build/Release/logs/tankeye_20260806_152730.log` with non-empty OBB/SEG model arguments and OpenVINO model loading lines.
- Leftover: The GUI smoke stayed open as expected and was manually terminated after verification; no real PLC, real camera, or auto-grasp path was exercised.

## 2026-08-06 - Package TankEye-Iris 1.4.2

- Goal: Generate a fresh `1.4.2` runtime package containing the fixed manual `-AutoLoadModels` launch behavior.
- Change: Updated package version/default release name/usage docs to `1.4.2`; kept startup contract as `-StartupProfile AutoStart`; made the packaged launcher accept `-AutoLoadModels` as a manual compatibility switch so old field commands do not fail.
- Result: Generated `dist\TankEye-Iris_1.4.2` and `dist\TankEye-Iris_1.4.2.zip`.
- Verification: PowerShell parse passed for launch/package/shortcut scripts; `cmake --build build --config Release --target tankeye-openvino_qt_app tankeye-openvino_engineering_settings_service_test tankeye-openvino_runtime_status_presenter_test` passed; default `scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` passed; package generation passed with known `VCINSTALLDIR is not set` warning; manifest shows `name=TankEye-Iris_1.4.2`, `version=1.4.2`, `runtime_only=true`, `files=129`; package contains no `docs/` or `AGENTS.md`; packaged exe hash matches `build\Release`; packaged simulated-PLC launch with `-Device CPU -SimulatePlc -AutoLoadModels` produced `dist\TankEye-Iris_1.4.2\logs\tankeye_20260806_153900.log` confirming OBB/SEG model arguments, CPU OpenVINO loading, compile, and warmup.
- Leftover: No real PLC, real camera, or auto-grasp smoke was run.

## 2026-08-06 - Show App Name And Version In Top Bar

- Goal: Print the current software name and version next to the top-left icon, and make both the left app label and the centered system title editable from config.
- Change: Updated `GraspMainWindow` top-bar brand label to show the configured app display label, reused the same label for the window title, and fixed the top-bar layout after screenshot feedback showed the first label was still invisible. The brand label now uses white bold text, fixed readable width, and its own left grid column instead of sharing the centered title cell. Added editable `config/tankeye.json` keys `app.name`, `app.version`, and `app.title`, plus `AppConfigService` parsing and tests. Added UTF-8 default read/write rules to `AGENTS.md` and `docs/project-memory/OPERATING_LIMITS.md`.
- Result: The top-left header should show the configured software name/version beside the icon, and the top-center title should show `app.title`. Future display changes can be made by editing `config/tankeye.json`, for example `"app": { "name": "TankEye-Iris", "version": "1.4.3", "title": "截止阀抓取上料系统" }`.
- Verification: `cmake --build build --config Release --target tankeye-openvino_app_config_service_test tankeye-openvino_qt_app` passed; default `scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` passed; `git diff --check` reported no whitespace errors, only the known CRLF warning.
- Leftover: No package rebuild was run. Real GUI screenshot verification was not run because launching the app can self-sync the current user's Windows Startup entry outside the repository; ask before doing that environment-touching check.

## 2026-08-06 - Repair Packaging And Runtime Usage Docs Encoding

- Goal: Fix mojibake in `docs/PACKAGING_README.md` and `runtime/USAGE_GUIDE.txt`.
- Change: Rewrote both files as clean UTF-8 Chinese documentation, preserving current 1.4.2 build/package/startup guidance and adding the editable `config\tankeye.json` app display keys.
- Result: Both files are readable with `Get-Content -Encoding UTF8`; the previous mojibake text is removed.
- Verification: Read the first sections of both files with explicit UTF-8; searched both files for common mojibake markers (`缂`, `鐩`, `鍚`, replacement characters) with no remaining bad-content hits other than intentional “避免中文乱码” wording; `cmake --build build --config Release --target tankeye-openvino_qt_app` passed; default `scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` passed.
- Leftover: No package rebuild was run, so existing `dist\TankEye-Iris_1.4.2` still contains the older generated usage guide until repackaged.

## 2026-08-06 - Repackage 1.4.2 With Locked App Display

- Goal: Rebuild the runtime package and prevent field testers from changing the displayed app name, version, or centered title through ordinary packaged config edits.
- Change: Added `TANKEYE_LOCK_APP_DISPLAY=1` handling in `AppConfigService`; when set, the app ignores `config\tankeye.json` `app.name/app.version/app.title` and uses built-in defaults. The packaged launcher now sets that lock variable. Updated the packaged docs and fixed `scripts/package_runtime.ps1` to read and rewrite packaged JSON with explicit UTF-8 when disabling invalid machine limits.
- Result: Regenerated `dist\TankEye-Iris_1.4.2` and `dist\TankEye-Iris_1.4.2.zip`. Package config remains readable UTF-8 and keeps `app.version=1.4.3`, but ordinary package launches ignore runtime edits to these display fields because the launcher locks app display.
- Verification: `cmake --build build --config Release --target tankeye-openvino_app_config_service_test tankeye-openvino_qt_app` passed; default `scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` passed; packaging command passed with known `VCINSTALLDIR is not set` warning; manifest check shows `name=TankEye-Iris_1.4.2`, `version=1.4.2`, `runtime_only=True`, `files=129`; packaged launcher contains `TANKEYE_LOCK_APP_DISPLAY = "1"`; packaged `config\tankeye.json` reads cleanly as UTF-8; packaged exe hash matches `build\Release`; zip exists; package contains no `docs` or `AGENTS.md`.
- Leftover: No packaged GUI smoke was run because launching the app can self-sync the current user's Windows Startup entry outside the repository; ask before doing that environment-touching check.

## 2026-08-06 - Repackage TankEye-Iris 1.4.3 With Locked App Display

- Goal: Align the runtime package with the current 1.4.3 display/version settings.
- Change: Updated `scripts/package_runtime.ps1`, docs, and runtime usage notes to `1.4.3`; kept the packaged app-display lock so ordinary field edits to `config\tankeye.json` do not change the visible app name/version/title.
- Result: Generated `dist\TankEye-Iris_1.4.3` and `dist\TankEye-Iris_1.4.3.zip`. The package now carries `app.version=1.4.3` in its config, while the launcher enforces `TANKEYE_LOCK_APP_DISPLAY=1`.
- Verification: `cmake --build build --config Release --target tankeye-openvino_app_config_service_test tankeye-openvino_qt_app` passed; default `scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` passed; package generation passed with known `VCINSTALLDIR is not set` warning; manifest check shows `name=TankEye-Iris_1.4.3`, `version=1.4.3`, `runtime_only=True`, `files=129`; packaged launcher contains `TANKEYE_LOCK_APP_DISPLAY = "1"`; packaged `config\tankeye.json` reads cleanly as UTF-8; packaged exe hash matches `build\Release`; zip exists; package contains no `docs` or `AGENTS.md`.
- Leftover: If the user later needs a field-side version change without source access, there is still no secure way to allow that while keeping ordinary config edits blocked; that would require a separate approved override mechanism or a new package build.

## 2026-08-06 - Safe AUTO OpenVINO Device Probe And Field Guide Cleanup

- Goal: Fix the field crash where the app reached Qt event loop but exited after OpenVINO `AUTO` tried `GPU`, and remove internal-only sections from the field usage guide.
- Change: Added `tankeye-openvino_device_probe.exe` as an independent OpenVINO GPU compile probe; source and packaged launchers now resolve `-Device AUTO` by running the probe first and passing `GPU` to the Qt app only when it succeeds, otherwise passing `CPU`. The Qt app now treats raw `TANKEYE_OPENVINO_DEVICE=AUTO` as CPU for stability and logs the device probe result. `runtime\USAGE_GUIDE.txt` no longer contains the field-unneeded “界面标题和版本显示” or “当前关键行为” sections.
- Result: Regenerated `dist\TankEye-Iris_1.4.3` and `dist\TankEye-Iris_1.4.3.zip`; the package includes `tankeye-openvino_device_probe.exe`, `USAGE_GUIDE.txt`, OBB/SEG models, CPU/GPU OpenVINO plugins, and no `docs\` or `AGENTS.md`. Manifest now reports `files=130` because of the new probe executable.
- Verification: `cmake --build build --config Release --target tankeye-openvino_qt_app tankeye-openvino_device_probe` passed; PowerShell parse checks passed for `launch_tankeye.ps1`, `scripts\package_runtime.ps1`, and `scripts\create_desktop_shortcut.ps1`; default `scripts\run_build_tests.ps1 -BuildDir build -Configuration Release` passed; `git diff --check` reported no whitespace errors, only known CRLF warnings. Source-tree CPU smoke log `build\Release\logs\tankeye_20260806_181848.log` shows CPU model loading and no GPU attempt. Source-tree AUTO short-timeout smoke log `build\Release\logs\tankeye_20260806_182000.log` shows probe fallback to CPU and no app-side GPU attempt. Packaged CPU smoke log `dist\TankEye-Iris_1.4.3\logs\tankeye_20260806_182236.log` and packaged AUTO short-timeout smoke log `dist\TankEye-Iris_1.4.3\logs\tankeye_20260806_182347.log` both enter the Qt event loop and load models through CPU. Package manifest/hash/no-docs/no-AGENTS and packaged usage-guide assertions passed.
- Follow-up: Fresh empty-cache GPU probe timing on this PC was about 23.1 seconds, while cached probe timing was about 2 seconds. The default probe timeout was raised from 20 to 60 seconds so a normal first-run GPU compile is not misclassified as CPU fallback.
- Leftover: No real PLC, real camera, real device motion, real Windows reboot, or real GPU success path validation was run. GUI smoke tests used temporary `APPDATA` and `TANKEYE_SETTINGS_INI_PATH` to avoid modifying the current user's real Startup entry.

## 2026-08-06 - Isolate Engineering Settings Per Runtime Package

- Goal: Prevent different runtime package versions on the same Windows user account, such as `1.2`, `1.3`, `1.4`, and `1.4.3`, from sharing the same engineering settings through Qt user-level `QSettings`.
- Change: Source launch now sets `TANKEYE_SETTINGS_INI_PATH` to `build\Release\config\engineering_settings.ini`; packaged launch now sets it to `config\engineering_settings.ini` inside the package. The Qt app logs the effective engineering settings path at startup. Field and packaging docs now state that engineering settings are package-local and not shared across versions. No automatic migration from the old shared user-level settings is performed.
- Result: Regenerated `dist\TankEye-Iris_1.4.3` and `dist\TankEye-Iris_1.4.3.zip`; each package now creates or uses its own `config\engineering_settings.ini`.
- Verification: `cmake --build build --config Release --target tankeye-openvino_engineering_settings_service_test tankeye-openvino_qt_app tankeye-openvino_device_probe` passed; default `scripts\run_build_tests.ps1 -BuildDir build -Configuration Release` passed; packaging passed with the known `VCINSTALLDIR is not set` warning. Packaged CPU smoke log `dist\TankEye-Iris_1.4.3\logs\tankeye_20260806_191850.log` confirms `Engineering settings path: ...\config\engineering_settings.ini`, CPU model loading, and Qt event loop entry; the package created `dist\TankEye-Iris_1.4.3\config\engineering_settings.ini`.
- Leftover: Existing older packages still share the old user-level settings until rebuilt with this launcher change. Existing user-level engineering settings are intentionally not migrated.

## 2026-08-06 - Remove Internal Support Files From Field Package

- Goal: Respond to the user's field-package complaint that `README_RUNTIME.md` was mojibake and that `RELEASE_MANIFEST.json` / `THIRD_PARTY_NOTICES.md` were confusing, internal-facing files for现场调试人员.
- Change: Removed generation of `README_RUNTIME.md`, `RELEASE_MANIFEST.json`, `THIRD_PARTY_NOTICES.md`, and `SHA256SUMS.txt` from `scripts\package_runtime.ps1`; the runtime package now exposes `USAGE_GUIDE.txt` as the only field-facing documentation file.
- Result: Regenerated `dist\TankEye-Iris_1.4.3` and `dist\TankEye-Iris_1.4.3.zip` without those internal support files.
- Verification: PowerShell parser check passed for `scripts\package_runtime.ps1`; package generation passed with the known `VCINSTALLDIR is not set` warning; directory and zip checks confirmed the removed files are absent while `USAGE_GUIDE.txt`, `tankeye-openvino_qt_app.exe`, `tankeye-openvino_device_probe.exe`, OBB/SEG model files, and CPU/GPU OpenVINO plugins are present. `git diff --check` reported no whitespace errors, only known CRLF warnings.
- Leftover: No GUI, PLC, camera, or model-loading smoke was rerun for this documentation/package-surface cleanup.

## 2026-08-06 - Guard Engineering Settings Inputs From Hover Wheel Changes

- Goal: Prevent field users from accidentally changing engineering numeric or selection parameters while scrolling the engineering settings page.
- Change: Engineering settings numeric controls and combo boxes now use click-focused widgets: wheel events are ignored unless the widget or its internal editor already has focus. The behavior is applied through shared helper factories for all engineering `QDoubleSpinBox`, `QSpinBox`, and `QComboBox` controls. Runtime-log combo boxes were intentionally left unchanged.
- Result: Hovering the mouse over numeric fields or selection fields such as `正向/反向`, `0~360/-180~180`, axis mapping, or coordinate profile while scrolling should scroll the settings page instead of changing values; clicking or tabbing into the field still allows wheel-based adjustment.
- Verification: Added `tankeye-openvino_engineering_settings_spinbox_wheel_test`, covering focused vs unfocused wheel behavior for double spin boxes, integer spin boxes, and combo boxes. `cmake --build build --config Release --target tankeye-openvino_qt_app tankeye-openvino_engineering_settings_spinbox_wheel_test` passed; full `scripts\run_build_tests.ps1 -BuildDir build -Configuration Release` passed; static search confirmed the engineering settings dialog no longer directly creates raw numeric spin boxes or raw combo boxes; `git diff --check` reported no whitespace errors, only known CRLF warnings.
- Leftover: No real GUI manual scroll test, packaging, PLC, camera, or real device validation was run.

## 2026-08-06 - Package TankEye-Iris 1.4.5 And Prepare GitHub Upload

- Goal: Generate a fresh `TankEye-Iris_1.4.5` runtime package and upload the current maintained source changes to GitHub.
- Change: Updated `scripts/package_runtime.ps1`, `runtime/USAGE_GUIDE.txt`, `docs/PACKAGING_README.md`, and `config/tankeye.json` to 1.4.5 using explicit UTF-8 reads/writes; kept runtime package artifacts under ignored `dist/`. This upload also includes the current maintained source changes for safe AUTO OpenVINO probing, package-local engineering settings, field package surface cleanup, and engineering settings wheel guards.
- Result: Generated `dist\TankEye-Iris_1.4.5` and `dist\TankEye-Iris_1.4.5.zip`. The package contains the Qt app, device probe, models, CPU/GPU OpenVINO plugins, package-local config, and `USAGE_GUIDE.txt`; it does not contain `docs\`, `AGENTS.md`, `README_RUNTIME.md`, `RELEASE_MANIFEST.json`, `THIRD_PARTY_NOTICES.md`, or `SHA256SUMS.txt`.
- Verification: `cmake --build build --config Release --target tankeye-openvino_qt_app tankeye-openvino_device_probe tankeye-openvino_engineering_settings_service_test tankeye-openvino_engineering_settings_spinbox_wheel_test` passed; full `scripts\run_build_tests.ps1 -BuildDir build -Configuration Release` passed; package generation passed with the known `VCINSTALLDIR is not set` warning; package key file checks, exe hash checks, UTF-8 usage/config reads, lock/settings/probe launcher checks, and `git diff --check` passed with only known CRLF warnings. Direct packaged CPU + simulated PLC smoke used package-local `APPDATA` and settings paths; log `dist\TankEye-Iris_1.4.5\logs\tankeye_pkg145_smoke_20260806_215728.log` confirms main window creation, package-local engineering settings, OBB/SEG CPU compile, and model warmup, then the GUI was stopped after the timeout.
- Leftover: No real PLC, real camera, real device motion, real reboot, or GPU-success validation was run. Untracked `samples/` images/logs and `docs/USE_Readme.md` were not uploaded.

## 2026-08-08 - Project Structure Review

- Goal:梳理当前 `TankEye-Iris V5` 源码快照的项目结构、运行链路、构建目标和后续维护重点。
- Change:未修改业务源码；只追加本次项目梳理记录到项目记忆文档。确认当前目录是 GitHub 分支 zip 解压得到的源码快照，没有 `.git` 元数据。
- Result:明确项目主链路为 Qt 上位机入口、相机采集、OBB/SEG OpenVINO 推理、后处理、坐标转换、限位判断、Modbus TCP/模拟 PLC 输出；当前主要维护重心仍是减少 `GraspMainWindow`、工程设置、后处理和 overlay 周边重复逻辑。
- Verification:只读检查了 `AGENTS.md`、`docs/project-memory/*`、`CMakeLists.txt`、`config/tankeye.json`、`app/`、`core/`、`tests/`、`scripts/`；文档追加后用显式 UTF-8 读取验证。未运行真实 PLC、真实相机或设备动作。
- Leftover:当前目录不是 Git 仓库，无法查看分支、未提交改动或未跟踪文件；如后续需要提交/对比历史，需先恢复完整 `.git` 仓库或重新 clone。
## 2026-08-08 - Angle Calibration Anchor Model

- Goal: Convert engineering angle calibration into a direction-independent physical mapping anchored by `raw_angle_deg` and `target_angle_deg`.
- Change: Extended `AngleCalibrationSettings` with persistent anchor fields; kept `EngineeringSettingsService` and `AppConfigService` backward compatible; made the engineering settings dialog recompute the equivalent offset when direction or range changes; added coverage to `app_config_service_test`, `engineering_settings_service_test`, and `plc_result_contract_test`; fixed the save-button capture bug and the missing `app_config_service_test` link dependency.
- Result: The same calibration anchor now yields equivalent offsets across forward/reverse and angle-range changes, while the PLC register contract stays unchanged and legacy configs still load.
- Verification: `cmake --build build --config Release --target tankeye-openvino_qt_app tankeye-openvino_engineering_settings_service_test tankeye-openvino_plc_result_contract_test tankeye-openvino_app_config_service_test` passed; `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release` passed; source-tree `launch_tankeye.ps1 -Device CPU -SimulatePlc -AutoLoadModels` smoke passed, with logs in `build/Release/logs/tankeye_20260808_173414.log`.
- Leftover: No real PLC, real camera, or real device motion was exercised.

## 2026-08-08 - Angle Calibration Display Mapping

- Goal: Keep angle calibration as a numeric image-angle to machine-angle mapping, not a whole-frame rotation, while making the main display show the calibrated semantics for head-type results such as `大上左` and `小上左`.
- Change: Added per-detection `display_angle_deg` / `has_display_angle`; `FrameProcessingService` now computes it through the same angle calibration and head-type compensation path used for PLC output. The main overlay angle badge and right-side angle labels use this display angle first, falling back to raw-angle calibration only for older/manual result objects. `frame_overlay_test` now covers the case where no PLC command pose is available but the overlay still draws the calibrated display angle instead of raw `angle_deg`.
- Result: CV still outputs raw image angle; the app does not rotate the full image. PLC register semantics stay unchanged. The screen angle label now follows the calibrated numeric result even when command-pose coordinates cannot be drawn.
- Verification: `cmake --build build --config Release --target tankeye-openvino_qt_app tankeye-openvino_frame_overlay_test` passed; `scripts/run_build_tests.ps1 -BuildDir build -Configuration Release -Filter tankeye-openvino_frame_overlay_test.exe` passed; full `scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` passed. Source-tree CPU + simulated PLC GUI smoke passed with temporary `APPDATA` and log `build/Release/logs/tankeye_20260808_180318.log`, confirming simulated PLC, main window creation, package-local engineering settings path, OBB/SEG CPU model loading, and clean Qt exit.
- Leftover: No real PLC, real camera, real device motion, real reboot, or GPU-success validation was run. Field-side visual confirmation with real images is still recommended for `大上左` and `小上左` overlays.

## 2026-08-08 - Remove Normal Overlay Angle Badge

- Goal: Remove the black `D508=... angle` badge from the normal camera/image overlay because it blocks the operator's view and the user does not want that on-screen label.
- Change: Removed the `DrawAngleBadge` overlay path from `frame_overlay.cpp`; kept `display_angle_deg` for the right-side panel, target list, calibration semantics, and PLC-aligned display data. Updated `frame_overlay_test` to assert the badge region remains blank in normal mode.
- Result: Normal mode no longer draws the black angle label box. Arrows, OBB/夹爪框, head-type calculations, right-side calibrated angle display, and PLC output semantics remain unchanged.
- Verification: `cmake --build build --config Release --target tankeye-openvino_qt_app tankeye-openvino_frame_overlay_test` passed; `scripts/run_build_tests.ps1 -BuildDir build -Configuration Release -Filter tankeye-openvino_frame_overlay_test.exe` passed; full `scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` passed.
- Leftover: No real camera/PLC/device validation or manual screenshot smoke was run for this visual cleanup.

## 2026-08-08 - Keep Overlay Geometry Raw After Angle Calibration

- Goal: Fix the field symptom from `build/Release/logs/tankeye_20260808_181624.log` where angle calibration made normal-mode image boxes look rotated/skewed.
- Change: Stopped using `plc_command_angle_deg` to rotate normal/debug overlay geometry. The overlay may still translate to a command center when one exists, but OBB boxes, gripper boxes, and AC arrows keep the raw CV image direction. Calibrated `display_angle_deg` remains available for right-side numeric display and PLC-aligned semantics.
- Result: Angle calibration now only aligns numeric image angle to mechanical angle; it no longer visually rotates the image overlay frame geometry. PLC `D504` output, head-type selection, and calibration storage are unchanged.
- Verification: `cmake --build build --config Release --target tankeye-openvino_qt_app tankeye-openvino_frame_overlay_test` passed; focused `scripts/run_build_tests.ps1 -BuildDir build -Configuration Release -Filter tankeye-openvino_frame_overlay_test.exe` passed; full `scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` passed.
- Leftover: No real camera/PLC/device validation or manual screenshot smoke was run after this geometry fix.

## 2026-08-08 - Restore Head-Type Visual Angle Compensation

- Goal: Restore the expected visual effect for per-head-type angle compensation (`大上左` / `大上右` / `小上左` / `小上右`) without reintroducing global calibration rotation into the image overlay.
- Change: Added per-detection `display_geometry_angle_offset_deg` / `has_display_geometry_angle_offset`. `FrameProcessingService` fills it only from `HeadTypeCompensation::angle_offset_deg`; `frame_overlay.cpp` rotates OBB/gripper/AC-arrow geometry by this type-specific visual offset while still ignoring global angle calibration for geometry.
- Result: Changing the global angle calibration changes numeric display and PLC `D504` only. Changing a specific head-type angle compensation visibly rotates that target's overlay geometry and still participates in final PLC angle output.
- Verification: `cmake --build build --config Release --target tankeye-openvino_qt_app tankeye-openvino_frame_overlay_test` passed; focused `scripts/run_build_tests.ps1 -BuildDir build -Configuration Release -Filter tankeye-openvino_frame_overlay_test.exe` passed; full `scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` passed. Tests now cover both "global calibration does not rotate geometry" and "head-type visual offset rotates geometry".
- Leftover: No real camera/PLC/device validation or manual screenshot smoke was run after this visual-compensation fix.

## 2026-08-08 - Angle Reverse Means Opposite Gripper Pose

- Goal: Correct the meaning of angle `正向/反向` so reverse is the opposite gripper pose on the same calibrated axis, not `raw_angle * -1 + offset`.
- Change: `ApplyPlcAngleCalibration()` now computes `raw + global_offset + head_type_offset` first, then subtracts 180 degrees when reverse is selected, and only then applies `0~360` or `-180~180` formatting. Engineering-settings anchors are stored as forward physical target angles; the dialog converts reverse-mode displayed targets back to forward anchors before saving/calculating.
- Result: With `raw=30` and `offset=15`, forward writes/displays `45`; reverse writes/displays `225` in `0~360` and `-135` in `-180~180`. Head-type angle compensation is included before the reverse 180-degree pose flip.
- Verification: `cmake --build build --config Release --target tankeye-openvino_plc_result_contract_test tankeye-openvino_engineering_settings_service_test tankeye-openvino_app_config_service_test tankeye-openvino_qt_app` passed; focused PLC/settings/config tests passed; full `scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` passed. CPU + simulated PLC GUI smoke reached main window, Qt event loop, OBB/SEG CPU model loading, and warmup in `build/Release/logs/tankeye_20260808_191607.log`; the GUI stayed in the event loop until the 60s smoke timeout and the residual process was stopped afterward.
- Leftover: No real PLC, real camera, real device motion, real reboot, GPU-success path, or manual engineering-settings click-through was run.

## 2026-08-08 - 写入严格重构替换规则

- 目标：把用户要求的严格重构规则写入项目记忆，后续代码修改必须按“先删旧逻辑、再写新逻辑”的方式执行。
- 修改：在 `docs/project-memory/OPERATING_LIMITS.md` 新增中文“代码重构替换规则”和“UTF-8 读写规则”，要求先输出删除清单、再输出变更清单和完整函数/文件，并禁止保留注释掉的旧代码、备用实现、重复逻辑和 TODO/FIXME/HACK 式拖延清理。
- 结果：后续代码变更必须默认使用 UTF-8 读写仓内文本，且同一功能只能保留一份实现；如果用户认为删除清单有误，只保留用户明确要求保留的内容。
- 验证：用显式 UTF-8 读取 `docs/project-memory/OPERATING_LIMITS.md`、`docs/project-memory/TASK_HISTORY.md` 和 `docs/project-memory/MEMORY.md`，确认规则已写入并替换为中文。未运行构建/测试，因为本次只改项目规则文档，不改源码和运行逻辑。
- 遗留：无源码行为变化。

## 2026-08-08 - 默认模型目录切换到 DG_8_weights

- 目标：将默认自动加载模型路径从 `models/weights/` 改为用户指定的 `models/DG_8_weights/`。
- 修改：`launch_tankeye.ps1` 默认 OBB/SEG 候选路径改为 `models\DG_8_weights`；`scripts/package_runtime.ps1` 的模型校验、复制和包内启动器默认路径同步；`scripts/deploy_windows.ps1` 和 `scripts/export_pt_to_openvino.py` 同步新目录；`PROJECT_CONFIGURATION.md` 与 `MEMORY.md` 更新当前默认路径描述。
- 结果：使用 `-AutoLoadModels` 或 `-StartupProfile AutoStart` 时，源码启动脚本默认加载 `models\DG_8_weights\best_obb.xml` 与 `models\DG_8_weights\best_seg.xml`；重新打包后的运行包也会默认携带并加载 `models\DG_8_weights`。
- 验证：确认 `models\DG_8_weights` 下存在 `best_obb.xml/.bin` 与 `best_seg.xml/.bin`；PowerShell 解析检查通过；Python 导出脚本编译检查通过；静态搜索确认可执行脚本中的默认模型目录已切到 `models\DG_8_weights`。
- 遗留：本轮未重新构建主程序、未重新生成运行包、未启动 GUI 加载模型、未连接真实 PLC/相机。

## 2026-08-08 - 物料工程方案与导入导出

- 目标：支持 DG_8、DG_10 等不同物料使用独立工程方案；切换方案时恢复各自设置和模型，避免已调参数互相覆盖，并支持迁移到新电脑。
- 修改：新增 `app/qt/workflow/project_profile_store.h/.cpp`，以实例本地 `config/project_profiles/<name>/profile.json` 保存物料相关模型路径、阈值、曝光、角度校准、OBB 后处理、头部补偿、轴向补偿、上下限、九点标定和夹爪尺寸；当前激活方案名写入实例本地 `engineering_settings.ini`。`GraspMainWindow` 增加首次 DG_8 迁移、方案加载/保存、停止运行后切换并重载模型；工程设置新增方案选择、新建、保存、另存为、删除、JSON/ZIP 导入导出和应用方案；`scripts/package_runtime.ps1` 与 CMake 同步方案目录；新增 `tests/project_profile_store_test.cpp`；`EngineeringSettingsService` 增加相机 IP 独立全局读写接口，曝光改由方案保存。
- 结果：首次运行会生成 DG_8 方案；保存 DG_10 后切回 DG_8 再切回 DG_10 时，各自物料参数和模型路径独立恢复。导入 JSON 只导配置，ZIP 可选择只带配置或带 OBB/SEG 四个 XML/BIN 模型；导入不会自动切换，需点击 `应用方案`，应用时自动停止相机和 PLC 轮询并重新加载模型。
- 验证：`cmake --build build --config Release --target tankeye-openvino_qt_app tankeye-openvino_project_profile_store_test` 通过；`project_profile_store_test` 直接运行和 CTest 通过，覆盖 DG_8/DG_10 隔离、激活方案持久化、JSON 导入、含四个模型文件的 ZIP 导出/导入；默认 `scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；PowerShell 启动/打包脚本解析通过；源码 `launch_tankeye.ps1 -Device CPU -SimulatePlc -AutoLoadModels` 真实启动冒烟进入 Qt 事件循环并完成 OBB/SEG CPU 加载，方案文件生成于 `build/Release/config/project_profiles/DG_8/profile.json`。
- 遗留：未重新生成正式 `dist` 运行包；未人工点击工程设置完成 DG_8/DG_10 视觉回归；未连接真实 PLC、真实相机或执行真实抓取动作。需要现场重点确认应用方案时的 UI 提示、模型加载等待、相机停止/恢复和新电脑导入模型后的路径。

## 2026-08-08 - 修复新建物料方案关闭窗口和参数未初始化

- 目标：修复新建工程方案后工程设置窗口被关闭、控件仍显示 DG_8 参数且运行方案被误改的问题。
- 修改：`engineering_settings_dialog.cpp` 增加独立编辑方案选择和完整草稿控件刷新；新建改为保存 `CreateBlankProjectProfile()` 安全空白方案；保存、另存为、导入、删除保持窗口打开并只操作编辑草稿。`grasp_main_window.cpp/.h` 增加 pending 方案和失败回滚，应用前校验 OBB/SEG XML 与 BIN，active 方案名改为模型成功加载后才持久化。`project_profile_store_test.cpp` 增加空白方案默认值断言。
- 结果：新建 DG_10 后窗口不关闭，控件刷新为空白安全默认值，当前运行 DG_8 不变；另存为继续复制当前编辑内容；应用无模型方案会失败且 DG_8 保持运行，完整方案模型加载成功后才切换 active。
- 验证：Release 构建 `tankeye-openvino_qt_app` 和 `tankeye-openvino_project_profile_store_test` 通过；`scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；CPU + 模拟 PLC 真实启动进入 Qt event loop，并完成 DG_8 OBB/SEG CPU 加载和 warmup，日志为 `build/Release/logs/tankeye_20260808_213033.log`。直接运行测试 exe 未注入运行库 PATH 因 DLL 缺失退出，已以项目测试脚本的完整通过结果为准。
- 遗留：尚未人工点击验证工程设置窗口、新建 DG_10 控件清空、应用失败提示和 DG_8/DG_10 来回恢复；未连接真实 PLC/相机。

## 2026-08-08 - 打包 TankEye-Iris 1.4.6

- 目标：基于物料工程方案新建修复后的源码生成 1.4.6 运行包。
- 修改：统一 `config/tankeye.json`、`app_config_service.h`、`scripts/package_runtime.ps1`、`runtime/USAGE_GUIDE.txt` 和 `docs/PACKAGING_README.md` 的版本号为 1.4.6；生成目录包和 ZIP。
- 结果：产物为 `dist/TankEye-Iris_1.4.6` 与 `dist/TankEye-Iris_1.4.6.zip`，包内带 DG_8 默认方案、四个 DG_8 模型文件、主程序、设备探测程序和包内工程设置存储路径。
- 验证：Release 主程序/设备探测程序构建通过；`scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；包内版本和文件存在性检查通过；包内 CPU + 模拟 PLC 启动进入 Qt event loop，OBB/SEG CPU 模型加载和 warmup 成功，日志为 `dist/TankEye-Iris_1.4.6/logs/tankeye_20260808_213652.log`。
- 遗留：包内启动时写入当前用户 Startup 入口出现权限警告；不影响本次程序启动和模型加载。未连接真实 PLC/相机。

## 2026-08-08 - 操作工主界面物料方案选择

- 目标：让操作工在主界面选择 DG_8、DG_10 等物料方案，同时禁止操作工直接修改方案参数。
- 修改：`grasp_main_window.h/.cpp` 在“当前目标参数”卡片加入物料方案下拉框；新增 active 方案回填和操作工选择处理；选择后使用二次确认，复用既有方案切换、模型校验、停止流程、异步加载和失败回滚链路；模型加载期间禁用下拉框。
- 结果：主界面只提供方案选择，不提供参数编辑、新建、保存、导入、导出和删除；管理员工程设置继续作为唯一参数维护入口。空白方案可显示但应用时会因模型缺失安全失败，不影响当前运行方案。
- 验证：`cmake --build build --config Release --target tankeye-openvino_qt_app` 通过；`scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；CPU + 模拟 PLC 启动进入 Qt event loop，DG_8 OBB/SEG 加载和 warmup 成功，日志为 `build/Release/logs/tankeye_20260808_221011.log`。
- 遗留：尚未人工点击确认框、切换中禁用状态和 DG_8/DG_10 来回切换；Qt 5.15 对 `QComboBox::activated` 产生兼容性弃用警告，不影响当前构建。

## 2026-08-08 - 修正操作工方案选择器位置

- 目标：根据现场截图，将方案选择器从当前主目标参数卡片内部移动到“当前目标参数”标题右侧。
- 修改：删除卡片内部的物料方案行；在结果区域标题行增加固定宽度的方案下拉框，与标题并排显示；不改变操作工二次确认、模型校验、异步加载和失败回滚逻辑。
- 结果：方案选择器现在位于右侧栏“当前目标参数”标题同一行右侧，不再占用目标坐标、角度和大小头参数区域。
- 验证：`tankeye-openvino_qt_app` Release 构建通过；`scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；CPU + 模拟 PLC 启动进入 Qt event loop，日志为 `build/Release/logs/tankeye_20260808_221614.log`。
- 遗留：尚未人工截图确认不同 DPI/窗口宽度下标题和下拉框的最终视觉间距。

## 2026-08-08 - 加宽操作工方案选择框

- 目标：让主界面方案名称显示更清楚。
- 修改：将“当前目标参数”标题右侧方案下拉框宽度从 `SC(126)` 调整为 `SC(180)`。
- 结果：DG_8、DG_10 及较长方案名称有更充足显示空间。
- 验证：`cmake --build build --config Release --target tankeye-openvino_qt_app` 通过。
- 遗留：未进行不同分辨率下的人工截图确认。

## 2026-08-08 - 方案选择器自适应与圆角优化

- 目标：修复方案选择框不随侧栏缩放、缺少“方案选择：”文案和边角过硬的问题。
- 修改：标题行现在按“当前目标参数 + 方案选择： + 下拉框”排列；下拉框从固定宽度改为最小宽度加 Expanding；在响应式侧栏刷新中设置圆角、深色背景、蓝灰边框、悬停/焦点/禁用和下拉列表样式。
- 结果：方案选择框可随侧栏宽度变化，操作工能明确识别控件用途，方案切换逻辑和管理员权限不变。
- 验证：`cmake --build build --config Release --target tankeye-openvino_qt_app` 通过；`scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；CPU + 模拟 PLC 启动进入 Qt event loop，并完成 DG_8 OBB/SEG CPU 模型加载和 warmup，日志为 `build/Release/logs/tankeye_20260808_222718.log`。
- 遗留：尚未在不同窗口宽度和 DPI 下人工截图确认最终间距。

## 2026-08-08 - 统一方案选择框箭头颜色

- 目标：让方案选择框右侧下拉箭头与方案文字使用一致颜色。
- 修改：由 `ProjectProfileSelector::paintEvent()` 直接在 Qt 下拉箭头区域绘制空心折线 `⌄`；正常状态使用 `#f4f8fc`，禁用状态使用 `#9aaab7`。折线最小尺寸设为 `12×7` 像素，使用圆头 2 像素描边；当 Qt 未返回有效箭头区域时，使用控件最右侧 28 像素作为绘制回退区域。
- 结果：方案选择框不再依赖 Windows 原生箭头；右侧应始终显示与方案文字同色的下向折角。
- 验证：`cmake --build build --config Release --target tankeye-openvino_qt_app` 通过；`scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过。
- 遗留：未进行目标机器的人工截图确认。

## 2026-08-08 - 打包 TankEye-Iris 1.4.7

- 目标：将当前包含操作工方案选择框和空心下拉箭头的版本打包为 1.4.7。
- 修改：统一 `config/tankeye.json`、`app/qt/workflow/app_config_service.h`、`scripts/package_runtime.ps1`、`runtime/USAGE_GUIDE.txt` 和 `docs/PACKAGING_README.md` 的版本号与打包命令为 1.4.7。
- 结果：生成 `dist/TankEye-Iris_1.4.7` 和 `dist/TankEye-Iris_1.4.7.zip`；包内包含 DG_8 工程方案、四个 DG_8 模型文件、Qt 主程序和设备探测程序。
- 验证：Release 构建 `tankeye-openvino_qt_app`、`tankeye-openvino_device_probe` 通过；完整 `scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` 通过；包内版本为 1.4.7，主程序、设备探测程序、方案文件和四个模型文件检查通过。
- 遗留：未启动包内 launcher，避免其写入当前用户桌面/Startup 快捷方式；未连接真实 PLC、相机或设备。

## 2026-08-08 - 补齐 DG_10 模型并重打包 1.4.7

- 目标：修复 1.4.7 运行包只包含 DG_8 模型、缺少 `DG_10_weights` 的问题。
- 修改：`scripts/package_runtime.ps1` 用统一模型目录循环替换仅复制 DG_8 的写死逻辑，校验并复制 DG_8/DG_10 两套各四个 XML/BIN；方案目录优先从 `build/Release/config/project_profiles` 复制，以带入当前 DG_10 草稿方案。
- 结果：重新生成的 `dist/TankEye-Iris_1.4.7` 和 ZIP 均包含 DG_8、DG_10 两个方案，以及 `models/DG_8_weights`、`models/DG_10_weights` 的八个模型文件。
- 验证：PowerShell 解析检查和重新打包通过；包内八个模型文件、两个方案文件和 ZIP 均存在；完整 `scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` 通过。
- 遗留：DG_10 方案当前模型路径为空且坐标转换关闭，管理员仍需在工程设置中填写 `models/DG_10_weights` 下的 OBB/SEG XML 后才能安全应用；未启动包内 launcher，未连接真实 PLC、相机或设备。

## 2026-08-08 - 工程设置支持按文件夹自动识别模型

- 目标：保留模型 A/B 分别手动选择，同时允许管理员选择一个标准模型文件夹自动填入两份模型。
- 修改：`engineering_settings_dialog_helpers` 新增 `DetectStandardModelFolder()` 和 `StandardModelFolderSelection`，严格检查 `best_obb.xml/.bin`、`best_seg.xml/.bin` 四个文件；工程设置基础设置区域新增“选择模型文件夹”，成功只更新当前草稿控件，失败显示缺失文件且保留原路径；原有两个手动“浏览”按钮不变。新增 `tests/model_folder_detector_test.cpp` 与 CMake 测试目标。
- 结果：DG_8/DG_10 标准模型目录可一次选择并自动分配模型 A/B；不会自动保存、应用、停止当前流程或修改其他物料参数。
- 验证：`cmake --build build --config Release --target tankeye-openvino_qt_app tankeye-openvino_model_folder_detector_test` 通过；完整 `scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` 通过；新测试覆盖完整目录、缺失 BIN、非标准文件名和不存在目录。
- 遗留：尚未人工点击工程设置确认按钮位置、弹窗文案和草稿保存语义；尚未执行 CPU + 模拟 PLC GUI 启动，因为启动链路会同步写入用户 Startup 入口；未连接真实 PLC、相机或设备。

## 2026-08-08 - 打包 TankEye-Iris 1.4.8

- 目标：将包含模型文件夹自动识别功能的当前版本发布为 1.4.8。
- 修改：统一应用配置、打包脚本、运行说明和打包文档版本为 1.4.8；保留 1.4.7 旧产物。
- 结果：生成 `dist/TankEye-Iris_1.4.8` 和 `dist/TankEye-Iris_1.4.8.zip`，包内包含模型文件夹识别功能、DG_8/DG_10 两个方案及两套共八个模型文件。
- 验证：Release 构建 `tankeye-openvino_qt_app`、`tankeye-openvino_device_probe` 通过；完整 `scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` 通过；包内版本为 1.4.8，目录包 130 个文件，关键程序、方案、模型和使用说明检查通过。
- 遗留：未启动包内 launcher，未连接真实 PLC、相机或设备。

## 2026-08-09 - 工程设置保存后实时生效

- 目标：修复同一 active 工程方案点击“保存设置”后必须重启程序才能生效的问题。
- 修改：`GraspMainWindow` 新增 `applySavedEngineeringSettings()`，保存 active 方案后立即停止运行流程、清空结果、应用完整方案参数并按需后台重载模型；非 active 方案只保存不切换；抽取 `validateProjectProfileModels()` 供保存实时应用和“应用方案”共用；保存失败时保留草稿并通过既有 pending 状态恢复运行参数。
- 结果：保存 DG_8 当前方案后无需重启即可更新运行态；保存 DG_10 等非运行方案不会影响当前 DG_8；抓取中保存会停止抓取，操作工需重新点击“开始”；工程设置窗口保持打开。
- 验证：`cmake --build build --config Release --target tankeye-openvino_qt_app tankeye-openvino_model_folder_detector_test` 通过；完整 `scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` 通过。
- 遗留：尚未人工点击验证保存后阈值/角度/坐标/限位实时变化、抓取停止和重新开始；尚未执行 CPU + 模拟 PLC GUI 冒烟，未连接真实 PLC、相机或设备。
## 2026-08-09 - 工程设置保存实时生效验证补充

- 目标：完成上一轮实时保存改动的构建、自动测试和安全启动验证。
- 修改：未新增业务代码；补充验证记录。
- 结果：active 方案保存走实时应用入口，非 active 方案只保存；模型加载失败继续走运行态回滚，工程设置不因保存而关闭。
- 验证：Release 构建 `tankeye-openvino_qt_app` 与 `tankeye-openvino_project_profile_store_test` 通过；完整 `scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；CPU + 模拟 PLC 真实启动冒烟通过，日志为 `build/Release/logs/tankeye_20260809_093157.log`，确认 Qt event loop、DG_8 OBB/SEG 模型加载和 warmup。
- 遗留：未进行人工 UI 点击验收；未连接真实 PLC、相机或设备。

## 2026-08-09 - 升级严格重构替换规则

- 目标：把用户补充的“最小改动原则”和更严格的代码修改流程写入项目记忆规则。
- 修改：替换 `docs/project-memory/OPERATING_LIMITS.md` 中已有“代码重构替换规则”，补充只改指定功能、禁止顺手优化、禁止改动未提及逻辑、变更清单必须声明未改动部分、自检必须确认未改动其他现有逻辑。保留并继续强调 UTF-8 读写规则。
- 结果：后续代码修改必须先列删除清单，再按最小改动范围替换旧逻辑；同一功能只保留一份实现，不能保留注释旧代码、备用方案或重复逻辑。
- 验证：用显式 UTF-8 读取 `docs/project-memory/OPERATING_LIMITS.md`、`docs/project-memory/TASK_HISTORY.md` 和 `docs/project-memory/MEMORY.md` 验证内容可读。未运行构建/测试，因为本次只改项目规则文档，不改源码和运行逻辑。
- 遗留：无源码行为变化。
## 2026-08-09 - 工程方案保存后延迟生效

- 目标：保存当前 active 方案时不停止相机/PLC、不清空画面和检测结果，改为下一次图片检测或 PLC 开始时应用新参数。
- 修改：`GraspMainWindow` 增加待生效方案和待启动动作状态；保存 active 方案只校验、写盘并登记待生效配置；`startDetection()` 与 `togglePlcLinkMode()` 在开始前应用待生效方案；异步模型加载增加完成回调；`GraspWorkflow::loadModels()` 改为事务式替换，加载失败保留旧模型；切换工程方案时清理过期待生效状态。
- 结果：当前运行过程继续使用旧参数；图片检测需要再次点击“开始检测”才接管，PLC 需要关闭后再次点击“开始”才接管；保存成功弹窗提示下一次开始生效；错误模型路径在保存时直接拦截。
- 验证：`cmake --build build --config Release --target tankeye-openvino_qt_app` 通过；完整 `scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；CPU + 模拟 PLC 启动冒烟日志为 `build/Release/logs/tankeye_20260809_095938.log`，确认 Qt event loop、DG_8 OBB/SEG 模型加载和 warmup。
- 遗留：尚未人工点击验证保存后画面保留、图片再次检测接管、PLC 关闭再开始接管和模型失败回滚；未连接真实 PLC、相机或设备。

## 2026-08-09 - 全局角度增量校准与头型姿态统一

- 目标：让全局角度偏移按当前转换后的机械基准角累计校准，避免 `-67.55` 显示角被计算为难理解的 `-292.45` 等价偏移；保持全局校准不旋转图像框，大小头补偿继续影响对应 D504 和图像框。
- 修改：删除旧 `CalculateAngleCalibrationOffsetFromRawAngle()` 原始角绝对重算入口，新增最短角差规范化、首次锚点重算和累计偏移计算；“取当前角度”只记录视觉抓取角和正向物理目标锚点，不覆盖既有偏移；“计算校准”以旧偏移后的当前机械角计算增量并累加。正反向或角度范围切换时按首次锚点重算。工程设置新增视觉抓取角、当前机械角和最终 D504 三段只读诊断；工程方案读写把等价偏移规范为 `[-180, 180)` 便于排查。
- 结果：截图口径中视觉基准 `-67.55 deg`、目标 `0 deg`、旧偏移 `0 deg` 时得到 `+67.55 deg`；同一目标再次计算保持该偏移。D508 头型补偿仍只在最终命令与对应图像框姿态阶段叠加，全局偏移不参与图像几何旋转。
- 验证：`cmake --build build --config Release --target tankeye-openvino_plc_result_contract_test tankeye-openvino_engineering_settings_service_test tankeye-openvino_project_profile_store_test tankeye-openvino_qt_app` 通过；`powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过。未启动 GUI，因为当前启动链路可能写入当前用户 Startup 项；未连接真实 PLC、相机或机械设备。
- 遗留：尚未人工打开工程设置验证三段诊断的布局、连续计算、正反向/范围切换后的数值和大小头框旋转效果；需要在获得系统 Startup 写入许可后再做安全 GUI 冒烟，真实 PLC/相机/设备动作仍未验证。

## 2026-08-09 - 固定角度输入并分离运行态与草稿 D504

- 目标：修复“取当前角度”与输入框不一致，以及切换正向/反向、角度范围时自动改写“当前显示角度”和“机器目标角度”的问题；同时把工程设置草稿预览 D504 与右侧当前真实运行 D504 明确分开。
- 修改：`engineering_settings_dialog.cpp` 移除方向/范围切换时对两个角度输入框的自动 `setValue()`；“视觉基准角”输入框在点击“取当前角度”时按当时口径写入一次，之后不再被方向/范围切换改写；“机器目标角度”固定表示正向机械基准目标，不再随反向切换变成等价值。诊断区改为显示“视觉原始角、草稿机械基准角、草稿预览 D504、当前运行 D504”。`plc_result_contract_test` 新增/替换为目标 `39°` 时正向草稿 D504 为 `39°`、反向为 `-141°` 的合同断言。
- 结果：右侧主界面继续只显示当前真实运行值；工程设置弹窗单独显示未生效草稿的机械基准角和预览 D504，避免把运行态 `-164°` 与草稿 `39°` 混为一谈。反向只改变草稿/最终 D504 预览口径，不再改写你已经取到的当前角度值和手动输入的目标角。
- 验证：`cmake --build build --config Release --target tankeye-openvino_plc_result_contract_test tankeye-openvino_qt_app tankeye-openvino_project_profile_store_test tankeye-openvino_engineering_settings_service_test` 通过；`powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过。未启动 GUI，因为当前启动链路可能写入当前用户 Startup 项；未连接真实 PLC、相机或机械设备。
- 遗留：尚未人工点击工程设置验证“视觉基准角/机器目标角度”在切换正反向和范围时保持不变，以及“草稿预览 D504 / 当前运行 D504”文案和布局是否足够清晰；未做真实 PLC、真实相机或真实抓取动作验收。

## 2026-08-09 - 打包 TankEye-Iris 1.4.9.1

- 目标：把当前源码和运行包版本统一到 1.4.9.1，并生成新的正式运行包与 ZIP。
- 修改：更新 `config/tankeye.json`、`app/qt/workflow/app_config_service.h`、`scripts/package_runtime.ps1`、`runtime/USAGE_GUIDE.txt` 和 `docs/PACKAGING_README.md` 中的当前版本号、示例命令和运行包路径；执行正式打包生成 `dist/TankEye-Iris_1.4.9.1` 与 `dist/TankEye-Iris_1.4.9.1.zip`。
- 结果：源码内置显示版本、运行包默认发布名、使用说明和打包文档统一为 1.4.9.1；新包包含主程序、设备探测程序、DG_8/DG_10 模型、使用说明和配置文件。
- 验证：`cmake --build build --config Release --target tankeye-openvino_qt_app tankeye-openvino_device_probe` 通过；`powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；`powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\package_runtime.ps1 -BuildDir build -ReleaseName TankEye-Iris_1.4.9.1 -Force` 成功生成目录包和 ZIP；已确认包内主程序、设备探测程序、`platforms\qwindows.dll`、DG_8/DG_10 模型、`USAGE_GUIDE.txt` 与 ZIP 全部存在，主程序和设备探测程序与本次 `build\Release` hash 一致，包内 `config\tankeye.json` 版本和 `USAGE_GUIDE.txt` 首行均为 1.4.9.1，且运行包不包含 `docs\` 与 `AGENTS.md`。打包过程出现 `VCINSTALLDIR is not set` 警告，但产物生成成功。
- 遗留：本轮未启动包内 launcher，避免触发桌面或 Startup 快捷方式写入；未连接真实 PLC、真实相机或真实设备。

## 2026-08-09 - 角度范围独立偏移

- 目标：修正 `0~360` 与 `-180~180` 共用一份全局角度偏移的问题，让两种角度口径各自计算、保存和恢复自己的偏移。
- 修改：`AngleCalibrationSettings`、工程设置草稿、主窗口运行态和工程方案 JSON/INI 增加两套偏移槽位及有效标记；PLC 角度合同新增按范围规范化和按范围累计校准；工程设置切换范围时只切换对应偏移槽位，不改写“视觉基准角”和“机器目标角度”；增加两范围独立偏移和持久化回归测试。
- 结果：以视觉 `292.45°`、正向目标 `39°` 为例，`0~360` 得到 `-253.45°`，`-180~180` 得到 `+106.55°`；两套值可以独立累计、切换和保存，大小头补偿/画框联动逻辑保持不变。
- 验证：Release 构建 `tankeye-openvino_plc_result_contract_test`、`tankeye-openvino_engineering_settings_service_test`、`tankeye-openvino_project_profile_store_test`、`tankeye-openvino_qt_app` 通过；上述三个定向测试通过；完整 `scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；重新生成 `dist/TankEye-Iris_1.4.9.1` 和 `dist/TankEye-Iris_1.4.9.1.zip` 成功，过程仅出现已知 `VCINSTALLDIR is not set` 警告。
- 遗留：未启动 GUI 做人工点击验收，未连接真实 PLC、真实相机或真实机械设备；需要现场确认范围切换时偏移框、草稿 D504 和实际运行 D504 的显示口径。

## 2026-08-09 - 全局角度校准回退到 new_pc2 老逻辑

- 目标：按用户确认，将当前全局角度校准从 anchor、累计校准和双范围 offset 口径回退到 `TankEye_source_for_new_pc2` 的老逻辑。
- 修改：`ApplyPlcAngleCalibration()` 恢复为 `原始角度 * 正反方向 + 全局偏移 + 大小头角度补偿`；`AngleCalibrationSettings`、工程方案、应用配置、主窗口运行态和工程设置草稿只保留一份 `offset_deg`、`direction`、`range_mode`；工程设置角度区删除“取当前角度”“计算校准”和草稿/运行 D504 诊断，只保留角度偏移、正反向、角度范围、中心偏移和原有大小头补偿；默认 DG_8 方案 JSON 删除不再写出的 anchor 字段。
- 结果：正向 D504 为 `raw + offset + type_offset`，反向 D504 为 `-raw + offset + type_offset`；`0~360` 与 `-180~180` 只负责最终格式化，不再各自维护独立偏移；大小头补偿继续保留。
- 验证：`cmake --build build --config Release --target tankeye-openvino_plc_result_contract_test tankeye-openvino_engineering_settings_service_test tankeye-openvino_project_profile_store_test tankeye-openvino_qt_app` 通过；完整 `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过。
- 遗留：未启动 GUI，未连接真实 PLC、真实相机或真实机械设备；仍需人工打开工程设置确认角度区域精简后的实际布局和现场夹爪角度是否与老逻辑一致。

## 2026-08-09 - 恢复全局角度手动计算入口

- 目标：在保持 `new_pc2` 老全局角度公式的前提下，恢复工程设置中的“取当前角度”“机械目标角度”“计算校准”入口。
- 修改：工程设置角度区恢复当前角度输入、机械目标角度输入、取当前角度按钮和计算校准按钮；`取当前角度` 使用当前正反向和角度范围、零全局偏移读取主目标视觉角；`计算校准` 按 `机械目标角度 - 当前角度` 计算单一 `offset_deg` 并写入角度偏移框；恢复 `CreateAngleReferenceSpinBox()` 作为当前角度/目标角输入控件。
- 结果：管理员仍可直接手填全局角度偏移，也可通过当前角度与机械目标角度快速算出单一 offset；未恢复 anchor、累计校准、双范围 offset 或草稿/运行 D504 诊断。
- 验证：`cmake --build build --config Release --target tankeye-openvino_qt_app tankeye-openvino_plc_result_contract_test` 通过；完整 `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过。
- 遗留：未启动 GUI，未连接真实 PLC、真实相机或真实机械设备；仍需人工打开工程设置确认恢复控件布局和现场计算口径。

## 2026-08-09 - 打包 TankEye-Iris 1.5.1

- 目标：将当前源码和运行包版本统一为 1.5.1，并生成正式运行包与 ZIP。
- 修改：更新 `config/tankeye.json`、`app/qt/workflow/app_config_service.h`、`scripts/package_runtime.ps1`、`runtime/USAGE_GUIDE.txt` 和 `docs/PACKAGING_README.md` 中的当前版本号、默认包名、示例命令和路径。
- 结果：生成 `dist/TankEye-Iris_1.5.1` 和 `dist/TankEye-Iris_1.5.1.zip`；包内包含 Qt 主程序、设备探测程序、Qt 平台插件、DG_8/DG_10 两套模型、配置和使用说明。
- 验证：`cmake --build build --config Release --target tankeye-openvino_qt_app tankeye-openvino_device_probe` 通过；完整 `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；`powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\package_runtime.ps1 -BuildDir build -ReleaseName TankEye-Iris_1.5.1 -Force` 成功生成目录包和 ZIP；包内关键文件、DG_8/DG_10 模型、`USAGE_GUIDE.txt`、版本号、exe hash 一致性、无 `docs/` 和无 `AGENTS.md` 检查均通过。打包过程仅出现已知 `VCINSTALLDIR is not set` 警告。
- 遗留：未启动包内 launcher，避免写入桌面或 Startup 快捷方式；未连接真实 PLC、真实相机或真实机械设备。

## 2026-08-10 - 按 Python v1.0.15 规则替换视觉 AC 角度

- 目标：将视觉原始角从旧 B 框方向对齐后的 A 框规则，替换为 `推理v1.0.15.py` 的原始 left OBB 四边垂足 AC 规则。
- 修改：`frame_postprocess.cpp` 删除旧 B 框长/短边旋转候选、候选择优 helper 和旧调试字段；`BuildAcRayGeometry()` 改为以 SEG 中心 O、原始 left OBB 中心 A、原始 A 框四边垂足 C 中 `OA·AC` 最大者计算 AC；`tests/frame_postprocess_smoke.cpp` 新增 Python 规则合同测试，验证 B 框方向冲突时 AC 来自原始 A 框、垂足按最大点积选择、角度与抓取长轴保持一致。
- 结果：`PoseDetection::angle_deg`、AC 箭头、基础 `corners`、`grip_long_angle_deg` 和后续真实夹爪/碰撞几何统一基于新 AC；D508 判型、B 候选选择、PLC 全局角度校准、正反向、范围、头型补偿和真实夹爪尺寸保持原逻辑。
- 验证：`cmake --build build --config Release --target tankeye-openvino_frame_postprocess_smoke`、`tankeye-openvino_frame_overlay_test`、`tankeye-openvino_plc_result_contract_test`、`tankeye-openvino_qt_app` 均通过；`powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release -Filter tankeye-openvino_frame_postprocess_smoke.exe` 通过；完整 `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过。直接运行测试 exe 曾因缺少运行时 PATH 返回 `0xc0000135`，使用项目测试脚本设置 PATH 后通过。
- 遗留：未启动 GUI，避免潜在 Startup 写入；未连接真实 PLC、真实相机或真实设备。仍需用现场图片与 Python 脚本人工对照角度、箭头和真实夹爪框方向。

## 2026-08-10 - 打包 TankEye-Iris 2.1

- 目标：将当前源码和运行包版本统一为 2.1，并生成正式运行包与 ZIP。
- 修改：更新 `config/tankeye.json`、`app/qt/workflow/app_config_service.h`、`scripts/package_runtime.ps1`、`runtime/USAGE_GUIDE.txt` 和 `docs/PACKAGING_README.md` 中的当前版本号、默认包名、示例命令和路径。
- 结果：生成 `dist/TankEye-Iris_2.1` 和 `dist/TankEye-Iris_2.1.zip`；包内包含 Qt 主程序、设备探测程序、Qt 平台插件、DG_8/DG_10 两套模型、配置和使用说明。
- 验证：`cmake --build build --config Release --target tankeye-openvino_qt_app` 和 `tankeye-openvino_device_probe` 通过；完整 `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；`powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\package_runtime.ps1 -BuildDir build -ReleaseName TankEye-Iris_2.1 -Force` 成功生成目录包和 ZIP；包内关键文件、DG_8/DG_10 模型、`USAGE_GUIDE.txt`、版本号、exe hash 一致性、无 `docs/` 和无 `AGENTS.md` 检查均通过。打包过程仅出现已知 `VCINSTALLDIR is not set` 警告。
- 遗留：未启动包内 launcher，避免写入桌面或 Startup 快捷方式；未连接真实 PLC、真实相机或真实机械设备。

## 2026-08-10 - 移除物料工程方案说明文案

- 目标：按用户截图反馈，移除工程设置“物料工程方案”按钮行下方的解释文字。
- 修改：`app/qt/ui/engineering_settings_dialog.cpp` 删除该分组下方 QLabel 文案；方案控件和保存/应用逻辑不变。
- 结果：工程设置中“物料工程方案”下方不再显示“工程方案保存物料相关参数；PLC、相机 IP、开机自启和管理员账号保持当前电脑设置。”。
- 验证：`cmake --build build --config Release --target tankeye-openvino_qt_app` 通过；完整 `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；源码中旧文案搜索无结果。
- 遗留：未启动 GUI 人工截图确认；未连接真实 PLC、真实相机或真实机械设备。

## 2026-08-10 - 排查旧数据大小头日志

- 目标：查看 `build/Release/logs/tankeye_20260810_152146.log`，解释旧数据为什么看不到或很少看到大小头框。
- 修改：未修改源码；仅更新项目记忆中的本次日志诊断结论。
- 结果：日志显示第一次检测 `obb_count=2`，只有两个 `left`，无 `big/small`，所以目标因 `missing_head_candidate` 拒抓；后续在当前 `obb_conf_threshold=0.05` 下出现低置信度大小头，右上目标 `small conf=0.176025` 被选中并输出 `D508=2`、`D506=1`、`D504=153.24`，但低阈值也带出重复低置信度 `left conf=0.0673218`，导致另一个 segment 因 `left_count_in_segment=2` 被结构保护拒抓。
- 验证：真实读取 `AGENTS.md`、`docs/project-memory/*` 指定记忆文件、当前 DG_8 profile 和日志文件；未运行构建/测试，因为本次是只读日志分析，不改程序逻辑。
- 遗留：若后续要解决旧数据，可评审分类阈值方案：大小头允许更低阈值，`left` 保持较高阈值，避免全局降阈值引入重复 `left` 误检。

## 2026-08-10 - 对比 C++ 与 Python OBB 置信度

- 目标：对比 `tankeye_20260810_160116.log` 和 `推理v1.0.16.py` 在同一张图片上的 OBB 类别置信度输出。
- 修改：未修改源码；仅更新项目记忆中的只读对比结论。
- 结果：C++ 程序在 `samples/Image_20260808154107626.bmp` 上输出 `big conf=0.919434`、`left conf=0.861816`，判型 `D508=1` 大上右；Python 输出 `left conf=0.8937`、`big conf=0.8162`，同样为大上右。C++ 的 big 置信度比 Python 高约 `0.1032`，left 比 Python 低约 `0.0319`。
- 验证：真实读取 C++ 日志、DG_8 profile 和仓外 Python 脚本；未运行构建/测试，因为本次只做日志与脚本输出对比，不改程序逻辑。
- 遗留：Python 当前脚本路径显示使用 `obb_best.pt`，C++ 使用 OpenVINO `best_obb.xml`，若要做逐层一致性，需要确认 PT 与 XML/BIN 是否由同一权重、同一导出参数生成。

## 2026-08-10 - 定位旧数据 PT 与 OpenVINO OBB 置信度落差

- 目标：在用户确认 PT/XML 源自同一模型后，定位旧图 `test.png` 上 Python 高分大小头与程序低分大小头的差异发生位置。
- 修改：未修改源码；仅更新项目记忆中的实测结论。
- 结果：使用 `conda run -n cll_yolo` 实测 Python PT `obb_best.pt` 输出 `big=0.965852`、`big=0.720049`；同环境直接用 OpenVINO Python `Core` 读取程序 XML，在 CPU 输出 big 最高 `0.118255/0.074596`，GPU 输出 `0.122986/0.078369`。GPU 数值与 C++ 日志 `tankeye_20260810_152146.log` 完全一致，small `0.176025` 也一致。由此确认 C++ 阈值、旋转 NMS、坐标转换、AC 角度和大小头后处理均未改写置信度；落差发生在 PT 与 OpenVINO XML/BIN 的导出或数值执行层。
- 验证：真实执行 `cll_yolo` 环境中的 Ultralytics PT 推理；真实执行 OpenVINO Python CPU/GPU 对同一 XML、同一图像的原始输出读取；未运行构建/测试，因为本次未改源码。
- 遗留：现有 XML 含 FP16 压缩权重。下一步应在用户确认后，用同一 PT 做 `--no-half` 的 FP32 OpenVINO 导出并在同图上与 PT、现有 XML 对照，确认是否由 FP16 导出/运行造成；该操作会覆盖或生成模型文件，需先获得用户明确许可。

## 2026-08-10 - 导出 FP32 试验 OBB 模型供工程设置切换

- 目标：按用户要求导出一份新的 OBB OpenVINO 模型，放到仓内独立文件夹里，便于在工程设置中手动切换试验。
- 修改：未修改源码；只新增 `models/DG_8_weights_fp32_test/best_obb.xml/.bin`，来源仍是 `D:\work_floder\jiezhifa\code\obb_best.pt`，通过仓内临时复制 PT 后再导出，避免写仓外目录。
- 结果：新模型导出成功；同图 `samples/images/test.png` 上，FP32 XML 在 CPU 输出 `left=0.926380`、`big=0.965852`，GPU 输出 `left=0.925781`、`big=0.965820`，已回到和 PT 接近的高置信度量级，可直接在工程设置里把模型 A 指向这个新 XML 试用。
- 验证：真实执行 `conda run -n cll_yolo python .\scripts\export_pt_to_openvino.py ... --no-half --overwrite`；真实执行 OpenVINO Python CPU/GPU 对新 XML 的同图置信度读取；未运行构建/测试，因为本次未改源码。
- 遗留：尚未在 TankEye 工程设置里实际切换此新 XML；若用户要正式替换现有 DG_8 模型目录，建议先用这份试验模型现场确认旧数据效果，再决定是否覆盖现有模型。

## 2026-08-10 - 升级六角色协作流程

- 目标：按用户要求把项目的五角色评审流程补成六角色，增加架构师视角。
- 修改：更新 `docs/project-memory/AI_ROLE_WORKFLOW.md`，将流程标题、目的、适用说明、角色清单、第三步评审和收尾表述统一为“六角色”；新增架构师角色，负责模块边界、状态归属、重复实现和长期收敛建议；删除文件开头的旧注释版五角色流程，避免后续搜索混淆。
- 结果：后续新功能或高风险改动会按“产品、项目、架构、CV、Qt、测试”六视角进行评审，架构师专门负责把项目越做越乱的风险提前拦住。
- 验证：真实读取并修改 `docs/project-memory/AI_ROLE_WORKFLOW.md`，随后检查关键字符串已切换为六角色；本次仅改项目规则文档，未运行构建或测试。
- 遗留：后续若再做高风险改动，默认先把六角色评审写出来，再进入实现。

## 2026-08-10 - 导出脚本改为显式精度参数

- 目标：修正 `scripts/export_pt_to_openvino.py` 默认隐式导出 FP16 的风险，让以后导出 OpenVINO 模型时必须语义明确，默认更保守地使用 FP32。
- 修改：删除 `--no-half` 参数和 `half = not args.no_half` 旧逻辑；新增 `--precision {fp32,fp16}`，默认 `fp32`；导出时通过 `export_half = args.precision == "fp16"` 映射到 Ultralytics 的 `half` 参数。
- 结果：不写参数时导出 FP32；需要更小更快模型时可显式传 `--precision fp16`。暂不加入 FP8，因为当前 Ultralytics/OpenVINO 导出路径未显示 FP8 支持。
- 验证：真实执行 `conda run -n cll_yolo python .\scripts\export_pt_to_openvino.py --help`，确认帮助中出现 `--precision {fp32,fp16}`；真实执行默认导出到独立临时目录，产物为 FP32 量级约 10.6 MB；真实执行 `--precision fp16` 导出到独立临时目录，产物为 FP16 量级约 5.6 MB；验证临时目录已清理。未运行 C++ 构建，因为本次只改 Python 导出脚本。
- 遗留：后续正式导出发布模型时，应记录使用的 `--precision` 值；若未来要支持 INT8/FP8，需要先确认 Ultralytics 与 OpenVINO 版本、校准数据和现场验证方案。

## 2026-08-10 - 新增类型补偿夹取框显示开关

- 目标：按用户要求，把“大小头类型角度微调是否影响主画面夹取框显示”做成可选项；关闭后主画面保持原始视觉检测姿态。
- 修改：在 `ObbPostprocessSettings` 新增 `show_head_type_adjusted_geometry`，默认 `true`；工程设置“调试设置”新增“显示类型补偿后的夹取框”复选框；工程方案 JSON 和工程设置 INI 均读写该字段；`frame_overlay.cpp` 只在该开关开启时把 `display_geometry_angle_offset_deg` 应用于夹取框、检测框和 AC 箭头。
- 结果：默认行为保持不变；关闭后仅界面 overlay 不再叠加大小头角度补偿，PLC/D504、D508 大小头判定、拒抓/碰撞、安全框真实尺寸、模型推理和坐标转换逻辑均保持不变。
- 验证：`cmake --build build --config Release --target tankeye-openvino_frame_overlay_test tankeye-openvino_engineering_settings_service_test tankeye-openvino_project_profile_store_test tankeye-openvino_qt_app` 通过；`powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release -Filter ...` 分别验证 overlay、工程设置、工程方案测试通过；完整 `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过。直接运行测试 exe 曾因运行时 PATH 缺失返回 1，随后使用仓库脚本补齐运行时环境后通过。
- 遗留：未启动真实 GUI 人工点选确认；未连接真实 PLC、真实相机或真实机械设备。

## 2026-08-10 - 打包 TankEye-Iris 2.1.1

- 目标：按用户要求生成 `2.1.1` 运行包。
- 修改：将 `config/tankeye.json`、`app/qt/workflow/app_config_service.h`、`scripts/package_runtime.ps1`、`runtime/USAGE_GUIDE.txt` 和 `docs/PACKAGING_README.md` 的当前发布版本、默认包名、示例路径和显示说明同步到 `2.1.1`。
- 结果：生成 `dist/TankEye-Iris_2.1.1` 和 `dist/TankEye-Iris_2.1.1.zip`；包内包含 Qt 主程序、设备探测程序、Qt 平台插件、DG_8/DG_10 两套模型、配置和使用说明。
- 验证：`cmake --build build --config Release --target tankeye-openvino_qt_app tankeye-openvino_device_probe` 通过，仅有既有 Qt deprecated warning；完整 `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；`powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\package_runtime.ps1 -BuildDir build -ReleaseName TankEye-Iris_2.1.1 -Force` 成功生成目录包和 ZIP，仅有已知 `VCINSTALLDIR is not set` 警告；包内关键文件、DG_8/DG_10 模型、`USAGE_GUIDE.txt`、版本号、exe hash 一致性、无 `docs/` 和无 `AGENTS.md` 检查均通过。
- 遗留：未启动包内 launcher，避免写入桌面或 Startup 快捷方式；未连接真实 PLC、真实相机或真实机械设备。

## 2026-08-12 - 正式管理员授权绑定升级

- 目标：把管理员创建/重置和工程设置入口从旧手输授权码改为按硬件指纹绑定的 `admin_license.json` 授权；普通检测和操作工使用不被锁死。
- 修改：`admin_auth_helpers` 新增五类硬件指纹、短机器码、授权申请 JSON、签名 license 生成和 4/5 指纹匹配校验；删除旧 `BuildAdminAuthCode/VerifyAdminAuthCode` 手输码链路；管理员创建/重置/登录和工程设置入口统一检查 `AdminAuthLicenseStatus()`；授权生成脚本改为 `-RequestFile/-Output/-KeyFile` license 文件流程；打包脚本要求正式 `config/admin_auth.key` 存在且不默认打包单机 `admin_license.json`；文档更新 license 申请、生成和现场放置流程。
- 结果：没有有效 `config/admin_license.json` 时，软件仍可启动和普通检测，但不能创建/重置管理员、不能进入工程设置；license 有效时允许管理员流程；license 删除或替换为不匹配文件后，已有管理员账号也不能进入工程设置。MAC 类按任一授权物理网卡命中算匹配，空指纹字段不再虚假计入匹配。
- 验证：`cmake --build build --config Release --target tankeye-openvino_admin_auth_helpers_test tankeye-admin-auth-code tankeye-openvino_qt_app` 通过；完整 `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；`scripts\generate_admin_auth_code.ps1` 缺少 key 文件时真实失败；`scripts\package_runtime.ps1 -BuildDir build -ReleaseName TankEye-Iris_auth_binding_check -Force` 成功生成检查包，包内 `config/admin_auth.key` 存在且 `config/admin_license.json` 不存在，随后已清理检查包和临时测试目录。
- 遗留：未启动真实 GUI 人工点击管理员/工程设置弹窗；未连接真实 PLC、真实相机或真实机械设备；未读取或输出真实 `config/admin_auth.key` 内容。

## 2026-08-12 - 简化管理员授权缺失提示

- 目标：按用户要求，将缺少管理员授权文件时的现场提示改成更短、更容易听懂的文案。
- 修改：`AdminAuthLicenseStatus()` 缺 license 时返回 `未找到管理员授权文件。`；主窗口管理员创建、登录、重置和工程设置入口的拦截提示统一为 `普通检测可继续使用；管理员和工程设置需要本机有效授权文件。`；保留授权申请弹窗中的 `admin_license.json` 文件名说明，方便工程师生成和放置文件。
- 结果：缺授权文件时弹窗显示为 `未找到管理员授权文件。\n普通检测可继续使用；\n管理员和工程设置需要本机有效授权文件。`，不再在错误提示里暴露具体路径。
- 验证：`cmake --build build --config Release --target tankeye-openvino_admin_auth_helpers_test tankeye-openvino_qt_app` 通过；`powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release -Filter tankeye-openvino_admin_auth_helpers_test.exe` 通过；完整 `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过。
- 遗留：未启动真实 GUI 人工查看弹窗；未连接真实 PLC、真实相机或真实机械设备。

## 2026-08-12 - 授权缺失弹窗补充复制申请入口

- 目标：修复缺少 `admin_license.json` 时弹窗只能点 OK，现场人员无法复制授权申请给工程师生成授权文件的问题。
- 修改：`admin_auth_dialogs` 新增 `ShowAdminLicenseRequestDialog()`，复用现有机器码、授权状态和“复制授权申请”逻辑；主窗口管理员创建、登录、重置、用户按钮和工程设置入口在授权无效时改为打开该申请窗口。
- 结果：没有有效管理员授权文件时，普通检测仍可继续使用；点击管理员或工程设置入口会显示机器码、授权状态和“复制授权申请”按钮，现场复制后可交给工程师生成 `admin_license.json`。
- 验证：`cmake --build build --config Release --target tankeye-openvino_qt_app tankeye-openvino_admin_auth_helpers_test` 通过，仅有既有 Qt deprecated warning；完整 `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过。
- 遗留：未启动真实 GUI 人工点击确认新弹窗视觉效果；未连接真实 PLC、真实相机或真实机械设备。

## 2026-08-12 - 管理员授权状态三态文案

- 目标：按现场使用口径明确区分授权状态：无文件、不匹配、已授权。
- 修改：`AdminAuthLicenseStatus()` 缺少 `admin_license.json` 时返回 `无授权文件`；`VerifyAdminLicenseJson()` 校验通过返回 `已授权`，校验失败返回 `授权文件不符`；`admin_auth_helpers_test` 补充有效和不符状态文案断言。
- 结果：授权申请窗口中的“授权状态”会按 `无授权文件`、`授权文件不符`、`已授权` 三种情况显示。
- 验证：`cmake --build build --config Release --target tankeye-openvino_admin_auth_helpers_test tankeye-openvino_qt_app` 通过；完整 `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过。
- 遗留：未启动真实 GUI 人工查看三态显示；未连接真实 PLC、真实相机或真实机械设备。

## 2026-08-12 - 授权申请改为保存 JSON 文件

- 目标：让现场操作员直接把本机授权申请保存成 JSON 文件发给工程师，避免复制 JSON 文本再手工另存。
- 修改：`admin_auth_dialogs` 中授权申请按钮由 `复制授权申请` 改为 `保存授权申请`；点击后通过保存文件对话框导出 `AdminAuthLicenseRequestText()` 生成的 request JSON，默认文件名为 `TankEye_admin_license_request_<机器码>.json`；创建/重置管理员和授权申请窗口提示同步改为保存文件流程，并移除剪贴板/复制提示逻辑。
- 结果：缺少或不匹配 `admin_license.json` 时，现场仍可看到机器码和三态授权状态，并可选择路径保存授权申请 JSON 文件；保存成功提示将文件发给工程师生成 `admin_license.json`。
- 验证：`cmake --build build --config Release --target tankeye-openvino_qt_app tankeye-openvino_admin_auth_helpers_test` 通过；完整 `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；静态搜索确认授权申请入口不再保留 `复制授权申请`、剪贴板和复制 toast 逻辑。
- 遗留：未启动真实 GUI 人工点击保存文件对话框；未连接真实 PLC、真实相机或真实机械设备。

## 2026-08-12 - 根据现场授权申请生成管理员授权文件

- 目标：使用现场导出的授权申请 JSON 生成本机可用的 `admin_license.json`。
- 修改：通过 `scripts/generate_admin_auth_code.ps1` 读取 `config/TankEye_admin_license_request_TK-B57A-B3ED-5AA9-3F90.json`，使用仓内 `config/admin_auth.key` 和已构建授权工具生成 `config/admin_license.json`；未修改授权算法或源码。
- 结果：已生成 `config/admin_license.json`，文件存在且大小为 785 字节。
- 验证：确认授权申请文件、密钥文件和 `tankeye-admin-auth-code.exe` 存在；执行生成脚本成功；`cmake --build build --config Release --target tankeye-openvino_admin_auth_helpers_test tankeye-openvino_qt_app` 通过。
- 遗留：未输出或读取真实密钥内容；未启动真实 GUI 查看状态是否显示 `已授权`；未连接真实 PLC、真实相机或真实机械设备。

## 2026-08-12 - 更新构建运行打包指南的管理员授权流程

- 目标：修正 `docs/BUILD_RUN_PACKAGE_GUIDE.md` 中仍描述旧版复制授权申请/授权码流程的问题。
- 修改：将管理员授权步骤改为现场点击 `保存授权申请` 导出 `TankEye_admin_license_request_<机器码>.json`，工程师用 `scripts/generate_admin_auth_code.ps1` 生成 `config/admin_license.json` 后发回现场；常见问题补充 `无授权文件`、`授权文件不符`、`已授权` 三态说明。
- 结果：构建运行打包指南与当前软件授权申请保存 JSON 文件流程一致。
- 验证：真实读取并修改文档；静态搜索确认该文档不再保留 `复制授权申请` 和旧 `授权码` 说法，新的保存授权申请、生成 `admin_license.json` 和三态文案均存在。
- 遗留：本次仅修改文档，未运行源码构建或 GUI；未连接真实 PLC、真实相机或真实机械设备。

## 2026-08-12 - 管理员授权文件开发路径兜底

- 目标：修复已在项目根目录 `config/admin_license.json` 生成授权文件，但开发环境程序仍显示 `无授权文件` 的问题。
- 修改：`LicensePath()` 优先读取 exe 所在目录 `config/admin_license.json`，找不到时兜底读取项目根目录 `../../config/admin_license.json`；同时将当前已生成的授权文件复制到 `build/Release/config/admin_license.json`；`docs/BUILD_RUN_PACKAGE_GUIDE.md` 补充开发环境和正式运行包的授权文件放置路径说明。
- 结果：开发环境从 `build/Release` 启动时，即使授权文件只在项目根目录 `config`，程序也能自动识别；正式运行包仍以运行包 `config` 为准。
- 验证：确认源码 `config/admin_license.json` 存在但 `build/Release/config` 和 `dist/TankEye-Iris_2.1.1/config` 原本不存在授权文件；复制到 `build/Release/config` 后文件存在；`cmake --build build --config Release --target tankeye-openvino_admin_auth_helpers_test tankeye-openvino_qt_app` 通过；完整 `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过。
- 遗留：未启动真实 GUI 查看状态是否显示 `已授权`；未连接真实 PLC、真实相机或真实机械设备。

## 2026-08-12 - 移除管理员授权成功展示文案

- 目标：按瘦身口径删除无实际用户展示价值的 `已授权` 成功状态文案。
- 修改：`VerifyAdminLicenseJson()` 校验通过时只设置 `valid=true`，不再写入 `已授权` message；保留失败时的 `授权文件不符`；删除授权测试中的成功文案断言；构建运行打包指南改为说明授权成功后直接进入管理员账号或密码阶段。
- 结果：授权成功只由 `status.valid` 表示；用户可见授权申请窗口只负责显示失败原因和保存授权申请。
- 验证：`cmake --build build --config Release --target tankeye-openvino_admin_auth_helpers_test tankeye-openvino_qt_app` 通过；完整 `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过。
- 遗留：未启动真实 GUI；未连接真实 PLC、真实相机或真实机械设备。

## 2026-08-12 - 恢复管理员账号表单已授权状态

- 目标：修正创建/重置管理员账号表单中授权成功时“授权状态”为空的问题。
- 修改：`VerifyAdminLicenseJson()` 校验通过时恢复 `已授权` message；`admin_auth_helpers_test` 补回成功文案断言；构建运行打包指南说明授权成功后进入管理员账号或密码阶段，表单状态显示 `已授权`。
- 结果：授权异常仍显示 `无授权文件` 或 `授权文件不符`；授权成功进入账号相关表单时，状态行显示 `已授权`。
- 验证：`cmake --build build --config Release --target tankeye-openvino_admin_auth_helpers_test tankeye-openvino_qt_app` 通过；完整 `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过。
- 遗留：未启动真实 GUI 查看账号表单状态行；未连接真实 PLC、真实相机或真实机械设备。

## 2026-08-12 - 管理员重置密码增加本地恢复问题

- 目标：避免现场操作员仅凭长期机器授权文件 `admin_license.json` 直接重置管理员密码，同时不让工程师参与每次忘记密码。
- 修改：管理员账号创建/重置保存逻辑新增恢复问题和恢复答案；恢复答案使用独立 salt/hash，不保存明文；创建管理员表单新增恢复问题/答案，恢复答案带小眼睛；忘记密码时必须先正确回答恢复问题，旧账号没有恢复问题时提示联系负责人清除账号后重新初始化；工程设置中知道当前密码的账号修改保留原恢复问题/答案不变；构建运行打包指南同步新流程。
- 结果：首次创建管理员仍需要本机有效 `admin_license.json`；之后忘记密码由老板/调试员通过本地恢复问题重置，不能只靠授权文件重置。
- 验证：`cmake --build build --config Release --target tankeye-openvino_admin_auth_helpers_test tankeye-openvino_qt_app` 通过；完整 `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；静态搜索确认教程和 UI 文案已同步恢复问题流程。
- 遗留：未启动真实 GUI 人工点击创建管理员、忘记密码和恢复答案小眼睛；未连接真实 PLC、真实相机或真实机械设备。

## 2026-08-12 - 简化恢复问题表单标签

- 目标：去掉管理员创建/重置表单中 `新恢复问题`、`新恢复答案` 的冗余“新”字。
- 修改：`admin_auth_dialogs` 中设置恢复问题/答案的标签统一为 `恢复问题`、`恢复答案`。
- 结果：创建管理员和重置管理员表单文案更自然，恢复答案隐藏/显示和校验逻辑不变。
- 验证：静态搜索确认 `新恢复问题`、`新恢复答案` 无残留；`cmake --build build --config Release --target tankeye-openvino_qt_app` 通过。
- 遗留：未启动真实 GUI 人工查看表单；未连接真实 PLC、真实相机或真实机械设备。

## 2026-08-12 - 修正恢复问题重置表单语义

- 目标：修复忘记密码时恢复答案正确仍提示错误的风险、重置表单新恢复问题/答案语义不清，以及小眼睛按钮占用 Tab 焦点的问题。
- 修改：恢复答案 hash 改用专用输入规范化；新增 `AdminAuthResetCredentials()`，重置密码时不填新恢复问题/答案会保留旧恢复凭据，成对填写才替换，单填一个会拒绝；重置表单下方标签改为 `新恢复问题`、`新恢复答案`；密码/恢复答案小眼睛按钮设置 `Qt::NoFocus`；测试补充保留旧恢复答案、替换新恢复答案和半填写失败。
- 结果：重置流程区分“回答旧恢复答案”和“可选设置新恢复问题/答案”；Tab 一次跳到下一个输入框；创建管理员表单仍使用必填的 `恢复问题`、`恢复答案`。
- 验证：`cmake --build build --config Release --target tankeye-openvino_admin_auth_helpers_test tankeye-openvino_qt_app` 通过；完整 `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过。
- 遗留：未启动真实 GUI 人工验证 Tab 顺序、恢复答案小眼睛和重置表单；未连接真实 PLC、真实相机或真实机械设备。

## 2026-08-12 - 修正创建和重置管理员保存回调接反

- 目标：修复重置管理员时不填 `新恢复问题/新恢复答案` 仍提示 `恢复问题不能为空` 的问题。
- 修改：`showCreateAdminAccountDialog()` 改回调用 `setAdminCredentials()`，保持创建管理员时恢复问题/答案必填；`showAdminResetDialog()` 改为调用 `AdminAuthResetCredentials()`，使重置管理员时新恢复问题/答案可选。
- 结果：创建管理员和重置管理员分别走正确保存逻辑；重置时两个新恢复字段都不填会保留旧恢复凭据。
- 验证：静态确认创建流程调用 `setAdminCredentials()`、重置流程调用 `AdminAuthResetCredentials()`；`cmake --build build --config Release --target tankeye-openvino_admin_auth_helpers_test tankeye-openvino_qt_app` 通过；完整 `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过。
- 遗留：未启动真实 GUI 人工复测重置弹窗；未连接真实 PLC、真实相机或真实机械设备。

## 2026-08-12 - 同步打包和运行包授权教程

- 目标：修正 `docs/PACKAGING_README.md` 和 `runtime/USAGE_GUIDE.txt` 中仍描述旧版复制授权申请、首次创建或重置都靠授权文件的问题。
- 修改：两份文档同步为“保存授权申请 JSON -> 工程师生成 `admin_license.json` -> 首次创建管理员账号并设置恢复问题/答案”；忘记密码说明改为检查本机授权文件后回答恢复问题，新恢复问题/答案可不填，填则必须两个都填。
- 结果：打包说明和运行包使用说明与当前管理员授权和本地恢复问题流程一致。
- 验证：真实读取并修改两份文档；静态搜索确认旧 `复制授权申请` 和 `首次创建或重置管理员账号` 口径已清除，新 `保存授权申请`、恢复问题和 `admin_license.json` 流程均存在。
- 遗留：本次仅修改文档，未运行源码构建；未连接真实 PLC、真实相机或真实机械设备。

## 2026-08-12 - 创建并推送 Unordered_scraping_v6 分支

- 目标：按用户要求将当前项目提交到 GitHub 仓库 `https://github.com/fuckMiss/Unordered_scraping` 的新分支 `Unordered_scraping_v6`。
- 修改：当前目录原本没有 `.git`，本轮初始化本地 Git 仓库并配置远端；补充 `.gitignore`，排除单机授权文件 `config/admin_license.json`、授权申请 JSON 和 DG_10 模型权重，避免上传机器授权、密钥或运行资产。
- 结果：本地分支为 `Unordered_scraping_v6`，准备提交并推送当前源码、配置、脚本、测试和文档快照。
- 验证：执行 `git status --short --branch` 确认原目录不是 Git 仓库后完成初始化；执行 `git ls-remote --heads` 确认远端仓库存在且尚无 `Unordered_scraping_v6`；执行 `git check-ignore -v` 确认真实密钥、授权文件、授权申请 JSON、构建产物和发布包被忽略。
- 遗留：推送依赖 GitHub 网络和本机凭据；本轮不连接真实 PLC、真实相机或真实机械设备。

## 2026-08-12 - 打包 TankEye-Iris 2.1.2

- 目标：按用户要求生成 `2.1.2` 运行包。
- 修改：将 `config/tankeye.json`、`app/qt/workflow/app_config_service.h`、`scripts/package_runtime.ps1`、`runtime/USAGE_GUIDE.txt` 和 `docs/PACKAGING_README.md` 的当前发布版本、默认包名、示例路径和打包说明同步到 `2.1.2`。
- 结果：生成 `dist/TankEye-Iris_2.1.2` 和 `dist/TankEye-Iris_2.1.2.zip`；包内包含 Qt 主程序、设备探测程序、Qt 平台插件、DG_8/DG_10 两套模型、CPU/GPU OpenVINO 插件、授权密钥、配置和使用说明。
- 验证：`cmake --build build --config Release --target tankeye-openvino_qt_app tankeye-openvino_device_probe` 通过，仅有既有 Qt deprecated warning；完整 `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；`powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\package_runtime.ps1 -BuildDir build -ReleaseName TankEye-Iris_2.1.2 -Force` 成功生成目录包和 ZIP，仅有已知 `VCINSTALLDIR is not set` 警告；包内关键文件、DG_8/DG_10 模型、`USAGE_GUIDE.txt`、版本号、exe hash 一致性、无 `docs/` 和无 `AGENTS.md` 检查均通过。
- 遗留：未启动包内 launcher，避免写入桌面或 Startup 快捷方式；未连接真实 PLC、真实相机或真实机械设备。

## 2026-08-13 - 收紧管理员授权为单机全匹配

- 目标：按用户要求让每台机器都必须单独申请管理员授权，避免第一台机器生成的 `admin_license.json` 在第二台机器继续可用。
- 修改：`VerifyAdminLicenseJson()` 的授权匹配阈值从 5 类指纹至少 4 类匹配改为 5 类全部匹配；MAC 类从任一 MAC 命中改为授权 MAC 列表与当前 MAC 列表完全一致；`admin_auth_helpers_test` 同步改为任一类变化、MAC 列表变化或指纹字段缺失均判无效；`MEMORY.md` 同步更新授权口径。
- 结果：`admin_license.json` 现在绑定完整 Windows MachineGuid、BIOS/主板序列号、系统盘序列号、Qt machineUniqueId 和物理网卡 MAC 列表；任一类不一致都会显示 `授权文件不符`。
- 验证：`cmake --build build --config Release --target tankeye-openvino_admin_auth_helpers_test tankeye-admin-auth-code tankeye-openvino_qt_app` 通过；`powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release -Filter tankeye-openvino_admin_auth_helpers_test.exe` 通过。
- 遗留：未连接真实 PLC、真实相机或真实机械设备；旧授权文件在非原申请机器上应失效，现场需重新按每台机器保存授权申请并生成对应 `admin_license.json`。

## 2026-08-13 - 打包 TankEye-Iris 2.1.3

- 目标：把管理员授权单机全匹配、自启 VBS 临时残留修复和当前版本同步打包为 `2.1.3`。
- 修改：将 `config/tankeye.json`、`app/qt/workflow/app_config_service.h`、`scripts/package_runtime.ps1`、`runtime/USAGE_GUIDE.txt` 和 `docs/PACKAGING_README.md` 同步到 `2.1.3`；运行包内嵌 `create_desktop_shortcut.ps1` 同步使用临时目录中转写入 Startup VBS，并只清理 `TankEye-Iris.vbs.*` 残留，不删除正式 `TankEye-Iris.vbs`；打包说明同步 5 类硬件指纹全匹配口径。
- 结果：生成 `dist/TankEye-Iris_2.1.3` 和 `dist/TankEye-Iris_2.1.3.zip`；包内包含 Qt 主程序、设备探测程序、Qt 平台插件、DG_8/DG_10 两套模型、CPU/GPU OpenVINO 插件、授权密钥、配置、运行说明和修正后的快捷方式脚本。
- 验证：`cmake --build build --config Release --target tankeye-openvino_qt_app tankeye-openvino_device_probe tankeye-admin-auth-code tankeye-openvino_admin_auth_helpers_test` 通过，仅有既有 Qt deprecated warning；完整 `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；`powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\package_runtime.ps1 -BuildDir build -ReleaseName TankEye-Iris_2.1.3 -Force` 成功生成目录包和 ZIP，仅有已知 `VCINSTALLDIR is not set` 警告；包内关键文件、DG_8/DG_10 模型、`USAGE_GUIDE.txt`、版本号、exe hash 一致性、无 `docs/`、无 `AGENTS.md`、以及包内快捷方式脚本临时残留清理逻辑检查均通过。
- 遗留：未启动包内 launcher，避免写入桌面或 Startup 快捷方式；未连接真实 PLC、真实相机或真实机械设备；`tankeye-admin-auth-code.exe` 仍按现有设计保留在工程师/开发构建侧，不随运行包发给现场。
