<#
  harness.ps1 — host-side driver for the autonomous GWToolbox pathfinding test loop.

  Build config is Debug: the dev tooling (TestHarness module + [timing]/[AStar] diagnostics) is gated on
  _DEBUG only, so it is compiled out of the shipped RelWithDebInfo/Release builds. Debug now mirrors
  the log to log.txt (Logger.cpp) in addition to the console, so the harness can read it. Pass
  -Config RelWithDebInfo (+ define GWTB_HARNESS) only if you deliberately want it in a release build.

  Pairs with the in-DLL TestHarness module: it polls <HarnessDir>\harness_command.txt and auto-fires
  the waypoint from harness_config.txt; it writes harness_status.txt and the runtime log.txt.

  Loop (GW already in-game, toolbox injected once):
      .\harness.ps1 reload          # shutdown old DLL -> rebuild -> re-inject (GW stays open)
      .\harness.ps1 goto 1234 -5678 0
      .\harness.ps1 tail            # follow log.txt for [timing]/[AStar]
  Cold start (GW not running): GW Launcher logs the account in and early-injects the Debug DLL.
  NEVER launch Gw.exe directly.
      .\harness.ps1 up
#>

[CmdletBinding()]
param(
  [Parameter(Position = 0)] [string]$Cmd = 'status',
  [Parameter(Position = 1, ValueFromRemainingArguments = $true)] [string[]]$Rest,

  [string]$Repo       = '',  # default: repo root inferred from this script's location
  [string]$Dll        = '',  # default <Repo>\bin\Debug\GWToolboxdll.dll
  [string]$Launcher   = '',  # default <Repo>\bin\Debug\GWToolbox.exe
  [string]$HarnessDir = '',  # default OneDrive\Documents\GWToolboxpp\<COMPUTERNAME>
  [string]$Config     = 'Debug',  # the harness + diagnostics are gated on _DEBUG only
  [string]$Account    = 'Mystical Dub'  # GW Launcher account Name (Accounts.json) for cold starts
)

$ErrorActionPreference = 'Stop'
if (-not $Repo) {
  $scriptDir = if ($PSScriptRoot) { $PSScriptRoot } elseif ($MyInvocation.MyCommand.Path) { Split-Path -Parent $MyInvocation.MyCommand.Path } else { (Get-Location).Path }
  $Repo = Split-Path -Parent $scriptDir
}
if (-not $Dll)      { $Dll      = Join-Path $Repo "bin\$Config\GWToolboxdll.dll" }
if (-not $Launcher) { $Launcher = Join-Path $Repo "bin\$Config\GWToolbox.exe" }

function Resolve-HarnessDir {
  if ($HarnessDir) { return $HarnessDir }
  $name = $env:COMPUTERNAME
  $c = @(
    (Join-Path $env:OneDrive "Documents\GWToolboxpp\$name"),
    (Join-Path ([Environment]::GetFolderPath('MyDocuments')) "GWToolboxpp\$name")
  ) | Where-Object { $_ -and (Test-Path $_) } | Select-Object -First 1
  if (-not $c) { throw "Could not resolve HarnessDir; pass -HarnessDir." }
  return $c
}

function Get-GwPid { (Get-Process -Name 'Gw' -ErrorAction SilentlyContinue | Select-Object -First 1).Id }

function Build-Dll {
  Write-Host "[harness] building $Config GWToolboxdll..." -ForegroundColor Cyan
  & cmake --build (Join-Path $Repo 'build') --config $Config --target GWToolboxdll 2>&1 |
    Select-String -Pattern 'error C|error LNK|GWToolboxdll.vcxproj ->|LNK1168' | ForEach-Object { $_.Line }
  if (-not (Test-Path $Dll)) { throw "DLL not found after build: $Dll" }
}

function Inject {
  if (-not (Test-Path $Launcher)) { throw "Launcher not found: $Launcher (build target GWToolbox)." }
  $gw = Get-GwPid
  if (-not $gw) { throw "Gw.exe not running. Use 'up' to launch it." }
  # /localdll loads GWToolboxdll.dll from the launcher's own folder. Stage the fresh build there
  # (no-op when the launcher already sits next to the built DLL, e.g. both in bin\RelWithDebInfo).
  $stage = Join-Path (Split-Path $Launcher -Parent) 'GWToolboxdll.dll'
  if ((Resolve-Path $stage -ErrorAction SilentlyContinue).Path -ne (Resolve-Path $Dll).Path) { Copy-Item $Dll $stage -Force }
  Write-Host "[harness] injecting into Gw.exe pid=$gw" -ForegroundColor Cyan
  & $Launcher /pid $gw /localdll /noupdate /noinstall /noexecheck /quiet | Out-Null
}

function Loaded-Dll { Join-Path (Split-Path $Launcher -Parent) 'GWToolboxdll.dll' }

