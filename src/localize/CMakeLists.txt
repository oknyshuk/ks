# SPDX-License-Identifier: MIT
# Copyright (c) 2026 oknyshuk

add_library(localize OBJECT
    localize.cpp
    ../public/UnicodeFileHelpers.cpp
)
target_link_libraries(localize PRIVATE ks_common tier0 memoverride public_common tier1 tier2 interfaces vstdlib)
