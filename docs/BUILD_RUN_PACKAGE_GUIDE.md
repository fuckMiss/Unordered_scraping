# TankEye-Iris 2.1.6 构建运行打包指南

## 适用场景

这个流程适用于你把 `build` 目录删掉之后，重新生成、编译、运行、生成管理员授权文件、打包项目。

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

正式运行必须提供 `config\admin_auth.key` 作为管理员授权密钥。缺少该文件时，管理员授权校验无效，正式打包脚本也会直接报错。

## 4. 生成管理员授权文件

新机器首次创建管理员账号时，现场在软件的管理员授权窗口点击 `保存授权申请`，选择保存位置和文件名。软件会生成类似下面的授权申请文件：

```text
TankEye_admin_license_request_TK-B57A-B3ED-5AA9-3F90.json
```

现场把这个授权申请 JSON 文件发给工程师。工程师在项目根目录执行下面的命令，生成最终授权文件：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\generate_admin_auth_code.ps1 -RequestFile .\TankEye_admin_license_request_TK-B57A-B3ED-5AA9-3F90.json -Output .\config\admin_license.json
```


将生成的 `admin_license.json` 发回现场，放到运行包 `config` 目录。开发环境下也可以放在项目根目录 `config`，程序会在 `build\Release\config` 找不到时自动读取项目根目录 `config`。再次打开管理员或工程设置入口时，会进入创建管理员账号阶段，表单里的授权状态应显示 `已授权`。

创建管理员账号时必须同时设置：

- 管理员账号和密码。
- 恢复问题和恢复答案。

恢复答案会像密码一样隐藏显示，并带有小眼睛按钮。恢复问题用于以后忘记管理员密码时由老板或调试员本地重置，不需要工程师再次生成授权文件。

忘记管理员密码时：

- 点击登录窗口的 `忘记密码`。
- 软件仍会检查本机 `admin_license.json` 是否有效。
- 输入正确的恢复答案后，才能设置新的管理员账号密码。
- `新恢复问题` 和 `新恢复答案` 可以不填；不填时继续沿用原恢复问题和恢复答案。
- 如果要更换恢复问题，必须同时填写 `新恢复问题` 和 `新恢复答案`。
- 如果旧账号没有设置恢复问题，软件会提示联系负责人清除账号后重新初始化。

正式使用时必须保管好：

```text
config\admin_auth.key
```

该文件会被 git 忽略，不上传 GitHub；打包时会复制进运行包。

## 5. 打包运行目录

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\package_runtime.ps1 -BuildDir build -ReleaseName TankEye-Iris_2.1.6 -Force
```

打包完成后，发布目录在：

```text
dist\TankEye-Iris_2.1.6
```

压缩包在：

```text
dist\TankEye-Iris_2.1.6.zip
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

### 管理员授权无效

确认以下几点：

- 没有 `admin_license.json` 时，软件会显示 `无授权文件`。
- 有 `admin_license.json` 但不是本机申请生成、签名不对或硬件指纹不符时，软件会显示 `授权文件不符`。
- 授权申请必须从软件里点击 `保存授权申请` 生成，不要手工拼写 JSON。
- 正式运行包必须把 `admin_license.json` 放在运行包 `config` 目录；开发环境可放在项目根目录 `config` 或 `build\Release\config`。
- 生成 license 的电脑和打包用的是同一个 `config\admin_auth.key`。
