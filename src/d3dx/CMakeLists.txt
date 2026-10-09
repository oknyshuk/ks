add_library(d3dx STATIC d3dxmath.cpp)
target_link_libraries(d3dx PRIVATE ks_common)
