add_library(shaderapiempty OBJECT
    shaderapiempty.cpp
)
target_link_libraries(shaderapiempty PRIVATE ks_common tier0 memoverride tier1 tier2 interfaces vstdlib mathlib shaderlib)
target_include_directories(shaderapiempty PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
