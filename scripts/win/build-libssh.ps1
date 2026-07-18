<#
.SYNOPSIS
    Vendor + build libssh (server-capable) into ssh.lib/ssh.dll for the Windows
    build and CI. libssh's own portfile source (libssh.org) is sometimes
    unreachable, so this clones the GitHub mirror and builds it with CMake against
    the vcpkg OpenSSL/zlib, enabling the embedded SSH/SFTP server (tools/SshServer).

.DESCRIPTION
    Requires an active MSVC environment + Ninja + a vcpkg install of openssl/zlib.
    Emits $Out\_build\src\ssh.lib (+ ssh.dll) and reports the include paths.

.PARAMETER Out         Directory to clone/build libssh into.
.PARAMETER VcpkgToolchain  Path to vcpkg.cmake (default: $env:VCPKG_INSTALLATION_ROOT).
#>
param(
    [Parameter(Mandatory)] [string]$Out,
    [string]$VcpkgToolchain = "$env:VCPKG_INSTALLATION_ROOT/scripts/buildsystems/vcpkg.cmake"
)
$ErrorActionPreference = "Stop"
if (-not (Test-Path (Join-Path $Out "CMakeLists.txt"))) {
    git clone --depth 1 https://github.com/libssh/libssh-mirror.git $Out
}
$build = Join-Path $Out "_build"
# CMAKE_POLICY_VERSION_MINIMUM=3.5: libssh declares cmake_minimum_required 3.3,
# which CMake 4.x rejects; this shims it without patching the port.
cmake -S $Out -B $build -G Ninja -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 `
    "-DCMAKE_TOOLCHAIN_FILE=$VcpkgToolchain" -DVCPKG_TARGET_TRIPLET=x64-windows `
    -DWITH_SERVER=ON -DWITH_SFTP=ON -DWITH_EXAMPLES=OFF -DWITH_GSSAPI=OFF `
    -DWITH_PCAP=OFF -DBUILD_SHARED_LIBS=ON -DUNIT_TESTING=OFF `
    -DCLIENT_TESTING=OFF -DSERVER_TESTING=OFF
cmake --build $build --parallel
Write-Host "SSH_LIB=$build\src\ssh.lib"
Write-Host "SSH_INCLUDE=$Out\include;$build\include"
