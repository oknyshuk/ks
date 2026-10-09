# SPDX-License-Identifier: MIT
# Copyright (c) 2026 oknyshuk

add_library(shaderlib STATIC
    BaseShader.cpp
    ShaderDLL.cpp
    shaderlib_cvar.cpp
)
target_link_libraries(shaderlib PRIVATE ks_common )
target_include_directories(shaderlib PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_compile_definitions(shaderlib PRIVATE FAST_MATERIALVAR_ACCESS=1)
