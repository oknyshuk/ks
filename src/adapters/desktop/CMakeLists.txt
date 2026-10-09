# SPDX-License-Identifier: MIT
# Copyright (c) 2026 oknyshuk

add_library(ks_desktop STATIC ks.desktop-sdl.cpp)
target_sources(ks_desktop PUBLIC
    FILE_SET CXX_MODULES
    FILES ks.desktop.cppm)
target_link_libraries(ks_desktop PRIVATE ks_common_base PkgConfig::SDL3)
