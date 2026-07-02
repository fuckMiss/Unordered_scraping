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
    [string]$Configuration = "Release",
    [string]$BuildDir = "build_win_unit"
)

$ErrorActionPreference = "Stop"

$AppDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$TargetDir = Join-Path $AppDir "$BuildDir\$Configuration"
$AppExe = Join-Path $TargetDir "tankeye-openvino_qt_app.exe"
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
        (Join-Path $AppDir "weights\best_obb.xml"),
        (Join-Path $TargetDir "weights\best_obb.xml"),
        (Join-Path (Split-Path -Parent $TargetDir) "weights\best_obb.xml")
    )
    $ObbModel = $ObbModelCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
    if ([string]::IsNullOrWhiteSpace($ObbModel)) {
        $ObbModel = $ObbModelCandidates[0]
    }
}
if ([string]::IsNullOrWhiteSpace($SegModel)) {
    $SegModelCandidates = @(
        (Join-Path $AppDir "weights\best_seg.xml"),
        (Join-Path $TargetDir "weights\best_seg.xml"),
        (Join-Path (Split-Path -Parent $TargetDir) "weights\best_seg.xml")
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
    "D:\Qt\5.15.2\msvc2019_64\bin",
    "D:\opencv\opencv-4.12.0\opencv\build\x64\vc16\bin"
) + ($OpenVinoBinCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -Unique)

$env:Path = (($RuntimePaths + @($env:Path)) -join ";")
$env:TANKEYE_LOG_FILE = $LogFile
$env:TANKEYE_OPENVINO_DEVICE = $Device
$env:TANKEYE_WINDOW_MODE = $WindowMode
$env:TANKEYE_WINDOW_WIDTH = [string]$WindowWidth
$env:TANKEYE_WINDOW_HEIGHT = [string]$WindowHeight
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
Write-Host "[TankEye] UI scale: $(if ($UiScale -gt 0) { $UiScale } else { "AUTO $env:QT_SCALE_FACTOR" })"
Write-Host "[TankEye] Log: $LogFile"
Write-Host "[TankEye] Runtime PATH entries:"
foreach ($PathItem in $RuntimePaths) {
    Write-Host "  $PathItem"
}

if ((Test-Path -LiteralPath $ObbModel) -and (Test-Path -LiteralPath $SegModel)) {
    & $AppExe $ObbModel $SegModel
} else {
    Write-Warning "Model files not found. Starting app without model arguments."
    Write-Warning "Expected OBB: $ObbModel"
    Write-Warning "Expected SEG: $SegModel"
    & $AppExe
}

$ExitCode = if ($null -ne $LASTEXITCODE) { $LASTEXITCODE } else { 0 }
Write-Host "[TankEye] App exited with code $ExitCode"
Write-Host "[TankEye] Open the log with:"
Write-Host "  notepad `"$LogFile`""
Write-Host "[TankEye] Check selected device with:"
Write-Host "  findstr /i `"OpenVINO Requested Selected Available Device GPU CPU`" `"$LogFile`""
