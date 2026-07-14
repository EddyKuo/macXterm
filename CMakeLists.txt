cmake_minimum_required(VERSION 3.21)
project(macXterm VERSION 0.1.0 LANGUAGES CXX)

# ── macXterm: native Qt + C/C++ cross-platform MobaXterm clone (MIT) ──
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

option(MACXTERM_BUILD_GUI "Build the Qt GUI executable" ON)
option(MACXTERM_BUILD_TESTS "Build unit tests" ON)

# Help CMake find Homebrew Qt / libs on macOS Apple Silicon.
if(APPLE)
  foreach(pkg qt openssl@3 libvterm libssh2 libssh freerdp)
    if(EXISTS "/opt/homebrew/opt/${pkg}")
      list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew/opt/${pkg}")
    endif()
  endforeach()
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network SerialPort Sql Test)

# QtWebEngine (optional) — embedded browser session type when present.
find_package(Qt6 QUIET COMPONENTS WebEngineWidgets)
if(Qt6WebEngineWidgets_FOUND)
  set(MACXTERM_WEBENGINE ON)
  message(STATUS "QtWebEngine found — embedded browser session enabled")
else()
  set(MACXTERM_WEBENGINE OFF)
  message(STATUS "QtWebEngine not found — Browser session falls back to system browser")
endif()

# libssh (LGPL, optional, server-capable) — embedded SSH/SFTP server.
find_library(SSH_LIB ssh HINTS /opt/homebrew/opt/libssh/lib)
find_path(SSH_INCLUDE libssh/libssh.h HINTS /opt/homebrew/opt/libssh/include)
if(SSH_LIB AND SSH_INCLUDE)
  set(MACXTERM_LIBSSH ON)
  message(STATUS "libssh found — embedded SSH/SFTP server enabled")
else()
  set(MACXTERM_LIBSSH OFF)
  message(STATUS "libssh not found — SSH/SFTP server disabled")
endif()

# ── Third-party C libs (all permissive / MIT-compatible per ADR_6) ──
find_package(PkgConfig QUIET)

# OpenSSL (Apache-2.0) — crypto for the credential vault. Only pin the Homebrew
# path on macOS; on Linux/Windows let find_package use the system/vcpkg install.
if(APPLE AND EXISTS "/opt/homebrew/opt/openssl@3")
  set(OPENSSL_ROOT_DIR "/opt/homebrew/opt/openssl@3")
endif()
find_package(OpenSSL REQUIRED)

# libvterm (MIT), libssh2 (BSD) — located via prefix hints.
# Argon2id KDF comes from OpenSSL 3's EVP_KDF (no separate argon2 dependency).
find_library(VTERM_LIB vterm REQUIRED HINTS /opt/homebrew/opt/libvterm/lib)
find_path(VTERM_INCLUDE vterm.h REQUIRED HINTS /opt/homebrew/opt/libvterm/include)
find_library(SSH2_LIB ssh2 REQUIRED HINTS /opt/homebrew/opt/libssh2/lib)
find_path(SSH2_INCLUDE libssh2.h REQUIRED HINTS /opt/homebrew/opt/libssh2/include)

# FreeRDP (Apache-2.0, optional) — real RDP protocol integration when present.
find_library(FREERDP3_LIB freerdp3 HINTS /opt/homebrew/opt/freerdp/lib)
find_library(FREERDP_CLIENT3_LIB freerdp-client3 HINTS /opt/homebrew/opt/freerdp/lib)
find_library(WINPR3_LIB winpr3 HINTS /opt/homebrew/opt/freerdp/lib)
find_path(FREERDP3_INCLUDE freerdp/freerdp.h HINTS /opt/homebrew/opt/freerdp/include/freerdp3)
if(FREERDP3_LIB AND FREERDP_CLIENT3_LIB AND FREERDP3_INCLUDE)
  set(MACXTERM_FREERDP ON)
  message(STATUS "FreeRDP found — RDP protocol integration enabled")
else()
  set(MACXTERM_FREERDP OFF)
  message(STATUS "FreeRDP not found — RDP builds as scaffold")
endif()

add_subdirectory(src)

if(MACXTERM_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
