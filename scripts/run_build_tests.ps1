param(
    [string]$BuildDir = "build",
    [string]$Configuration = "",
    [string]$Filter = "*test*.exe",
    [switch]$BuildFirst
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$AppDir = Split-Path -Parent $ScriptDir
$BuildPath = Join-Path $AppDir $BuildDir
if (-not (Test-Path -LiteralPath $BuildPath)) {
    throw "Build directory not found: $BuildPath"
}

$TestDir = $BuildPath
if (-not [string]::IsNullOrWhiteSpace($Configuration)) {
    $ConfiguredDir = Join-Path $BuildPath $Configuration
    if (Test-Path -LiteralPath $ConfiguredDir) {
        $TestDir = $ConfiguredDir
    }
}

if ($BuildFirst) {
    $BuildArgs = @("--build", $BuildPath)
    if (-not [string]::IsNullOrWhiteSpace($Configuration)) {
        $BuildArgs += @("--config", $Configuration)
    }
    & cmake @BuildArgs
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

$RuntimePathCandidates = @(
    $TestDir,
    $BuildPath,
    (Join-Path $AppDir "vendor\hik_mvs\Runtime\Win64_x64"),
    "C:\Program Files (x86)\Common Files\MVS\Runtime\Win64_x64",
    "D:\Qt\5.15.2\msvc2019_64\bin",
    (Join-Path $AppDir "_deps\opencv\opencv\build\x64\vc16\bin"),
    "D:\opencv\opencv-4.12.0\opencv\build\x64\vc16\bin"
)

$OpenVinoRoots = @()
if ($env:CONDA_PREFIX) {
    $OpenVinoRoots += (Join-Path $env:CONDA_PREFIX "Lib\site-packages\openvino")
}
if ($env:openvino_DIR) {
    $OpenVinoRoots += (Split-Path -Parent $env:openvino_DIR)
}
if ($env:OpenVINO_DIR) {
    $OpenVinoRoots += (Split-Path -Parent $env:OpenVINO_DIR)
}
if ($env:OPENVINO_ROOT) {
    $OpenVinoRoots += $env:OPENVINO_ROOT
}
$OpenVinoRoots += @(
    "D:\Anaconda\envs\cll_yolo\Lib\site-packages\openvino",
    "D:\Anaconda\envs\python_wx\Lib\site-packages\openvino",
    "D:\Anaconda3\envs\cll_yolo\Lib\site-packages\openvino",
    "C:\Program Files (x86)\Intel\openvino"
)
foreach ($Root in $OpenVinoRoots) {
    if ([string]::IsNullOrWhiteSpace($Root)) {
        continue
    }
    $RuntimePathCandidates += @(
        (Join-Path $Root "libs"),
        $Root,
        (Join-Path $Root "runtime\bin\intel64\Release"),
        (Join-Path $Root "runtime\bin\intel64\Debug"),
        (Join-Path $Root "runtime\3rdparty\tbb\bin")
    )
}

$RuntimePaths = $RuntimePathCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -Unique
$env:Path = (($RuntimePaths + @($env:Path)) -join ";")

$Tests = Get-ChildItem -LiteralPath $TestDir -Filter $Filter -File |
    Sort-Object Name
if ($Tests.Count -eq 0) {
    throw "No test executables matched '$Filter' in $TestDir"
}

Write-Host "[TankEyeTests] Test directory: $TestDir"
Write-Host "[TankEyeTests] Runtime PATH entries:"
foreach ($PathItem in $RuntimePaths) {
    Write-Host "  $PathItem"
}

$Failed = @()
foreach ($Test in $Tests) {
    Write-Host "[TankEyeTests] RUN $($Test.Name)"
    & $Test.FullName
    if ($LASTEXITCODE -ne 0) {
        $Failed += $Test.Name
        Write-Host "[TankEyeTests] FAIL $($Test.Name) exit=$LASTEXITCODE"
    } else {
        Write-Host "[TankEyeTests] PASS $($Test.Name)"
    }
}

if ($Failed.Count -gt 0) {
    Write-Host "[TankEyeTests] Failed tests:"
    foreach ($Name in $Failed) {
        Write-Host "  $Name"
    }
    exit 1
}

Write-Host "[TankEyeTests] All tests passed."
