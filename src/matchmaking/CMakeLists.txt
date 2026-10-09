add_library(matchmaking OBJECT
    cstrike15/mm_title.cpp
    cstrike15/mm_title_gamesettingsmgr.cpp
    cstrike15/mm_title_main.cpp
    cstrike15/mm_title_richpresence.cpp
    cstrike15/mm_title_titledata.cpp
    ../game/shared/cstrike15/gametypes.cpp
)
target_link_libraries(matchmaking PRIVATE ks_common tier0 memoverride public_common matchmaking_base tier1 tier2 tier3 interfaces vstdlib mathlib)
target_include_directories(matchmaking PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../common/xlast_csgo" "${CMAKE_CURRENT_SOURCE_DIR}/../game/shared")
target_compile_definitions(matchmaking PRIVATE MATCHMAKING_DLL=1 NO_STRING_T=1 VECTOR=1 NO_STEAM_GAMECOORDINATOR=1)

add_library(matchmaking_base STATIC
    cstrike15/mm_title.cpp
    cstrike15/mm_title_gamesettingsmgr.cpp
    cstrike15/mm_title_main.cpp
    cstrike15/mm_title_richpresence.cpp
    cstrike15/mm_title_titledata.cpp
    ../game/shared/cstrike15/gametypes.cpp
)
target_link_libraries(matchmaking_base PRIVATE ks_common tier0 memoverride public_common tier1 tier2 tier3 interfaces vstdlib mathlib)
target_include_directories(matchmaking_base PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../common/xlast_csgo" "${CMAKE_CURRENT_SOURCE_DIR}/../game/shared")
target_compile_definitions(matchmaking_base PRIVATE MATCHMAKING_DLL=1 NO_STRING_T=1 VECTOR=1 NO_STEAM_GAMECOORDINATOR=1)
