# SPDX-License-Identifier: MIT
# Copyright (c) 2026 oknyshuk

add_library(videocfg STATIC
    videocfg.cpp
)
target_link_libraries(videocfg PRIVATE ks_common )
