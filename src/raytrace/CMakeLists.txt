# SPDX-License-Identifier: MIT
# Copyright (c) 2026 oknyshuk

add_library(raytrace STATIC
    raytrace.cpp
    trace2.cpp
)
target_link_libraries(raytrace PRIVATE ks_common )
target_include_directories(raytrace PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../utils/common")
