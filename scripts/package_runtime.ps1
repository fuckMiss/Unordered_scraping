param(
    [string]$BuildDir = "build",
    [string]$ReleaseName = "TankEye-Iris_1.2",
    [switch]$Force
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$AppDir = Split-Path -Parent $ScriptDir
$BuildPath = Join-Path $AppDir $BuildDir
$DistDir = Join-Path $AppDir "dist"
$ReleaseDir = Join-Path $DistDir $ReleaseName
$ZipPath = Join-Path $DistDir "$ReleaseName.zip"
$StagingDir = Join-Path $DistDir "_staging_$ReleaseName"

function Require-File($Path, $Name) {
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "$Name not found: $Path"
    }
}

function Copy-File($Source, $DestinationDir) {
    Require-File $Source "Required file"
    New-Item -ItemType Directory -Force -Path $DestinationDir | Out-Null
    Copy-Item -LiteralPath $Source -Destination $DestinationDir -Force
}

function Copy-FileIfExists($Source, $DestinationDir) {
    if (Test-Path -LiteralPath $Source) {
        New-Item -ItemType Directory -Force -Path $DestinationDir | Out-Null
        Copy-Item -LiteralPath $Source -Destination $DestinationDir -Force
    }
}

function Copy-DirectoryIfExists($Source, $Destination) {
    if (Test-Path -LiteralPath $Source) {
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Destination) | Out-Null
        Copy-Item -LiteralPath $Source -Destination $Destination -Recurse -Force
    }
}

function Remove-IfExists($Path) {
    if (Test-Path -LiteralPath $Path) {
        Remove-Item -LiteralPath $Path -Recurse -Force
    }
}

function Write-Utf8File($Path, $Content) {
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Path) | Out-Null
    [System.IO.File]::WriteAllText($Path, $Content, [System.Text.UTF8Encoding]::new($false))
}

