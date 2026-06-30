# SEG 推理说明

只说明 SEG 推理目标的构建和运行。

## 构建

```bash
cmake -S . -B build
cmake --build build --target yolov11-tensorrt_seg -j4
```

## 运行

```bash
./build/yolov11-tensorrt_seg ./weights/best_seg.engine ./asset/20260331_090339_757.jpg
```

也可以输入视频或文件夹。

## 关键参数

- `--num-classes`
- `--conf`
- `--nms`
- `--mask-thres`
- `--alpha`
- `--labels`
- `--fp16`
- `--no-warmup`

## 类别名

类别名优先级：

1. 命令行 `--labels`
2. `src/YOLOv11_SEG.cpp` 里的默认数组
3. 自动回退到 `class_0 / class_1 / ...`

## 备注

- SEG 模型必须用 `yolov11-tensorrt_seg`
- engine 和 GPU/TensorRT/CUDA 绑定，换机器通常要重做

