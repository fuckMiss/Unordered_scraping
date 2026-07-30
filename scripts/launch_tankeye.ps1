param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$RemainingArgs
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$AppDir = Split-Path -Parent $ScriptDir
$RootLauncher = Join-Path $AppDir "launch_tankeye.ps1"

if (-not (Test-Path -LiteralPath $RootLauncher)) {
    throw "Root launcher not found: $RootLauncher"
}

& $RootLauncher @RemainingArgs
exit $LASTEXITCODE
