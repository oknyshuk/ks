// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

module;
#include <math.h>
#include "declare_class.h"
#include "eiface.h"
#include "networkvar.h"
#include "reflect.h"
#include "responserules/response_types.h"
#include "server_class.h"
#include "studio.h"
#include "vphysics_interface.h"
#include "baseentity.h"
#include "entity_dispatch.h"
#include "entity_dispatch_list.h"
#include "entity_kinds_dump.h"
#include "reflect_datamap.h"
#include "reflect_sendtable.h"
#include "util.h"
#include "map_utils.h"
#include "datamap.h"
#include "worldsize.h"
module ks.server;

import std;
import ks.core.basetypes;
import ks.core.dbg;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.interfaces;
import ks.tier1.reflect_wirevar;
import :fish;
import :vscript;
import :sharedvar;
import :baseentity;
import :player;
import :nav_4;
import :entity_dispatch;
import :entity_kind;
import :entity_2;
import ks.game.shareddefs;
import :cstrike15.bot.bot;
import :entityutil;
import ks.game.gamemovement;
import ks.game.mapentities_shared;
import ks.core.platform;
import ks.tier1.strtools;
import ks.tier1.convar;
import ks.tier1.iconvar;
import ks.tier1.createinterface;
import ks.tier1.registry;
import :cstrike15.cs_player;
import :cstrike15.bot.cs_bot;
import :cstrike15.cs_player_2;
import ks.core.types;
import ks.core.vprof;
import ks.math.ssemath;

class CBaseEntity;

using namespace ::ks::reflect;

// Purpose: Frequently used global functions.

// Landmark class
void CPointEntity::Spawn_Impl( void )
{
	SetSolid( SOLID_NONE );
//	UTIL_SetSize(this, vec3_origin, vec3_origin);
}

class CNullEntity : public CBaseEntity
{
public:
	DECLARE_CLASS( CNullEntity, CBaseEntity );

	void Spawn_Impl( void );
};

// Null Entity, remove on startup
void CNullEntity::Spawn_Impl( void )
{
	UTIL_Remove( this );
}
LINK_ENTITY_TO_CLASS(info_null,CNullEntity);

class CBaseDMStart : public CPointEntity
{
public:
	DECLARE_CLASS( CBaseDMStart, CPointEntity );

	bool IsTriggered_Impl( CBaseEntity *pEntity );

	DECLARE_DATADESC();

	[[= ks::reflect::Key{ .name = "master" } ]] string_t m_Master;

private:
};

IMPLEMENT_REFLECT_DATAMAP( CBaseDMStart )

// These are the new entry points to entities. 
LINK_ENTITY_TO_CLASS(info_player_deathmatch,CBaseDMStart);
LINK_ENTITY_TO_CLASS(info_player_start,CPointEntity);
LINK_ENTITY_TO_CLASS_ALIAS(info_landmark,CPointEntity);

bool CBaseDMStart::IsTriggered_Impl( CBaseEntity *pEntity )
{
	bool master = UTIL_IsMasterTriggered( m_Master, pEntity );

	return master;
}

// Convenient way to delay removing oneself
void CBaseEntity::SUB_Remove( void )
{
	if (m_iHealth > 0)
	{
		// this situation can screw up NPCs who can't tell their entity pointers are invalid.
		m_iHealth = 0;
		DevWarning( 2, "SUB_Remove called on entity with health > 0\n");
	}

	UTIL_Remove( this );
}

