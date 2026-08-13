# TankEye-Iris 动态记忆

更新时间：2026-08-04

本文件记录当前进行中的任务、未完成决策、已知问题和跨会话继续工作所需上下文。它是动态文档，可以随任务推进更新。

## 当前任务

- 让项目代码和目录组织更干净、可维护，按小批次重构。
- 当前总体目标升级为全项目瘦身优化：所有代码清理都优先合并重复、减少总代码、减少维护点。
- 第一批选择低风险 UI 辅助拆分：运行日志查看器已从 `GraspMainWindow` 抽离。
- 第二批继续拆分管理员创建、登录、重置弹窗，主窗口只保留薄包装和账号状态逻辑。
- 第二批补充已合并创建/重置管理员弹窗重复结构，并提取复用 UI 缩放工具。
- 全项目瘦身第一批已合并 OBB/SEG CLI 重复入口。
- 全项目瘦身第二批已合并 CMake 测试目标样板、管理员账号存储逻辑和工程设置 grid/helper 重复构造。
- 全项目瘦身第三批已合并 `GraspMainWindow` 顶部栏图标按钮创建/静态图标刷新，以及目标卡片创建/字段刷新重复逻辑，未新增文件。
- 当前功能批次已实现上下限保护区域可视化：机械 ROI 反投影到图像、主画面 overlay、工程设置 `显示保护区域` 开关和五角色协作流程文档。
- 当前功能批次已实现坐标转换方案导入与 `应用` 按钮升级：工程设置支持导入原生 JSON 和 VisionMaster TXT 五列格式（图像X、图像Y、机械X、机械Y、角度；角度列不参与矩阵计算），导入只填编辑区；`应用` 读取当前九点编辑区并刷新原有坐标转换状态提示；单独 `计算矩阵` 按钮和重复矩阵显示框已移除；`另存为` 改为高反差自定义命名对话框。
- 当前功能批次已实现“延长夹爪只按真实 SEG mask 碰撞拒抓”：废弃 bbox/minAreaRect 外框碰撞拒抓，只有延长夹爪 OBB 与旁边物料 `segment.mask` 白色像素发生 1 像素及以上重叠才不可抓；自身 segment 继续忽略；空 mask、尺寸异常或 ROI 为空不 fallback 到外框拒抓；debug reason 为 `extended_touches_other_mask`；调试 overlay 通过 `mask_collisions` 用半透明红色显示真实重叠像素。
- 当前已补充“放大夹取 OBB 碰撞归属修正”：放大 OBB 形状和倍率不变，但碰撞 overlap 需要先扣掉当前 matched segment mask；只有旁边物料 mask 在当前自身 mask 之外仍落入当前放大 OBB 时，才拒当前目标。旁边框碰到当前 mask、或旁边 mask 与当前 mask 重叠但没有外露像素，不拒当前目标；A 扫到 B 时只拒 A。
- 当前已实现普通显示按 SEG 去重 OBB 框：普通主画面和普通目标列表通过 `SelectNormalDisplayDetectionIndices()` 每个 `matched_segment_index` 只显示一个代表 detection，优先级为主目标、可抓、置信度最高；全显/调试模式仍显示全部 detections 和 raw OBB；抓取安全逻辑、PLC 写入和主目标选择不变。
- 2026-08-04 已按用户要求丢弃 2026-08-04 本地 AC/绿色框/工程设置角度校准试改，重新从 GitHub `origin/Unordered_Scraping_V5` 拉取并硬回退到提交 `8698d85a33b9c1bdb18ff9b790b91265429504fe`；后续角度问题需要在该 V5 基线上重新设计和实施。
- 2026-08-04 新增提交信息规则：后续每次提交必须写清楚相对上一版发生了什么变化、改动范围和验证结果，不能使用笼统的 `update`、`fix`、`change`。
- 2026-08-04 已按五角色计划重新实现角度修正：`frame_postprocess` 最终抓取角度从旧红色 A 框自身 OBB keep-point 射线改为 `SEG 中心 O -> A -> AC` 射线；同一 SEG 内多 big/small B 候选按 `|∠AOB - 90°|` 最小择优，置信度只作并列 tie-break；A 框先按最佳 B 长边/短边最小旋转对齐，再沿长边保持现有 3.0 倍放大，角度、overlay 箭头、PLC 中心偏移和真实 mask 碰撞检测都复用该对齐放大框；无 B、无有效 minRect、OA/AC 退化时不回退旧角度并拒抓。
- 2026-08-04 已补充修正 AC 角度框显示与右上角类似安全框歪斜问题：普通画面 `PoseDetection::corners` 改为 AC 对齐后的基础抓取 OBB，raw 模型 OBB 只保留在全显/调试 raw overlay；AC 对齐不再只按“旋转幅度最小”选 B 长边/短边方案，而是两种候选都试算，优先选择 AC 垂足落在长边上的夹爪几何，再按 AC 点积和旋转幅度兜底，避免 3 倍安全框长轴被选到明显不正常的抓取方向；debug 日志新增 AC 候选角度、点积和长边使用信息。
- 2026-08-04 已将 AC 角度排查字段接入工程设置的“启用后处理调试日志”开关：开启后 `[PostprocessDebug] target` 行会输出 O/A/B/C、arrow_start、`selected_aob_angle_deg`、`selected_aob_diff_deg`、`ac_uses_long_edge`、`ac_dot`、raw/target/final 长边角度和 `ac_angle_deg`，用于现场判断右上角等目标是 B 点选择、候选轴还是 AC 方向问题。

## 当前工作区状态

- 当前已跟踪源码和文档已回到 GitHub V5 基线；按协作规则，本文件和 `TASK_HISTORY.md` 会保留本次回退记录，未跟踪参考脚本 `models/推理v1.0.13.py` 仍保留在本地，未删除。
- 已纳入 Git 的既有协作文档：
  - `AGENTS.md`
  - `docs/project-memory/PROJECT_CONFIGURATION.md`
  - `docs/project-memory/TASK_HISTORY.md`
  - `docs/project-memory/MEMORY.md`
  - `docs/project-memory/OPERATING_LIMITS.md`
  - `docs/project-memory/HANDOFF.md`
- 本次新增协作文档：
  - `docs/project-memory/AI_ROLE_WORKFLOW.md`
- 已纳入 Git 的既有新增代码结构：
  - `app/cli/cli_inference_runner.h`
  - `app/qt/ui/runtime_log_dialog.h`
  - `app/qt/ui/runtime_log_dialog.cpp`
  - `app/qt/ui/admin_auth_dialogs.h`
  - `app/qt/ui/admin_auth_dialogs.cpp`
  - `app/qt/ui/ui_scale_utils.h`
- 已纳入 Git 的既有新增测试：
  - `tests/admin_auth_helpers_test.cpp`
- 本次新增测试：
  - `tests/frame_overlay_test.cpp`
  - `tests/calibration_profile_store_test.cpp`
  - `tests/frame_postprocess_smoke.cpp` 已扩展真实 mask 碰撞、bbox 空白、mask 空洞、空/异常 mask 和多旁料碰撞用例
- 已纳入 Git 的既有构建配置修改：
  - `CMakeLists.txt`

## 未完成决策

- 是否保留 `app/qt/ui/grasp_main_window.cpp` 当前 UI/图标缩放相关改动，需要后续结合真实窗口运行效果决定。
- 是否保留 `launch_tankeye.ps1` 中移除自动 `QT_SCALE_FACTOR` 的改动，需要后续实机验证不同分辨率下 UI 缩放表现。

## 已知问题与风险

- `README*` 可能滞后；后续梳理项目时不得优先依赖 README。
- 当前仓库包含本地生成目录和私有/大文件目录，如 `build/`、`dist/`、`_deps/`、`vendor/hik_mvs/`、`models/DG_8_weights/`，它们多数被 `.gitignore` 排除。
- `config/admin_auth.key` 是真实授权密钥文件，已被 `.gitignore` 排除，不应提交或外传。
- UI、打包、PLC、相机、OpenVINO 模型加载相关结论，需要真实程序或真实运行包验证。

## 下一步建议

- 继续拆分前，优先人工或桌面自动化点击“运行日志”，确认日志文件选择、搜索、筛选、分页、自动刷新正常。
- 管理员弹窗拆分后，优先人工或桌面自动化打开管理员创建/登录/重置弹窗，确认机器码复制、密码显示、错误提示、记住密码行为。
- 工程设置 helper 合并后，优先人工或桌面自动化打开工程设置，检查模型路径、曝光、角度校准、轴向映射、限位、坐标转换和管理员账号区域布局。
- 顶部栏和目标卡片 helper 合并后，已完成模拟 PLC 启动冒烟；后续可人工或桌面自动化点检用户、工程设置、目标列表、最小化、最大化/还原、关闭按钮。
- 涉及新功能或高风险 UI/CV/PLC 改动时，默认先按 `docs/project-memory/AI_ROLE_WORKFLOW.md` 的五角色流程做只读评审，再实现。
- 上下限保护区域可视化已完成 Release 构建、默认测试、新增渲染测试和模拟 PLC 真实启动；仍需用有效九点标定人工观察保护区叠层位置。
- 坐标转换导入与应用按钮升级已完成 `calibration_profile_store_test`、Release 主程序构建、默认测试脚本和模拟 PLC 真实启动；已按用户提供 VM 五列样例补充解析测试，仍需现场人工打开工程设置确认导入/状态提示/另存为弹窗视觉效果。
- 延长夹爪碰撞拒抓规则已改为只看旁边真实 SEG mask 白色像素重叠；`frame_postprocess_smoke`、`frame_overlay_test`、Release 主程序构建、默认测试脚本和模拟 PLC 真实启动均已通过；仍需现场真实画面人工确认调试红色重叠像素显示效果。
- 放大夹取 OBB 碰撞归属已进一步修正为 `当前放大 OBB ∩ 旁边 mask ∩ 非当前 mask`；新增“旁边 mask 落在当前 mask 内不拒”和“A 扫 B 只拒 A”的后处理回归测试；仍需用用户同一张现场图片人工复核中间目标。
- 普通显示按 SEG 去重 OBB 框已完成 `frame_overlay_test`、Release 主程序构建、默认测试脚本和模拟 PLC 真实启动；仍需现场图片人工切换普通/全显，确认普通只显示一个代表框、全显能看到全部误检框。
- AC 射线角度修正已完成 `tankeye-openvino_frame_postprocess_smoke`、`tankeye-openvino_frame_overlay_test`、`tankeye-openvino_qt_app` Release 构建、默认测试脚本和模拟 PLC 真实启动，日志：`build/Release/logs/tankeye_20260804_143746.log`；未连接真实 PLC/真实相机，仍需用现场真实图片或相机画面人工复核 D504 角度、D508 类型和调试 overlay 箭头是否符合夹爪实际方向。
- AC 框显示/候选方向修正已完成 `tankeye-openvino_frame_postprocess_smoke`、`tankeye-openvino_frame_overlay_test`、`tankeye-openvino_qt_app` Release 构建、默认测试脚本和模拟 PLC 真实启动，日志：`build/Release/logs/tankeye_20260804_150600.log`；仍需重新加载用户截图对应图片并人工复核右上角绿色安全框是否恢复正常，以及 D504/D508 是否符合现场夹爪方向。
- AC 调试日志字段已完成 `tankeye-openvino_frame_postprocess_smoke`、`tankeye-openvino_qt_app` Release 构建、默认测试脚本和模拟 PLC 真实启动，日志：`build/Release/logs/tankeye_20260804_151451.log`；用户后续可打开工程设置中的“启用后处理调试日志”并重新加载现场图，查看 `[PostprocessDebug] target` 行新增字段。
- 后续清理优先“合并重复逻辑、减少总代码”，不要只做文件搬迁；每次拆模块后检查是否产生重复 helper。
- 下一批高收益候选：继续压缩 `GraspMainWindow` 中目标列表/状态刷新重复显示逻辑；合并 `app/qt/main.cpp` 和启动脚本中的日志/启动参数说明重复；继续审查 OBB/SEG 里更深层但低风险的共享运行时状态或 class-name 校验逻辑。
- 若继续 UI 调整，先构建 `tankeye-openvino_qt_app`，再实机启动截图确认窗口、右侧栏、标题栏和缩放表现。
- 若继续启动脚本调整，需在目标分辨率或目标机器上验证 Qt/Windows 自动缩放效果。

## 2026-08-04 当前补充记忆：校准幂等与重复检测稳态

