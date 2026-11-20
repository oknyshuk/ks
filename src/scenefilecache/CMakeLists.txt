# SPDX-License-Identifier: MIT
# Copyright (c) 2026 oknyshuk

add_library(scenefilecache OBJECT
    SceneFileCache.cpp
)
target_link_libraries(scenefilecache PRIVATE ks_common tier0 memoverride tier1 interfaces vstdlib)
