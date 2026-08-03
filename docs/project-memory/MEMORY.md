# TankEye-Iris 动态记忆

更新时间：2026-08-03

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

## 当前工作区状态

- 当前本地工作区包含本次上下限保护区域可视化和五角色流程改动，尚未提交。
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
- 后续清理优先“合并重复逻辑、减少总代码”，不要只做文件搬迁；每次拆模块后检查是否产生重复 helper。
- 下一批高收益候选：继续压缩 `GraspMainWindow` 中目标列表/状态刷新重复显示逻辑；合并 `app/qt/main.cpp` 和启动脚本中的日志/启动参数说明重复；审查 OpenVINO OBB/SEG 推理类中可共享的预处理/运行时状态。
- 若继续 UI 调整，先构建 `tankeye-openvino_qt_app`，再实机启动截图确认窗口、右侧栏、标题栏和缩放表现。
- 若继续启动脚本调整，需在目标分辨率或目标机器上验证 Qt/Windows 自动缩放效果。
