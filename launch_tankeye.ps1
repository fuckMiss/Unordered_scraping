param(
    [string]$ObbModel = "",
    [string]$SegModel = "",
    [ValidateSet("AUTO", "GPU", "CPU")]
    [string]$Device = "AUTO",
    [ValidateSet("Normal", "Maximized", "Fullscreen")]
    [string]$WindowMode = "Normal",
    [int]$WindowWidth = 1280,
    [int]$WindowHeight = 720,
    [double]$UiScale = 0,
    [string]$PlcHost = "",
    [int]$PlcPort = 0,
    [switch]$SimulatePlc,
    [switch]$DebugPostprocess,
    [switch]$AutoLoadModels,
    [ValidateSet("Manual", "AutoStart")]
    [string]$StartupProfile = "Manual",
    [string]$CameraIp = "192.168.0.233",
    [string]$Configuration = "Release",
    [string]$BuildDir = "build",
    [int]$StartupDelaySeconds = 0,
    [int]$DeviceProbeTimeoutSeconds = 60
)

$ErrorActionPreference = "Stop"

$UseAutoStartProfile = ($StartupProfile -eq "AutoStart")
if ($UseAutoStartProfile) {
    $WindowMode = "Maximized"
}
$LoadModels = ($AutoLoadModels -or $UseAutoStartProfile)

function Join-ProcessArguments($Arguments) {
    $Quoted = foreach ($Argument in $Arguments) {
        $Text = [string]$Argument
        if ($Text -match '^[A-Za-z]:\\' -or $Text -match '\s') {
            '"' + $Text.Replace('"', '\"') + '"'
        } else {
            $Text
        }
    }
    return ($Quoted -join " ")
}

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
if ((Split-Path -Leaf $ScriptDir) -ieq "scripts") {
    $AppDir = Split-Path -Parent $ScriptDir
} else {
    $AppDir = $ScriptDir
}
$TargetDir = Join-Path $AppDir "$BuildDir\$Configuration"
$AppExe = Join-Path $TargetDir "tankeye-openvino_qt_app.exe"
if (-not (Test-Path -LiteralPath $AppExe)) {
    $SingleConfigTargetDir = Join-Path $AppDir $BuildDir
    $SingleConfigExe = Join-Path $SingleConfigTargetDir "tankeye-openvino_qt_app.exe"
    if (Test-Path -LiteralPath $SingleConfigExe) {
        $TargetDir = $SingleConfigTargetDir
        $AppExe = $SingleConfigExe
    }
}
if (-not (Test-Path -LiteralPath $AppExe)) {
    $StandaloneExe = Join-Path $AppDir "tankeye-openvino_qt_app.exe"
    if (Test-Path -LiteralPath $StandaloneExe) {
        $TargetDir = $AppDir
        $AppExe = $StandaloneExe
    }
}
$LogDir = Join-Path $TargetDir "logs"
$LogStamp = Get-Date -Format "yyyyMMdd_HHmmss"
$LogFile = Join-Path $LogDir "tankeye_$LogStamp.log"
$DeviceProbeExe = Join-Path $TargetDir "tankeye-openvino_device_probe.exe"

if (-not (Test-Path -LiteralPath $AppExe)) {
    throw "Qt app executable not found: $AppExe"
}
if (-not (Test-Path -LiteralPath $LogDir)) {
    New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
}

