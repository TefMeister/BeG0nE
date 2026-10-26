# run_testapp.ps1 -- the dev PC's end-to-end check: our test program (xr_testapp.exe, D3D12) on the OpenXR Simulator,
# with Flickers BeG0nE in between, either as the OpenXR layer or as the REFramework plugin; then prints its log.
# Usage: run_testapp.ps1 -Loader <folder holding openxr_loader.dll> -Simulator <OpenXR-Simulator folder>
#                        [-Seconds 10] [-Sessions 1] [-Shell layer|plugin|plugin-late] [-Settings "on = 2`nfake = 1"]
#                        [-KeepDumps]  (leave earlier dumps in place: new ones must be numbered after them)
param(
    [Parameter(Mandatory = $true)][string]$Loader,
    [Parameter(Mandatory = $true)][string]$Simulator,
    [int]$Seconds = 10,
    [int]$Sessions = 1,
    [switch]$KeepDumps,
    [ValidateSet("layer", "plugin", "plugin-late")][string]$Shell = "layer",
    [string]$Settings = "on = 1"
)
$root = Resolve-Path (Join-Path $PSScriptRoot "..")
$dist = Join-Path $root "dist"
$app = Join-Path $root "build\testapp"
Copy-Item (Join-Path $Loader "openxr_loader.dll") $app -Force
$env:XR_RUNTIME_JSON = Join-Path $Simulator "openxr_simulator.json"
Remove-Item Env:XR_API_LAYER_PATH, Env:XR_ENABLE_API_LAYERS -ErrorAction SilentlyContinue
$args = "--seconds $Seconds --sessions $Sessions"
if ($Shell -eq "layer") {
    $dataDir = $dist
    $env:XR_API_LAYER_PATH = "$dist"
    $env:XR_ENABLE_API_LAYERS = "XR_APILAYER_BEGONE_flickers"
} else {
    $dataDir = Join-Path $app "reframework\data\flickers"   # where the plugin keeps its files, beside the game's exe
    New-Item -ItemType Directory -Force $dataDir | Out-Null
    $args += " --plugin `"$(Join-Path $dist 'flickers_begone.dll')`""
    if ($Shell -eq "plugin-late") { $args += " --late" }
}
if (-not $KeepDumps) { Get-ChildItem $dataDir -Directory -Filter "flickers-dump-*" -ErrorAction SilentlyContinue | Remove-Item -Recurse -Force }
Set-Content -Path (Join-Path $dataDir "flickers_begone.txt") -Value $Settings -Encoding ascii
$p = Start-Process -FilePath (Join-Path $app "xr_testapp.exe") -ArgumentList $args -WorkingDirectory $app -PassThru `
    -RedirectStandardOutput (Join-Path $app "out.txt") -WindowStyle Minimized
if (-not $p.WaitForExit(($Seconds * $Sessions + 25) * 1000)) { $p | Stop-Process -Force; Write-Output "TEST PROGRAM HUNG (stopped)" }
Write-Output "== test program (exit $($p.ExitCode))"; Get-Content (Join-Path $app "out.txt")
Write-Output "== flickers_begone.log"; Get-Content (Join-Path $dataDir "flickers_begone.log") -ErrorAction SilentlyContinue
Get-ChildItem $dataDir -Directory -Filter "flickers-dump-*" -ErrorAction SilentlyContinue | ForEach-Object {
    Write-Output "== $($_.Name): $((Get-ChildItem $_.FullName -Filter *.ppm).Count) pictures" }