// Convenient way to explicitly do nothing (passed to functions that require a method)
void CBaseEntity::SUB_DoNothing( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Finds all active entities with the given targetname and calls their
//			'Use' function.
// Input  : targetName - Target name to search for.
//			pActivator - 
//			pCaller - 
//			useType - 
//			value - 
//-----------------------------------------------------------------------------
void FireTargets( const char *targetName, CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBaseEntity *pTarget = nullptr;
	if ( !targetName || !targetName[0] )
		return;

	DevMsg( 2, "Firing: (%s)\n", targetName );

	for (;;)
	{
		CBaseEntity *pSearchingEntity = pActivator;
		pTarget = gEntList.FindEntityByName( pTarget, targetName, pSearchingEntity, pActivator, pCaller );
		if ( !pTarget )
			break;

		if (!pTarget->IsMarkedForDeletion() )	// Don't use dying ents
		{
			DevMsg( 2, "[%03d] Found: %s, firing (%s)\n", gpGlobals->tickcount%1000, pTarget->GetDebugName(), targetName );
			pTarget->Use( pActivator, pCaller, useType, value );
		}
	}
}

enum togglemovetypes_t
{
	MOVE_TOGGLE_NONE = 0,
	MOVE_TOGGLE_LINEAR = 1,
	MOVE_TOGGLE_ANGULAR = 2,
};

// Global Savedata for Toggle
IMPLEMENT_REFLECT_DATAMAP( CBaseToggle )

IMPLEMENT_REFLECT_SERVERCLASS( CBaseToggle, DT_BaseToggle )

CBaseToggle::CBaseToggle()
{
}

//-----------------------------------------------------------------------------
// Purpose: Returns the velocity imparted to players standing on us.
//-----------------------------------------------------------------------------
void CBaseToggle::GetGroundVelocityToApply_Impl( Vector &vecGroundVel )
{
	vecGroundVel = GetLocalVelocity();
}

bool CBaseToggle::KeyValue_Impl( const char *szKeyName, const char *szValue )
{
	if (FStrEq(szKeyName, "lip"))
	{
		m_flLip = std::atof(szValue);
	}
	else if (FStrEq(szKeyName, "wait"))
	{
		m_flWait = std::atof(szValue);
	}
	else if (FStrEq(szKeyName, "master"))
	{
		m_sMaster = AllocPooledString(szValue);
	}
	else if (FStrEq(szKeyName, "distance"))
	{
		m_flMoveDistance = std::atof(szValue);
	}
	else
		return BaseClass::KeyValue_Impl( szKeyName, szValue );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Calculate m_vecVelocity and m_flNextThink to reach vecDest from
//			GetOrigin() traveling at flSpeed.
// Input  : Vector	vecDest - 
//			flSpeed - 
//-----------------------------------------------------------------------------
void CBaseToggle::LinearMove( const Vector &vecDest, float flSpeed )
{
	ASSERTSZ(flSpeed != 0, "LinearMove:  no speed is defined!");
	
	m_vecFinalDest = vecDest;

	m_movementType = MOVE_TOGGLE_LINEAR;
	// Already there?
	if (vecDest == GetLocalOrigin())
	{
		MoveDone();
		return;
	}
		
	// set destdelta to the vector needed to move
	Vector vecDestDelta = vecDest - GetLocalOrigin();
	
	// divide vector length by speed to get time to reach dest
	float flTravelTime = vecDestDelta.Length() / flSpeed;

	// set m_flNextThink to trigger a call to LinearMoveDone when dest is reached
	SetMoveDoneTime( flTravelTime );
	m_flMoveTargetTime = gpGlobals->curtime + flTravelTime;

	// scale the destdelta vector by the time spent traveling to get velocity
	SetLocalVelocity( vecDestDelta / flTravelTime );
}

void CBaseToggle::MoveDone_Impl( void )
{
	switch ( m_movementType )
	{
	case MOVE_TOGGLE_LINEAR:
		LinearMoveDone();
		break;
	case MOVE_TOGGLE_ANGULAR:
		AngularMoveDone();
		break;
	}
	m_movementType = MOVE_TOGGLE_NONE;
	BaseClass::MoveDone_Impl();
}
//-----------------------------------------------------------------------------
// Purpose: After moving, set origin to exact final destination, call "move done" function.
//-----------------------------------------------------------------------------
void CBaseToggle::LinearMoveDone( void )
{
	UTIL_SetOrigin( this, m_vecFinalDest);
	SetAbsVelocity( vec3_origin );
	SetMoveDoneTime( -1 );
}

// DVS TODO: obselete, remove?
bool CBaseToggle::IsLockedByMaster_Impl( void )
{
	if (m_sMaster != NULL_STRING && !UTIL_IsMasterTriggered(m_sMaster, m_hActivator))
		return true;
	else
		return false;
}

//-----------------------------------------------------------------------------
// Purpose: Calculate m_vecVelocity and m_flNextThink to reach vecDest from
//			GetLocalOrigin() traveling at flSpeed. Just like LinearMove, but rotational.
// Input  : vecDestAngle - 
//			flSpeed - 
//-----------------------------------------------------------------------------
void CBaseToggle::AngularMove( const QAngle &vecDestAngle, float flSpeed )
{
	ASSERTSZ(flSpeed != 0, "AngularMove:  no speed is defined!");
	
	m_vecFinalAngle = vecDestAngle;

	m_movementType = MOVE_TOGGLE_ANGULAR;
	// Already there?
	if (vecDestAngle == GetLocalAngles())
	{
		MoveDone();
		return;
	}
	
	// set destdelta to the vector needed to move
	QAngle vecDestDelta = vecDestAngle - GetLocalAngles();
	
	// divide by speed to get time to reach dest
	float flTravelTime = vecDestDelta.Length() / flSpeed;

	const float MinTravelTime = 0.01f;
	if ( flTravelTime < MinTravelTime )
	{
		// If we only travel for a short time, we can fail WillSimulateGamePhysics()
		flTravelTime = MinTravelTime;
		flSpeed = vecDestDelta.Length() / flTravelTime;
	}

	// set m_flNextThink to trigger a call to AngularMoveDone when dest is reached
	SetMoveDoneTime( flTravelTime );

	// scale the destdelta vector by the time spent traveling to get velocity
	SetLocalAngularVelocity( vecDestDelta * (1.0 / flTravelTime) );
}

//-----------------------------------------------------------------------------
// Purpose: After rotating, set angle to exact final angle, call "move done" function.
//-----------------------------------------------------------------------------
void CBaseToggle::AngularMoveDone( void )
{
	SetLocalAngles( m_vecFinalAngle );
	SetLocalAngularVelocity( vec3_angle );
	SetMoveDoneTime( -1 );
}

float CBaseToggle::AxisValue( int flags, const QAngle &angles )
{
	if ( FBitSet(flags, SF_DOOR_ROTATE_ROLL) )
		return angles.z;
	if ( FBitSet(flags, SF_DOOR_ROTATE_PITCH) )
		return angles.x;

	return angles.y;
}

void CBaseToggle::AxisDir( void )
{
	if ( m_spawnflags & SF_DOOR_ROTATE_ROLL )
		m_vecMoveAng = QAngle( 0, 0, 1 );	// angles are roll
	else if ( m_spawnflags & SF_DOOR_ROTATE_PITCH )
		m_vecMoveAng = QAngle( 1, 0, 0 );	// angles are pitch
	else
		m_vecMoveAng = QAngle( 0, 1, 0 );		// angles are yaw
}

float CBaseToggle::AxisDelta( int flags, const QAngle &angle1, const QAngle &angle2 )
{
	// UNDONE: Use AngleDistance() here?
	if ( FBitSet (flags, SF_DOOR_ROTATE_ROLL) )
		return angle1.z - angle2.z;
	
	if ( FBitSet (flags, SF_DOOR_ROTATE_PITCH) )
		return angle1.x - angle2.x;

	return angle1.y - angle2.y;
}

class CBaseEntity;
struct inputdata_t;

using namespace ::ks::reflect;

class CSun : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_Sun", .base = false } ]]
	[[= ks::reflect::From<"m_clrRender", ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED }, SendProxy_Color32ToInt32>{} ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CSun, CBaseEntity );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CSun();

	void	Activate_Impl();

	// Input handlers
	[[= ks::reflect::Input{ .name = "TurnOn", .type = FIELD_VOID } ]] void InputTurnOn( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "TurnOff", .type = FIELD_VOID } ]] void InputTurnOff( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetColor", .type = FIELD_COLOR32 } ]] void InputSetColor( inputdata_t &inputdata );

	int UpdateTransmitState_Impl();

public:
	CNetworkVector( m_vDirection, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NORMAL, .enc = ks::reflect::WireEnc::Vector } ]] );
	
	[[= ks::reflect::Key{ .name = "material" } ]] string_t	m_strMaterial;
	[[= ks::reflect::Key{ .name = "overlaymaterial" } ]] string_t	m_strOverlayMaterial;

	[[= ks::reflect::Key{ .name = "use_angles" } ]] int		m_bUseAngles;
	[[= ks::reflect::Key{ .name = "pitch" } ]] float	m_flPitch;
	[[= ks::reflect::Key{ .name = "angle" } ]] float	m_flYaw;
	
	CNetworkVar( int, m_nSize, [[= ks::reflect::Net{ .bits = 10, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "size" } ]] );		// Size of the main core image
	CNetworkVar( int, m_nOverlaySize, [[= ks::reflect::Net{ .bits = 10, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "overlaysize" } ]] ); // Size for the glow overlay
	CNetworkVar( color32, m_clrOverlay, [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Proxy<SendProxy_Color32ToInt32, ks::reflect::WireSide::Send>{} ]] [[= ks::reflect::Key{ .name = "overlaycolor" } ]] );
	CNetworkVar( bool, m_bOn, [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, m_nMaterial, [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, m_nOverlayMaterial, [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( float, m_flHDRColorScale, [[= ks::reflect::Net{ .bits = 0, .low = 0.0f, .high = 100.0f, .flags = SPROP_NOSCALE, .wire = "HDRColorScale" } ]] [[= ks::reflect::Key{ .name = "HDRColorScale" } ]] );
};

IMPLEMENT_REFLECT_SERVERCLASS( CSun, DT_Sun )

LINK_ENTITY_TO_CLASS( env_sun, CSun );

IMPLEMENT_REFLECT_DATAMAP( CSun )

CSun::CSun()
{
	m_vDirection.Init( 0, 0, 1 );
	
	m_bUseAngles = false;
	m_flPitch = 0;
	m_flYaw = 0;
	m_nSize = 16;

	m_bOn = true;
	AddEFlags( EFL_FORCE_CHECK_TRANSMIT );

	m_strMaterial = NULL_STRING;
	m_strOverlayMaterial = NULL_STRING;
	m_nOverlaySize = -1;
}

void CSun::Activate_Impl()
{
	BaseClass::Activate_Impl();

	// Find our target.
	if ( m_bUseAngles )
	{
		SetupLightNormalFromProps( GetAbsAngles(), m_flYaw, m_flPitch, m_vDirection.GetForModify() );
		m_vDirection = -m_vDirection.Get();
	}
	else
	{
		CBaseEntity *pEnt = gEntList.FindEntityByName( 0, m_target );
		if( pEnt )
		{
			Vector vDirection = GetAbsOrigin() - pEnt->GetAbsOrigin();
			VectorNormalize( vDirection );
			m_vDirection = vDirection;
		}
	}

	// Default behavior
	if ( m_nOverlaySize == -1 )
	{
		m_nOverlaySize = m_nSize;
	}

	// Cache off our image indices
	if ( m_strMaterial == NULL_STRING )
	{
		m_strMaterial = AllocPooledString( "sprites/light_glow02_add_noz.vmt" );
	}
	else 
	{
		const char *pExtension = V_GetFileExtension( STRING( m_strMaterial ) );
		if ( !pExtension )
		{
			char szFixedString[MAX_PATH];
			V_strncpy( szFixedString, STRING( m_strMaterial ), sizeof( szFixedString ) );
			V_strncat( szFixedString, ".vmt", sizeof( szFixedString ) );
			m_strMaterial = AllocPooledString( szFixedString );
		}
	}

	if ( m_strOverlayMaterial == NULL_STRING )
	{
		m_strOverlayMaterial = AllocPooledString( "sprites/light_glow02_add_noz.vmt" );
	}
	else 
	{
		const char *pExtension = V_GetFileExtension( STRING( m_strOverlayMaterial ) );
		if ( !pExtension )
		{
			char szFixedString[MAX_PATH];
			V_strncpy( szFixedString, STRING( m_strOverlayMaterial ), sizeof( szFixedString ) );
			V_strncat( szFixedString, ".vmt", sizeof( szFixedString ) );
			m_strOverlayMaterial = AllocPooledString( szFixedString );
		}
	}

	m_nMaterial = PrecacheModel( STRING( m_strMaterial ) );
	m_nOverlayMaterial = PrecacheModel( STRING( m_strOverlayMaterial ) );
}

void CSun::InputTurnOn( inputdata_t &inputdata )
{
	if( !m_bOn )
	{
		m_bOn = true;
	}
}

void CSun::InputTurnOff( inputdata_t &inputdata )
{
	if ( m_bOn )
	{
		m_bOn = false;
	}
}

void CSun::InputSetColor( inputdata_t &inputdata )
{
	m_clrRender = inputdata.value.Color32();
}

int CSun::UpdateTransmitState_Impl()
{
	return SetTransmitState( FL_EDICT_ALWAYS );
}

class CBaseEntity;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Sunlight shadow control entity.

//------------------------------------------------------------------------------
// FIXME: This really should inherit from something	more lightweight
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Purpose : Sunlight shadow control entity
//------------------------------------------------------------------------------
class CSunlightShadowControl : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_SunlightShadowControl", .base = false } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CSunlightShadowControl, CBaseEntity );

	CSunlightShadowControl();

	void Spawn_Impl( void );
	using BaseClass::KeyValue_Impl;
	bool KeyValue_Impl( const char *szKeyName, const char *szValue );
	bool GetKeyValue_Impl( const char *szKeyName, char *szValue, int iMaxLen );
	int  UpdateTransmitState_Impl();

	// Inputs
	[[= ks::reflect::Input{ .name = "SetAngles", .type = FIELD_STRING } ]] void	InputSetAngles( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void	InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void	InputDisable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetTexture", .type = FIELD_STRING } ]] void	InputSetTexture( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "EnableShadows", .type = FIELD_BOOLEAN } ]] void	InputSetEnableShadows( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "LightColor", .type = FIELD_COLOR32 } ]] void	InputSetLightColor( inputdata_t &inputdata );

	int	ObjectCaps_Impl( void ) { return BaseClass::ObjectCaps_Impl() & ~FCAP_ACROSS_TRANSITION; }

	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

private:
	CNetworkVector( m_shadowDirection, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE, .enc = ks::reflect::WireEnc::Vector } ]] );

	CNetworkVar( bool, m_bEnabled, [[= ks::reflect::Net{} ]] [[= ks::reflect::Key{ .name = "enabled" } ]] );
	[[= ks::reflect::Key{ .name = "StartDisabled" } ]] bool m_bStartDisabled;

	CNetworkString( m_TextureName, MAX_PATH, [[= ks::reflect::Net{} ]] );
	CNetworkColor32( m_LightColor, [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Proxy<SendProxy_Color32ToInt32, ks::reflect::WireSide::Send>{} ]] );
	CNetworkVar( float, m_flColorTransitionTime, [[= ks::reflect::Net{ .bits = 32 } ]] [[= ks::reflect::Key{ .name = "colortransitiontime" } ]] );
	CNetworkVar( float, m_flSunDistance, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "distance" } ]] [[= ks::reflect::Key{ .name = "SetDistance", .input = true } ]] );
	CNetworkVar( float, m_flFOV, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "fov" } ]] [[= ks::reflect::Key{ .name = "SetFOV", .input = true } ]] );
	CNetworkVar( float, m_flNearZ, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "nearz" } ]] [[= ks::reflect::Key{ .name = "SetNearZDistance", .input = true } ]] );
	CNetworkVar( float, m_flNorthOffset, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "northoffset" } ]] [[= ks::reflect::Key{ .name = "SetNorthOffset", .input = true } ]] );
	CNetworkVar( bool, m_bEnableShadows, [[= ks::reflect::Net{} ]] [[= ks::reflect::Key{ .name = "enableshadows" } ]] );
};

