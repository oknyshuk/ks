add_library(tier3 STATIC
    tier3.cpp
    mdlutils.cpp
    choreoutils.cpp
    scenetokenprocessor.cpp
    studiohdrstub.cpp)

target_link_libraries(tier3 PRIVATE ks_common)
