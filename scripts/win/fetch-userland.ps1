<#
.SYNOPSIS
    Fetch a BusyBox-for-Windows binary into assets/win-userland/busybox.exe so the
    "New Local Unix Terminal" feature (and the packager) can bundle it.

.DESCRIPTION
    macXterm does NOT commit a BusyBox binary: BusyBox is GPL, so shipping it is a
    licensing decision to record in an ADR, and the binary should be fetched from a
    source you trust. This script automates that fetch once you've chosen one.

    Default source is the rmyorston/busybox-w32 project (the de-facto Windows
    BusyBox). Override -Url to pin a specific build/version/mirror you trust.

    The binary must be invoked as a SEPARATE PROCESS (never linked), keeping the
    project's "no GPL code linked" rule intact — same as the Mosh decision.

.PARAMETER Url   Direct URL to a busybox*.exe (default: busybox-w32 64-bit build).
.PARAMETER Dest  Where to write it (default: assets/win-userland/busybox.exe).
#>
param(
    [string]$Url  = "https://frigidcode.com/busybox/busybox64.exe",
    [string]$Dest = ""
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (-not $Dest) { $Dest = Join-Path $root "assets\win-userland\busybox.exe" }
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Dest) | Out-Null

Write-Host "Fetching BusyBox from: $Url"
try {
    Invoke-WebRequest -Uri $Url -OutFile $Dest -UseBasicParsing
} catch {
    Write-Error @"
Download failed ($_).
Pick a BusyBox-for-Windows build you trust and pass it via -Url, e.g. a release
from https://github.com/rmyorston/busybox-w32 or a mirror, then re-run. Record the
source + licensing (GPL, invoked as a separate process) in an ADR before shipping.
"@
    exit 1
}
Write-Host "Wrote $Dest ($([math]::Round((Get-Item $Dest).Length/1KB)) KB)."
Write-Host "The packager (package.ps1) will now copy it into the app's userland/ folder."
