param(
    [string]$ShortcutName = "TankEye-Iris",
    [ValidateSet("Normal", "Maximized", "Fullscreen")]
    [string]$WindowMode = "Maximized",
    [int]$WindowWidth = 1280,
    [int]$WindowHeight = 720,
    [double]$UiScale = 0
)

$ErrorActionPreference = "Stop"

$AppDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$LaunchScript = Join-Path $AppDir "launch_tankeye.ps1"
$AppExe = Join-Path $AppDir "build_win_unit\Release\tankeye-openvino_qt_app.exe"

if (-not (Test-Path -LiteralPath $LaunchScript)) {
    throw "Launch script not found: $LaunchScript"
}
if (-not (Test-Path -LiteralPath $AppExe)) {
    throw "Application executable not found. Build and deploy first: $AppExe"
}

$DesktopDir = [Environment]::GetFolderPath("Desktop")
$ShortcutPath = Join-Path $DesktopDir "$ShortcutName.lnk"
$PowerShellExe = Join-Path $env:SystemRoot "System32\WindowsPowerShell\v1.0\powershell.exe"

$Arguments = @(
    "-NoProfile",
    "-ExecutionPolicy", "Bypass",
    "-File", "`"$LaunchScript`"",
    "-WindowMode", $WindowMode,
    "-WindowWidth", [string]$WindowWidth,
    "-WindowHeight", [string]$WindowHeight
)
if ($UiScale -gt 0) {
    $Arguments += @("-UiScale", [string]$UiScale)
}

$Shell = New-Object -ComObject WScript.Shell
$Shortcut = $Shell.CreateShortcut($ShortcutPath)
$Shortcut.TargetPath = $PowerShellExe
$Shortcut.Arguments = ($Arguments -join " ")
$Shortcut.WorkingDirectory = $AppDir
$Shortcut.IconLocation = "$AppExe,0"
$Shortcut.Description = "Start TankEye-Iris"
$Shortcut.Save()

Write-Host "Desktop shortcut created:"
Write-Host "  $ShortcutPath"
Write-Host "Target:"
Write-Host "  $PowerShellExe $($Shortcut.Arguments)"
