# SPDX-License-Identifier: MIT
# Copyright (c) 2026 oknyshuk

add_library(client OBJECT
    ks.client.cpp
    achievementmgr.cpp
    base_playeranimstate.cpp
    baseanimatingoverlay.cpp
    basecombatweapon.cpp
    baseentity_shared.cpp
    baseflex.cpp
    baseparticleentity.cpp
    baseplayer.cpp
    basetempentity.cpp
    baseanimating.cpp
    info.cpp
    physicsprop.cpp
    basedoor.cpp
    baseentity.cpp
    baselesson.cpp
    basetoggle.cpp
    clientsideeffects.cpp
    dummyproxy.cpp
    env.cpp
    eventlist.cpp
    func.cpp
    game.cpp
    glow.cpp
    modelrendersystem.cpp
    particle.cpp
    particlemgr.cpp
    physics.cpp
    precache.cpp
    ragdoll.cpp
    smoke.cpp
    sprite.cpp
    team.cpp
    voice.cpp
    cdll.cpp
    classmap.cpp
    clientleafsystem.cpp
    clientmode_shared.cpp
    clientshadowmgr.cpp
    death.cpp
    detailobjectsystem.cpp
    functionproxy.cpp
    fx.cpp
    hud.cpp
    in.cpp
    datamap.cpp
    view.cpp
    viewangleanim.cpp
    viewpostprocess.cpp
    viewrender.cpp
    te.cpp
    te_2.cpp
    game_controls/baseviewport.cpp
    NextBot/NextBot.cpp
    cstrike15/fx.cpp
    cstrike15/achievements.cpp
    cstrike15/buy_presets/buy.cpp
    cstrike15/cs_ammodef.cpp
    cstrike15/cs_player_2.cpp
    cstrike15/cs_gamestats.cpp
    cstrike15/cs_player.cpp
    cstrike15/cs_playeranimstate.cpp
    cstrike15/clientmode_csnormal.cpp
    cstrike15/cs_gamerules.cpp
    econ/econ_item_schema_minimal.cpp
    cstrike15/cs_client_gamestats.cpp
    cstrike15/bot/shared_util.cpp
    cstrike15/gameui/bonusmapsdatabase.cpp
    cstrike15/weapon.cpp
    cstrike15/Effects/clientinferno.cpp
    cstrike15/Effects/chicken.cpp
    cstrike15/VGUI/counterstrikeviewport.cpp
    cstrike15/RocketUI/rkconsole.cpp
)
target_sources(client PUBLIC
    FILE_SET CXX_MODULES FILES
        ks.client.cppm
        noopanimcontroller.cppm
        particlemgr.cppm
        iinput.cppm
        NextBot/NextBot.cppm
        baseanimatedtextureproxy.cppm
        basetoggle.cppm
        achievements_and_stats_interface.cppm
        baseentity.cppm
        props.cppm
        util.cppm
        sharedvar.cppm
        entitydatainstantiator.cppm
        triggers.cppm
        cstrike15/Effects/chicken.cppm
        cstrike15/Effects/clientinferno.cppm
        cstrike15/RocketUI/rkconsole.cppm
        cstrike15/VGUI/counterstrikeviewport.cppm
        cstrike15/cs_player.cppm
        cstrike15/bot/bot.cppm
        cstrike15/buy_presets/buy.cppm
        cstrike15/cs_team.cppm
        cstrike15/cs_player_shared.cppm
        cstrike15/cs_shareddefs.cppm
        cstrike15/cs_view_scene.cppm
        cstrike15/gameui/bonusmapsdatabase.cppm
        econ/econ_item_schema_minimal.cppm
        game_controls/baseviewport.cppm
        gamevars_shared.cppm
        precache.cppm
)
target_link_libraries(client PRIVATE game ks_common ks_jpeg rmlui public_common bonesetup particles raytrace mathlib_extended resourcefile choreoobjects bitmap dmxloader game_public tier1 tier2 tier3 interfaces mathlib vtf videocfg bzip2 RmlCore)
target_include_directories(client PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/." "${CMAKE_CURRENT_SOURCE_DIR}/cstrike15" "${CMAKE_CURRENT_SOURCE_DIR}/cstrike15/gameui" "${CMAKE_CURRENT_SOURCE_DIR}/cstrike15/VGUI" "${CMAKE_CURRENT_SOURCE_DIR}/cstrike15/RocketUI" "${CMAKE_CURRENT_SOURCE_DIR}/game_controls" "${CMAKE_CURRENT_SOURCE_DIR}/../../public/rocketui")
target_compile_definitions(client PRIVATE NO_STRING_T=1 CLIENT_DLL=1 VECTOR=1 ENABLE_STUDIO_SOFTBODY=1 CSTRIKE_CLIENT_DLL=1 CSTRIKE_DLL=1 GAMEUI_EMBEDDED=1 GAMEUI_EXPORTS=1)

# Client-only code of common/, public/ and utils/ that defines what their headers declare. It is built
# as ordinary code (not as part of the ks_client namespace) so that those headers stay shared.
add_library(client_public STATIC
    ../../common/imageutils.cpp
    ../../common/GameUI/ObjectList.cpp
    ../../public/client_class.cpp
    ../../utils/common/cmdlib.cpp
    ../../utils/common/filesystem_tools.cpp
    ../../utils/common/scriplib.cpp
    ../../tier1/fileio.cpp
    ../../public/interpolatedvar.cpp
    ../../public/render_bounds.cpp
)
foreach(_prop INCLUDE_DIRECTORIES COMPILE_DEFINITIONS LINK_LIBRARIES)
    get_target_property(_value client ${_prop})
    set(_client_${_prop} ${_value})
endforeach()
list(REMOVE_ITEM _client_LINK_LIBRARIES game)   # its objects belong to the executable, once
target_include_directories(client_public PRIVATE ${_client_INCLUDE_DIRECTORIES})
target_compile_definitions(client_public PRIVATE ${_client_COMPILE_DEFINITIONS})
target_link_libraries(client_public PRIVATE ${_client_LINK_LIBRARIES})
target_link_libraries(client PRIVATE client_public)

