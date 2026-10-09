# SPDX-License-Identifier: MIT
# Copyright (c) 2026 oknyshuk

add_library(vstdlib OBJECT
    cvar.cpp
    jobthread.cpp
    keyvaluessystem.cpp
    random.cpp
    vcover.cpp
    eventsystem.cpp
    vstrtools.cpp
    vstdlib_strtools.cpp)

target_link_libraries(vstdlib PRIVATE ks_common)
target_link_libraries(vstdlib PUBLIC tier0 memoverride tier1 interfaces)
target_compile_definitions(vstdlib PRIVATE VSTDLIB_DLL_EXPORT=1)

add_library(ks_vstdlib STATIC)
target_sources(ks_vstdlib PUBLIC
    FILE_SET CXX_MODULES
    FILES ks.vstdlib.cppm
          ks.vstdlib-ikeyvaluessystem.cppm
          ks.vstdlib-vcover.cppm
          ks.vstdlib-vstrtools.cppm
          ks.vstdlib-jobthread.cppm
          ks.vstdlib-cvar.cppm
          ks.vstdlib-ieventsystem.cppm
          ks.vstdlib-iprocessutils.cppm
          ks.vstdlib-callqueue.cppm)
target_link_libraries(ks_vstdlib PUBLIC ks_tier1 PRIVATE ks_common_base)
set_property(TARGET ks_vstdlib PROPERTY CXX_SCAN_FOR_MODULES ON)
