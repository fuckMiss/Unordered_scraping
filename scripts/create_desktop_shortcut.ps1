param(
    [string]$ShortcutName = "TankEye-Iris",
    [ValidateSet("Normal", "Maximized", "Fullscreen")]
    [string]$WindowMode = "Maximized",
    [int]$WindowWidth = 1280,
    [int]$WindowHeight = 720,
    [double]$UiScale = 0
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
if ((Split-Path -Leaf $ScriptDir) -ieq "scripts") {
    $AppDir = Split-Path -Parent $ScriptDir
} else {
    $AppDir = $ScriptDir
}
$LaunchScript = Join-Path $AppDir "launch_tankeye.ps1"
$MainLauncher = Join-Path $AppDir "launch_tankeye_main_only.vbs"
$AppExeCandidates = @(
    (Join-Path $AppDir "tankeye-openvino_qt_app.exe"),
    (Join-Path $AppDir "build\Release\tankeye-openvino_qt_app.exe")
)
$AppExe = $AppExeCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1

if (-not (Test-Path -LiteralPath $LaunchScript)) {
    throw "Launch script not found: $LaunchScript"
}
if (-not (Test-Path -LiteralPath $MainLauncher)) {
    throw "Main launcher not found: $MainLauncher"
}
if ([string]::IsNullOrWhiteSpace($AppExe)) {
    throw "Application executable not found. Checked: $($AppExeCandidates -join '; ')"
}

$DesktopDir = [Environment]::GetFolderPath("Desktop")
$ShortcutPath = Join-Path $DesktopDir "$ShortcutName.lnk"
$OldUrlPath = Join-Path $DesktopDir "$ShortcutName.url"
$WScriptExe = Join-Path $env:SystemRoot "System32\wscript.exe"

Remove-Item -LiteralPath $ShortcutPath -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $OldUrlPath -Force -ErrorAction SilentlyContinue

$Shell = New-Object -ComObject WScript.Shell
$Shortcut = $Shell.CreateShortcut($ShortcutPath)
$Shortcut.TargetPath = $WScriptExe
$Shortcut.Arguments = "`"$MainLauncher`""
$Shortcut.WorkingDirectory = $AppDir
$Shortcut.IconLocation = "$AppExe,0"
$Shortcut.Description = "Start TankEye-Iris"
$Shortcut.Save()

Write-Host "Desktop shortcut created:"
Write-Host "  $ShortcutPath"
Write-Host "Target:"
Write-Host "  $WScriptExe $($Shortcut.Arguments)"
