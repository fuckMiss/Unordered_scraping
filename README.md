# YOLOv11 TensorRT

这是一个以部署层为核心、并额外带有 `Qt` 应用层的 YOLOv11 TensorRT C++ 项目。

当前包含两部分：

- 部署层：`DET / OBB / SEG`
- 应用层：`Qt` 联合界面，负责同时调用 `OBB + SEG`

当前保留 4 个入口：

- `DET`：标准目标检测
- `OBB`：旋转框检测
- `SEG`：实例分割
- `APP`：Qt 联合应用层

## 界面演示

![Qt APP 截止阀无序抓取上料视觉检测系统演示](docs/images/app-demo.png)

## 文档导航

- `README.md`
  项目总览与构建入口
- `README_APP.md`
  Qt 应用层说明、分层方式、构建和运行方法
- `README_OBB.md`
  `OBB` 部署与推理说明
- `README_SEG.md`
  `SEG` 部署与推理说明

## 当前可执行入口

### 1. DET

- 源文件：`main.cpp`
- 目标：`yolov11-tensorrt`

### 2. OBB

- 源文件：`main_obb.cpp`
- 目标：`yolov11-tensorrt_obb`

### 3. SEG

- 源文件：`main_seg.cpp`
- 目标：`yolov11-tensorrt_seg`

### 4. APP

- 源文件：`qt/main.cpp`
- 目标：`yolov11-tensorrt_qt_app`

## 项目结构

```text
yolov11-tensorrt/
├── asset/
├── build/
├── qt/
├── src/
│   ├── YOLOv11.cpp
│   ├── YOLOv11.h
│   ├── YOLOv11_OBB.cpp
│   ├── YOLOv11_OBB.h
│   ├── YOLOv11_SEG.cpp
│   ├── YOLOv11_SEG.h
│   ├── deploy_utils.cpp
│   ├── deploy_utils.h
│   ├── trt_utils.cpp
│   ├── trt_utils.h
│   ├── preprocess.cu
│   ├── preprocess.h
│   ├── cuda_utils.h
│   ├── runtime_utils.h
│   ├── logging.h
│   ├── macros.h
│   ├── model_utils.h
│   └── common.h
├── main.cpp
├── main_obb.cpp
├── main_seg.cpp
├── CMakeLists.txt
├── README.md
├── README_APP.md
├── README_OBB.md
└── README_SEG.md
```

## 基本构建

```bash
mkdir -p build
cd build
cmake ..
cmake --build . --config Release
```

如果只想单独编译某个目标：

```bash
cmake --build . --target yolov11-tensorrt --config Release
cmake --build . --target yolov11-tensorrt_obb --config Release
cmake --build . --target yolov11-tensorrt_seg --config Release
cmake --build . --target yolov11-tensorrt_qt_app --config Release
```

## 快速开始

### 1. DET

```bash
./build/yolov11-tensorrt ./weights/yolo11s_trt.engine ./asset/bus.jpg
```

### 2. OBB

```bash
./build/yolov11-tensorrt_obb ./weights/best_obb.engine ./asset/boats.jpg
```

### 3. SEG

```bash
./build/yolov11-tensorrt_seg ./weights/yolo11s-seg.engine ./asset/bus.jpg
```

### 4. APP

```bash
./build/yolov11-tensorrt_qt_app ./weights/best_obb.engine ./weights/yolo11s-seg.engine
```

如果启动时传了两个模型路径，程序会在首次显示时自动尝试加载。
如果不在启动参数里传模型，也可以先打开程序，再在界面里的 `工程设置` 手动加载两个 engine。

## 说明

- `APP` 的应用层说明看 `README_APP.md`
- `DET` 的基础入口看 `README.md`
- `OBB` 的部署细节看 `README_OBB.md`
- `SEG` 的部署细节看 `README_SEG.md`
