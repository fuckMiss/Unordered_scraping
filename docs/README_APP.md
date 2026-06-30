# 应用层说明

这份文档只说明 `qt/` 里的应用层职责和当前行为。

## 结构

- `qt/grasp_main_window.*`：主窗口和交互
- `qt/grasp_workflow.*`：模型加载、相机、单帧推理
- `qt/frame_postprocess.*`：OBB + SEG 联合后处理
- `qt/frame_overlay.*`：结果绘制
- `qt/robot_controller.*`：PLC 通信和机械臂流程
- `qt/frame_result.h`：统一结果结构

## 当前界面能力

- 同时加载 OBB engine 和 SEG engine
- 加载图片或打开相机
- 执行单次检测或自动抓取
- PLC 联动，监听 `D1500`
- PLC 测试写入
- 右下角显示模式切换：`隐藏 / 全显`

## 显示模式

显示模式按钮当前用于控制叠加内容：

- `隐藏`：只显示主目标
- `全显`：显示全部检测框和 SEG 结果

对应实现位置：

- `qt/grasp_main_window.cpp`
- `qt/frame_overlay.cpp`

## 联合后处理

当前后处理以抓取场景为目标：

- 将 OBB 和 SEG 原始输出转换成应用层结构
- 选择主目标 `primary_index`
- 生成抓取所需的角度、中心点、状态码、头型编号
- 结果最终会写入 PLC 的 `D500/D502/D504/D506/D508`

## 自动抓取

自动抓取是模拟闭环流程：

```text
检测主目标 -> 交给 RobotController -> 模拟机械臂回原点 -> 回调后继续检测
```

当前 `RobotController` 里仍然是本地模拟流程，不是真实机械臂驱动。

