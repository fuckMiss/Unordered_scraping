# TankEye-Iris 动态记忆

更新时间：2026-10-14

## 当前状态

- 用户确认当前 2.2.3 为最终版，并授权同步所有项目文档、提交/推送 main、删除已被 main 完整包含的本地与远端其他分支。
- 发布前已确认远端旧 main=`7e0bfad977794a4a86d621598b9a7bdd7050b0fd`，V7 基线=`59e0e27b60994f096b02c5902a09d46796144697`；两条历史没有共同祖先。发布保留两条历史，以当前最终 Windows 源码树为准，不强制推送。
- 当前发布执行结果以 HANDOFF.md、TASK_HISTORY.md 及 `git log/status/ls-remote` 为准；本文件不固定自引用的最终提交哈希。

## 最终实现

- 当前默认版本和正式本地产物均为 2.2.3。旧几何恢复以 59e0e27 和 2.1.9 前后记录为依据，无 2.1.9 原包/标签，不声称原包精确复原。
- Canny 三边微调、矩形重建、二次垂足选 C、固定 45 px A、基础框与垂直 AC 两倍放大保留；corners 为最终 A/C 基础框，真实夹爪方向为归一化 AC+90°，类型显示旋转后平移，不写回安全几何。
- OBB/SEG 使用由原彩色 BGR 派生的灰度图，预处理按模型 shape 支持 1/3 通道；显示、保存和后处理保留彩色原图语义。
- 保存错误图片保持最近 PLC 完成帧原图与同帧 AllDebug 诊断图，写入 error_images，不参与普通 FIFO；普通 PLC 图片默认保留1000，设置范围1~100000，成功保存后才清理最旧目标 PNG。
- 回车交互、方案管理、管理员授权、机械尺寸、坐标转换、安全过滤和 PLC 通信没有因本轮几何回退而重写。
- DG_8 仅在方案列表为空时创建，至少保留一个方案；删除活动方案先成功加载替代模型。
- Manual 默认不自动进入抓取，AutoStart 在模型加载后请求已有 PLC 联动，仍受工程设置自启开关约束。

## 产物与验证

- `dist/TankEye-Iris_2.2.3` 143个文件，ZIP149条目、156952177字节，SHA256=`101BB804D1DF71CE66803C977D2113BECE08D85285B1D905198F3589ED4FF948`。
- 2026-10-12 构建和打包核验通过，目录/ZIP文件哈希一致、exe与build一致；本轮文档不改变包内容。
- 同会话默认15/17通过，认证/滚轮各30秒超时并终止，后处理冒烟另行通过；2.2.3打包前相关六项测试通过。2026-10-14复核结果另见TASK_HISTORY，不把历史验证当本轮执行。
- 原认证测试会清空Windows用户级QSettings，必须先隔离设置及临时目录，不能直接运行；Release标准assert受NDEBUG影响，本轮新增几何/显示/安全检查使用显式失败路径。
- 本地无有效启用九点方案，包内machine_limits.enabled=false是既有打包规则；现场需核对标定和限位。自定义模型目录不会自动进入包，脚本只显式打包DG_8/DG_10。
- 未收到用户现场验收结果；未确认真实GUI、相机、PLC或设备动作，本地提交/发布不等于现场部署。
- 旧算法可能保留历史方向偏差，0x20线状退化检测框可产生候选；几何变化可能影响拒抓及PLC数值。

## 工作区与资料保护

- build、dist、依赖、私有SDK、模型、授权key、output及结果BMP不纳入源码提交，不删除或移动。
- PROGRESS.md、BLOCKED.md是历史复核现场，已标注历史状态并保留；不能继续作为当前任务入口。
- 用户已有推理v1.0.16(1).py删除状态纳入最终源码快照；无回滚用户改动。
- 收尾前完整动态记忆保存在archive/2026-10-14-memory-before-closeout.md；历史只作追溯。唯一当前状态见本文件和HANDOFF，详细事件见TASK_HISTORY。

