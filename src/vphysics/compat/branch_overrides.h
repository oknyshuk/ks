// SPDX-License-Identifier: MIT
// Copyright 2022 Joshua Ashton and Josh Dowell (Slartibarty)


#pragma once

#define override_not_csgo override

// GMod SDK2013 x86 branch
#define override_not_gmod override

#if defined( GAME_CSGO ) || defined( GAME_DESOLATION ) || defined( GAME_PORTAL2 )
#define GAME_PORTAL2_OR_NEWER
#else
#endif

#if defined( GAME_CSGO ) || defined( GAME_DESOLATION ) || defined( GAME_PORTAL2 ) || defined( GAME_L4D2 )
#else
#endif

#if defined( GAME_CSGO ) || defined( GAME_DESOLATION ) || defined( GAME_PORTAL2 ) || defined( GAME_L4D2 ) || defined( GAME_ASW )
#define GAME_ASW_OR_NEWER
#else
#endif

using strlen_t = int;

#ifndef GAME_CSGO_OR_NEWER
#endif
