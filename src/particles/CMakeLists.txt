# SPDX-License-Identifier: MIT
# Copyright (c) 2026 oknyshuk

add_library(particles STATIC
    builtin_constraints.cpp
    builtin_initializers.cpp
    builtin_particle_emitters.cpp
    builtin_particle_forces.cpp
    addbuiltin_ops.cpp
    builtin_particle_ops.cpp
    builtin_particle_render_ops.cpp
    particle_snapshot.cpp
    particle_sort.cpp
    particles.cpp
)
target_link_libraries(particles PRIVATE ks_common )
