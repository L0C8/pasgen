<#
.SYNOPSIS
    Pasgen - out-of-the-box installer for Windows.

.DESCRIPTION
    Checks what is already on the machine, installs only the missing
    dependencies (Git, CMake, the MSVC C++ build tools via winget, and
    OpenSSL / SDL2 / Argon2 via vcpkg), builds pasgen from source, and
    installs the executable together with its DLLs and bundled fonts.

.EXAMPLE
    .\install.ps1
    .\install.ps1 -Prefix "C:\Tools\Pasgen"
    .\install.ps1 -NoDeps
    .\install.ps1 -Clean
    .\install.ps1 -Check
#>
[CmdletBinding()]
param(
    [string] $Prefix = "$env:LOCALAPPDATA\Programs\Pasgen",
    [string] $Triplet = 'x64-windows',
    [int]    $Jobs = 0,
    [switch] $NoDeps,
    [switch] $Clean,
    [switch] $Check
)

# Native commands (git, cmake, winget, vcpkg) are checked explicitly via
# $LASTEXITCODE below. 'Stop' is deliberately NOT used globally: on Windows
# PowerShell 5.1 it turns any stderr output from a native command into a
# terminating NativeCommandError, and git writes progress to stderr.
$ErrorActionPreference = 'Continue'

$RepoDir  = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildDir = Join-Path $RepoDir 'build'

function Say  ($m) { Write-Host "==> $m" -ForegroundColor White }
function Ok   ($m) { Write-Host "    [+] $m" -ForegroundColor Green }
function Miss ($m) { Write-Host "    [ ] $m - missing" -ForegroundColor Yellow }
function Note ($m) { Write-Host "    $m" -ForegroundColor DarkGray }
function Warn ($m) { Write-Host "warning: $m" -ForegroundColor Yellow }
function Die  ($m) { Write-Host "error: $m" -ForegroundColor Red; exit 1 }

# winget puts new tools on the machine PATH, but this process keeps the PATH it
# started with. Re-read it so freshly installed tools are usable right away.
function Update-SessionPath {
    $machine = [Environment]::GetEnvironmentVariable('Path', 'Machine')
    $user    = [Environment]::GetEnvironmentVariable('Path', 'User')
    $env:Path = ($machine, $user | Where-Object { $_ }) -join ';'
}

function Test-Command ($name) {
    return [bool](Get-Command $name -ErrorAction SilentlyContinue)
}

function Test-CMake {
    if (-not (Test-Command 'cmake')) { return $false }
    try {
        $line = (& cmake --version 2>$null | Select-Object -First 1)
        if ($line -match '(\d+)\.(\d+)') {
            $major = [int]$Matches[1]; $minor = [int]$Matches[2]
            return ($major -gt 3) -or ($major -eq 3 -and $minor -ge 16)
        }
    } catch { }
    return $false
}

function Get-VsWhere {
    $p = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path $p) { return $p }
    return $null
}

function Test-MsvcTools {
    $vswhere = Get-VsWhere
    if (-not $vswhere) { return $false }
    $found = & $vswhere -latest -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath 2>$null
    return -not [string]::IsNullOrWhiteSpace($found)
}

function Get-VcpkgRoot {
    if ($env:VCPKG_ROOT -and (Test-Path (Join-Path $env:VCPKG_ROOT 'vcpkg.exe'))) {
        return $env:VCPKG_ROOT
    }
    $local = Join-Path $env:LOCALAPPDATA 'vcpkg'
    if (Test-Path (Join-Path $local 'vcpkg.exe')) { return $local }
    return $null
}

function Install-WithWinget ($id, $label, $override) {
    if (-not (Test-Command 'winget')) {
        Die "winget is not available. Install 'App Installer' from the Microsoft Store, or install $label manually."
    }
    Say "Installing $label"
    $wgArgs = @('install','--id',$id,'--exact','--silent',
              '--accept-package-agreements','--accept-source-agreements')
    if ($override) { $wgArgs += @('--override', $override) }
    & winget @wgArgs
    # winget returns non-zero when a package is already present; that is fine.
    if ($LASTEXITCODE -ne 0) { Note "winget exited with $LASTEXITCODE (continuing)" }
    Update-SessionPath
}

Write-Host "=== Pasgen installer (Windows) ===" -ForegroundColor Cyan

Update-SessionPath

# --- dependency status --------------------------------------------------------
Say 'Checking dependencies'

$state = [ordered]@{
    'Git'                 = (Test-Command 'git')
    'CMake 3.16+'         = (Test-CMake)
    'MSVC C++ build tools'= (Test-MsvcTools)
    'vcpkg'               = ([bool](Get-VcpkgRoot))
}
foreach ($k in $state.Keys) { if ($state[$k]) { Ok $k } else { Miss $k } }

if ($Check) {
    if ($state.Values -contains $false) { Say 'Run without -Check to install what is missing.' }
    else { Say 'Everything needed is already installed.' }
    exit 0
}

