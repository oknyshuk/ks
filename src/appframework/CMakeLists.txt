add_library(appframework STATIC
    AppSystemGroup.cpp
    ../public/filesystem_init.cpp
    posixapp.cpp
    sdlmgr.cpp)

target_link_libraries(appframework PRIVATE ks_common)
