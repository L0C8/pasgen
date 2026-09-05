@echo off
REM Pasgen - Windows updater shim. Double-click this, or run it from cmd.
REM All arguments are forwarded to update.ps1.
setlocal
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0update.ps1" %*
set RC=%ERRORLEVEL%
if not "%RC%"=="0" echo.& echo Update failed with exit code %RC%.
echo.
pause
exit /b %RC%
