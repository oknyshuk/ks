add_library(shaderapidx9 OBJECT
    colorformatdx8.cpp
    d3d_async.cpp
    hardwareconfig.cpp
    meshbase.cpp
    meshdx8.cpp
    shaderapidx8.cpp
    shaderdevicebase.cpp
    shaderapibase.cpp
    shaderdevicedx8.cpp
    shadershadowdx8.cpp
    texturedx8.cpp
    TransitionTable.cpp
    cvballoctracker.cpp
    vertexdecl.cpp
    vertexshaderdx8.cpp
    winutils.cpp
)
target_link_libraries(shaderapidx9 PRIVATE ks_common PkgConfig::SDL3 tier0 memoverride public_common bitmap tier1 tier2 interfaces vstdlib videocfg mathlib d3d9 d3dx shaderlib vtf appframework bzip2)
target_include_directories(shaderapidx9 PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_compile_definitions(shaderapidx9 PRIVATE SHADERAPIDX9=1 SHADER_DLL_EXPORT=1)
