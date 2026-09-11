
#include "reflect_annotations.h"
//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Bomb Target Area ent
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "triggers.h"
#include "cvisibilitymonitor.h"
//#include "cs_player_resource.h"

class CBombTarget : public CBaseTrigger
{
public:
	DECLARE_CLASS( CBombTarget, CBaseTrigger );
	DECLARE_DATADESC();

	CBombTarget();

	void Spawn();
	virtual void ReInitOnRoundStart( void );
	void EXPORT BombTargetTouch( CBaseEntity* pOther );
	void EXPORT BombTargetUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

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

	void Spawn( void );
	virtual int UpdateTransmitState( void )	// set transmit filter to transmit always
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

	void Spawn( void );
	virtual int UpdateTransmitState( void )	// set transmit filter to transmit always
	{
		return SetTransmitState( FL_EDICT_ALWAYS );
	}

};
