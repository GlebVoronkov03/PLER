# Build pler-blender-2.0.zip with bundled Release CLI binaries.
# Usage: pwsh -File addons/blender/package_addon.ps1
param(
  [string]$Tag = "v2.0.0",
  [string]$Repo = "GlebVoronkov03/PLER"
)

$ErrorActionPreference = "Stop"
$Root = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$AddonSrc = Join-Path $Root "addons\blender\pler_metric"
$Stage = Join-Path $Root "dist\pler_blender_stage"
$BinRoot = Join-Path $Stage "pler_metric\bin"
$OutZip = Join-Path $Root "dist\pler-blender-2.0.zip"
$Tmp = Join-Path $Root "dist\_blender_pkg_tmp"

function Ensure-Dir($p) {
  New-Item -ItemType Directory -Force -Path $p | Out-Null
}

if (Test-Path $Stage) { Remove-Item -Recurse -Force $Stage }
if (Test-Path $Tmp) { Remove-Item -Recurse -Force $Tmp }
Ensure-Dir $Stage
Ensure-Dir $Tmp
Ensure-Dir (Join-Path $Root "dist")

# Copy Python addon (no bin/)
robocopy $AddonSrc (Join-Path $Stage "pler_metric") /E /XD bin __pycache__ /NFL /NDL /NJH /NJS /nc /ns /np | Out-Null
Ensure-Dir $BinRoot

Write-Host "Downloading Release $Tag assets..."
gh release download $Tag -R $Repo -D $Tmp --clobber `
  -p "pler-2.0-win64.zip" `
  -p "pler-2.0-linux-x64.tar.gz" `
  -p "pler-2.0-macos-arm64.zip" `
  -p "pler-2.0-macos-x64.zip"

function Copy-CliFromZip($zipPath, $destKey, $exeName) {
  $extract = Join-Path $Tmp "extract_$destKey"
  Ensure-Dir $extract
  if ($zipPath -like "*.tar.gz") {
    tar -xzf $zipPath -C $extract
  } else {
    Expand-Archive -Path $zipPath -DestinationPath $extract -Force
  }
  $pler = Get-ChildItem -Path $extract -Recurse -Filter $exeName | Select-Object -First 1
  if (-not $pler) { throw "Could not find $exeName in $zipPath" }
  $dest = Join-Path $BinRoot $destKey
  Ensure-Dir $dest
  Copy-Item $pler.FullName (Join-Path $dest $exeName) -Force
  # Shared libs next to CLI when present
  $binDir = $pler.Directory.FullName
  foreach ($pat in @("pler.dll", "libpler.so", "libpler.dylib", "cudart64_*.dll", "libcudart.so*")) {
    Get-ChildItem -Path $binDir -Filter $pat -ErrorAction SilentlyContinue | ForEach-Object {
      Copy-Item $_.FullName $dest -Force
    }
  }
  Write-Host "  staged $destKey/$exeName"
}

Copy-CliFromZip (Join-Path $Tmp "pler-2.0-win64.zip") "windows-x64" "pler.exe"
Copy-CliFromZip (Join-Path $Tmp "pler-2.0-linux-x64.tar.gz") "linux-x64" "pler"
Copy-CliFromZip (Join-Path $Tmp "pler-2.0-macos-arm64.zip") "macos-arm64" "pler"
Copy-CliFromZip (Join-Path $Tmp "pler-2.0-macos-x64.zip") "macos-x64" "pler"

if (Test-Path $OutZip) { Remove-Item -Force $OutZip }
Push-Location $Stage
Compress-Archive -Path "pler_metric" -DestinationPath $OutZip -Force
Pop-Location

Write-Host "Wrote $OutZip"
Get-Item $OutZip | Format-List Name, Length