if ([string]::IsNullOrWhiteSpace($ObbModel)) {
    $ObbModelCandidates = @(
        (Join-Path $AppDir "models\DG_8_weights\best_obb.xml"),
        (Join-Path $TargetDir "models\DG_8_weights\best_obb.xml"),
        (Join-Path $TargetDir "DG_8_weights\best_obb.xml"),
        (Join-Path (Split-Path -Parent $TargetDir) "models\DG_8_weights\best_obb.xml"),
        (Join-Path $AppDir "DG_8_weights\best_obb.xml")
    )
    $ObbModel = $ObbModelCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
    if ([string]::IsNullOrWhiteSpace($ObbModel)) {
        $ObbModel = $ObbModelCandidates[0]
    }
}
if ([string]::IsNullOrWhiteSpace($SegModel)) {
    $SegModelCandidates = @(
        (Join-Path $AppDir "models\DG_8_weights\best_seg.xml"),
        (Join-Path $TargetDir "models\DG_8_weights\best_seg.xml"),
        (Join-Path $TargetDir "DG_8_weights\best_seg.xml"),
        (Join-Path (Split-Path -Parent $TargetDir) "models\DG_8_weights\best_seg.xml"),
        (Join-Path $AppDir "DG_8_weights\best_seg.xml")
    )
    $SegModel = $SegModelCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
    if ([string]::IsNullOrWhiteSpace($SegModel)) {
        $SegModel = $SegModelCandidates[0]
    }
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
    $OpenVinoBinCandidates.Add((Join-Path $Root "runtime\bin\intel64\Debug"))
    $OpenVinoBinCandidates.Add((Join-Path $Root "runtime\3rdparty\tbb\bin"))
}

$RuntimePaths = @(
    $TargetDir,
    (Join-Path $AppDir "vendor\hik_mvs\Runtime\Win64_x64"),
    "C:\Program Files (x86)\Common Files\MVS\Runtime\Win64_x64",
    "D:\Qt\5.15.2\msvc2019_64\bin",
    (Join-Path $AppDir "_deps\opencv\opencv\build\x64\vc16\bin"),
    "D:\opencv\opencv-4.12.0\opencv\build\x64\vc16\bin"
) | Where-Object { Test-Path -LiteralPath $_ }
$RuntimePaths = $RuntimePaths + ($OpenVinoBinCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -Unique)

$env:Path = (($RuntimePaths + @($env:Path)) -join ";")
$env:TANKEYE_LOG_FILE = $LogFile
$OpenVinoCacheDir = Join-Path $TargetDir "openvino_cache"
if (-not (Test-Path -LiteralPath $OpenVinoCacheDir)) {
    New-Item -ItemType Directory -Force -Path $OpenVinoCacheDir | Out-Null
}
$env:TANKEYE_OPENVINO_CACHE_DIR = $OpenVinoCacheDir
$SettingsDir = Join-Path $TargetDir "config"
if (-not (Test-Path -LiteralPath $SettingsDir)) {
    New-Item -ItemType Directory -Force -Path $SettingsDir | Out-Null
}
$env:TANKEYE_SETTINGS_INI_PATH = Join-Path $SettingsDir "engineering_settings.ini"
$AdminAuthKeyCandidates = @(
    (Join-Path $TargetDir "config\admin_auth.key"),
    (Join-Path $AppDir "config\admin_auth.key")
)
$AdminAuthKey = $AdminAuthKeyCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if ($AdminAuthKey) {
    $env:TANKEYE_ADMIN_AUTH_KEY_FILE = $AdminAuthKey
} else {
    Remove-Item Env:\TANKEYE_ADMIN_AUTH_KEY_FILE -ErrorAction SilentlyContinue
}
$env:TANKEYE_WINDOW_MODE = $WindowMode
$env:TANKEYE_WINDOW_WIDTH = [string]$WindowWidth
$env:TANKEYE_WINDOW_HEIGHT = [string]$WindowHeight
if (-not [string]::IsNullOrWhiteSpace($PlcHost)) {
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
}
if ($UseAutoStartProfile) {
    $env:TANKEYE_AUTO_START_GRASP = "1"
} else {
    Remove-Item Env:\TANKEYE_AUTO_START_GRASP -ErrorAction SilentlyContinue
}
if (-not [string]::IsNullOrWhiteSpace($CameraIp)) {
    $env:TANKEYE_CAMERA_IP = $CameraIp
} else {
    Remove-Item Env:\TANKEYE_CAMERA_IP -ErrorAction SilentlyContinue
}
if ($UiScale -gt 0) {
    $env:TANKEYE_UI_SCALE = [string]$UiScale
    $env:QT_SCALE_FACTOR = [string]$UiScale
} else {
    Remove-Item Env:\TANKEYE_UI_SCALE -ErrorAction SilentlyContinue
    Remove-Item Env:\QT_SCALE_FACTOR -ErrorAction SilentlyContinue
}

