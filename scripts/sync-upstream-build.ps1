# Deprecated: merge-based sync. Prefer rebase to keep fork commits on latest upstream.
# Forwards to scripts/sync-upstream-rebase.ps1
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

Write-Host "Note: sync-upstream-build.ps1 now forwards to sync-upstream-rebase.ps1 (rebase workflow)." -ForegroundColor Yellow
& (Join-Path $PSScriptRoot "sync-upstream-rebase.ps1") @PSBoundParameters
