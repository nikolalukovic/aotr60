# Builds AotR60 (x86) with the Visual Studio 18 toolchain and runs the unit tests.
# Usage: .\build.ps1 [-Config Release|Debug]
param([ValidateSet("Release", "Debug")][string]$Config = "Release")
$ErrorActionPreference = "Stop"

$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
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
    Write-Host "Built $(Join-Path $PSScriptRoot "build\$Config\dinput8.dll")"
}
finally {
    Pop-Location
}
