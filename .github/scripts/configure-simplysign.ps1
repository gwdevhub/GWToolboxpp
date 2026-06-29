# Pre-configure SimplySign Desktop for unattended use:
#  - auto-show the login dialog on launch (so the TOTP keystrokes have a target)
#  - cache the smart-card PIN in the CSP, so signtool signs without a per-call
#    PIN prompt, and forget it again when the session disconnects.
# Mirrors the settings used by the blinkdisk / docuscope CI signing setups.

$ErrorActionPreference = "Stop"

$RegistryPath = "HKCU:\Software\Certum\SimplySign"

$settings = [ordered]@{
    ShowLoginDialogOnStart             = 1
    ShowLoginDialogOnAppRequest        = 1
    RememberLastUserName               = 0
    Autostart                          = 0
    UnregisterCertificatesOnDisconnect = 0
    RememberPINinCSP                   = 1
    ForgetPINinCSPonDisconnect         = 1
    LangID                             = 9
}

Write-Host "=== Configuring SimplySign Desktop registry ==="
New-Item -Path $RegistryPath -Force | Out-Null
foreach ($name in $settings.Keys) {
    Set-ItemProperty -Path $RegistryPath -Name $name -Value $settings[$name] -Type DWord
    Write-Host "  $name = $($settings[$name])"
}
Write-Host "Done."
