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
	int	ObjectCaps_Impl( void ) { return (BaseClass::ObjectCaps_Impl() | FCAP_IMPULSE_USE); }
	int OnTakeDamage_Impl( const CTakeDamageInfo &info );
	void Spawn_Impl( void );
	void Precache_Impl( void );
	void Think_Impl( void );
	//void Pain( float flDamage );
	void Use_Impl ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	// Don't treat as a live target
	virtual bool IsAlive( void ) { return false; }

	// Inputs
	[[= ks::reflect::Input{ .name = "SetSequence", .type = FIELD_STRING } ]] void	InputSetSequence( inputdata_t &inputdata );

	DECLARE_DATADESC();

	int			m_animate;
};
