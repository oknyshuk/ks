add_library(bonesetup STATIC
    bone_setup.cpp
    bone_ik.cpp
    bone_utils.cpp
    bone_decode.cpp
    bone_constraints.cpp
)
target_link_libraries(bonesetup PRIVATE ks_common )
