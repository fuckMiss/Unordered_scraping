param(
    [string]$Configuration = "Release",
    [string]$BuildDir = "build_win_unit"
)

$ErrorActionPreference = "Stop"

$AppDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$TargetDir = Join-Path $AppDir "$BuildDir\$Configuration"
$AppExe = Join-Path $TargetDir "tankeye-openvino_qt_app.exe"

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

$OpenCvRoot = if ($env:OpenCV_DIR) { $env:OpenCV_DIR } else { "D:\opencv\opencv-4.12.0\opencv\build" }
$OpenCvBinCandidates = @(
    (Join-Path $OpenCvRoot "x64\vc16\bin"),
    (Join-Path $OpenCvRoot "bin\Release"),
    (Join-Path $OpenCvRoot "bin")
)
$OpenCvBin = $OpenCvBinCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if ([string]::IsNullOrWhiteSpace($OpenCvBin)) {
    throw "OpenCV bin directory not found. Checked: $($OpenCvBinCandidates -join '; ')"
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
    $OpenVinoBinCandidates.Add((Join-Path $Root "runtime\3rdparty\tbb\bin"))
}
$OpenVinoBin = $OpenVinoBinCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if ([string]::IsNullOrWhiteSpace($OpenVinoBin)) {
    throw "OpenVINO runtime bin/libs directory not found. Set CONDA_PREFIX, openvino_DIR, OpenVINO_DIR, OPENVINO_ROOT, or update deploy_windows.ps1."
}

$RuntimePaths = @($QtBin, $OpenCvBin, $OpenVinoBin) | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -Unique
$env:Path = (($RuntimePaths + @($env:Path)) -join ";")

& $WinDeployQt --release --compiler-runtime --no-translations $AppExe
if ($LASTEXITCODE -ne 0) {
    throw "windeployqt failed with exit code $LASTEXITCODE"
}

$OpenCvDlls = Get-ChildItem -LiteralPath $OpenCvBin -Filter "opencv*.dll" -File
if ($OpenCvDlls.Count -eq 0) {
    throw "No OpenCV runtime DLLs found in: $OpenCvBin"
}
foreach ($Dll in $OpenCvDlls) {
    Copy-IfExists $Dll.FullName $TargetDir
}

Get-ChildItem -LiteralPath $OpenVinoBin -Filter "*.dll" -File | ForEach-Object {
    Copy-IfExists $_.FullName $TargetDir
}

$AssetTargetDir = Join-Path $TargetDir "qt\assets"
New-Item -ItemType Directory -Force -Path $AssetTargetDir | Out-Null
Copy-IfExists (Join-Path $AppDir "qt\assets\app_logo_cutout.png") $AssetTargetDir
Copy-IfExists (Join-Path $AppDir "qt\assets\app_icon.ico") $AssetTargetDir

$WeightsTargetDir = Join-Path $TargetDir "weights"
New-Item -ItemType Directory -Force -Path $WeightsTargetDir | Out-Null
Copy-IfExists (Join-Path $AppDir "weights\best_obb.xml") $WeightsTargetDir
Copy-IfExists (Join-Path $AppDir "weights\best_obb.bin") $WeightsTargetDir
Copy-IfExists (Join-Path $AppDir "weights\best_seg.xml") $WeightsTargetDir
Copy-IfExists (Join-Path $AppDir "weights\best_seg.bin") $WeightsTargetDir

Copy-IfExists (Join-Path $AppDir "launch_tankeye.ps1") $TargetDir
Copy-IfExists (Join-Path $AppDir "deploy_windows.ps1") $TargetDir
Copy-IfExists (Join-Path $AppDir "README.md") $TargetDir
Copy-IfExists (Join-Path $AppDir "README_zh.md") $TargetDir

Write-Host "Windows runtime deployed to: $TargetDir"
