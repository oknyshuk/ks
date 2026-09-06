//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: The worldspawn entity. This spawns first when each level begins.
//
// $NoKeywords: $
//=============================================================================//

#ifndef WORLD_H
#define WORLD_H

#include "reflect_annotations.h"
#ifdef _WIN32
#pragma once
#endif

enum
{
	TIME_MIDNIGHT	= 0,
	TIME_DAWN,
	TIME_MORNING,
	TIME_AFTERNOON,
	TIME_DUSK,
	TIME_EVENING,
};

class [[= ks::reflect::NetTable{ .name = "DT_WORLD" } ]]
      CWorld : public CBaseEntity
{
public:
	DECLARE_CLASS( CWorld, CBaseEntity );

	CWorld();
	~CWorld();

	DECLARE_SERVERCLASS();

	virtual int RequiredEdictIndex( void ) { return 0; }   // the world always needs to be in slot 0
	
	static void RegisterSharedActivities( void );
	static void RegisterSharedEvents( void );
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void UpdateOnRemove( void );
	virtual bool KeyValue( const char *szKeyName, const char *szValue );
	virtual void DecalTrace( trace_t *pTrace, char const *decalName );
	virtual void VPhysicsCollision( int index, gamevcollisionevent_t *pEvent ) {}
	virtual void VPhysicsFriction( IPhysicsObject *pObject, float energy, int surfaceProps, int surfacePropsHit ) {}

	inline void GetWorldBounds( Vector &vecMins, Vector &vecMaxs )
	{
		VectorCopy( m_WorldMins, vecMins );
		VectorCopy( m_WorldMaxs, vecMaxs );
	}

	inline float GetWaveHeight() const
	{
		return (float)m_flWaveHeight;
	}

	bool GetDisplayTitle() const;
	bool GetStartDark() const;

	void SetDisplayTitle( bool display );
	void SetStartDark( bool startdark );

	int GetTimeOfDay() const;
	void SetTimeOfDay( int iTimeOfDay );

	bool IsColdWorld( void );

	int GetTimeOfDay()	{ return m_iTimeOfDay; }

#ifdef PORTAL2
	int GetMaxBlobCount() const { return m_nMaxBlobCount; }
#endif

private:
	DECLARE_DATADESC();

	[[= ks::reflect::Key{ .name = "chaptertitle" } ]] string_t m_iszChapterTitle;

	CNetworkVar( float, m_flWaveHeight, [[= ks::reflect::Net{ .bits = 8, .low = 0.0f, .high = 8.0f, .flags = SPROP_ROUNDUP } ]] );
	CNetworkVector( m_WorldMins, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::ENC_VECTOR } ]] );
	CNetworkVector( m_WorldMaxs, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::ENC_VECTOR } ]] );
	CNetworkVar( float, m_flMaxOccludeeArea, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "maxoccludeearea" } ]] );
	CNetworkVar( float, m_flMinOccluderArea, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "minoccluderarea" } ]] );
	CNetworkVar( float, m_flMinPropScreenSpaceWidth, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "minpropscreenwidth" } ]] );
	CNetworkVar( float, m_flMaxPropScreenSpaceWidth, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "maxpropscreenwidth" } ]] );
	CNetworkVar( string_t, m_iszDetailSpriteMaterial, [[= ks::reflect::Net{} ]] [[= ks::reflect::Key{ .name = "detailmaterial" } ]] );

	// start flags
	CNetworkVar( bool, m_bStartDark, [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "startdark" } ]] );
	CNetworkVar( bool, m_bColdWorld, [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "coldworld" } ]] );
	CNetworkVar( int, m_iTimeOfDay );
	[[= ks::reflect::Key{ .name = "gametitle" } ]] bool m_bDisplayTitle;

#ifdef PORTAL2
	CNetworkVar( int, m_nMaxBlobCount );
#endif
};


CWorld* GetWorldEntity();
extern const char *GetDefaultLightstyleString( int styleIndex );


#endif // WORLD_H
