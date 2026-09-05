<#
.SYNOPSIS
    Pasgen - updater for Windows.

.DESCRIPTION
    Pulls the latest source, then hands off to install.ps1, which re-checks
    every dependency and installs anything the new revision needs before
    rebuilding. Because the hand-off happens after the pull, the freshly
    pulled install.ps1 is the one that runs, so a dependency added upstream is
    picked up on the same run. (A change to update.ps1 itself only takes
    effect the run after.)

.EXAMPLE
    .\update.ps1
    .\update.ps1 -Check
    .\update.ps1 -Force
#>
[CmdletBinding()]
param(
    [switch] $Check,
    [switch] $Force,
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]] $Passthru
)

# Native commands (git, cmake, winget, vcpkg) are checked explicitly via
# $LASTEXITCODE below. 'Stop' is deliberately NOT used globally: on Windows
# PowerShell 5.1 it turns any stderr output from a native command into a
# terminating NativeCommandError, and git writes progress to stderr.
$ErrorActionPreference = 'Continue'

$RepoDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $RepoDir

function Say  ($m) { Write-Host "==> $m" -ForegroundColor White }
function Ok   ($m) { Write-Host "    [+] $m" -ForegroundColor Green }
function Note ($m) { Write-Host "    $m" -ForegroundColor DarkGray }
function Warn ($m) { Write-Host "warning: $m" -ForegroundColor Yellow }
function Die  ($m) { Write-Host "error: $m" -ForegroundColor Red; exit 1 }

Write-Host "=== Pasgen updater (Windows) ===" -ForegroundColor Cyan

if (-not (Get-Command git -ErrorAction SilentlyContinue)) { Die 'git is not installed' }
& git rev-parse --git-dir *> $null
if ($LASTEXITCODE -ne 0) { Die "$RepoDir is not a git checkout - re-clone the repo to use the updater" }

$branch = (& git rev-parse --abbrev-ref HEAD).Trim()
if ($branch -eq 'HEAD') { Die "the checkout is in a detached HEAD state - run 'git switch main' first" }

& git rev-parse --abbrev-ref --symbolic-full-name '@{u}' *> $null
if ($LASTEXITCODE -ne 0) { Die "branch '$branch' has no upstream remote - nothing to update from" }

Say 'Fetching origin'
& git fetch --quiet --prune
if ($LASTEXITCODE -ne 0) { Die 'git fetch failed' }

$local  = (& git rev-parse HEAD).Trim()
$remote = (& git rev-parse '@{u}').Trim()
$base   = (& git merge-base HEAD '@{u}').Trim()

if ($local -eq $remote) {
    Ok "Already at the latest revision ($(& git rev-parse --short HEAD), branch $branch)"
    if ($Check) { exit 0 }
    if (-not $Force) {
        Note 'Nothing to pull. Use -Force to rebuild and reinstall anyway.'
        exit 0
    }
    Say 'Rebuilding anyway (-Force)'
} else {
    if ($local -ne $base) {
        Die "local commits on '$branch' diverge from the remote - resolve that by hand (git rebase / git reset)"
    }
    $behind = (& git rev-list --count "HEAD..@{u}").Trim()
    Say "$behind new commit(s) available on $branch"
    & git --no-pager log --oneline --no-decorate "HEAD..@{u}" | ForEach-Object { Write-Host "    $_" }

    if ($Check) { Note 'Run .\update.ps1 to apply.'; exit 0 }

    & git diff-index --quiet HEAD -- *> $null
    if ($LASTEXITCODE -ne 0) {
        Warn 'You have uncommitted changes; the pull is fast-forward only and will refuse to clobber them.'
    }

    Say 'Pulling'
    & git pull --ff-only
    if ($LASTEXITCODE -ne 0) { Die 'pull failed - commit or stash your local changes and retry' }
    Ok "Now at $(& git rev-parse --short HEAD)"
}

# install.ps1 re-runs the full dependency check, so a dependency introduced by
# the commits just pulled is detected and installed here.
Say 'Re-checking dependencies and rebuilding'
$installer = Join-Path $RepoDir 'install.ps1'
if (-not (Test-Path $installer)) { Die "install.ps1 is missing from $RepoDir" }
if ($Passthru) { & $installer @Passthru } else { & $installer }
exit $LASTEXITCODE
