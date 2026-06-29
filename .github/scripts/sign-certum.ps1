# Sign the given files with the Certum cloud certificate (selected by SHA1
# thumbprint from the store) and verify each signature. Assumes SimplySign
# Desktop has already authenticated (connect-simplysign.ps1).

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string[]]$Files,
    [string]$CertSha1     = $env:CERTUM_CERT_SHA1,
    [string]$TimestampUrl = "http://time.certum.pl"
)

$ErrorActionPreference = "Stop"

if (-not $CertSha1) { throw "CERTUM_CERT_SHA1 not provided." }
$thumb = ($CertSha1 -replace '\s', '').ToUpperInvariant()

# Resolve the newest signtool.exe from the installed Windows SDK(s).
$kitsBin = "C:\Program Files (x86)\Windows Kits\10\bin"
$signtool = Get-ChildItem -Path $kitsBin -Recurse -Filter signtool.exe -File -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -like '*\x64\*' } |
    Sort-Object { [version]$_.Directory.Parent.Name } |
    Select-Object -Last 1 -ExpandProperty FullName
if (-not $signtool) { throw "signtool.exe not found under $kitsBin" }
Write-Host "Using signtool: $signtool"

# The SimplySign virtual smart card registers in the store a moment after the
# desktop client authenticates; poll for our certificate before signing.
Write-Host "Waiting for certificate $thumb to appear in the store..."
$deadline = (Get-Date).AddSeconds(120)
$cert = $null
do {
    $cert = Get-ChildItem Cert:\CurrentUser\My, Cert:\LocalMachine\My -ErrorAction SilentlyContinue |
        Where-Object { $_.Thumbprint -eq $thumb } | Select-Object -First 1
    if ($cert) { break }
    Start-Sleep -Seconds 3
} while ((Get-Date) -lt $deadline)

if (-not $cert) {
    throw "Certificate $thumb not found in the store - SimplySign authentication likely failed."
}
Write-Host "Found signing certificate: $($cert.Subject)"

$failed = @()
foreach ($file in $Files) {
    if (-not (Test-Path $file)) { throw "File to sign not found: $file" }
    Write-Host "=== Signing $file ==="
    & $signtool sign /sha1 $thumb /fd sha256 /td sha256 /tr $TimestampUrl /v $file
    if ($LASTEXITCODE -ne 0) { $failed += $file; continue }
    & $signtool verify /pa /v $file
    if ($LASTEXITCODE -ne 0) { $failed += $file }
}

if ($failed.Count -gt 0) {
    throw "Signing/verification failed for: $($failed -join ', ')"
}
Write-Host "All binaries signed and verified."
