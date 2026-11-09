#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "ai_planesolver.h"
#include "baseentity.h"

inline constexpr auto FLAME_DAMAGE_INTERVAL = 0.2f; // How often to deal damage.
inline constexpr auto FLAME_DIRECT_DAMAGE_PER_SEC = 5.0f;
inline constexpr auto FLAME_RADIUS_DAMAGE_PER_SEC = 4.0f;

inline constexpr auto FLAME_DIRECT_DAMAGE = ( FLAME_DIRECT_DAMAGE_PER_SEC * FLAME_DAMAGE_INTERVAL );
inline constexpr auto FLAME_RADIUS_DAMAGE = ( FLAME_RADIUS_DAMAGE_PER_SEC * FLAME_DAMAGE_INTERVAL );

inline constexpr auto FLAME_MAX_LIFETIME_ON_DEAD_NPCS = 10.0f;

class CAI_Link;


#include "reflect_annotations.h"

class [[= ks::reflect::NetTable{ .name = "DT_EntityFlame" } ]] CEntityFlame : public CBaseEntity 
{
	DECLARE_SERVERCLASS();
	DECLARE_CLASS( CEntityFlame, CBaseEntity );

public:
	static CEntityFlame	*Create( CBaseEntity *pTarget, float flLifetime, float flSize = 0.0f, bool bUseHitboxes = true );

	CEntityFlame( void );

	void	AttachToEntity( CBaseEntity *pTarget );
	void	SetLifetime( float lifetime );
	void	SetUseHitboxes( bool use );

	void	SetNumHitboxFires( int iNumHitBoxFires );
	void	SetHitboxFireScale( float flHitboxFireScale );

	float	GetRemainingLife( void ) const;
	int		GetNumHitboxFires( void );
	float	GetHitboxFireScale( void );

	void Precache_Impl();
	void UpdateOnRemove_Impl();

	void Spawn_Impl();
	void Activate_Impl();

	void	UseCheapEffect( bool bCheap );

	void	SetSize( float size ) { m_flSize = size; }
	
	void	SetAttacker( CBaseEntity *pAttacker ) { m_hAttacker = pAttacker; }
	CBaseEntity *GetAttacker( void ) const;

protected:
	void	FlameThink( void );

	CNetworkHandle( CBaseEntity, m_hEntAttached, [[= ks::reflect::Net{} ]] );		// The entity that we are burning (attached to).
	CNetworkVar( bool, m_bCheapEffect, [[= ks::reflect::Net{} ]] );

	CNetworkVar( float, m_flSize );
	CNetworkVar( bool, m_bUseHitboxes );
	CNetworkVar( int, m_iNumHitboxFires );
	CNetworkVar( float, m_flHitboxFireScale );

	CNetworkVar( float, m_flLifetime, [[= ks::reflect::Key{ .name = "lifetime" } ]] );
	string_t	m_iszPlayingSound;	// Track the sound so we can StopSound later

	EHANDLE m_hAttacker;

	int m_iDangerSound;
	bool m_bPlayingSound;
	std::vector< CAI_Link * > m_DangerLinks;
	Obstacle_t m_hObstacle;
};
