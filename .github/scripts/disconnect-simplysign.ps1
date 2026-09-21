$ErrorActionPreference = "Stop"

$processes = @(Get-Process -Name "SimplySignDesktop" -ErrorAction SilentlyContinue)
foreach ($process in $processes) {
    if ($process.CloseMainWindow()) {
        $process.WaitForExit(5000) | Out-Null
    }
}

$remaining = @(Get-Process -Name "SimplySignDesktop" -ErrorAction SilentlyContinue)
if ($remaining.Count -gt 0) {
    $remaining | Stop-Process -Force
    $remaining | Wait-Process -Timeout 10
}

if (Get-Process -Name "SimplySignDesktop" -ErrorAction SilentlyContinue) {
    throw "SimplySign Desktop is still running after disconnect."
}

Write-Host "SimplySign Desktop disconnected."