- 已完成工程设置角度校准幂等修正：计算偏移基于“当前显示角、机器目标角、基准偏移、方向、范围”的快照；程序自动写入计算结果时不刷新基准，因此同一输入连续点击“计算校准”不再继续叠加变化。
- 已完成重复检测稳态优化：不引入同图缓存，仍每次真实重跑 OBB/SEG；OBB NMS、SEG NMS 输出、B 候选和主目标选择增加稳定 tie-break；最终 `PoseDetection` 的抓取中心和视觉角度按 0.01 像素/0.01 度量化，降低浮点尾差和近似并列选择导致的 UI/PLC 数值抖动。
- 已新增/扩展测试覆盖：`plc_result_contract_test` 验证 `CalculateAngleCalibrationOffset()` 幂等；`frame_postprocess_smoke` 验证 B 候选并列稳定择优、近似同置信度主目标稳定选择和最终位姿量化。
- 验证已完成：`tankeye-openvino_plc_result_contract_test`、`tankeye-openvino_frame_postprocess_smoke`、`tankeye-openvino_frame_overlay_test`、`tankeye-openvino_obb`、`tankeye-openvino_seg`、`tankeye-openvino_qt_app` Release 构建通过；默认 `scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；`launch_tankeye.ps1 -BuildDir build -Device CPU -SimulatePlc` 已生成 `build/Release/logs/tankeye_20260804_153721.log`，GUI 常驻导致 30 秒超时后确认无残留 `tankeye` 进程。
- 未做真实 PLC/真实相机动作；重复检测仍可能存在模型后端本身的极小数值差异，本轮解决的是排序/并列/浮点尾差导致的可见抖动，后续需要用户用同一现场图片连续点击复测 UI 数值。

## 2026-08-04 当前补充记忆：角度校准零偏移绝对重算

- 用户澄清工程设置角度校准的正确语义：点击“计算校准”时，旧角度偏移必须视为 0；新偏移是当前零偏移显示角到机器目标角的绝对差值，不是在上一轮偏移上继续叠加。
- 已完成修正：`CalculateAngleCalibrationOffset()` 现在只计算 `target_angle - current_display_angle` 并沿用原有归一化；“取当前角度”使用原始视觉 `angle_deg` + 当前反向/范围 + 零偏移生成当前角度；如果取角后切换反向/范围，当前角度框会基于同一原始视觉角重新按零偏移刷新。
- 已验证：`tankeye-openvino_plc_result_contract_test` Release 构建和脚本运行通过，`tankeye-openvino_qt_app` Release 构建通过，默认 `scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过，模拟 PLC 启动日志为 `build/Release/logs/tankeye_20260804_155627.log`。
- 未验证真实 UI 点击、真实 PLC、真实相机；后续现场复测应重点确认连续点击“计算校准”不变，以及修改当前角/目标角/反向/范围后按零偏移重新计算。

## 2026-08-04 当前补充记忆：正反向校准偏移差异

- 用户发现正向和反向重新计算出的角度偏移相同。根因是上一轮公式虽然变成零偏移绝对重算，但计算按钮仍主要依赖“当前角度输入框”的值；如果当前角度框没有随正反向变化，反向开关本身不会进入偏移公式。
- 已修正：计算按钮现在优先使用最近一次“取当前角度”保存的原始视觉角，在点击计算时按当前正向/反向和角度范围、零偏移重新求当前显示角，再计算偏移；同时刷新当前角度框，让用户能看到正反向下参与计算的当前角不同。
- 已验证：新增 `CalculateAngleCalibrationOffsetFromRawAngle()` 测试，覆盖 raw=30、target=90 时正向偏移 60、反向偏移 -240；Release 主程序构建、完整测试脚本和模拟 PLC 启动均通过，最新启动日志 `build/Release/logs/tankeye_20260804_162431.log`。
- 未验证真实 UI 点击和真实设备；下一步用户应在工程设置中按“取当前角度 -> 计算 -> 切反向 -> 再计算”现场确认偏移和当前角度框会一起变化。

## 2026-08-04 当前补充记忆：校准输入框不随正反向跳变

- 用户确认最终交互口径：正向/反向相当于顺时针/逆时针计算口径切换；如果用户手动输入当前角度 30，切反向或点击计算时，当前角度输入框不要自动变成 -30，但计算偏移时内部仍要按反向后的角度参与。
- 已修正：工程设置中正反向/范围变化不再改写当前角度框，计算按钮也不再回写当前角度框；计算内部仍使用原始视觉角或手动输入值作为 raw angle，结合当前正反向和角度范围计算偏移。
- 已验证：新增截图口径测试 raw=30、target=95、`-180~180` 下正向偏移 65、反向偏移 125；Release 主程序构建、完整测试脚本和模拟 PLC 启动均通过，最新启动日志 `build/Release/logs/tankeye_20260804_163707.log`。
- 未验证真实 UI 点击和真实设备；后续现场重点复测“手输 30 -> 正向算 65 -> 切反向框仍显示 30 -> 再算 125”。

## 2026-08-04 当前补充记忆：OpenVINO OBB/SEG 公共逻辑瘦身

- 本轮按“真正瘦身优化”口径清洗 `core/inference`：将 OBB/SEG 两套推理类中重复的 NCHW letterbox 输入准备、OpenVINO 输入 Tensor 绑定和模型 warmup 合并到 `openvino_utils`。
- 已新增 `ParseDetectionOutputLayout()`，统一解析 YOLO 输出张量中的属性维、候选维和类别数；OBB/SEG 分别只传入固定属性数和 SEG mask 维度，不再各维护一份相似分支。
- 行为边界：不改检测阈值、不改后处理语义、不改 UI/PLC 寄存器、不连接真实 PLC/真实相机。
- 验证已完成：`tankeye-openvino_obb`、`tankeye-openvino_seg`、`tankeye-openvino_qt_app` Release 构建通过；默认 `scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；模拟 PLC 启动生成 `build/Release/logs/tankeye_20260804_164915.log`，GUI 常驻 30 秒超时后确认无残留 `tankeye` 进程。

## 2026-08-04 当前补充记忆：GraspMainWindow 工程设置职责拆分

- 用户指出 `app/qt/ui/grasp_main_window.cpp` 三千多行不方便维护；本轮先拆低风险、边界清楚的工程设置加载/保存职责。
- 已新增 `app/qt/ui/grasp_main_window_settings.cpp`，承接主窗口中的 `load*Settings()`、`save*Settings()`、`plcOutputConfig()`、轴向标签和坐标转换状态重建等函数；`CMakeLists.txt` 已加入该源文件；`grasp_main_window.cpp` 删除对应实现并清理部分无用 include。
- 当前 `grasp_main_window.cpp` 约 3038 行，新设置文件约 155 行。管理员 UI 拆分暂缓，因为它依赖主窗口匿名命名空间的运行时缩放函数 `UiScale()/SC()`；后续若继续拆，应先把响应式缩放状态抽成可共享 helper，避免复制缩放逻辑。
- 验证已完成：`tankeye-openvino_qt_app` Release 构建通过；默认 `scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；模拟 PLC 启动生成 `build/Release/logs/tankeye_20260804_170243.log`，GUI 常驻 30 秒超时后确认无残留 `tankeye` 进程。

## 2026-08-04 当前补充记忆：上传 GitHub 前汇总

- 用户要求总结当前改动作为提示词并上传 GitHub；本次提交范围包含当前工作区全部已验证源码/文档改动，以及参考脚本 `models/推理v1.0.13.py`。
- 上传前已确认 `.gitignore` 仍排除 `build/`、模型权重和 `config/admin_auth.key`；`git diff --check` 无空白错误，仅有 CRLF 换行提示。
- 本次上传目标分支为 `Unordered_Scraping_V5`，远端为 `origin`。

## 2026-08-04 当前补充记忆：真实夹爪长宽可视化

