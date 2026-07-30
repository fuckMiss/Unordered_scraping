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
    [string]$CameraIp = "192.168.0.233",
    [string]$Configuration = "Release",
    [string]$BuildDir = "build"
)

$ErrorActionPreference = "Stop"

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

if (-not (Test-Path -LiteralPath $AppExe)) {
    throw "Qt app executable not found: $AppExe"
}
if (-not (Test-Path -LiteralPath $LogDir)) {
    New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
}

if ([string]::IsNullOrWhiteSpace($ObbModel)) {
    $ObbModelCandidates = @(
        (Join-Path $AppDir "models\weights\best_obb.xml"),
        (Join-Path $TargetDir "models\weights\best_obb.xml"),
        (Join-Path $TargetDir "weights\best_obb.xml"),
        (Join-Path (Split-Path -Parent $TargetDir) "models\weights\best_obb.xml"),
        (Join-Path $AppDir "weights\best_obb.xml")
    )
    $ObbModel = $ObbModelCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
    if ([string]::IsNullOrWhiteSpace($ObbModel)) {
        $ObbModel = $ObbModelCandidates[0]
    }
}
if ([string]::IsNullOrWhiteSpace($SegModel)) {
    $SegModelCandidates = @(
        (Join-Path $AppDir "models\weights\best_seg.xml"),
        (Join-Path $TargetDir "models\weights\best_seg.xml"),
        (Join-Path $TargetDir "weights\best_seg.xml"),
        (Join-Path (Split-Path -Parent $TargetDir) "models\weights\best_seg.xml"),
        (Join-Path $AppDir "weights\best_seg.xml")
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
$env:TANKEYE_OPENVINO_DEVICE = $Device
$OpenVinoCacheDir = Join-Path $TargetDir "openvino_cache"
if (-not (Test-Path -LiteralPath $OpenVinoCacheDir)) {
    New-Item -ItemType Directory -Force -Path $OpenVinoCacheDir | Out-Null
}
$env:TANKEYE_OPENVINO_CACHE_DIR = $OpenVinoCacheDir
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
    Add-Type -AssemblyName System.Windows.Forms
    $WorkingArea = [System.Windows.Forms.Screen]::PrimaryScreen.WorkingArea
    $ScaleByWidth = [double]$WorkingArea.Width / 1360.0
    $ScaleByHeight = [double]$WorkingArea.Height / 820.0
    $AutoUiScale = [Math]::Min($ScaleByWidth, $ScaleByHeight)
    if (($WorkingArea.Width -le 1366) -or ($WorkingArea.Height -le 720)) {
        $AutoUiScale = $AutoUiScale * 0.82
    }
    if ($AutoUiScale -lt 0.65) {
        $AutoUiScale = 0.65
    }
    if ($AutoUiScale -gt 1.0) {
        $AutoUiScale = 1.0
    }
    $env:QT_SCALE_FACTOR = $AutoUiScale.ToString("0.00", [Globalization.CultureInfo]::InvariantCulture)
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
Write-Host "[TankEye] UI scale: $(if ($UiScale -gt 0) { $UiScale } else { "AUTO $env:QT_SCALE_FACTOR" })"
Write-Host "[TankEye] Postprocess debug: $(if ($env:TANKEYE_DEBUG_POSTPROCESS -eq "1") { "ON" } else { "OFF" })"
Write-Host "[TankEye] Log: $LogFile"
Write-Host "[TankEye] OpenVINO cache: $OpenVinoCacheDir"
Write-Host "[TankEye] Runtime PATH entries:"
foreach ($PathItem in $RuntimePaths) {
    Write-Host "  $PathItem"
}

if ($AutoLoadModels -and (Test-Path -LiteralPath $ObbModel) -and (Test-Path -LiteralPath $SegModel)) {
    $Process = Start-Process -FilePath $AppExe -ArgumentList @($ObbModel, $SegModel) -WorkingDirectory $TargetDir -Wait -PassThru
    $LASTEXITCODE = $Process.ExitCode
} else {
    if (-not $AutoLoadModels) {
        Write-Host "[TankEye] Auto model loading is disabled for this source-tree launch."
        Write-Host "[TankEye] Use -AutoLoadModels only from an ASCII-only path or after Unicode path support is fixed."
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
