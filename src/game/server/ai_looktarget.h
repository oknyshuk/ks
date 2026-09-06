//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//
#pragma once
#ifndef AI_LOOKTARGET_H
#define AI_LOOKTARGET_H

#include "reflect_annotations.h"

#define SF_LOOKTARGET_ONLYONCE	0x00000001

//=============================================================================
//=============================================================================
class CAI_LookTarget : public CPointEntity
{
public:
	DECLARE_CLASS( CAI_LookTarget, CPointEntity );
	DECLARE_DATADESC();

	CAI_LookTarget() { m_flTimeNextAvailable = -1; }

	// Debugging
	int DrawDebugTextOverlays(void);

	// Accessors & Availability
	bool IsEligible( CBaseEntity *pLooker );
	bool IsEnabled() { return !m_bDisabled; }
	bool IsAvailable() { return (gpGlobals->curtime > m_flTimeNextAvailable); }
	void Reserve( float flDuration );

	// Searching
	static CAI_LookTarget *GetFirstLookTarget();
	static CAI_LookTarget *GetNextLookTarget( CAI_LookTarget *pCurrentTarget );

	[[= ks::reflect::Key{ .name = "context" } ]] int		m_iContext;
	[[= ks::reflect::Key{ .name = "priority" } ]] int		m_iPriority;

	void	Enable()	{ m_bDisabled = false; }
	void	Disable()	{ m_bDisabled = true; }

private:
	[[= ks::reflect::Key{ .name = "StartDisabled" } ]] bool	m_bDisabled;
	float	m_flTimeNextAvailable;
	[[= ks::reflect::Key{ .name = "maxdist" } ]] float	m_flMaxDist;
};

#endif//AI_LOOKTARGET_H
