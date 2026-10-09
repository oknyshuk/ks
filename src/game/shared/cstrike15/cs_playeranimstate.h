#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.tier1;
#include "iplayeranimstate.h"
#include "base_playeranimstate.h"

#ifdef CLIENT_DLL
	class C_BaseAnimatingOverlay;
	class C_WeaponCSBase;
	#define CBaseAnimatingOverlay C_BaseAnimatingOverlay
	#define CWeaponCSBase C_WeaponCSBase
	#define CCSPlayer C_CSPlayer
#else
	class CBaseAnimatingOverlay;
	class CWeaponCSBase; 
	class CCSPlayer;
#endif


// When moving this fast, he plays run anim.
inline constexpr auto ARBITRARY_RUN_SPEED = 175.0f;

inline constexpr auto HOSTAGE_JUMP_POWER = 200.0f;
inline constexpr const char *HOSTAGE_ANIM_MODEL = "models/hostage/hostage.mdl";

inline constexpr auto MOVESTATE_IDLE = 0;
inline constexpr auto MOVESTATE_WALK = 1;
inline constexpr auto MOVESTATE_RUN = 2;


// This abstracts the differences between CS players and hostages.
class ICSPlayerAnimStateHelpers
{
public:
	virtual CWeaponCSBase* CSAnim_GetActiveWeapon() = 0;
	virtual bool CSAnim_CanMove() = 0;
};


IPlayerAnimState* CreatePlayerAnimState( CBaseAnimatingOverlay *pEntity, ICSPlayerAnimStateHelpers *pHelpers, LegAnimType_t legAnimType, bool bUseAimSequences );
IPlayerAnimState* CreateHostageAnimState( CBaseAnimatingOverlay *pEntity, ICSPlayerAnimStateHelpers *pHelpers, LegAnimType_t legAnimType, bool bUseAimSequences );

// If this is set, then the game code needs to make sure to send player animation events
// to the local player if he's the one being watched.
extern ConVar cl_showanimstate;
