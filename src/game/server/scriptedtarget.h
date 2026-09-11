//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef SCRIPTEDTARGET_H
#define SCRIPTEDTARGET_H

#include "reflect_annotations.h"

#ifndef SCRIPTEVENT_H
#include "scriptevent.h"
#endif

#include "ai_basenpc.h"

class CScriptedTarget : public CAI_BaseNPC
{
	DECLARE_CLASS( CScriptedTarget, CAI_BaseNPC );
public:
	DECLARE_DATADESC();

	void				Spawn( void );
	virtual int			ObjectCaps( void ) { return (BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION); }


	void				ScriptThink( void );
	CBaseEntity*		FindEntity( void );

	void TurnOn(void);
	void TurnOff(void);

	// Input handlers
	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void InputDisable( inputdata_t &inputdata );

	CScriptedTarget*	NextScriptedTarget(void);
	float				MoveSpeed(void)			{ return m_nMoveSpeed; };
	float				EffectDuration(void)	{ return m_flEffectDuration; };

	int					DrawDebugTextOverlays(void);
	void				DrawDebugGeometryOverlays(void);
	float				PercentComplete(void);

	Vector				m_vLastPosition;	// Last position that's been reached
	
private:
	[[= ks::reflect::Key{ .name = "StartDisabled" } ]] int					m_iDisabled;		// Initial state
	[[= ks::reflect::Key{ .name = "m_iszEntity" } ]] string_t			m_iszEntity;		// entity that is wanted for this script
	[[= ks::reflect::Key{ .name = "m_flRadius" } ]] float				m_flRadius;			// range to search

	[[= ks::reflect::Key{ .name = "MoveSpeed" } ]] int					m_nMoveSpeed;		// How fast do I burn from target to target
	[[= ks::reflect::Key{ .name = "PauseDuration" } ]] float				m_flPauseDuration;	// How long to pause at this target
	float				m_flPauseDoneTime;	// When is pause over
	[[= ks::reflect::Key{ .name = "EffectDuration" } ]] float				m_flEffectDuration;	// How long should any associated effect last?

	[[= ks::reflect::Key{ .name = "AtTarget" } ]] COutputEvent		m_AtTarget;			// Fired when scripted target has been reached
	[[= ks::reflect::Key{ .name = "LeaveTarget" } ]] COutputEvent		m_LeaveTarget;		// Fired when scripted target is left
};

#endif // SCRIPTEDTARGET_H
