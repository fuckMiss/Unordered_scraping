param(
    [string]$Configuration = "Release",
    [string]$BuildDir = "build_win_qt"
)

$ErrorActionPreference = "Stop"

$AppDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$TargetDir = Join-Path $AppDir "$BuildDir\$Configuration"
$AppExe = Join-Path $TargetDir "yolov11-tensorrt_qt_app.exe"

function Require-File($Path, $Name) {
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "$Name not found: $Path"
    }
}

function Require-Dir($Path, $Name) {
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "$Name not found: $Path"
    }
}

function Copy-IfExists($Path, $Destination) {
    if (Test-Path -LiteralPath $Path) {
        Copy-Item -LiteralPath $Path -Destination $Destination -Force
        Write-Host "Copied $([System.IO.Path]::GetFileName($Path))"
    } else {
        Write-Warning "Missing optional runtime file: $Path"
    }
}

Require-Dir $TargetDir "Build output directory"
Require-File $AppExe "Qt app executable"

$QtRoot = if ($env:Qt5_DIR) {
    Resolve-Path (Join-Path $env:Qt5_DIR "..\..\..")
} else {
    "D:\Qt\5.15.2\msvc2019_64"
}
$QtBin = Join-Path $QtRoot "bin"
$WinDeployQt = Join-Path $QtBin "windeployqt.exe"
Require-File $WinDeployQt "windeployqt"

$TensorRtRoot = if ($env:TENSORRT_ROOT) { $env:TENSORRT_ROOT } else { "D:\Tensorrt\TensorRT-8.6.1.6" }
$TensorRtLib = Join-Path $TensorRtRoot "lib"
Require-Dir $TensorRtLib "TensorRT lib directory"

$OpenCvRoot = if ($env:OpenCV_DIR) { $env:OpenCV_DIR } else { "D:\opencv\opencv-4.12.0\opencv\build" }
$OpenCvBin = Join-Path $OpenCvRoot "x64\vc16\bin"
Require-Dir $OpenCvBin "OpenCV bin directory"

$CudaRoot = if ($env:CUDA_PATH) { $env:CUDA_PATH } else { "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.1" }
$CudaBin = Join-Path $CudaRoot "bin"
Require-Dir $CudaBin "CUDA bin directory"

$env:Path = "$QtBin;$OpenCvBin;$TensorRtLib;$CudaBin;$env:Path"

& $WinDeployQt --release --compiler-runtime --no-translations $AppExe
if ($LASTEXITCODE -ne 0) {
    throw "windeployqt failed with exit code $LASTEXITCODE"
}

$RuntimeFiles = @(
    (Join-Path $OpenCvBin "opencv_world4120.dll"),
    (Join-Path $TensorRtLib "nvinfer.dll"),
    (Join-Path $TensorRtLib "nvinfer_plugin.dll"),
    (Join-Path $TensorRtLib "nvonnxparser.dll"),
    (Join-Path $TensorRtLib "nvparsers.dll"),
    (Join-Path $TensorRtLib "nvinfer_builder_resource.dll"),
    (Join-Path $TensorRtLib "nvinfer_dispatch.dll"),
    (Join-Path $TensorRtLib "nvinfer_lean.dll"),
    (Join-Path $TensorRtLib "nvinfer_vc_plugin.dll"),
    (Join-Path $CudaBin "cudart64_12.dll"),
    (Join-Path $CudaBin "cublas64_12.dll"),
    (Join-Path $CudaBin "cublasLt64_12.dll"),
    (Join-Path $CudaBin "cudnn64_8.dll")
)

foreach ($File in $RuntimeFiles) {
    Copy-IfExists $File $TargetDir
}

$AssetTargetDir = Join-Path $TargetDir "qt\assets"
New-Item -ItemType Directory -Force -Path $AssetTargetDir | Out-Null
Copy-IfExists (Join-Path $AppDir "qt\assets\app_logo_cutout.png") $AssetTargetDir

Write-Host "Windows runtime deployed to: $TargetDir"