function Add-HashEntry($Root, $File) {
    $relative = $File.FullName.Substring($Root.Length + 1).Replace("\", "/")
    $hash = (Get-FileHash -LiteralPath $File.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    [PSCustomObject]@{
        path = $relative
        bytes = $File.Length
        sha256 = $hash
    }
}

$BuildOutputCandidates = @(
    $BuildPath,
    (Join-Path $BuildPath "Release"),
    (Join-Path $BuildPath "RelWithDebInfo"),
    (Join-Path $BuildPath "MinSizeRel")
)
$BuildOutputPath = $BuildOutputCandidates |
    Where-Object { Test-Path -LiteralPath (Join-Path $_ "tankeye-openvino_qt_app.exe") } |
    Select-Object -First 1
if ([string]::IsNullOrWhiteSpace($BuildOutputPath)) {
    $CheckedPaths = ($BuildOutputCandidates | ForEach-Object {
        Join-Path $_ "tankeye-openvino_qt_app.exe"
    }) -join "; "
    throw "Qt app executable not found. Checked: $CheckedPaths"
}
Write-Host "[Package] Build output: $BuildOutputPath"
Require-File (Join-Path $BuildOutputPath "tankeye-openvino_qt_app.exe") "Qt app executable"
Require-File (Join-Path $AppDir "models\weights\best_obb.xml") "OBB model XML"
Require-File (Join-Path $AppDir "models\weights\best_obb.bin") "OBB model BIN"
Require-File (Join-Path $AppDir "models\weights\best_seg.xml") "SEG model XML"
Require-File (Join-Path $AppDir "models\weights\best_seg.bin") "SEG model BIN"

if ((Test-Path -LiteralPath $ReleaseDir) -and -not $Force) {
    throw "Release directory already exists: $ReleaseDir. Re-run with -Force to replace it."
}
if ((Test-Path -LiteralPath $ZipPath) -and -not $Force) {
    throw "Release zip already exists: $ZipPath. Re-run with -Force to replace it."
}

Remove-IfExists $StagingDir
if ($Force) {
    Remove-IfExists $ReleaseDir
    if (Test-Path -LiteralPath $ZipPath) {
        Remove-Item -LiteralPath $ZipPath -Force
    }
}
New-Item -ItemType Directory -Force -Path $StagingDir | Out-Null

Write-Host "[Package] Staging: $StagingDir"

$QtRoot = if ($env:Qt5_DIR) {
    Resolve-Path (Join-Path $env:Qt5_DIR "..\..\..")
} else {
    "D:\Qt\5.15.2\msvc2019_64"
}
$QtBin = Join-Path $QtRoot "bin"
$WinDeployQt = Join-Path $QtBin "windeployqt.exe"
Require-File $WinDeployQt "windeployqt"
$VcRuntimeDirCandidates = @(
    "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Redist\MSVC\14.50.35710\x64\Microsoft.VC145.CRT",
    "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Redist\MSVC\v145\x64\Microsoft.VC145.CRT"
)
if ($env:VCToolsRedistDir) {
    $VcRuntimeDirCandidates = @(
        (Join-Path $env:VCToolsRedistDir "x64\Microsoft.VC143.CRT")
    ) + $VcRuntimeDirCandidates
}
$VcRuntimeDir = $VcRuntimeDirCandidates | Where-Object {
    -not [string]::IsNullOrWhiteSpace($_) -and (Test-Path -LiteralPath $_)
} | Select-Object -First 1
if ([string]::IsNullOrWhiteSpace($VcRuntimeDir)) {
    throw "Visual C++ x64 runtime directory not found."
}

$OpenCvRoot = if ($env:OpenCV_DIR) { $env:OpenCV_DIR } else { Join-Path $AppDir "_deps\opencv\opencv\build" }
$OpenCvBinCandidates = @(
    (Join-Path $OpenCvRoot "x64\vc16\bin"),
    (Join-Path $OpenCvRoot "bin\Release"),
    (Join-Path $OpenCvRoot "bin")
)
$OpenCvBin = $OpenCvBinCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if ([string]::IsNullOrWhiteSpace($OpenCvBin)) {
    throw "OpenCV runtime directory not found."
}

$OpenVinoRoots = New-Object System.Collections.Generic.List[string]
if ($env:CONDA_PREFIX) {
    $OpenVinoRoots.Add((Join-Path $env:CONDA_PREFIX "Lib\site-packages\openvino"))
}
if ($env:openvino_DIR) {
    $OpenVinoRoots.Add((Split-Path -Parent $env:openvino_DIR))
}
if ($env:OpenVINO_DIR) {
    $OpenVinoRoots.Add((Split-Path -Parent $env:OpenVINO_DIR))
}
if ($env:OPENVINO_ROOT) {
    $OpenVinoRoots.Add($env:OPENVINO_ROOT)
}
$OpenVinoRoots.Add("D:\Anaconda\envs\cll_yolo\Lib\site-packages\openvino")
$OpenVinoRoots.Add("D:\Anaconda\envs\python_wx\Lib\site-packages\openvino")
$OpenVinoRoots.Add("D:\Anaconda3\envs\cll_yolo\Lib\site-packages\openvino")
$OpenVinoRoots.Add("C:\Program Files (x86)\Intel\openvino")

$OpenVinoBinCandidates = New-Object System.Collections.Generic.List[string]
foreach ($Root in $OpenVinoRoots) {
    if ([string]::IsNullOrWhiteSpace($Root)) {
        continue
    }
    $OpenVinoBinCandidates.Add((Join-Path $Root "libs"))
    $OpenVinoBinCandidates.Add($Root)
    $OpenVinoBinCandidates.Add((Join-Path $Root "runtime\bin\intel64\Release"))
    $OpenVinoBinCandidates.Add((Join-Path $Root "runtime\3rdparty\tbb\bin"))
}
$OpenVinoBin = $OpenVinoBinCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if ([string]::IsNullOrWhiteSpace($OpenVinoBin)) {
    throw "OpenVINO runtime directory not found."
}

$env:Path = (($QtBin, $OpenCvBin, $OpenVinoBin, $env:Path) -join ";")
Copy-File (Join-Path $BuildOutputPath "tankeye-openvino_qt_app.exe") $StagingDir
& $WinDeployQt --release --compiler-runtime --no-translations (Join-Path $StagingDir "tankeye-openvino_qt_app.exe")
if ($LASTEXITCODE -ne 0) {
    throw "windeployqt failed with exit code $LASTEXITCODE"
}

Get-ChildItem -LiteralPath $OpenCvBin -File -Filter "opencv*.dll" | Where-Object {
    $_.Name -notmatch 'd\.dll$'
} | ForEach-Object {
    Copy-File $_.FullName $StagingDir
}
Get-ChildItem -LiteralPath $OpenVinoBin -File -Filter "*.dll" | ForEach-Object {
    Copy-File $_.FullName $StagingDir
}
Get-ChildItem -LiteralPath $VcRuntimeDir -File -Filter "*.dll" | ForEach-Object {
    Copy-File $_.FullName $StagingDir
}

Get-ChildItem -LiteralPath $BuildOutputPath -File | Where-Object {
    $_.Extension -ieq ".dll" -or $_.Extension -ieq ".cti" -or $_.Extension -ieq ".ax" -or $_.Name -ieq "CommonParameters.ini"
} | ForEach-Object {
    Copy-File $_.FullName $StagingDir
}

foreach ($DirName in @("bearer", "iconengines", "imageformats", "platforms", "styles", "ThirdParty")) {
    Copy-DirectoryIfExists (Join-Path $BuildOutputPath $DirName) (Join-Path $StagingDir $DirName)
}

foreach ($AssetName in @("app_logo_cutout.png", "app_icon.ico")) {
    Copy-FileIfExists (Join-Path $AppDir "app\qt\assets\$AssetName") (Join-Path $StagingDir "app\qt\assets")
    Copy-FileIfExists (Join-Path $AppDir "app\qt\assets\$AssetName") (Join-Path $StagingDir "qt\assets")
}

$WeightsDir = Join-Path $StagingDir "models\weights"
Copy-File (Join-Path $AppDir "models\weights\best_obb.xml") $WeightsDir
Copy-File (Join-Path $AppDir "models\weights\best_obb.bin") $WeightsDir
Copy-File (Join-Path $AppDir "models\weights\best_seg.xml") $WeightsDir
Copy-File (Join-Path $AppDir "models\weights\best_seg.bin") $WeightsDir

$ConfigSource = Join-Path $BuildOutputPath "config\tankeye.json"
if (-not (Test-Path -LiteralPath $ConfigSource)) {
    $ConfigSource = Join-Path $BuildPath "config\tankeye.json"
}
if (-not (Test-Path -LiteralPath $ConfigSource)) {
    $ConfigSource = Join-Path $AppDir "config\tankeye.json"
}
Copy-File $ConfigSource (Join-Path $StagingDir "config")

function Test-ValidCoordinateProfile($ProfilePath) {
    try {
        $profile = Get-Content -LiteralPath $ProfilePath -Raw | ConvertFrom-Json
        if (-not $profile.enabled) {
            return $false
        }
        $valid_points = @($profile.points | Where-Object {
            $null -ne $_.image_x -and $null -ne $_.image_y -and
            $null -ne $_.machine_x -and $null -ne $_.machine_y -and
            [double]::IsFinite([double]$_.image_x) -and [double]::IsFinite([double]$_.image_y) -and
            [double]::IsFinite([double]$_.machine_x) -and [double]::IsFinite([double]$_.machine_y) -and
            (([double]$_.image_x -ne 0.0) -or ([double]$_.image_y -ne 0.0) -or
             ([double]$_.machine_x -ne 0.0) -or ([double]$_.machine_y -ne 0.0))
        })
        return $valid_points.Count -ge 4
    } catch {
        return $false
    }
}

Copy-DirectoryIfExists (Join-Path $BuildOutputPath "calibration_profiles") (Join-Path $StagingDir "calibration_profiles")
if (-not (Test-Path -LiteralPath (Join-Path $StagingDir "calibration_profiles"))) {
    Copy-DirectoryIfExists (Join-Path $BuildPath "calibration_profiles") (Join-Path $StagingDir "calibration_profiles")
}
Copy-DirectoryIfExists (Join-Path $AppDir "calibration_output") (Join-Path $StagingDir "calibration_output")

$PackagedConfigPath = Join-Path $StagingDir "config\tankeye.json"
$PackagedProfileDir = Join-Path $StagingDir "calibration_profiles"
$HasValidCoordinateProfile = $false
if (Test-Path -LiteralPath $PackagedProfileDir) {
    $HasValidCoordinateProfile = [bool](Get-ChildItem -LiteralPath $PackagedProfileDir -File -Filter "*.json" |
        Where-Object { Test-ValidCoordinateProfile $_.FullName } |
        Select-Object -First 1)
}
if (-not $HasValidCoordinateProfile) {
    try {
        $config_json = Get-Content -LiteralPath $PackagedConfigPath -Raw | ConvertFrom-Json
        if ($null -ne $config_json.machine_limits) {
            $config_json.machine_limits.enabled = $false
            $config_json | ConvertTo-Json -Depth 20 | Set-Content -LiteralPath $PackagedConfigPath -Encoding utf8
            Write-Host "[Package] No valid enabled nine-point profile found; packaged machine_limits.enabled=false."
        }
    } catch {
        Write-Warning "Failed to adjust packaged machine_limits: $($_.Exception.Message)"
    }
}

Copy-FileIfExists (Join-Path $AppDir "samples\images\2.jpg") (Join-Path $StagingDir "samples\images")
Copy-File (Join-Path $AppDir "runtime\USAGE_GUIDE.txt") $StagingDir

$PickerCandidates = @(
    (Join-Path $BuildOutputPath "nine_point_circle_picker.exe"),
    (Join-Path $BuildPath "nine_point_circle_picker.exe"),
    (Join-Path $AppDir "dist\TankEye-Iris_1.0\nine_point_circle_picker.exe")
)
$Picker = $PickerCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if ($Picker) {
    Copy-File $Picker $StagingDir
}

$Launcher = @'
param(
    [ValidateSet("AUTO", "GPU", "CPU")]
    [string]$Device = "AUTO",
    [ValidateSet("Normal", "Maximized", "Fullscreen")]
    [string]$WindowMode = "Maximized",
    [int]$WindowWidth = 1280,
    [int]$WindowHeight = 720,
    [double]$UiScale = 0,
    [string]$PlcHost = "",
    [int]$PlcPort = 0,
    [switch]$SimulatePlc,
    [switch]$DebugPostprocess,
    [switch]$ClearOpenVinoCache,
    [string]$CameraIp = "192.168.0.233"
)

$ErrorActionPreference = "Stop"

$AppDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$AppExe = Join-Path $AppDir "tankeye-openvino_qt_app.exe"
$ObbModel = Join-Path $AppDir "models\weights\best_obb.xml"
$SegModel = Join-Path $AppDir "models\weights\best_seg.xml"
$LogDir = Join-Path $AppDir "logs"
$LogStamp = Get-Date -Format "yyyyMMdd_HHmmss"
$LogFile = Join-Path $LogDir "tankeye_$LogStamp.log"
$OpenVinoCacheDir = Join-Path $AppDir "openvino_cache"

foreach ($Required in @($AppExe, $ObbModel, $SegModel)) {
    if (-not (Test-Path -LiteralPath $Required)) {
        throw "Required runtime file not found: $Required"
    }
}

New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
if ($ClearOpenVinoCache) {
    Remove-Item -LiteralPath $OpenVinoCacheDir -Recurse -Force -ErrorAction SilentlyContinue
}
New-Item -ItemType Directory -Force -Path $OpenVinoCacheDir | Out-Null

try {
    $Desktop = [Environment]::GetFolderPath("Desktop")
    $ShortcutPath = Join-Path $Desktop "TankEye-Iris.lnk"
    if (-not (Test-Path -LiteralPath $ShortcutPath)) {
        $WshShell = New-Object -ComObject WScript.Shell
        $Shortcut = $WshShell.CreateShortcut($ShortcutPath)
        $Shortcut.TargetPath = "$env:SystemRoot\System32\wscript.exe"
        $Shortcut.Arguments = "`"$(Join-Path $AppDir 'launch_tankeye_main_only.vbs')`""
        $Shortcut.WorkingDirectory = $AppDir
        $IconPath = Join-Path $AppDir "app\qt\assets\app_icon.ico"
        if (Test-Path -LiteralPath $IconPath) {
            $Shortcut.IconLocation = $IconPath
        }
        $Shortcut.Save()
    }
} catch {
    Write-Warning "Desktop shortcut was not created: $($_.Exception.Message)"
}

$RuntimePaths = @(
    $AppDir,
    (Join-Path $AppDir "ThirdParty"),
    (Join-Path $AppDir "platforms"),
    (Join-Path $AppDir "imageformats"),
    (Join-Path $AppDir "iconengines"),
    (Join-Path $AppDir "styles"),
    (Join-Path $AppDir "bearer")
) | Where-Object { Test-Path -LiteralPath $_ }
$env:Path = (($RuntimePaths + @($env:Path)) -join ";")

$env:TANKEYE_OPENVINO_CACHE_DIR = $OpenVinoCacheDir
$env:TANKEYE_CONFIG_PATH = Join-Path $AppDir "config\tankeye.json"
$env:TANKEYE_WINDOW_MODE = $WindowMode
$env:TANKEYE_WINDOW_WIDTH = [string]$WindowWidth
$env:TANKEYE_WINDOW_HEIGHT = [string]$WindowHeight

if ($PlcHost) {
    $env:TANKEYE_PLC_HOST = $PlcHost
} else {
    Remove-Item Env:\TANKEYE_PLC_HOST -ErrorAction SilentlyContinue
}
if ($PlcPort -gt 0) {
    $env:TANKEYE_PLC_PORT = [string]$PlcPort
} else {
    Remove-Item Env:\TANKEYE_PLC_PORT -ErrorAction SilentlyContinue
}
if ($SimulatePlc) {
    $env:TANKEYE_PLC_SIM = "1"
} else {
    Remove-Item Env:\TANKEYE_PLC_SIM -ErrorAction SilentlyContinue
}
if ($DebugPostprocess) {
    $env:TANKEYE_DEBUG_POSTPROCESS = "1"
} else {
    Remove-Item Env:\TANKEYE_DEBUG_POSTPROCESS -ErrorAction SilentlyContinue
}
if ($CameraIp) {
    $env:TANKEYE_CAMERA_IP = $CameraIp
}
if ($UiScale -gt 0) {
    $env:TANKEYE_UI_SCALE = [string]$UiScale
    $env:QT_SCALE_FACTOR = [string]$UiScale
} else {
    Remove-Item Env:\TANKEYE_UI_SCALE -ErrorAction SilentlyContinue
}

function Invoke-TankEyeApp {
    param(
        [string]$SelectedDevice,
        [string]$SelectedLogFile,
        [int]$StartupGuardSeconds = 60
    )

    $env:TANKEYE_OPENVINO_DEVICE = $SelectedDevice
    $env:TANKEYE_LOG_FILE = $SelectedLogFile

    Write-Host "[TankEye] App: $AppExe"
    Write-Host "[TankEye] OBB: $ObbModel"
    Write-Host "[TankEye] SEG: $SegModel"
    Write-Host "[TankEye] Device: $SelectedDevice"
    Write-Host "[TankEye] PLC: $(if ($SimulatePlc) { 'SIMULATED' } elseif ($PlcHost) { "$PlcHost`:$PlcPort" } else { 'config default' })"
    Write-Host "[TankEye] Log: $SelectedLogFile"
    Write-Host "[TankEye] OpenVINO cache: $OpenVinoCacheDir"

    $Process = Start-Process -FilePath $AppExe -ArgumentList @($ObbModel, $SegModel) -WorkingDirectory $AppDir -PassThru
    if ($StartupGuardSeconds -gt 0) {
        $ExitedDuringStartup = $Process.WaitForExit($StartupGuardSeconds * 1000)
        if ($ExitedDuringStartup) {
            Write-Host "[TankEye] App exited with code $($Process.ExitCode)"
            return $Process.ExitCode
        }
    }

    $Process.WaitForExit()
    Write-Host "[TankEye] App exited with code $($Process.ExitCode)"
    return $Process.ExitCode
}

$RequestedDevice = $Device.ToUpperInvariant()
exit (Invoke-TankEyeApp -SelectedDevice $RequestedDevice -SelectedLogFile $LogFile -StartupGuardSeconds 0)
'@
Write-Utf8File (Join-Path $StagingDir "launch_tankeye.ps1") $Launcher

$LauncherVbs = @'
Option Explicit

Dim fso, shell, appDir, launchScript, command

Set fso = CreateObject("Scripting.FileSystemObject")
Set shell = CreateObject("WScript.Shell")

appDir = fso.GetParentFolderName(WScript.ScriptFullName)
launchScript = fso.BuildPath(appDir, "launch_tankeye.ps1")

command = "powershell.exe -NoProfile -ExecutionPolicy Bypass -File " & _
          """" & launchScript & """ -Device AUTO -WindowMode Maximized"

shell.CurrentDirectory = appDir
shell.Run command, 0, False
'@
Write-Utf8File (Join-Path $StagingDir "launch_tankeye_main_only.vbs") $LauncherVbs

$ShortcutScript = @'
$ErrorActionPreference = "Stop"
$AppDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$Desktop = [Environment]::GetFolderPath("Desktop")
$ShortcutPath = Join-Path $Desktop "TankEye-Iris.lnk"
$WshShell = New-Object -ComObject WScript.Shell
$Shortcut = $WshShell.CreateShortcut($ShortcutPath)
$Shortcut.TargetPath = "powershell.exe"
$Shortcut.Arguments = "-NoProfile -ExecutionPolicy Bypass -File `"$AppDir\launch_tankeye.ps1`" -Device AUTO -WindowMode Maximized"
$Shortcut.WorkingDirectory = $AppDir
$IconPath = Join-Path $AppDir "app\qt\assets\app_icon.ico"
if (Test-Path -LiteralPath $IconPath) {
    $Shortcut.IconLocation = $IconPath
}
$Shortcut.Save()
Write-Host "Desktop shortcut created: $ShortcutPath"
'@
Write-Utf8File (Join-Path $StagingDir "create_desktop_shortcut.ps1") $ShortcutScript

$Readme = @'
# TankEye-Iris 1.2 独立运行包

## 启动

双击 `launch_tankeye_main_only.vbs`，或在 PowerShell 中运行：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1
```

测试 PLC 模拟和 CPU 推理：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1 -Device CPU -SimulatePlc
```

## 包内内容

- 主程序：`tankeye-openvino_qt_app.exe`
- 模型：`models\weights\best_obb.*`、`models\weights\best_seg.*`
- 配置：`config\tankeye.json`
- 标定：`calibration_profiles\`、`calibration_output\`
- 演示图：`samples\images\2.jpg`
- 运行库：Qt、OpenCV、OpenVINO、TBB、Hikrobot MVS Runtime
- 工具：`nine_point_circle_picker.exe`、`create_desktop_shortcut.ps1`
- 缓存：`openvino_cache\`
- 日志：`logs\tankeye_*.log`

## 新电脑前置条件

- Windows 64 位。
- CPU 推理可直接运行。
- GPU 推理需要目标电脑安装官方 Intel 显卡驱动。
- 包内带 Hikrobot SDK 运行库；首次接入海康相机前，目标电脑仍需安装官方 MVS 相机驱动。

## 不包含源码

此运行包不包含 C/C++ 源码、头文件、Python 脚本、CMake 工程、测试、调试符号、`.lib` 或训练资产。

## 1.2 说明

- 主界面左侧图像区约 75%，右侧控制栏约 25%。
- 图像完整显示，允许边缘留白，不裁剪。
- 图片读取使用后台线程。
- OpenVINO 缓存默认保留；如需清理，启动时显式添加 `-ClearOpenVinoCache`。
- 运行日志默认带时间戳，可在程序内打开“运行日志”查看。
'@
Write-Utf8File (Join-Path $StagingDir "README_RUNTIME.md") $Readme

$UsageGuidePath = Join-Path $StagingDir "USAGE_GUIDE.txt"
Require-File $UsageGuidePath "Usage guide"

$Notices = @'
# Third-party Runtime Notices

This package bundles runtime components required to run TankEye-Iris:

- Qt 5 runtime libraries and plugins
- OpenCV runtime libraries
- OpenVINO runtime libraries and plugins
- Intel TBB runtime libraries
- Hikrobot MVS runtime libraries and GenTL producers
- Microsoft Visual C++ runtime components copied by the deployment toolchain

These files are redistributed only as runtime dependencies for this application. Install official hardware drivers on target machines where required.
'@
Write-Utf8File (Join-Path $StagingDir "THIRD_PARTY_NOTICES.md") $Notices

$ForbiddenPatterns = @("*.cpp", "*.c", "*.h", "*.hpp", "*.py", "*.cmake", "CMakeLists.txt", "*.sln", "*.vcxproj", "*.lib", "*.pdb", "*.ilk", "*.pt")
$Forbidden = foreach ($Pattern in $ForbiddenPatterns) {
    Get-ChildItem -LiteralPath $StagingDir -Recurse -File -Filter $Pattern -ErrorAction SilentlyContinue
}
if ($Forbidden) {
    $Forbidden | Select-Object FullName
    throw "Forbidden source/build artifacts found in staging."
}

$FilesForHash = Get-ChildItem -LiteralPath $StagingDir -Recurse -File |
    Where-Object { $_.Name -notin @("RELEASE_MANIFEST.json", "SHA256SUMS.txt") } |
    Sort-Object FullName
$HashEntries = @($FilesForHash | ForEach-Object { Add-HashEntry $StagingDir $_ })

$Manifest = [PSCustomObject]@{
    name = $ReleaseName
    version = "1.2"
    built_at = (Get-Date).ToString("o")
    source_build_dir = $BuildDir
    runtime_only = $true
    cpu_supported = $true
    gpu_requires_vendor_driver = $true
    hikrobot_mvs_driver_prerequisite = $true
    files = $HashEntries
}
$ManifestJson = $Manifest | ConvertTo-Json -Depth 5
Write-Utf8File (Join-Path $StagingDir "RELEASE_MANIFEST.json") $ManifestJson

$ShaLines = $HashEntries | ForEach-Object { "$($_.sha256)  $($_.path)" }
Write-Utf8File (Join-Path $StagingDir "SHA256SUMS.txt") ($ShaLines -join [Environment]::NewLine)

New-Item -ItemType Directory -Force -Path $DistDir | Out-Null
Move-Item -LiteralPath $StagingDir -Destination $ReleaseDir
Compress-Archive -LiteralPath $ReleaseDir -DestinationPath $ZipPath -CompressionLevel Optimal -Force

Write-Host "[Package] Release directory: $ReleaseDir"
Write-Host "[Package] Release zip: $ZipPath"







