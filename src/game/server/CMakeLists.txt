# SPDX-License-Identifier: MIT
# Copyright (c) 2026 oknyshuk

add_library(server OBJECT
    ks.server.cpp
    activitylist.cpp
    ambientgeneric.cpp
    baseanimating.cpp
    BaseAnimatingOverlay.cpp
    basecombatcharacter.cpp
    basecombatweapon.cpp
    baseentity.cpp
    datamap.cpp
    baseentity_shared.cpp
    baseflex.cpp
    basetempentity.cpp
    buttons.cpp
    coerciblevariant.cpp
    doors.cpp
    env.cpp
    envmicrophone.cpp
    fogvolume.cpp
    func.cpp
    game.cpp
    gameinterface.cpp
    gamerules.cpp
    hltvdirector.cpp
    logicrelay.cpp
    movie.cpp
    ndebugoverlay.cpp
    physics.cpp
    physics_2.cpp
    physobj.cpp
    player.cpp
    player_2.cpp
    playerlocaldata.cpp
    props.cpp
    props_shared.cpp
    querycache.cpp
    sceneentity.cpp
    sendproxy.cpp
    soundent.cpp
    subs.cpp
    team.cpp
    trains.cpp
    triggers.cpp
    usercmd.cpp
    util.cpp
    variant.cpp
    world.cpp
    te.cpp
    nav_3.cpp
    nav.cpp
    nav_generate.cpp
    nav_2.cpp
    cstrike15/basecsgrenade.cpp
    cstrike15/cs_gamerules.cpp
    cstrike15/cs_gamestats.cpp
    cstrike15/cs_player.cpp
    cstrike15/cs_player_2.cpp
    cstrike15/cs_playeranimstate.cpp
    cstrike15/csgo_playeranimstate.cpp
    econ/econ_item_schema_minimal.cpp
    cstrike15/decoy.cpp
    cstrike15/funfact.cpp
    cstrike15/point.cpp
    cstrike15/weapon.cpp
    cstrike15/weapon_3.cpp
    cstrike15/Effects/chicken.cpp
    cstrike15/bot/cs_bot.cpp
    cstrike15/bot/cs_bot_2.cpp
    cstrike15/bot/cs_bot_3.cpp
    cstrike15/bot/cs_bot_4.cpp
    cstrike15/bot/cs_gamestate.cpp
    cstrike15/bot/states/cs_bot.cpp
    cstrike15/bot/bot.cpp
    cstrike15/hostage/cs_simple_hostage.cpp
)
target_sources(server PUBLIC
    FILE_SET CXX_MODULES FILES
        ks.server.cppm
        entity_2.cppm
        baseentity.cppm
        fish.cppm
        nav_4.cppm
        vscript.cppm
        sharedvar.cppm
        player.cppm
        cstrike15/Effects/chicken.cppm
        cstrike15/cs_player.cppm
        cstrike15/bot/bot.cppm
        cstrike15/bot/cs_bot.cppm
        cstrike15/cs_player_2.cppm
        cstrike15/cs_playeranimstate.cppm
        cstrike15/cs_team.cppm
        cstrike15/hostage/cs_simple_hostage.cppm
        econ/econ_item_schema_minimal.cppm
        entity_dispatch.cppm
        entity_kind.cppm
        entityutil.cppm
)
target_link_libraries(server PRIVATE game ks_common ks_shared public_common game_public responserules_runtime bonesetup particles mathlib_extended choreoobjects bitmap dmxloader tier1 tier2 tier3 interfaces mathlib)
target_include_directories(server PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/." "${CMAKE_CURRENT_SOURCE_DIR}/cstrike15" "${CMAKE_CURRENT_SOURCE_DIR}/cstrike15/bot" "${CMAKE_CURRENT_SOURCE_DIR}/cstrike15/bot/states" "${CMAKE_CURRENT_SOURCE_DIR}/cstrike15/control" "${CMAKE_CURRENT_SOURCE_DIR}/cstrike15/hostage" "${CMAKE_CURRENT_SOURCE_DIR}/../../utils/common")
target_compile_definitions(server PRIVATE GAME_DLL=1 VECTOR=1 BOTS=1 CSTRIKE_DLL=1)


