//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef TESLA_H
#define TESLA_H

#include "reflect_annotations.h"


#include "baseentity.h"


class [[= ks::reflect::NetTable{ .name = "DT_Tesla" } ]]
      CTesla : public CBaseEntity
{
public:
	DECLARE_CLASS( CTesla, CBaseEntity );
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	CTesla();

	virtual void Spawn();
	virtual void Activate();
	virtual void Precache();

	[[= ks::reflect::Input{ .name = "TurnOn", .type = FIELD_VOID } ]] void InputTurnOn( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "TurnOff", .type = FIELD_VOID } ]] void InputTurnOff( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "DoSpark", .type = FIELD_VOID } ]] void InputDoSpark( inputdata_t &inputdata );

	void DoSpark();
	void ShootArcThink();
	
	void SetupForNextArc();
	CBaseEntity* GetSourceEntity();


public:
	
	// Tesla parameters.
	[[= ks::reflect::Key{ .name = "m_SourceEntityName" } ]] string_t m_SourceEntityName;	// Which entity the arcs come from.
	CNetworkVar( string_t, m_SoundName, [[= ks::reflect::Net{} ]] [[= ks::reflect::Key{ .name = "m_SoundName" } ]] );			// What sound to play when arcing.

	[[= ks::reflect::Key{ .name = "m_Color" } ]] color32 m_Color;
	[[= ks::reflect::Key{ .name = "beamcount_max", .index = 1 } ]] [[= ks::reflect::Key{ .name = "beamcount_min", .index = 0 } ]] int m_NumBeams[2];		// Number of beams per spark.
	
	[[= ks::reflect::Key{ .name = "m_flRadius" } ]] float m_flRadius;		// Radius it looks for surfaces to arc to.
	
	[[= ks::reflect::Key{ .name = "thick_max", .index = 1 } ]] [[= ks::reflect::Key{ .name = "thick_min", .index = 0 } ]] float m_flThickness[2];		// Beam thickness.
	[[= ks::reflect::Key{ .name = "lifetime_max", .index = 1 } ]] [[= ks::reflect::Key{ .name = "lifetime_min", .index = 0 } ]] float m_flTimeVisible[2];	// How long each beam stays around (min/max).
	[[= ks::reflect::Key{ .name = "interval_max", .index = 1 } ]] [[= ks::reflect::Key{ .name = "interval_min", .index = 0 } ]] float m_flArcInterval[2];	// Time between args (min/max).

	[[= ks::reflect::Key{ .name = "m_bOn" } ]] bool m_bOn;

	CNetworkVar( string_t, m_iszSpriteName, [[= ks::reflect::Net{} ]] [[= ks::reflect::Key{ .name = "texture" } ]] );
};


#endif // TESLA_H
