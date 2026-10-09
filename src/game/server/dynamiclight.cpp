// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Dynamic light.

import std;
import ks.tier1;
#include "string_t.h"
#include "vphysics_interface.h"
#include "const.h"
#include "edict.h"
#include "eiface.h"
#include "iserverentity.h"
#include "dt_send.h"
#include "shareddefs.h"
#include "ehandle.h"
#include "datamap.h"
#include "util.h"
#include "predictable_entity.h"
#include "predictableid.h"
#include "variant_t.h"
#include "takedamageinfo.h"
#include "touchlink.h"
#include "groundlink.h"
#include "base_transmit_proxy.h"
#include "soundflags.h"
#include "networkvar.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "basetoggle.h"
#include "igameevents.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "gamerules.h"
#include "entitylist.h"
#include "basetempentity.h"
#include "player.h"
#include "ndebugoverlay.h"
#include "reflect_datamap.h"
#include "reflect_sendtable.h"
#include "reflect_annotations.h"
#include "dlight.h"

constexpr auto NUM_DL_EXPONENT_BITS = 8;
constexpr auto MIN_DL_EXPONENT_VALUE = -((1 << (NUM_DL_EXPONENT_BITS-1)) - 1);
constexpr auto MAX_DL_EXPONENT_VALUE = ((1 << (NUM_DL_EXPONENT_BITS-1)) - 1);


class [[= ks::reflect::NetTable{ .name = "DT_DynamicLight" } ]]
      CDynamicLight : public CBaseEntity
{
public:
	DECLARE_CLASS( CDynamicLight, CBaseEntity );

	void Spawn( void );
	void DynamicLightThink( void );
	bool KeyValue( const char *szKeyName, const char *szValue );

	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	// Turn on and off the light
	[[= ks::reflect::Input{ .name = "TurnOn", .type = FIELD_VOID } ]] void InputTurnOn( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "TurnOff", .type = FIELD_VOID } ]] void InputTurnOff( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Toggle", .type = FIELD_VOID } ]] void InputToggle( inputdata_t &inputdata );

public:
	unsigned char m_ActualFlags;
	CNetworkVar( unsigned char, m_Flags, [[= ks::reflect::Net{ .bits = 4, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( unsigned char, m_LightStyle, [[= ks::reflect::Net{ .bits = 4, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "style", .input = true } ]] );
	bool	m_On;
	CNetworkVar( float, m_Radius, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "distance", .input = true } ]] );
	CNetworkVar( int, m_Exponent, [[= ks::reflect::Net{ .bits = NUM_DL_EXPONENT_BITS } ]] [[= ks::reflect::Key{ .name = "brightness", .input = true } ]] );
	CNetworkVar( float, m_InnerAngle, [[= ks::reflect::Net{ .bits = 8, .low = 0.0, .high = 360.0f } ]] [[= ks::reflect::Key{ .name = "_inner_cone", .input = true } ]] );
	CNetworkVar( float, m_OuterAngle, [[= ks::reflect::Net{ .bits = 8, .low = 0.0, .high = 360.0f } ]] [[= ks::reflect::Key{ .name = "_cone", .input = true } ]] );
	CNetworkVar( float, m_SpotRadius, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "spotlight_radius", .input = true } ]] );
};

LINK_ENTITY_TO_CLASS(light_dynamic, CDynamicLight);

IMPLEMENT_REFLECT_DATAMAP( CDynamicLight )


IMPLEMENT_REFLECT_SERVERCLASS( CDynamicLight, DT_DynamicLight )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CDynamicLight::KeyValue( const char *szKeyName, const char *szValue )
{
	if ( FStrEq( szKeyName, "_light" ) )
	{
		color32 tmp;
		V_StringToColor32( &tmp, szValue );
		SetRenderColor( tmp.r, tmp.g, tmp.b );
	}
	else if ( FStrEq( szKeyName, "pitch" ) )
	{
		float angle = std::atof(szValue);
		if ( angle )
		{
			QAngle angles = GetAbsAngles();
			angles[PITCH] = -angle;
			SetAbsAngles( angles );
		}
	}
	else if ( FStrEq( szKeyName, "spawnflags" ) )
	{
		m_ActualFlags = m_Flags = std::atoi(szValue);
	}
	else
	{
		return BaseClass::KeyValue( szKeyName, szValue );
	}

	return true;
}

//------------------------------------------------------------------------------
// Turn on and off the light
//------------------------------------------------------------------------------
void CDynamicLight::InputTurnOn( inputdata_t &inputdata )
{
	m_Flags = m_ActualFlags;
	m_On = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CDynamicLight::InputTurnOff( inputdata_t &inputdata )
{
	// This basically shuts it off
	m_Flags = DLIGHT_NO_MODEL_ILLUMINATION | DLIGHT_NO_WORLD_ILLUMINATION;
	m_On = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CDynamicLight::InputToggle( inputdata_t &inputdata )
{
	if (m_On)
	{
		InputTurnOff( inputdata );
	}
	else
	{
		InputTurnOn( inputdata );
	}
}

//------------------------------------------------------------------------------
// Purpose :
//------------------------------------------------------------------------------
void CDynamicLight::Spawn( void )
{
	Precache();
	SetSolid( SOLID_NONE );
	m_On = true;
	UTIL_SetSize( this, vec3_origin, vec3_origin );
	AddEFlags( EFL_FORCE_CHECK_TRANSMIT );

	// If we have a target, think so we can orient towards it
	if ( m_target != NULL_STRING )
	{
		SetThink( &CDynamicLight::DynamicLightThink );
		SetNextThink( gpGlobals->curtime + 0.1 );
	}
	
	int clampedExponent = clamp( m_Exponent.Get(), MIN_DL_EXPONENT_VALUE, MAX_DL_EXPONENT_VALUE );
	if ( m_Exponent != clampedExponent )
	{
		Warning( "light_dynamic at [%d %d %d] has invalid exponent value (%d must be between %d and %d).\n",
			(int)GetAbsOrigin().x, (int)GetAbsOrigin().x, (int)GetAbsOrigin().x, 
			m_Exponent.Get(),
			MIN_DL_EXPONENT_VALUE,
			MAX_DL_EXPONENT_VALUE );
		
		m_Exponent = clampedExponent;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDynamicLight::DynamicLightThink( void )
{
	if ( m_target == NULL_STRING )
		return;

	CBaseEntity *pEntity = GetNextTarget();
	if ( pEntity )
	{
		Vector vecToTarget = (pEntity->GetAbsOrigin() - GetAbsOrigin());
		QAngle vecAngles;
		VectorAngles( vecToTarget, vecAngles );
		SetAbsAngles( vecAngles );
	}
	
	SetNextThink( gpGlobals->curtime + 0.1 );
}
