# TankEye-Iris 会话交接

更新时间：2026-08-03

## 当前状态

- 已建立项目协作档案体系。
- 本次文档整理不依赖 README，事实来源为源码、CMake、配置、脚本和测试。
- 当前本地工作区包含本次上下限保护区域可视化和五角色流程改动，尚未提交。
- 已完成第一批代码清理：运行日志查看器从 `GraspMainWindow` 抽离到 `app/qt/ui/runtime_log_dialog.h/.cpp`。
- 已完成第二批代码清理：管理员创建、登录、重置弹窗从 `GraspMainWindow` 抽离到 `app/qt/ui/admin_auth_dialogs.h/.cpp`。
- 已完成第二批补充清理：创建/重置管理员弹窗重复结构已合并，日志/管理员弹窗复用 `app/qt/ui/ui_scale_utils.h`。
- 已完成全项目瘦身第一批：OBB/SEG CLI 入口复用 `app/cli/cli_inference_runner.h`。
- 已完成全项目瘦身第二批：CMake 测试目标复用 `add_tankeye_test()`；管理员账号存储集中到 `admin_auth_helpers` 并新增测试；工程设置弹窗复用 grid/标签 helper。
- 已完成全项目瘦身第三批：`GraspMainWindow` 顶部栏图标按钮创建/静态图标刷新，以及目标卡片创建/字段刷新复用内部 helper，未新增文件。
- 已实现上下限保护区域可视化：机械 ROI 反投影、主画面 overlay、工程设置显示开关和五角色流程文档。
- 已实现坐标转换方案导入与 `应用` 按钮升级：支持原生 JSON 和 VisionMaster TXT 五列导入（图像X、图像Y、机械X、机械Y、角度；角度不参与矩阵计算），`应用` 读取当前九点编辑区并刷新原有状态提示，单独 `计算矩阵` 按钮和重复矩阵显示框已移除，`另存为` 改为高反差命名对话框。
- 已实现延长夹爪只按真实 SEG mask 碰撞拒抓：bbox/minAreaRect 外框不再参与拒抓；只有延长夹爪 OBB 与旁边物料 `segment.mask` 白色像素发生 1 像素及以上重叠才判不可抓；自身 segment 继续忽略；调试 overlay 通过 `mask_collisions` 显示真实重叠红色像素。
- 已补充放大夹取 OBB 碰撞归属修正：放大 OBB 的中心放大形状和倍率不变，但最终碰撞 mask 会扣除当前 matched segment mask；旁边框或旁边 mask 只压到当前自身 mask 内不拒当前目标，A 扫到 B 时只拒 A。
- 已实现普通显示按 SEG 去重 OBB 框：普通主画面和普通目标列表每个 SEG 只显示一个代表 detection；全显/调试模式保留全部 detections 和 raw OBB 以便排查误检。

## 本次新增/整理

- 根目录协作入口：`AGENTS.md`
- 静态项目档案：`docs/project-memory/PROJECT_CONFIGURATION.md`
- 动态记忆：`docs/project-memory/MEMORY.md`
- 历史记录：`docs/project-memory/TASK_HISTORY.md`
- 操作限制：`docs/project-memory/OPERATING_LIMITS.md`
- 当前交接：`docs/project-memory/HANDOFF.md`
- 五角色协作流程：`docs/project-memory/AI_ROLE_WORKFLOW.md`
- 旧交接摘要归档：`docs/project-memory/archive/2026-08-03-legacy-session-handoff.md`

## 继续工作建议

1. 执行 `git status --short`，确认本次未提交改动范围。
2. 人工或桌面自动化点击“运行日志”，验证新抽离模块的 UI 行为。
3. 人工或桌面自动化打开管理员创建/登录/重置弹窗，验证新抽离模块的 UI 行为。
4. 先审查 `app/qt/ui/grasp_main_window.cpp` 与 `launch_tankeye.ps1` 的既有改动。
5. 下一批优先选“能减少重复代码”的任务，例如目标列表/状态刷新显示逻辑、OpenVINO OBB/SEG 共享逻辑、启动参数与日志初始化重复。
6. 如继续 UI/启动脚本任务，必须构建并真实启动程序验证；顶部栏和目标列表本轮已做模拟 PLC 启动冒烟，但未人工点击目标列表、最小化/最大化/关闭等按钮。
7. 每个任务结束后追加 `TASK_HISTORY.md`，并更新 `MEMORY.md`。

## 未完成事项