Write-Host "[TankEye] App: $AppExe"
Write-Host "[TankEye] OBB: $ObbModel"
Write-Host "[TankEye] SEG: $SegModel"
Write-Host "[TankEye] Device: $Device"
Write-Host "[TankEye] Window: $WindowMode ${WindowWidth}x${WindowHeight}"
$PlcDisplay = if ($env:TANKEYE_PLC_HOST) {
    "${env:TANKEYE_PLC_HOST}:${env:TANKEYE_PLC_PORT}"
} elseif ($SimulatePlc) {
    "SIMULATED"
} else {
    "default 192.168.3.205:502"
}
Write-Host "[TankEye] PLC: $PlcDisplay"
Write-Host "[TankEye] Camera IP: $(if ($env:TANKEYE_CAMERA_IP) { $env:TANKEYE_CAMERA_IP } else { "first enumerated camera" })"
Write-Host "[TankEye] UI scale: $(if ($UiScale -gt 0) { $UiScale } else { "Qt/Windows auto" })"
Write-Host "[TankEye] Postprocess debug: $(if ($env:TANKEYE_DEBUG_POSTPROCESS -eq "1") { "ON" } else { "OFF" })"
Write-Host "[TankEye] Startup profile: $StartupProfile"
Write-Host "[TankEye] Auto model loading: $(if ($LoadModels) { "ON" } else { "OFF" })"
Write-Host "[TankEye] Auto start grasp: $(if ($env:TANKEYE_AUTO_START_GRASP -eq "1") { "ON" } else { "OFF" })"
Write-Host "[TankEye] Log: $LogFile"
Write-Host "[TankEye] OpenVINO cache: $OpenVinoCacheDir"
Write-Host "[TankEye] Engineering settings: $env:TANKEYE_SETTINGS_INI_PATH"
Write-Host "[TankEye] Admin auth key: $(if ($AdminAuthKey) { $AdminAuthKey } else { "development default" })"
Write-Host "[TankEye] Runtime PATH entries:"
foreach ($PathItem in $RuntimePaths) {
    Write-Host "  $PathItem"
}

