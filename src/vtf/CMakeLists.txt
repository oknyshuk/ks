# SPDX-License-Identifier: MIT
# Copyright (c) 2026 oknyshuk

add_library(vtf STATIC vtf.cpp)
target_link_libraries(vtf PRIVATE ks_common)
