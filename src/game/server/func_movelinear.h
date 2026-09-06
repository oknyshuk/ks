//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef FUNC_MOVELINEAR_H
#define FUNC_MOVELINEAR_H

#include "reflect_annotations.h"

#pragma once

#include "basetoggle.h"
#include "entityoutput.h"


class IPhysicsFluidController;


class [[= ks::reflect::NetTable{ .name = "DT_FuncMoveLinear" } ]]
      [[= ks::reflect::From<"m_vecVelocity", ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE, .enc = ks::reflect::ENC_VECTOR }>{} ]]
      [[= ks::reflect::From<"m_fFlags", ks::reflect::Net{ .bits = 0, .flags = SPROP_UNSIGNED }>{} ]]
      CFuncMoveLinear : public CBaseToggle
{
public:
	DECLARE_CLASS( CFuncMoveLinear, CBaseToggle );
	DECLARE_SERVERCLASS();

	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_vecVelocity );
	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_fFlags );

	void		Spawn( void );
	void		Precache( void );
	bool		CreateVPhysics( void );
	bool		ShouldSavePhysics( void );

	void		MoveTo(Vector vPosition, float flSpeed);
	void		Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void		MoveDone( void );
	void		StopMoveSound( void );
	void		Blocked( CBaseEntity *pOther );
	void		SetPosition( float flPosition );

	int			DrawDebugTextOverlays(void);

	// Input handlers
	[[= ks::reflect::Input{ .name = "Open", .type = FIELD_VOID } ]] void InputOpen( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Close", .type = FIELD_VOID } ]] void InputClose( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetPosition", .type = FIELD_FLOAT } ]] void InputSetPosition( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetSpeed", .type = FIELD_FLOAT } ]] void InputSetSpeed( inputdata_t &inputdata );
	
	DECLARE_DATADESC();

	[[= ks::reflect::Key{ .name = "movedir" } ]] Vector		m_vecMoveDir;			// Move direction.

	[[= ks::reflect::As{ FIELD_SOUNDNAME } ]] [[= ks::reflect::Key{ .name = "StartSound" } ]] string_t	m_soundStart;			// start and looping sound
	[[= ks::reflect::As{ FIELD_SOUNDNAME } ]] [[= ks::reflect::Key{ .name = "StopSound" } ]] string_t	m_soundStop;			// stop sound
	string_t	m_currentSound;			// sound I'm playing

	[[= ks::reflect::Key{ .name = "BlockDamage" } ]] float		m_flBlockDamage;		// Damage inflicted when blocked.
	[[= ks::reflect::Key{ .name = "StartPosition" } ]] float		m_flStartPosition;		// Position of brush when spawned
	[[= ks::reflect::Key{ .name = "MoveDistance" } ]] float		m_flMoveDistance;		// Total distance the brush can move

	IPhysicsFluidController *m_pFluidController;

	// Outputs
	[[= ks::reflect::Key{ .name = "OnFullyOpen" } ]] COutputEvent m_OnFullyOpen;
	[[= ks::reflect::Key{ .name = "OnFullyClosed" } ]] COutputEvent m_OnFullyClosed;
};
#endif // FUNC_MOVELINEAR_H