- 第一批代码清理已完成主程序构建、完整测试、后处理冒烟测试和真实启动验证；真实点击“运行日志”弹窗仍需执行。
- 第二批代码清理与重复逻辑合并已完成主程序构建、默认测试和后处理冒烟测试；真实管理员弹窗点开验证仍需执行。
- CLI 重复入口合并已完成 OBB/SEG 目标构建、Release 全部构建、默认测试和后处理冒烟测试；usage/错误输出文字尚未自动断言。
- CMake/管理员存储/工程设置 helper 合并已完成 Release 全部构建、默认测试、后处理冒烟测试和真实启动验证；工程设置/管理员/运行日志弹窗尚需人工或桌面自动化点检。
- 顶部栏/目标卡片 helper 合并已完成 `tankeye-openvino_qt_app` Release 构建、默认测试和模拟 PLC 真实启动验证，日志：`build/Release/logs/codex_target_card_smoke_20260803_123043.log`。
- 上下限保护区域可视化已完成 `tankeye-openvino_qt_app` Release 构建、默认测试、新增 `frame_overlay_test` 和模拟 PLC 真实启动，日志：`build/Release/logs/codex_grab_limit_overlay_smoke_20260803_144745.log`；人工加载图片或相机画面观察保护区叠层仍需执行。
- 坐标转换导入与应用按钮升级已完成 `tankeye-openvino_calibration_profile_store_test`、`tankeye-openvino_qt_app` Release 构建、默认测试脚本和模拟 PLC 真实启动，日志：`build/Release/logs/tankeye_20260803_153259.log`；已按用户提供 VM 五列样例补充解析测试，工程设置视觉细节仍建议现场人工确认。
- 延长夹爪真实 mask 碰撞拒抓已完成 `tankeye-openvino_frame_postprocess_smoke`、`tankeye-openvino_frame_overlay_test`、`tankeye-openvino_qt_app` Release 构建、默认测试脚本和模拟 PLC 真实启动，日志：`build/Release/logs/tankeye_20260803_175948.log`；未连接真实 PLC/相机，未用现场真实画面人工确认调试红色重叠像素。
- 放大夹取 OBB 碰撞归属修正已完成 `tankeye-openvino_frame_postprocess_smoke`、`tankeye-openvino_frame_overlay_test`、`tankeye-openvino_qt_app` Release 构建、默认测试脚本和模拟 PLC 真实启动，日志：`build/Release/logs/tankeye_20260803_182210.log`；未连接真实 PLC/相机，仍需用用户同一张现场图片复核中间目标。
- 普通显示按 SEG 去重 OBB 框已完成 `tankeye-openvino_frame_overlay_test`、`tankeye-openvino_qt_app` Release 构建、默认测试脚本和模拟 PLC 真实启动，日志：`build/Release/logs/tankeye_20260803_184719.log`；未连接真实 PLC/相机，仍需用现场图片人工切换普通/全显确认显示差异。
- 本次源码与协作文档改动尚未提交；后续继续改动时仍需避免覆盖用户新增变更。

- 2026-08-06 note: Startup entry generation now includes `-AutoLoadModels`; `0s` only removes TankEye's extra sleep and Windows can still wait for user login/session startup.
- `main.cpp` 已读取 `TANKEYE_AUTO_START_GRASP` 并传入主窗，启动入口与运行包生成脚本也都同步带上 `-AutoLoadModels -AutoStartGrasp`。
- `main.cpp` 现在会在程序启动时按工程设置自愈同步当前用户 Startup 入口；已确认当前用户 `Startup\TankEye-Iris.vbs` 指向 `dist\TankEye-Iris_1.4.1\launch_tankeye.ps1`，并带 `-StartupDelaySeconds 0 -AutoLoadModels -AutoStartGrasp`。
- 已重新生成 `dist\TankEye-Iris_1.4.1` 和 `dist\TankEye-Iris_1.4.1.zip`；包内 `launch_tankeye.ps1` 已能接收自启参数，包内模拟 PLC 启动日志为 `dist\TankEye-Iris_1.4.1\logs\tankeye_20260806_141949.log`。
- 按当前约束，这轮没有实际跑带自动抓取的 GUI 冒烟，因为自动抓取会进入相机链路；如果后续要补最终现场确认，建议在允许相机动作的环境里单独做一次启动验证。
- 2026-08-06 latest: startup contract has been refactored to `-StartupProfile AutoStart -StartupDelaySeconds 0`; the old Startup LNK path is now only migration cleanup. Source and packaged launchers no longer expose legacy `-AutoLoadModels/-AutoStartGrasp`; `StartupProfile` is the only automatic startup contract.
