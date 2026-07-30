param(
    [string]$BuildDir = "build",
    [string]$ReleaseName = "TankEye-Iris_1.1",
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

Require-File (Join-Path $BuildPath "tankeye-openvino_qt_app.exe") "Qt app executable"
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
Copy-File (Join-Path $BuildPath "tankeye-openvino_qt_app.exe") $StagingDir
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

Get-ChildItem -LiteralPath $BuildPath -File | Where-Object {
    $_.Extension -ieq ".dll" -or $_.Extension -ieq ".cti" -or $_.Extension -ieq ".ax" -or $_.Name -ieq "CommonParameters.ini"
} | ForEach-Object {
    Copy-File $_.FullName $StagingDir
}

foreach ($DirName in @("bearer", "iconengines", "imageformats", "platforms", "styles", "ThirdParty")) {
    Copy-DirectoryIfExists (Join-Path $BuildPath $DirName) (Join-Path $StagingDir $DirName)
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

$ConfigSource = Join-Path $BuildPath "config\tankeye.json"
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

Copy-DirectoryIfExists (Join-Path $BuildPath "calibration_profiles") (Join-Path $StagingDir "calibration_profiles")
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
Remove-Item -LiteralPath $OpenVinoCacheDir -Recurse -Force -ErrorAction SilentlyContinue

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

Remove-Item Env:\TANKEYE_OPENVINO_CACHE_DIR -ErrorAction SilentlyContinue
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
if ($RequestedDevice -eq "CPU") {
    exit (Invoke-TankEyeApp -SelectedDevice "CPU" -SelectedLogFile $LogFile -StartupGuardSeconds 0)
}

$GpuExitCode = Invoke-TankEyeApp -SelectedDevice "GPU" -SelectedLogFile $LogFile -StartupGuardSeconds 60
if ($GpuExitCode -eq 0) {
    exit 0
}

$CpuLogFile = Join-Path $LogDir "tankeye_$($LogStamp)_cpu_fallback.log"
Write-Warning "[TankEye] GPU startup failed with code $GpuExitCode. Falling back to CPU."
exit (Invoke-TankEyeApp -SelectedDevice "CPU" -SelectedLogFile $CpuLogFile -StartupGuardSeconds 0)
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
# TankEye-Iris 1.1 鐙珛杩愯鍖?
## 鍚姩

鍙屽嚮 `launch_tankeye_main_only.vbs`锛屾垨鍦?PowerShell 涓繍琛岋細

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1
```

娴嬭瘯 PLC 妯℃嫙鍜?CPU 鎺ㄧ悊锛?
```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1 -Device CPU -SimulatePlc
```

## 鍖呭唴鍐呭

- 涓荤▼搴忥細`tankeye-openvino_qt_app.exe`
- 妯″瀷锛歚models\weights\best_obb.*`銆乣models\weights\best_seg.*`
- 閰嶇疆锛歚config\tankeye.json`
- 鏍囧畾锛歚calibration_profiles\`銆乣calibration_output\`
- 婕旂ず鍥撅細`samples\images\2.jpg`
- 杩愯搴擄細Qt銆丱penCV銆丱penVINO銆乀BB銆丠ikrobot MVS Runtime
- 宸ュ叿锛歚nine_point_circle_picker.exe`銆乣create_desktop_shortcut.ps1`

## 鏂扮數鑴戝墠缃潯浠?
- Windows 64 浣嶃€?- CPU 鎺ㄧ悊鍙洿鎺ヨ繍琛屻€?- GPU 鎺ㄧ悊闇€瑕佺洰鏍囩數鑴戝畨瑁呭畼鏂?Intel 鏄惧崱椹卞姩銆?- 鍖呭唴甯?Hikrobot SDK 杩愯搴擄紱棣栨鎺ュ叆娴峰悍鐩告満鍓嶏紝鐩爣鐢佃剳浠嶉渶瀹夎瀹樻柟 MVS 鐩告満椹卞姩銆?
## 涓嶅寘鍚簮鐮?
姝よ繍琛屽寘涓嶅寘鍚?C/C++ 婧愮爜銆佸ご鏂囦欢銆丳ython 鑴氭湰銆丆Make 宸ョ▼銆佹祴璇曘€佽皟璇曠鍙枫€乣.lib` 鎴栬缁冭祫浜с€?'@
Write-Utf8File (Join-Path $StagingDir "README_RUNTIME.md") $Readme

$UsageGuideBase64 = "VGFua0V5ZS1JcmlzIOS9v+eUqOivtOaYjgo9PT09PT09PT09PT09PT09PT09PT0KCuS4gOOAgemmluasoeS9v+eUqAoxLiDlsIYgVGFua0V5ZS1JcmlzXzEuMS56aXAg6Kej5Y6L5Yiw5pys5Zyw56OB55uY77yM5L6L5aaCIEQ6XFRhbmtFeWUtSXJpc18xLjHjgIIKMi4g5LiN6KaB55u05o6l5Zyo5Y6L57yp5YyF5YaF6L+Q6KGM56iL5bqP44CCCjMuIOWPjOWHu+WMheagueebruW9leeahCBsYXVuY2hfdGFua2V5ZV9tYWluX29ubHkudmJz44CCCjQuIOesrOS4gOasoeWQr+WKqOS8muiHquWKqOWcqOW9k+WJjSBXaW5kb3dzIOeUqOaIt+ahjOmdouWIm+W7uiBUYW5rRXllLUlyaXMg5Zu+5qCH44CCCjUuIOWQjue7reWPr+WPjOWHu+ahjOmdouWbvuagh+WQr+WKqOOAggoK5LqM44CB5Zu+54mH5qOA5rWLCjEuIOWQr+WKqOeoi+W6j+WQjueCueWHu+KAnOWKoOi9veWbvueJh+KAneOAggoyLiDlj6/pgInmi6nljIXlhoUgc2FtcGxlc1xpbWFnZXNcMi5qcGcg5L2c5Li65ryU56S65Zu+77yM5oiW6YCJ5oup6Ieq5bex55qE5Zu+54mH44CCCjMuIOeCueWHu+KAnOW8gOWni+ajgOa1i+KAneOAggo0LiDnqIvluo/kvJrmmL7npLogT0JC44CBU0VH44CB5aS55Y+W5bCE57q/44CB5oqT5Y+W54K55Y+K5qOA5rWL57uT5p6c44CCCgrkuInjgIHnm7jmnLrmo4DmtYsKMS4g56Gu6K6k55S16ISR5bey5a6J6KOF5a6Y5pa5IEhpa3JvYm90IE1WUyDnm7jmnLrpqbHliqjjgIIKMi4g56Gu6K6k55u45py6572R5q615ZKM55S16ISR572R5Y2hIElQIOWPr+mAmuS/oeOAggozLiDlnKjlt6XnqIvorr7nva7kuK3loavlhpnnm7jmnLogSVDjgIHmm53lhYnlj4LmlbDlkoznjrDlnLrmoIflrprjgIIKNC4g54K55Ye74oCc5omT5byA55u45py64oCd77yM5YaN54K55Ye74oCc5byA5aeL5qOA5rWL4oCd44CCCgrlm5vjgIFQTEMKMS4g6buY6K6kIFBMQyDphY3nva7kvY3kuo4gY29uZmlnXHRhbmtleWUuanNvbuOAggoyLiDnjrDlnLrogZTosIPliY3noa7orqQgUExDIElQ44CB56uv5Y+j44CB5a+E5a2Y5Zmo5Zyw5Z2A44CB5Z2Q5qCH5pig5bCE5ZKM6ZmQ5L2N6K6+572u44CCCjMuIOmcgOimgeemu+e6v+mqjOivgeaXtu+8jOWPr+S9v+eUqOWQr+WKqOWRveS7pO+8mgogICBwb3dlcnNoZWxsLmV4ZSAtTm9Qcm9maWxlIC1FeGVjdXRpb25Qb2xpY3kgQnlwYXNzIC1GaWxlIC5cbGF1bmNoX3RhbmtleWUucHMxIC1EZXZpY2UgQ1BVIC1TaW11bGF0ZVBsYwo0LiBTaW11bGF0ZVBsYyDlj6rmqKHmi58gUExDIOivu+WGme+8jOS4jeS8mui/nuaOpeaIluWGmeWFpeeOsOWcuiBQTEPjgIIKCuS6lOOAgeaWsOeUteiEkeWJjee9ruadoeS7tgoxLiBXaW5kb3dzIDY0IOS9jeOAggoyLiBDUFUg5o6o55CG5LiN6ZyA6KaB5a6J6KOFIFB5dGhvbuOAgUNNYWtl44CBUXTjgIFPcGVuQ1Yg5oiWIE9wZW5WSU5P44CCCjMuIOS9v+eUqCBJbnRlbCBHUFUg5Yqg6YCf5pe277yM55uu5qCH55S16ISR6ZyA6KaB5a6J6KOF5a6Y5pa5IEludGVsIOaYvuWNoempseWKqOOAggo0LiDkvb/nlKjmtbflurfnm7jmnLrml7bvvIznm67moIfnlLXohJHpnIDopoHlronoo4XlrpjmlrkgTVZTIOebuOacuumpseWKqO+8m+WMheWGheW3suWMheWQq+eoi+W6j+i/kOihjOaJgOmcgOeahOa1t+W6tyBTREsgUnVudGltZeOAggoK5YWt44CB6YeN6KaB55uu5b2VCi0gbW9kZWxzXHdlaWdodHPvvJpPQkIg5ZKMIFNFRyDmqKHlnovjgIIKLSBjb25maWdcdGFua2V5ZS5qc29u77yaUExD44CB55u45py65ZKM5bel56iL6buY6K6k6YWN572u44CCCi0gY2FsaWJyYXRpb25fcHJvZmlsZXPvvJrkuZ3ngrnmoIflrprphY3nva7jgIIKLSBjYWxpYnJhdGlvbl9vdXRwdXTvvJrmoIflrprlt6XlhbfovpPlh7rjgIIKLSBsb2dz77ya56iL5bqP6L+Q6KGM5pel5b+X44CCCgrkuIPjgIHpl67popjmjpLmn6UKMS4g56iL5bqP5peg5rOV5ZCv5Yqo77ya5p+l55yLIGxvZ3Mg55uu5b2V5Lit5pyA5paw55qEIHRhbmtleWVfKi5sb2fjgIIKMi4g5om+5LiN5Yiw55u45py677ya5qOA5p+lIE1WUyDpqbHliqjjgIHnm7jmnLrkvpvnlLXjgIHnvZHmrrXlkoznm7jmnLogSVDjgIIKMy4gR1BVIOS4jeWPr+eUqO+8muS9v+eUqCBDUFUg5ZCv5Yqo77yM5oiW5a6J6KOF5a6Y5pa5IEludGVsIOaYvuWNoempseWKqOOAggo0LiBQTEMg5LiN5bqU6IGU5py65pe277ya5L2/55SoIC1TaW11bGF0ZVBsYyDlj4LmlbDov5vooYzmtYvor5XjgII="
$UsageGuidePath = Join-Path $StagingDir "USAGE_GUIDE.txt"
[System.IO.File]::WriteAllBytes($UsageGuidePath, [System.Convert]::FromBase64String($UsageGuideBase64))
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
    version = "1.1"
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







