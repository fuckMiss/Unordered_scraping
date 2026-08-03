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

## 当前工作区状态

- 已知未提交业务代码改动：
  - `app/qt/ui/grasp_main_window.cpp`
  - `launch_tankeye.ps1`
- 已知新增协作文档：
  - `AGENTS.md`
  - `docs/project-memory/PROJECT_CONFIGURATION.md`
  - `docs/project-memory/TASK_HISTORY.md`
  - `docs/project-memory/MEMORY.md`
  - `docs/project-memory/OPERATING_LIMITS.md`
  - `docs/project-memory/HANDOFF.md`
- 本批新增代码结构：
  - `app/cli/cli_inference_runner.h`
  - `app/qt/ui/runtime_log_dialog.h`
  - `app/qt/ui/runtime_log_dialog.cpp`
  - `app/qt/ui/admin_auth_dialogs.h`
  - `app/qt/ui/admin_auth_dialogs.cpp`
  - `app/qt/ui/ui_scale_utils.h`
- 本批新增测试：
  - `tests/admin_auth_helpers_test.cpp`
- 本批修改构建配置：
  - `CMakeLists.txt`
- 本次不回滚、不覆盖既有业务代码改动。

## 未完成决策

- 是否保留 `app/qt/ui/grasp_main_window.cpp` 当前 UI/图标缩放相关改动，需要后续结合真实窗口运行效果决定。
- 是否保留 `launch_tankeye.ps1` 中移除自动 `QT_SCALE_FACTOR` 的改动，需要后续实机验证不同分辨率下 UI 缩放表现。
- 是否需要把当前协作文档体系纳入 Git 提交，由用户决定。

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
- 后续清理优先“合并重复逻辑、减少总代码”，不要只做文件搬迁；每次拆模块后检查是否产生重复 helper。
- 下一批高收益候选：继续压缩 `GraspMainWindow` 中目标列表/状态刷新重复显示逻辑；合并 `app/qt/main.cpp` 和启动脚本中的日志/启动参数说明重复；审查 OpenVINO OBB/SEG 推理类中可共享的预处理/运行时状态。
- 先审查当前两个业务代码改动的意图和影响，再决定是否拆分提交。
- 若继续 UI 调整，先构建 `tankeye-openvino_qt_app`，再实机启动截图确认窗口、右侧栏、标题栏和缩放表现。
- 若继续启动脚本调整，需在目标分辨率或目标机器上验证 Qt/Windows 自动缩放效果。
