add_library(launcher OBJECT
    launcher.cpp
    reslistgenerator.cpp
)
target_link_libraries(launcher PRIVATE ks_common PkgConfig::SDL3 tier0 memoverride appframework tier1 tier2 tier3 interfaces vstdlib)
target_compile_definitions(launcher PRIVATE LAUNCHERONLY=1)
