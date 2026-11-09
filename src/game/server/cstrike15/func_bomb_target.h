#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Bomb Target Area ent

#include "reflect_annotations.h"

#include "string_t.h"
#include "vphysics_interface.h"
#include "shareddefs.h"
#include "ehandle.h"
#include "datamap.h"
#include "util.h"
#include "predictable_entity.h"
#include "variant_t.h"
#include "takedamageinfo.h"
#include "touchlink.h"
#include "groundlink.h"
#include "networkvar.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "basetoggle.h"
#include "baseanimating.h"
#include "triggers.h"
#include "cvisibilitymonitor.h"
//#include "cs_player_resource.h"

class CBombTarget : public CBaseTrigger
{
public:
	DECLARE_CLASS( CBombTarget, CBaseTrigger );
	DECLARE_DATADESC();

	CBombTarget();

	void Spawn_Impl();
	virtual void ReInitOnRoundStart( void );
	void BombTargetTouch( CBaseEntity* pOther );
	void BombTargetUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	[[= ks::reflect::Input{ .name = "BombExplode", .type = FIELD_VOID } ]] void OnBombExplode( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "BombPlanted", .type = FIELD_VOID } ]] void OnBombPlanted( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "BombDefused", .type = FIELD_VOID } ]] void OnBombDefused( inputdata_t &inputdata );

	bool	IsHeistBombTarget( void ) { return m_bIsHeistBombTarget; }
	const char *GetBombMountTarget( void ){ return STRING( m_szMountTarget ); }

private:
	[[= ks::reflect::Key{ .name = "BombExplode" } ]] COutputEvent m_OnBombExplode;	//Fired when the bomb explodes
	[[= ks::reflect::Key{ .name = "BombPlanted" } ]] COutputEvent m_OnBombPlanted;	//Fired when the bomb is planted
	[[= ks::reflect::Key{ .name = "BombDefused" } ]] COutputEvent m_OnBombDefused;	//Fired when the bomb is defused

	[[= ks::reflect::Key{ .name = "heistbomb" } ]] bool		m_bIsHeistBombTarget;
	bool		m_bBombPlantedHere;
	[[= ks::reflect::Key{ .name = "bomb_mount_target" } ]] string_t	m_szMountTarget;
	EHANDLE		m_hInstructorHint;		// Hint that's used by the instructor system
};

//-----------------------------------------------------------------------------
// Purpose: A generic target entity that gets replicated to the client for displaying a hint for the CS bomb targets
//-----------------------------------------------------------------------------
class CInfoInstructorHintBombTargetA : public CPointEntity
{
public:
	DECLARE_CLASS( CInfoInstructorHintBombTargetA, CPointEntity );

	void Spawn_Impl( void );
	int UpdateTransmitState_Impl( void )	// set transmit filter to transmit always
	{
		return SetTransmitState( FL_EDICT_ALWAYS );
	}

};

//-----------------------------------------------------------------------------
// Purpose: A generic target entity that gets replicated to the client for displaying a hint for the CS bomb targets
//-----------------------------------------------------------------------------
class CInfoInstructorHintBombTargetB : public CPointEntity
{
public:
	DECLARE_CLASS( CInfoInstructorHintBombTargetB, CPointEntity );

	void Spawn_Impl( void );
	int UpdateTransmitState_Impl( void )	// set transmit filter to transmit always
	{
		return SetTransmitState( FL_EDICT_ALWAYS );
	}

};