LINK_ENTITY_TO_CLASS(sunlight_shadow_control, CSunlightShadowControl);

IMPLEMENT_REFLECT_DATAMAP( CSunlightShadowControl )

IMPLEMENT_REFLECT_SERVERCLASS( CSunlightShadowControl, DT_SunlightShadowControl )

CSunlightShadowControl::CSunlightShadowControl()
{
	V_strcpy( m_TextureName.GetForModify(), "effects/flashlight001" );
	m_LightColor.Init( 255, 255, 255, 1 );
	m_flColorTransitionTime = 0.5f;
	m_flSunDistance = 10000.0f;
	m_flFOV = 5.0f;
	m_bEnableShadows = false;
}

//------------------------------------------------------------------------------
// Purpose : Send even though we don't have a model
//------------------------------------------------------------------------------
int CSunlightShadowControl::UpdateTransmitState_Impl()
{
	// ALWAYS transmit to all clients.
	return SetTransmitState( FL_EDICT_ALWAYS );
}

bool CSunlightShadowControl::KeyValue_Impl( const char *szKeyName, const char *szValue )
{
	if ( FStrEq( szKeyName, "color" ) )
	{
		float tmp[4];
		UTIL_StringToFloatArray( tmp, 4, szValue );

		m_LightColor.SetR( tmp[0] );
		m_LightColor.SetG( tmp[1] );
		m_LightColor.SetB( tmp[2] );
		m_LightColor.SetA( tmp[3] );
	}
	else if ( FStrEq( szKeyName, "angles" ) )
	{
		QAngle angles;
		UTIL_StringToVector( angles.Base(), szValue );
		if (angles == vec3_angle)
		{
			angles.Init( 80, 30, 0 );
		}
		Vector vForward;
		AngleVectors( angles, &vForward );
		m_shadowDirection = vForward;
		return true;
	}
	else if ( FStrEq( szKeyName, "texturename" ) )
	{
		V_strcpy( m_TextureName.GetForModify(), szValue );
	}

	return BaseClass::KeyValue_Impl( szKeyName, szValue );
}

bool CSunlightShadowControl::GetKeyValue_Impl( const char *szKeyName, char *szValue, int iMaxLen )
{
	if ( FStrEq( szKeyName, "color" ) )
	{
		V_snprintf( szValue, iMaxLen, "%d %d %d %d", m_LightColor.GetR(), m_LightColor.GetG(), m_LightColor.GetB(), m_LightColor.GetA() );
		return true;
	}
	else if ( FStrEq( szKeyName, "texturename" ) )
	{
		V_snprintf( szValue, iMaxLen, "%s", m_TextureName.Get() );
		return true;
	}
	return BaseClass::GetKeyValue_Impl( szKeyName, szValue, iMaxLen );
}

//------------------------------------------------------------------------------
// Purpose :
//------------------------------------------------------------------------------
void CSunlightShadowControl::Spawn_Impl( void )
{
	Precache();
	SetSolid( SOLID_NONE );

	if( m_bStartDisabled )
	{
		m_bEnabled = false;
	}
	else
	{
		m_bEnabled = true;
	}
}

//------------------------------------------------------------------------------
// Input values
//------------------------------------------------------------------------------
void CSunlightShadowControl::InputSetAngles( inputdata_t &inputdata )
{
	const char *pAngles = inputdata.value.String();

	QAngle angles;
	UTIL_StringToVector( angles.Base(), pAngles );

	Vector vTemp;
	AngleVectors( angles, &vTemp );
	m_shadowDirection = vTemp;
}

//------------------------------------------------------------------------------
// Purpose : Input handlers
//------------------------------------------------------------------------------
void CSunlightShadowControl::InputEnable( inputdata_t &inputdata )
{
	m_bEnabled = true;
}

void CSunlightShadowControl::InputDisable( inputdata_t &inputdata )
{
	m_bEnabled = false;
}

void CSunlightShadowControl::InputSetTexture( inputdata_t &inputdata )
{
	V_strcpy( m_TextureName.GetForModify(), inputdata.value.String() );
}

void CSunlightShadowControl::InputSetEnableShadows( inputdata_t &inputdata )
{
	m_bEnableShadows = inputdata.value.Bool();
}

void CSunlightShadowControl::InputSetLightColor( inputdata_t &inputdata )
{
	m_LightColor = inputdata.value.Color32();
}

class CBaseEntity;
class CCSPlayer;
class CTakeDamageInfo;

using namespace ::ks::reflect;

ConVar phys_pushscale( side_registry(), "phys_pushscale", "1", FCVAR_REPLICATED );

IMPLEMENT_REFLECT_DATAMAP_SIMPLE( CTakeDamageInfo )

