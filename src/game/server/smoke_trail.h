//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#ifndef SMOKE_TRAIL_H
#define SMOKE_TRAIL_H

#include "reflect_annotations.h"

#include "baseparticleentity.h"

//==================================================
// SmokeTrail
//==================================================

class [[= ks::reflect::NetTable{ .name = "DT_SmokeTrail" } ]]
      SmokeTrail : public CBaseParticleEntity
{
	DECLARE_DATADESC();
public:
	DECLARE_CLASS( SmokeTrail, CBaseParticleEntity );
	DECLARE_SERVERCLASS();

	SmokeTrail();
	virtual bool KeyValue( const char *szKeyName, const char *szValue ); 
	void					SetEmit(bool bVal);
	void					FollowEntity( CBaseEntity *pEntity, const char *pAttachmentName = NULL);
	static	SmokeTrail*		CreateSmokeTrail();

public:
	// Effect parameters. These will assume default values but you can change them.
	CNetworkVector( m_StartColor, [[= ks::reflect::Net{ .bits = 8, .low = 0, .high = 1, .enc = ks::reflect::ENC_VECTOR } ]] );			// Fade between these colors.
	CNetworkVector( m_EndColor, [[= ks::reflect::Net{ .bits = 8, .low = 0, .high = 1, .enc = ks::reflect::ENC_VECTOR } ]] );
	CNetworkVar( float, m_Opacity, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "opacity" } ]] );

	CNetworkVar( float, m_SpawnRate, [[= ks::reflect::Net{ .bits = 8, .low = 1, .high = 1024 } ]] [[= ks::reflect::Key{ .name = "spawnrate" } ]] );			// How many particles per second.
	CNetworkVar( float, m_ParticleLifetime, [[= ks::reflect::Net{ .bits = 16, .low = 0.1, .high = 100, .flags = SPROP_ROUNDUP } ]] [[= ks::reflect::Key{ .name = "lifetime" } ]] );		// How long do the particles live?
	CNetworkVar( float, m_StopEmitTime, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );			// When do I stop emitting particles?
	CNetworkVar( float, m_MinSpeed, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "minspeed" } ]] );				// Speed range.
	CNetworkVar( float, m_MaxSpeed, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "maxspeed" } ]] );
	CNetworkVar( float, m_StartSize, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "startsize" } ]] );			// Size ramp.
	CNetworkVar( float, m_EndSize, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "endsize" } ]] );	
	CNetworkVar( float, m_SpawnRadius, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "spawnradius" } ]] );
	CNetworkVar( float, m_MinDirectedSpeed, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "mindirectedspeed" } ]] );				// Speed range.
	CNetworkVar( float, m_MaxDirectedSpeed, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "maxdirectedspeed" } ]] );
	CNetworkVar( bool, m_bEmit, [[= ks::reflect::Net{} ]] );

	CNetworkVar( int, m_nAttachment, [[= ks::reflect::Net{ .bits = 32 } ]] );
};

//==================================================
// RocketTrail
//==================================================

class [[= ks::reflect::NetTable{ .name = "DT_RocketTrail" } ]]
      RocketTrail : public CBaseParticleEntity
{
public:
	DECLARE_CLASS( RocketTrail, CBaseParticleEntity );
	DECLARE_SERVERCLASS();

	RocketTrail();
	void					SetEmit(bool bVal);
	void					FollowEntity( CBaseEntity *pEntity, const char *pAttachmentName = NULL);
	static RocketTrail		*CreateRocketTrail();

public:
	// Effect parameters. These will assume default values but you can change them.
	CNetworkVector( m_StartColor, [[= ks::reflect::Net{ .bits = 8, .low = 0, .high = 1, .enc = ks::reflect::ENC_VECTOR } ]] );			// Fade between these colors.
	CNetworkVector( m_EndColor, [[= ks::reflect::Net{ .bits = 8, .low = 0, .high = 1, .enc = ks::reflect::ENC_VECTOR } ]] );
	CNetworkVar( float, m_Opacity, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] );

	CNetworkVar( float, m_SpawnRate, [[= ks::reflect::Net{ .bits = 8, .low = 1, .high = 1024 } ]] );			// How many particles per second.
	CNetworkVar( float, m_ParticleLifetime, [[= ks::reflect::Net{ .bits = 16, .low = 0.1, .high = 100, .flags = SPROP_ROUNDUP } ]] );		// How long do the particles live?
	CNetworkVar( float, m_StopEmitTime, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );			// When do I stop emitting particles?
	CNetworkVar( float, m_MinSpeed, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] );				// Speed range.
	CNetworkVar( float, m_MaxSpeed, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_StartSize, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] );			// Size ramp.
	CNetworkVar( float, m_EndSize, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] );	
	CNetworkVar( float, m_SpawnRadius, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] );
	
	CNetworkVar( bool, m_bEmit, [[= ks::reflect::Net{} ]] );

	CNetworkVar( int, m_nAttachment, [[= ks::reflect::Net{ .bits = 32 } ]] );
	
	CNetworkVar( bool, m_bDamaged, [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED } ]] );

	CNetworkVar( float, m_flFlareScale, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] );			// Size of the flare
};

//==================================================
// SporeTrail
//==================================================

