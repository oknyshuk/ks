# SPDX-License-Identifier: MIT
# Copyright (c) 2026 oknyshuk

add_library(choreoobjects STATIC
    choreoactor.cpp
    choreochannel.cpp
    choreoevent.cpp
    choreoscene.cpp
    sceneimage.cpp
)
target_link_libraries(choreoobjects PRIVATE ks_common )
target_include_directories(choreoobjects PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../utils/common")
