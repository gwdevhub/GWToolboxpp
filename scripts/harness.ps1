<#
  harness.ps1 — host-side driver for the autonomous GWToolbox pathfinding test loop.

  Build config is Debug: the dev tooling (TestHarness module + [polyanya] diagnostics) is gated on
  _DEBUG only, so it is compiled out of the shipped RelWithDebInfo/Release builds. Debug now mirrors
  the log to log.txt (Logger.cpp) in addition to the console, so the harness can read it. Pass
  -Config RelWithDebInfo (+ define GWTB_HARNESS) only if you deliberately want it in a release build.

  Pairs with the in-DLL TestHarness module: it polls <HarnessDir>\harness_command.txt and auto-fires
  the waypoint from harness_config.txt; it writes harness_status.txt and the runtime log.txt.

  Loop (GW already in-game, toolbox injected once):
      .\harness.ps1 reload          # shutdown old DLL -> rebuild -> re-inject (GW stays open)
      .\harness.ps1 goto 1234 -5678 0
      .\harness.ps1 mode 2
      .\harness.ps1 tail            # follow log.txt for [polyanya]/[timing]
  Cold start (GW not running): launch -> auto-login (foregrounded Enter presses) -> inject:
      .\harness.ps1 up
#>

[CmdletBinding()]
param(
  [Parameter(Position = 0)] [string]$Cmd = 'status',
  [Parameter(Position = 1, ValueFromRemainingArguments = $true)] [string[]]$Rest,

  [string]$Repo       = '',  # default: repo root inferred from this script's location
  [string]$Dll        = '',  # default <Repo>\bin\Debug\GWToolboxdll.dll
  [string]$Launcher   = '',  # default <Repo>\bin\Debug\GWToolbox.exe
  [string]$GwExe      = '',  # default: resolved from the Guild Wars registry key, else the standard install path
  [string]$HarnessDir = '',  # default OneDrive\Documents\GWToolboxpp\<COMPUTERNAME>
  [string]$Config     = 'Debug'  # the harness + diagnostics are gated on _DEBUG only
)

$ErrorActionPreference = 'Stop'
if (-not $Repo) {
  $scriptDir = if ($PSScriptRoot) { $PSScriptRoot } elseif ($MyInvocation.MyCommand.Path) { Split-Path -Parent $MyInvocation.MyCommand.Path } else { (Get-Location).Path }
  $Repo = Split-Path -Parent $scriptDir
}
if (-not $Dll)      { $Dll      = Join-Path $Repo "bin\$Config\GWToolboxdll.dll" }
if (-not $Launcher) { $Launcher = Join-Path $Repo "bin\$Config\GWToolbox.exe" }

function Resolve-GwExe {
  if ($GwExe -and (Test-Path $GwExe)) { return $GwExe }
  foreach ($k in 'HKLM:\SOFTWARE\WOW6432Node\ArenaNet\Guild Wars', 'HKLM:\SOFTWARE\ArenaNet\Guild Wars', 'HKCU:\SOFTWARE\ArenaNet\Guild Wars') {
    try { $p = (Get-ItemProperty -Path $k -Name 'Path' -ErrorAction Stop).Path; if ($p -and (Test-Path $p)) { return $p } } catch {}
  }
  $std = 'C:\Program Files (x86)\Guild Wars\Gw.exe'
  if (Test-Path $std) { return $std }
  throw "Gw.exe not found (registry + standard path). Pass -GwExe <full path to Gw.exe>."
}

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

# Host-side cold-start login: GW ignores synthetic input from the DLL on the account screen, but a
# foregrounded real keystroke works. Restore+foreground the window, then inject Enter (down/up).
Add-Type -Namespace HInput -Name Win -MemberDefinition @'
[System.Runtime.InteropServices.DllImport("user32.dll")] public static extern bool SetForegroundWindow(System.IntPtr h);
[System.Runtime.InteropServices.DllImport("user32.dll")] public static extern bool ShowWindow(System.IntPtr h, int n);
[System.Runtime.InteropServices.DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte sc, uint f, System.UIntPtr e);
'@
function Press-Enter {
  $p = Get-Process -Name 'Gw' -ErrorAction SilentlyContinue | Select-Object -First 1
  if (-not $p) { return }
  [HInput.Win]::ShowWindow($p.MainWindowHandle, 9) | Out-Null   # SW_RESTORE
  [HInput.Win]::SetForegroundWindow($p.MainWindowHandle) | Out-Null
  Start-Sleep -Milliseconds 700
  [HInput.Win]::keybd_event(0x0D, 0x1C, 0, [System.UIntPtr]::Zero)   # Enter down
  Start-Sleep -Milliseconds 60
  [HInput.Win]::keybd_event(0x0D, 0x1C, 2, [System.UIntPtr]::Zero)   # Enter up
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
  'mode'    { Send-Command ("mode " + ($Rest -join ' ')) }
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
    $fresh = $false
    if (-not (Get-GwPid)) {
      $gwExePath = Resolve-GwExe
      Write-Host "[harness] launching $gwExePath" -ForegroundColor Cyan
      Start-Process $gwExePath
      for ($i = 0; $i -lt 90 -and -not (Get-GwPid); $i++) { Start-Sleep -Seconds 1 }
      if (-not (Get-GwPid)) { throw "Gw.exe did not start." }
      $fresh = $true
    }
    if ($fresh) {
      # Account + character are pre-filled, so two foregrounded Enters reach the outpost.
      Write-Host "[harness] waiting for login screen, then auto-logging in..." -ForegroundColor Cyan
      Start-Sleep -Seconds 16
      Press-Enter; Start-Sleep -Seconds 6     # account login
      Press-Enter; Start-Sleep -Seconds 10    # play pre-selected character -> map load
    }
    Inject
    if (Wait-Status 'harness_initialized|path_set' 30) { '[harness] up + injected.' } else { '[harness] WARN: injected but no harness status (still loading / not in-game?).' }
  }
  'tail'    {
    $log = Join-Path (Resolve-HarnessDir) 'log.txt'
    if (-not (Test-Path $log)) { throw "log not found: $log" }
    Get-Content $log -Tail 60 -Wait | Where-Object { $_ -match '\[polyanya|\[harness|\[timing|\[AStar' }
  }
  default   { "Unknown '$Cmd'. Use: status|build|inject|goto|mode|login|shutdown|reload|up|tail" }
}
