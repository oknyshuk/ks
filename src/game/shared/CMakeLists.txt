# SPDX-License-Identifier: MIT
# Copyright (c) 2026 oknyshuk

# ks.game: the side-free game code, compiled once and imported by ks.server and ks.client.
add_library(game OBJECT
    mapentities_shared.cpp
    gamemovement.cpp
    predictableid.cpp
    rope_helpers.cpp
    sheetsimulator.cpp
)
target_sources(game PUBLIC
    FILE_SET CXX_MODULES FILES
        mapentities_shared.cppm
        ai_activity.cppm
        beam_flags.cppm
        cellcoord.cppm
        cstrike15/cs_achievement_constants.cppm
        cstrike15/cs_achievementdefs.cppm
        cstrike15/cs_player_rank_shared.cppm
        cstrike15/cstrike15_gcconstants.cppm
        cstrike15/playerdecals_signature.cppm
        entity_cast.cppm
        entitycore.cppm
        func_dust_shared.cppm
        gamemovement.cppm
        igamemovement.cppm
        in_buttons.cppm
        iplayeranimstate.cppm
        postprocess_shared.cppm
        precipitation_shared.cppm
        predictableid.cppm
        rope_helpers.cppm
        rumble_shared.cppm
        sheetsimulator.cppm
        shareddefs.cppm
        sun_shared.cppm
        tempentity.cppm
        triggers_shared.cppm
        vphysicsupdateai.cppm
)
target_link_libraries(game PUBLIC ks_common PRIVATE public_common tier1 tier2 tier3 interfaces mathlib)
target_include_directories(game PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/cstrike15")
