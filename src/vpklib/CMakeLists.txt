add_library(vpk STATIC
    packedstore.cpp
)
target_link_libraries(vpk PRIVATE ks_common )
