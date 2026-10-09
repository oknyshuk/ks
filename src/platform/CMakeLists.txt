# SPDX-License-Identifier: MIT
# Copyright (c) 2026 oknyshuk

add_library(ks_platform STATIC
    posix/ks.platform-posix.cpp)
target_sources(ks_platform PUBLIC
    FILE_SET CXX_MODULES
    FILES ks.platform.cppm)
target_link_libraries(ks_platform PRIVATE ks_common_base)
