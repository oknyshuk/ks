add_library(mathlib STATIC
    expressioncalculator.cpp
    color_conversion.cpp
    cholesky.cpp
    halton.cpp
    lightdesc.cpp
    mathlib_base.cpp
    powsse.cpp
    sparse_convolution_noise.cpp
    sseconst.cpp
    sse.cpp
    ssenoise.cpp
    anorms.cpp
    bumpvects.cpp
    IceKey.cpp
    kdop.cpp
    imagequant.cpp
    spherical.cpp
    polyhedron.cpp
    quantize.cpp
    randsse.cpp
    simdvectormatrix.cpp
    vmatrix.cpp
    almostequal.cpp
    simplex.cpp
    eigen.cpp
    box_buoyancy.cpp
    camera.cpp
    planefit.cpp
    polygon.cpp
    volumeculler.cpp
    transform.cpp
    sphere.cpp
    capsule.cpp)

add_library(mathlib_extended STATIC
    disjoint_set_forest.cpp
    dynamictree.cpp
    eigen.cpp
    simdvectormatrix.cpp
    femodel.cpp
    femodelbuilder.cpp
    feagglomerator.cpp
    svd.cpp
    transform.cpp
    femodeldesc.cpp
    softbody.cpp
    softbodyenvironment.cpp)

foreach(_t mathlib mathlib_extended)
    target_link_libraries(${_t} PRIVATE ks_common)
    target_include_directories(${_t} PRIVATE "${CMAKE_SOURCE_DIR}/public/mathlib")
endforeach()
