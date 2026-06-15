# TankEye1.1 项目迁移说明

本文档说明如何把本项目复制到其他机器，并做成双击图标启动。环境安装不在本文档展开，默认目标机器已经装好 CUDA、TensorRT、OpenCV、Qt 等运行依赖。

## 1. 选择迁移包

推荐使用带模型包：

```bash
yolov11-test-main-with-weights-20260602.tar.gz
```

这个包包含：

- C++ / CUDA / Qt 源码
- README 文档
- 固定桌面图标资源 `qt/assets/app_icon.png`
- `weights/best_obb.engine`
- `weights/best_seg.engine`
- 启动脚本 `launch_tankeye.sh`
- 桌面/应用菜单安装脚本 `install_desktop_launcher.sh`

源码包：

```bash
yolov11-test-main-source-20260602.tar.gz
```

源码包不包含 `weights/`，迁移后需要自己放入 engine 模型。

## 2. 解压项目

在目标机器选择一个固定目录，例如：

```bash
mkdir -p ~/workdir
tar -xzf yolov11-test-main-with-weights-20260602.tar.gz -C ~/workdir
```

如果解压后目录名不是 `yolov11-test-main`，可以自己重命名：

```bash
mv ~/workdir/<解压出来的目录> ~/workdir/yolov11-test-main
cd ~/workdir/yolov11-test-main
```

## 3. 确认模型文件

默认启动脚本会加载下面两个模型：

```text
weights/best_obb.engine
weights/best_seg.engine
```

如果目标机器上模型文件名不一样，需要修改：

```text
launch_tankeye.sh
```

里面这两行：

```bash
OBB_ENGINE="${APP_DIR}/weights/best_obb.engine"
SEG_ENGINE="${APP_DIR}/weights/best_seg.engine"
```

## 4. 编译程序

进入项目目录：

```bash
cd ~/workdir/yolov11-test-main
```

编译 Qt 应用：

```bash
cmake -S . -B build
cmake --build build --target yolov11-tensorrt_qt_app -j4
```

如果也要单独测试 OBB / SEG 命令行程序，可以编译：

```bash
cmake --build build --target yolov11-tensorrt_obb -j4
cmake --build build --target yolov11-tensorrt_seg -j4
```

## 5. 命令行测试一次

先确认程序能正常启动：

```bash
./launch_tankeye.sh
```

如果能打开 Qt 界面，说明模型路径和运行库路径基本正常。

## 6. 安装桌面图标

执行：

```bash
./install_desktop_launcher.sh
```

它会生成两个启动入口：

```text
桌面图标：~/桌面/TankEye.desktop
应用菜单：~/.local/share/applications/tankeye.desktop
```

以后可以用两种方式启动：

- 双击桌面上的 `TankEye.desktop`
- 按 `Super` 键，搜索 `TankEye1.1`，从应用菜单启动

如果希望图标更干净，推荐使用应用菜单入口，再把它固定到 Dock。

### 桌面图标第一次启动

Linux 桌面对 `.desktop` 启动器有安全限制。第一次安装后，如果系统提示“不信任的启动器”，右键图标，选择：


```text
允许启动
```

或：

```text
Allow Launching
```

如果双击后打开的是一段 `[Desktop Entry]` 文本，例如：

```text
[Desktop Entry]
Type=Application
Name=TankEye1.1
...
```

说明系统还没有把它当成启动器，而是把它当普通文本打开了。处理方式：

1. 右键桌面上的 `TankEye.desktop`
2. 选择 `属性`
3. 进入 `权限`
4. 勾选 `允许作为程序执行文件`
5. 关闭属性窗口
6. 再右键图标，选择 `允许启动` 或 `Allow Launching`
7. 再双击启动

### 图标没有刷新

固定图标文件是：

```text
qt/assets/app_icon.png
```

如果桌面图标还没有刷新成固定图标，通常是桌面缓存导致，不影响程序启动。可以按下面顺序处理：

1. 右键桌面图标，选择 `允许启动`
2. 删除桌面上的 `TankEye.desktop`
3. 回到项目目录重新执行：

```bash
./install_desktop_launcher.sh
```

4. 如果仍然不刷新，注销重新登录一次

通常不需要重启电脑。

### 图标上有小链接标记

这个小标记不是图标图片自带的，而是桌面环境给 `.desktop` 快捷方式加的标记。它可能表示：

- 这个启动器还没有被 `允许启动`
- 桌面环境把 `.desktop` 文件显示成快捷方式

如果右键 `允许启动` 后仍然有小标记，这是桌面环境的显示规则，不影响程序运行。

想要更干净、没有桌面快捷方式小标记的图标，推荐从系统应用菜单启动：

1. 按 `Super` 键
2. 搜索 `TankEye1.1`
3. 点击启动
4. 也可以右键固定到 Dock

### 不想处理桌面图标时

可以直接命令行启动：

```bash
cd ~/workdir/yolov11-test-main
./launch_tankeye.sh
```

## 7. 启动逻辑说明

桌面图标实际调用的是：

```text
launch_tankeye.sh
```

启动脚本会自动：

- 找到当前项目目录
- 设置 TensorRT / OpenCV 动态库路径
- 检查 Qt 可执行文件是否存在
- 检查 OBB engine 是否存在
- 检查 SEG engine 是否存在
- 启动 `build/yolov11-tensorrt_qt_app`

所以项目可以放在不同目录，不需要手动改桌面图标路径。每台机器只要解压、编译、执行一次 `install_desktop_launcher.sh` 即可。

## 8. 常见问题

### 双击没反应

先在项目目录执行：

```bash
./launch_tankeye.sh
```

再看日志：

```bash
cat logs/tankeye-launch.log
```

如果没有日志，说明桌面启动器可能还没有真正执行。先按第 6 节处理 `允许启动`。

### 双击打开文本内容

这是 `.desktop` 还没有被系统允许执行。按第 6 节里的“桌面图标第一次启动”处理。

### 桌面图标有小链接标记

这是桌面环境对快捷方式的显示标记，不是图标文件的问题。推荐从应用菜单搜索 `TankEye1.1` 启动，或固定到 Dock。

### 提示找不到可执行文件

说明还没有编译，执行：

```bash
cmake -S . -B build
cmake --build build --target yolov11-tensorrt_qt_app -j4
```

### 提示找不到 engine

确认下面两个文件存在：

```text
weights/best_obb.engine
weights/best_seg.engine
```

如果文件名不同，就修改 `launch_tankeye.sh`。

### engine 在另一台机器上不能用

TensorRT engine 和 GPU、TensorRT 版本、CUDA 环境有关。不同机器如果 engine 无法加载，需要在目标机器上重新由 ONNX 生成 engine。

## 9. 推荐迁移流程

最稳妥的流程：

```bash
tar -xzf yolov11-test-main-with-weights-20260602.tar.gz -C ~/workdir
cd ~/workdir/yolov11-test-main
cmake -S . -B build
cmake --build build --target yolov11-tensorrt_qt_app -j4
./launch_tankeye.sh
./install_desktop_launcher.sh
```

之后就可以从桌面图标启动。
