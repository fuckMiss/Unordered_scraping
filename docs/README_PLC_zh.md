# PLC 接口

TankEye-Iris 通过 Modbus TCP 将抓取结果写入 PLC。本文档按当前 `qt/robot_controller.*` 和 PLC 联动 UI 逻辑整理。

## 连接

- 协议：Modbus TCP
- 默认端点：`192.168.3.205:502`
- 实现位置：`qt/robot_controller.cpp`

## 寄存器映射

当前代码将每个业务值作为 32 位浮点数写入，占用两个连续 holding registers。默认字序为高字在前：

```text
D500   center_x
D502   center_y
D504   angle_deg
D506   pick_status
D508   head_type
D1500  trigger
```

## 写入值

`writeFrameResult()` 会写入当前主目标：

- `center_x`
- `center_y`
- `angle_deg`
- `pick_status`
- `head_type`

`writePlcTestValues()` 会写入固定测试值：

```text
D500 = 123.4
D502 = 56.7
D504 = 90.0
D506 = 1.0
D508 = 4.0
```

## 触发流程

启用 PLC 联动模式后，界面会轮询 `D1500`。

```text
D1500 = 1
-> 读取触发信号
-> 执行一次检测
-> 写入 D500/D502/D504/D506/D508
-> 清除 D1500
```

触发读取逻辑会先按 32 位 float 读取 `D1500/D1501`，判断是否接近 `1.0`；如果不是，再按单 word 读取 `D1500`，判断是否等于 `1`。清除触发时，当前代码会向 `D1500/D1501` 写入 float `0.0`。

如果没有可用相机帧，应用会写入空结果。按当前逻辑，这通常表示 `D506 = 3`。

## 抓取状态

当前后处理使用：

```text
1  可抓取
2  多个可抓取目标
3  不可抓取
```

注意：当前 `frame_postprocess.cpp` 定义了“多个可抓取目标”的状态码，但主结果选择逻辑会选择一个 primary target 并写出它的状态。PLC 最终写入值以 `RobotController::buildWriteResult()` 的当前实现为准。

## 说明

- 当前 PLC 读写调用是同步执行的。
- 触发处理完成后会清除 `D1500`。
- 如果 PLC 寄存器地址、数据类型或字序发生变化，请更新 `qt/robot_controller.*`。
