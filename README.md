# YOLOv11 TensorRT Qt App

这是一个基于 YOLOv11 OBB + SEG 的工业视觉上位机，支持图片/相机检测、联合后处理、结果显示、PLC 联动写入和自动抓取流程。

## 主要功能

- OBB + SEG 联合推理
- Qt 界面显示检测结果
- 右下角显示模式切换：`隐藏 / 全显`
- PLC 联动触发与结果写入
- Windows 打包运行支持

## 当前数据流

```text
图片/相机输入 -> OBB 检测 -> SEG 分割 -> 联合后处理 -> 界面显示 -> PLC 写入
```

## PLC 默认配置

- Host：`192.168.3.205`
- Port：`502`
- 写入寄存器：`D500 / D502 / D504 / D506 / D508`
- 触发位：`D1500`

## 目录说明

- [docs/README_APP.md](docs/README_APP.md)
- [docs/README_WINDOWS.md](docs/README_WINDOWS.md)
- [docs/README_DEPLOY.md](docs/README_DEPLOY.md)
- [docs/README_NEW_MACHINE_WINDOWS.md](docs/README_NEW_MACHINE_WINDOWS.md)
- [docs/README_OBB.md](docs/README_OBB.md)
- [docs/README_SEG.md](docs/README_SEG.md)
- [docs/README_STEP_BY_STEP.md](docs/README_STEP_BY_STEP.md)
- [docs/README_PLC.md](docs/README_PLC.md)

## 构建目标

- `yolov11-tensorrt_obb`
- `yolov11-tensorrt_seg`
- `yolov11-tensorrt_qt_app`

## Windows 快速开始

```powershell
cmake -S . -B build_win_qt -G "Visual Studio 17 2022" -A x64 -DBUILD_QT_APP=ON
cmake --build build_win_qt --config Release --target yolov11-tensorrt_qt_app
powershell -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1
```

## 备注

- 当前代码已统一到 Qt 界面、OBB/SEG 联合推理、PLC 联动流程。
- Windows 构建已关闭静态 CUDA runtime，避免 `LIBCMT` 冲突。
- PLC 通信采用 Modbus TCP。
