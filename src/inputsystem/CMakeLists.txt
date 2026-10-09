add_library(inputsystem OBJECT
    inputstacksystem.cpp
    inputsystem.cpp
    steamcontroller.cpp
    key_translation.cpp
    joystick_linux.cpp
)
target_link_libraries(inputsystem PRIVATE ks_common PkgConfig::SDL3 tier0 memoverride public_common tier1 tier2 interfaces vstdlib mathlib)
target_compile_definitions(inputsystem PRIVATE NO_STRING_T=1)
