# Stage Release natives into pler_metric/_native/<platform>/ and build a wheel for this OS.
# Usage: powershell -File bindings/python/build_wheels.ps1 [-Tag v2.0.0]
param(
  [string]$Tag = "v2.0.0",
  [string]$Repo = "GlebVoronkov03/PLER",
  [string]$Platform = ""
)

$ErrorActionPreference = "Stop"
$Root = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$Pkg = Join-Path $Root "bindings\python"
$NativeRoot = Join-Path $Pkg "pler_metric\_native"
$Tmp = Join-Path $Root "dist\_py_wheel_tmp"
$Out = Join-Path $Root "dist\wheels"

function Ensure-Dir($p) { New-Item -ItemType Directory -Force -Path $p | Out-Null }

function Detect-Platform {
  if ($env:PLER_PLATFORM) { return $env:PLER_PLATFORM }
  if ($IsWindows -or $env:OS -match "Windows") { return "windows-x64" }
  $uname = uname -s 2>$null
  $mach = uname -m 2>$null
  if ($uname -eq "Linux") { return "linux-x64" }
  if ($uname -eq "Darwin") {
    if ($mach -eq "arm64") { return "macos-arm64" }
    return "macos-x64"
  }
  throw "Cannot detect platform"
}

if (-not $Platform) { $Platform = Detect-Platform }

if (Test-Path $Tmp) { Remove-Item -Recurse -Force $Tmp }
Ensure-Dir $Tmp
Ensure-Dir $Out
Ensure-Dir (Join-Path $NativeRoot $Platform)

Write-Host "Platform=$Platform Tag=$Tag"

# Prefer CUDA linux asset when present
$asset = switch ($Platform) {
  "windows-x64" { "pler-2.0-win64.zip" }
  "linux-x64" {
    $cuda = "pler-2.0-linux-x64-cuda.tar.gz"
    $check = gh api "repos/$Repo/releases/tags/$Tag" --jq ".assets[].name" 2>$null
    if ($check -match [regex]::Escape($cuda)) { $cuda } else { "pler-2.0-linux-x64.tar.gz" }
  }
  "macos-arm64" { "pler-2.0-macos-arm64.zip" }
  "macos-x64" { "pler-2.0-macos-x64.zip" }
  default { throw "Unknown platform $Platform" }
}

Write-Host "Downloading $asset ..."
gh release download $Tag -R $Repo -D $Tmp --clobber -p $asset

$extract = Join-Path $Tmp "extract"
Ensure-Dir $extract
$archive = Join-Path $Tmp $asset
if ($asset -like "*.tar.gz") {
  tar -xzf $archive -C $extract
} else {
  Expand-Archive -Path $archive -DestinationPath $extract -Force
}

$dest = Join-Path $NativeRoot $Platform
Get-ChildItem -Path $dest -Force -ErrorAction SilentlyContinue | Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
Ensure-Dir $dest

$cliName = if ($Platform -eq "windows-x64") { "pler.exe" } else { "pler" }
$libName = switch -Wildcard ($Platform) {
  "windows*" { "pler.dll" }
  "macos*" { "libpler.dylib" }
  default { "libpler.so" }
}

$cli = Get-ChildItem -Path $extract -Recurse -Filter $cliName | Select-Object -First 1
$lib = Get-ChildItem -Path $extract -Recurse -Filter $libName | Select-Object -First 1
if (-not $cli) { throw "CLI $cliName not found in $asset" }
if (-not $lib) { throw "Library $libName not found in $asset" }
Copy-Item $cli.FullName (Join-Path $dest $cliName) -Force
Copy-Item $lib.FullName (Join-Path $dest $libName) -Force
# Optional CUDA runtime next to lib
$binDir = $cli.Directory.FullName
Get-ChildItem -Path $binDir -Filter "cudart64_*.dll" -ErrorAction SilentlyContinue | Copy-Item -Destination $dest -Force
Get-ChildItem -Path $binDir -Filter "libcudart.so*" -ErrorAction SilentlyContinue | Copy-Item -Destination $dest -Force

Write-Host "Staged $dest"
Push-Location $Pkg
python -m pip install -q build wheel
python -m build --wheel -o $Out
Pop-Location
Write-Host "Wheels in $Out"
Get-ChildItem $Out -Filter "*.whl" | Format-Table Name, Length
