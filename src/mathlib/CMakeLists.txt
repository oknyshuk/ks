# SPDX-License-Identifier: MIT
# Copyright (c) 2026 oknyshuk

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
    femodel.cpp
    femodelbuilder.cpp
    feagglomerator.cpp
    svd.cpp
    femodeldesc.cpp
    softbody.cpp
    softbodyenvironment.cpp)

# mathlib_extended used to list eigen.cpp, simdvectormatrix.cpp and transform.cpp as well, which
# built them a second time for no gain; it links mathlib instead.
target_link_libraries(mathlib_extended PUBLIC mathlib)

foreach(_t mathlib mathlib_extended)
    target_link_libraries(${_t} PRIVATE ks_common)
    target_include_directories(${_t} PRIVATE "${CMAKE_SOURCE_DIR}/public/mathlib")
endforeach()

add_library(ks_math STATIC)
target_sources(ks_math PUBLIC
    FILE_SET CXX_MODULES
    FILES ks.math.cppm
          ks.math-pfns.cppm
          ks.math-vector2d.cppm
          ks.math-vector.cppm
          ks.math-fltx4.cppm
          ks.math-vplane.cppm
          ks.math-vector4d.cppm
          ks.math-mathlib.cppm
          ks.math-ssemath.cppm
          ks.math-ssequaternion.cppm
          ks.math-svd.cppm
          ks.math-eigen.cppm
          ks.math-cholesky.cppm
          ks.math-ssecholesky.cppm
          ks.math-aabb.cppm
          ks.math-vmatrix.cppm
          ks.math-transform.cppm
          ks.math-noise.cppm
          ks.math-halton.cppm
          ks.math-spherical_geometry.cppm
          ks.math-quantize.cppm
          ks.math-simplex.cppm
          ks.math-polyhedron.cppm
          ks.math-quadric.cppm
          ks.math-lightdesc.cppm
          ks.math-compressed_vector.cppm
          ks.math-compressed_light_cube.cppm
          ks.math-vertexcolor.cppm
          ks.math-bumpvects.cppm
          ks.math-anorms.cppm
          ks.math-box_buoyancy.cppm
          ks.math-volumeculler.cppm)
target_link_libraries(ks_math PUBLIC ks_common_base ks_core ks_simd)
set_property(TARGET ks_math PROPERTY CXX_SCAN_FOR_MODULES ON)
