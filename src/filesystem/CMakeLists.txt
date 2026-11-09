# SPDX-License-Identifier: MIT
# Copyright (c) 2026 oknyshuk

add_library(filesystem_stdio OBJECT
    basefilesystem.cpp
    basefilesystemasync.cpp
    filetracker.cpp
    filesystem_stdio.cpp
    ../public/kevvaluescompiler.cpp
    ../public/zip_utils.cpp
    ${CMAKE_SOURCE_DIR}/platform/linux/linux_support.cpp
)
target_link_libraries(filesystem_stdio PRIVATE ks_common tier0 memoverride tier1 tier2 interfaces vstdlib vpk)
target_compile_definitions(filesystem_stdio PRIVATE FILESYSTEM_STDIO_EXPORTS=1)