# --- install missing tooling --------------------------------------------------
if ($NoDeps) {
    if ($state.Values -contains $false) { Warn '-NoDeps given but tooling is missing; the build may fail' }
} else {
    if (-not (Test-Command 'git')) { Install-WithWinget 'Git.Git' 'Git' $null }
    if (-not (Test-CMake))         { Install-WithWinget 'Kitware.CMake' 'CMake' $null }
    if (-not (Test-MsvcTools)) {
        Install-WithWinget 'Microsoft.VisualStudio.2022.BuildTools' 'MSVC C++ build tools' `
            '--quiet --wait --norestart --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended'
    }
    if (-not (Test-Command 'git')) { Die 'Git still not on PATH - open a new terminal and re-run this script.' }
    if (-not (Test-CMake))         { Die 'CMake still not on PATH - open a new terminal and re-run this script.' }
    if (-not (Test-MsvcTools))     { Die 'The MSVC C++ build tools are still missing - install the "Desktop development with C++" workload.' }
}

# --- vcpkg and the C libraries ------------------------------------------------
$VcpkgRoot = Get-VcpkgRoot
if (-not $VcpkgRoot) {
    if ($NoDeps) { Die 'vcpkg not found and -NoDeps was given.' }
    $VcpkgRoot = Join-Path $env:LOCALAPPDATA 'vcpkg'
    Say "Bootstrapping vcpkg into $VcpkgRoot"
    if (-not (Test-Path $VcpkgRoot)) {
        & git clone --depth 1 https://github.com/microsoft/vcpkg.git $VcpkgRoot
        if ($LASTEXITCODE -ne 0) { Die 'failed to clone vcpkg' }
    }
    & (Join-Path $VcpkgRoot 'bootstrap-vcpkg.bat') -disableMetrics
    if ($LASTEXITCODE -ne 0) { Die 'failed to bootstrap vcpkg' }
    [Environment]::SetEnvironmentVariable('VCPKG_ROOT', $VcpkgRoot, 'User')
    $env:VCPKG_ROOT = $VcpkgRoot
}
Ok "vcpkg at $VcpkgRoot"

$vcpkgExe = Join-Path $VcpkgRoot 'vcpkg.exe'
if (-not $NoDeps) {
    # vcpkg is itself idempotent: already-built ports are skipped.
    Say "Installing C libraries via vcpkg ($Triplet)"
    Note 'The first run compiles OpenSSL and SDL2 and can take a while.'
    & $vcpkgExe install "openssl:$Triplet" "sdl2:$Triplet" "argon2:$Triplet"
    if ($LASTEXITCODE -ne 0) { Die 'vcpkg failed to install the required libraries' }
}

# --- build --------------------------------------------------------------------
if ($Clean -and (Test-Path $BuildDir)) {
    Say "Cleaning $BuildDir"
    Remove-Item -Recurse -Force $BuildDir -ErrorAction Stop
}

if ($Jobs -le 0) { $Jobs = [Environment]::ProcessorCount }
$toolchain = Join-Path $VcpkgRoot 'scripts\buildsystems\vcpkg.cmake'

Say 'Configuring'
Note 'The first build downloads ImGui, nlohmann/json and portable-file-dialogs.'
& cmake -S $RepoDir -B $BuildDir -DCMAKE_TOOLCHAIN_FILE="$toolchain" -DVCPKG_TARGET_TRIPLET="$Triplet"
if ($LASTEXITCODE -ne 0) { Die 'CMake configuration failed' }

Say "Building with $Jobs parallel jobs"
& cmake --build $BuildDir --config Release --parallel $Jobs
if ($LASTEXITCODE -ne 0) { Die 'build failed' }

# --- install ------------------------------------------------------------------
Say "Installing to $Prefix"
& cmake --install $BuildDir --config Release --prefix $Prefix
if ($LASTEXITCODE -ne 0) { Die 'install step failed' }

# CMake's install rules cover the exe and the fonts but not the vcpkg runtime
# DLLs, which vcpkg drops next to the executable in the build tree.
$binDir = Join-Path $Prefix 'bin'
$dlls = Get-ChildItem (Join-Path $BuildDir 'Release') -Filter *.dll -ErrorAction SilentlyContinue
if ($dlls) {
    Copy-Item $dlls.FullName -Destination $binDir -Force -ErrorAction Stop
    Ok "copied $($dlls.Count) runtime DLL(s)"
}

# --- PATH ---------------------------------------------------------------------
$userPath = [Environment]::GetEnvironmentVariable('Path', 'User')
if (-not $userPath) { $userPath = '' }
if (($userPath -split ';') -notcontains $binDir) {
    Say 'Adding the install directory to your user PATH'
    $newPath = if ($userPath.TrimEnd(';')) { $userPath.TrimEnd(';') + ';' + $binDir } else { $binDir }
    [Environment]::SetEnvironmentVariable('Path', $newPath, 'User')
    Note 'Open a new terminal for the PATH change to take effect.'
}

Write-Host ''
Ok "pasgen installed to $binDir\pasgen.exe"
Note 'Run: pasgen'
Note 'Update later with: .\update.ps1'
