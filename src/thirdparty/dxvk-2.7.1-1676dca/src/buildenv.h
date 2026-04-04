#pragma once

// Generated buildenv header stub for WAF builds
#if defined(__clang__)
#define BUILD_COMPILER "clang"
#elif defined(__GNUC__)
#define BUILD_COMPILER "gcc"
#else
#define BUILD_COMPILER "unknown"
#endif
#define BUILD_COMPILER_VERSION __VERSION__
#define BUILD_TARGET "x86_64"

#define DXVK_TARGET BUILD_TARGET
#define DXVK_COMPILER BUILD_COMPILER
#define DXVK_COMPILER_VERSION BUILD_COMPILER_VERSION
