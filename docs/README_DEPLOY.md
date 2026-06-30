# 部署说明

这份文档说明当前项目如何从源码到可运行包。

## 最小流程

```text
拉代码 -> 安装依赖 -> 放入 engine -> 编译 Qt 程序 -> 打包 DLL -> 启动
```

## Windows 部署

1. 配好 CMake、Visual Studio、CUDA、TensorRT、OpenCV、Qt。
2. 生成 `weights/best_obb.engine` 和 `weights/best_seg.engine`。
3. 编译 `yolov11-tensorrt_qt_app`。
4. 运行 `deploy_windows.ps1`。
5. 运行 `launch_tankeye.ps1`。

## 运行时文件

`deploy_windows.ps1` 会把这些东西放到 Qt 程序旁边：

- Qt 运行库
- OpenCV DLL
- TensorRT DLL
- CUDA DLL
- `qt/assets/app_logo_cutout.png`

## 启动脚本

- `launch_tankeye.sh`：Linux 启动脚本
- `launch_tankeye.ps1`：Windows 启动脚本

## 常见问题

- 找不到可执行文件：先编译 `yolov11-tensorrt_qt_app`
- 找不到 engine：检查 `weights/best_obb.engine` 和 `weights/best_seg.engine`
- 打包后无法启动：多半是路径或运行时 DLL 缺失

