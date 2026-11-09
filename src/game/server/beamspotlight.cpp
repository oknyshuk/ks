// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: A special kind of beam effect that traces from its start position to
//			its end position and stops if it hits anything.

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
#include "ndebugoverlay.h"
#include "recipientfilter.h"
#include "npcevent.h"
#include "reflect_sendtable.h"
#include "reflect_annotations.h"
#include "reflect_datamap.h"
#include "EnvLaser.h"
#include "Sprite.h"

// Spawnflags
enum BeamSpotlightSpawnFlags_t
{
	SF_BEAM_SPOTLIGHT_START_LIGHT_ON    = 1,
	SF_BEAM_SPOTLIGHT_NO_DYNAMIC_LIGHT  = 2,
	SF_BEAM_SPOTLIGHT_START_ROTATE_ON   = 4,
	SF_BEAM_SPOTLIGHT_REVERSE_DIRECTION = 8,
	SF_BEAM_SPOTLIGHT_X_AXIS            = 16,
	SF_BEAM_SPOTLIGHT_Y_AXIS            = 32,
};


class [[= ks::reflect::NetTable{ .name = "DT_BeamSpotlight" } ]]
      CBeamSpotlight : public CBaseEntity
{
	DECLARE_CLASS( CBeamSpotlight, CBaseEntity );
public:
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	CBeamSpotlight();

	void	Spawn_Impl( void );
	void	Precache_Impl( void );

	[[= ks::reflect::Input{ .name = "LightOn",  .type = FIELD_VOID } ]] void InputTurnOn( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "LightOff", .type = FIELD_VOID } ]] void InputTurnOff( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Start",    .type = FIELD_VOID } ]] void InputStart( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Stop",     .type = FIELD_VOID } ]] void InputStop( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Reverse",  .type = FIELD_VOID } ]] void InputReverse( inputdata_t &inputdata );

protected:
	public: using BaseClass::KeyValue_Impl;
	bool KeyValue_Impl( const char *szKeyName, const char *szValue );
	protected:

private:
	public: int  UpdateTransmitState_Impl();
	private:
	void RecalcRotation( void );

	CNetworkVar( int, m_nHaloIndex , [[= ks::reflect::Net{ .bits = 16, .flags = SPROP_UNSIGNED } ]]);

	CNetworkVar( bool, m_bSpotlightOn , [[= ks::reflect::Net{} ]]);
	CNetworkVar( bool, m_bHasDynamicLight , [[= ks::reflect::Net{} ]]);

	CNetworkVar( float, m_flSpotlightMaxLength, [[= ks::reflect::Key{ .name = "SpotlightLength" } ]] [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]]);
	CNetworkVar( float,	m_flSpotlightGoalWidth, [[= ks::reflect::Key{ .name = "SpotlightWidth" } ]] [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]]);
	CNetworkVar( float,	m_flHDRColorScale, [[= ks::reflect::Key{ .name = "HDRColorScale" } ]] [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]]);
	CNetworkVar( int, m_nMinDXLevel );
	CNetworkVar( int, m_nRotationAxis , [[= ks::reflect::Net{ .bits = 2, .flags = SPROP_UNSIGNED } ]]);
	CNetworkVar( float, m_flRotationSpeed , [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]]);

	[[= ks::reflect::Key{ .name = "maxspeed" } ]] float m_flmaxSpeed;
	bool m_isRotating;
	bool m_isReversed;

public:
	// Split from one declaration: each output needs its own external name.
	[[= ks::reflect::Key{ .name = "OnLightOn" } ]]  COutputEvent m_OnOn;
	[[= ks::reflect::Key{ .name = "OnLightOff" } ]] COutputEvent m_OnOff;
};


LINK_ENTITY_TO_CLASS( beam_spotlight, CBeamSpotlight );

IMPLEMENT_REFLECT_DATAMAP( CBeamSpotlight )

IMPLEMENT_REFLECT_SERVERCLASS( CBeamSpotlight, DT_BeamSpotlight )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBeamSpotlight::CBeamSpotlight()
: m_bSpotlightOn( false )
, m_bHasDynamicLight( true )
, m_flSpotlightMaxLength( 500.0f )
, m_flSpotlightGoalWidth( 50.0f )
, m_flHDRColorScale( 0.7f )
, m_isRotating( false )
, m_isReversed( false )
, m_flmaxSpeed( 100.0f )
, m_flRotationSpeed(0.0f)
, m_nRotationAxis(0)
{
}

