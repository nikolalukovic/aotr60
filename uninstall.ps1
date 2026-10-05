# Removes the AotR60 proxy (rotwk\dinput8.dll). Settings and logs in %APPDATA%\Age of the Ring\aotr60 are kept.
$ErrorActionPreference = "Stop"

$target = Join-Path (Split-Path $PSScriptRoot -Parent) "rotwk\dinput8.dll"
if (-not (Test-Path $target)) {
    Write-Host "Not installed."
    return
}
$text = [System.Text.Encoding]::ASCII.GetString([System.IO.File]::ReadAllBytes($target))
if (-not $text.Contains("AotR60")) {
    throw "$target is not AotR60's proxy; leaving it alone."
}
Remove-Item $target
Write-Host "Removed $target"
