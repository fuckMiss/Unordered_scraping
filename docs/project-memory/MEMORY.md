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
- 当前仓库包含本地生成目录和私有/大文件目录，如 `build/`、`dist/`、`_deps/`、`vendor/hik_mvs/`、`models/weights/`，它们多数被 `.gitignore` 排除。
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
