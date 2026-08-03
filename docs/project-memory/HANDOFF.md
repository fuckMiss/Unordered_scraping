# TankEye-Iris 会话交接

更新时间：2026-08-03

## 当前状态

- 已建立项目协作档案体系。
- 本次文档整理不依赖 README，事实来源为源码、CMake、配置、脚本和测试。
- 现有业务代码改动未被回滚或覆盖。
- 已完成第一批代码清理：运行日志查看器从 `GraspMainWindow` 抽离到 `app/qt/ui/runtime_log_dialog.h/.cpp`。
- 已完成第二批代码清理：管理员创建、登录、重置弹窗从 `GraspMainWindow` 抽离到 `app/qt/ui/admin_auth_dialogs.h/.cpp`。
- 已完成第二批补充清理：创建/重置管理员弹窗重复结构已合并，日志/管理员弹窗复用 `app/qt/ui/ui_scale_utils.h`。
- 已完成全项目瘦身第一批：OBB/SEG CLI 入口复用 `app/cli/cli_inference_runner.h`。
- 已完成全项目瘦身第二批：CMake 测试目标复用 `add_tankeye_test()`；管理员账号存储集中到 `admin_auth_helpers` 并新增测试；工程设置弹窗复用 grid/标签 helper。
- 已完成全项目瘦身第三批：`GraspMainWindow` 顶部栏图标按钮创建/静态图标刷新，以及目标卡片创建/字段刷新复用内部 helper，未新增文件。

## 本次新增/整理

- 根目录协作入口：`AGENTS.md`
- 静态项目档案：`docs/project-memory/PROJECT_CONFIGURATION.md`
- 动态记忆：`docs/project-memory/MEMORY.md`
- 历史记录：`docs/project-memory/TASK_HISTORY.md`
- 操作限制：`docs/project-memory/OPERATING_LIMITS.md`
- 当前交接：`docs/project-memory/HANDOFF.md`
- 旧交接摘要归档：`docs/project-memory/archive/2026-08-03-legacy-session-handoff.md`

## 继续工作建议

1. 执行 `git status --short`，确认当前脏工作区。
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
- 未判断既有业务代码改动是否应保留。
