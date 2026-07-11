@echo off
setlocal EnableDelayedExpansion

:: Pasgen - Windows Installer
:: Downloads the latest release and installs to %LOCALAPPDATA%\Pasgen

set REPO=YOUR_GITHUB_USERNAME/pasgen
set RELEASE_URL=https://github.com/%REPO%/releases/latest/download/pasgen-windows-x64.zip
set INSTALL_DIR=%LOCALAPPDATA%\Pasgen

echo === Pasgen Windows Installer ===
echo.

:: Create install directory
if not exist "%INSTALL_DIR%" mkdir "%INSTALL_DIR%"

:: Download using PowerShell (available on Windows 7+)
echo Downloading pasgen...
powershell -NoProfile -Command ^
    "try { Invoke-WebRequest -Uri '%RELEASE_URL%' -OutFile '%TEMP%\pasgen-windows.zip' -UseBasicParsing } catch { Write-Error $_.Exception.Message; exit 1 }"

if errorlevel 1 (
    echo ERROR: Download failed. Check your internet connection.
    pause
    exit /b 1
)

:: Extract
echo Extracting...
powershell -NoProfile -Command ^
    "Expand-Archive -Path '%TEMP%\pasgen-windows.zip' -DestinationPath '%INSTALL_DIR%' -Force"

del "%TEMP%\pasgen-windows.zip" 2>nul

:: Add to PATH (current user)
echo Adding to PATH...
powershell -NoProfile -Command ^
    "$p = [System.Environment]::GetEnvironmentVariable('PATH','User'); if ($p -notlike '*%INSTALL_DIR%*') { [System.Environment]::SetEnvironmentVariable('PATH', $p + ';%INSTALL_DIR%', 'User') }"

echo.
echo Pasgen installed to: %INSTALL_DIR%\pasgen.exe
echo.
echo You may need to restart your terminal for PATH changes to take effect.
echo Run: pasgen
echo.
pause
