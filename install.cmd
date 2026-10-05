@echo off
rem Installs AotR60 into the Age of the Ring folder this repository was cloned into (see README.md).
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0install.ps1" %*
if errorlevel 1 (echo. & echo Installation failed. & pause & exit /b 1)
echo.
pause
