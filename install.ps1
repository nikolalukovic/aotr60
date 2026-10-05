# Installs AotR60 (native 60 FPS for Age of the Ring) into the game folder this repository was cloned into.
#
#   <Age of the Ring folder>\aotr60\install.cmd          (double-click, or run from a console)
#   powershell -ExecutionPolicy Bypass -File install.ps1 [-Build]
#
# By default the prebuilt bin\dinput8.dll is installed. -Build compiles it from source first (needs Visual Studio
# with the C++ x86 tools). Only rotwk\dinput8.dll is written; the game itself is patched in memory at start-up.
# Re-run after an AotR update or after accepting the AotR launcher's PATCH (it deletes unknown files in rotwk\).
param(
    [switch]$Build,
    [ValidateSet("Release", "Debug")][string]$Config = "Release"
)
$ErrorActionPreference = "Stop"

$repo = $PSScriptRoot
$game = Split-Path $repo -Parent
$rotwk = Join-Path $game "rotwk"
$gameDat = Join-Path $rotwk "game.dat"
$target = Join-Path $rotwk "dinput8.dll"

if (-not (Test-Path $gameDat)) {
    throw "Age of the Ring not found: '$gameDat' does not exist.`nClone this repository into the Age of the Ring folder (the one that contains rotwk\ and aotr\), then run install again."
}

# The DLL refuses to patch an unknown build at run time; tell the user up front.
$knownBuilds = @{
    "CC08275D60FF8E3BFD4374C29D61304DEA8336E6DD00AB8ADD88B1DF95A705DC" = "Age of the Ring game.dat (supported)"
}
$hash = (Get-FileHash -Algorithm SHA256 $gameDat).Hash
if ($knownBuilds.ContainsKey($hash)) {
    Write-Host "Found $($knownBuilds[$hash])."
}
else {
    Write-Warning "rotwk\game.dat (SHA-256 $hash) is not the build AotR60 was made for. AotR60 will install, but it stays inactive (stock 30 FPS) until it is updated for this game version."
}

if ($Build) {
    & (Join-Path $repo "build.ps1") -Config $Config
    $source = Join-Path $repo "build\$Config\dinput8.dll"
}
else {
    $source = Join-Path $repo "bin\dinput8.dll"
}
if (-not (Test-Path $source)) { throw "AotR60 DLL not found: $source" }

function Test-IsAotR60([string]$path) {
    $bytes = [System.IO.File]::ReadAllBytes($path)
    return [System.Text.Encoding]::ASCII.GetString($bytes).Contains("AotR60")
}
if ((Test-Path $target) -and -not (Test-IsAotR60 $target)) {
    throw "$target exists and is not AotR60's DLL (another mod or tool uses dinput8.dll); refusing to overwrite it."
}
Copy-Item $source $target -Force
Write-Host "Installed AotR60: $target"
Write-Host "Start the game as usual. Settings and logs: $env:APPDATA\Age of the Ring\aotr60\"
