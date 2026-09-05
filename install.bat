@echo off
REM Pasgen - Windows installer shim. Double-click this, or run it from cmd.
REM All arguments are forwarded to install.ps1.
setlocal
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0install.ps1" %*
set RC=%ERRORLEVEL%
if not "%RC%"=="0" echo.& echo Installation failed with exit code %RC%.
echo.
pause
exit /b %RC%