bool CBeamSpotlight::KeyValue_Impl( const char *szKeyName, const char *szValue )
{
	if ( !V_stricmp( szKeyName, "SpotlightWidth" ) )
	{
		m_flSpotlightGoalWidth = V_atof(szValue);
		if ( m_flSpotlightGoalWidth > MAX_BEAM_WIDTH )
		{
			Warning( "Map Bug:  %s has SpotLightWidth %f > %f, clamping value\n",
					 STRING( GetEntityName()), m_flSpotlightGoalWidth.m_Value, (float)MAX_BEAM_WIDTH );
			m_flSpotlightGoalWidth = Min( MAX_BEAM_WIDTH, m_flSpotlightGoalWidth.Get() );
		}
		return true;
	}
	
	return BaseClass::KeyValue_Impl( szKeyName, szValue );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBeamSpotlight::Precache_Impl( void )
{
	BaseClass::Precache_Impl();

	// Sprites.
	m_nHaloIndex = PrecacheModel("sprites/light_glow03.vmt");
	PrecacheModel( "sprites/glow_test02.vmt" );
}

//-----------------------------------------------------------------------------
void CBeamSpotlight::RecalcRotation( void )
{
	if ( !m_isRotating || m_flmaxSpeed == 0.0f ) 
	{
		m_flRotationSpeed = 0.0f;
		return;
	}

	//
	// Build the axis of rotation based on spawnflags.
	//
	// Pitch Yaw Roll -> Y Z X
	m_nRotationAxis = 1;
	if ( HasSpawnFlags(SF_BEAM_SPOTLIGHT_Y_AXIS) )
	{
		m_nRotationAxis = 0;
	}
	else if ( HasSpawnFlags(SF_BEAM_SPOTLIGHT_X_AXIS) )
	{
		m_nRotationAxis = 2;
	}

	m_flRotationSpeed = m_flmaxSpeed;
	if ( m_isReversed ) 
	{
		m_flRotationSpeed *= -1.0f;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBeamSpotlight::Spawn_Impl( void )
{
	Precache();

	UTIL_SetSize( this,vec3_origin,vec3_origin );
	AddSolidFlags( FSOLID_NOT_SOLID );
	SetMoveType( MOVETYPE_NONE );
	AddEFlags( EFL_FORCE_CHECK_TRANSMIT );

	m_bHasDynamicLight = !HasSpawnFlags( SF_BEAM_SPOTLIGHT_NO_DYNAMIC_LIGHT);
	m_bSpotlightOn = HasSpawnFlags( SF_BEAM_SPOTLIGHT_START_LIGHT_ON );
	m_isRotating = HasSpawnFlags( SF_BEAM_SPOTLIGHT_START_ROTATE_ON );
	m_isReversed = HasSpawnFlags( SF_BEAM_SPOTLIGHT_REVERSE_DIRECTION );

	RecalcRotation();
}

//-----------------------------------------------------------------------------
void CBeamSpotlight::InputTurnOn( inputdata_t &inputdata )
{
	if ( !m_bSpotlightOn )
	{
		m_bSpotlightOn = true;
	}
}


//-----------------------------------------------------------------------------
void CBeamSpotlight::InputTurnOff( inputdata_t &inputdata )
{
	if ( m_bSpotlightOn )
	{
		m_bSpotlightOn = false;
	}
}

//-----------------------------------------------------------------------------
void CBeamSpotlight::InputStart( inputdata_t &inputdata )
{
	if ( !m_isRotating )
	{
		m_isRotating = true;
		RecalcRotation();
	}
}

//-----------------------------------------------------------------------------
void CBeamSpotlight::InputStop( inputdata_t &inputdata )
{
	if ( m_isRotating )
	{
		m_isRotating = false;
		RecalcRotation();
	}
}

//-----------------------------------------------------------------------------
void CBeamSpotlight::InputReverse( inputdata_t &inputdata )
{
	m_isReversed = !m_isReversed;
	RecalcRotation();
}

//-------------------------------------------------------------------------------------
// Purpose : Send even though we don't have a model so spotlight gets proper position
//-------------------------------------------------------------------------------------
int CBeamSpotlight::UpdateTransmitState_Impl()
{
	return SetTransmitState( FL_EDICT_PVSCHECK );
}
