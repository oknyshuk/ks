#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: The worldspawn entity. This spawns first when each level begins.

#include "reflect_annotations.h"
#include "baseentity.h"

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

	int RequiredEdictIndex_Impl( void ) { return 0; }   // the world always needs to be in slot 0
	
	static void RegisterSharedActivities( void );
	static void RegisterSharedEvents( void );
	void Spawn_Impl( void );
	void Precache_Impl( void );
	void UpdateOnRemove_Impl( void );
	using BaseClass::KeyValue_Impl;
	bool KeyValue_Impl( const char *szKeyName, const char *szValue );
	void DecalTrace_Impl( trace_t *pTrace, char const *decalName );
	void VPhysicsCollision_Impl( int index, gamevcollisionevent_t *pEvent ) {}
	void VPhysicsFriction_Impl( IPhysicsObject *pObject, float energy, int surfaceProps, int surfacePropsHit ) {}

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


private:
	DECLARE_DATADESC();

	[[= ks::reflect::Key{ .name = "chaptertitle" } ]] string_t m_iszChapterTitle;

	CNetworkVar( float, m_flWaveHeight, [[= ks::reflect::Net{ .bits = 8, .low = 0.0f, .high = 8.0f, .flags = SPROP_ROUNDUP } ]] );
	CNetworkVector( m_WorldMins, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVector( m_WorldMaxs, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
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

};


CWorld* GetWorldEntity();
extern const char *GetDefaultLightstyleString( int styleIndex );
