# TankEye1.1 运行版安装说明

这是 TankEye1.1 的运行版交付包，只包含运行程序和模型，不包含 C++ 源码。

## 包内内容

```text
build/yolov11-tensorrt_qt_app   # Qt 主程序，已去除调试符号
weights/best_obb.engine         # OBB TensorRT 模型
weights/best_seg.engine         # SEG TensorRT 模型
qt/assets/app_icon.png          # 固定应用图标
launch_tankeye.sh               # 启动脚本
install_desktop_launcher.sh     # 桌面/应用菜单安装脚本
install_runtime.sh              # 安装脚本，将运行版复制到固定安装目录
README_RUNTIME.md               # 本说明
```

运行版包不会包含：

```text
src/
qt/*.cpp
qt/*.h
main*.cpp
CMakeLists.txt
export.py
推理.py
```

## 运行环境

目标机器需要提前安装好项目运行环境，包括：

- NVIDIA 驱动
- CUDA
- TensorRT
- OpenCV
- Qt5

当前启动脚本默认会查找：

```text
/opt/TensorRT-8.6.1.6/lib
/usr/local/cuda-11.6/lib64
/home/cll/下载/opencv/build/lib
```

如果目标机器上的 TensorRT、CUDA、OpenCV 路径不同，需要修改：

```text
launch_tankeye.sh
```

里面的：

```bash
LD_LIBRARY_PATH
```

## 安装步骤

解压运行版包：

```bash
tar -xzf TankEye1.1-runtime-20260602.tar.gz -C ~/workdir
cd ~/workdir/TankEye1.1-runtime
```

安装到当前用户目录：

```bash
./install_runtime.sh
```

默认会安装到：

```text
~/TankEye1.1
```

如果想指定安装目录：

```bash
./install_runtime.sh /home/cll/TankEye1.1
```

也可以用环境变量：

```bash
TANKEYE_INSTALL_DIR=/home/cll/TankEye1.1 ./install_runtime.sh
```

安装脚本会自动：

- 复制运行程序、模型和图标到安装目录
- 给启动脚本加执行权限
- 生成桌面图标
- 生成应用菜单入口

安装完成后，先命令行测试一次：

```bash
~/TankEye1.1/launch_tankeye.sh
```

如果你不想安装，只想在解压目录临时运行，也可以直接执行：

```bash
./launch_tankeye.sh
```

安装后可以通过两种方式启动：

- 桌面双击 `TankEye.desktop`
- 按 `Super` 键，搜索 `TankEye1.1`

## 修改默认模型

默认模型在：

```text
launch_tankeye.sh
```

修改这两行：

```bash
OBB_ENGINE="${APP_DIR}/weights/best_obb.engine"
SEG_ENGINE="${APP_DIR}/weights/best_seg.engine"
```

如果换模型，只需要把新的 `.engine` 放进 `weights/`，然后改这两个路径。

## 常见问题

### 双击打开的是文本

右键桌面图标，进入 `属性 -> 权限`，勾选 `允许作为程序执行文件`。然后右键图标，选择 `允许启动`。

### 图标上有小链接标记

这是 Linux 桌面环境对 `.desktop` 快捷方式的显示标记，不影响程序运行。想要更干净的图标，可以从应用菜单搜索 `TankEye1.1` 启动并固定到 Dock。

### 提示找不到 engine

确认下面文件存在：

```text
weights/best_obb.engine
weights/best_seg.engine
```

### 提示找不到动态库

检查 `launch_tankeye.sh` 里的 `LD_LIBRARY_PATH`，确认 TensorRT、CUDA、OpenCV 路径和目标机器一致。

### engine 在另一台机器上不能用

TensorRT engine 与 GPU、CUDA、TensorRT 版本有关。如果目标机器加载失败，需要在目标机器上重新生成 engine。
