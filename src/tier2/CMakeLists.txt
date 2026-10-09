add_library(tier2 STATIC
    beamsegdraw.cpp
    defaultfilesystem.cpp
    fileutils.cpp
    keybindings.cpp
    ../public/map_utils.cpp
    ../public/materialsystem/MaterialSystemUtil.cpp
    meshutils.cpp
    renderutils.cpp
    riff.cpp
    soundutils.cpp
    tier2.cpp
    tier2_logging.cpp
    tokenreader.cpp
    util_init.cpp
    vconfig.cpp
    schemaclassbinding.cpp
    socketcreator.cpp
    resourceprecacher.cpp
    interval.cpp)

target_link_libraries(tier2 PRIVATE ks_common)
target_include_directories(tier2 PRIVATE "${CMAKE_SOURCE_DIR}/public/tier2")