function Resolve-TankEyeOpenVinoDevice {
    param(
        [string]$RequestedDevice,
        [string]$ProbeExe,
        [string]$ObbModelPath,
        [string]$SegModelPath,
        [string]$CacheDir,
        [int]$TimeoutSeconds
    )

    $UpperDevice = $RequestedDevice.ToUpperInvariant()
    if ($UpperDevice -ne "AUTO") {
        $env:TANKEYE_OPENVINO_DEVICE_PROBE_RESULT = "not required; requested $UpperDevice"
        return $UpperDevice
    }

    if (-not (Test-Path -LiteralPath $ProbeExe)) {
        $env:TANKEYE_OPENVINO_DEVICE_PROBE_RESULT = "AUTO GPU probe missing; resolved CPU"
        Write-Warning "[TankEye] Device probe not found; AUTO resolved to CPU: $ProbeExe"
        return "CPU"
    }
    if (-not (Test-Path -LiteralPath $ObbModelPath) -or -not (Test-Path -LiteralPath $SegModelPath)) {
        $env:TANKEYE_OPENVINO_DEVICE_PROBE_RESULT = "AUTO GPU probe skipped because model files are missing; resolved CPU"
        Write-Warning "[TankEye] Model files missing; AUTO resolved to CPU."
        return "CPU"
    }

    $TimeoutSeconds = [Math]::Max(1, [Math]::Min($TimeoutSeconds, 120))
    Write-Host "[TankEye] AUTO device probe: GPU, timeout ${TimeoutSeconds}s"
    $ProbeProcess = Start-Process -FilePath $ProbeExe `
        -ArgumentList @($ObbModelPath, $SegModelPath, "--device=GPU", "--cache-dir=$CacheDir") `
        -WorkingDirectory (Split-Path -Parent $ProbeExe) `
        -WindowStyle Hidden `
        -PassThru
    $Exited = $ProbeProcess.WaitForExit($TimeoutSeconds * 1000)
    if (-not $Exited) {
        Stop-Process -Id $ProbeProcess.Id -Force -ErrorAction SilentlyContinue
        $env:TANKEYE_OPENVINO_DEVICE_PROBE_RESULT = "AUTO GPU probe timed out after ${TimeoutSeconds}s; resolved CPU"
        Write-Warning "[TankEye] AUTO GPU probe timed out; resolved to CPU."
        return "CPU"
    }
    if ($ProbeProcess.ExitCode -eq 0) {
        $env:TANKEYE_OPENVINO_DEVICE_PROBE_RESULT = "AUTO GPU probe succeeded; resolved GPU"
        Write-Host "[TankEye] AUTO GPU probe succeeded; resolved to GPU."
        return "GPU"
    }

    $env:TANKEYE_OPENVINO_DEVICE_PROBE_RESULT = "AUTO GPU probe exited with code $($ProbeProcess.ExitCode); resolved CPU"
    Write-Warning "[TankEye] AUTO GPU probe failed with code $($ProbeProcess.ExitCode); resolved to CPU."
    return "CPU"
}

$RequestedDevice = $Device.ToUpperInvariant()
$ResolvedDevice = Resolve-TankEyeOpenVinoDevice `
    -RequestedDevice $RequestedDevice `
    -ProbeExe $DeviceProbeExe `
    -ObbModelPath $ObbModel `
    -SegModelPath $SegModel `
    -CacheDir $OpenVinoCacheDir `
    -TimeoutSeconds $DeviceProbeTimeoutSeconds
$env:TANKEYE_OPENVINO_DEVICE = $ResolvedDevice
Write-Host "[TankEye] Requested device: $RequestedDevice"
Write-Host "[TankEye] Resolved device: $ResolvedDevice"

if ($StartupDelaySeconds -gt 0) {
    $StartupDelaySeconds = [Math]::Min($StartupDelaySeconds, 600)
    Write-Host "[TankEye] Startup delay: $StartupDelaySeconds seconds"
    Start-Sleep -Seconds $StartupDelaySeconds
}

if ($LoadModels -and (Test-Path -LiteralPath $ObbModel) -and (Test-Path -LiteralPath $SegModel)) {
    $Process = Start-Process -FilePath $AppExe -ArgumentList @($ObbModel, $SegModel) -WorkingDirectory $TargetDir -Wait -PassThru
    $LASTEXITCODE = $Process.ExitCode
} else {
    if (-not $LoadModels) {
        Write-Host "[TankEye] Auto model loading is disabled for this source-tree launch."
        Write-Host "[TankEye] Use -AutoLoadModels for manual model loading, or -StartupProfile AutoStart for startup-style auto loading."
    } else {
        Write-Warning "Model files not found. Starting app without model arguments."
        Write-Warning "Expected OBB: $ObbModel"
        Write-Warning "Expected SEG: $SegModel"
    }
    $Process = Start-Process -FilePath $AppExe -WorkingDirectory $TargetDir -Wait -PassThru
    $LASTEXITCODE = $Process.ExitCode
}

$ExitCode = if ($null -ne $LASTEXITCODE) { $LASTEXITCODE } else { 0 }
Write-Host "[TankEye] App exited with code $ExitCode"
Write-Host "[TankEye] Open the log with:"
Write-Host "  notepad `"$LogFile`""
Write-Host "[TankEye] Check selected device with:"
Write-Host "  findstr /i `"OpenVINO Requested Selected Available Device GPU CPU`" `"$LogFile`""
