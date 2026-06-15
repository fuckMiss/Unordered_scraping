param(
    [string]$ObbEngine = "",
    [string]$SegEngine = "",
    [string]$Configuration = "Release",
    [string]$BuildDir = "build_win_qt"
)

$ErrorActionPreference = "Stop"

$AppDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$TargetDir = Join-Path $AppDir "$BuildDir\$Configuration"
$AppExe = Join-Path $TargetDir "yolov11-tensorrt_qt_app.exe"

if (-not (Test-Path -LiteralPath $AppExe)) {
    throw "Qt app executable not found: $AppExe"
}

if ([string]::IsNullOrWhiteSpace($ObbEngine)) {
    $ObbEngine = Join-Path $AppDir "weights\best_obb.engine"
}
if ([string]::IsNullOrWhiteSpace($SegEngine)) {
    $SegEngine = Join-Path $AppDir "weights\best_seg.engine"
}

$RuntimePaths = @(
    $TargetDir,
    "D:\Qt\5.15.2\msvc2019_64\bin",
    "D:\Tensorrt\TensorRT-8.6.1.6\bin",
    "D:\Tensorrt\TensorRT-8.6.1.6\lib",
    "D:\opencv\opencv-4.12.0\opencv\build\x64\vc16\bin",
    "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.1\bin"
) | Where-Object { Test-Path -LiteralPath $_ }

$env:Path = (($RuntimePaths + @($env:Path)) -join ";")

if ((Test-Path -LiteralPath $ObbEngine) -and (Test-Path -LiteralPath $SegEngine)) {
    & $AppExe $ObbEngine $SegEngine
} else {
    Write-Warning "Engine files not found. Starting app without model arguments."
    Write-Warning "Expected OBB: $ObbEngine"
    Write-Warning "Expected SEG: $SegEngine"
    & $AppExe
}
