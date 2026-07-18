<#
.SYNOPSIS
    Build libvterm into a static vterm.lib with MSVC. libvterm has no vcpkg port and
    no upstream CMake, but the neovim fork's src/*.c compiles directly (the encoding
    tables are pre-generated .inc files — no Perl needed). Used by the Windows build
    and CI to satisfy CMake's find_library(vterm) / find_path(vterm.h).

.DESCRIPTION
    Requires an active MSVC environment (cl.exe + lib.exe on PATH) — e.g. run after
    ilammy/msvc-dev-cmd in CI, or from an x64 Native Tools prompt locally.

.PARAMETER Src  libvterm checkout (contains src/ and include/)
.PARAMETER Out  output directory for the objects + vterm.lib
#>
param(
    [Parameter(Mandatory)] [string]$Src,
    [Parameter(Mandatory)] [string]$Out
)
$ErrorActionPreference = "Stop"
New-Item -ItemType Directory -Force -Path $Out | Out-Null

$cfiles = Get-ChildItem (Join-Path $Src "src\*.c") | ForEach-Object { $_.FullName }
if (-not $cfiles) { throw "No src/*.c under $Src — is this a libvterm checkout?" }

Push-Location $Src
try {
    & cl /nologo /c /O2 /MD /utf-8 /D_CRT_SECURE_NO_WARNINGS `
        "/Iinclude" "/Isrc" $cfiles "/Fo$Out\"
    if ($LASTEXITCODE -ne 0) { throw "cl failed ($LASTEXITCODE)" }
    $objs = Get-ChildItem (Join-Path $Out "*.obj") | ForEach-Object { $_.FullName }
    & lib /nologo "/OUT:$Out\vterm.lib" $objs
    if ($LASTEXITCODE -ne 0) { throw "lib failed ($LASTEXITCODE)" }
}
finally { Pop-Location }
Write-Host "Built: $Out\vterm.lib  (include: $Src\include)"
