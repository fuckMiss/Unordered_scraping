param(
    [string]$BuildDir = "build",
    [string]$ReleaseName = "TankEye-Iris_1.4.5",
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
Require-File (Join-Path $BuildOutputPath "tankeye-openvino_device_probe.exe") "OpenVINO device probe executable"
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
Copy-File (Join-Path $BuildOutputPath "tankeye-openvino_device_probe.exe") $StagingDir
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
$AdminAuthKeySource = Join-Path $AppDir "config\admin_auth.key"
if (Test-Path -LiteralPath $AdminAuthKeySource) {
    Copy-File $AdminAuthKeySource (Join-Path $StagingDir "config")
}

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
        $config_json = Get-Content -LiteralPath $PackagedConfigPath -Raw -Encoding UTF8 | ConvertFrom-Json
        if ($null -ne $config_json.machine_limits) {
            $config_json.machine_limits.enabled = $false
            Write-Utf8File $PackagedConfigPath ($config_json | ConvertTo-Json -Depth 20)
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
    [switch]$AutoLoadModels,
    [ValidateSet("Manual", "AutoStart")]
    [string]$StartupProfile = "Manual",
    [switch]$ClearOpenVinoCache,
    [string]$CameraIp = "192.168.0.233",
    [int]$StartupDelaySeconds = 0,
    [int]$DeviceProbeTimeoutSeconds = 60
)

$ErrorActionPreference = "Stop"

$UseAutoStartProfile = ($StartupProfile -eq "AutoStart")
if ($UseAutoStartProfile) {
    $WindowMode = "Maximized"
}

$AppDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$AppExe = Join-Path $AppDir "tankeye-openvino_qt_app.exe"
$DeviceProbeExe = Join-Path $AppDir "tankeye-openvino_device_probe.exe"
$ObbModel = Join-Path $AppDir "models\weights\best_obb.xml"
$SegModel = Join-Path $AppDir "models\weights\best_seg.xml"
$LogDir = Join-Path $AppDir "logs"
$LogStamp = Get-Date -Format "yyyyMMdd_HHmmss"
$LogFile = Join-Path $LogDir "tankeye_$LogStamp.log"
$OpenVinoCacheDir = Join-Path $AppDir "openvino_cache"

foreach ($Required in @($AppExe, $DeviceProbeExe, $ObbModel, $SegModel)) {
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
$env:TANKEYE_SETTINGS_INI_PATH = Join-Path $AppDir "config\engineering_settings.ini"
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
if ($UseAutoStartProfile) {
    $env:TANKEYE_AUTO_START_GRASP = "1"
} else {
    Remove-Item Env:\TANKEYE_AUTO_START_GRASP -ErrorAction SilentlyContinue
}
$env:TANKEYE_LOCK_APP_DISPLAY = "1"
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
    Write-Host "[TankEye] Startup profile: $StartupProfile"
    Write-Host "[TankEye] Auto model loading: ON"
    Write-Host "[TankEye] Auto start grasp: $(if ($env:TANKEYE_AUTO_START_GRASP -eq '1') { 'ON' } else { 'OFF' })"
    Write-Host "[TankEye] Log: $SelectedLogFile"
    Write-Host "[TankEye] OpenVINO cache: $OpenVinoCacheDir"
    Write-Host "[TankEye] Engineering settings: $env:TANKEYE_SETTINGS_INI_PATH"
    if ($StartupDelaySeconds -gt 0) {
        $StartupDelaySeconds = [Math]::Min($StartupDelaySeconds, 600)
        Write-Host "[TankEye] Startup delay: $StartupDelaySeconds seconds"
        Start-Sleep -Seconds $StartupDelaySeconds
    }

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

    $TimeoutSeconds = [Math]::Max(1, [Math]::Min($TimeoutSeconds, 120))
    Write-Host "[TankEye] AUTO device probe: GPU, timeout ${TimeoutSeconds}s"
    $ProbeProcess = Start-Process -FilePath $ProbeExe `
        -ArgumentList @($ObbModelPath, $SegModelPath, "--device=GPU", "--cache-dir=$CacheDir") `
        -WorkingDirectory $AppDir `
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

$ResolvedDevice = Resolve-TankEyeOpenVinoDevice `
    -RequestedDevice $RequestedDevice `
    -ProbeExe $DeviceProbeExe `
    -ObbModelPath $ObbModel `
    -SegModelPath $SegModel `
    -CacheDir $OpenVinoCacheDir `
    -TimeoutSeconds $DeviceProbeTimeoutSeconds
Write-Host "[TankEye] Requested device: $RequestedDevice"
Write-Host "[TankEye] Resolved device: $ResolvedDevice"
exit (Invoke-TankEyeApp -SelectedDevice $ResolvedDevice -SelectedLogFile $LogFile -StartupGuardSeconds 0)
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
param(
    [int]$StartupDelaySeconds = 0,
    [switch]$NoAutoStart
)

$ErrorActionPreference = "Stop"
$AppDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$Desktop = [Environment]::GetFolderPath("Desktop")
$Startup = [Environment]::GetFolderPath("Startup")
$ShortcutPath = Join-Path $Desktop "TankEye-Iris.lnk"
$StartupVbsPath = Join-Path $Startup "TankEye-Iris.vbs"
$LegacyStartupShortcutPath = Join-Path $Startup "TankEye-Iris.lnk"
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
if (-not $NoAutoStart) {
    $StartupDelaySeconds = [Math]::Max(0, [Math]::Min($StartupDelaySeconds, 600))
    Remove-Item -LiteralPath $LegacyStartupShortcutPath -Force -ErrorAction SilentlyContinue
    $StartupVbs = @"
Option Explicit

Dim shell, appDir, launchScript, command

Set shell = CreateObject("WScript.Shell")

appDir = "$($AppDir.Replace('"', '""'))"
launchScript = "$((Join-Path $AppDir 'launch_tankeye.ps1').Replace('"', '""'))"
command = "powershell.exe -NoProfile -ExecutionPolicy Bypass -File " & _
          """" & launchScript & """" & " -StartupProfile AutoStart -StartupDelaySeconds $StartupDelaySeconds"

shell.CurrentDirectory = appDir
shell.Run command, 0, False
"@
    [System.IO.File]::WriteAllText($StartupVbsPath, $StartupVbs, [System.Text.UTF8Encoding]::new($false))
    Write-Host "Startup shortcut created: $StartupVbsPath"
}
'@
Write-Utf8File (Join-Path $StagingDir "create_desktop_shortcut.ps1") $ShortcutScript

$UsageGuidePath = Join-Path $StagingDir "USAGE_GUIDE.txt"
Require-File $UsageGuidePath "Usage guide"

$ForbiddenPatterns = @("*.cpp", "*.c", "*.h", "*.hpp", "*.py", "*.cmake", "CMakeLists.txt", "*.sln", "*.vcxproj", "*.lib", "*.pdb", "*.ilk", "*.pt")
$Forbidden = foreach ($Pattern in $ForbiddenPatterns) {
    Get-ChildItem -LiteralPath $StagingDir -Recurse -File -Filter $Pattern -ErrorAction SilentlyContinue
}
if ($Forbidden) {
    $Forbidden | Select-Object FullName
    throw "Forbidden source/build artifacts found in staging."
}
$ForbiddenPaths = @(
    (Join-Path $StagingDir "docs"),
    (Join-Path $StagingDir "AGENTS.md")
)
$ForbiddenPathHits = @($ForbiddenPaths | Where-Object { Test-Path -LiteralPath $_ })
if ($ForbiddenPathHits.Count -gt 0) {
    $ForbiddenPathHits | ForEach-Object { Write-Host "[Package] Forbidden package path: $_" }
    throw "Forbidden documentation or agent files found in staging."
}

New-Item -ItemType Directory -Force -Path $DistDir | Out-Null
Move-Item -LiteralPath $StagingDir -Destination $ReleaseDir
Compress-Archive -LiteralPath $ReleaseDir -DestinationPath $ZipPath -CompressionLevel Optimal -Force

Write-Host "[Package] Release directory: $ReleaseDir"
Write-Host "[Package] Release zip: $ZipPath"







