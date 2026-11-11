# Builds the plugin and produces the release packages in release\<Version>\ (nothing is published).
#   powershell -NoProfile -ExecutionPolicy Bypass -File scripts\package.ps1 [-EswcHostZip <zip>] [-SunriseHostZip <zip>] [-Version x.y.z] [-HostVersion x.y.z] [-SkipBuild]
param(
    [string]$EswcHostZip,
    [string]$SunriseHostZip,
    [string]$Version,
    [string]$HostVersion,
    [switch]$SkipBuild
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

$root = Split-Path -Parent $PSScriptRoot
if (-not $Version) { $Version = (Get-Content -Raw (Join-Path $root 'src\plugin\plugin.json') | ConvertFrom-Json).version }
# Default: the newest Horizon-Host-<version>-ESWC/Sunrise.zip in horizon-host\dist\release
function Newest([string]$game) {
    $dir = 'C:\Users\osaro\Downloads\horizon-host\dist\release'
    $f = Get-ChildItem $dir -Filter "Horizon-Host-*-$game.zip" | Sort-Object { [version]($_.Name -replace '^Horizon-Host-(\d+(\.\d+)+)-.*$', '$1') } | Select-Object -Last 1
    if (-not $f) { throw "No Horizon-Host-*-$game.zip in $dir" }
    $f.FullName
}
if (-not $EswcHostZip) { $EswcHostZip = Newest 'ESWC' }
if (-not $SunriseHostZip) { $SunriseHostZip = Newest 'Sunrise' }
foreach ($z in @($EswcHostZip, $SunriseHostZip)) { if (-not (Test-Path $z)) { throw "Host zip not found: $z" } }
if (-not $HostVersion) {
    if ((Split-Path -Leaf $EswcHostZip) -match 'Host-(\d+(\.\d+)+)') { $HostVersion = $Matches[1] }
    else { throw 'Cannot read the host version from the zip name; pass -HostVersion' }
}

if (-not $SkipBuild) {
    & cmd /c "`"$root\build.cmd`"" | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'build.cmd failed' }
}

$dist = Join-Path $root 'dist'
$out = Join-Path $root "release\$Version"
if (Test-Path $out) { Remove-Item -Recurse -Force $out }
New-Item -ItemType Directory -Force $out | Out-Null

# Check plugin.json against the files
$pj = Get-Content -Raw (Join-Path $dist 'plugin.json') | ConvertFrom-Json
if ($pj.version -ne $Version) { throw "dist\plugin.json version $($pj.version) != $Version" }
foreach ($n in @($pj.dll, $pj.icon)) { if (-not (Test-Path (Join-Path $dist $n))) { throw "plugin.json names $n, missing in dist" } }
$id = $pj.id

# Text files with placeholders
function Fill([string]$src, [string]$dst) {
    $t = [IO.File]::ReadAllText($src, [Text.Encoding]::UTF8).Replace('{VERSION}', $Version).Replace('{HOSTVERSION}', $HostVersion)
    $t = $t -replace "`r`n", "`n"
    [IO.File]::WriteAllText($dst, $t, (New-Object Text.UTF8Encoding($false)))
}
foreach ($f in 'README.de.md', 'README.en.md', 'RELEASE-DRAFT.md', 'THIRD-PARTY.txt') { Fill (Join-Path $root "packaging\$f") (Join-Path $out $f) }

# Entries: name -> bytes (sorted, fixed timestamp => same input gives the same zip)
$stamp = [DateTimeOffset]::new(2020, 1, 1, 0, 0, 0, [TimeSpan]::Zero)
function New-Zip([string]$path, [hashtable]$entries) {
    $fs = [IO.File]::Create($path)
    $za = New-Object IO.Compression.ZipArchive($fs, [IO.Compression.ZipArchiveMode]::Create)
    foreach ($name in ($entries.Keys | Sort-Object { $_ } -CaseSensitive)) {
        $e = $za.CreateEntry($name, [IO.Compression.CompressionLevel]::Optimal)
        $e.LastWriteTime = $stamp
        $s = $e.Open(); $b = $entries[$name]; $s.Write($b, 0, $b.Length); $s.Dispose()
    }
    $za.Dispose(); $fs.Dispose()
}
function Bytes([string]$p) { [IO.File]::ReadAllBytes($p) }

$plugin = @{}
$plugin["horizon/plugins/$id/$($pj.dll)"] = Bytes (Join-Path $dist $pj.dll)
$plugin["horizon/plugins/$id/plugin.json"] = Bytes (Join-Path $dist 'plugin.json')
$plugin["horizon/plugins/$id/$($pj.icon)"] = Bytes (Join-Path $dist $pj.icon)
$plugin["horizon/plugins/$id/LICENSE.txt"] = Bytes (Join-Path $root 'LICENSE')
$plugin["horizon/plugins/$id/THIRD-PARTY.txt"] = Bytes (Join-Path $out 'THIRD-PARTY.txt')

$pluginZip = Join-Path $out "LegacyRandomizer-$Version-plugin.zip"
New-Zip $pluginZip $plugin

function Host-Entries([string]$zip) {
    $h = @{}
    $za = [IO.Compression.ZipFile]::OpenRead($zip)
    foreach ($e in $za.Entries) {
        if ($e.FullName.EndsWith('/')) { continue }
        $ms = New-Object IO.MemoryStream; $s = $e.Open(); $s.CopyTo($ms); $s.Dispose()
        $h[$e.FullName] = $ms.ToArray()
    }
    $za.Dispose(); $h
}
$bundles = @()
foreach ($b in @(@('ESWC', $EswcHostZip), @('Sunrise', $SunriseHostZip))) {
    $e = Host-Entries $b[1]
    foreach ($k in $plugin.Keys) { $e[$k] = $plugin[$k] }
    $e['README.en.md'] = Bytes (Join-Path $out 'README.en.md')
    $e['README.de.md'] = Bytes (Join-Path $out 'README.de.md')
    $p = Join-Path $out "LegacyRandomizer-$Version-$($b[0])-with-Horizon-Host.zip"
    New-Zip $p $e
    $bundles += $p
}

# SHA256SUMS.txt (zips and the DLL)
$lines = @()
foreach ($f in @($pluginZip) + $bundles) { $lines += '{0} *{1}' -f (Get-FileHash $f -Algorithm SHA256).Hash.ToLower(), (Split-Path -Leaf $f) }
$lines += '{0} *{1}' -f (Get-FileHash (Join-Path $dist $pj.dll) -Algorithm SHA256).Hash.ToLower(), $pj.dll
[IO.File]::WriteAllText((Join-Path $out 'SHA256SUMS.txt'), (($lines -join "`n") + "`n"), (New-Object Text.UTF8Encoding($false)))

Get-ChildItem $out | Select-Object Name, Length | Format-Table -AutoSize
Write-Host "Done: $out (version $Version, host $HostVersion)"
