//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef ENTITYDISSOLVE_H
#define ENTITYDISSOLVE_H


#include "reflect_annotations.h"

class [[= ks::reflect::NetTable{ .name = "DT_EntityDissolve" } ]] CEntityDissolve : public CBaseEntity 
{
public:
	DECLARE_SERVERCLASS();
	DECLARE_CLASS( CEntityDissolve, CBaseEntity );

	CEntityDissolve( void );
	~CEntityDissolve( void );

	static CEntityDissolve	*Create( CBaseEntity *pTarget, const char *pMaterialName, 
		float flStartTime, int nDissolveType = 0, bool *pRagdollCreated = nullptr );
	static CEntityDissolve	*Create( CBaseEntity *pTarget, CBaseEntity *pSource );
	
	void	Precache();
	void	Spawn();
	void	AttachToEntity( CBaseEntity *pTarget );
	void	SetStartTime( float flStartTime );
	void	SetDissolverOrigin( Vector vOrigin ) { m_vDissolverOrigin = vOrigin; }
	void	SetMagnitude( int iMagnitude ){ m_nMagnitude = iMagnitude; }
	void	SetDissolveType( int iType ) { m_nDissolveType = iType;	}

	Vector	GetDissolverOrigin( void ) 
	{ 
		Vector vReturn = m_vDissolverOrigin; 
		return vReturn;	
	}
	int		GetMagnitude( void ) { return m_nMagnitude;	}
	int		GetDissolveType( void ) { return m_nDissolveType;	}

	DECLARE_DATADESC();

	CNetworkVar( float, m_flStartTime, [[= ks::reflect::As{ FIELD_TIME } ]] [[= ks::reflect::Net{} ]] );
	CNetworkVar( float, m_flFadeInStart,        [[= ks::reflect::Net{ .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_flFadeInLength,       [[= ks::reflect::Net{ .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_flFadeOutModelStart,  [[= ks::reflect::Net{ .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_flFadeOutModelLength, [[= ks::reflect::Net{ .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_flFadeOutStart,       [[= ks::reflect::Net{ .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_flFadeOutLength,      [[= ks::reflect::Net{ .flags = SPROP_NOSCALE } ]] );

protected:
	[[= ks::reflect::Input{ .name = "Dissolve", .type = FIELD_STRING } ]] void	InputDissolve( inputdata_t &inputdata );
	void	DissolveThink( void );
	void	ElectrocuteThink( void );

	CNetworkVar( int, m_nDissolveType, [[= ks::reflect::Net{ .bits = ENTITY_DISSOLVE_BITS, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "dissolvetype" } ]] );
	CNetworkVector( m_vDissolverOrigin, [[= ks::reflect::Net{ .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( int, m_nMagnitude, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "magnitude" } ]] );
};

#endif // ENTITYDISSOLVE_H
