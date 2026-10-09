# SPDX-License-Identifier: MIT
# Copyright (c) 2026 oknyshuk

add_library(studiorender OBJECT
    studiorender.cpp
    studiorendercontext.cpp
    flexrenderdata.cpp
    r_studio.cpp
    r_studiodecal.cpp
    r_studiodraw.cpp
    r_studiodraw_computeflexedvertex.cpp
    r_studioflex.cpp
    r_studiosubd.cpp
    r_studiosubd_patches.cpp
    r_studiogettriangles.cpp
    r_studiolight.cpp
)
target_link_libraries(studiorender PRIVATE ks_common tier0 memoverride bitmap tier1 tier2 tier3 interfaces vstdlib mathlib)
target_compile_definitions(studiorender PRIVATE STUDIORENDER_EXPORTS=1)
