# SPDX-License-Identifier: MIT
# Copyright (c) 2026 oknyshuk

pkg_check_modules(JPEG REQUIRED IMPORTED_TARGET libjpeg)

add_library(ks_jpeg STATIC ks.jpeg-libjpeg.cpp)
target_sources(ks_jpeg PUBLIC
    FILE_SET CXX_MODULES
    FILES ks.jpeg.cppm)
target_link_libraries(ks_jpeg PRIVATE ks_common_base PkgConfig::JPEG)
