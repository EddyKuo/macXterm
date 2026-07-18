<#
.SYNOPSIS
    Stage a runnable/portable macXterm for Windows: the built exe plus its Qt and
    vcpkg runtime DLLs, ready to zip or feed to the NSIS installer (macxterm.nsi).

.DESCRIPTION
    Run from an x64 Native Tools (vcvars64) prompt so windeployqt is on PATH, or
    pass -QtBin. Produces dist\macXterm\ (portable) and dist\macXterm-portable.zip.

    Code signing is intentionally NOT done here — it needs an Authenticate code
    certificate you supply out of band:
        signtool sign /fd SHA256 /a /tr http://timestamp.digicert.com \
                 /td SHA256 dist\macXterm\macXterm.exe

.PARAMETER BuildDir   CMake build directory (default: build)
.PARAMETER QtBin      Qt bin dir containing windeployqt (default: C:\Qt\6.8.1\msvc2022_64\bin)
.PARAMETER VcpkgBin   vcpkg installed bin dir (default: C:\vcpkg\installed\x64-windows\bin)
#>
param(
    [string]$BuildDir  = "build",
    [string]$QtBin     = "C:\Qt\6.8.1\msvc2022_64\bin",
    [string]$VcpkgBin  = "C:\vcpkg\installed\x64-windows\bin",
    [string]$LibsshBin = ""   # dir with the vendored ssh.dll (embedded SSH server)
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)   # repo root
$exe  = Join-Path $root "$BuildDir\src\macXterm.exe"
if (-not (Test-Path $exe)) { throw "macXterm.exe not found at $exe — build first." }

$dist = Join-Path $root "dist\macXterm"
if (Test-Path $dist) { Remove-Item -Recurse -Force $dist }
New-Item -ItemType Directory -Force -Path $dist | Out-Null
Copy-Item $exe $dist

# Qt runtime (plugins, platform, DLLs).
$windeployqt = Join-Path $QtBin "windeployqt.exe"
& $windeployqt --release --no-translations --compiler-runtime (Join-Path $dist "macXterm.exe")

# vcpkg runtime DLLs (libssh2, openssl (libcrypto), zlib, ...).
if (Test-Path $VcpkgBin) {
    Get-ChildItem "$VcpkgBin\*.dll" | ForEach-Object { Copy-Item $_.FullName $dist -Force }
}

# Vendored libssh runtime (ssh.dll + its libcrypto/zlib) for the embedded SSH server.
if ($LibsshBin -and (Test-Path $LibsshBin)) {
    Get-ChildItem "$LibsshBin\*.dll" | ForEach-Object { Copy-Item $_.FullName $dist -Force }
}

# Optional bundled Unix userland (BusyBox) for the local Unix terminal.
$userland = Join-Path $root "assets\win-userland"
if (Test-Path $userland) {
    Copy-Item $userland (Join-Path $dist "userland") -Recurse -Force
}

$zip = Join-Path $root "dist\macXterm-portable.zip"
if (Test-Path $zip) { Remove-Item -Force $zip }
Compress-Archive -Path "$dist\*" -DestinationPath $zip
Write-Host "Portable build:  $dist"
Write-Host "Portable zip:    $zip"
Write-Host "Installer:       run 'makensis scripts\win\macxterm.nsi' after this (needs NSIS)."