- 用户澄清本轮只需要在工程设置增加夹爪长宽参数，并把该机械尺寸换算后的真实夹爪矩形绘制到图上观察效果；所有现有判断逻辑保持不变。
- 已完成：工程设置“调试设置”新增“夹爪长度/夹爪宽度”输入框；`UiOverlaySettings` 持久化这两个值；后处理在 `PoseDetection::grip_long_angle_deg` 显式输出与延长 OBB 同源的 AC 对齐抓取长轴角度；主窗口显示刷新时，若长宽为正且九点坐标转换有效，会用当前检测的原始夹取 OBB 中心、`grip_long_angle_deg` 和中心附近的局部机械/图像比例生成 `mechanical_gripper_corners`，不依赖未来可能删除的绿色延长框或从框边反推角度；`frame_overlay` 用蓝青色绘制该框；打开现有“启用后处理调试日志”后，会输出 `[PostprocessDebug] mechanical_gripper` 或 `mechanical_gripper_skip`，包含长宽、中心、检测角度、抓取长轴角度、局部比例、像素长宽、四角点或跳过原因。
- 行为边界更新：该阶段仅用于历史参考；当前实现已删除 `extended_corners`，真实夹爪框完全接管碰撞、显示、AC 射线 C 点和中心偏移。
- 验证已完成：`tankeye-openvino_engineering_settings_service_test`、`tankeye-openvino_frame_overlay_test`、`tankeye-openvino_qt_app` Release 构建通过；补充日志和修正方向来源后再次构建 `tankeye-openvino_frame_postprocess_smoke`、`tankeye-openvino_qt_app` 和 `tankeye-openvino_frame_overlay_test` 通过；`scripts/run_build_tests.ps1 -BuildDir build -Configuration Release -Filter tankeye-openvino_frame_postprocess_smoke.exe` 通过；默认 `scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；模拟 PLC 启动生成 `build/Release/logs/tankeye_20260804_175624.log`，GUI 常驻超时后确认无残留 `tankeye` 进程；`git diff --check` 仅提示 CRLF。
- 未验证真实 PLC/真实相机；仍需用户用现场图片或相机画面人工确认蓝青色真实夹爪框与实际夹具长宽、方向是否匹配。

## 2026-08-04 当前补充记忆：真实夹爪框接管碰撞拒抓

- 用户确认真实夹爪长宽不只是可视化，而是要替代旧 3 倍框成为新的碰撞拒抓范围；旧延长框后续可能删除，因此真实夹爪框不能依赖旧框长边或旧框角点。
- 已实现边界更新：`frame_postprocess` 仍负责 AC 姿态基础信息、`grip_long_angle_deg`、基础抓取框和 O/A/B 选择；旧 3 倍框字段、生成、显示和测试残留已删除；真实夹爪碰撞与最终 AC 射线 C 点在 `frame_processing_service` 坐标转换后执行，不把坐标转换塞进 OpenVINO。
- 真实夹爪生成语义：中心使用 `PoseDetection::obb_center`，方向使用 `grip_long_angle_deg`，机械长宽来自工程设置，图像尺度由九点坐标转换在中心附近沿长轴/宽轴各取 1 像素估算；生成结果写入 `mechanical_gripper_corners`，普通/全显画面都只绘制该框、基础抓取框和射线，不再绘制旧延长框。
- 拒抓语义：若夹爪长宽 <= 0、坐标转换禁用/无效、局部尺度无效、当前自身 segment mask 无效，则保守把 detection 判为不可抓；若真实夹爪框与旁边 segment mask 在扣除当前 matched segment mask 后仍有重叠像素，则写入 `mask_collisions` 并拒抓；最后统一调用 `ResolveResultAfterFiltering()` 重算 `primary_index`、总 `pick_status_code` 和 detection 状态。
- 调试日志：沿用工程设置“启用后处理调试日志”，输出 `[PostprocessDebug] mechanical_gripper`、`mechanical_gripper_skip`、`mechanical_gripper_collision`、`mechanical_gripper_reject`，包含长宽、中心、角度、局部比例、像素长宽、四角点、碰撞 segment/ROI/像素数和拒抓原因。
- 验证已完成：Release 构建 `tankeye-openvino_frame_postprocess_smoke`、`tankeye-openvino_frame_overlay_test`、`tankeye-openvino_grab_limit_evaluator_test`、`tankeye-openvino_qt_app` 通过；完整 `scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` 通过；模拟 PLC 启动真实 Qt 程序生成 `build/Release/logs/tankeye_20260804_182109.log`，GUI 常驻导致 45 秒超时后确认无残留 `tankeye` 进程。
- 未验证风险：未连接真实 PLC/真实相机；需要用户在现场输入真实夹爪机械长宽、启用有效九点标定，用真实图片或相机画面复核蓝色真实夹爪框与实际夹具是否一致，并观察拒抓效果。

## 2026-08-04 当前补充记忆：真实夹爪框状态颜色

- 用户指出真实夹爪框接管旧延长框功能后，是否可抓取的框颜色没有按之前来。已修正：真实夹爪框不再使用固定蓝青色，而是复用 `DetectionStateColor()`，即主目标色/可抓绿色/不可抓红色。
- 后续又按用户要求彻底删除旧延长框显示；全显/调试模式也不再画旧框对比。
- 已验证：`tankeye-openvino_frame_overlay_test` Release 构建通过、`tankeye-openvino_qt_app` Release 构建通过、完整 `scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` 通过。

## 2026-08-04 当前补充记忆：旧延长框彻底删除

- 用户要求“把原来的那个延长的框的所有相关逻辑去除”，并确认采用“真实夹爪框完全接管”。当前实现已删除旧 3 倍延长 OBB 框字段、生成函数、缩放逻辑、overlay 显示、调试文案和自动化测试断言。
- 当前真实夹爪最终化流程：`frame_postprocess` 只输出 AC 基础姿态、`grip_long_angle_deg`、基础抓取框和 O/A/B 结构判断；`frame_processing_service` 先应用坐标转换，再生成 `mechanical_gripper_corners`，用 SEG 中心 O 到 OBB 中心 A 的方向在真实夹爪框边界求 C，更新 `arrow_start=A`、`arrow_end=C`、`angle_deg` 和按 `center_ray_offset_px` 偏移后的 `center_x/center_y`，随后重新应用坐标转换得到最终 `machine_x/machine_y`。
- 当前拒抓流程：真实夹爪框仍执行 `mechanical_gripper_corners ∩ 旁边 SEG mask ∩ 非当前自身 mask`；长宽无效、坐标转换无效、真实夹爪框或 C 点生成失败、当前自身 mask 无效时保守不可抓。
- 当前显示流程：普通和全显模式都不再绘制旧延长框；真实夹爪框复用状态色，基础抓取框和射线继续显示。
- 验证已完成：四个关键 Release 目标构建通过，完整测试脚本通过，模拟 PLC 启动日志为 `build/Release/logs/tankeye_20260804_184839.log`，无残留 `tankeye` 进程。未连接真实 PLC/真实相机。

## 2026-08-04 当前补充记忆：TankEye-Iris 1.4 打包
- 用户要求读取 `docs/PACKAGING_README.md`，先更新使用说明，然后打包版本号 1.4，且运行包不能包含 `docs/` 或 `AGENTS.md`。
- 已更新 `docs/PACKAGING_README.md`、`runtime/USAGE_GUIDE.txt` 和 `scripts/package_runtime.ps1`：路径和命令统一为 `TankEye-Iris_1.4`；使用说明补充真实夹爪框接管旧 3 倍延长 OBB 的行为；打包脚本默认版本/manifest 为 1.4，并增加 staging 守卫，若 `docs` 或 `AGENTS.md` 进入运行包则直接失败。
- 已执行真实打包命令：`powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\package_runtime.ps1 -BuildDir build -ReleaseName TankEye-Iris_1.4 -Force`，生成 `dist\TankEye-Iris_1.4` 和 `dist\TankEye-Iris_1.4.zip`；`windeployqt` 输出 `VCINSTALLDIR is not set` 警告但脚本成功完成。
- 已验证运行包关键文件存在：主程序、`platforms\qwindows.dll`、OBB/SEG OpenVINO 模型、`USAGE_GUIDE.txt` 和 zip；已验证 `dist\TankEye-Iris_1.4\docs=False`、`dist\TankEye-Iris_1.4\AGENTS.md=False`，zip 条目也不包含 `docs/` 或 `AGENTS.md`；`RELEASE_MANIFEST.json` 显示 `name=TankEye-Iris_1.4`、`version=1.4`、`runtime_only=true`；运行包内 `launch_tankeye.ps1 -Device CPU -SimulatePlc` 已启动真实 Qt 程序，日志 `dist\TankEye-Iris_1.4\logs\tankeye_20260804_190721.log` 确认模拟 PLC、主窗口创建、OBB/SEG 模型按 CPU 加载成功，GUI 常驻超时后已确认无残留 `tankeye` 进程；`git diff --check` 无空白错误，仅有 CRLF 换行提示。
- 本次没有连接真实 PLC、真实相机或真实设备；打包脚本提示未找到有效启用的九点方案，运行包内 `machine_limits.enabled=false`，现场部署前需要确认目标机器配置和九点标定方案。

## 2026-08-04 当前补充记忆：GitHub 上传
- 用户要求将当前改动上传到 GitHub。
- 已提交并推送主改动到 `origin/Unordered_Scraping_V5`：提交 `55f9b46`，提交标题 `Use mechanical gripper frame for collision and package 1.4`；远端从 `998dfff` 更新到 `55f9b46`。
- 该提交包含真实夹爪框完全接管旧延长框、相关测试、TankEye-Iris 1.4 打包脚本/使用说明和项目记忆更新；推送前确认未纳入 `dist/`、`build/`、模型权重或 `config/admin_auth.key`。
- 本地运行包产物仍在 `dist\TankEye-Iris_1.4` 和 `dist\TankEye-Iris_1.4.zip`，未纳入 Git；本次未连接真实 PLC、真实相机或真实设备。

## 2026-08-05 当前补充记忆：1.0.14 AC 垂直夹爪语义
- 用户提供 `models/推理v1.0.14(1).py` 作为相对 `models/推理v1.0.13.py` 的参考变化，并确认采用“真实夹爪长轴垂直 AC、真实夹爪宽度沿 AC”的 C++ 迁移口径；不要恢复 Python 临时 2 倍框或旧 3 倍延长 OBB 框。
- 已实现：`frame_postprocess` 仍先按现有 A/B/SEG 匹配和 `O -> A -> C` 几何选择 AC 射线，PLC/overlay 的 `angle_deg` 仍等于 `A -> C`；但输出给真实夹爪生成的 `grip_long_angle_deg` 改为 `AC angle + 90°` 并归一化，后处理日志标识更新为 `ray_logic=v1.0.14_ac_perp_gripper`。
- 已保持：`grab_limit_evaluator` 继续用工程设置真实夹爪长宽、九点坐标转换和 `grip_long_angle_deg` 生成 `mechanical_gripper_corners`；真实夹爪 mask 碰撞规则仍是 `mechanical_gripper_corners ∩ 旁边 SEG mask ∩ 非当前自身 mask`；长宽/坐标/自身 mask/C 点失败仍保守不可抓。
- 已补测试：`frame_postprocess_smoke` 验证 AC 角度仍为最终抓取角且 `grip_long_angle_deg` 与 AC 相差 90°；`grab_limit_evaluator_test` 验证水平 AC 下真实夹爪长边竖直、宽边水平，C 点落在宽向边界。
- 验证已完成：四个 Release 目标 `tankeye-openvino_frame_postprocess_smoke`、`tankeye-openvino_grab_limit_evaluator_test`、`tankeye-openvino_frame_overlay_test`、`tankeye-openvino_qt_app` 构建通过；默认测试脚本全部通过；单独 `frame_postprocess_smoke` 通过；模拟 PLC 启动日志为 `build/Release/logs/tankeye_20260805_152102.log`，GUI 常驻 60 秒超时后确认无残留 `tankeye` 进程；`git diff --check` 仅提示 CRLF。
- 当前工作区注意：`docs/project-memory/AI_ROLE_WORKFLOW.md` 是用户此前未提交修改，`models/推理v1.0.14(1).py` 是未跟踪参考脚本；后续不要覆盖或误删。未连接真实 PLC/真实相机，仍需现场复核真实夹爪方向、C 点、中心偏移和拒抓效果。

## 2026-08-05 当前补充记忆：参考脚本纳入仓库
- 用户要求将 `models/推理v1.0.14(1).py` 也上传到 GitHub，但明确 `models/推理v1.0.13.py` 不必改动。
- 本次仅是把 1.0.14 参考脚本纳入版本库管理，不改变当前 C++ 后处理和测试语义；后续若继续演进，仍以 C++ 现状和现场验证为准。
- 当前仓库同时保留 `推理v1.0.13.py` 和 `推理v1.0.14(1).py`，方便对照 1.0.13 与 1.0.14 的几何变化。未引入新的构建/运行验证需求。

## 2026-08-05 当前补充记忆：四类型角度补偿与沿 AC 中心偏移

- 已在当前 1.0.14 AC 几何语义上实现四类现场调机参数：D508 `1=大上右`、`3=大上左`、`4=小上右`、`2=小上左`。每类均有独立角度补偿和沿 `A -> C` 的机械毫米中心偏移，默认均为 0，并兼容缺少新字段的旧工程设置。
- 最终 PLC 命令合同统一叠加全局角度/轴补偿与类型补偿：类型角度补偿在全局角度补偿后应用；类型中心偏移先沿机械坐标 AC 单位向量移动，再叠加全局前后/左右轴补偿。`RobotController` 的真实 PLC 写入和诊断写入复用该合同，避免界面值与实际写入值分叉。
- 补偿后的最终中心和角度会重新执行机械 ROI 与角度限位检查；越界时保守拒抓。若配置了非零类型 AC 偏移但无有效机械坐标或 AC 方向，同样保守拒抓。真实夹爪框碰撞几何、九点标定和 A/B/SEG 匹配语义未改变。
- 工程设置已增加“四种抓取类型补偿”表；普通信息面板和目标卡片显示补偿后的 PLC 中心、机械坐标和角度，全显保留原始 OBB 中心和 AC 射线用于现场对照。
- 验证已完成：Release 构建 `tankeye-openvino_plc_result_contract_test`、`tankeye-openvino_engineering_settings_service_test`、`tankeye-openvino_grab_limit_evaluator_test`、`tankeye-openvino_frame_postprocess_smoke`、`tankeye-openvino_frame_overlay_test`、`tankeye-openvino_qt_app` 通过；`scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；模拟 PLC Qt 启动日志为 `build/Release/logs/tankeye_20260805_165752.log`，随后确认无残留 `tankeye` 进程。
- 未连接真实 PLC、真实相机或真实设备。后续现场应以四类实物分别确认：角度和 AC 偏移的正负方向、最终 D500/D502/D504、补偿后越界拒抓，以及工程设置表的可读性。

## 2026-08-05 当前补充记忆：四类型补偿调试日志

- 针对日志 `build/Release/logs/tankeye_20260805_172705.log` 无法判断补偿是否进入最终 PLC 命令的问题，已将四类型补偿诊断接入现有“启用后处理调试日志”开关。
- 调试开启时，`[PLC_DEBUG] compensation` 输出 D508、原始图像/机械中心、原始角度、机械 AC 单位向量、全局角度补偿、类型角度补偿、类型 AC 偏移、全局轴补偿和最终 D500/D502/D504/D506/D508；无有效坐标或 AC 方向时输出拒抓原因。
- 工程设置加载/保存时输出 `[PLC_DEBUG] head_type_compensation_loaded/saved`，逐项列出大上右、大上左、小上右、小上左及对应 D508 参数；PLC 触发和图片手动写入的最终限位判断输出 `[PLC_DEBUG] final_limit_check` 或 `manual_final_limit_check`。
- 调试关闭时上述新增 `[PLC_DEBUG]` 日志不输出。现有 PLC 正常写入日志保持原有行为。
- 验证已完成：Release 全量构建通过，`scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；使用 `launch_tankeye.ps1 -BuildDir build -Device CPU -SimulatePlc -DebugPostprocess` 启动成功，日志 `build/Release/logs/tankeye_20260805_174628.log` 确认模拟 PLC、`[PLC_DEBUG] head_type_compensation_loaded`、调试日志开关和 Qt 事件循环，随后确认无残留进程。

## 2026-08-05 当前补充记忆：补偿后命令姿态显示与写入日志闭环

- 根据 `tankeye_20260805_175305.log` 复核，四类型补偿计算已实际生效：例如 D508=1 在全局角度补偿 66、类型角度补偿 45、AC 偏移 5 mm 后，输出 `final_D500=183.652`、`final_D502=328.03`、`final_D504=43.45`。用户仍感觉“没效果”的主要原因是普通 overlay 仍画原始真实夹爪框/AC 射线，未使用补偿后的命令姿态。
- 已修正显示语义：普通画面使用补偿后的 `plc_command_center/plc_command_arrow`，并将真实夹爪框按命令中心偏移后绘制；全显/调试模式继续显示原始真实夹爪框和原始 AC 射线，便于对照补偿前后。
- 已补齐窗口缩放：`ScaleResultForDisplay()` 现在同步缩放 `plc_command_center`、`plc_command_arrow_start`、`plc_command_arrow_end`，避免窗口适配后命令姿态坐标不一致。
- 已补齐图片写 PLC 日志：调试开启时，`WriteFrameResultToPlc()` 会输出 `[PLC_DEBUG] manual_write status=result/reject/failed` 和对应 D500/D502/D504/D506/D508；主窗口图片写入回调已删除原有 `return` 后死代码，失败/成功状态提示走同一中文分支。
- 验证已完成：`tankeye-openvino_frame_overlay_test` Release 构建通过，`tankeye-openvino_qt_app` Release 构建通过，`scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；`launch_tankeye.ps1 -BuildDir build -Device CPU -SimulatePlc -DebugPostprocess` 启动成功，日志 `build/Release/logs/tankeye_20260805_180224.log` 确认模拟 PLC、补偿参数加载日志和 Qt 事件循环，随后确认无残留进程。

## 2026-08-05 当前补充记忆：角度补偿普通画面旋转闭环

