# YOLOv11 TensorRT

这是一个纯部署层的 YOLOv11 TensorRT C++ 推理项目，当前保留 3 个独立入口：

- `DET`：标准目标检测
- `OBB`：旋转框检测
- `SEG`：实例分割

## 文档导航

- `README.md`
  项目总览与构建入口
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

## 项目结构

```text
yolov11-tensorrt/
├── asset/
├── build/
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

## 说明

- `DET` 的基础入口看 `README.md`
- `OBB` 的部署细节看 `README_OBB.md`
- `SEG` 的部署细节看 `README_SEG.md`
