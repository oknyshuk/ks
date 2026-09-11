//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef AI_SPEECHFILTER_H
#define AI_SPEECHFILTER_H

#include "reflect_annotations.h"
#ifdef _WIN32
#pragma once
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CAI_SpeechFilter : public CBaseEntity, public IEntityListener
{
	DECLARE_CLASS( CAI_SpeechFilter, CBaseEntity );
public:
	DECLARE_DATADESC();

	void	Spawn( void );
	void	Activate( void );
	void	UpdateOnRemove( void );

	void	Enable( bool bEnable );
	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void	InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void	InputDisable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetIdleModifier", .type = FIELD_FLOAT } ]] void	InputSetIdleModifier( inputdata_t &inputdata );

	void	PopulateSubjectList( bool purge = false );


	// Accessors for our NPC
	float	GetIdleModifier( void ) { return m_flIdleModifier; }
	bool	NeverSayHello( void ) { return m_bNeverSayHello; }

	void	OnEntityCreated( CBaseEntity *pEntity );
	void	OnEntityDeleted( CBaseEntity *pEntity );

protected:
	[[= ks::reflect::Key{ .name = "subject" } ]] string_t	m_iszSubject;
	[[= ks::reflect::Key{ .name = "IdleModifier" } ]] float		m_flIdleModifier;	// Multiplier to the percentage chance that our NPC will idle speak
	[[= ks::reflect::Key{ .name = "NeverSayHello" } ]] bool		m_bNeverSayHello;	// If set, the NPC never says hello to the player
	[[= ks::reflect::Key{ .name = "StartDisabled" } ]] bool		m_bDisabled;
};

#endif // AI_SPEECHFILTER_H
