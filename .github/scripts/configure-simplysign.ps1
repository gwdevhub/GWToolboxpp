$ErrorActionPreference = "Stop"

$RegistryPath = "HKCU:\Software\Certum\SimplySign"

$settings = [ordered]@{
    ShowLoginDialogOnStart             = 1
    ShowLoginDialogOnAppRequest        = 1
    RememberLastUserName               = 0
    Autostart                          = 0
    UnregisterCertificatesOnDisconnect = 1
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
