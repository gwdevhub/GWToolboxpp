<#
.SYNOPSIS
  Fetch upstream GWToolbox++, rebase the current branch onto it, rebuild the fork DLL.

.DESCRIPTION
  Keeps fork-specific commits (Quest Tracker, etc.) on top of the latest gwdevhub/GWToolboxpp
  master. Close Guild Wars before running - the linker cannot replace GWToolboxdll.dll while
  the game holds the file.

.EXAMPLE
  .\scripts\sync-upstream-rebase.ps1

.EXAMPLE
  .\scripts\sync-upstream-rebase.ps1 -Continue   # after resolving rebase conflicts

.EXAMPLE
  .\scripts\sync-upstream-rebase.ps1 -Abort      # cancel an in-progress rebase
#>
[CmdletBinding()]
param(
    [string]$Remote = "upstream",
    [string]$Branch = "master",
    [string]$Configuration = "RelWithDebInfo",
    [switch]$SkipBuild,
    [switch]$SkipTests,
    [switch]$SkipConfigure,
    [switch]$NoAutoStash,
    [switch]$Continue,
    [switch]$Abort
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

function Write-Step([string]$Message) {
    Write-Host ""
    Write-Host "==> $Message" -ForegroundColor Cyan
}

function Get-CMakeExe {
    $cmd = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }

    $candidates = @(
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "C:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    )
    foreach ($path in $candidates) {
        if (Test-Path $path) { return $path }
    }
    throw "cmake not found. Add Visual Studio CMake to PATH or install CMake."
}

function Get-MSBuildExe {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $found = & $vswhere -latest -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
        if ($found) { return $found }
    }
    $fallback = "C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\MSBuild.exe"
    if (Test-Path $fallback) { return $fallback }
    throw "MSBuild not found. Open a Visual Studio Developer shell or install VS Build Tools."
}

function Test-GwRunning {
    return $null -ne (Get-Process -Name "Gw" -ErrorAction SilentlyContinue | Select-Object -First 1)
}

function Invoke-RebaseContinue {
    Write-Step "Continuing rebase..."
    git rebase --continue
    if ($LASTEXITCODE -ne 0) {
        throw "git rebase --continue failed. Resolve remaining conflicts, then run with -Continue again."
    }
}

function Invoke-RebaseAbort {
    Write-Step "Aborting rebase..."
    git rebase --abort
    Write-Host "Rebase aborted. Branch restored to pre-rebase state." -ForegroundColor Yellow
    exit 0
}

function Invoke-UpstreamRebase {
    param(
        [string]$RemoteName,
        [string]$BranchName,
        [bool]$UseAutoStash
    )

    $upstreamRef = "$RemoteName/$BranchName"

    if (-not (git remote get-url $RemoteName 2>$null)) {
        throw "Git remote '$RemoteName' is missing. Add upstream: git remote add upstream https://github.com/gwdevhub/GWToolboxpp.git"
    }

    if (Test-GwRunning) {
        Write-Host "Warning: Gw.exe is running. Close Guild Wars before rebuild if the linker fails (LNK1168)." -ForegroundColor Yellow
    }

    $rebaseDir = Join-Path $root ".git\rebase-merge"
    $rebaseApplyDir = Join-Path $root ".git\rebase-apply"
    if ((Test-Path $rebaseDir) -or (Test-Path $rebaseApplyDir)) {
        throw "A rebase is already in progress. Resolve conflicts, then run: .\scripts\sync-upstream-rebase.ps1 -Continue`nOr cancel with: .\scripts\sync-upstream-rebase.ps1 -Abort"
    }

    Write-Step "Fetching $RemoteName..."
    git fetch $RemoteName --prune
    if ($LASTEXITCODE -ne 0) { throw "git fetch $RemoteName failed." }

    if (-not (git rev-parse --verify "$upstreamRef^{commit}" 2>$null)) {
        throw "Upstream ref '$upstreamRef' not found after fetch."
    }

    $localBranch = (git branch --show-current).Trim()
    if (-not $localBranch) {
        throw "Detached HEAD - checkout your fork branch first (e.g. feature/quest-tracker-phase-2-persistence)."
    }

    $behind = [int](git rev-list --count "HEAD..$upstreamRef")
    $ahead = [int](git rev-list --count "$upstreamRef..HEAD")
    Write-Host "Branch: $localBranch"
    Write-Host "Upstream: $upstreamRef  $(git log -1 --oneline $upstreamRef)"
    Write-Host "Fork commits ahead of upstream: $ahead; upstream commits not in fork: $behind"

    if ($behind -eq 0 -and $ahead -eq 0) {
        Write-Host "Already aligned with $upstreamRef - nothing to rebase." -ForegroundColor Green
        return
    }

    if ($behind -eq 0) {
        Write-Host "Fork is up to date with upstream; no new upstream commits to integrate." -ForegroundColor Green
        return
    }

    Write-Step "Rebasing $localBranch onto $upstreamRef..."
    $rebaseArgs = @("rebase", $upstreamRef)
    if ($UseAutoStash) {
        $rebaseArgs += "--autostash"
    }
    & git @rebaseArgs
    if ($LASTEXITCODE -ne 0) {
        Write-Host ""
        Write-Host "Rebase stopped with conflicts." -ForegroundColor Red
        Write-Host "1. Fix conflict markers in the listed files"
        Write-Host "2. git add (each resolved file)"
        Write-Host "3. .\scripts\sync-upstream-rebase.ps1 -Continue"
        Write-Host ""
        Write-Host "To cancel: .\scripts\sync-upstream-rebase.ps1 -Abort"
        exit 1
    }

    Write-Host "Rebase complete. Fork commits now sit on latest upstream." -ForegroundColor Green
}

