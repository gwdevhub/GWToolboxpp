# Authenticate SimplySign Desktop non-interactively.
#
# Certum's SimplySign cloud has no headless login: the certificate only reaches
# the Windows store after the GUI client authenticates. So we generate the
# current TOTP from the otpauth:// secret (CERTUM_OTP_URI) and paste the
# credentials into the login dialog. This needs an interactive desktop session,
# which GitHub-hosted Windows runners provide.
#
# Based on https://www.devas.life/how-to-automate-signing-your-windows-app-with-certum/
# and the refinements in blinkdisk's connect-simplysign.ps1.

param(
    [string]$OtpUri  = $env:CERTUM_OTP_URI,
    [string]$UserId  = $env:CERTUM_USERID,
    [string]$ExePath = $env:CERTUM_EXE_PATH
)

if (-not $OtpUri) { Write-Host "ERROR: CERTUM_OTP_URI not provided"; exit 1 }
if (-not $UserId) { Write-Host "ERROR: CERTUM_USERID not provided"; exit 1 }
if (-not $ExePath) {
    $ExePath = "C:\Program Files\Certum\SimplySign Desktop\SimplySignDesktop.exe"
}

Write-Host "=== SimplySign Desktop TOTP authentication ==="
if (-not (Test-Path $ExePath)) {
    Write-Host "ERROR: SimplySign Desktop not found at $ExePath"
    exit 1
}

# --- Parse the otpauth:// URI (works on PowerShell 5.1 and 7+) ---------------
$uri = [Uri]$OtpUri
try {
    $q = [System.Web.HttpUtility]::ParseQueryString($uri.Query)
} catch {
    $q = @{}
    foreach ($part in $uri.Query.TrimStart('?') -split '&') {
        $kv = $part -split '=', 2
        if ($kv.Count -eq 2) { $q[$kv[0]] = [Uri]::UnescapeDataString($kv[1]) }
    }
}

$Base32    = $q['secret']
$Digits    = if ($q['digits'])    { [int]$q['digits'] }    else { 6 }
$Period    = if ($q['period'])    { [int]$q['period'] }    else { 30 }
$Algorithm = if ($q['algorithm']) { $q['algorithm'].ToUpper() } else { 'SHA256' }

if (-not $Base32) { Write-Host "ERROR: otpauth URI has no 'secret'"; exit 1 }
if ($Algorithm -notin @('SHA1', 'SHA256', 'SHA512')) {
    Write-Host "ERROR: unsupported TOTP algorithm: $Algorithm"
    exit 1
}

# --- TOTP generator (RFC 6238), inline C# so there are no dependencies --------
Add-Type -Language CSharp @"
using System;
using System.Security.Cryptography;

public static class Totp
{
    private const string B32 = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

    private static byte[] Base32Decode(string s)
    {
        s = s.TrimEnd('=').ToUpperInvariant();
        byte[] bytes = new byte[s.Length * 5 / 8];
        int bitBuffer = 0, bitsLeft = 0, idx = 0;
        foreach (char c in s)
        {
            int val = B32.IndexOf(c);
            if (val < 0) throw new ArgumentException("Invalid Base32 char: " + c);
            bitBuffer = (bitBuffer << 5) | val;
            bitsLeft += 5;
            if (bitsLeft >= 8) { bytes[idx++] = (byte)(bitBuffer >> (bitsLeft - 8)); bitsLeft -= 8; }
        }
        return bytes;
    }

    private static HMAC Hmac(string algorithm, byte[] key)
    {
        switch (algorithm.ToUpper())
        {
            case "SHA1":   return new HMACSHA1(key);
            case "SHA256": return new HMACSHA256(key);
            case "SHA512": return new HMACSHA512(key);
            default: throw new ArgumentException("Unsupported algorithm: " + algorithm);
        }
    }

    public static string Now(string secret, int digits, int period, string algorithm)
    {
        byte[] key = Base32Decode(secret);
        long counter = DateTimeOffset.UtcNow.ToUnixTimeSeconds() / period;
        byte[] cnt = BitConverter.GetBytes(counter);
        if (BitConverter.IsLittleEndian) Array.Reverse(cnt);

        byte[] hash;
        using (var h = Hmac(algorithm, key)) { hash = h.ComputeHash(cnt); }

        int offset = hash[hash.Length - 1] & 0x0F;
        int binary =
            ((hash[offset]     & 0x7F) << 24) |
            ((hash[offset + 1] & 0xFF) << 16) |
            ((hash[offset + 2] & 0xFF) << 8)  |
             (hash[offset + 3] & 0xFF);
        int otp = binary % (int)Math.Pow(10, digits);
        return otp.ToString(new string('0', digits));
    }
}
"@

function Find-UiByName($el, $walker, $name) {
    $c = $walker.GetFirstChild($el)
    while ($c) {
        if ($c.Current.Name -eq $name) { return $c }
        $r = Find-UiByName $c $walker $name
        if ($r) { return $r }
        $c = $walker.GetNextSibling($c)
    }
    return $null
}

