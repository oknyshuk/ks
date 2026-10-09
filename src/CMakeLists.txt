cmake_minimum_required(VERSION 3.28)
project(ks LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL3 REQUIRED IMPORTED_TARGET sdl3)
pkg_check_modules(FREETYPE REQUIRED IMPORTED_TARGET freetype2)
pkg_check_modules(ZLIB REQUIRED IMPORTED_TARGET zlib)
pkg_check_modules(JPEG REQUIRED IMPORTED_TARGET libjpeg)
pkg_check_modules(PNG REQUIRED IMPORTED_TARGET libpng)

file(GLOB _ks_dxvk "${CMAKE_SOURCE_DIR}/thirdparty/dxvk-*")
file(GLOB _ks_dxmath "${CMAKE_SOURCE_DIR}/thirdparty/DirectXMath-*")

add_library(ks_common INTERFACE)
target_include_directories(ks_common INTERFACE
    "${CMAKE_SOURCE_DIR}/public"
    "${CMAKE_SOURCE_DIR}/public/tier0"
    "${CMAKE_SOURCE_DIR}/public/tier1"
    "${CMAKE_SOURCE_DIR}/common"
    "${_ks_dxvk}/include/native/windows"
    "${_ks_dxvk}/include/native/directx")
target_include_directories(ks_common SYSTEM INTERFACE "${_ks_dxmath}/Inc")
target_link_libraries(ks_common INTERFACE PkgConfig::SDL3 PkgConfig::FREETYPE)
target_compile_definitions(ks_common INTERFACE
    COMPILER_GCC=1 _POSIX=1 POSIX=1 USE_SDL=1 _LINUX=1 LINUX=1 GNUC=1
    _DLL_EXT=.so CSTRIKE15=1 CSTRIKE_REL_BUILD=1 VPROF_LEVEL=1
    RAD_TELEMETRY_DISABLED=1 DX_TO_VK_ABSTRACTION=1 PLATFORM_64BITS=1
    NDEBUG=1 HAVE_SDL3=1 HAVE_FREETYPE=1 HAVE_ZLIB=1 HAVE_JPEG=1 HAVE_PNG=1)
target_compile_options(ks_common INTERFACE
    -march=x86-64-v3 -fvisibility=hidden -fdiagnostics-color=always
    $<$<COMPILE_LANGUAGE:CXX>:-freflection;-Wno-invalid-offsetof;-Wno-free-nonheap-object>)
target_link_options(ks_common INTERFACE -fuse-ld=mold)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Dev CACHE STRING "" FORCE)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
elseif(CMAKE_BUILD_TYPE STREQUAL "MinSizeRel" OR CMAKE_BUILD_TYPE STREQUAL "None")
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Dev")
    target_compile_options(ks_common INTERFACE -O1 -g -gsplit-dwarf)
    target_link_options(ks_common INTERFACE -gsplit-dwarf)
elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(ks_common INTERFACE -O1 -g -gsplit-dwarf -fno-omit-frame-pointer)
    target_link_options(ks_common INTERFACE -gsplit-dwarf)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(ks_common INTERFACE -O3 -fno-semantic-interposition)
    target_link_options(ks_common INTERFACE -fno-semantic-interposition)
elseif(CMAKE_BUILD_TYPE STREQUAL "Sanitize")
    target_compile_options(ks_common INTERFACE -O0 -g -gsplit-dwarf -fno-omit-frame-pointer
        -fsanitize=address,undefined -pthread)
    target_link_options(ks_common INTERFACE -gsplit-dwarf -fsanitize=address,undefined -pthread)
elseif(CMAKE_BUILD_TYPE STREQUAL "Tsan")
    target_compile_options(ks_common INTERFACE -O1 -g -gsplit-dwarf -fno-omit-frame-pointer
        -fsanitize=thread -pthread)
    target_link_options(ks_common INTERFACE -gsplit-dwarf -fsanitize=thread -pthread)
else()
    message(FATAL_ERROR "Unknown CMAKE_BUILD_TYPE '${CMAKE_BUILD_TYPE}' (Dev/Debug/Release/Sanitize/Tsan)")
endif()

add_library(memoverride OBJECT public/tier0/memoverride.cpp)
target_link_libraries(memoverride PRIVATE ks_common)

add_library(public_common STATIC
    public/filesystem_helpers.cpp public/collisionutils.cpp public/filesystem_init.cpp
    public/imagepacker.cpp public/networkvar.cpp public/registry.cpp
    public/scratchpad3d.cpp public/dt_utlvector_common.cpp public/XZip.cpp public/XUnzip.cpp
    common/steamid.cpp common/platforminputdevice.cpp common/randoverride.cpp)
target_link_libraries(public_common PRIVATE ks_common)

add_subdirectory(appframework)
add_subdirectory(bitmap)
add_subdirectory(bonesetup)
add_subdirectory(choreoobjects)
add_subdirectory(d3dx)
add_subdirectory(dedicated_main)
add_subdirectory(dmxloader)
add_subdirectory(interfaces)
add_subdirectory(launcher_main)
add_subdirectory(materialsystem/shaderlib)
add_subdirectory(mathlib)
add_subdirectory(particles)
add_subdirectory(raytrace)
add_subdirectory(resourcefile)
add_subdirectory(responserules/runtime)
add_subdirectory(soundsystem/lowlevel)
add_subdirectory(thirdparty)
add_subdirectory(tier0)
add_subdirectory(tier1)
add_subdirectory(tier2)
add_subdirectory(tier3)
add_subdirectory(utils/bzip2)
add_subdirectory(videocfg)
add_subdirectory(vpklib)
add_subdirectory(vstdlib)
add_subdirectory(vtf)
add_subdirectory(datacache)
add_subdirectory(dedicated)
add_subdirectory(engine/voice_codecs/celt)
add_subdirectory(filesystem)
add_subdirectory(inputsystem)
add_subdirectory(launcher)
add_subdirectory(localize)
add_subdirectory(matchmaking)
add_subdirectory(materialsystem)
add_subdirectory(materialsystem/shaderapidx9)
add_subdirectory(materialsystem/shaderapiempty)
add_subdirectory(materialsystem/stdshaders)
add_subdirectory(rocketui)
add_subdirectory(scenefilecache)
add_subdirectory(soundemittersystem)
add_subdirectory(studiorender)
add_subdirectory(vphysics)
add_subdirectory(vscript)
add_subdirectory(engine)
add_subdirectory(game/client)
add_subdirectory(game/server)
