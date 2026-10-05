# Installs the AotR60 proxy as rotwk\dinput8.dll. Nothing else in the game folder is touched.
# Re-run after an AotR update or after accepting the AotR launcher's PATCH (it deletes unknown files in rotwk\).
param([ValidateSet("Release", "Debug")][string]$Config = "Release")
$ErrorActionPreference = "Stop"

$source = Join-Path $PSScriptRoot "build\$Config\dinput8.dll"
$target = Join-Path (Split-Path $PSScriptRoot -Parent) "rotwk\dinput8.dll"
if (-not (Test-Path $source)) { throw "Build output not found: $source (run build.ps1 first)" }

function Test-IsAotR60([string]$path) {
    $bytes = [System.IO.File]::ReadAllBytes($path)
    return [System.Text.Encoding]::ASCII.GetString($bytes).Contains("AotR60")
}

if ((Test-Path $target) -and -not (Test-IsAotR60 $target)) {
    throw "$target exists and is not AotR60's proxy; refusing to overwrite it."
}
Copy-Item $source $target -Force
Write-Host "Installed $target"
