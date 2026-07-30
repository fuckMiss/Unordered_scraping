param(
    [string]$Configuration = "Release",
    [string]$BuildDir = "build"
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
if ((Split-Path -Leaf $ScriptDir) -ieq "scripts") {
    $AppDir = Split-Path -Parent $ScriptDir
} else {
    $AppDir = $ScriptDir
}
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

function Copy-DirectoryFilesIfExists($Path, $Destination, $Pattern = "*") {
    if (Test-Path -LiteralPath $Path) {
        Get-ChildItem -LiteralPath $Path -Filter $Pattern -File | ForEach-Object {
            Copy-IfExists $_.FullName $Destination
        }
    } else {
        Write-Warning "Missing optional runtime directory: $Path"
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

$OpenCvRoot = if ($env:OpenCV_DIR) { $env:OpenCV_DIR } else { Join-Path $AppDir "_deps\opencv\opencv\build" }
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
$OpenVinoRoots.Add("D:\Anaconda\envs\python_wx\Lib\site-packages\openvino")
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

$BundledHikRuntime = Join-Path $AppDir "vendor\hik_mvs\Runtime\Win64_x64"
Copy-DirectoryFilesIfExists $BundledHikRuntime $TargetDir "*.dll"
Copy-DirectoryFilesIfExists $BundledHikRuntime $TargetDir "*.cti"
Copy-DirectoryFilesIfExists $BundledHikRuntime $TargetDir "*.ax"
Copy-IfExists (Join-Path $BundledHikRuntime "CommonParameters.ini") $TargetDir
if (Test-Path -LiteralPath (Join-Path $BundledHikRuntime "ThirdParty")) {
    Copy-Item -LiteralPath (Join-Path $BundledHikRuntime "ThirdParty") -Destination $TargetDir -Recurse -Force
}

$AssetTargetDir = Join-Path $TargetDir "app\qt\assets"
New-Item -ItemType Directory -Force -Path $AssetTargetDir | Out-Null
Copy-IfExists (Join-Path $AppDir "app\qt\assets\app_logo_cutout.png") $AssetTargetDir
Copy-IfExists (Join-Path $AppDir "app\qt\assets\app_icon.ico") $AssetTargetDir

$WeightsTargetDir = Join-Path $TargetDir "models\weights"
New-Item -ItemType Directory -Force -Path $WeightsTargetDir | Out-Null
Copy-IfExists (Join-Path $AppDir "models\weights\best_obb.xml") $WeightsTargetDir
Copy-IfExists (Join-Path $AppDir "models\weights\best_obb.bin") $WeightsTargetDir
Copy-IfExists (Join-Path $AppDir "models\weights\best_seg.xml") $WeightsTargetDir
Copy-IfExists (Join-Path $AppDir "models\weights\best_seg.bin") $WeightsTargetDir

Copy-IfExists (Join-Path $AppDir "launch_tankeye.ps1") $TargetDir
Copy-IfExists (Join-Path $AppDir "launch_tankeye_main_only.vbs") $TargetDir

$ScriptsTargetDir = Join-Path $TargetDir "scripts"
New-Item -ItemType Directory -Force -Path $ScriptsTargetDir | Out-Null
Copy-IfExists (Join-Path $AppDir "scripts\launch_tankeye.ps1") $ScriptsTargetDir
Copy-IfExists (Join-Path $AppDir "scripts\launch_tankeye_main_only.vbs") $ScriptsTargetDir
Copy-IfExists (Join-Path $AppDir "scripts\create_desktop_shortcut.ps1") $ScriptsTargetDir
Copy-IfExists (Join-Path $AppDir "scripts\deploy_windows.ps1") $ScriptsTargetDir

$DocsTargetDir = Join-Path $TargetDir "docs"
New-Item -ItemType Directory -Force -Path $DocsTargetDir | Out-Null
Copy-IfExists (Join-Path $AppDir "README.md") $TargetDir
Copy-IfExists (Join-Path $AppDir "README_zh.md") $TargetDir
Copy-IfExists (Join-Path $AppDir "docs\README_NEW_PC_BUILD.md") $DocsTargetDir
Copy-IfExists (Join-Path $AppDir "docs\PLC_AND_GRAB_SETUP.md") $DocsTargetDir
Copy-IfExists (Join-Path $AppDir "docs\PLC_MAPPING_CURRENT.md") $DocsTargetDir

$ConfigTargetDir = Join-Path $TargetDir "config"
New-Item -ItemType Directory -Force -Path $ConfigTargetDir | Out-Null
Copy-IfExists (Join-Path $AppDir "config\tankeye.json") $ConfigTargetDir

Write-Host "Windows runtime deployed to: $TargetDir"
