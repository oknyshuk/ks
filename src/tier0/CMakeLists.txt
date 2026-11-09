# SPDX-License-Identifier: MIT
# Copyright (c) 2026 oknyshuk

add_library(tier0 OBJECT
    assert_dialog.cpp
    commandline.cpp
    fasttimer.cpp
    cpu.cpp
    cpumonitoring.cpp
    dbg.cpp
    logging.cpp
    mem.cpp
    memstd.cpp
    dynfunction.cpp
    minidump.cpp
    tier0_strtools.cpp
    perfstats.cpp
    platwindow.cpp
    progressbar.cpp
    stacktools.cpp
    systeminformation.cpp
    threadtools.cpp
    tslist.cpp
    vprof.cpp
    win32consoleio.cpp
    cpu_posix.cpp
    ${CMAKE_SOURCE_DIR}/platform/posix/platform_posix.cpp
    l2cache.cpp)

target_link_libraries(tier0 PRIVATE ks_common)
target_compile_definitions(tier0 PRIVATE TIER0_DLL_EXPORT=1)
set_property(TARGET tier0 PROPERTY CXX_MODULE_STD ON)

add_library(ks_core STATIC)
target_sources(ks_core PUBLIC
    FILE_SET CXX_MODULES
    FILES ks.core.cppm
          ks.core-basetypes.cppm
          ks.core-cache_hints.cppm
          ks.core-cmdline.cppm
          ks.core-color.cppm
          ks.core-console.cppm
          ks.core-constants.cppm
          ks.core-cpu.cppm
          ks.core-dbg.cppm
          ks.core-dynfunction.cppm
          ks.core-etwprof.cppm
          ks.core-l2cache.cppm
          ks.core-logging.cppm
          ks.core-mem.cppm
          ks.core-memalloc.cppm
          ks.core-microprofiler.cppm
          ks.core-minidump.cppm
          ks.core-numeric.cppm
          ks.core-perfstats.cppm
          ks.core-platform.cppm
          ks.core-platwindow.cppm
          ks.core-prelude.cppm
          ks.core-random.cppm
          ks.core-progress.cppm
          ks.core-scratcharena.cppm
          ks.core-stacktools.cppm
          ks.core-sysinfo.cppm
          ks.core-threadtools.cppm
          ks.core-time.cppm
          ks.core-tslist.cppm
          ks.core-types.cppm
          ks.core-vprof.cppm
          ks.core-wchartypes.cppm)
target_link_libraries(ks_core PUBLIC ks_platform PRIVATE ks_common_base)
target_compile_definitions(ks_core PRIVATE TIER0_DLL_EXPORT=1)
set_property(TARGET ks_core PROPERTY CXX_SCAN_FOR_MODULES ON)