- 根据用户复测日志 `build/Release/logs/tankeye_20260805_180512.log`，四类型角度补偿在最终 PLC 命令中已生效：例如 D508=1、`raw_angle=292.45`、`global_angle_offset=66`、`type_angle_offset=30` 时，`final_D504=28.45`；此前容易误解的是 `[PostprocessDebug] target ac_angle_deg` 和 `mechanical_gripper_pose angle_deg` 仍打印原始 CV 几何角度。
- 已修正普通 overlay 的视觉口径：`ApplyPlcCommandPose()` 现在按最终 `D504` 相对原始 `angle_deg` 的角度差生成命令 AC 箭头；普通画面中的真实夹爪框按同一角度差绕原始中心旋转并平移到补偿后中心。
- 为避免现场看到两个方向不同的框，普通画面在存在补偿后 PLC 命令姿态且真实夹爪框有效时，不再额外绘制原始基础 OBB；全显/调试模式仍显示原始几何用于对照。
- 验证已完成：`tankeye-openvino_frame_overlay_test`、`tankeye-openvino_qt_app` Release 构建通过；`scripts/run_build_tests.ps1 -BuildDir build -Configuration Release -Filter tankeye-openvino_frame_overlay_test.exe` 通过；默认 `scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；`launch_tankeye.ps1 -BuildDir build -Device CPU -SimulatePlc -DebugPostprocess` 启动成功，日志 `build/Release/logs/tankeye_20260805_181435.log` 确认模拟 PLC、补偿参数加载和 Qt 事件循环，随后确认无残留 `tankeye` 进程。
- 未连接真实 PLC、真实相机或真实设备；仍需用户用现场图片复核普通画面只保留补偿后的旋转夹爪框和最终 D504 箭头方向。

## 2026-08-05 当前补充记忆：D508 左右恢复为夹取 OBB 本地 AC 判定

- 用户指出“大上左”现场物料被反馈为“大上右”，且调“大上右”角度实际影响现场大上左。经 Git 历史核查，早期实现使用 `head_side_basis=left_local_keep`，前几轮在 AC 角度修正中改成了 `left_x_vs_segment_center`，即按 A.x 与 SEG 中心 O.x 判定左右，导致物料朝向变化时左右语义错误。
- 已按用户确认口径修正：D508 左右判定恢复为以夹取 OBB 本地坐标为基准，并采用当前 1.0.14 的 `A -> C` 射线作为本地前向；头部 B 在 `right_axis=(forward.y,-forward.x)` 一侧为右，否则为左。
- D508 编码和工程设置表含义保持不变：`1=大上右`、`3=大上左`、`4=小上右`、`2=小上左`；四类型补偿继续按最终 D508 应用。
- 调试日志 `head_side_basis` 已改为 `grip_local_ac`。新增后处理回归：A 在 SEG 右侧但 B 位于 A->C 本地左侧时，必须输出 `D508=3 大上左`，防止再次退回全局图像 X 判定。

## 2026-08-05 当前补充记忆：普通/全显同时显示原始 OBB 与补偿姿态

- 用户确认原始 OBB 框在普通模式也应该显示，全显模式也应该显示经过补偿的真实夹爪框和射线。上一轮“避免双框”的处理过度隐藏了普通基础 OBB，并在全显路径关闭了补偿姿态显示。
- 已修正：`frame_overlay` 现在始终绘制基础 `detection.corners`；只要 detection 有 `plc_command_pose`，普通和全显都会额外绘制补偿后的真实夹爪框与 AC 射线。全显继续额外显示 raw OBB、SEG、mask collision 等调试层。
- 已更新 `frame_overlay_test`：普通模式必须同时看到原始 OBB、补偿框和补偿射线；全显模式也必须同时看到原始与补偿姿态。
- 验证已完成：`tankeye-openvino_frame_overlay_test`、`tankeye-openvino_qt_app` Release 构建通过；`scripts/run_build_tests.ps1 -BuildDir build -Configuration Release -Filter tankeye-openvino_frame_overlay_test.exe` 通过；默认 `scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过。

## 2026-08-05 当前补充记忆：overlay 原始 OBB 去重与补偿同步

- 用户继续指出全显有两个原始 OBB 框，且普通模式下原始 OBB 框没有跟着夹爪框变化。根因是全显同时绘制 `raw_obb_regions` 与 detection 基础 OBB；普通模式只对真实夹爪框做了补偿姿态变换，基础 OBB 仍停在原检测位置。
- 已修正显示口径：普通模式绘制 detection 基础 OBB，但它会随 `plc_command_pose` 使用同一角度差和中心偏移旋转/平移；全显模式由 `raw_obb_regions` 负责显示原始模型 OBB，不再通过 detection 重复画一遍基础 OBB，同时仍绘制补偿后的真实夹爪框和 AC 射线。
- 已更新 `frame_overlay_test`：普通模式旧位置不应再残留基础 OBB，新补偿位置必须有基础 OBB/真实夹爪框，补偿射线也必须可见；全显模式必须能看到 raw OBB 与补偿姿态。
- 验证已完成：`tankeye-openvino_frame_overlay_test` 和 `tankeye-openvino_qt_app` Release 构建通过；`scripts/run_build_tests.ps1 -BuildDir build -Configuration Release -Filter tankeye-openvino_frame_overlay_test.exe` 通过；默认 `scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过。

## 2026-08-05 当前补充记忆：长时间运行风险扫查与 1.4.1 打包
- 用户要求大扫是否存在 bug、内存泄漏或长时间使用风险；有问题则修复，没有阻塞问题则打包 `TankEye-Iris_1.4.1`。
- 本轮静态扫查重点覆盖裸 `new/delete`、`deleteLater`、`QFutureWatcher`、`QtConcurrent`、`QTimer`、相机线程、`_dupenv_s/free`、PLC/overlay 最近改动和打包脚本。未发现新的确定性内存泄漏；Qt 对象基本由父对象或 `deleteLater()` 释放，相机线程使用 `std::atomic<bool>` 控制并在停止/析构 join。
- 已修一个低风险长跑性能/内存压力点：`ApplyPlcCommandPose()` 原来为每个 detection 复制完整 `FrameInferenceResult`，现在改为只构造当前目标需要的最小结果，避免多目标连续检测时复制 segments/raw OBB 等不必要数据；PLC 输出语义不变。
- 已将打包脚本、包内使用说明和打包说明更新到 1.4.1：`scripts/package_runtime.ps1` 默认 `ReleaseName=TankEye-Iris_1.4.1`，manifest `version=1.4.1`，`runtime/USAGE_GUIDE.txt` 与 `docs/PACKAGING_README.md` 路径同步。
- 已生成本地运行包：`dist\TankEye-Iris_1.4.1` 与 `dist\TankEye-Iris_1.4.1.zip`。包内 `RELEASE_MANIFEST.json` 显示 `name=TankEye-Iris_1.4.1`、`version=1.4.1`、`runtime_only=true`、`files=129`；包目录和 zip 均不包含 `docs/` 或 `AGENTS.md`。
- 验证已完成：`cmake --build build --config Release --target tankeye-openvino_qt_app` 通过；默认 `scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；`git diff --check` 无空白错误，仅有 CRLF 提示；打包命令成功，仅有已知 `VCINSTALLDIR is not set` 警告；包内 exe 哈希与 `build\Release` exe 一致；包内 `launch_tankeye.ps1 -Device CPU -SimulatePlc` 已启动真实 Qt 程序并正常退出，日志 `dist\TankEye-Iris_1.4.1\logs\tankeye_20260805_192737.log` 确认模拟 PLC、主窗口创建、OBB/SEG CPU 模型加载成功，退出后无残留 `tankeye` 进程。
- 本轮未连接真实 PLC、真实相机或真实设备；长期稳定性仍建议现场用真实相机连续采集观察内存占用、日志增长和 PLC 模拟/实机节拍，但实机连接必须先取得用户明确同意。
## 2026-08-05 当前补充记忆：开机自启与延迟启动

- 已完成开机自启功能：工程设置新增“启动设置”，包含“开机自启”开关和“延迟启动”秒数，默认开启、默认延迟 0 秒，合法范围 0~600 秒。
- 自启实现限定为当前 Windows 用户 Startup 文件夹入口，不使用注册表、服务或计划任务；关闭开机自启时会移除 `TankEye-Iris.vbs` 和 `TankEye-Iris.lnk` 启动入口。
- 生成桌面图标脚本已同步：源码 `scripts/create_desktop_shortcut.ps1` 和运行包内 `create_desktop_shortcut.ps1` 默认创建 Startup 快捷方式，可用 `-NoAutoStart` 跳过；启动脚本支持 `-StartupDelaySeconds`。
- 已重新生成本地运行包：`dist\TankEye-Iris_1.4.1` 和 `dist\TankEye-Iris_1.4.1.zip`。包内 manifest 仍为 `version=1.4.1`，文件数 129。
- 验证已完成：Release 主程序构建通过；`engineering_settings_service_test` 覆盖启动设置读写、延迟夹紧和临时 Startup 目录创建/删除；默认测试脚本全部通过；脚本语法检查通过；打包成功。未实际写入真实桌面/Startup，也未重启 Windows 做真实开机自启验证。
## 2026-08-05 当前补充记忆：普通 OBB 显示对齐真实夹爪框

- 用户基于 `build/Release/logs/tankeye_20260805_221938.log` 指出：普通画面中基础 OBB 框和真实夹爪框有时角度不同，有时又一致，期望后逻辑统一处理后两者同向。
- 根因已确认：`pose.corners` 来自 AC 几何候选对齐，候选可为 `head_long_angle` 或 `head_long_angle+90`；真实夹爪框固定使用 `grip_long_angle_deg=AC+90`。因此不同目标会因候选分支不同出现 0 度或 90 度视觉差异。
- 已修复显示层：普通 overlay 绘制基础 detection corners 前，根据真实夹爪框长边方向旋转对齐基础 OBB；不改 CV/PLC/碰撞/全显 raw OBB。全显仍保留 raw OBB、SEG、mask collision 和最终夹爪姿态用于对照。
- 验证已完成：`frame_overlay_test` 新增对齐回归并通过，Qt 主程序 Release 构建通过，默认测试脚本全部通过，1.4.1 运行包已重新生成。仍需现场图片肉眼复核普通/全显观感。
## 2026-08-06 - 修正开机自启 VBS 续行语法

- 用户重启后报 `TankEye-Iris.vbs` 第 18 行 `800A03EA`，新电脑“闪退”日志也只证明 Qt 主程序已进入主窗口和 OpenVINO 初始化，不是主程序先崩。
- 根因定位：`app_startup_manager` 生成的 VBS 含有 `command = "... & _"` 续行，但写文件时若再走 Qt 文本模式会把已带 CRLF 的内容二次换行，导致 `_` 与下一行之间插入空白行。
- 修正：开机启动脚本改为二进制原样写入，测试改为按原始字节检查 `"_\r\n\r\n"` / `"_\n\n"` 不存在，并确认续行命令行连续。
- 验证：`tankeye-openvino_engineering_settings_service_test`、`tankeye-openvino_qt_app` 构建通过，`scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过。

## 2026-08-06 - 开机自启默认延迟改为 0 秒并重新打包

- 用户确认目标口径：生成桌面图标后默认创建开机自启入口，且默认延迟为 0 秒；只有进入管理员工程设置关闭后才不自启。
- 已同步默认值：`StartupLaunchSettings::delay_seconds`、工程设置草稿默认值、源码 `scripts/create_desktop_shortcut.ps1`、运行包模板内 `create_desktop_shortcut.ps1` 均改为 0 秒。
- 已同步文档和测试：`engineering_settings_service_test` 默认启动延迟断言改为 0；`docs/PACKAGING_README.md`、`runtime/USAGE_GUIDE.txt` 和项目记忆同步默认 0 秒口径。
- 验证与产物：`tankeye-openvino_engineering_settings_service_test`、`tankeye-openvino_qt_app` Release 构建通过；默认 `scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；已重新生成 `dist\TankEye-Iris_1.4.1` 与 `dist\TankEye-Iris_1.4.1.zip`，包内 `create_desktop_shortcut.ps1` 显示 `[int]$StartupDelaySeconds = 0`。

