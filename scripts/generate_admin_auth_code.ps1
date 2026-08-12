param(
    [Parameter(Mandatory = $true)]
    [string]$RequestFile,

    [string]$Output = "",

    [string]$KeyFile = ""
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$AppDir = Split-Path -Parent $ScriptDir
if ([string]::IsNullOrWhiteSpace($KeyFile)) {
    $KeyFile = Join-Path $AppDir "config\admin_auth.key"
}
if ([string]::IsNullOrWhiteSpace($Output)) {
    $Output = Join-Path $AppDir "config\admin_license.json"
}
if (-not (Test-Path -LiteralPath $KeyFile)) {
    throw "Missing admin authorization key: $KeyFile"
}
if (-not (Test-Path -LiteralPath $RequestFile)) {
    throw "Missing license request file: $RequestFile"
}

$ToolCandidates = @(
    (Join-Path $AppDir "build\Release\tankeye-admin-auth-code.exe"),
    (Join-Path $AppDir "build\Debug\tankeye-admin-auth-code.exe"),
    (Join-Path $AppDir "tankeye-admin-auth-code.exe")
)
$AuthTool = $ToolCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if (-not $AuthTool) {
    throw "tankeye-admin-auth-code.exe not found. Build target tankeye-admin-auth-code first."
}

$ToolRuntimePaths = @(
    (Split-Path -Parent $AuthTool),
    "D:\Qt\5.15.2\msvc2019_64\bin",
    (Join-Path $AppDir "_deps\opencv\opencv\build\x64\vc16\bin"),
    "D:\opencv\opencv-4.12.0\opencv\build\x64\vc16\bin"
) | Where-Object { Test-Path -LiteralPath $_ }
$env:Path = (($ToolRuntimePaths + @($env:Path)) -join ";")
$env:TANKEYE_ADMIN_AUTH_KEY_FILE = $KeyFile

& $AuthTool -RequestFile $RequestFile -Output $Output
exit $LASTEXITCODE
