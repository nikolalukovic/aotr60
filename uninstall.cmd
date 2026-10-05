@echo off
rem Removes AotR60 (rotwk\dinput8.dll). Settings and logs in %APPDATA%\Age of the Ring\aotr60 are kept.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0uninstall.ps1" %*
if errorlevel 1 (echo. & echo Uninstall failed. & pause & exit /b 1)
echo.
pause
