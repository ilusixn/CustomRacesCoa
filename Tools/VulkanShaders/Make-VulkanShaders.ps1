# Builds VulkanShaders\ for one game folder: fetches glslang, runs translate_shaders.py --deploy.
# Usage: powershell -ExecutionPolicy Bypass -File Tools\VulkanShaders\Make-VulkanShaders.ps1 -Client "C:\Games\CoA"
param(
    [Parameter(Mandatory = $true)][string]$Client,
    [string]$GlslangVersion = '16.6.0'
)
$ErrorActionPreference = 'Stop'

if (-not (Test-Path (Join-Path $Client 'Data\common.MPQ'))) { throw "No Data\common.MPQ in $Client." }

$python = $null
foreach ($candidate in @('py', 'python', 'python3')) {
    $cmd = Get-Command $candidate -ErrorAction SilentlyContinue
    if ($cmd) { $python = $cmd.Source; break }
}
if (-not $python) { throw 'Python 3 is not installed (https://www.python.org/downloads/, tick "Add to PATH").' }
$pyArgs = @()
if ((Split-Path $python -Leaf) -ieq 'py.exe') { $pyArgs = @('-3') }

$tools = Join-Path $env:LOCALAPPDATA 'CoA64-tools'
$glslang = Join-Path $tools "glslang-$GlslangVersion\bin\glslang.exe"
if (-not (Test-Path $glslang)) {
    New-Item -ItemType Directory -Force -Path $tools | Out-Null
    $zip = Join-Path $tools "glslang-$GlslangVersion.zip"
    $url = "https://github.com/KhronosGroup/glslang/releases/download/$GlslangVersion/glslang-$GlslangVersion-windows-x86_64-release.zip"
    Write-Host "Downloading glslang $GlslangVersion ..."
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    Invoke-WebRequest -Uri $url -OutFile $zip -UseBasicParsing
    Expand-Archive -Path $zip -DestinationPath (Join-Path $tools "glslang-$GlslangVersion") -Force
    $found = Get-ChildItem (Join-Path $tools "glslang-$GlslangVersion") -Recurse -Filter 'glslang.exe' | Select-Object -First 1
    if (-not $found) { throw 'glslang.exe was not in the download.' }
    $glslang = $found.FullName
}

$script = Join-Path $PSScriptRoot 'translate_shaders.py'
Write-Host "Translating the shaders of $Client (a few minutes) ..."
& $python @pyArgs $script --client $Client --glslang $glslang --deploy
if ($LASTEXITCODE -ne 0) { throw "translate_shaders.py failed ($LASTEXITCODE)." }
Write-Host "Done: $Client\VulkanShaders"
