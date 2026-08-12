# TankEye-Iris 协作规则

本文件是本仓库后续 AI/代理协作的入口规则。项目事实以源码、CMake、配置、脚本和测试为准；`README*` 可能滞后，不能作为唯一依据。

## 工作边界

- 只允许修改本仓库目录内的文件：`D:\work_floder\jiezhifa\TankEye_source_for_new_pc`。
- 仓库外文件只读，不得修改、移动或删除。
- 系统级、环境级、依赖安装、注册表、驱动、全局配置、目录外写入、真实设备/PLC 相关副作用操作，必须先询问并获得明确同意。
- 不回滚、不覆盖用户已有改动；遇到脏工作区先识别来源和影响，再继续。

## 工作方式

- 先读实际项目结构与配置，再做修改；不要依赖过期 README。
- 读取和写入仓内文本文件默认使用 UTF-8；PowerShell 必须显式使用 `-Encoding UTF8`，脚本生成文本也必须写 UTF-8，避免中文文档、配置和界面文案乱码。
- 大任务必须拆成小批次推进，每批有清晰目标、改动范围和验证方式。
- 每次提交的提交信息必须写清楚相对上一版发生了什么变化、改了哪些范围、验证了什么；不能只写笼统的 `update`、`fix`、`change`。
- 每次任务结束后更新 `docs/project-memory/TASK_HISTORY.md`。
- 当前任务、未完成决策、已知问题和交接信息维护在 `docs/project-memory/MEMORY.md`。
- 当上下文消耗接近 80% 时，先更新动态记忆；需要压缩时使用 `/compact`。上下文即将耗尽时，必须把未完成事项写入 `docs/project-memory/HANDOFF.md`。

## 验证要求

- 不能用模拟结果替代真实验证结论。
- 多文件或多功能改动必须做集成验证，全部通过后才算完成。
- 涉及 UI、启动、打包、硬件、PLC、模型加载的改动，需要尽可能在真实程序或真实产物上验证；无法实机验证时，必须明确标注未验证风险。

## 文档入口

- 静态项目档案：`docs/project-memory/PROJECT_CONFIGURATION.md`
- 动态任务记忆：`docs/project-memory/MEMORY.md`
- 历史任务记录：`docs/project-memory/TASK_HISTORY.md`
- 操作限制细则：`docs/project-memory/OPERATING_LIMITS.md`
- 会话交接：`docs/project-memory/HANDOFF.md`
