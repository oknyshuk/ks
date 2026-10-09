add_library(soundsystem_lowlevel STATIC
    device_null.cpp
    mix.cpp
    device_sdl.cpp
)
target_link_libraries(soundsystem_lowlevel PRIVATE ks_common )
