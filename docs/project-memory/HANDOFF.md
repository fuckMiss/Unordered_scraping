# TankEye-Iris 会话交接

更新时间：2026-10-14

## 最终基线与发布

- 用户确认2.2.3为最终版，授权本地/GitHub main同步及删除已被main包含的其他分支。
- 本轮从V7基线59e0e27创建本地main，保留远端旧main 7e0bfad的独立历史，以最终Windows源码树为准正常推送，不force push。
- 发布与分支删除已完成，结果见本文件末尾及TASK_HISTORY；当前提交以`git rev-parse HEAD`和`git ls-remote origin refs/heads/main`核对，不写入自引用哈希。
- 项目事实以源码、CMake、配置、脚本、测试和project-memory为准。历史事件见TASK_HISTORY，收尾前记忆快照见archive/2026-10-14-memory-before-closeout.md。

## 2.2.3 源码与本地产物

- 旧几何候选基线为59e0e27；无2.1.9原包/标签，不能声称原包精确还原。
- 保留Canny、重建矩形、二次选C、固定45px、基础框、垂直AC两倍放大；corners为基础框，夹爪方向为AC+90°；类型显示旋转后平移，不改安全几何。
- 灰度推理支持1/3通道；原图显示/保存、回车、工程方案、权限、FIFO和错误图片保存保留。
- 错误图片来自最近PLC完成帧的原图与同帧AllDebug，不重跑推理、不参与FIFO；普通PLC图片默认1000，成功保存后清理目标PNG。
- `dist/TankEye-Iris_2.2.3`和ZIP是当前正式本地产物；143文件、149ZIP条目、156952177字节，SHA256=`101BB804D1DF71CE66803C977D2113BECE08D85285B1D905198F3589ED4FF948`。
- 旧2.2.2包保留；构建/包/模型/插件/key及ZIP文件哈希已在2026-10-12核验，本轮再次核验结果见TASK_HISTORY。运行包不提交Git。
- 包内无有效启用九点方案，因此machine_limits.enabled=false；现场核对标定和限位。DG_8/DG_10模型显式打包，自定义模型不会自动加入。

## 验证边界与待验

- 历史Release默认15/17通过，认证/滚轮超时终止；后处理冒烟另行通过，2.2.3打包相关6项通过。本轮实际复核另行记录，不改成全通过。
- 认证原测试会清空用户级QSettings，执行前必须仓内隔离；Release旧assert会被NDEBUG移除。两个Qt阻塞测试本次不修复。
- 没有用户现场验收反馈；未确认包内GUI、真实相机、PLC或设备动作，提交/推送不等于现场已部署。
- 旧方向偏差和0x20线状退化框候选行为保留，几何结果变化可影响拒抓和PLC数值。
- 既有VCINSTALLDIR打包警告、QComboBox弃用警告未消除。

## 资料保护与下一步

- 模型、结果BMP、output、build、dist、私有依赖和授权资料均保留本地，未上传。
- PROGRESS/BLOCKED保留为历史复核现场，不作为当前任务入口；无文件清场。
- 用户现场验收2.2.3，优先核对标定/限位，再检查A/C、方向、显示补偿、错误图同帧及FIFO/回车；反馈后继续处理。


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


## 2026-10-14 - 重新提交当前 README 到 main

- 用户要求当前项目重新提交main；实际新增变更仅为README.md/README_zh.md删除末尾“最终维护基线”说明，原样纳入提交，不恢复该段、不改变用户文件字节。
- 源码、2.2.3功能及运行包没有新增修改，均保持上一版状态；PROGRESS.md、BLOCKED.md继续保留为未跟踪历史复核文件，不纳入源码提交。
- 实际验证：两份README按UTF-8读取及内部链接存在性检查通过，git diff --check通过；仅文档变更，本轮未重新构建、启动GUI或连接设备。
- 本次仅提交两份README及三份动态记忆记录，并正常推送main；发布哈希与工作区状态以本轮Git核验为准。此前“README改动未提交”的记载已由本次重新提交取代。
