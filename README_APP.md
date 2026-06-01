# YOLOv11 Qt 应用层说明

本文档说明当前 `Qt` 应用层的职责、与部署层的边界、构建方式以及运行方式。

## 当前结构

项目现在分成两层：

- `src/`
  纯部署层，只负责 TensorRT 推理能力本身。
- `qt/`
  纯应用层，只负责界面、输入源管理、双模型编排、结果展示，以及后续业务后处理扩展。

当前应用层是一个联合入口：

- 同一套 Qt 界面同时调用 `OBB` 和 `SEG`
- 对同一帧先跑 `OBB`，再跑 `SEG`
- 在应用层完成结果汇总、关联和叠加显示

## 目录说明

```text
qt/
├── main.cpp                # Qt 应用入口
├── grasp_main_window.*     # 主窗口与界面交互
├── grasp_workflow.*        # 应用层双模型编排
├── frame_result.h          # 应用层统一结果结构
├── frame_postprocess.*     # 应用层后处理与 OBB/SEG 关联
├── frame_overlay.*         # 应用层联合绘制
└── assets/
```

## 分层原则

当前代码按下面原则组织：

- `src/` 不包含 Qt 依赖
- `src/` 不包含界面逻辑
- `src/` 不包含业务后处理规则
- `qt/` 不直接改写部署层推理内部逻辑
- 未来要加的抓取判定、筛选、优先级、轨迹选择等逻辑，应继续放在 `qt/frame_postprocess.*`

## 当前应用层能力

当前 Qt 应用已经支持：

- 同时加载 `OBB engine` 和 `SEG engine`
- 加载单张图片
- 打开默认相机
- 在同一帧上执行 `OBB + SEG` 联合推理
- 在主画面叠加：
  - `SEG mask`
  - `SEG bbox`
  - `OBB rotated box`
  - `OBB 方向箭头`
- 在右侧显示：
  - `OBB 类别`
  - `SEG 关联类别`
  - `X / Y`
  - `角度`
  - `关联状态`
- 在顶部显示：
  - 运行模式
  - 当前状态
  - `OBB` 数量
  - `SEG` 数量
  - `OBB / SEG` 分阶段耗时
  - 总耗时

## 当前后处理逻辑

当前应用层后处理是一个基础版本：

- 将 `OBB` 原始结果转换成应用层目标结构
- 将 `SEG` 原始结果转换成应用层区域结构
- 用 `bbox IoU` 做轻量关联
- 给出一个主目标 `primary_index`

代码位置：

- `qt/frame_postprocess.cpp`

这只是预留的第一版挂点，不是最终业务规则。

## 构建

构建 Qt 联合版：

```bash
cmake -S . -B build
cmake --build build --target yolov11-tensorrt_qt_app -j4
```

Qt 应用目标名：

```bash
build/yolov11-tensorrt_qt_app
```

## 运行

### 方式 1：启动时直接传入两个 engine

```bash
./build/yolov11-tensorrt_qt_app ./weights/best_obb.engine ./weights/yolo11s-seg.engine
```

程序首次显示时会自动尝试加载这两个模型。

参数顺序固定：

```text
argv[1] = OBB engine
argv[2] = SEG engine
```

### 方式 2：先启动界面，再手动加载模型

```bash
./build/yolov11-tensorrt_qt_app
```

然后在界面里：

1. 点 `工程设置`
2. 选择 `OBB engine`
3. 选择 `SEG engine`
4. 点 `加载双模型`
5. 再加载图片或打开相机
6. 点 `开始检测`

## 建议的模型使用方式

虽然程序层面允许任意 `OBB` 和 `SEG` engine 组合，但实际业务上建议：

- `OBB` 和 `SEG` 使用同一业务域数据训练出来的模型
- 类别定义尽量匹配
- 输入尺寸和部署方式尽量统一

否则界面上的“关联结果”可以跑通，但业务意义可能不成立。

## 当前已验证到的范围

本机已完成的验证包括：

- Qt 目标编译通过
- 原有 `DET / OBB / SEG` 命令行目标编译通过
- Qt 程序可启动
- 本机 GPU 上单独 `OBB` 和 `SEG` 部署命令已实跑成功

Qt 联合版的最终显示效果仍建议在桌面环境中手动点一遍确认，因为它本质上是交互式 GUI 程序。

## 后续扩展建议

后面如果你继续加业务逻辑，建议优先加在这些位置：

- `qt/frame_postprocess.*`
  放抓取可行性判断、目标过滤、优先级排序、业务规则
- `qt/grasp_workflow.*`
  放输入源和多模型编排
- `qt/grasp_main_window.*`
  放界面交互和显示

不要把这些逻辑塞回 `src/` 部署层。
