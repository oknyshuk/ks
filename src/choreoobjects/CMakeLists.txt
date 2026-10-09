# SPDX-License-Identifier: MIT
# Copyright (c) 2026 oknyshuk

add_library(choreoobjects STATIC
    ../game/shared/choreoactor.cpp
    ../game/shared/choreochannel.cpp
    ../game/shared/choreoevent.cpp
    ../game/shared/choreoscene.cpp
    ../game/shared/sceneimage.cpp
)
target_link_libraries(choreoobjects PRIVATE ks_common )
target_include_directories(choreoobjects PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../game/shared" "${CMAKE_CURRENT_SOURCE_DIR}/../utils/common")
