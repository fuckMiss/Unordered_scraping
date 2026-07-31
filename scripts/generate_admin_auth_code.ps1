param(
    [Parameter(Mandatory = $true)]
    [string]$MachineCode,

    [ValidateSet("INIT", "RESET")]
    [string]$Purpose = "INIT",

    [string]$KeyFile = "",
    [string]$Secret = ""
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$AppDir = Split-Path -Parent $ScriptDir
if ([string]::IsNullOrWhiteSpace($KeyFile)) {
    $KeyFile = Join-Path $AppDir "config\admin_auth.key"
}
$ToolCandidates = @(
    (Join-Path $AppDir "build\Release\tankeye-admin-auth-code.exe"),
    (Join-Path $AppDir "build\Debug\tankeye-admin-auth-code.exe"),
    (Join-Path $AppDir "tankeye-admin-auth-code.exe")
)
$AuthTool = $ToolCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if ($AuthTool -and [string]::IsNullOrWhiteSpace($Secret)) {
    $ToolRuntimePaths = @(
        (Split-Path -Parent $AuthTool),
        "D:\Qt\5.15.2\msvc2019_64\bin",
        (Join-Path $AppDir "_deps\opencv\opencv\build\x64\vc16\bin"),
        "D:\opencv\opencv-4.12.0\opencv\build\x64\vc16\bin"
    ) | Where-Object { Test-Path -LiteralPath $_ }
    $env:Path = (($ToolRuntimePaths + @($env:Path)) -join ";")
    $Args = @("-MachineCode", $MachineCode, "-Purpose", $Purpose)
    if (-not [string]::IsNullOrWhiteSpace($KeyFile)) {
        $env:TANKEYE_ADMIN_AUTH_KEY_FILE = $KeyFile
    }
    & $AuthTool @Args
    exit $LASTEXITCODE
}

function Normalize-Code([string]$Value) {
    return (($Value.ToUpperInvariant()) -replace '[^A-Z0-9]', '')
}

function Format-Code([string]$Hex, [int]$Length = 16) {
    $Text = $Hex.ToUpperInvariant().Substring(0, $Length)
    $Groups = New-Object System.Collections.Generic.List[string]
    for ($Index = 0; $Index -lt $Text.Length; $Index += 4) {
        $Groups.Add($Text.Substring($Index, [Math]::Min(4, $Text.Length - $Index)))
    }
    return ($Groups -join "-")
}

function Read-SecretFromFile([string]$Path) {
    if ([string]::IsNullOrWhiteSpace($Path) -or -not (Test-Path -LiteralPath $Path)) {
        return ""
    }
    $Text = (Get-Content -LiteralPath $Path -Raw -Encoding UTF8).Trim()
    foreach ($Line in ($Text -split "(`r`n|`n|`r)")) {
        $Trimmed = $Line.Trim()
        if ($Trimmed.StartsWith("secret=", [System.StringComparison]::OrdinalIgnoreCase)) {
            return $Trimmed.Substring("secret=".Length).Trim()
        }
        if ($Trimmed.StartsWith("private_key=", [System.StringComparison]::OrdinalIgnoreCase)) {
            return $Trimmed.Substring("private_key=".Length).Trim()
        }
    }
    return $Text
}

if ([string]::IsNullOrWhiteSpace($Secret)) {
    $Secret = Read-SecretFromFile $KeyFile
}

if ([string]::IsNullOrWhiteSpace($Secret)) {
    Write-Warning "Admin authorization secret is empty. Using development default secret. Create config\admin_auth.key before release."
    $Secret = "TankEye-Iris-Admin-Dev-Secret-v1"
}

$NormalizedMachine = Normalize-Code $MachineCode
$Payload = "$($Purpose.ToUpperInvariant())|$NormalizedMachine|$($Secret.Trim())"
$Bytes = [System.Text.Encoding]::UTF8.GetBytes($Payload)
$Sha = [System.Security.Cryptography.SHA256]::Create()
$Digest = $Sha.ComputeHash($Bytes)
$Hex = -join ($Digest | ForEach-Object { $_.ToString("x2") })
$Code = Format-Code $Hex

Write-Host "Purpose: $($Purpose.ToUpperInvariant())"
Write-Host "Machine: $MachineCode"
Write-Host "Code: $Code"
