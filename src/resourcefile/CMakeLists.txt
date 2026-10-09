# SPDX-License-Identifier: MIT
# Copyright (c) 2026 oknyshuk

add_library(resourcefile STATIC
    resourcedictionary.cpp
    resourcefile.cpp
    resourceintrospection.cpp
    resourcestream.cpp
)
target_link_libraries(resourcefile PRIVATE ks_common )
