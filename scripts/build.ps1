# Cross-platform build entry point for Windows (PowerShell). Mirrors
# scripts/build.sh. Assumes Qt6, OpenSSL, libvterm, and libssh2 are available
# (e.g. via vcpkg); the Pty layer uses ConPTY and SSH uses Winsock on Windows.
param(
    [string]$BuildType = "Debug",
    [string]$BuildDir  = "build"
)
$ErrorActionPreference = "Stop"

cmake -S . -B $BuildDir -DCMAKE_BUILD_TYPE=$BuildType
cmake --build $BuildDir --parallel

Push-Location $BuildDir
try {
    $env:QT_QPA_PLATFORM = "offscreen"
    ctest --output-on-failure
} finally {
    Pop-Location
}
Write-Host "Build + tests complete ($BuildType)."