function Invoke-Configure {
    param([string]$CMakeExe)
    Write-Step "Configuring CMake (vcpkg preset)..."
    & $CMakeExe --preset vcpkg
    if ($LASTEXITCODE -ne 0) { throw "cmake --preset vcpkg failed." }
}

function Invoke-Build {
    param(
        [string]$CMakeExe,
        [string]$MSBuildExe,
        [string]$Config
    )

    $proj = Join-Path $root "build\GWToolboxdll\GWToolboxdll.vcxproj"
    if (-not (Test-Path $proj)) {
        throw "Missing $proj - cmake configure did not generate the GWToolboxdll project."
    }

    Write-Step "Building GWToolboxdll ($Config)..."
    & $MSBuildExe $proj /p:Configuration=$Config /m /v:minimal
    if ($LASTEXITCODE -ne 0) { throw "GWToolboxdll build failed." }

    $dll = Join-Path $root "bin\$Config\GWToolboxdll.dll"
    if (-not (Test-Path $dll)) {
        throw "Build finished but $dll was not produced."
    }
    Get-Item $dll | Format-List FullName, LastWriteTime, Length
}

function Invoke-QuestProgressTests {
    param(
        [string]$CMakeExe,
        [string]$Config
    )

    Write-Step "Building QuestProgressTests..."
    & $CMakeExe --build (Join-Path $root "build") --config $Config --target QuestProgressTests -j 8
    if ($LASTEXITCODE -ne 0) { throw "QuestProgressTests build failed." }

    $exe = Join-Path $root "bin\$Config\QuestProgressTests.exe"
    if (-not (Test-Path $exe)) {
        throw "QuestProgressTests.exe not found at $exe"
    }

    Write-Step "Running QuestProgressTests..."
    & $exe
    if ($LASTEXITCODE -ne 0) { throw "QuestProgressTests reported failures." }
}

if ($Abort) {
    Invoke-RebaseAbort
}

if ($Continue) {
    Invoke-RebaseContinue
}
else {
    Invoke-UpstreamRebase -RemoteName $Remote -BranchName $Branch -UseAutoStash:(-not $NoAutoStash)
}

if ($SkipBuild) {
    Write-Host "SkipBuild set - done (no compile)." -ForegroundColor Yellow
    exit 0
}

$cmake = Get-CMakeExe
$msbuild = Get-MSBuildExe

if (-not $SkipConfigure) {
    Invoke-Configure -CMakeExe $cmake
}

Invoke-Build -CMakeExe $cmake -MSBuildExe $msbuild -Config $Configuration

if (-not $SkipTests) {
    Invoke-QuestProgressTests -CMakeExe $cmake -Config $Configuration
}

Write-Host ""
Write-Host "Done. Restart GWToolbox / Guild Wars to load the new DLL." -ForegroundColor Green
