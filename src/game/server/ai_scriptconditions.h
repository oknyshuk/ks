//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#ifndef AI_SCRIPTCONDITIONS_H
#define AI_SCRIPTCONDITIONS_H

#include "baseentity.h"
#include "entityoutput.h"
#include "simtimer.h"
#include "ai_npcstate.h"
#include "reflect_annotations.h"

#if defined( _WIN32 )
#pragma once
#endif

//-----------------------------------------------------------------------------

class CAI_ProxTester
{
public:
	CAI_ProxTester()
		: m_distSq( 0 ),
		m_fInside( false )
	{
	}

	void Init( float dist )
	{
		m_fInside = ( dist > 0 );
		m_distSq = dist * dist;
	}

	bool Check( CBaseEntity *pEntity1, CBaseEntity *pEntity2 )
	{
		if ( m_distSq != 0 && pEntity1 && pEntity2 )
		{
			float distSq = ( pEntity1->GetAbsOrigin() - pEntity2->GetAbsOrigin() ).LengthSqr();
			bool fInside = ( distSq < m_distSq );

			return ( m_fInside == fInside );
		}
		return true;
	}

	DECLARE_SIMPLE_DATADESC();

private:

	float m_distSq;
	bool  m_fInside;
};

//-----------------------------------------------------------------------------
class CAI_ScriptConditionsElement
{
public:

	DECLARE_SIMPLE_DATADESC();

	void			SetActor( CBaseEntity *pEntity ) { m_hActor = pEntity; }
	CBaseEntity		*GetActor( void ){ return m_hActor.Get(); }

	void			SetTimer( CSimTimer timer ) { m_Timer = timer;	}
	CSimTimer		*GetTimer( void ) { return &m_Timer;	}
	
	void			SetTimeOut( CSimTimer timeout) { m_Timeout = timeout;	}
	CSimTimer		*GetTimeOut( void ) { return &m_Timeout;	}

private:
	EHANDLE			m_hActor;
	CSimTimer		m_Timer;
	CSimTimer		m_Timeout;
};

//-----------------------------------------------------------------------------
// class CAI_ScriptConditions
//
// Purpose: Watches a set of conditions relative to a given NPC, and when they
//			are all satisfied, fires the relevant output
//-----------------------------------------------------------------------------

class CAI_ScriptConditions : public CBaseEntity, public IEntityListener
{
	DECLARE_CLASS( CAI_ScriptConditions, CBaseEntity );

public:
	CAI_ScriptConditions()
		:	m_fDisabled( true ),
		m_flRequiredTime( 0 ),
		m_fMinState( NPC_STATE_IDLE ),
		m_fMaxState( NPC_STATE_IDLE ),
		m_fScriptStatus( TRS_NONE ),
		m_fActorSeePlayer( TRS_NONE ),
		m_flPlayerActorProximity( 0 ),
		m_flPlayerActorFOV( -1 ),
		m_fPlayerActorLOS( TRS_NONE ),
		m_fActorSeeTarget( TRS_NONE ),
		m_flActorTargetProximity( 0 ),
		m_flPlayerTargetProximity( 0 ),
		m_flPlayerTargetFOV( 0 ),
		m_fPlayerTargetLOS( TRS_NONE ),
		m_fPlayerBlockingActor( TRS_NONE ),
		m_flMinTimeout( 0 ),
		m_flMaxTimeout( 0 ),
		m_fActorInPVS( TRS_NONE ),
		m_fActorInVehicle( TRS_NONE ),
		m_fPlayerInVehicle( TRS_NONE )
	{
#ifndef HL2_EPISODIC
		m_hActor = NULL;
#endif
	}

private:
	void Spawn();
	void Activate();

	void EvaluationThink();

	void Enable();
	void Disable();

	void SetThinkTime()			{ SetNextThink( gpGlobals->curtime + 0.250 ); }

	// Evaluators
	struct EvalArgs_t
	{
		CBaseEntity *pActor; 
		CBasePlayer *pPlayer; 
		CBaseEntity *pTarget;
	};

	bool EvalState( const EvalArgs_t &args );
	bool EvalActorSeePlayer( const EvalArgs_t &args );
	bool EvalPlayerActorLook( const EvalArgs_t &args );
	bool EvalPlayerTargetLook( const EvalArgs_t &args );
	bool EvalPlayerActorProximity( const EvalArgs_t &args );
	bool EvalPlayerTargetProximity( const EvalArgs_t &args );
	bool EvalActorTargetProximity( const EvalArgs_t &args );
	bool EvalActorSeeTarget( const EvalArgs_t &args );
	bool EvalPlayerActorLOS( const EvalArgs_t &args );
	bool EvalPlayerTargetLOS( const EvalArgs_t &args );
	bool EvalPlayerBlockingActor( const EvalArgs_t &args );
	bool EvalActorInPVS( const EvalArgs_t &args );
	bool EvalPlayerInVehicle( const EvalArgs_t &args );
	bool EvalActorInVehicle( const EvalArgs_t &args );

	void OnEntitySpawned( CBaseEntity *pEntity );

	int AddNewElement( CBaseEntity *pActor );

	bool ActorInList( CBaseEntity *pActor );
	void UpdateOnRemove( void );

	// Input handlers
	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void InputDisable( inputdata_t &inputdata );