## 下一步

- 用户现场验证A/C、AC+90°方向、类型显示补偿、拒抓/PLC值、错误图同帧内容、FIFO和回车交互；反馈后再处理。
- 未授权额外设备连接或系统改动，不修复本次范围外的两个Qt测试。


## 2026-10-14 本轮复核

完整Release构建通过；默认安全测试15PASS/滚轮1TIMEOUT，后处理冒烟通过，认证因用户级设置副作用未重跑。文档UTF-8/链接、业务文件哈希、2.2.3包/ZIP及exe核验通过。架构PDF两页已实际渲染目视检查，SHA256 `DC7E2B1059799E407A4528EE51AE4265CD3B32A94A7DF292B3879D7DEE4B2E9A`；现场验收仍待用户反馈。发布执行结果见下方完成记录。

## 2026-10-14 发布与分支收尾完成

- 最终源码快照提交 `55fe8b6a6d5f3d4cb8ffda1c4e39a43fbbf6247f`；保留旧 main 历史的合并提交 `781201f4b6ddf671c53702217bcf3864fb6533f2` 已正常推送到 GitHub main，并通过 ls-remote 核验本地/远端一致。后续只有本段文档闭环提交，最终 HEAD 以 Git 查询为准；未改写历史、未强制推送 main。
- 用户明确授权后，逐条验证祖先关系；本地及远端 Unordered_scraping_v6、Unordered_scraping_v7 已删除，远端删除使用原子操作和预期哈希租约，防止误删并发新增提交。本地仅保留 main。
- 保留6个具有独有提交的远端分支：Unordered_Scraping_V5, Unordered_scraping-v3, Unordered_scraping_unit, Unordered_scraping_v2, Unordered_scraping_v4, runtime-package。它们未被 main 完整包含，按用户确认的条件不删除，不为清场擅自合入旧功能。
- 业务/模型/结果文件及2.2.3运行包保持不变；本地PROGRESS.md、BLOCKED.md保留为已标注历史的未跟踪复核现场，未上传、不删除。其他资料遵循ignore保留本地。
- 代码/local包/文档/规则/获准记忆均已核验或同步；GitHub发布已核验。现场GUI、相机、PLC及部署验收仍pending，不将推送当作现场上线；两个Qt测试限制和既有工具警告保留。

## 2026-10-14 - 按最终授权删除全部非 main 分支

- 用户追加明确授权：GitHub 仓库除 main 外的其余分支全部删除；本次授权取代此前“仅删除已被 main 包含的分支”的条件。
- 删除前 main 为 f6da27c19a8d982fbb08f0e62a4523188aa1c93f；远端6条剩余分支与其提交已逐一核对。
- 先生成并通过 git bundle verify 校验完整历史备份：build/branch_cleanup_20261014/retired_remote_branches.bundle；备份仅保存在本仓库，不创建其他远端分支或标签。
- 已使用预期提交哈希租约和原子操作删除 Unordered_Scraping_V5、Unordered_scraping-v3、Unordered_scraping_unit、Unordered_scraping_v2、Unordered_scraping_v4、runtime-package。
- 删除后 git ls-remote --heads origin 实测仅剩 main；本地分支仅 main，远端跟踪分支仅 origin/main。没有删除文件、改业务代码或改2.2.3运行包。
- 原文中“保留6个独有提交分支”记录的是上一阶段状态；当前状态为全部非main分支已删除，历史可从仓内bundle恢复。
- 任务开始后观察到README_zh.md及随后README.md的用户本地未提交改动，均保留，不夹带到本次记录提交。PROGRESS/BLOCKED历史现场也保留。
- 本次仅Git分支操作和记录更新，不重复构建/GUI/设备测试；真实验证为备份完整性、原子删除返回值及远端/本地分支清单。发布记录单独提交main，最终提交以Git查询为准。
