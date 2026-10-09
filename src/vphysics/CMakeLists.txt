add_library(vphysics OBJECT
    vjolt_collide.cpp
    vjolt_collide_trace.cpp
    vjolt_constraints.cpp
    vjolt_controller_fluid.cpp
    vjolt_controller_motion.cpp
    vjolt_controller_player.cpp
    vjolt_controller_vehicle.cpp
    vjolt_controller_shadow.cpp
    vjolt_debugrender.cpp
    vjolt_environment.cpp
    vjolt_friction.cpp
    vjolt_interface.cpp
    vjolt_keyvalues_schema.cpp
    vjolt_object.cpp
    vjolt_objectpairhash.cpp
    vjolt_parse.cpp
    vjolt_querymodel.cpp
    vjolt_surfaceprops.cpp
)
target_link_libraries(vphysics PRIVATE ks_common tier0 memoverride public_common tier1 tier2 interfaces vstdlib mathlib joltphysics)
target_compile_definitions(vphysics PRIVATE GAME_PORTAL2=1)

target_precompile_headers(vphysics PRIVATE cbase.h)
