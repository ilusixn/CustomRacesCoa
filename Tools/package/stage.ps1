# Copies the release files (files.txt beside this script, paths relative to -Bin) plus -Extra into -Stage, keeping
# their folders; throws if any is missing, so a zip never ships without DXVK or a Streamline DLL.
param(
    [Parameter(Mandatory)][string]$Bin,
    [Parameter(Mandatory)][string]$Stage,
    [string[]]$Extra = @()
)

$ErrorActionPreference = 'Stop'
$files = @(Get-Content (Join-Path $PSScriptRoot 'files.txt') | ForEach-Object { $_.Trim() } |
    Where-Object { $_ -and -not $_.StartsWith('#') }) + $Extra
$missing = @($files | Where-Object { -not (Test-Path (Join-Path $Bin $_)) })

if ($missing.Count -gt 0) {
    throw "Missing release files in ${Bin}: $($missing -join ', ')"
}

if (Test-Path $Stage) {
    Remove-Item -Recurse -Force $Stage
}

foreach ($file in $files) {
    $dest = Join-Path $Stage $file
    New-Item -ItemType Directory -Force -Path (Split-Path $dest) | Out-Null
    Copy-Item (Join-Path $Bin $file) $dest
}

Write-Host "Staged $($files.Count) files in $Stage"
