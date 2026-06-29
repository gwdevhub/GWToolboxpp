param([string]$Path, [string]$Config)
if ($Config -ne 'RelWithDebInfo') { exit 0 }
$gz = $Path + '.gz'
if (Test-Path $gz) { Remove-Item $gz }
$in  = [System.IO.File]::OpenRead($Path)
$out = [System.IO.File]::Create($gz)
$gs  = [System.IO.Compression.GZipStream]::new($out, [System.IO.Compression.CompressionMode]::Compress)
$in.CopyTo($gs)
$gs.Dispose(); $out.Dispose(); $in.Dispose()
Write-Host "Compressed: $gz"