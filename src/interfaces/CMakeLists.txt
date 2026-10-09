# SPDX-License-Identifier: MIT
# Copyright (c) 2026 oknyshuk

add_library(interfaces STATIC interfaces.cpp)
target_link_libraries(interfaces PRIVATE ks_common)
