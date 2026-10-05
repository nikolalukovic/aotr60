@echo off
setlocal
rem Usage: analyze.cmd <binary> <outDir> <projectDir> <projectName>
if not defined JAVA_HOME (echo Set JAVA_HOME to a JDK 21. & exit /b 1)
set "PATH=%JAVA_HOME%\bin;%PATH%"
if not defined GHIDRA_HOME (echo Set GHIDRA_HOME to the Ghidra 11.3.2 folder. & exit /b 1)
set "GH=%GHIDRA_HOME%"
call "%GH%\support\launch.bat" fg jdk Ghidra-Headless "20G" "-XX:ParallelGCThreads=8 -XX:CICompilerCount=4" ghidra.app.util.headless.AnalyzeHeadless "%~3" "%~4" -import "%~f1" -overwrite -scriptPath "%~dp0." -postScript DumpAllParallel.java "%~2" 120 20
