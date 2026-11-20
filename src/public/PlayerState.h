#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "edict.h"
#include "networkvar.h"
#include "reflect_annotations.h"
import ks.math.vector;
// Only care about this stuff in game/client .dlls

class [[= ks::reflect::NetTable{ .name = "DT_PlayerState", .base = false } ]]
      CPlayerState
{
public:
	DECLARE_CLASS_NOBASE( CPlayerState );
	DECLARE_EMBEDDED_NETWORKVAR();
	
	// This virtual method is necessary to generate a vtable in all cases
	// (DECLARE_PREDICTABLE will generate a vtable also)!
	virtual ~CPlayerState() {}

	// true if the player is dead
	CNetworkVar( bool, deadflag, [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]]
	                             [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED } ]] );	
	// Viewing angle (player only)
	QAngle		v_angle;		
	
// The client .dll only cares about deadflag
//  the game and engine .dlls need to worry about the rest of this data
	// Player's network name
	string_t	netname;
	// 0:nothing, 1:force view angles, 2:add avelocity
	int			fixangle;
	// delta angle for fixangle == FIXANGLE_RELATIVE
	QAngle		anglechange;
	// flag to single the HLTV/Replay fake client, not transmitted
	bool		hltv;
	bool		replay;
	int			frags;
	int			assists;
	int			deaths;
	int			score;

// NOTE:  Only care about this stuff in game/client dlls
// Put at end in case it has any effect on size of structure

};
