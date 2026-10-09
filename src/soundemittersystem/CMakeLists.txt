add_library(soundemittersystem OBJECT
    soundemittersystembase.cpp
    ../public/SoundParametersInternal.cpp
)
target_link_libraries(soundemittersystem PRIVATE ks_common tier0 memoverride public_common tier1 tier2 tier3 interfaces vstdlib)
target_include_directories(soundemittersystem PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/.")
target_compile_definitions(soundemittersystem PRIVATE SOUNDEMITTERSYSTEM_EXPORTS=1)
