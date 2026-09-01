<#
  build-xwin.ps1 — Windows host driver for the clang + xwin cross-build.

  PowerShell port of scripts/build-xwin.sh, driving the same Linux container
  (scripts/xwin/Dockerfile) through Docker Desktop. Output is bit-identical to the Linux
  path and to CI, which is the reason to use this on Windows at all: an ordinary Windows
  dev build should go through Visual Studio or build-clang.bat instead. Reach for this to
  reproduce a CI result, or to check a change builds clean under clang before pushing.

  Requires Docker Desktop with Linux containers.

      .\scripts\build-xwin.ps1                              # GWToolboxdll, RelWithDebInfo
      .\scripts\build-xwin.ps1 -Config Debug -Target all
      .\scripts\build-xwin.ps1 -Shell                       # poke around in the container
      .\scripts\build-xwin.ps1 -RebuildImage                # after editing the Dockerfile
#>

[CmdletBinding()]
param(
  [ValidateSet('Debug', 'RelWithDebInfo', 'Release')] [string]$Config = 'RelWithDebInfo',
  [string]$Target = 'GWToolboxdll',
  [string[]]$CMakeArg = @(),
  [int]$Jobs = 0,
  [switch]$RebuildImage,
  [switch]$Shell
)

# NOT 'Stop': Windows PowerShell 5.1 surfaces native-command stderr as an ErrorRecord, which
# 'Stop' escalates into a terminating error - so `docker image inspect` on a missing image,
# the normal first-run case, would abort the script. Every failure below is raised explicitly
# from a $LASTEXITCODE check instead, and `throw` terminates regardless of this preference.
$ErrorActionPreference = 'Continue'
Set-StrictMode -Version Latest

$ImageName = 'gwtoolboxpp-xwin'
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..') -ErrorAction Stop).Path
$XwinDir = Join-Path $PSScriptRoot 'xwin'

if (-not (Get-Command docker -ErrorAction SilentlyContinue)) {
  throw 'docker is required but was not found in PATH. Install Docker Desktop and enable Linux containers.'
}

# StrictMode makes reading an unset $LASTEXITCODE fatal, and it stays unset until a native
# command has actually run to completion.
$global:LASTEXITCODE = 0

# A Windows daemon cannot run this image at all; failing here beats a confusing build error.
# Collect the whole output before picking a line: piping into Select-Object -First stops the
# native command early, which leaves $LASTEXITCODE unset.
$dockerVersionOutput = @(docker version --format '{{.Server.Os}}' 2>&1)
if ($LASTEXITCODE -ne 0) {
  throw 'Could not talk to the Docker daemon. Is Docker Desktop running?'
}
$dockerOs = ($dockerVersionOutput | ForEach-Object { "$_".Trim() } | Where-Object { $_ } | Select-Object -First 1)
if ($dockerOs -and $dockerOs -ne 'linux') {
  throw "Docker is in $dockerOs-container mode; this image is Linux. Switch Docker Desktop to Linux containers."
}

# The bind-mounted .sh files run as-is inside the container, so CRLF endings break the
# shebang. .gitattributes pins them to LF, but a checkout predating it (or an editor that
# rewrote one) still bites -- and the resulting "bad interpreter" is not an obvious symptom.
$crlfScripts = Get-ChildItem -Path $XwinDir -Filter '*.sh' -ErrorAction Stop | Where-Object {
  [System.IO.File]::ReadAllBytes($_.FullName) -contains 13
}
if ($crlfScripts) {
  $names = ($crlfScripts | ForEach-Object { $_.Name }) -join ', '
  throw "These container scripts have CRLF line endings and will fail inside the container: $names. " +
        "Fix with: git add --renormalize . (or re-clone with .gitattributes in place)."
}

# Test the exit code, not the output: a missing image still prints "[]" on stdout, which is
# truthy, so an output test would silently skip building the image on a first run.
docker image inspect $ImageName 2>&1 | Out-Null
$imageExists = ($LASTEXITCODE -eq 0)

if ($RebuildImage -or -not $imageExists) {
  Write-Host "[build-xwin] building docker image '$ImageName' (this downloads the MSVC + Windows SDK headers/libs and builds vkd3d; expect it to take a while the first time)..."
  $buildArgs = @('build')
  if ($RebuildImage) { $buildArgs += '--no-cache' }
  $buildArgs += @('-t', $ImageName, '-f', (Join-Path $XwinDir 'Dockerfile'), $XwinDir)
  docker @buildArgs
  if ($LASTEXITCODE -ne 0) { throw "docker build failed with exit code $LASTEXITCODE" }
}

$runArgs = @('run', '--rm')
if ([Environment]::UserInteractive -and -not [Console]::IsInputRedirected) { $runArgs += '-it' }
# Forward slashes: Docker Desktop accepts a Windows path for a bind mount, but "C:/repo"
# is handled far more consistently than "C:\repo" across its backends.
$mountPath = $RepoRoot -replace '\\', '/'
$runArgs += @(
  '-v', "$($mountPath):/src"
  '-w', '/src'
  '-e', "CONFIG=$Config"
  '-e', "TARGET=$Target"
  '-e', "CMAKE_ARGS=$($CMakeArg -join ' ')"
)
# Bind mounts from a Windows host have no Linux ownership to hand back, so unlike the .sh
# there is deliberately no HOST_UID/HOST_GID and no chown pass.
if ($Jobs -gt 0) { $runArgs += @('-e', "JOBS=$Jobs") }
$runArgs += $ImageName

if ($Shell) {
  docker @runArgs bash
  exit $LASTEXITCODE
}

Write-Host "[build-xwin] configuring (preset: xwin, config: $Config)..."
docker @runArgs /src/scripts/xwin/build-in-container.sh
if ($LASTEXITCODE -ne 0) { throw "build failed with exit code $LASTEXITCODE" }

Write-Host "[build-xwin] done; artefacts are in $(Join-Path $RepoRoot 'bin')"
