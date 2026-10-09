add_library(resourcefile STATIC
    resourcedictionary.cpp
    resourcefile.cpp
    resourceintrospection.cpp
    resourcestream.cpp
)
target_link_libraries(resourcefile PRIVATE ks_common )
