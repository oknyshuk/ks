# SPDX-License-Identifier: MIT
# Copyright (c) 2026 oknyshuk

add_library(ks_simd STATIC)
target_sources(ks_simd PUBLIC
    FILE_SET CXX_MODULES
    FILES ks.simd.cppm)
target_link_libraries(ks_simd PRIVATE ks_common_base)