# An outdated SimplySign build pops a modal "New version found - download?" box
# over the login form, which swallows the credential keystrokes. Decline it by
# clicking "No" (Invoke / legacy default action / click the element's point).
function Dismiss-UpdatePrompt {
    try { Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes -ErrorAction Stop } catch { return $false }
    if (-not ('Win32Mouse' -as [type])) {
        Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class Win32Mouse {
    [DllImport("user32.dll")] static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] static extern void mouse_event(uint f, uint x, uint y, uint d, int e);
    public static void Click(int x, int y){ SetCursorPos(x,y); mouse_event(0x02,0,0,0,0); mouse_event(0x04,0,0,0,0); }
}
"@
    }
    $procIds = @((Get-Process -Name '*SimplySign*' -ErrorAction SilentlyContinue).Id)
    $walker = [System.Windows.Automation.TreeWalker]::ControlViewWalker
    $top = $walker.GetFirstChild([System.Windows.Automation.AutomationElement]::RootElement)
    while ($top) {
        if ($procIds -contains $top.Current.ProcessId) {
            $no = Find-UiByName $top $walker "No"
            if ($no) {
                Write-Host "Update prompt detected; declining (clicking 'No')."
                try { ($no.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern)).Invoke(); return $true } catch {}
                try { ($no.GetCurrentPattern([System.Windows.Automation.LegacyIAccessiblePattern]::Pattern)).DoDefaultAction(); return $true } catch {}
                try { $pt = $no.GetClickablePoint(); [Win32Mouse]::Click([int]$pt.X, [int]$pt.Y); return $true } catch { Write-Host "Click 'No' failed: $($_.Exception.Message)" }
            }
        }
        $top = $walker.GetNextSibling($top)
    }
    return $false
}

# Start from a clean slate so a stale window/session can't swallow the keystrokes.
if ($existing = Get-Process -Name "SimplySignDesktop" -ErrorAction Ignore) {
    Write-Host "Killing existing SimplySignDesktop process..."
    $existing | Stop-Process -Force
    Start-Sleep -Seconds 1
}

Write-Host "Launching SimplySign Desktop..."
$proc = Start-Process -FilePath $ExePath -PassThru
Start-Sleep -Seconds 3

$wshell = New-Object -ComObject WScript.Shell

Write-Host "Focusing the login window..."
$focused = $wshell.AppActivate($proc.Id)
if (-not $focused) { $focused = $wshell.AppActivate('SimplySign Desktop') }
for ($i = 0; (-not $focused) -and ($i -lt 10); $i++) {
    Start-Sleep -Milliseconds 500
    $focused = $wshell.AppActivate($proc.Id) -or $wshell.AppActivate('SimplySign Desktop')
}
if (-not $focused) {
    Write-Host "ERROR: could not bring SimplySign Desktop to the foreground"
    exit 1
}

# Decline the "new version available" modal if it's covering the login form.
if (Dismiss-UpdatePrompt) { Start-Sleep -Milliseconds 800 }

# Re-assert focus: dismissing the dialog can move the foreground window, and
# keystrokes sent to the wrong window are silently dropped.
$wshell.AppActivate($proc.Id) | Out-Null
$wshell.AppActivate('SimplySign Desktop') | Out-Null
Start-Sleep -Milliseconds 400

# Paste rather than type: SendKeys mangles characters like + ^ % ( ) and can
# drop characters; pasting delivers the exact string. Falls back to typing if
# the clipboard is unavailable.
function Set-Field([string]$text) {
    try {
        Set-Clipboard -Value $text -ErrorAction Stop
        Start-Sleep -Milliseconds 150
        $wshell.SendKeys("^v")
    } catch {
        Write-Host "Clipboard unavailable ($($_.Exception.Message)); typing instead."
        $wshell.SendKeys($text)
    }
    Start-Sleep -Milliseconds 250
}

Write-Host "Injecting credentials..."
Set-Field $UserId
$wshell.SendKeys("{TAB}")
Start-Sleep -Milliseconds 200

# Generate the code right before sending it so it can't expire while we focus.
$otp = [Totp]::Now($Base32, $Digits, $Period, $Algorithm)
Set-Field $otp
Start-Sleep -Milliseconds 200
$wshell.SendKeys("{ENTER}")

# Don't leave the OTP / username sitting on the clipboard afterwards.
try { Set-Clipboard -Value " " -ErrorAction Stop } catch {}

Write-Host "Waiting for authentication to settle..."
Start-Sleep -Seconds 5

if (-not (Get-Process -Id $proc.Id -ErrorAction SilentlyContinue)) {
    Write-Host "ERROR: SimplySign Desktop exited - authentication failed."
    exit 1
}

# Confirm the login actually mounted the signing certificate into the store.
$thumb = if ($env:CERTUM_CERT_SHA1) { ($env:CERTUM_CERT_SHA1 -replace '\s', '').ToUpperInvariant() } else { $null }
Write-Host "Verifying the signing certificate reached the store..."
$deadline = (Get-Date).AddSeconds(60)
$found = $false
do {
    $certs = Get-ChildItem Cert:\CurrentUser\My, Cert:\LocalMachine\My -ErrorAction SilentlyContinue
    if ($thumb) { $found = [bool]($certs | Where-Object { $_.Thumbprint -eq $thumb }) }
    else        { $found = [bool]($certs | Where-Object { $_.Subject -like '*Certum*' -or $_.Issuer -like '*Certum*' }) }
    if ($found) { break }
    Start-Sleep -Seconds 3
} while ((Get-Date) -lt $deadline)

if ($found) {
    Write-Host "=== Authentication complete: signing certificate is available. ==="
} else {
    Write-Host "ERROR: signing certificate did not appear after authentication."
    exit 1
}