void CTakeDamageInfo::Init( CBaseEntity *pInflictor, CBaseEntity *pAttacker, CBaseEntity *pWeapon, const Vector &damageForce, const Vector &damagePosition, const Vector &reportedPosition, float flDamage, int bitsDamageType, int iCustomDamage, int iObjectsPenetrated )
{
	m_hInflictor = pInflictor;
	if ( pAttacker )
	{
		m_hAttacker = pAttacker;
	}
	else
	{
		m_hAttacker = pInflictor;
	}

	m_hWeapon = pWeapon;

	m_flDamage = flDamage;

	m_flBaseDamage = BASEDAMAGE_NOT_SPECIFIED;

	m_bitsDamageType = bitsDamageType;
	m_iDamageCustom = iCustomDamage;

	m_flMaxDamage = flDamage;
	m_vecDamageForce = damageForce;
	m_vecDamagePosition = damagePosition;
	m_vecReportedPosition = reportedPosition;
	m_iAmmoType = -1;
	m_flRadius = 0.0f;
	m_iDamagedOtherPlayers = 0;
	m_iObjectsPenetrated = iObjectsPenetrated;
	m_uiBulletID = CCSPlayer::GetBulletGroup();
}

CTakeDamageInfo::CTakeDamageInfo()
{
	Init( nullptr, nullptr, nullptr, vec3_origin, vec3_origin, vec3_origin, 0, 0, 0, 0 );
}

CTakeDamageInfo::CTakeDamageInfo( CBaseEntity *pInflictor, CBaseEntity *pAttacker, float flDamage, int bitsDamageType, int iKillType, int iObjectsPenetrated )
{
	Set( pInflictor, pAttacker, flDamage, bitsDamageType, iKillType, iObjectsPenetrated );
}

CTakeDamageInfo::CTakeDamageInfo( CBaseEntity *pInflictor, CBaseEntity *pAttacker, CBaseEntity *pWeapon, float flDamage, int bitsDamageType, int iKillType, int iObjectsPenetrated )
{
	Set( pInflictor, pAttacker, pWeapon, flDamage, bitsDamageType, iKillType, iObjectsPenetrated );
}

CTakeDamageInfo::CTakeDamageInfo( CBaseEntity *pInflictor, CBaseEntity *pAttacker, const Vector &damageForce, const Vector &damagePosition, float flDamage, int bitsDamageType, int iKillType, Vector *reportedPosition, int iObjectsPenetrated )
{
	Set( pInflictor, pAttacker, damageForce, damagePosition, flDamage, bitsDamageType, iKillType, reportedPosition, iObjectsPenetrated );
}

CTakeDamageInfo::CTakeDamageInfo( CBaseEntity *pInflictor, CBaseEntity *pAttacker, CBaseEntity *pWeapon, const Vector &damageForce, const Vector &damagePosition, float flDamage, int bitsDamageType, int iKillType, Vector *reportedPosition, int iObjectsPenetrated )
{
	Set( pInflictor, pAttacker, pWeapon, damageForce, damagePosition, flDamage, bitsDamageType, iKillType, reportedPosition, iObjectsPenetrated );
}

void CTakeDamageInfo::Set( CBaseEntity *pInflictor, CBaseEntity *pAttacker, float flDamage, int bitsDamageType, int iKillType, int iObjectsPenetrated )
{
	Init( pInflictor, pAttacker, nullptr, vec3_origin, vec3_origin, vec3_origin, flDamage, bitsDamageType, iKillType, iObjectsPenetrated );
}

void CTakeDamageInfo::Set( CBaseEntity *pInflictor, CBaseEntity *pAttacker, CBaseEntity *pWeapon, float flDamage, int bitsDamageType, int iKillType, int iObjectsPenetrated )
{
	Init( pInflictor, pAttacker, pWeapon, vec3_origin, vec3_origin, vec3_origin, flDamage, bitsDamageType, iKillType, iObjectsPenetrated );
}

void CTakeDamageInfo::Set( CBaseEntity *pInflictor, CBaseEntity *pAttacker, const Vector &damageForce, const Vector &damagePosition, float flDamage, int bitsDamageType, int iKillType, Vector *reportedPosition, int iObjectsPenetrated )
{
	Set( pInflictor, pAttacker, nullptr, damageForce, damagePosition, flDamage, bitsDamageType, iKillType, reportedPosition, iObjectsPenetrated );
}

void CTakeDamageInfo::Set( CBaseEntity *pInflictor, CBaseEntity *pAttacker, CBaseEntity *pWeapon, const Vector &damageForce, const Vector &damagePosition, float flDamage, int bitsDamageType, int iKillType, Vector *reportedPosition, int iObjectsPenetrated )
{
	Vector vecReported = vec3_origin;
	if ( reportedPosition )
	{
		vecReported = *reportedPosition;
	}
	Init( pInflictor, pAttacker, pWeapon, damageForce, damagePosition, vecReported, flDamage, bitsDamageType, iKillType, iObjectsPenetrated );
}

