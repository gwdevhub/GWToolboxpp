<#
.SYNOPSIS
  Deterministic prep for adding the latest Guild Wars client to the Ghidra project.
  Detects the client build id, copies the live Gw.exe to a build-named file
  (Gw.<build>.exe), and prints the ReVa MCP steps the agent should run to import,
  analyze, and version it — keeping prior builds intact. See
  docs/gw-auto-update/09-ghidra-project-updates.md.
#>
param(
  [string]$GwExe  = "C:\Program Files (x86)\Guild Wars\Gw.exe",
  [string]$OutDir = "$PSScriptRoot\_ghidra-import"
)
$ErrorActionPreference = "Stop"
$here   = Split-Path -Parent $MyInvocation.MyCommand.Path
$detect = Join-Path $here "detect_build.py"

$info  = & python $detect --exe $GwExe | ConvertFrom-Json
$build = $info.build_id
if (-not $build) { Write-Error "could not detect build id from $GwExe"; exit 1 }

New-Item -ItemType Directory -Force $OutDir | Out-Null
$dst = Join-Path $OutDir ("Gw.{0}.exe" -f $build)
Copy-Item $GwExe $dst -Force

Write-Output "build id : $build"
Write-Output "sha256   : $($info.sha256)"
Write-Output "copied   : $dst"
Write-Output ""
Write-Output "Next (ReVa MCP, agent-driven; skip if /Gw.$build.exe already in project):"
Write-Output "  import-file   { path: '$dst', enableVersionControl: true, recursive: false }"
Write-Output "  analyze-program { programPath: '/Gw.$build.exe', persist: 'auto' }"
Write-Output "  (optional) diff-create-session prev->/Gw.$build.exe + diff-transfer-markup"
Write-Output "  GUI cleanup: delete the 'Gw.$build.exe.0' import duplicate"
