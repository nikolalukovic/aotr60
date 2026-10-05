# Builds AotR60 (x86) with the Visual Studio C++ toolchain and runs the unit tests.
# Usage: .\build.ps1 [-Config Release|Debug] [-Publish]
#   -Publish  copies the Release DLL to bin\dinput8.dll (the prebuilt DLL that install.cmd installs).
param(
    [ValidateSet("Release", "Debug")][string]$Config = "Release",
    [switch]$Publish
)
$ErrorActionPreference = "Stop"

$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { throw "Visual Studio was not found (needed only to build from source; install.cmd uses the prebuilt bin\dinput8.dll)." }
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw "Visual Studio with the C++ x86/x64 tools was not found." }
$cmakeBin = Join-Path $vs "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"

Push-Location $PSScriptRoot
try {
    & "$cmakeBin\cmake.exe" --preset x86
    if ($LASTEXITCODE) { throw "configure failed" }
    & "$cmakeBin\cmake.exe" --build --preset $Config.ToLower()
    if ($LASTEXITCODE) { throw "build failed" }
    & "$cmakeBin\ctest.exe" --test-dir build -C $Config --output-on-failure
    if ($LASTEXITCODE) { throw "tests failed" }
    $dll = Join-Path $PSScriptRoot "build\$Config\dinput8.dll"
    Write-Host "Built $dll"
    if ($Publish) {
        if ($Config -ne "Release") { throw "-Publish needs a Release build" }
        New-Item -ItemType Directory -Force (Join-Path $PSScriptRoot "bin") | Out-Null
        Copy-Item $dll (Join-Path $PSScriptRoot "bin\dinput8.dll") -Force
        Write-Host "Published bin\dinput8.dll"
    }
}
finally {
    Pop-Location
}