function Send-Command([string]$line) {
  # WriteAllText emits UTF-8 WITHOUT a BOM (Set-Content -Encoding UTF8 on PS 5.1 adds a BOM that
  # the DLL would read as part of the first token).
  [System.IO.File]::WriteAllText((Join-Path (Resolve-HarnessDir) 'harness_command.txt'), $line)
  Write-Host "[harness] -> $line" -ForegroundColor Green
}

function Wait-Status([string]$match, [int]$timeoutSec = 20) {
  $sf = Join-Path (Resolve-HarnessDir) 'harness_status.txt'
  $deadline = (Get-Date).AddSeconds($timeoutSec)
  while ((Get-Date) -lt $deadline) {
    if (Test-Path $sf) { $s = Get-Content $sf -Raw -EA SilentlyContinue; if ($s -match $match) { return $s } }
    Start-Sleep -Milliseconds 250
  }
  return $null
}

function Wait-DllUnloaded([int]$timeoutSec = 20) {
  $f = Loaded-Dll
  $deadline = (Get-Date).AddSeconds($timeoutSec)
  while ((Get-Date) -lt $deadline) {
    try { $h = [IO.File]::Open($f, 'Open', 'ReadWrite', 'None'); $h.Close(); return $true } catch { Start-Sleep -Milliseconds 250 }
  }
  return $false
}

switch ($Cmd) {
  'status'  { $hd = Resolve-HarnessDir; "HarnessDir: $hd"; "Gw pid: $(Get-GwPid)"; if (Test-Path "$hd\harness_status.txt") { 'status: ' + (Get-Content "$hd\harness_status.txt" -Raw) } }
  'build'   { Build-Dll }
  'inject'  { Inject; if (Wait-Status 'harness_initialized|waypoint_set|login' 20) { '[harness] injected.' } else { '[harness] WARN: no status (harness may not have loaded).' } }
  'goto'    { Send-Command ("waypoint " + ($Rest -join ' ')) }
  'setgoal' { Send-Command 'setgoal'; if (Wait-Status 'goal_captured' 6) { '[harness] ' + (Get-Content (Join-Path (Resolve-HarnessDir) 'harness_status.txt') -Raw) } }
  'repath'  { Send-Command 'repath' }
  'login'   { Send-Command 'login' }
  'shutdown'{ Send-Command 'shutdown'; if (Wait-DllUnloaded) { '[harness] DLL unloaded.' } else { '[harness] WARN: DLL still locked.' } }
  'reload'  {
    Send-Command 'shutdown'
    if (-not (Wait-DllUnloaded)) { throw "DLL did not unload; cannot relink." }
    Build-Dll
    Inject
    if (-not (Wait-Status 'harness_initialized|path_set' 20)) { '[harness] WARN: no status after inject.' }
    Start-Sleep -Milliseconds 600
    Send-Command 'repath' # force a path to the quest marker (in case the auto-fire raced the quest load)
    if (Wait-Status 'path_set' 10) { '[harness] reloaded + repathed.' } else { '[harness] reloaded (no path_set yet).' }
  }
  'up'      {
    if (Get-GwPid) {
      Inject
      if (Wait-Status 'harness_initialized|path_set' 30) { '[harness] up + injected.' } else { '[harness] WARN: injected but no harness status (still loading / not in-game?).' }
      break
    }
    # Cold starts MUST go through GW Launcher (user directive): it logs the account in itself
    # (no foregrounded keystroke hacks) and early-injects everything in bin\<Config>\plugins\
    # (gMod + the locally built toolbox DLL) before Gw.dat is opened. The launcher resolves
    # Accounts.json/Settings.json/plugins\ relative to its CWD, so WorkingDirectory is required.
    $launcherDir = Split-Path $Launcher -Parent
    $gwLauncher  = Join-Path $launcherDir 'GW Launcher.exe'
    if (-not (Test-Path $gwLauncher)) { throw "GW Launcher not found: $gwLauncher" }
    Write-Host "[harness] launching via GW Launcher (account: $Account)" -ForegroundColor Cyan
    Start-Process -FilePath $gwLauncher -WorkingDirectory $launcherDir -ArgumentList ('-launch "{0}"' -f $Account)
    for ($i = 0; $i -lt 90 -and -not (Get-GwPid); $i++) { Start-Sleep -Seconds 1 }
    if (-not (Get-GwPid)) { throw "Gw.exe did not start via GW Launcher." }
    if (Wait-Status 'harness_initialized|path_set' 90) { '[harness] up via GW Launcher (early-injected).' } else { '[harness] WARN: Gw running but no harness status yet (login may still be in progress).' }
  }
  'tail'    {
    $log = Join-Path (Resolve-HarnessDir) 'log.txt'
    if (-not (Test-Path $log)) { throw "log not found: $log" }
    Get-Content $log -Tail 60 -Wait | Where-Object { $_ -match '\[harness|\[timing|\[AStar' }
  }
  default   { "Unknown '$Cmd'. Use: status|build|inject|goto|login|shutdown|reload|up|tail" }
}