//-----------------------------------------------------------------------------
// Squirrel the damage value away as BaseDamage, which will later be used to 
// calculate damage force. 
//-----------------------------------------------------------------------------
void CTakeDamageInfo::AdjustPlayerDamageInflictedForSkillLevel()
{
	CopyDamageToBaseDamage();
	SetDamage( g_pGameRules->AdjustPlayerDamageInflicted(GetDamage()) );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTakeDamageInfo::AdjustPlayerDamageTakenForSkillLevel()
{
	CopyDamageToBaseDamage();
	g_pGameRules->AdjustPlayerDamageTaken(this);
}

//-----------------------------------------------------------------------------
// Purpose: get the name of the ammo that caused damage
// Note: returns the ammo name, or the classname of the object, or the model name in the case of physgun ammo.
//-----------------------------------------------------------------------------
const char *CTakeDamageInfo::GetAmmoName() const
{
	const char *pszAmmoType;

	if ( m_iAmmoType >= 0 )
	{
		pszAmmoType = GetAmmoDef()->GetAmmoOfIndex( m_iAmmoType )->pName;
	}
	// no ammoType, so get the ammo name from the inflictor
	else if ( m_hInflictor != nullptr )
	{
		pszAmmoType = m_hInflictor->GetClassname();

		// check for physgun ammo.  unfortunate that this is in game_shared.
		if ( V_strcmp( pszAmmoType, "prop_physics" ) == 0 )
		{
			pszAmmoType = STRING( m_hInflictor->GetModelName() );
		}
	}
	else
	{
		pszAmmoType = "Unknown";
	}

	return pszAmmoType;
}

// -------------------------------------------------------------------------------------------------- //
// MultiDamage
// Collects multiple small damages into a single damage
// -------------------------------------------------------------------------------------------------- //
IMPLEMENT_REFLECT_DATAMAP_SIMPLE_( CMultiDamage, CTakeDamageInfo )

CMultiDamage g_MultiDamage;

CMultiDamage::CMultiDamage()
{
	m_hTarget = nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMultiDamage::Init( CBaseEntity *pTarget, CBaseEntity *pInflictor, CBaseEntity *pAttacker, CBaseEntity *pWeapon, const Vector &damageForce, const Vector &damagePosition, const Vector &reportedPosition, float flDamage, int bitsDamageType, int iKillType, int iObjectsPenetrated )
{
	m_hTarget = pTarget;
	BaseClass::Init( pInflictor, pAttacker, pWeapon, damageForce, damagePosition, reportedPosition, flDamage, bitsDamageType, iKillType, iObjectsPenetrated );
}

//-----------------------------------------------------------------------------
// Purpose: Resets the global multi damage accumulator
//-----------------------------------------------------------------------------
void ClearMultiDamage( void )
{
	g_MultiDamage.Init( nullptr, nullptr, nullptr, nullptr, vec3_origin, vec3_origin, vec3_origin, 0, 0, 0, 0 );
}

//-----------------------------------------------------------------------------
// Purpose: inflicts contents of global multi damage register on gMultiDamage.pEntity
//-----------------------------------------------------------------------------
void ApplyMultiDamage( void )
{
	Vector		vecSpot1;//where blood comes from
	Vector		vecDir;//direction blood should go
	trace_t		tr;

	if ( !g_MultiDamage.GetTarget() )
		return;

	const CBaseEntity *host = te->GetSuppressHost();
	te->SetSuppressHost( nullptr );
		
	g_MultiDamage.GetTarget()->TakeDamage( g_MultiDamage );

	te->SetSuppressHost( (CBaseEntity*)host );

	// Damage is done, clear it out
	ClearMultiDamage();
}

//-----------------------------------------------------------------------------
// Purpose: Add damage to the existing multidamage, and apply if it won't fit
//-----------------------------------------------------------------------------
void AddMultiDamage( const CTakeDamageInfo &info, CBaseEntity *pEntity )
{
	if ( !pEntity )
		return;

	if ( pEntity != g_MultiDamage.GetTarget() )
	{
		ApplyMultiDamage();
		g_MultiDamage.Init( pEntity, info.GetInflictor(), info.GetAttacker(), info.GetWeapon(), vec3_origin, vec3_origin, vec3_origin, 0.0, info.GetDamageType(), info.GetDamageCustom(), info.GetObjectsPenetrated() );
		g_MultiDamage.SetDamagedOtherPlayers( info.GetDamagedOtherPlayers() );
	}

	g_MultiDamage.AddDamageType( info.GetDamageType() );
	g_MultiDamage.SetDamage( g_MultiDamage.GetDamage() + info.GetDamage() );
	g_MultiDamage.SetDamageForce( g_MultiDamage.GetDamageForce() + info.GetDamageForce() );
	g_MultiDamage.SetDamagePosition( info.GetDamagePosition() );
	g_MultiDamage.SetReportedPosition( info.GetReportedPosition() );
	g_MultiDamage.SetMaxDamage( std::max( g_MultiDamage.GetMaxDamage(), info.GetMaxDamage() ) );
	g_MultiDamage.SetAmmoType( info.GetAmmoType() );
	g_MultiDamage.SetBulletID( info.GetBulletID(), info.GetRecoilIndex() );

	bool bHasPhysicsForceDamage = !g_pGameRules->Damage_NoPhysicsForce( info.GetDamageType() );
	if ( bHasPhysicsForceDamage && g_MultiDamage.GetDamageType() != DMG_GENERIC )
	{
		// If you hit this assert, you've called TakeDamage with a damage type that requires a physics damage
		// force & position without specifying one or both of them. Decide whether your damage that's causing 
		// this is something you believe should impart physics force on the receiver. If it is, you need to 
		// setup the damage force & position inside the CTakeDamageInfo (Utility functions for this are in
		// takedamageinfo.cpp. If you think the damage shouldn't cause force (unlikely!) then you can set the 
		// damage type to DMG_GENERIC, or | DMG_CRUSH if you need to preserve the damage type for purposes of HUD display.
		if ( g_MultiDamage.GetDamageForce() == vec3_origin || g_MultiDamage.GetDamagePosition() == vec3_origin )
		{
			static int warningCount = 0;
			if ( ++warningCount < 10 )
			{
				if ( g_MultiDamage.GetDamageForce() == vec3_origin )
				{
					Warning( "AddMultiDamage:  g_MultiDamage.GetDamageForce() == vec3_origin\n" );
				}

				if ( g_MultiDamage.GetDamagePosition() == vec3_origin)
				{
					Warning( "AddMultiDamage:  g_MultiDamage.GetDamagePosition() == vec3_origin\n" );
				}
			}
		}
	}
}

//============================================================================================================
// Utility functions for physics damage force calculation 
//============================================================================================================
//-----------------------------------------------------------------------------
// Purpose: Returns an impulse scale required to push an object.
// Input  : flTargetMass - Mass of the target object, in kg
//			flDesiredSpeed - Desired speed of the target, in inches/sec.
//-----------------------------------------------------------------------------
float ImpulseScale( float flTargetMass, float flDesiredSpeed )
{
	return (flTargetMass * flDesiredSpeed);
}

//-----------------------------------------------------------------------------
// Purpose: Fill out a takedamageinfo with a damage force for an explosive
//-----------------------------------------------------------------------------
void CalculateExplosiveDamageForce( CTakeDamageInfo *info, const Vector &vecDir, const Vector &vecForceOrigin, float flScale )
{
	info->SetDamagePosition( vecForceOrigin );

	float flClampForce = ImpulseScale( 75, 400 );

	// Calculate an impulse large enough to push a 75kg man 4 in/sec per point of damage
	float flForceScale = info->GetBaseDamage() * ImpulseScale( 75, 4 );

	if( flForceScale > flClampForce )
		flForceScale = flClampForce;

	// Fudge blast forces a little bit, so that each
	// victim gets a slightly different trajectory. 
	// This simulates features that usually vary from
	// person-to-person variables such as bodyweight,
	// which are all indentical for characters using the same model.
	flForceScale *= random_valve->RandomFloat( 0.85, 1.15 );

	// Calculate the vector and stuff it into the takedamageinfo
	Vector vecForce = vecDir;
	VectorNormalize( vecForce );
	vecForce *= flForceScale;
	vecForce *= phys_pushscale.GetFloat();
	vecForce *= flScale;
	info->SetDamageForce( vecForce );
}

//-----------------------------------------------------------------------------
// Purpose: Fill out a takedamageinfo with a damage force for a bullet impact
//-----------------------------------------------------------------------------
void CalculateBulletDamageForce( CTakeDamageInfo *info, int iBulletType, const Vector &vecBulletDir, const Vector &vecForceOrigin, float flScale )
{
	info->SetDamagePosition( vecForceOrigin );
	Vector vecForce = vecBulletDir;
	VectorNormalize( vecForce );
	vecForce *= GetAmmoDef()->DamageForce( iBulletType );
	vecForce *= phys_pushscale.GetFloat();
	vecForce *= flScale;
	info->SetDamageForce( vecForce );
	contract_assert( vecForce!=vec3_origin );
}

//-----------------------------------------------------------------------------
// Purpose: Fill out a takedamageinfo with a damage force for a melee impact
//-----------------------------------------------------------------------------
void CalculateMeleeDamageForce( CTakeDamageInfo *info, const Vector &vecMeleeDir, const Vector &vecForceOrigin, float flScale )
{
	info->SetDamagePosition( vecForceOrigin );

	// Calculate an impulse large enough to push a 75kg man 4 in/sec per point of damage
	float flForceScale = info->GetBaseDamage() * ImpulseScale( 75, 4 );
	Vector vecForce = vecMeleeDir;
	VectorNormalize( vecForce );
	vecForce *= flForceScale;
	vecForce *= phys_pushscale.GetFloat();
	vecForce *= flScale;
	info->SetDamageForce( vecForce );
}

//-----------------------------------------------------------------------------
// Purpose: Try and guess the physics force to use.
//			This shouldn't be used for any damage where the damage force is unknown.
//			i.e. only use it for mapmaker specified damages.
//-----------------------------------------------------------------------------
void GuessDamageForce( CTakeDamageInfo *info, const Vector &vecForceDir, const Vector &vecForceOrigin, float flScale )
{
	if ( info->GetDamageType() & DMG_BULLET )
	{
		CalculateBulletDamageForce( info, GetAmmoDef()->Index("SMG1"), vecForceDir, vecForceOrigin, flScale );
	}
	else if ( info->GetDamageType() & DMG_BLAST )
	{
		CalculateExplosiveDamageForce( info, vecForceDir, vecForceOrigin, flScale );
	}
	else
	{
		CalculateMeleeDamageForce( info, vecForceDir, vecForceOrigin, flScale );
	}
}

// Debug functions for printing out damage types

// This table maps the DMG_* defines to their strings such that 
// for DMG_XXX = i << x  then table[i] = string for DMG_XXX

static const char * const s_DamageTypeToStrTable[] =
{
	"GENERIC",
	"CRUSH",
	"BULLET",
	"SLASH",
	"BURN",
	"VEHICLE",
	"FALL",
	"BLAST",
	"CLUB",
	"SHOCK",
	"SONIC",
	"ENERGYBEAM",
	"PREVENT_PHYSICS_FORCE",
	"NEVERGIB",
	"ALWAYSGIB",
	"DROWN",
	"PARALYZE",
	"NERVEGAS",
	"POISON",
	"RADIATION",
	"DROWNRECOVER",
	"ACID",
	"SLOWBURN",
	"REMOVENORAGDOLL",
	"PHYSGUN",
	"PLASMA",
	"AIRBOAT",
	"DISSOLVE",
	"BLAST_SURFACE",
	"DIRECT",
	"BUCKSHOT"
};
static constexpr auto DAMAGE_TYPE_STR_TABLE_ENTRIES = 31; // number of entries in table above

void CTakeDamageInfo::DebugGetDamageTypeString(unsigned int damageType, char *outbuf, int outbuflength )
{
	contract_assert( outbuflength > 0 );

	// we need to use snprintf to actually copy out the strings here because that's the only function that returns
	// how much text was output
	if ( damageType == 0 )
	{
		int charsWrit = V_snprintf(outbuf, outbuflength, "%s", s_DamageTypeToStrTable[0]);
		
		outbuflength -= charsWrit;
		outbuf += charsWrit; // advance the output pointer (now it sits on the null terminator)
	}

	// loop through the other entries in the table
	for (int i = 0;
		 outbuflength > 0 && i < (DAMAGE_TYPE_STR_TABLE_ENTRIES - 1);
		 ++i )
	{
		if ( damageType & (1 << i) )
		{
			// this bit was set. Print the corresponding entry from the table
			// (the index is +1 because entry 1 in the table corresponds to 1 << 0)
			int charsWrit = V_snprintf(outbuf, outbuflength, "%s ", s_DamageTypeToStrTable[i + 1]); 

			outbuflength -= charsWrit; // reduce the chars left
			outbuf += charsWrit; // advance the output pointer (now it sits on the null terminator)
		}
	}
}

/*
// instant damage

#define DMG_GENERIC			0			// generic damage was done
#define DMG_CRUSH			(1 << 0)	// crushed by falling or moving object. 
// NOTE: It's assumed crush damage is occurring as a result of physics collision, so no extra physics force is generated by crush damage.
// DON'T use DMG_CRUSH when damaging entities unless it's the result of a physics collision. You probably want DMG_CLUB instead.
#define DMG_BULLET			(1 << 1)	// shot
#define DMG_SLASH			(1 << 2)	// cut, clawed, stabbed
#define DMG_BURN			(1 << 3)	// heat burned
#define DMG_VEHICLE			(1 << 4)	// hit by a vehicle
#define DMG_FALL			(1 << 5)	// fell too far
#define DMG_BLAST			(1 << 6)	// explosive blast damage
#define DMG_CLUB			(1 << 7)	// crowbar, punch, headbutt
#define DMG_SHOCK			(1 << 8)	// electric shock
#define DMG_SONIC			(1 << 9)	// sound pulse shockwave
#define DMG_ENERGYBEAM		(1 << 10)	// laser or other high energy beam 
#define DMG_PREVENT_PHYSICS_FORCE		(1 << 11)	// Prevent a physics force 
#define DMG_NEVERGIB		(1 << 12)	// with this bit OR'd in, no damage type will be able to gib victims upon death
#define DMG_ALWAYSGIB		(1 << 13)	// with this bit OR'd in, any damage type can be made to gib victims upon death.
#define DMG_DROWN			(1 << 14)	// Drowning

#define DMG_PARALYZE		(1 << 15)	// slows affected creature down
#define DMG_NERVEGAS		(1 << 16)	// nerve toxins, very bad
#define DMG_POISON			(1 << 17)	// blood poisoning - heals over time like drowning damage
#define DMG_RADIATION		(1 << 18)	// radiation exposure
#define DMG_DROWNRECOVER	(1 << 19)	// drowning recovery
#define DMG_ACID			(1 << 20)	// toxic chemicals or acid burns
#define DMG_SLOWBURN		(1 << 21)	// in an oven

#define DMG_REMOVENORAGDOLL	(1<<22)		// with this bit OR'd in, no ragdoll will be created, and the target will be quietly removed.
// use this to kill an entity that you've already got a server-side ragdoll for

#define DMG_PHYSGUN			(1<<23)		// Hit by manipulator. Usually doesn't do any damage.
#define DMG_PLASMA			(1<<24)		// Shot by Cremator
#define DMG_AIRBOAT			(1<<25)		// Hit by the airboat's gun

#define DMG_DISSOLVE		(1<<26)		// Dissolving!
#define DMG_BLAST_SURFACE	(1<<27)		// A blast on the surface of water that cannot harm things underwater
#define DMG_DIRECT			(1<<28)
#define DMG_BUCKSHOT		(1<<29)		// not quite a bullet. Little, rounder, different.
*/

CBaseEntity* CTakeDamageInfo::GetInflictor() const
{
	return m_hInflictor;
}

void CTakeDamageInfo::SetInflictor( CBaseEntity *pInflictor )
{
	m_hInflictor = pInflictor;
}

CBaseEntity* CTakeDamageInfo::GetAttacker() const
{
	return m_hAttacker;
}

void CTakeDamageInfo::SetAttacker( CBaseEntity *pAttacker )
{
	m_hAttacker = pAttacker;
}

CBaseEntity* CTakeDamageInfo::GetWeapon() const
{
	return m_hWeapon;
}

void CTakeDamageInfo::SetWeapon( CBaseEntity *pWeapon )
{
	m_hWeapon = pWeapon;
}

float CTakeDamageInfo::GetDamage() const
{
	return m_flDamage;
}

void CTakeDamageInfo::SetDamage( float flDamage )
{
	m_flDamage = flDamage;
}

float CTakeDamageInfo::GetMaxDamage() const
{
	return m_flMaxDamage;
}

void CTakeDamageInfo::SetMaxDamage( float flMaxDamage )
{
	m_flMaxDamage = flMaxDamage;
}

void CTakeDamageInfo::ScaleDamage( float flScaleAmount )
{
	m_flDamage *= flScaleAmount;
}

void CTakeDamageInfo::AddDamage( float flAddAmount )
{
	m_flDamage += flAddAmount;
}

void CTakeDamageInfo::SubtractDamage( float flSubtractAmount )
{
	m_flDamage -= flSubtractAmount;
}

float CTakeDamageInfo::GetBaseDamage() const
{
	if( BaseDamageIsValid() )
		return m_flBaseDamage;

	// No one ever specified a base damage, so just return damage.
	return m_flDamage;
}

bool CTakeDamageInfo::BaseDamageIsValid() const
{
	return (m_flBaseDamage != BASEDAMAGE_NOT_SPECIFIED);
}

Vector CTakeDamageInfo::GetDamageForce() const
{
	return m_vecDamageForce;
}

void CTakeDamageInfo::SetDamageForce( const Vector &damageForce )
{
	m_vecDamageForce = damageForce;
}

void	CTakeDamageInfo::ScaleDamageForce( float flScaleAmount )
{
	m_vecDamageForce *= flScaleAmount;
}

Vector CTakeDamageInfo::GetDamagePosition() const
{
	return m_vecDamagePosition;
}

void CTakeDamageInfo::SetDamagePosition( const Vector &damagePosition )
{
	m_vecDamagePosition = damagePosition;
}

Vector CTakeDamageInfo::GetReportedPosition() const
{
	return m_vecReportedPosition;
}

void CTakeDamageInfo::SetReportedPosition( const Vector &reportedPosition )
{
	m_vecReportedPosition = reportedPosition;
}

int CTakeDamageInfo::GetDamageType() const
{
	return m_bitsDamageType;
}

void CTakeDamageInfo::SetDamageType( int bitsDamageType )
{
	m_bitsDamageType = bitsDamageType;
}

void	CTakeDamageInfo::AddDamageType( int bitsDamageType )
{
	m_bitsDamageType |= bitsDamageType;
}

int CTakeDamageInfo::GetDamageCustom() const
{
	return m_iDamageCustom;
}

void CTakeDamageInfo::SetDamageCustom( int iDamageCustom )
{
	m_iDamageCustom = iDamageCustom;
}

int CTakeDamageInfo::GetDamageStats() const
{
	return m_iDamageCustom;
}

void CTakeDamageInfo::SetDamageStats( int iDamageCustom )
{
	m_iDamageCustom = iDamageCustom;
}

int CTakeDamageInfo::GetAmmoType() const
{
	return m_iAmmoType;
}

void CTakeDamageInfo::SetAmmoType( int iAmmoType )
{
	m_iAmmoType = iAmmoType;
}

void CTakeDamageInfo::CopyDamageToBaseDamage()
{ 
	m_flBaseDamage = m_flDamage;
}

float CTakeDamageInfo::GetRadius() const
{
	return m_flRadius;
}

void CTakeDamageInfo::SetRadius( float flRadius )
{
	m_flRadius = flRadius;
}

int CTakeDamageInfo::GetObjectsPenetrated() const
{
	return m_iObjectsPenetrated;
}

void CTakeDamageInfo::SetObjectsPenetrated( int iObjectsPenetrated )
{
	m_iObjectsPenetrated = iObjectsPenetrated;
}

// -------------------------------------------------------------------------------------------------- //
// Inlines.
// -------------------------------------------------------------------------------------------------- //
CBaseEntity *CMultiDamage::GetTarget() const
{
	return m_hTarget;
}

void CMultiDamage::SetTarget( CBaseEntity *pTarget )
{
	m_hTarget = pTarget;
}

void CTakeDamageInfo::SetBulletID( uint32 bulletid, uint8 uiRecoilIndex)
{
	m_uiBulletID = bulletid; 
	m_uiRecoilIndex = uiRecoilIndex;
}

#undef DMG_GENERIC
#undef DMG_CRUSH
#undef DMG_BULLET
#undef DMG_SLASH
#undef DMG_BURN
#undef DMG_VEHICLE
#undef DMG_FALL
#undef DMG_BLAST
#undef DMG_CLUB
#undef DMG_SHOCK
#undef DMG_SONIC
#undef DMG_ENERGYBEAM
#undef DMG_PREVENT_PHYSICS_FORCE
#undef DMG_NEVERGIB
#undef DMG_ALWAYSGIB
#undef DMG_DROWN
#undef DMG_PARALYZE
#undef DMG_NERVEGAS
#undef DMG_POISON
#undef DMG_RADIATION
#undef DMG_DROWNRECOVER
#undef DMG_ACID
#undef DMG_SLOWBURN
#undef DMG_REMOVENORAGDOLL
#undef DMG_PHYSGUN
#undef DMG_PLASMA
#undef DMG_AIRBOAT
#undef DMG_DISSOLVE
#undef DMG_BLAST_SURFACE
#undef DMG_DIRECT
#undef DMG_BUCKSHOT

class CBaseEntity;
class CTakeDamageInfo;
struct inputdata_t;

using namespace ::ks::reflect;

void UTIL_RemoveHierarchy( CBaseEntity *pDead )
{
	if ( !pDead )
		return;

	if ( pDead->edict() )
	{
		CBaseEntity *pChild = pDead->FirstMoveChild();
		while ( pChild )
		{
			CBaseEntity *pEntity = pChild;
			pChild = pChild->NextMovePeer();

			UTIL_RemoveHierarchy( pEntity );
		}
	}
	UTIL_Remove( pDead );
}

class CFuncTankTrain : public CFuncTrackTrain
{
public:
	DECLARE_CLASS( CFuncTankTrain, CFuncTrackTrain );

	void Spawn_Impl( void );

	// Filter out damage messages that don't contain blast damage (impervious to other forms of attack)
	int	OnTakeDamage_Impl( const CTakeDamageInfo &info );
	void Event_Killed_Impl( const CTakeDamageInfo &info );
	void Blocked_Impl( CBaseEntity *pOther )
	{
		// FIxme, set speed to zero?
	}
	DECLARE_DATADESC();

private:

	[[= ks::reflect::Key{ .name = "OnDeath" } ]] COutputEvent m_OnDeath;
};

LINK_ENTITY_TO_CLASS( func_tanktrain, CFuncTankTrain );

IMPLEMENT_REFLECT_DATAMAP( CFuncTankTrain )

void CFuncTankTrain::Spawn_Impl( void )
{
	m_takedamage = true;
	BaseClass::Spawn_Impl();
}

// Filter out damage messages that don't contain blast damage (impervious to other forms of attack)
int	CFuncTankTrain::OnTakeDamage_Impl( const CTakeDamageInfo &info )
{
	if ( ! (info.GetDamageType() & DMG_BLAST) )
		return 0;

	return BaseClass::OnTakeDamage_Impl( info );
}

//-----------------------------------------------------------------------------
// Purpose: Called when the train is killed.
// Input  : pInflictor - What killed us.
//			pAttacker - Who killed us.
//			flDamage - The damage that the killing blow inflicted.
//			bitsDamageType - Bitfield of damage types that were inflicted.
//-----------------------------------------------------------------------------
void CFuncTankTrain::Event_Killed_Impl( const CTakeDamageInfo &info )
{
	m_takedamage = DAMAGE_NO;
	m_lifeState = LIFE_DEAD;

	m_OnDeath.FireOutput( info.GetInflictor(), this );
}

//-----------------------------------------------------------------------------
// Purpose: Changes the target entity for a func_tank or tanktrain_ai
//-----------------------------------------------------------------------------
class CTankTargetChange : public CPointEntity
{
public:
	DECLARE_CLASS( CTankTargetChange, CPointEntity );

	void Precache_Impl( void );
	void Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	DECLARE_DATADESC();

private:
	variant_t	m_newTarget;
	[[= ks::reflect::Key{ .name = "newtarget" } ]] string_t	m_newTargetName;
};

LINK_ENTITY_TO_CLASS( tanktrain_aitarget, CTankTargetChange );

IMPLEMENT_REFLECT_DATAMAP( CTankTargetChange )

void CTankTargetChange::Precache_Impl( void )
{
	BaseClass::Precache_Impl();

	// This needs to be in Precache so save/load works
	m_newTarget.SetString( m_newTargetName );
}

void CTankTargetChange::Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBaseEntity *pTarget = gEntList.FindEntityByName( nullptr, m_target, nullptr, pActivator, pCaller );

	// UNDONE: This should use more of the event system
	while ( pTarget )
	{
		// Change the target over
		pTarget->AcceptInput( "TargetEntity", this, this, m_newTarget, 0 );
		pTarget = gEntList.FindEntityByName( pTarget, m_target, nullptr, pActivator, pCaller );
	}
}

// UNDONE: Should be just a logical entity, but we act as another static sound channel for the train
class CTankTrainAI : public CPointEntity
{
public:
	DECLARE_CLASS( CTankTrainAI, CPointEntity );

	virtual ~CTankTrainAI( void );

	void Precache_Impl( void );
	void Spawn_Impl( void );
	void Activate_Impl( void );
	void Think_Impl( void );

	int		SoundEnginePitch( void );
	void	SoundEngineStart( void );
	void	SoundEngineStop( void );
	void	SoundShutdown( void );

	CBaseEntity *FindTarget( string_t target, CBaseEntity *pActivator );

	DECLARE_DATADESC();

	// INPUTS
	[[= ks::reflect::Input{ .name = "TargetEntity", .type = FIELD_STRING } ]] void InputTargetEntity( inputdata_t &inputdata );

private:
	CHandle<CFuncTrackTrain>	m_hTrain;
	EHANDLE			m_hTargetEntity;
	int				m_soundPlaying;

	CSoundPatch		*m_soundTreads;
	CSoundPatch		*m_soundEngine;

	[[= ks::reflect::Key{ .name = "startsound" } ]] string_t		m_startSoundName;
	[[= ks::reflect::Key{ .name = "enginesound" } ]] string_t		m_engineSoundName;
	[[= ks::reflect::Key{ .name = "movementsound" } ]] string_t		m_movementSoundName;
	string_t		m_targetEntityName;
};

LINK_ENTITY_TO_CLASS( tanktrain_ai, CTankTrainAI );

IMPLEMENT_REFLECT_DATAMAP( CTankTrainAI )

//-----------------------------------------------------------------------------
// Purpose: Input handler for setting the target entity by name.
//-----------------------------------------------------------------------------
void CTankTrainAI::InputTargetEntity( inputdata_t &inputdata )
{
	m_targetEntityName = inputdata.value.StringID();
	m_hTargetEntity = FindTarget( m_targetEntityName, inputdata.pActivator );
	SetNextThink( gpGlobals->curtime );
}

//-----------------------------------------------------------------------------
// Purpose: Finds the first entity in the entity list with the given name.
// Input  : target - String ID of the entity to find.
//			pActivator - The activating entity if this is called from an input
//				or Use handler, NULL otherwise.
//-----------------------------------------------------------------------------
CBaseEntity *CTankTrainAI::FindTarget( string_t target, CBaseEntity *pActivator )
{
	return gEntList.FindEntityGeneric( nullptr, STRING( target ), this, pActivator );
}

CTankTrainAI::~CTankTrainAI( void )
{
	CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();

	if ( m_soundTreads )
	{
		controller.SoundDestroy( m_soundTreads );
	}
	
	if ( m_soundEngine )
	{
		controller.SoundDestroy( m_soundEngine );
	}
}

void CTankTrainAI::Precache_Impl( void )
{
	PrecacheScriptSound( STRING( m_startSoundName ) );
	PrecacheScriptSound( STRING( m_engineSoundName ) );
	PrecacheScriptSound( STRING( m_movementSoundName ) );
}

int CTankTrainAI::SoundEnginePitch( void )
{
	CFuncTrackTrain *pTrain = m_hTrain;
	
	// we know this isn't NULL here
	if ( pTrain->GetMaxSpeed() )
	{
		return 90 + (fabs(pTrain->GetCurrentSpeed()) * (20) / pTrain->GetMaxSpeed());
	}
	return 100;
}

void CTankTrainAI::SoundEngineStart( void )
{
	CFuncTrackTrain *pTrain = m_hTrain;

	SoundEngineStop();
	// play startup sound for train
	if ( m_startSoundName != NULL_STRING )
	{
		CPASAttenuationFilter filter( pTrain );

		EmitSound_t ep;
		ep.m_nChannel = CHAN_ITEM;
		ep.m_pSoundName = STRING(m_startSoundName);
		ep.m_flVolume = 1.0f;
		ep.m_SoundLevel = SNDLVL_NORM;

		EmitSound( filter, pTrain->entindex(), ep );
	}

	// play the looping sounds using the envelope controller
	CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();

	if ( m_soundTreads )
	{
		controller.Play( m_soundTreads, 1.0, 100 );
	}
	
	if ( m_soundEngine )
	{
		controller.Play( m_soundEngine, 0.5, 90 );
		controller.CommandClear( m_soundEngine );
		controller.CommandAdd( m_soundEngine, 0, SOUNDCTRL_CHANGE_PITCH, 1.5, random_valve->RandomInt(130, 145) );
		controller.CommandAdd( m_soundEngine, 1.5, SOUNDCTRL_CHANGE_PITCH, 2, random_valve->RandomInt(105, 115) );
	}
	
	m_soundPlaying = true;
}

void CTankTrainAI::SoundEngineStop( void )
{
	if ( !m_soundPlaying )
		return;

	CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
	
	if ( m_soundTreads )
	{
		controller.SoundFadeOut( m_soundTreads, 0.25 );
	}

	if ( m_soundEngine )
	{
		controller.CommandClear( m_soundEngine );
		controller.SoundChangePitch( m_soundEngine, 70, 3.0 );
	}
	m_soundPlaying = false;	
}

void CTankTrainAI::SoundShutdown( void )
{
	CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
	if ( m_soundTreads )
	{
		controller.Shutdown( m_soundTreads );
	}

	if ( m_soundEngine )
	{
		controller.Shutdown( m_soundEngine );
	}
	m_soundPlaying = false;	
}

//-----------------------------------------------------------------------------
// Purpose: Set up think and AI
//-----------------------------------------------------------------------------
void CTankTrainAI::Spawn_Impl( void )
{
	Precache();
	m_soundPlaying = false;
	m_hTargetEntity = nullptr;
}

void CTankTrainAI::Activate_Impl( void )
{
	BaseClass::Activate_Impl();
	
	CBaseEntity *pTarget = nullptr;

	CFuncTrackTrain *pTrain = nullptr;

	if ( m_target != NULL_STRING )
	{
		do
		{
			pTarget = gEntList.FindEntityByName( pTarget, m_target );
			pTrain = dynamic_cast<CFuncTrackTrain *>(pTarget);
		} while (!pTrain && pTarget);
	}

	m_hTrain = pTrain;

	if ( pTrain )
	{
		SetNextThink( gpGlobals->curtime + 0.5f );
		CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();

		if ( m_movementSoundName != NULL_STRING )
		{
			CPASAttenuationFilter filter( this, ATTN_NORM * 0.5 );
			m_soundTreads = controller.SoundCreate( filter, pTrain->entindex(), CHAN_STATIC, STRING(m_movementSoundName), ATTN_NORM*0.5 );
		}
		if ( m_engineSoundName != NULL_STRING )
		{
			CPASAttenuationFilter filter( this );
			m_soundEngine = controller.SoundCreate( filter, pTrain->entindex(), CHAN_STATIC, STRING(m_engineSoundName), ATTN_NORM );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Dumb linear serach of the path
// Input  : *pStart - starting path node
//			&startPosition - starting position
//			&destination - position to move close to
// Output : int move direction 1 = forward, -1 = reverse, 0 = stop
//-----------------------------------------------------------------------------
int PathFindDirection( CPathTrack *pStart, const Vector &startPosition, const Vector &destination )
{
	if ( !pStart )
		return 0;		// no path, don't move

	CPathTrack *pPath = pStart->m_pnext;
	CPathTrack *pNearest = pStart;

	float nearestDist = (pNearest->GetLocalOrigin() - destination).LengthSqr();
	float length = 0;
	float nearestForward = 0, nearestReverse = 0;

	do
	{
		float dist = (pPath->GetLocalOrigin() - destination).LengthSqr();
		
		// This is closer than our current estimate
		if ( dist < nearestDist )
		{
			nearestDist = dist;
			pNearest = pPath;
			nearestForward = length;	// current path length forward
			nearestReverse = 0;			// count until we hit the start again
		}
		CPathTrack *pNext = pPath->m_pnext;
		if ( pNext )
		{
			// UNDONE: Cache delta in path?
			float delta = (pNext->GetLocalOrigin() - pPath->GetLocalOrigin()).LengthSqr();
			length += delta;
			// add to current reverse estimate
			nearestReverse += delta;
			pPath = pNext;
		}
		else
		{
			// not a looping path
			// traverse back to other end of the path
			int fail = 0;
			while ( pPath->m_pprevious )
			{
				fail++;
				// HACKHACK: Don't infinite loop
				if ( fail > 256 )
					break;
				pPath = pPath->m_pprevious;
			}
			// don't take the reverse path to old node
			nearestReverse = nearestForward + 1;
			// dont' take forward path to new node (if we find one)
			length = (float)COORD_EXTENT * (float)COORD_EXTENT; // HACKHACK: Max quad length
		}

	} while ( pPath != pStart );

	// UNDONE: Fix this fudge factor
	// if you are already at the path, or <100 units away, don't move
	if ( pNearest == pStart || (pNearest->GetLocalOrigin() - startPosition).LengthSqr() < 100 )
		return 0;

	if ( nearestForward <= nearestReverse )
		return 1;

	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: Find a point on my path near to the target and move toward it
//-----------------------------------------------------------------------------
void CTankTrainAI::Think_Impl( void )
{
	CFuncTrackTrain *pTrain = m_hTrain;

	if ( !pTrain || pTrain->m_lifeState != LIFE_ALIVE )
	{
		SoundShutdown();
		if ( pTrain )
			UTIL_RemoveHierarchy( pTrain );
		UTIL_Remove( this );
		return;
	}

	int desired = 0;
	CBaseEntity *pTarget = m_hTargetEntity;
	if ( pTarget )
	{
		desired = PathFindDirection( pTrain->m_ppath, pTrain->GetLocalOrigin(), pTarget->GetLocalOrigin() );
	}

	// If the train wants to stop, figure out throttle
	// otherwise, just throttle in the indicated direction and let the train logic
	// clip the speed
	if ( !desired )
	{
		if ( pTrain->m_flSpeed > 0 )
		{
			desired = -1;
		}
		else if ( pTrain->m_flSpeed < 0 )
		{
			desired = 1;
		}
	}
	
	// UNDONE: Align the think time with arrival, and bump this up to a few seconds
	SetNextThink( gpGlobals->curtime + 0.5f );

	if ( desired != 0 )
	{
		int wasMoving = (pTrain->m_flSpeed == 0) ? false : true;
		// chaser wants train to move, send message
		pTrain->SetSpeed( desired );
		int isMoving = (pTrain->m_flSpeed == 0) ? false : true;

		if ( !isMoving && wasMoving )
		{
			SoundEngineStop();
		}
		else if ( isMoving )
		{
			if ( !wasMoving )
			{
				SoundEngineStart();
			}
		}
	}
	else
	{
		SoundEngineStop();
		// UNDONE: Align the think time with arrival, and bump this up to a few seconds
		SetNextThink( gpGlobals->curtime + 1.0f );
	}
}

KS_ENTITY_ABSTRACT_KIND( CBaseToggle )