- 2026-08-06: Startup auto-start now also appends `-AutoLoadModels`, so `0s` means no extra wait, not manual model selection.
- 2026-08-06: Startup auto-start now also appends `-AutoStartGrasp`; the app reads `TANKEYE_AUTO_START_GRASP` in `main.cpp`, only auto-enters PLC grasp after models are loaded, and the main toggle button label is now `开始 / 关闭`.
- 2026-08-06: Startup chain was consolidated further after finding the stale Startup entry problem. `main.cpp` now self-syncs the current user's Startup entry on app launch, `app_startup_manager` exposes `BuildStartupLaunchArguments()` for the single auto-start argument contract, and package `launch_tankeye.ps1` now accepts `-AutoLoadModels -AutoStartGrasp -StartupDelaySeconds` instead of failing on those arguments. Rebuilt `dist\TankEye-Iris_1.4.1` and zip; package smoke log `dist\TankEye-Iris_1.4.1\logs\tankeye_20260806_141949.log` confirms simulated PLC and CPU OBB/SEG model loading. Current user's `Startup\TankEye-Iris.vbs` was verified to contain `-StartupDelaySeconds 0 -AutoLoadModels -AutoStartGrasp`. No real camera/PLC auto-grasp smoke was run because auto-grasp attempts to open the camera.
- 2026-08-06: Startup chain was refactored again to reduce patchiness: the startup contract is now `-StartupProfile AutoStart -StartupDelaySeconds N`, not a long repeated switch list. `SyncStartupShortcut()` now skips rewriting unchanged VBS content and only migrates old `TankEye-Iris.lnk` leftovers. Both source and packaged `create_desktop_shortcut.ps1` now generate a Startup VBS instead of a Startup LNK, and the packaged launcher expands `StartupProfile` into auto-load/auto-start behavior. Rebuilt `dist\TankEye-Iris_1.4.1` and the current user Startup entry now contains `-StartupProfile AutoStart -StartupDelaySeconds 0`. No real camera auto-grasp smoke was run.
- 2026-08-06: Removed the leftover `-AutoLoadModels/-AutoStartGrasp` compatibility switches from source and packaged launch scripts after the user correctly pointed out that the first cleanup increased code. `-StartupProfile AutoStart` is now the only automatic startup contract; manual launch remains manual. Rebuilt `dist\TankEye-Iris_1.4.1`; package smoke log `dist\TankEye-Iris_1.4.1\logs\tankeye_20260806_145501.log` confirms simulated PLC and model loading.
- 2026-08-06: Generated project architecture diagram artifacts under `docs/architecture/`: editable source `TankEye-Iris_Architecture_Map.html` and PDF `TankEye-Iris_Architecture_Map.pdf`. The PDF contains a mind map, runtime flow architecture, and boundary notes for UI/workflow/CV/PLC/scripts.
- 2026-08-06: Restored source-tree `launch_tankeye.ps1 -AutoLoadModels` as a manual model-loading switch after the user ran the documented old command and saw empty OBB/SEG model arguments. The automatic startup contract remains `-StartupProfile AutoStart`; `-AutoLoadModels` only means "manual launch should pass default OBB/SEG model paths". Verified script parse, Release Qt build, default test script, and simulated PLC GUI launch log `build/Release/logs/tankeye_20260806_152730.log`, which shows non-empty OBB/SEG model arguments and OpenVINO loading both models.
- 2026-08-06: Packaged `TankEye-Iris_1.4.2` after restoring manual `-AutoLoadModels`. Updated package version/default release name/usage docs to 1.4.2 and made the packaged launcher accept `-AutoLoadModels` as a harmless manual compatibility switch. Verification passed: PowerShell parse, Release Qt/settings/status builds, default test script, package generation, manifest check (`name=TankEye-Iris_1.4.2`, `version=1.4.2`, `files=129`), no packaged `docs/` or `AGENTS.md`, exe hash matches build output, and packaged simulated-PLC launch log `dist/TankEye-Iris_1.4.2/logs/tankeye_20260806_153900.log` confirms OBB/SEG model arguments and CPU OpenVINO loading/warmup. No real PLC/camera/auto-grasp verification was run.
- 2026-08-06: Top-left app header now uses a fixed visible left column and white bold text for the configured app label beside the logo; this corrects the first attempt where the label could be squeezed to zero width by the top-bar grid. The visible name/version and centered title are editable in `config/tankeye.json` under `app.name`, `app.version`, and `app.title`, and parsed through `AppConfigService` with defaults `TankEye-Iris` / `1.4.2` / `截止阀抓取上料系统`. Current config keeps the user's `app.version` as `1.4.3`. UTF-8 default read/write rules were added to `AGENTS.md` and `OPERATING_LIMITS.md`. Verified app config test + Qt app Release builds, default `scripts/run_build_tests.ps1 -BuildDir build -Configuration Release`, and `git diff --check` with only the known CRLF warning. No package rebuild or GUI screenshot smoke was run.
- 2026-08-06: Repaired mojibake in `docs/PACKAGING_README.md` and `runtime/USAGE_GUIDE.txt` by rewriting both as clean UTF-8 Chinese documentation. Verified explicit UTF-8 reads, searched for common mojibake markers with no remaining bad-content hits other than intentional “避免中文乱码” wording, rebuilt `tankeye-openvino_qt_app`, and ran the default build test script successfully. Existing `dist\TankEye-Iris_1.4.2` was not regenerated, so packaged docs remain unchanged until the next package build.
- 2026-08-06: Repackaged `dist\TankEye-Iris_1.4.2` and zip after adding app-display locking for packaged launches. `AppConfigService` now ignores `app.name/app.version/app.title` when `TANKEYE_LOCK_APP_DISPLAY=1`, and packaged `launch_tankeye.ps1` sets that variable, so field testers cannot change displayed name/version/title by ordinary edits to packaged `config\tankeye.json`. Fixed `package_runtime.ps1` JSON rewrite path to explicit UTF-8 to prevent packaged config mojibake. Verification passed: app config test + Qt app Release builds, default build tests, package generation, manifest/hash/no-docs checks, packaged lock-variable check, and packaged config UTF-8 read. No packaged GUI smoke was run because app launch can write current-user Startup outside the repo.
- 2026-08-06: Repackaged `TankEye-Iris_1.4.3` after aligning scripts/docs/runtime usage notes to 1.4.3. The packaged launcher still sets `TANKEYE_LOCK_APP_DISPLAY=1`, so ordinary package config edits do not change the visible app name/version/title. Verification passed: app config test + Qt app Release builds, default build tests, package generation, manifest/hash/no-docs checks, packaged lock-variable check, and packaged config UTF-8 read. If a field-side version change is needed without source access, there is still no secure local-file-only way to make ordinary testers unable to edit it while allowing the owner to edit it; that would need a separate override design or a new package build.
- 2026-08-06: Fixed the field crash path shown by `samples\tankeye_20260806_175348.log`: `AUTO` no longer lets the Qt app directly try OpenVINO GPU. Added `tankeye-openvino_device_probe.exe`; source and packaged launchers run it for `-Device AUTO` and pass `GPU` only after probe success, otherwise pass `CPU`. The Qt app logs `Device probe result` and treats any raw `AUTO` env as CPU. The probe success definition is: OBB and SEG XML both read successfully and both compile on GPU; it does not run image inference. Fresh empty-cache GPU probe on this PC took 22.227 seconds, so the default probe timeout was raised to 60 seconds; cached probe took about 2 seconds. The field `USAGE_GUIDE.txt` was simplified by removing the internal “界面标题和版本显示” and “当前关键行为” sections and now says the app prefers GPU acceleration but automatically falls back to CPU if the graphics environment is unstable. Rebuilt `dist\TankEye-Iris_1.4.3` and zip; manifest file count is now 130. Verification passed: device probe + Qt app Release build, PowerShell parse checks, full build tests, source and packaged CPU smoke, source and packaged AUTO short-timeout fallback smoke, package manifest/hash/no-docs/no-AGENTS and usage-guide assertions. Smoke logs: `build\Release\logs\tankeye_20260806_181848.log`, `build\Release\logs\tankeye_20260806_182000.log`, `dist\TankEye-Iris_1.4.3\logs\tankeye_20260806_182236.log`, `dist\TankEye-Iris_1.4.3\logs\tankeye_20260806_182347.log`. No real PLC/camera/device/reboot/GPU-success validation was run.
- 2026-08-06: Engineering settings are now isolated per launch/package. Source `launch_tankeye.ps1` sets `TANKEYE_SETTINGS_INI_PATH=build\Release\config\engineering_settings.ini`; packaged launcher sets `TANKEYE_SETTINGS_INI_PATH=config\engineering_settings.ini` inside that package. This stops different package versions on the same Windows user from sharing Qt `QSettings("TankEye","TankEye-Iris")`. The Qt app logs `Engineering settings path` on startup. No migration from old user-level settings is done by design. Rebuilt `dist\TankEye-Iris_1.4.3` and zip. Verification passed: engineering settings service test, Qt app/device probe build, default test script, package generation, and packaged CPU smoke `dist\TankEye-Iris_1.4.3\logs\tankeye_20260806_191850.log`, which created `dist\TankEye-Iris_1.4.3\config\engineering_settings.ini` and logged the package-local path.
- 2026-08-06: Field package surface was cleaned after the user asked why `README_RUNTIME.md` was mojibake and what `RELEASE_MANIFEST.json` / `THIRD_PARTY_NOTICES.md` were. `scripts\package_runtime.ps1` no longer generates `README_RUNTIME.md`, `RELEASE_MANIFEST.json`, `THIRD_PARTY_NOTICES.md`, or `SHA256SUMS.txt`;现场包只保留 `USAGE_GUIDE.txt` 作为说明文档。Regenerated `dist\TankEye-Iris_1.4.3` and zip; directory and zip checks confirm the removed files are absent, while the app exe, device probe, models, CPU/GPU OpenVINO plugins, and `USAGE_GUIDE.txt` remain present. Parser check passed; packaging passed with known `VCINSTALLDIR is not set` warning; `git diff --check` only reported known CRLF warnings.
- 2026-08-06: Engineering settings numeric spin boxes and combo boxes now ignore mouse-wheel events unless the widget or its internal editor has focus. This prevents accidental parameter changes while scrolling the settings page, including `正向/反向`, `0~360/-180~180`, axis mapping, and coordinate profile selections. Implemented via click-focused `QDoubleSpinBox`/`QSpinBox`/`QComboBox` helpers in `engineering_settings_dialog_helpers`; runtime-log combo boxes were intentionally left unchanged. Added `tankeye-openvino_engineering_settings_spinbox_wheel_test` for focused/unfocused double spin box, integer spin box, and combo box behavior. Verification passed: Qt app + new test build, full `scripts\run_build_tests.ps1 -BuildDir build -Configuration Release`, static search for raw engineering spin boxes/combo boxes, and `git diff --check` with only known CRLF warnings. No real GUI manual scroll smoke or package rebuild was run.
- 2026-08-06: Packaged `TankEye-Iris_1.4.5` and prepared GitHub upload. Updated package default release name, source config display version, packaging docs and runtime usage guide to 1.4.5, all read/written as UTF-8. Verification passed: Release build of `tankeye-openvino_qt_app`, `tankeye-openvino_device_probe`, `tankeye-openvino_engineering_settings_service_test`, and `tankeye-openvino_engineering_settings_spinbox_wheel_test`; full `scripts\run_build_tests.ps1 -BuildDir build -Configuration Release`; package generation to `dist\TankEye-Iris_1.4.5` and zip; package file/hash/no-docs/no-AGENTS/no-internal-readme/no-manifest checks; package config and usage guide UTF-8 reads; direct packaged CPU + simulated PLC smoke log `dist\TankEye-Iris_1.4.5\logs\tankeye_pkg145_smoke_20260806_215728.log` confirms package-local settings path plus OBB/SEG CPU model compile and warmup. No real PLC, real camera, real device motion, real reboot, or GPU-success validation was run.
- 2026-08-08: Current workspace `D:\work_floder\jiezhifa\TankEye_source_for_new_pc` is a GitHub branch zip source snapshot, not a Git repository (`.git` is absent). Branch/worktree/untracked status cannot be determined from Git until a full clone or `.git` restoration is done. Project structure review confirmed the main maintained code is under `app/`, `core/`, `tests/`, `scripts/`, `config/`, `runtime/`, and `docs/project-memory/`; `_deps/`, `build/`, `dist/`, `vendor/`, model weights, logs, private keys, and local packages remain local/generated/private surfaces. Continue slimming by reducing duplicated implementation and maintenance points, not by moving code into more files.
- 2026-08-08: Angle calibration now uses a persistent physical anchor (`raw_angle_deg` + `target_angle_deg`) instead of treating forward/reverse as a one-shot calibration result. Direction and range changes recompute an equivalent offset, old settings still load, and PLC register semantics stayed unchanged. Verified with release builds, default test script, and a source-tree CPU + simulated PLC smoke log at `build/Release/logs/tankeye_20260808_173414.log`.
- 2026-08-08: Angle calibration display semantics were completed: per-detection `display_angle_deg` is computed from the same global angle calibration plus head-type compensation used for PLC output, and Qt overlay/right-side labels now use that calibrated display angle. This keeps calibration as numeric mapping only, does not rotate the full frame, and lets `大上左` / `小上左` style head-type results show the adjusted angle on screen even if a full PLC command pose cannot be drawn. Verified Release Qt/overlay builds, focused overlay test, full default test script, and CPU + simulated PLC GUI smoke log `build/Release/logs/tankeye_20260808_180318.log`. No real PLC/camera/device validation was run.
- 2026-08-08: The normal overlay black angle badge (`D508=... ...deg`) was removed after user screenshot feedback. `display_angle_deg` remains in the data path for right-side UI labels and calibrated semantics, but `frame_overlay.cpp` no longer draws the black label box over the image. Verified Qt/overlay Release builds, focused `frame_overlay_test`, and full default test script. No real camera/PLC/device validation was run.
- 2026-08-08: User log `build/Release/logs/tankeye_20260808_181624.log` showed why boxes became skewed after calibration: `global_angle_offset=223.32` changed final `D504` from raw `13.27` to `-149.95`, and the overlay was incorrectly rotating boxes/arrows by that PLC command angle delta. Fixed `frame_overlay.cpp` so overlay boxes and AC arrows keep raw CV image geometry; calibration remains numeric only for `D504` and right-side/display data. Verified Qt/overlay Release builds, focused overlay test, and full default test script. No real camera/PLC/device validation was run.
- 2026-08-08: After user noted that individual `大上左/大上右/小上左/小上右` visual adjustments no longer changed the on-screen box, the geometry rule was split: global angle calibration remains numeric-only, while per-head-type `angle_offset_deg` is now copied into `display_geometry_angle_offset_deg` and used by `frame_overlay.cpp` to rotate the corresponding OBB/gripper/AC-arrow display. Verified Qt/overlay Release builds, focused overlay test, and full default test script. No real camera/PLC/device validation was run.
- 2026-08-08: Angle `正向/反向` now means opposite gripper pose on the same calibrated axis. The PLC/display formula is `raw + global_offset + head_type_offset`, then reverse subtracts 180 degrees, then range formatting is applied. Example: `raw=30` and `offset=15` gives forward `45`; reverse gives `225` in `0~360` and `-135` in `-180~180`. Engineering-settings anchors remain stored as forward physical target angles, while the dialog converts reverse-mode displayed targets back to forward anchors before saving/calculating. Verified related Release builds, focused PLC/settings/config tests, full default test script, and CPU + simulated PLC GUI startup log `build/Release/logs/tankeye_20260808_191607.log`; no real PLC/camera/device validation was run.
- 2026-08-08: 已将用户要求的严格重构替换规则写入 `docs/project-memory/OPERATING_LIMITS.md`：代码修改必须先删除被替换旧逻辑并输出带行号的删除清单，再说明变更并输出完整函数/完整文件；禁止保留注释掉的旧代码、备用实现、重复逻辑和 TODO/FIXME/HACK 式拖延清理。已补充规则：以后读取和写入仓内文本默认使用 UTF-8，PowerShell 文本读写必须显式 `-Encoding UTF8`。本次为文档规则变更，无源码构建/测试需求。
- 2026-08-08: 默认模型目录已从 `models/weights/` 改为 `models/DG_8_weights/`。源码启动脚本、运行包生成脚本、部署脚本和 PT 转 OpenVINO 导出脚本都已同步；运行包内启动器也会默认加载包内 `models\DG_8_weights\best_obb.xml` 与 `best_seg.xml`。已确认当前目录存在四个模型文件并通过脚本解析验证；未重新构建主程序、未重新打包、未启动 GUI、未连接真实 PLC/相机。
- 2026-08-08: 已实现物料工程方案：新增 `project_profile_store`，每个运行实例在自身 `config/project_profiles/<方案名>/profile.json` 保存物料相关模型、阈值、曝光、角度校准、后处理、头部补偿、轴向补偿、限位、九点标定和夹爪尺寸；当前激活方案名保存在实例自己的 `engineering_settings.ini`。首次启动自动迁移生成 `DG_8`，默认模型为 `models/DG_8_weights`；切换方案会自动停止相机/PLC轮询、清空当前结果、应用参数并后台重载 OBB/SEG。管理员账号、开机自启、PLC连接参数、相机 IP 和界面状态仍保持全局。
- 2026-08-08: 工程设置顶部新增物料方案下拉、`新建`、`保存设置`、`另存为`、`删除`、`导入`、`导出`、`应用方案`；导入支持 JSON 和 ZIP，ZIP 可包含配置或配置+四个模型文件，导入后默认只加入方案列表并等待用户点击 `应用方案`。已新增 `project_profile_store_test` 覆盖 DG_8/DG_10 隔离、当前方案持久化、JSON 导入导出、带模型 ZIP 导入导出。
- 2026-08-08: 方案功能验证完成：`tankeye-openvino_qt_app` 和方案测试 Release 构建通过；默认 `scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；源码 CPU + 模拟 PLC 冒烟日志 `build/Release/logs/tankeye_20260808_2053xx.log` 确认进入 Qt event loop、使用 `build/Release/config/project_profiles/DG_8/profile.json` 运行并从 `models/DG_8_weights` 加载 OBB/SEG。未重新生成正式 dist 包，未人工点击工程设置，未连接真实 PLC/相机。
- 2026-08-08: 修复物料工程方案新建语义：工程设置中的编辑方案已与 active 运行方案分离；新建方案调用 `CreateBlankProjectProfile()`，模型路径为空、曝光/补偿/夹爪尺寸归零、坐标转换和上下限关闭，保存后立即刷新全部控件且工程设置保持打开；新建、保存、另存为、导入、删除均不改变当前运行 DG_8，也不关闭工程设置。
- 2026-08-08: 方案下拉切换、另存为和导入现在会把目标方案载入工程设置控件作为编辑草稿；只有“应用方案”会读取并保存当前草稿、校验 XML/BIN、停止运行流程并异步加载模型。模型加载失败时恢复应用前的运行方案参数，active 方案名只有模型加载成功后才持久化。
- 2026-08-08: 验证通过 `cmake --build build --config Release --target tankeye-openvino_qt_app tankeye-openvino_project_profile_store_test`、`scripts/run_build_tests.ps1 -BuildDir build -Configuration Release`，以及 CPU + 模拟 PLC 启动日志 `build/Release/logs/tankeye_20260808_213033.log`。尚未进行人工点击工程设置的完整 DG_8/DG_10 UI 验收，未连接真实 PLC/相机。
- 2026-08-08: 已生成 `dist/TankEye-Iris_1.4.6` 和 `dist/TankEye-Iris_1.4.6.zip`；源码配置、打包脚本、运行说明和应用默认版本统一为 1.4.6。包内包含 `config/project_profiles/DG_8/profile.json`、DG_8 四个模型文件、Qt 主程序和设备探测程序；包内 CPU + 模拟 PLC 冒烟日志为 `dist/TankEye-Iris_1.4.6/logs/tankeye_20260808_213652.log`，确认 OBB/SEG 从包内模型目录加载并 warmup。未连接真实 PLC/相机。
- 2026-08-08: 主界面“当前目标参数”区域新增操作工物料方案下拉框 `operatorProjectProfileSelector`。下拉显示全部已保存方案，包括空白草稿；操作工选择后必须二次确认，确认后复用 `switchProjectProfile()`，切换期间下拉禁用，取消/校验失败/模型加载失败均回填 active 方案。参数编辑、新建、保存、导入导出仍只在管理员工程设置中可用。
- 2026-08-08: 方案选择功能验证通过 `tankeye-openvino_qt_app` Release 构建和完整 `scripts/run_build_tests.ps1 -BuildDir build -Configuration Release`；CPU + 模拟 PLC 启动日志为 `build/Release/logs/tankeye_20260808_221011.log`，确认主界面创建、Qt event loop、DG_8 OBB/SEG CPU 模型加载和 warmup。尚未人工点击主界面下拉完成 DG_8/DG_10 切换回归，未连接真实 PLC/相机。
- 2026-08-08: 根据界面截图修正操作工方案选择器位置：从“当前主目标”参数卡片内部移除，移动到右侧栏标题“当前目标参数”同一行右侧，与标题并排显示；方案切换逻辑保持不变。验证通过主程序 Release 构建、完整测试脚本和 CPU + 模拟 PLC 启动冒烟，日志为 `build/Release/logs/tankeye_20260808_221614.log`。
- 2026-08-08: 操作工方案选择器宽度从 `SC(126)` 调整为 `SC(180)`，便于显示 DG_8、DG_10 及更长方案名；主程序 Release 构建通过。
- 2026-08-08: 根据用户反馈进一步优化方案选择器：在“当前目标参数”同一行增加明确的“方案选择：”文案；移除固定宽度和 Fixed 策略，改为最小宽度 + Expanding 随侧栏变化；增加专属深色、圆角、悬停、焦点和禁用样式。验证通过主程序构建、完整测试脚本和 CPU + 模拟 PLC 启动冒烟，日志为 `build/Release/logs/tankeye_20260808_222718.log`。
- 2026-08-08: 方案选择框右侧下拉箭头由 `ProjectProfileSelector::paintEvent()` 直接绘制为空心折线 `⌄`，正常色 `#f4f8fc` 与方案文字一致，禁用色为 `#9aaab7`；折线最小尺寸为 `12×7` 像素，使用圆头 2 像素描边，并在 Qt 未返回有效箭头区域时回退到控件最右侧 28 像素区域，避免 Windows 主题导致箭头不可见。主程序 Release 构建和完整测试脚本通过；尚未取得目标机器的人工截图确认。
- 2026-08-08: 已重新打包 `TankEye-Iris_1.4.7`，产物为 `dist/TankEye-Iris_1.4.7` 和 `dist/TankEye-Iris_1.4.7.zip`；打包脚本现统一校验和复制 `models/DG_8_weights`、`models/DG_10_weights` 两套 XML/BIN，并优先复制 `build/Release/config/project_profiles` 的当前方案目录。包内包含主程序、设备探测程序、DG_8/DG_10 两个方案和两套共八个模型文件。DG_10 当前仍是安全空白草稿，模型路径为空，首次应用前需管理员将 OBB/SEG 路径配置为 `models/DG_10_weights`。完整测试脚本通过；未启动包内 launcher，避免其写入当前用户桌面/Startup 快捷方式，未连接真实 PLC/相机。
- 2026-08-08: 工程设置新增“选择模型文件夹”：严格识别同一目录中的 `best_obb.xml/.bin` 和 `best_seg.xml/.bin`，完整时只填入当前编辑草稿的模型 A/B 路径，缺任一文件时显示缺失清单且不改变原路径；两个独立“浏览”按钮保留。新增 `model_folder_detector_test` 覆盖完整目录、缺 BIN、非标准命名和不存在目录；主程序构建与完整测试通过。尚未进行人工工程设置点击验证或 CPU + 模拟 PLC GUI 启动，因为当前启动链路会同步写入用户 Startup 入口。
- 2026-08-08: 已打包 `TankEye-Iris_1.4.8`，产物为 `dist/TankEye-Iris_1.4.8` 和 `dist/TankEye-Iris_1.4.8.zip`；源码配置、应用默认版本、打包脚本和运行说明统一为 1.4.8。包内包含模型文件夹自动识别功能、DG_8/DG_10 两个工程方案、两套共八个模型文件，目录包共 130 个文件。Release 主程序/设备探测程序构建、完整测试和包内静态完整性检查通过；未启动包内 launcher，未连接真实 PLC/相机。
- 2026-08-09: 修复工程设置“保存设置”只写文件、运行态不立即更新的问题。当前 active 方案保存后会停止相机/PLC、清空结果、应用完整方案参数，并在模型路径/阈值变化或模型未加载时后台重载模型；非 active 方案只保存不切换；模型应用失败时沿用现有 pending 方案回滚链路，工程设置保持打开，抓取不会自动恢复。新增共用 `validateProjectProfileModels()` 和 `applySavedEngineeringSettings()`；主程序构建与完整 Release 测试通过。尚未人工点击验证保存/抓取停止/重新开始流程，未连接真实 PLC/相机。
 - 2026-08-09: 继续验证“工程设置保存后立即生效”：Release 主程序与方案存储测试构建通过，`scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；真实 CPU + 模拟 PLC 启动冒烟生成 `build/Release/logs/tankeye_20260809_093157.log`，确认进入 Qt event loop，并从 `models/DG_8_weights` 加载、warmup OBB/SEG。未连接真实 PLC/相机，未人工点击保存 active/非 active 方案、抓取中保存和失败回滚流程。
- 2026-08-09: 已升级 `docs/project-memory/OPERATING_LIMITS.md` 中的严格重构替换规则：代码修改必须遵守最小改动原则，只改用户明确指定的功能；禁止顺手优化、重构周边代码或改动未提及逻辑；必须先输出带行号的删除清单，再输出变更清单并声明未改动部分，随后输出完整函数/完整文件和自检。继续要求仓内文本读写默认 UTF-8，PowerShell 必须显式 `-Encoding UTF8`。本次仅修改项目规则文档，无源码构建/测试需求。
- 2026-08-09: 按用户确认修正保存语义：active 方案保存只校验并写入方案文件，登记为待生效配置，不停止相机/PLC、不清空当前图片和检测结果，当前运行继续使用旧参数；下一次图片“开始检测”或 PLC 关闭后重新“开始”时才应用完整新方案。模型加载改为事务式替换，加载失败保留旧模型；方案切换会清理旧待生效状态，避免 DG_8 的待生效配置误套到 DG_10。保存 active 方案时缺少 XML/BIN 直接拦截，保存成功弹窗提示下一次开始生效。
- 2026-08-09: 本轮验证完成：`tankeye-openvino_qt_app` Release 构建通过，完整 `scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；CPU + 模拟 PLC 冒烟日志为 `build/Release/logs/tankeye_20260809_095938.log`，确认 Qt event loop、DG_8 OBB/SEG 模型加载和 warmup。未人工点击保存/开始接管流程，未连接真实 PLC/相机。
- 2026-08-09: 全局角度校准已改为累计口径。`取当前角度`保存视觉抓取角和正向物理目标锚点，不覆盖现有全局偏移；`计算校准`先用当前正反向/范围下的视觉基准角叠加旧全局偏移得到当前机械角，再以最短角差计算本次增量并累加。示例：视觉基准 `-67.55`、目标 `0`、旧偏移 `0` 得到 `+67.55`；连续同图再计算保持不变。切换正反向或范围会按首次锚点重算并放弃后续累计微调。
- 2026-08-09: 全局偏移仍只影响最终机械角/D504，不旋转图像检测框；D508 大小头角度补偿继续同时影响最终 D504 和对应图像框。工程设置已新增只读“视觉抓取角、当前机械角、最终 D504”三段诊断，工程方案读写会把 `-292.45` 这类等价偏移规范成易读的 `+67.55`。
- 2026-08-09: 验证完成：`tankeye-openvino_plc_result_contract_test`、`tankeye-openvino_engineering_settings_service_test`、`tankeye-openvino_project_profile_store_test`、`tankeye-openvino_qt_app` Release 构建通过；完整 `scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过。未启动 GUI，因为当前启动链路可能写入用户 Startup；未连接真实 PLC、真实相机或真实设备。后续需在获得该系统级写入许可后，人工确认工程设置诊断区布局、连续计算、方向/范围重算及大小头画框旋转。
- 2026-08-09: 进一步按用户口径修正角度弹窗输入稳定性：切换正向/反向或 `0~360 / -180~180` 时，不再改写“当前显示角度”和“机器目标角度”两个输入框。当前角输入框现在明确是“视觉基准角”，点击“取当前角度”按当时口径写入一次；目标角输入框始终表示正向机械基准目标，反向只影响最终 D504 预览和实际命令，不改输入值本身。
- 2026-08-09: 工程设置诊断区已从旧“三段角度”改为“视觉原始角、草稿机械基准角、草稿预览 D504、当前运行 D504”。右侧主界面仍只显示当前真实运行 D504；弹窗中的草稿预览专门用于区分未生效编辑值，避免运行态 `-164°` 与草稿 `39°` 混淆。
- 2026-08-09: 新增合同验证：当视觉原始角 `292.45°`、目标角输入 `39°`、全局偏移 `106.55°` 时，正向 D504 为 `39°`，反向 D504 为 `-141°`；完整默认测试脚本再次通过。未启动 GUI，因为当前启动链路可能写入当前用户 Startup；尚需在取得许可后人工点检角度弹窗实际交互。
- 2026-08-09: 已统一源码和运行包版本为 1.4.9.1。`config/tankeye.json`、`app/qt/workflow/app_config_service.h`、`scripts/package_runtime.ps1`、`runtime/USAGE_GUIDE.txt` 和 `docs/PACKAGING_README.md` 当前版本均已更新；已生成 `dist/TankEye-Iris_1.4.9.1` 与 `dist/TankEye-Iris_1.4.9.1.zip`。
- 2026-08-09: 1.4.9.1 打包验证完成：Release 主程序/设备探测程序构建通过，完整 `scripts/run_build_tests.ps1 -BuildDir build -Configuration Release` 全部通过；新包内主程序、设备探测程序、Qt 平台插件、DG_8/DG_10 模型、`USAGE_GUIDE.txt` 和 ZIP 均存在，包内 exe 与 `build/Release` hash 一致，包内配置与使用说明版本号为 1.4.9.1，且不包含 `docs/` 与 `AGENTS.md`。未启动包内 launcher，避免写入桌面或 Startup 快捷方式；未连接真实 PLC/相机。
- 2026-08-09: 角度校准已改为按范围独立保存：`0~360` 和 `-180~180` 各有自己的全局偏移槽位，切换范围只切换当前槽位，不改写视觉基准角和正向机械目标角。当前合同示例视觉 `292.45°`、目标 `39°`：`0~360` 偏移为 `-253.45°`，`-180~180` 偏移为 `+106.55°`；两套偏移分别参与累计校准、工程方案 JSON/INI 持久化和当前范围的 PLC/D504 计算。
- 2026-08-09: 范围独立偏移改动已完成定向测试和完整默认测试，并重新生成 `dist/TankEye-Iris_1.4.9.1` 与 ZIP。未启动 GUI，未连接真实 PLC、真实相机或真实设备；现场仍需确认范围切换后的工程设置显示和实际夹爪姿态。
- 2026-08-09: 按用户确认，全局角度校准已回退到 `TankEye_source_for_new_pc2` 老逻辑：只保留一份 `offset_deg`，D504 计算为 `raw * direction + offset + head_type_offset`，其中反向为 `direction=-1`，角度范围只做最终格式化。已删除 anchor、累计校准、双范围 offset、工程设置“取当前角度/计算校准”和草稿/运行 D504 诊断 UI；工程方案和应用配置下一次保存只写 `offset_deg/direction/range_mode`。验证通过定向 Release 构建和完整默认测试脚本；未启动 GUI，未连接真实 PLC/相机/设备，仍需现场人工确认角度区布局和夹爪执行角。
- 2026-08-09: 已在老全局角度公式基础上恢复工程设置“取当前角度、机械目标角度、计算校准”入口。`取当前角度` 按当前正反向/范围和零 offset 写入当前角度框；`计算校准` 用 `机械目标角度 - 当前角度` 计算单一 `offset_deg` 并写入角度偏移框；未恢复 anchor、累计校准、双范围 offset 或草稿/运行 D504 诊断。验证通过 Qt 主程序与 PLC 合同测试 Release 构建，以及完整默认测试脚本；未启动 GUI 或连接真实设备。
- 2026-08-09: 已打包 `TankEye-Iris_1.5.1`。源码配置、应用默认版本、打包脚本、运行说明和打包说明已统一到 1.5.1；产物为 `dist/TankEye-Iris_1.5.1` 和 `dist/TankEye-Iris_1.5.1.zip`。验证通过 Release 主程序/设备探测程序构建、完整默认测试脚本、正式打包、包内关键文件/DG_8/DG_10 模型/版本号/exe hash/no-docs/no-AGENTS 检查。未启动包内 launcher，未连接真实 PLC、真实相机或真实设备。
- 2026-08-10: 已按用户确认将视觉 AC 角度切换为 `推理v1.0.15.py` 的原始 A 框规则：`SEG` 中心 O 到原始 left OBB 中心 A，再从原始 A 框四边垂足中选择 `OA·AC` 最大的 C，最终 `angle_deg=atan2(AC)`；`pose.corners`、AC 箭头、`grip_long_angle_deg` 和后续真实夹爪/碰撞几何统一基于该新 AC。已删除旧 B 框长/短边旋转对齐候选逻辑和相关调试字段；B 候选仍按 `∠AOB` 最接近 90° 选择，D508 判型、PLC 全局偏移/正反向/范围/头型补偿、工程设置真实夹爪尺寸均保持不变。验证通过 `tankeye-openvino_frame_postprocess_smoke`、`tankeye-openvino_frame_overlay_test`、`tankeye-openvino_plc_result_contract_test`、`tankeye-openvino_qt_app` Release 构建，以及完整 `scripts/run_build_tests.ps1 -BuildDir build -Configuration Release`；未启动 GUI，未连接真实 PLC/相机/设备，现场仍需用真实图片对照 Python 脚本确认角度和夹爪方向。
- 2026-08-10: 已将当前发布版本统一为 `2.1`：更新 `config/tankeye.json`、`app/qt/workflow/app_config_service.h`、`scripts/package_runtime.ps1`、`runtime/USAGE_GUIDE.txt` 和 `docs/PACKAGING_README.md`；生成 `dist/TankEye-Iris_2.1` 与 `dist/TankEye-Iris_2.1.zip`。Release 主程序/设备探测程序构建和完整默认测试通过；包内关键 exe、Qt 平台插件、DG_8/DG_10 两套模型、配置和使用说明存在，包内版本为 2.1，exe hash 与 `build/Release` 一致，且不含 `docs/`、`AGENTS.md`。未启动包内 launcher，未连接真实 PLC/相机/设备。
- 2026-08-10: 已按用户截图反馈移除工程设置“物料工程方案”分组下方说明文案“工程方案保存物料相关参数；PLC、相机 IP、开机自启和管理员账号保持当前电脑设置。”；方案下拉和新建/另存为/删除/导入/导出/应用逻辑保持不变。验证通过 `tankeye-openvino_qt_app` Release 构建和完整 `scripts/run_build_tests.ps1 -BuildDir build -Configuration Release`；未启动 GUI。
- 2026-08-10: 只读排查日志 `build/Release/logs/tankeye_20260810_152146.log`。结论：旧数据并非完全不能检测大小头；在 `obb_conf_threshold=0.05` 后，OBB 输出了低置信度头部框：右上目标 `small conf=0.176025`、`big conf=0.122986`，左下附近 `big conf=0.0783691`。第一次检测仍只有 2 个高置信度 `left`，没有头部候选，拒抓原因为 `missing_head_candidate`；后续检测 `obb_count=7` 时右上目标通过，`matched_segment_index=3`、`D508=2`、`can_grab=true`、最终 `D500=464.3 D502=151.821 D504=153.24`。同时低阈值带出低置信度重复 `left conf=0.0673218`，导致 segment 0 出现 `left_count_in_segment=2` 并按现有结构保护 `invalid_segment_obb_structure` 拒抓。后续若要改善旧数据，优先考虑“大小头低阈值、left 保持高阈值”的分类阈值方案，而不是简单继续降低全局 OBB 阈值。
- 2026-08-10: 只读对比日志 `build/Release/logs/tankeye_20260810_160116.log` 与仓外 Python `D:\work_floder\jiezhifa\code\推理v1.0.16.py` 输出。同图 `samples/Image_20260808154107626.bmp` 中，C++ OpenVINO/GPU 使用 `models/DG_8_weights/best_obb.xml`、输入 640x640、`obb_conf_threshold=0.5`，输出 `big conf=0.919434`、`left conf=0.861816`，最终 `D508=1` 大上右；Python 脚本使用 `obb_best.pt` 和 Ultralytics 默认调用，用户输出为 `left conf=0.8937`、`big conf=0.8162`，同样判为大上右。置信度差异表现为 C++ 的 big 更高约 `+0.1032`，C++ 的 left 更低约 `-0.0319`；类别数量和判型一致，差异更可能来自 PT/Ultralytics 与 OpenVINO/GPU 推理、导出和后处理细节，而非项目大小头判型逻辑。
- 2026-08-10: 用户确认 PT/XML 来自同一训练模型后，使用 `cll_yolo` 环境做了同图底层实测：Python PT `D:\work_floder\jiezhifa\code\obb_best.pt` 对 `samples/images/test.png` 输出 `big=0.965852`、另一个 `big=0.720049`；直接用 Python OpenVINO `Core` 读取程序实际 XML `models/DG_8_weights/best_obb.xml`，按 C++ 同样 RGB/1/255/640 letterbox 输入，CPU 输出 big 最高 `0.118255/0.074596`，GPU 输出 `0.122986/0.078369`，与 `tankeye_20260810_152146.log` 中 C++ GPU 的 `0.122986/0.0783691` 精确对应，small `0.176025` 也对应。`test.png` 为正方形，padding 114 与 C++ 的 128 对该图没有影响。结论：置信度落差发生在 PT 到 OpenVINO XML/BIN 的模型导出/数值执行层，非 C++ 阈值、NMS、坐标、角度或大小头后处理。当前 XML 明确包含 FP16 压缩权重；后续应针对同一 PT 做一次 FP32（`--no-half`）OpenVINO 导出对照，再决定是否替换运行模型。
- 2026-08-10: 已导出试验用 FP32 OBB 模型到 `models/DG_8_weights_fp32_test/best_obb.xml/.bin`。来源仍是仓外 `D:\work_floder\jiezhifa\code\obb_best.pt`，通过 `cll_yolo` 环境与仓内临时 PT 复制完成。导出后同图实测：CPU 输出 `left=0.926380`、`big=0.965852`，GPU 输出 `left=0.925781`、`big=0.965820`，已恢复到与 PT 接近的高置信度量级。该试验模型不影响现有 DG_8 目录，供用户在工程设置中手动切换验证。
- 2026-08-10: 已把项目评审流程升级为六角色：在 `docs/project-memory/AI_ROLE_WORKFLOW.md` 中新增架构师角色，负责模块边界、状态归属、重复实现和长期收敛建议；同时删除顶部旧的注释版五角色流程，避免后续搜索混淆。后续涉及新功能、高风险 UI/CV/PLC 改动时，按“产品、项目、架构、CV、Qt、测试”六视角评审。
- 2026-08-10: 已修正 `scripts/export_pt_to_openvino.py` 的精度参数：删除容易遗漏的反向 `--no-half`，新增显式 `--precision {fp32,fp16}`，默认 `fp32`，只有用户明确传 `--precision fp16` 时才导出 FP16 OpenVINO。真实验证：`--help` 显示新参数；默认导出产物为约 10.6 MB 的 FP32；显式 `--precision fp16` 产物为约 5.6 MB，两个路径均导出成功。验证临时目录已清理，保留用户试验用 `models/DG_8_weights_fp32_test`。
- 2026-08-10: 工程设置“调试设置”新增按物料工程方案保存的显示开关“显示类型补偿后的夹取框”，配置字段为 `ObbPostprocessSettings::show_head_type_adjusted_geometry`，默认开启以保持现有行为。开启时主画面夹取框/检测框/AC 箭头继续叠加大小头类型角度补偿；关闭时这些 overlay 保持原始视觉检测姿态。该开关只影响 `frame_overlay.cpp` 使用 `display_geometry_angle_offset_deg` 的显示层，不影响 PLC/D504、D508 判型、拒抓/碰撞、安全框、模型推理或坐标转换。验证通过相关 Release 构建、三项定向测试和完整 `scripts/run_build_tests.ps1 -BuildDir build -Configuration Release`；未启动 GUI、未连接真实设备。
- 2026-08-10: 已生成 `TankEye-Iris_2.1.1` 运行包和 ZIP。源码配置、默认显示版本、打包脚本、使用说明和打包说明已同步到 `2.1.1`。验证通过 Release 主程序/设备探测程序构建、完整默认测试、正式打包、包内关键文件/DG_8/DG_10 模型/版本号/exe hash/no-docs/no-AGENTS 检查。未启动包内 launcher，未连接真实 PLC/相机/设备。
- 2026-08-12: 已完成正式管理员/工程设置授权绑定升级。旧手输 INIT/RESET 授权码链路已替换为 `config/admin_license.json` 文件授权；授权申请包含 Windows MachineGuid、BIOS/主板序列号、系统盘序列号、物理网卡 MAC 列表和 Qt machineUniqueId，license 校验要求 5 类全部匹配，MAC 类要求授权 MAC 列表与当前 MAC 列表完全一致。没有有效 license 时，普通检测/操作工流程继续可用，但创建/重置管理员、管理员登录进入工程设置均被拦截；license 有效时允许管理员流程；删除或替换无效 license 后已有管理员账号也不能进工程设置。
- 2026-08-12: 授权密钥来源收敛为 `config/admin_auth.key` 或脚本显式指定的 `TANKEYE_ADMIN_AUTH_KEY_FILE`，不再保留开发默认密钥常量或直接 secret 环境变量旁路。`scripts/generate_admin_auth_code.ps1` 改为输入现场复制的 request JSON、输出 `admin_license.json`；`scripts/package_runtime.ps1` 正式打包时必须找到 `config/admin_auth.key`，且通用包不默认携带某台机器的 `admin_license.json`。验证通过授权目标 Release 构建、完整默认测试、缺 key 失败检查和临时检查包规则检查；未启动真实 GUI，未连接真实 PLC/相机/设备，未读取或输出真实 key 内容。
- 2026-08-12: 已按用户要求简化管理员授权缺失提示。缺少 license 时底层状态文案为 `未找到管理员授权文件。`；主窗口管理员创建、登录、重置和工程设置入口统一提示 `普通检测可继续使用；管理员和工程设置需要本机有效授权文件。`。授权申请弹窗中仍保留 `admin_license.json` 文件名，便于工程师按文件流程生成和放置。验证通过 Qt 主程序/授权测试构建、授权定向测试和完整默认测试；未启动真实 GUI。
- 2026-08-12: 已补充缺授权时的复制入口。主窗口管理员创建、管理员登录、管理员重置、用户按钮和工程设置入口在 license 无效时不再只弹 OK 提示，而是打开授权申请窗口，显示机器码、授权状态和“复制授权申请”按钮；复制内容继续供 `scripts/generate_admin_auth_code.ps1 -RequestFile ... -Output config/admin_license.json` 生成授权文件。验证通过 Qt 主程序和管理员授权测试 Release 构建，以及完整默认测试；未启动真实 GUI、未连接真实 PLC/相机/设备。
- 2026-08-12: 管理员授权状态文案保持三态：没有 `admin_license.json` 显示 `无授权文件`；存在授权文件但签名、机器指纹或匹配数不符显示 `授权文件不符`；校验通过显示 `已授权`。虽然主入口授权成功会继续进入账号/密码阶段，但创建/重置管理员账号表单中的“授权状态”需要显示 `已授权`，避免状态行为空。
- 2026-08-12: 授权申请入口已从复制文本改为保存 JSON 文件。授权申请窗口和创建/重置管理员窗口按钮显示 `保存授权申请`，点击后打开保存文件对话框，默认文件名 `TankEye_admin_license_request_<机器码>.json`，内容仍为 `AdminAuthLicenseRequestText()` 的 request JSON；保存成功提示现场将文件发给工程师生成 `admin_license.json`。已移除 `admin_auth_dialogs` 中剪贴板和复制 toast 逻辑；验证通过 Qt 主程序、管理员授权测试 Release 构建、完整默认测试和静态搜索；未启动真实 GUI、未连接真实 PLC/相机/设备。
- 2026-08-12: 已根据 `config/TankEye_admin_license_request_TK-B57A-B3ED-5AA9-3F90.json` 生成 `config/admin_license.json`。使用命令 `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\generate_admin_auth_code.ps1 -RequestFile .\config\TankEye_admin_license_request_TK-B57A-B3ED-5AA9-3F90.json -Output .\config\admin_license.json`，生成结果文件大小 785 字节；随后管理员授权测试和 Qt 主程序 Release 构建通过。未输出或读取真实 `admin_auth.key` 内容，未启动 GUI、未连接真实 PLC/相机/设备。
- 2026-08-12: `docs/BUILD_RUN_PACKAGE_GUIDE.md` 已同步当前管理员授权流程：现场点击 `保存授权申请` 导出 request JSON，工程师使用 `generate_admin_auth_code.ps1` 生成 `config/admin_license.json`，现场放回运行包 `config` 后进入管理员账号或密码阶段，表单授权状态应显示 `已授权`；常见问题同步 `无授权文件` 和 `授权文件不符` 排查。静态搜索确认该指南中旧 `复制授权申请`/`授权码` 口径已清除。
- 2026-08-12: 已修复开发环境授权文件路径误解。程序原先只读取 `QCoreApplication::applicationDirPath()/config/admin_license.json`，导致源码根目录 `config/admin_license.json` 已存在时，从 `build/Release` 启动仍显示 `无授权文件`。现 `LicensePath()` 优先读取 exe 目录授权文件，缺失时兜底读取项目根目录 `../../config/admin_license.json`；当前授权文件也已复制到 `build/Release/config/admin_license.json`。验证通过管理员授权测试、Qt 主程序 Release 构建和完整默认测试；未启动 GUI、未连接真实 PLC/相机/设备。
- 2026-08-12: 管理员忘记密码已改为本地恢复问题机制。创建管理员账号时必须设置恢复问题和恢复答案，恢复答案使用独立 salt/hash 保存并在 UI 中像密码一样带小眼睛；忘记密码时仍要求本机 `admin_license.json` 有效，但必须先正确回答恢复问题才能重置账号密码和新的恢复问题/答案。旧账号没有恢复问题时不允许仅凭授权文件重置，会提示联系负责人清除账号后重新初始化。工程设置中知道当前密码的账号修改保留原恢复问题/答案。验证通过管理员授权测试、Qt 主程序 Release 构建和完整默认测试；未启动 GUI、未连接真实 PLC/相机/设备。
- 2026-08-12: 管理员恢复问题表单标签已简化，设置恢复问题/答案时不再显示 `新恢复问题`、`新恢复答案`，统一为 `恢复问题`、`恢复答案`；逻辑不变。Qt 主程序 Release 构建通过，未启动 GUI。
- 2026-08-12: 管理员重置表单语义已修正：上方 `恢复问题/恢复答案` 用于校验旧恢复答案；下方 `新恢复问题/新恢复答案` 为可选更新项，不填会保留原恢复凭据，成对填写才替换，单填一个会提示必须同时填写。恢复答案 hash 改用专用输入规范化；密码/恢复答案小眼睛按钮已设置 `Qt::NoFocus`，避免 Tab 需要按两次。验证通过管理员授权测试、Qt 主程序 Release 构建和完整默认测试；未启动 GUI、未连接真实 PLC/相机/设备。
- 2026-08-12: 已修正创建和重置管理员保存回调接反问题。创建管理员流程现在调用 `setAdminCredentials()`，恢复问题/答案仍必填；重置管理员流程调用 `AdminAuthResetCredentials()`，`新恢复问题/新恢复答案` 都不填时保留旧恢复凭据。验证通过静态确认、管理员授权测试、Qt 主程序 Release 构建和完整默认测试；未启动 GUI、未连接真实 PLC/相机/设备。
- 2026-08-12: `docs/PACKAGING_README.md` 和 `runtime/USAGE_GUIDE.txt` 已同步当前管理员授权教程：首次创建管理员时保存授权申请 JSON 并由工程师生成 `admin_license.json`；创建管理员必须设置恢复问题/答案；忘记密码时需本机授权文件有效且正确回答恢复问题，新恢复问题/答案可选更新。静态搜索确认两份文档不再保留旧 `复制授权申请` 和“首次创建或重置管理员账号”口径。
- 2026-08-12: 当前目录原本缺少 `.git`，已按用户要求初始化为 Git 仓库，远端配置为 `https://github.com/fuckMiss/Unordered_scraping.git`，当前分支为 `Unordered_scraping_v6`。本轮补充 `.gitignore` 排除 `config/admin_license.json`、`config/TankEye_admin_license_request_*.json` 和 DG_10 模型权重，确保提交不包含单机授权文件、授权申请 JSON、真实密钥、构建产物或发布包。远端检查确认尚无 `Unordered_scraping_v6` 分支，准备提交并推送当前快照。
- 2026-08-12: 已生成 `TankEye-Iris_2.1.2` 运行包和 ZIP。源码配置、应用默认版本、打包脚本、使用说明和打包说明已同步到 `2.1.2`；产物为 `dist/TankEye-Iris_2.1.2` 与 `dist/TankEye-Iris_2.1.2.zip`。验证通过 Release 主程序/设备探测程序构建、完整默认测试脚本、正式打包、包内关键文件/DG_8/DG_10 模型/CPU/GPU OpenVINO 插件/授权密钥/版本号/exe hash/no-docs/no-AGENTS 检查。未启动包内 launcher，避免写入桌面或 Startup 快捷方式；未连接真实 PLC、真实相机或真实设备。
- 2026-08-13: 管理员授权已按用户口径收紧为单机全匹配：`admin_license.json` 校验要求 Windows MachineGuid、BIOS/主板序列号、系统盘序列号、Qt machineUniqueId 和物理网卡 MAC 列表 5 类全部一致；MAC 类不再允许任一 MAC 命中，必须授权 MAC 列表与当前 MAC 列表完全一致。验证通过授权工具/授权测试/Qt 主程序 Release 构建和授权定向测试；后续每台机器都需要单独保存授权申请并生成对应授权文件。
- 2026-08-13: 已生成 `TankEye-Iris_2.1.3` 运行包和 ZIP。源码配置、应用默认版本、打包脚本、运行说明和打包说明同步到 `2.1.3`；产物为 `dist/TankEye-Iris_2.1.3` 与 `dist/TankEye-Iris_2.1.3.zip`。本包包含管理员授权 5 类硬件指纹全匹配、MAC 列表完全一致规则，以及自启 VBS 临时残留清理修复；包内快捷方式脚本已同步临时目录中转写入和 `TankEye-Iris.vbs.*` 精确清理。验证通过 Release 主程序/设备探测程序/授权工具/授权测试构建、完整默认测试、正式打包和包内关键文件/版本/hash/no-docs/no-AGENTS/脚本逻辑检查；未启动包内 launcher，未连接真实 PLC、真实相机或真实设备。
