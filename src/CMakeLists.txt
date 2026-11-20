# SPDX-License-Identifier: MIT
# Copyright (c) 2026 oknyshuk

cmake_minimum_required(VERSION 4.4)
if(CMAKE_VERSION VERSION_LESS 4.5)
    set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "f35a9ac6-8463-4d38-8eec-5d6008153e7d")
endif()

project(ks LANGUAGES C CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

string(APPEND CMAKE_CXX_FLAGS " -freflection -fcontracts")
set(CMAKE_CXX_MODULE_STD ON)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/../game" CACHE PATH "" FORCE)
endif()
set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    string(CONCAT CMAKE_CXX_SCANDEP_SOURCE
        "<CMAKE_CXX_COMPILER> <DEFINES> <INCLUDES> <FLAGS> -E -x c++ <SOURCE>"
        " -MT <DYNDEP_FILE> -MD -MF <DEP_FILE>"
        " -fmodules-ts -fdeps-file=<DYNDEP_FILE> -fdeps-target=<OBJECT> -fdeps-format=p1689r5"
        " -o /dev/null")
endif()

find_package(PkgConfig REQUIRED)
set(PKG_CONFIG_ARGN "--silence-errors")

pkg_check_modules(SDL3 REQUIRED IMPORTED_TARGET sdl3)

file(GLOB _ks_dxmath "${CMAKE_SOURCE_DIR}/thirdparty/DirectXMath-*")

add_library(ks_common_base INTERFACE)
target_include_directories(ks_common_base INTERFACE
    "${CMAKE_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/public"
    "${CMAKE_SOURCE_DIR}/common")
target_include_directories(ks_common_base SYSTEM INTERFACE "${_ks_dxmath}/Inc")
target_compile_definitions(ks_common_base INTERFACE
    POSIX=1 _LINUX=1 LINUX=1 CSTRIKE15=1 NDEBUG=1)
target_compile_options(ks_common_base INTERFACE
    -march=x86-64-v3 -fdiagnostics-color=always
    $<$<COMPILE_LANGUAGE:CXX>:-freflection;-fcontracts;-Wno-invalid-offsetof;-Wno-free-nonheap-object>
    # GCC 16 crashes or misreads lazily loaded module data (ICEs in implicitly_declare_fn,
    # build_special_member_call); loading imports eagerly avoids that.
    $<$<COMPILE_LANGUAGE:CXX>:-fno-module-lazy>)
target_link_options(ks_common_base INTERFACE -fuse-ld=mold)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Dev CACHE STRING "" FORCE)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
elseif(CMAKE_BUILD_TYPE STREQUAL "MinSizeRel" OR CMAKE_BUILD_TYPE STREQUAL "None")
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Dev")
    target_compile_options(ks_common_base INTERFACE -O1 -g -gsplit-dwarf)
    target_link_options(ks_common_base INTERFACE -gsplit-dwarf)
elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(ks_common_base INTERFACE -O1 -g -gsplit-dwarf -fno-omit-frame-pointer)
    target_link_options(ks_common_base INTERFACE -gsplit-dwarf)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(ks_common_base INTERFACE -O3 -fno-semantic-interposition)
    target_link_options(ks_common_base INTERFACE -fno-semantic-interposition)
elseif(CMAKE_BUILD_TYPE STREQUAL "Sanitize")
    target_compile_options(ks_common_base INTERFACE -O0 -g -gsplit-dwarf -fno-omit-frame-pointer
        -fsanitize=address,undefined -pthread)
    target_link_options(ks_common_base INTERFACE -gsplit-dwarf -fsanitize=address,undefined -pthread)
elseif(CMAKE_BUILD_TYPE STREQUAL "Tsan")
    target_compile_options(ks_common_base INTERFACE -O1 -g -gsplit-dwarf -fno-omit-frame-pointer
        -fsanitize=thread -pthread)
    target_link_options(ks_common_base INTERFACE -gsplit-dwarf -fsanitize=thread -pthread)
else()
    message(FATAL_ERROR "Unknown CMAKE_BUILD_TYPE '${CMAKE_BUILD_TYPE}' (Dev/Debug/Release/Sanitize/Tsan)")
endif()

if(CMAKE_BUILD_TYPE MATCHES "^(Debug|Sanitize|Tsan)$")
    target_compile_options(ks_common_base INTERFACE $<$<COMPILE_LANGUAGE:CXX>:-fcontract-evaluation-semantic=observe>)
else()
    target_compile_options(ks_common_base INTERFACE $<$<COMPILE_LANGUAGE:CXX>:-fcontract-evaluation-semantic=ignore>)
endif()

if(NOT CMAKE_BUILD_TYPE MATCHES "^(Sanitize|Tsan)$")
    target_link_options(ks_common_base INTERFACE LINKER:-z,defs)
endif()

add_library(ks_common INTERFACE)
target_link_libraries(ks_common INTERFACE ks_common_base ks_core ks_platform ks_simd ks_math ks_tier1 ks_vstdlib)

add_library(memoverride OBJECT tier0/memoverride.cpp)
target_link_libraries(memoverride PRIVATE ks_common)

add_library(ks_shared OBJECT
    public/dt_send.cpp
    public/dt_utlvector_send.cpp
    public/editor_sendcommand.cpp)
target_link_libraries(ks_shared PRIVATE ks_common)

add_library(public_common STATIC
    public/filesystem_helpers.cpp public/collisionutils.cpp public/filesystem_init.cpp
    public/imagepacker.cpp public/networkvar.cpp public/registry.cpp
    public/dt_utlvector_common.cpp public/XZip.cpp public/XUnzip.cpp
    common/playerid.cpp common/platforminputdevice.cpp common/randoverride.cpp)
target_link_libraries(public_common PRIVATE ks_common)

add_library(game_public STATIC
    public/closedcaptions.cpp public/interpolatortypes.cpp public/keyframe/keyframe.cpp
    public/rope_physics.cpp public/server_class.cpp public/simple_physics.cpp
    public/stringregistry.cpp public/tools/bonelist.cpp)
target_link_libraries(game_public PRIVATE ks_common)

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
add_subdirectory(platform)
add_subdirectory(adapters)
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
add_subdirectory(game/shared)
add_subdirectory(game/client)
add_subdirectory(game/server)

function(ks_link_game exe)
    set(_objlibs)
    foreach(_lib IN LISTS ARGN)
        get_target_property(_deps ${_lib} LINK_LIBRARIES)
        foreach(_dep IN LISTS _deps)
            if(TARGET ${_dep})
                get_target_property(_type ${_dep} TYPE)
                if(_type STREQUAL "OBJECT_LIBRARY")
                    list(APPEND _objlibs ${_dep})
                endif()
            endif()
        endforeach()
    endforeach()
    list(REMOVE_DUPLICATES _objlibs)
    list(REMOVE_ITEM _objlibs ${ARGN})
    target_link_libraries(${exe} PRIVATE ${ARGN})
    foreach(_obj IN LISTS _objlibs)
        target_sources(${exe} PRIVATE $<TARGET_OBJECTS:${_obj}>)
    endforeach()
endfunction()

ks_link_game(srcds engine server)
ks_link_game(srceng engine server client)
foreach(_exe srceng srcds)
    target_link_libraries(${_exe} PRIVATE RmlCore d3d9 ks_jpeg PkgConfig::SDL3)
endforeach()

target_link_libraries(srceng PRIVATE shaderapidx9)
target_link_libraries(srcds PRIVATE shaderapiempty)

enable_testing()
add_subdirectory(tools/no_vtables)

install(TARGETS srceng srcds RUNTIME DESTINATION .)