class [[= ks::reflect::NetTable{ .name = "DT_SporeTrail" } ]]
      SporeTrail : public CBaseParticleEntity
{
public:
	DECLARE_CLASS( SporeTrail, CBaseParticleEntity );
	DECLARE_SERVERCLASS();

	SporeTrail( void );

	static SporeTrail*		CreateSporeTrail();

//Data members
public:

	CNetworkVector( m_vecEndColor, [[= ks::reflect::Net{ .bits = 8, .low = 0, .high = 1, .enc = ks::reflect::ENC_VECTOR } ]] );

	CNetworkVar( float, m_flSpawnRate, [[= ks::reflect::Net{ .bits = 8, .low = 1, .high = 1024 } ]] );
	CNetworkVar( float, m_flParticleLifetime, [[= ks::reflect::Net{ .bits = 16, .low = 0.1, .high = 100, .flags = SPROP_ROUNDUP } ]] );
	CNetworkVar( float, m_flStartSize, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_flEndSize, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_flSpawnRadius, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] );

	CNetworkVar( bool, m_bEmit, [[= ks::reflect::Net{} ]] );
};

//==================================================
// SporeExplosion
//==================================================

class [[= ks::reflect::NetTable{ .name = "DT_SporeExplosion" } ]]
      SporeExplosion : public CBaseParticleEntity
{
	DECLARE_DATADESC();
public:
	DECLARE_CLASS( SporeExplosion, CBaseParticleEntity );
	DECLARE_SERVERCLASS();

	SporeExplosion( void );
	void Spawn( void );

	static SporeExplosion*		CreateSporeExplosion();

	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void InputDisable( inputdata_t &inputdata );

//Data members
public:

	[[= ks::reflect::Key{ .name = "startdisabled" } ]] bool m_bDisabled;

	CNetworkVar( float, m_flSpawnRate, [[= ks::reflect::Net{ .bits = 8, .low = 1, .high = 1024 } ]] [[= ks::reflect::Key{ .name = "spawnrate" } ]] );
	CNetworkVar( float, m_flParticleLifetime, [[= ks::reflect::Net{ .bits = 16, .low = 0.1, .high = 100, .flags = SPROP_ROUNDUP } ]] );
	CNetworkVar( float, m_flStartSize, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_flEndSize, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_flSpawnRadius, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] );

	CNetworkVar( bool, m_bEmit, [[= ks::reflect::Net{} ]] );
	CNetworkVar( bool, m_bDontRemove, [[= ks::reflect::Net{} ]] );
};

//==================================================
// CFireTrail
//==================================================

class [[= ks::reflect::NetTable{ .name = "DT_FireTrail" } ]]
      CFireTrail : public CBaseParticleEntity
{
public:
	DECLARE_CLASS( CFireTrail, CBaseParticleEntity );
	DECLARE_SERVERCLASS();

	static CFireTrail	*CreateFireTrail( void );
	void				FollowEntity( CBaseEntity *pEntity, const char *pAttachmentName );
	void				Precache( void );

	CNetworkVar( int, m_nAttachment, [[= ks::reflect::Net{ .bits = 32 } ]] );
	CNetworkVar( float, m_flLifetime, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
};

//==================================================
// DustTrail
//==================================================

class [[= ks::reflect::NetTable{ .name = "DT_DustTrail" } ]]
      DustTrail : public CBaseParticleEntity
{
	DECLARE_DATADESC();
public:
	DECLARE_CLASS( DustTrail, CBaseParticleEntity );
	DECLARE_SERVERCLASS();

	DustTrail();
	virtual bool KeyValue( const char *szKeyName, const char *szValue ); 
	void					SetEmit(bool bVal);
	static	DustTrail*		CreateDustTrail();

public:
	// Effect parameters. These will assume default values but you can change them.
	CNetworkVector( m_Color, [[= ks::reflect::Net{ .bits = 8, .low = 0, .high = 1, .enc = ks::reflect::ENC_VECTOR } ]] );
	CNetworkVar( float, m_Opacity, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "opacity" } ]] );

	CNetworkVar( float, m_SpawnRate, [[= ks::reflect::Net{ .bits = 8, .low = 1, .high = 1024 } ]] [[= ks::reflect::Key{ .name = "spawnrate" } ]] );			// How many particles per second.
	CNetworkVar( float, m_ParticleLifetime, [[= ks::reflect::Net{ .bits = 16, .low = 0.1, .high = 100, .flags = SPROP_ROUNDUP } ]] [[= ks::reflect::Key{ .name = "lifetime" } ]] );		// How long do the particles live?
	CNetworkVar( float, m_StopEmitTime, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );			// When do I stop emitting particles?
	CNetworkVar( float, m_MinSpeed, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "minspeed" } ]] );				// Speed range.
	CNetworkVar( float, m_MaxSpeed, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "maxspeed" } ]] );
	CNetworkVar( float, m_StartSize, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "startsize" } ]] );			// Size ramp.
	CNetworkVar( float, m_EndSize, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "endsize" } ]] );	
	CNetworkVar( float, m_SpawnRadius, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "spawnradius" } ]] );
	CNetworkVar( float, m_MinDirectedSpeed, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "mindirectedspeed" } ]] );				// Speed range.
	CNetworkVar( float, m_MaxDirectedSpeed, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "maxdirectedspeed" } ]] );
	CNetworkVar( bool, m_bEmit, [[= ks::reflect::Net{} ]] );

	CNetworkVar( int, m_nAttachment );
};


#endif
