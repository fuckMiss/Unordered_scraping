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
