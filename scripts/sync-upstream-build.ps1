# Merge upstream GWToolbox++ into this fork and rebuild GWToolboxdll.
# Close Guild Wars before running — the linker cannot replace GWToolboxdll.dll while the game is running.
param(
    [string]$Remote = "upstream",
    [string]$Branch = "master",
    [string]$Configuration = "RelWithDebInfo"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

Write-Host "Fetching $Remote..."
git fetch $Remote

Write-Host "Merging $Remote/$Branch..."
git merge "$Remote/$Branch" --no-edit

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$msbuild = $null
if (Test-Path $vswhere) {
    $msbuild = & $vswhere -latest -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
}
if (-not $msbuild) {
  $fallback = "C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\MSBuild.exe"
  if (Test-Path $fallback) { $msbuild = $fallback }
}
if (-not $msbuild) {
    throw "MSBuild not found. Open a Visual Studio Developer shell or install VS Build Tools."
}

$proj = Join-Path $root "build\GWToolboxdll\GWToolboxdll.vcxproj"
if (-not (Test-Path $proj)) {
    throw "Missing $proj — run cmake --preset vcpkg first."
}

Write-Host "Building GWToolboxdll ($Configuration)..."
& $msbuild $proj /p:Configuration=$Configuration /m /v:minimal

$dll = Join-Path $root "bin\$Configuration\GWToolboxdll.dll"
if (Test-Path $dll) {
    Get-Item $dll | Format-List FullName, LastWriteTime, Length
    Write-Host "Done. Restart GWToolbox / Guild Wars to load the new DLL."
}
else {
    throw "Build finished but $dll was not produced."
}
