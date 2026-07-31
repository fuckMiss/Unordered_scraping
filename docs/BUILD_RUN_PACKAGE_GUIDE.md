# TankEye-Iris 1.2 构建运行打包指南

## 适用场景

这个流程适用于你把 `build` 目录删掉之后，重新生成、编译、运行、生成管理员授权码、打包项目。

## 当前环境路径

- OpenVINO CMake 配置目录：
  `D:\Anaconda\envs\cll_yolo\Lib\site-packages\openvino\cmake`
- OpenCV CMake 配置目录：
  `D:\work_floder\jiezhifa\TankEye_source_for_new_pc\_deps\opencv\opencv\build\x64\vc16\lib`

## 1. 重新生成 build

在项目根目录执行：

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64 `
  -DOpenVINO_DIR="D:\Anaconda\envs\cll_yolo\Lib\site-packages\openvino\cmake" `
  -DOpenCV_DIR="D:\work_floder\jiezhifa\TankEye_source_for_new_pc\_deps\opencv\opencv\build\x64\vc16\lib"
```

## 2. 编译程序

```powershell
cmake --build build --config Release --target tankeye-openvino_qt_app
cmake --build build --config Release --target tankeye-admin-auth-code
```

编译成功后，主程序在：

```text
build\Release\tankeye-openvino_qt_app.exe
```

## 3. 运行程序

直接启动开发环境下的程序：

```powershell
.\launch_tankeye.ps1 -BuildDir build -Configuration Release -WindowMode Maximized -AutoLoadModels
```

启动脚本会优先读取 `config\admin_auth.key` 作为管理员授权密钥。没有该文件时会回退到开发默认密钥，仅适合本机测试。

## 4. 生成管理员授权码

新机器首次创建管理员账号，用机器码生成 `INIT` 授权码：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\generate_admin_auth_code.ps1 -MachineCode "TK-客户机器码" -Purpose INIT
```

忘记管理员密码时，用同一机器码生成 `RESET` 重置码：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\generate_admin_auth_code.ps1 -MachineCode "TK-客户机器码" -Purpose RESET
```

正式使用时必须保管好：

```text
config\admin_auth.key
```

该文件会被 git 忽略，不上传 GitHub；打包时会复制进运行包。

## 5. 打包运行目录

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\package_runtime.ps1 -BuildDir build -ReleaseName TankEye-Iris_1.2 -Force
```

打包完成后，发布目录在：

```text
dist\TankEye-Iris_1.2
```

压缩包在：

```text
dist\TankEye-Iris_1.2.zip
```

## 6. 常见问题

### build 不存在

先执行第 1 步，不要直接跑 `cmake --build build`。

### OpenVINO 找不到

确认 `-DOpenVINO_DIR` 指向的是 `OpenVINOConfig.cmake` 所在目录，不是 dll 目录。

### OpenCV 找不到

确认 `-DOpenCV_DIR` 指向的是 `OpenCVConfig.cmake` 所在目录。

### 想重新来一遍

可以直接删掉 `build` 目录，然后从第 1 步重新执行。

### 管理员授权码无效

确认三点：

- 机器码必须从软件里复制完整内容。
- 首次创建管理员用 `-Purpose INIT`，忘记密码重置用 `-Purpose RESET`。
- 生成码的电脑和打包用的是同一个 `config\admin_auth.key`。
