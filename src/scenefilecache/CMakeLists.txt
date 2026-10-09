add_library(scenefilecache OBJECT
    SceneFileCache.cpp
)
target_link_libraries(scenefilecache PRIVATE ks_common tier0 memoverride tier1 interfaces vstdlib)
target_include_directories(scenefilecache PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../game/shared")
