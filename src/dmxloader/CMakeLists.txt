add_library(dmxloader STATIC
    dmxattribute.cpp
    dmxelement.cpp
    dmxloader.cpp
    dmxloadertext.cpp
    dmxserializationdictionary.cpp
    utlsoacontainer_serialization.cpp
)
target_link_libraries(dmxloader PRIVATE ks_common )
