#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "reflect_annotations.h"
#include "ai_basenpc.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CCycler : public CAI_BaseNPC
{
public:
	DECLARE_CLASS( CCycler, CAI_BaseNPC );

	void GenericCyclerSpawn(char *szModel, Vector vecMin, Vector vecMax);
	virtual int	ObjectCaps( void ) { return (BaseClass::ObjectCaps() | FCAP_IMPULSE_USE); }
	int OnTakeDamage( const CTakeDamageInfo &info );
	void Spawn( void );
	void Precache( void );
	void Think( void );
	//void Pain( float flDamage );
	void Use ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	// Don't treat as a live target
	virtual bool IsAlive( void ) { return false; }

	// Inputs
	[[= ks::reflect::Input{ .name = "SetSequence", .type = FIELD_STRING } ]] void	InputSetSequence( inputdata_t &inputdata );

	DECLARE_DATADESC();

	int			m_animate;
};
