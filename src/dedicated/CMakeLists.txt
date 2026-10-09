# SPDX-License-Identifier: MIT
# Copyright (c) 2026 oknyshuk

add_library(dedicated OBJECT
    console/textconsole.cpp
    console/TextConsoleUnix.cpp
    sys_common.cpp
    sys_ded.cpp
    sys_linux.cpp
    sys_subproc.cpp
)
target_link_libraries(dedicated PRIVATE ks_common tier0 memoverride tier1 tier2 tier3 interfaces appframework vstdlib mathlib)
target_include_directories(dedicated PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/.")