	// Output handlers
	[[= ks::reflect::Key{ .name = "OnConditionsSatisfied" } ]] COutputEvent	m_OnConditionsSatisfied;
	[[= ks::reflect::Key{ .name = "OnConditionsTimeout" } ]] COutputEvent	m_OnConditionsTimeout;
	[[= ks::reflect::Key{ .name = "NoValidActors" } ]] COutputEvent	m_NoValidActors;

	//---------------------------------

#ifndef HL2_EPISODIC
	CBaseEntity *GetActor()		{ return m_hActor.Get();			}
#endif
	CBasePlayer *GetPlayer()	{ return UTIL_GetLocalPlayer();	}

	//---------------------------------

	// @Note (toml 07-17-02): At some point, it may be desireable to switch to using function objects instead of functions. Probably
	// if support for NPCs addiing custom conditions becomes necessary
	typedef bool (CAI_ScriptConditions::*EvaluationFunc_t)( const EvalArgs_t &args );

	struct EvaluatorInfo_t
	{
		EvaluationFunc_t	pfnEvaluator;
		const char			*pszName;
	};

	static EvaluatorInfo_t gm_Evaluators[];

	//---------------------------------
	// Evaluation helpers

	static bool IsInFOV( CBaseEntity *pViewer, CBaseEntity *pViewed, float fov, bool bTrueCone );
	static bool PlayerHasLineOfSight( CBaseEntity *pViewer, CBaseEntity *pViewed, bool fNot );
	static bool ActorInPlayersPVS( CBaseEntity *pActor, bool bNot );

	virtual void OnRestore( void );

	//---------------------------------
	// General conditions info

	[[= ks::reflect::Key{ .name = "StartDisabled" } ]] bool			m_fDisabled;
	bool			m_bLeaveAsleep;
	EHANDLE			m_hTarget;

	[[= ks::reflect::Key{ .name = "RequiredTime" } ]] float			m_flRequiredTime;	// How long should the conditions me true

#ifndef HL2_EPISODIC
	EHANDLE 		m_hActor;
	CSimTimer		m_Timer; 			// @TODO (toml 07-16-02): save/load of timer once Jay has save/load of contained objects
	CSimTimer		m_Timeout;
#endif

	//---------------------------------
	// Specific conditions data
	[[= ks::reflect::Key{ .name = "MinimumState" } ]] NPC_STATE		m_fMinState;
	[[= ks::reflect::Key{ .name = "MaximumState" } ]] NPC_STATE		m_fMaxState;
	[[= ks::reflect::Key{ .name = "ScriptStatus" } ]] ThreeState_t 	m_fScriptStatus;
	[[= ks::reflect::Key{ .name = "ActorSeePlayer" } ]] ThreeState_t 	m_fActorSeePlayer;
	[[= ks::reflect::Key{ .name = "Actor" } ]] string_t		m_Actor;

	[[= ks::reflect::Key{ .name = "PlayerActorProximity" } ]] float 			m_flPlayerActorProximity;
	CAI_ProxTester	m_PlayerActorProxTester;

	[[= ks::reflect::Key{ .name = "PlayerActorFOV" } ]] float			m_flPlayerActorFOV;
	[[= ks::reflect::Key{ .name = "PlayerActorFOVTrueCone" } ]] bool			m_bPlayerActorFOVTrueCone;
	[[= ks::reflect::Key{ .name = "PlayerActorLOS" } ]] ThreeState_t	m_fPlayerActorLOS;
	[[= ks::reflect::Key{ .name = "ActorSeeTarget" } ]] ThreeState_t 	m_fActorSeeTarget;

	[[= ks::reflect::Key{ .name = "ActorTargetProximity" } ]] float 			m_flActorTargetProximity;
	CAI_ProxTester	m_ActorTargetProxTester;

	[[= ks::reflect::Key{ .name = "PlayerTargetProximity" } ]] float 			m_flPlayerTargetProximity;
	CAI_ProxTester	m_PlayerTargetProxTester;

	[[= ks::reflect::Key{ .name = "PlayerTargetFOV" } ]] float 			m_flPlayerTargetFOV;
	[[= ks::reflect::Key{ .name = "PlayerTargetFOVTrueCone" } ]] bool			m_bPlayerTargetFOVTrueCone;
	[[= ks::reflect::Key{ .name = "PlayerTargetLOS" } ]] ThreeState_t	m_fPlayerTargetLOS;
	[[= ks::reflect::Key{ .name = "PlayerBlockingActor" } ]] ThreeState_t	m_fPlayerBlockingActor;
	[[= ks::reflect::Key{ .name = "ActorInPVS" } ]] ThreeState_t	m_fActorInPVS;

	[[= ks::reflect::Key{ .name = "MinTimeout" } ]] float			m_flMinTimeout;
	[[= ks::reflect::Key{ .name = "MaxTimeout" } ]] float			m_flMaxTimeout;

	[[= ks::reflect::Key{ .name = "ActorInVehicle" } ]] ThreeState_t	m_fActorInVehicle;
	[[= ks::reflect::Key{ .name = "PlayerInVehicle" } ]] ThreeState_t	m_fPlayerInVehicle;

	CUtlVector< CAI_ScriptConditionsElement > m_ElementList;

	//---------------------------------

	DECLARE_DATADESC();
};

//=============================================================================

#endif // AI_SCRIPTCONDITIONS_H
