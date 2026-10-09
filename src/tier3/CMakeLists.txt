# SPDX-License-Identifier: MIT
# Copyright (c) 2026 oknyshuk

add_library(tier3 STATIC
    tier3.cpp
    mdlutils.cpp
    choreoutils.cpp
    scenetokenprocessor.cpp
    studiohdrstub.cpp)

target_link_libraries(tier3 PRIVATE ks_common)
