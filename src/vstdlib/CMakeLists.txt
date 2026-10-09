add_library(vstdlib OBJECT
    coroutine.cpp
    cvar.cpp
    jobthread.cpp
    keyvaluessystem.cpp
    random.cpp
    vcover.cpp
    eventsystem.cpp
    vstrtools.cpp
    vstdlib_strtools.cpp)

target_link_libraries(vstdlib PRIVATE ks_common)
target_link_libraries(vstdlib PUBLIC tier0 memoverride tier1 interfaces)
target_compile_definitions(vstdlib PRIVATE VSTDLIB_DLL_EXPORT=1)
