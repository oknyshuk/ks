#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Dynamic light at the end of a spotlight

#include "reflect_annotations.h"

#include "baseentity.h"

class [[= ks::reflect::NetTable{ .name = "DT_SpotlightEnd" } ]]
      CSpotlightEnd : public CBaseEntity
{
public:
	DECLARE_CLASS( CSpotlightEnd, CBaseEntity );

	void				Spawn_Impl( void );

	int					ObjectCaps_Impl( void )
	{
		// Don't save and don't go across transitions
		return (BaseClass::ObjectCaps_Impl() & ~FCAP_ACROSS_TRANSITION) | FCAP_DONT_SAVE; 
	}

	DECLARE_SERVERCLASS();

public:
	CNetworkVar( float, m_flLightScale, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_Radius, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
//	CNetworkVector( m_vSpotlightDir );
//	CNetworkVector( m_vSpotlightOrg );
	Vector			m_vSpotlightDir;
	Vector			m_vSpotlightOrg;
};
