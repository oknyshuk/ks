# SPDX-License-Identifier: MIT
# Copyright (c) 2026 oknyshuk

add_library(datacache OBJECT
    datacache.cpp
    mdlcache.cpp
    mdlcombine.cpp
    vtfcombine.cpp
    resourceaccesscontrol.cpp
    precachesystem.cpp
    ../public/studio_virtualmodel.cpp
    ../utils/common/datalinker.cpp
)
target_link_libraries(datacache PRIVATE ks_common tier0 memoverride tier1 tier2 tier3 interfaces vstdlib mathlib)
target_compile_definitions(datacache PRIVATE MDLCACHE_DLL_EXPORT=1)
