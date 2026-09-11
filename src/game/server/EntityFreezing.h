//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef ENTITYFREEZING_H
#define ENTITYFREEZING_H

#ifdef _WIN32
#pragma once
#endif


#include "reflect_annotations.h"

class [[= ks::reflect::NetTable{ .name = "DT_EntityFreezing" } ]] CEntityFreezing : public CBaseEntity 
{
public:
	DECLARE_SERVERCLASS();
	DECLARE_CLASS( CEntityFreezing, CBaseEntity );

	static CEntityFreezing	*Create( CBaseAnimating *pTarget );
	
	void	Precache();
	void	Spawn();
	void	AttachToEntity( CBaseEntity *pTarget );
	void	SetFreezingOrigin( Vector vOrigin ) { m_vFreezingOrigin = vOrigin; }
	Vector	GetFreezingOrigin( void ) { return m_vFreezingOrigin; }
	void	SetFrozen( float flFrozen ){ m_flFrozen = flFrozen; }
	int		GetFrozen( void ) { return m_flFrozen; }
	void	FinishFreezing( void ) { m_bFinishFreezing = true; }

	DECLARE_DATADESC();

protected:
	[[= ks::reflect::Input{ .name = "Freeze", .type = FIELD_STRING } ]] void	InputFreeze( inputdata_t &inputdata );

	CNetworkVector( m_vFreezingOrigin, [[= ks::reflect::Net{ .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_flFrozen, [[= ks::reflect::Net{} ]] [[= ks::reflect::Key{ .name = "frozen" } ]] );
	CNetworkVar( bool, m_bFinishFreezing, [[= ks::reflect::Net{} ]] );

public:
	// Declared last, sent second. Left in Valve's order deliberately: the migration does not
	// reorder members, and the name-keyed gate proves that is safe.
	CNetworkArray( float, m_flFrozenPerHitbox, 50, [[= ks::reflect::Net{} ]] );
};

#endif // ENTITYFREEZING_H
