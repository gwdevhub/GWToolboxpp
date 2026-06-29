# Authenticate SimplySign Desktop non-interactively.
#
# Certum's SimplySign cloud has no headless login: the certificate only reaches
# the Windows store after the GUI client authenticates. So we generate the
# current TOTP from the otpauth:// secret (CERTUM_OTP_URI) and type the
# credentials into the login dialog via SendKeys. This needs an interactive
# desktop session, which GitHub-hosted Windows runners provide.
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
Write-Host "User: $UserId"
Write-Host "Executable: $ExePath"

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

# --- Diagnostics: capture what the login form actually looks like -------------
# A screenshot needs a framebuffer; the UI Automation tree dump works regardless
# and gives us the exact control names/order to target.
function Save-Screenshot([string]$path) {
    try {
        Add-Type -AssemblyName System.Windows.Forms, System.Drawing -ErrorAction Stop
        $b = [System.Windows.Forms.SystemInformation]::VirtualScreen
        $bmp = New-Object System.Drawing.Bitmap($b.Width, $b.Height)
        $g = [System.Drawing.Graphics]::FromImage($bmp)
        $g.CopyFromScreen($b.Location, [System.Drawing.Point]::Empty, $b.Size)
        $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
        $g.Dispose(); $bmp.Dispose()
        Write-Host "Saved screenshot: $path ($($b.Width)x$($b.Height))"
    } catch {
        Write-Host "Screenshot failed: $($_.Exception.Message)"
    }
}

function Walk-Ui($el, $walker, $depth) {
    $c = $walker.GetFirstChild($el)
    while ($c) {
        $info = $c.Current
        Write-Host ("{0}[{1}] Name='{2}' Id='{3}'" -f ('  ' * $depth),
            $info.ControlType.ProgrammaticName.Replace('ControlType.', ''), $info.Name, $info.AutomationId)
        if ($depth -lt 8) { Walk-Ui $c $walker ($depth + 1) }
        $c = $walker.GetNextSibling($c)
    }
}

function Show-UiTree {
    try {
        Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes -ErrorAction Stop
    } catch { Write-Host "UI Automation unavailable: $($_.Exception.Message)"; return }
    $procIds = @((Get-Process -Name '*SimplySign*' -ErrorAction SilentlyContinue).Id)
    if (-not $procIds) { Write-Host "No SimplySign process to inspect."; return }
    $walker = [System.Windows.Automation.TreeWalker]::ControlViewWalker
    $top = $walker.GetFirstChild([System.Windows.Automation.AutomationElement]::RootElement)
    while ($top) {
        try {
            if ($procIds -contains $top.Current.ProcessId) {
                Write-Host "== WINDOW: '$($top.Current.Name)' =="
                Walk-Ui $top $walker 1
            }
        } catch {}
        $top = $walker.GetNextSibling($top)
    }
}

function Dump-SignDiagnostics([string]$tag) {
    $dir = Join-Path (Get-Location) "sign-diagnostics"
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
    Save-Screenshot (Join-Path $dir "$tag.png")
    Write-Host "--- UI tree ($tag) ---"
    Show-UiTree
    Write-Host "--- end UI tree ($tag) ---"
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

Write-Host "SimplySign windows after focus:"
Get-Process -Name '*SimplySign*' -ErrorAction SilentlyContinue |
    Select-Object Name, Id, MainWindowTitle | Format-Table -AutoSize | Out-String | Write-Host

# Capture the login form before typing into it so we can see its real layout.
Dump-SignDiagnostics "before-login"

Start-Sleep -Milliseconds 400
Write-Host "Injecting credentials (user -> TAB -> TOTP -> ENTER)..."
$wshell.SendKeys($UserId)
Start-Sleep -Milliseconds 200
$wshell.SendKeys("{TAB}")
Start-Sleep -Milliseconds 200

# Generate the code right before sending it so it can't expire while we focus.
$otp = [Totp]::Now($Base32, $Digits, $Period, $Algorithm)
Write-Host "Generated TOTP using $Algorithm."
$wshell.SendKeys($otp)
Start-Sleep -Milliseconds 200
$wshell.SendKeys("{ENTER}")

Write-Host "Waiting for authentication to settle..."
Start-Sleep -Seconds 5

if (-not (Get-Process -Id $proc.Id -ErrorAction SilentlyContinue)) {
    Write-Host "ERROR: SimplySign Desktop exited - authentication failed."
    exit 1
}

# Don't just trust that the process is alive: confirm the login actually mounted
# the signing certificate into the store, and dump diagnostics if it didn't.
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
    Write-Host "SimplySign processes/windows:"
    Get-Process -Name '*SimplySign*' -ErrorAction SilentlyContinue |
        Select-Object Name, Id, Responding, MainWindowTitle | Format-Table -AutoSize | Out-String | Write-Host
    Write-Host "CurrentUser\My + LocalMachine\My contents:"
    Get-ChildItem Cert:\CurrentUser\My, Cert:\LocalMachine\My -ErrorAction SilentlyContinue |
        Select-Object Thumbprint, Subject, HasPrivateKey | Format-Table -AutoSize | Out-String | Write-Host
    # Capture the post-attempt state (any error message / dialog) for debugging.
    Dump-SignDiagnostics "after-fail"
    exit 1
}
