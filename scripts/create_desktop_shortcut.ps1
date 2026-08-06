param(
    [string]$ShortcutName = "TankEye-Iris",
    [ValidateSet("Normal", "Maximized", "Fullscreen")]
    [string]$WindowMode = "Maximized",
    [int]$WindowWidth = 1280,
    [int]$WindowHeight = 720,
    [double]$UiScale = 0,
    [int]$StartupDelaySeconds = 0,
    [switch]$NoAutoStart
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
$StartupDir = [Environment]::GetFolderPath("Startup")
$ShortcutPath = Join-Path $DesktopDir "$ShortcutName.lnk"
$StartupVbsPath = Join-Path $StartupDir "$ShortcutName.vbs"
$LegacyStartupShortcutPath = Join-Path $StartupDir "$ShortcutName.lnk"
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

if (-not $NoAutoStart) {
    $StartupDelaySeconds = [Math]::Max(0, [Math]::Min($StartupDelaySeconds, 600))
    Remove-Item -LiteralPath $LegacyStartupShortcutPath -Force -ErrorAction SilentlyContinue
    $StartupVbs = @"
Option Explicit

Dim shell, appDir, launchScript, command

Set shell = CreateObject("WScript.Shell")

appDir = "$($AppDir.Replace('"', '""'))"
launchScript = "$($LaunchScript.Replace('"', '""'))"
command = "powershell.exe -NoProfile -ExecutionPolicy Bypass -File " & _
          """" & launchScript & """" & " -StartupProfile AutoStart -StartupDelaySeconds $StartupDelaySeconds"

shell.CurrentDirectory = appDir
shell.Run command, 0, False
"@
    [System.IO.File]::WriteAllText($StartupVbsPath, $StartupVbs, [System.Text.UTF8Encoding]::new($false))
}

Write-Host "Desktop shortcut created:"
Write-Host "  $ShortcutPath"
if (-not $NoAutoStart) {
    Write-Host "Startup shortcut created:"
    Write-Host "  $StartupVbsPath"
}
Write-Host "Target:"
Write-Host "  $WScriptExe $($Shortcut.Arguments)"
