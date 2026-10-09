add_library(materialsystem OBJECT
    CColorCorrection.cpp
    cmaterial.cpp
    cmaterial_queuefriendly.cpp
    CMaterialSubRect.cpp
    cmaterialvar.cpp
    cmatnullrendercontext.cpp
    colorspace.cpp
    ctexture.cpp
    mat_stub.cpp
    materialsystem_global.cpp
    morph.cpp
    SubdMgr.cpp
    occlusionquerymgr.cpp
    shadersystem.cpp
    texturemanager.cpp
    composite_texture.cpp
    custom_material.cpp
    base_visuals_data_processor.cpp
    cmaterialdict.cpp
    cmaterialsystem.cpp
    cmatlightmaps.cpp
    cmatpaintmaps.cpp
    cmatrendercontext.cpp
    cmatqueuedrendercontext.cpp
)
target_link_libraries(materialsystem PRIVATE ks_common tier0 memoverride public_common bitmap tier1 tier2 tier3 interfaces vstdlib mathlib shaderlib vtf videocfg)
target_compile_definitions(materialsystem PRIVATE DEFINE_MATERIALSYSTEM_INTERFACE=1 MATERIALSYSTEM_EXPORTS=1)
