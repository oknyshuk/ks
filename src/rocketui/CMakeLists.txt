# SPDX-License-Identifier: MIT
# Copyright (c) 2026 oknyshuk

add_library(rocketui_client OBJECT
    rocketuiimpl.cpp
    rocketsystem.cpp
    rocketfilesystem.cpp
    rocketrender_d3d9.cpp
)
target_link_libraries(rocketui_client PRIVATE ks_common ks_desktop tier0 tier1 tier2 tier3 vstdlib interfaces bitmap vtf mathlib rmlui RmlCore RmlDebugger)
target_include_directories(rocketui_client PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/." "${CMAKE_CURRENT_SOURCE_DIR}/../public" "${CMAKE_CURRENT_SOURCE_DIR}/../public/rocketui")
