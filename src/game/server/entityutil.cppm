// SPDX-License-Identifier: None
// Copyright 2007 Turtle Rock Studios, Inc.
// Copyright (c) 2007 Turtle Rock Studios, Inc. - All Rights Reserved

module;
#include "edict.h"
#include "networkvar.h"
#include "reflect_annotations.h"
#include "server_class.h"
#include "string_t.h"
#include "declare_class.h"
module ks.server:entityutil;

import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.ssemath;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.reflect_wirevar;
import :baseentity;
import :player;
import :sharedvar;
import ks.core.basetypes;
import ks.core.types;
import ks.game.shareddefs;
import ks.core.platform;
import :vscript;
import std;
import :fish;
import ks.tier1.keyvalues;

class CBaseEntity;
class CBasePlayer;

using namespace ::ks::reflect;

/**
 * EntityUtil.h
 * Various utility functions
 */

//------------------------------------------------------------------------------------------
enum WhoType
{
	ANYONE,
	ONLY_FRIENDS,
	ONLY_ENEMIES
};

//--------------------------------------------------------------------------------------------------------------
/**
 * Iterate over all entities in the game, invoking functor on each.
 * If functor returns false, stop iteration and return false.
 */
template < typename Functor >
bool ForEachEntity( Functor &func )
{
	CBaseEntity *entity = gEntList.FirstEnt();
	while ( entity )
	{
		if ( func( entity ) == false )
			return false;

		entity = gEntList.NextEnt( entity );
	}

	return true;
}

//--------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------
class FindInViewConeFunctor
{
public:
	FindInViewConeFunctor( CBasePlayer *me, WhoType who, float tolerance, CBaseEntity *ignore = nullptr )
	{
		m_me = me;
		m_who = who;
		m_tolerance = tolerance;
		m_target = nullptr;
		m_range = 9999999.9f;
		m_ignore = ignore;

		me->EyeVectors(&m_dir, nullptr, nullptr);
		m_origin = me->GetAbsOrigin() + me->GetViewOffset();
	}

	bool operator() ( CBasePlayer *player )
	{
		if (player != m_me && player != m_ignore && player->IsAlive())
		{
			if (m_who == ONLY_FRIENDS && m_me->GetTeamNumber() != player->GetTeamNumber() )
				return true;

			if (m_who == ONLY_ENEMIES && m_me->GetTeamNumber() == player->GetTeamNumber() )
				return true;

			Vector to = player->WorldSpaceCenter() - m_origin;
			float range = to.NormalizeInPlace();

			if (DotProduct( to, m_dir ) > m_tolerance && range < m_range)
			{
				if ( m_me->IsLineOfSightClear( player ) )
				{
					m_target = player;
					m_range = range;			
				}
			}
		}

		return true;
	}

	CBasePlayer *m_me;
	WhoType m_who;
	float m_tolerance;
	CBaseEntity *m_ignore;

	Vector m_origin;
	Vector m_dir;

	CBasePlayer *m_target;
	float m_range;
};

/**
 * Find the closest player within the given view cone
 */
inline CBasePlayer *GetClosestPlayerInViewCone( CBasePlayer *me, WhoType who, float tolerance = 0.95f, float *range = nullptr, CBaseEntity *ignore = nullptr )
{
	// choke the victim we are pointing at
	FindInViewConeFunctor checkCone( me, who, tolerance, ignore );

	ForEachPlayer( checkCone );

	if (range)
		*range = checkCone.m_range;

	return checkCone.m_target;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Find player closest to ray.
 * Return perpendicular distance to ray in 'offset' if it is non-NULL.
 */
extern CBasePlayer *GetPlayerClosestToRay( CBasePlayer *me, WhoType who, const Vector &start, const Vector &end, float *offset = nullptr );

//--------------------------------------------------------------------------------------------------------------
/**
 * Compute the closest point on the ray to 'pos' and return it in 'pointOnRay'.
 * If point projects beyond the ends of the ray, return false - but clamp 'pointOnRay' to the correct endpoint.
 */
inline bool ClosestPointOnRay( const Vector &pos, const Vector &rayStart, const Vector &rayEnd, Vector *pointOnRay )
{
	Vector to = pos - rayStart;
	Vector dir = rayEnd - rayStart;
	float length = dir.NormalizeInPlace();

	float rangeAlong = DotProduct( dir, to );

	if (rangeAlong < 0.0f)
	{
		// off start point
		*pointOnRay = rayStart;
		return false;
	}
	else if (rangeAlong > length)
	{
		// off end point
		*pointOnRay = rayEnd;
		return false;
	}
	else // within ray bounds
	{
		Vector onRay = rayStart + rangeAlong * dir;
		*pointOnRay = onRay;
		return true;
	}
}

//------------------------------------------------------------------------------------------
/**
 * Functor to find the player closest to a ray.
 * For use with ForEachPlayerNearRay().
 */
class ClosestToRayFunctor
{
public:
	ClosestToRayFunctor( CBasePlayer *ignore, WhoType who = ANYONE )
	{
		m_ignore = ignore;
		m_who = who;
		m_closestPlayer = nullptr;
		m_closestPlayerRange = 999999999.9f;
	}

	bool operator() ( CBasePlayer *player, float rangeToRay )
	{
		if (player == m_ignore)
			return true;

		if (!player->IsAlive())
			return true;

		if (m_who == ONLY_FRIENDS && m_ignore->GetTeamNumber() != player->GetTeamNumber() )
			return true;

		if (m_who == ONLY_ENEMIES && m_ignore->GetTeamNumber() == player->GetTeamNumber() )
			return true;

		// keep the player closest to the ray
		if (rangeToRay < m_closestPlayerRange)
		{
			m_closestPlayerRange = rangeToRay;
			m_closestPlayer = player;
		}	

		return true;
	}

	CBasePlayer *m_ignore;
	WhoType m_who;
	CBasePlayer *m_closestPlayer;
	float m_closestPlayerRange;
};

//--------------------------------------------------------------------------------------------------------------
/**
 * Iterate over all players that are within range of the ray and invoke functor.
 * If functor returns false, stop iteration and return false.
 * @todo Check LOS to ray.
 */
template < typename Functor >
bool ForEachPlayerNearRay( Functor &func, const Vector &start, const Vector &end, float maxRadius )
{
    for( int i=1; i<=gpGlobals->maxClients; ++i )
    {
        CBasePlayer *player = static_cast<CBasePlayer *>( UTIL_PlayerByIndex( i ) );

        if (player == nullptr)
			continue;

        if (FNullEnt( player->edict() ))
			continue;

        if (!player->IsPlayer())
			continue;

        if (!player->IsAlive())
			continue;

		float range = DistanceToRay( player->WorldSpaceCenter(), start, end );

		if (range < 0.0f || range > maxRadius)
			continue;

        if (func( player, range ) == false)
			return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Iterate over all players that are within range of the sphere and invoke functor.
 * If functor returns false, stop iteration and return false.
 * @todo Check LOS to ray.
 */
template < typename Functor >
bool ForEachPlayerNearSphere( Functor &func, const Vector &origin, float radius )
{
    for( int i=1; i<=gpGlobals->maxClients; ++i )
    {
        CBasePlayer *player = static_cast<CBasePlayer *>( UTIL_PlayerByIndex( i ) );

        if (player == nullptr)
			continue;

        if (FNullEnt( player->edict() ))
			continue;

        if (!player->IsPlayer())
			continue;

        if (!player->IsAlive())
			continue;

		Vector to = player->WorldSpaceCenter() - origin;
		float range = to.Length();
		if (range > radius)
			continue;

        if (func( player, range ) == false)
			return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Reflect a vector.
 * Assumes 'unitNormal' is normalized.
 */
inline Vector VectorReflect( const Vector &in, const Vector &unitNormal )
{
	// compute the unit vector out of the plane of 'in' and 'unitNormal'
	Vector lat = CrossProduct( in, unitNormal );
	Vector forward = CrossProduct( unitNormal, lat );
	forward.NormalizeInPlace();

	Vector forwardComponent = forward * DotProduct( in, forward );	
	Vector normalComponent = unitNormal * DotProduct( in, unitNormal );

	return forwardComponent - normalComponent;
}

struct inputdata_t;

using namespace ::ks::reflect;


//------------------------------------------------------------------------------
// Purpose : Ambient light controller entity
//------------------------------------------------------------------------------
class CEnvAmbientLight : public CSpatialEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_EnvAmbientLight" } ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CEnvAmbientLight, CSpatialEntity );

public:
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	void Spawn_Impl( void );

	[[= ks::reflect::Input{ .name = "SetColor", .type = FIELD_COLOR32 } ]] void InputSetColor(inputdata_t &inputdata);
	void SetColor( const Vector &vecColor );

private:
	[[= ks::reflect::Key{ .name = "Color" } ]] color32	m_Color;

	CNetworkVector( m_vecColor, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE, .enc = ks::reflect::WireEnc::Vector } ]] );
};

class CBaseEntity;
struct inputdata_t;

using namespace ::ks::reflect;

//------------------------------------------------------------------------------
// Purpose : Sunlight shadow control entity
//------------------------------------------------------------------------------
class CCascadeLight : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_CascadeLight", .base = false } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CCascadeLight, CBaseEntity );

	CCascadeLight();
	virtual ~CCascadeLight();

	void Spawn_Impl( void );
	void Release( void );
	void OnActivate();
	void OnDeactivate();

	using BaseClass::KeyValue_Impl;
	bool KeyValue_Impl( const char *szKeyName, const char *szValue );
	bool GetKeyValue_Impl( const char *szKeyName, char *szValue, int iMaxLen );
	int  UpdateTransmitState_Impl();

	inline const Vector &GetShadowDirection() const { return m_shadowDirection; }
	inline const Vector &GetEnvLightShadowDirection() const { return m_envLightShadowDirection; }
	// Inputs
	[[= ks::reflect::Input{ .name = "SetAngles", .type = FIELD_STRING } ]] void	InputSetAngles( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void	InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void	InputDisable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "LightColor", .type = FIELD_COLOR32 } ]] void	InputSetLightColor( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "LightColorScale", .type = FIELD_INTEGER } ]] void	InputSetLightColorScale( inputdata_t &inputdata );

	int	ObjectCaps_Impl( void ) { return BaseClass::ObjectCaps_Impl() & ~FCAP_ACROSS_TRANSITION; }

	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	static void SetEnvLightShadowPitch( float flPitch );
	static void SetEnvLightShadowAngles( const QAngle &angles );
	static void SetLightColor( int r, int g, int b, int a );
	void SetEnabled( bool bEnable );

private:
	CNetworkVector( m_shadowDirection, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVector( m_envLightShadowDirection, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE, .enc = ks::reflect::WireEnc::Vector } ]] );

	CNetworkVar( bool, m_bEnabled, [[= ks::reflect::Net{} ]] [[= ks::reflect::Key{ .name = "enabled" } ]] );
	[[= ks::reflect::Key{ .name = "StartDisabled" } ]] bool m_bStartDisabled;
	CNetworkVar( bool, m_bUseLightEnvAngles, [[= ks::reflect::Net{} ]] );

	CNetworkColor32( m_LightColor, [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Proxy<SendProxy_Color32ToInt32, ks::reflect::WireSide::Send>{} ]] );
	CNetworkVar( int, m_LightColorScale, [[= ks::reflect::Net{ .bits = 32 } ]] [[= ks::reflect::Proxy<SendProxy_Int32ToInt32, ks::reflect::WireSide::Send>{} ]] );
	CNetworkVar( float, m_flMaxShadowDist, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );

	void UpdateEnvLight();

	static float m_flEnvLightShadowPitch;
	static QAngle m_EnvLightShadowAngles;
	static bool m_bEnvLightShadowValid;
	static color32 m_EnvLightColor;
	static int m_EnvLightColorScale;
};

extern CCascadeLight *g_pCascadeLight;

struct inputdata_t;

using namespace ::ks::reflect;

struct DOFControlSettings_t
{
	// Near plane
	float	flNearBlurDepth;
	float	flNearBlurRadius;
	float	flNearFocusDistance;
	// Far plane
	float	flFarBlurDepth;
	float	flFarBlurRadius;
	float	flFarFocusDistance;
};

//-----------------------------------------------------------------------------
// Purpose: Entity that controls depth of field postprocessing
//-----------------------------------------------------------------------------
class CEnvDOFController : public CPointEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_EnvDOFController" } ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CEnvDOFController, CPointEntity );
public:
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	void	Spawn_Impl( void );
	void	Activate_Impl( void );
	int		UpdateTransmitState_Impl( void );
	void			SetControllerState( DOFControlSettings_t setting );

	void	UpdateParamBlend( void );

	// Inputs
	[[= ks::reflect::Input{ .name = "SetNearBlurDepth", .type = FIELD_FLOAT } ]] void	InputSetNearBlurDepth( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetNearFocusDepth", .type = FIELD_FLOAT } ]] void	InputSetNearFocusDepth( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetFarFocusDepth", .type = FIELD_FLOAT } ]] void	InputSetFarFocusDepth( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetFarBlurDepth", .type = FIELD_FLOAT } ]] void	InputSetFarBlurDepth( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetNearBlurRadius", .type = FIELD_FLOAT } ]] void	InputSetNearBlurRadius( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetFarBlurRadius", .type = FIELD_FLOAT } ]] void	InputSetFarBlurRadius( inputdata_t &inputdata );
	void	InputBlendDOFScale( inputdata_t &inputdata );
	
	[[= ks::reflect::Input{ .name = "SetFocusTarget", .type = FIELD_STRING } ]] void	InputSetFocusTarget( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetFocusTargetRange", .type = FIELD_STRING } ]] void	InputSetFocusTargetRange( inputdata_t &inputdata );

private:
	[[= ks::reflect::Key{ .name = "focus_range" } ]] float	m_flFocusTargetRange;

	[[= ks::reflect::Key{ .name = "focus_target" } ]] string_t	m_strFocusTargetName;	// Name of the entity to focus on
	EHANDLE		m_hFocusTarget;

	CNetworkVar( bool, m_bDOFEnabled, [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "enabled" } ]] );
	CNetworkVar( float, m_flNearBlurDepth, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "near_blur" } ]] );
	CNetworkVar( float, m_flNearFocusDepth, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "near_focus" } ]] );
	CNetworkVar( float, m_flFarFocusDepth, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "far_focus" } ]] );
	CNetworkVar( float, m_flFarBlurDepth, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "far_blur" } ]] );
	CNetworkVar( float, m_flNearBlurRadius, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "near_radius" } ]] );
	CNetworkVar( float, m_flFarBlurRadius, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "far_radius" } ]] );
};

class CBaseEntity;
struct inputdata_t;

using namespace ::ks::reflect;

inline constexpr auto ENV_PROJECTEDTEXTURE_STARTON = (1<<0);
inline constexpr auto ENV_PROJECTEDTEXTURE_ALWAYSUPDATE = (1<<1);

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CEnvProjectedTexture : public CPointEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_EnvProjectedTexture" } ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CEnvProjectedTexture, CPointEntity );
public:
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	CEnvProjectedTexture();
	using BaseClass::KeyValue_Impl;
	bool KeyValue_Impl( const char *szKeyName, const char *szValue );
	bool GetKeyValue_Impl( const char *szKeyName, char *szValue, int iMaxLen );

	// Always transmit to clients
	int UpdateTransmitState_Impl();
	void Activate_Impl( void );
	void Spawn_Impl( void );

	[[= ks::reflect::Input{ .name = "TurnOn", .type = FIELD_VOID } ]] void InputTurnOn( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "TurnOff", .type = FIELD_VOID } ]] void InputTurnOff( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "AlwaysUpdateOn", .type = FIELD_VOID } ]] void InputAlwaysUpdateOn( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "AlwaysUpdateOff", .type = FIELD_VOID } ]] void InputAlwaysUpdateOff( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "FOV", .type = FIELD_FLOAT } ]] void InputSetFOV( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Target", .type = FIELD_EHANDLE } ]] void InputSetTarget( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "CameraSpace", .type = FIELD_BOOLEAN } ]] void InputSetCameraSpace( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "LightOnlyTarget", .type = FIELD_BOOLEAN } ]] void InputSetLightOnlyTarget( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "LightWorld", .type = FIELD_BOOLEAN } ]] void InputSetLightWorld( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "EnableShadows", .type = FIELD_BOOLEAN } ]] void InputSetEnableShadows( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "LightColor", .type = FIELD_COLOR32 } ]] void InputSetLightColor( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SpotlightTexture", .type = FIELD_STRING } ]] void InputSetSpotlightTexture( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Ambient", .type = FIELD_FLOAT } ]] void InputSetAmbient( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetLightStyle", .type = FIELD_INTEGER } ]] void InputSetLightStyle( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetPattern", .type = FIELD_STRING } ]] void InputSetPattern( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetNearZ", .type = FIELD_FLOAT } ]] void InputSetNearZ( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetFarZ", .type = FIELD_FLOAT } ]] void InputSetFarZ( inputdata_t &inputdata );

	void InitialThink( void );

	CNetworkHandle( CBaseEntity, m_hTargetEntity, [[= ks::reflect::Net{} ]] );

private:

	void EnforceSingleProjectionRules( bool bWarnOnEnforcement = false );

	CNetworkVar( bool, m_bState, [[= ks::reflect::Net{} ]] );
	CNetworkVar( bool, m_bAlwaysUpdate, [[= ks::reflect::Net{} ]] );
	CNetworkVar( float, m_flLightFOV, [[= ks::reflect::Net{ .bits = 32 } ]] [[= ks::reflect::Key{ .name = "lightfov" } ]] );
	CNetworkVar( bool, m_bEnableShadows, [[= ks::reflect::Net{} ]] [[= ks::reflect::Key{ .name = "enableshadows" } ]] );
	CNetworkVar( bool, m_bSimpleProjection, [[= ks::reflect::Net{} ]] [[= ks::reflect::Key{ .name = "simpleprojection" } ]] );
	CNetworkVar( bool, m_bLightOnlyTarget, [[= ks::reflect::Net{} ]] [[= ks::reflect::Key{ .name = "lightonlytarget" } ]] );
	CNetworkVar( bool, m_bLightWorld, [[= ks::reflect::Net{} ]] [[= ks::reflect::Key{ .name = "lightworld" } ]] );
	CNetworkVar( bool, m_bCameraSpace, [[= ks::reflect::Net{} ]] [[= ks::reflect::Key{ .name = "cameraspace" } ]] );
	CNetworkVar( float, m_flBrightnessScale, [[= ks::reflect::Net{ .bits = 32 } ]] [[= ks::reflect::Key{ .name = "brightnessscale" } ]] );
	CNetworkColor32( m_LightColor, [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Proxy<SendProxy_Color32ToInt32, ks::reflect::WireSide::Send>{} ]] );
	CNetworkVar( float, m_flColorTransitionTime, [[= ks::reflect::Net{ .bits = 32 } ]] [[= ks::reflect::Key{ .name = "colortransitiontime" } ]] );
	CNetworkVar( float, m_flAmbient, [[= ks::reflect::Net{ .bits = 32 } ]] [[= ks::reflect::Key{ .name = "ambient" } ]] );
	CNetworkString( m_SpotlightTextureName, MAX_PATH, [[= ks::reflect::Net{} ]] );
	CNetworkVar( int, m_nSpotlightTextureFrame, [[= ks::reflect::Net{ .bits = -1 } ]] [[= ks::reflect::Key{ .name = "textureframe" } ]] );
	CNetworkVar( float, m_flNearZ, [[= ks::reflect::Net{ .bits = 16, .low = 0.0f, .high = 500.0f, .flags = SPROP_ROUNDDOWN } ]] [[= ks::reflect::Key{ .name = "nearz" } ]] );
	CNetworkVar( float, m_flFarZ, [[= ks::reflect::Net{ .bits = 18, .low = 0.0f, .high = 2500.0f, .flags = SPROP_ROUNDDOWN } ]] [[= ks::reflect::Key{ .name = "farz" } ]] );
	CNetworkVar( int, m_nShadowQuality, [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "shadowquality" } ]] );
	CNetworkVar( float, m_flProjectionSize, [[= ks::reflect::Net{ .bits = 32 } ]] [[= ks::reflect::Key{ .name = "projection_size" } ]] );
	CNetworkVar( float, m_flRotation, [[= ks::reflect::Net{ .bits = 32 } ]] [[= ks::reflect::Key{ .name = "projection_rotation" } ]] );
	
	// Light style
	CNetworkVar( int, m_iStyle, [[= ks::reflect::Net{ .bits = -1 } ]] [[= ks::reflect::Key{ .name = "style" } ]] );
	[[= ks::reflect::Key{ .name = "defaultstyle" } ]] int			m_iDefaultStyle;
	[[= ks::reflect::Key{ .name = "pattern" } ]] string_t	m_iszPattern;

};

class CBaseEntity;
class CBaseTrigger;

using namespace ::ks::reflect;

//--------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------
class CTonemapTrigger : public CBaseTrigger
{
public:
	DECLARE_CLASS( CTonemapTrigger, CBaseTrigger );
	DECLARE_DATADESC();

	void Spawn_Impl( void );
	void StartTouch_Impl( CBaseEntity *other );
	void EndTouch_Impl( CBaseEntity *other );

	CBaseEntity *GetTonemapController( void ) const;

private:
	[[= ks::reflect::Key{ .name = "TonemapName" } ]] string_t m_tonemapControllerName;
	EHANDLE m_hTonemapController;
};

//--------------------------------------------------------------------------------------------------------
// Tonemap Controller System.
class CTonemapSystem : public CAutoGameSystem
{
public:

	// Creation/Init.
	CTonemapSystem( char const *name ) : CAutoGameSystem( name ) 
	{
		m_hMasterController = nullptr;
	}

	~CTonemapSystem()
	{
		m_hMasterController = nullptr;
	}

	virtual void LevelInitPreEntity();
	virtual void LevelInitPostEntity();
	CBaseEntity *GetMasterTonemapController( void ) const;

private:

	EHANDLE m_hMasterController;
};

//--------------------------------------------------------------------------------------------------------
CTonemapSystem *TheTonemapSystem( void );

class CColorCorrection;
class CFogController;
class CPostProcessController;
struct inputdata_t;

using namespace ::ks::reflect;

//--------------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------------
// Fog volume entity
class CFogVolume : public CServerOnlyEntity
{
	DECLARE_CLASS( CFogVolume, CServerOnlyEntity );
	DECLARE_DATADESC();

public:
	CFogVolume();
	virtual ~CFogVolume();
	void Spawn_Impl( void );
	void Activate_Impl();

	static CFogVolume *FindFogVolumeForPosition( const Vector &position );

	const char *GetFogControllerName() const 
	{
		return STRING( m_fogName );
	}

	CFogController* GetFogController( ) const
	{
		return m_hFogController.Get();
	}

	CPostProcessController* GetPostProcessController( ) const
	{
		return m_hPostProcessController.Get();
	}

	CColorCorrection* GetColorCorrectionController( ) const
	{
		return m_hColorCorrectionController.Get();
	}

	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void InputEnable( inputdata_t &data );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void InputDisable( inputdata_t &data );

private:
	[[= ks::reflect::Key{ .name = "FogName" } ]] string_t m_fogName;
	[[= ks::reflect::Key{ .name = "PostProcessName" } ]] string_t m_postProcessName;
	[[= ks::reflect::Key{ .name = "ColorCorrectionName" } ]] string_t m_colorCorrectionName;

	CHandle< CFogController > m_hFogController;
	CHandle< CPostProcessController > m_hPostProcessController;
	CHandle< CColorCorrection > m_hColorCorrectionController;

	[[= ks::reflect::Key{ .name = "StartDisabled" } ]] bool m_bDisabled;
	bool m_bInFogVolumesList;

	void AddToGlobalList();
	void RemoveFromGlobalList();
};

extern std::vector< CFogVolume * > TheFogVolumes;

using namespace ::ks::reflect;

// func_elevator.h

//--------------------------------------------------------------------------------------------------------
class CInfoElevatorFloor;   // declared only: it was never implemented

//--------------------------------------------------------------------------------------------------------
struct FloorInfo
{
	float height;		// Height of this floor
	string_t name;		// Name of this floor
	EHANDLE button;		// Button that moves the elevator to this floor
};

//--------------------------------------------------------------------------------------------------------
class CFuncElevator;   // declared only: it was never implemented

//--------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------

class CBaseCombatCharacter;
class CBaseEntity;
class CBasePlayer;

using namespace ::ks::reflect;

// FunctorUtils.h
// Useful functors
// Author: Michael Booth, August 2005, Copyright 2005 Turtle Rock Studios, Inc.

class INextBot;

//--------------------------------------------------------------------------------------------------------
/**
 * NOTE: The functors in this file should ideally be game-independent, 
 * and work for any Source based game
 */
//--------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------
/**
 * Count the number of living players on a given team (or TEAM_ANY)
 */
class LivePlayerCounter
{
public:
	static const bool EXCLUDE_BOTS = false;
	LivePlayerCounter( int team, bool includeBots = true )
	{
		m_team = team;
		m_includeBots = includeBots;
		m_count = 0;
	}

	bool operator() ( CBasePlayer *player )
	{
		if (player->IsAlive() && (m_team == TEAM_ANY || player->GetTeamNumber() == m_team))
		{
			if (m_includeBots || !player->IsBot())
			{
				++m_count;
			}
		}
		return true;
	}

	int GetCount( void ) const
	{
		return m_count;
	}

	int m_team;
	bool m_includeBots;
	int m_count;
};

//--------------------------------------------------------------------------------------------------------
/**
* Count the number of dead players on a given team (or TEAM_ANY)
*/
class DeadPlayerCounter
{
public:
	static const bool EXCLUDE_BOTS = false;
	DeadPlayerCounter( int team, bool includeBots = true )
	{
		m_team = team;
		m_includeBots = includeBots;
		m_count = 0;
	}

	bool operator() ( CBasePlayer *player )
	{
		if (!player->IsAlive() && (m_team == TEAM_ANY || player->GetTeamNumber() == m_team))
		{
			if (m_includeBots || !player->IsBot())
			{
				++m_count;
			}
		}
		return true;
	}

	int GetCount( void ) const
	{
		return m_count;
	}

	int m_team;
	bool m_includeBots;
	int m_count;
};

//--------------------------------------------------------------------------------------------------------
/**
* Count the number of players on a given team (or TEAM_ANY)
*/
class PlayerCounter
{
public:
	static const bool EXCLUDE_BOTS = false;
	PlayerCounter( int team, int lifeState = -1, bool includeBots = true )
	{
		m_team = team;
		m_includeBots = includeBots;
		m_count = 0;
		m_lifeState = lifeState;
	}

	bool operator() ( CBasePlayer *player )
	{
		if ((player->m_lifeState == m_lifeState || m_lifeState == -1) && (m_team == TEAM_ANY || player->GetTeamNumber() == m_team))
		{
			if (m_includeBots || !player->IsBot())
			{
				++m_count;
			}
		}
		return true;
	}

	int GetCount( void ) const
	{
		return m_count;
	}

	int m_lifeState;
	int m_team;
	bool m_includeBots;
	int m_count;
};

//--------------------------------------------------------------------------------------------------------
/**
 * Return the closest living player on the given team (or TEAM_ANY)
 */
class ClosestPlayerScan
{
public:
	static const bool EXCLUDE_BOTS = false;
	ClosestPlayerScan( const Vector &spot, int team, float maxRange = 0.0f, CBasePlayer *ignore = nullptr, bool includeBots = true )
	{
		m_spot = spot;
		m_team = team;
		m_includeBots = includeBots;
		m_close = nullptr;
		
		if ( maxRange > 0.0f )
		{
			m_closeRangeSq = maxRange * maxRange;
		}
		else
		{
			m_closeRangeSq = 999999999.9f;
		}
		
		m_ignore = ignore;
	}
	
	bool operator() ( CBasePlayer *player )
	{
		if (player == m_ignore)
			return true;
			
		if (player->IsAlive() && (m_team == TEAM_ANY || player->GetTeamNumber() == m_team))
		{
			if ( !m_includeBots && player->IsBot() )
				return true;

			Vector to = player->WorldSpaceCenter() - m_spot;
			float rangeSq = to.LengthSqr();
			if (rangeSq < m_closeRangeSq)
			{
				m_closeRangeSq = rangeSq;
				m_close = player;
			}
		}
		return true;
	}
	
	CBasePlayer *GetPlayer( void ) const
	{
		return m_close;
	}
	
	bool IsCloserThan( float range )
	{
		return (m_closeRangeSq < (range * range));
	}

	bool IsFartherThan( float range )
	{
		return (m_closeRangeSq > (range * range));
	}
	
	Vector m_spot;
	int m_team;
	bool m_includeBots;
	CBasePlayer *m_close;
	float m_closeRangeSq;
	CBasePlayer *m_ignore;
};

//--------------------------------------------------------------------------------------------------------
/**
* Return the closest living BaseCombatCharacter on the given team (or TEAM_ANY)
*/
class ClosestActorScan
{
public:
	ClosestActorScan( const Vector &spot, int team, float maxRange = 0.0f, CBaseCombatCharacter *ignore = nullptr )
	{
		m_spot = spot;
		m_team = team;
		m_close = nullptr;

		if ( maxRange > 0.0f )
		{
			m_closeRangeSq = maxRange * maxRange;
		}
		else
		{
			m_closeRangeSq = 999999999.9f;
		}

		m_ignore = ignore;
	}

	bool operator() ( CBaseCombatCharacter *actor )
	{
		if (actor == m_ignore)
			return true;

		if (actor->IsAlive() && (m_team == TEAM_ANY || actor->GetTeamNumber() == m_team))
		{
			Vector to = actor->WorldSpaceCenter() - m_spot;
			float rangeSq = to.LengthSqr();
			if (rangeSq < m_closeRangeSq)
			{
				m_closeRangeSq = rangeSq;
				m_close = actor;
			}
		}
		return true;
	}

	CBaseCombatCharacter *GetClosestActor( void ) const
	{
		return m_close;
	}

	bool IsClosestActorCloserThan( float range )
	{
		return (m_closeRangeSq < (range * range));
	}

	bool IsClosestActorFartherThan( float range )
	{
		return (m_closeRangeSq > (range * range));
	}

	Vector m_spot;
	int m_team;
	CBaseCombatCharacter *m_close;
	float m_closeRangeSq;
	CBaseCombatCharacter *m_ignore;
};

//--------------------------------------------------------------------------------------------------------
class CShowViewportPanel
{
	int m_team;
	const char *m_panelName;
	bool m_show;
	KeyValues *m_data;

public:
	CShowViewportPanel( int team, const char *panelName, bool show, KeyValues *data = nullptr )
	{
		m_team = team;
		m_panelName = panelName;
		m_show = show;
		m_data = data;
	}

	bool operator() ( CBasePlayer *player )
	{
		if ( m_team != TEAM_ANY && m_team != player->GetTeamNumber() )
			return true;

		player->ShowViewPortPanel( m_panelName, m_show, m_data );
		return true;
	}
};

//--------------------------------------------------------------------------------------------------------------
/**
 * Iterate each "actor" in the game, where an actor is a Player or NextBot
 */
template < typename Functor >
inline bool ForEachActor( Functor &func )
{
	// iterate all non-bot players
	for( int i=1; i<=gpGlobals->maxClients; ++i )
	{
		CBasePlayer *player = UTIL_PlayerByIndex( i );

		if ( player == nullptr )
			continue;

		if ( FNullEnt( player->edict() ) )
			continue;

		if ( !player->IsPlayer() )
			continue;

		if ( !player->IsConnected() )
			continue;

		if ( func( player ) == false )
		{
			return false;
		}
	}

	return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * The interface for functors for use with ForEachActor() that
 * want notification before iteration starts and after interation
 * is complete (successful or not).
 */
class IActorFunctor
{
public:
	virtual void OnBeginIteration( void )						{ }		// invoked once before iteration begins

	virtual bool operator() ( CBaseCombatCharacter *them ) = 0;

	virtual void OnEndIteration( bool allElementsIterated )		{ }		// invoked once after iteration is complete whether successful or not
};

//--------------------------------------------------------------------------------------------------------------
/**
 * Iterate each "actor" in the game, where an actor is a Player or NextBot
 * Template specialization for IActorFunctors.
 */
template <>
inline bool ForEachActor( IActorFunctor &func )
{
	func.OnBeginIteration();

	bool isComplete = true;

	// iterate all non-bot players
	for( int i=1; i<=gpGlobals->maxClients; ++i )
	{
		CBasePlayer *player = UTIL_PlayerByIndex( i );

		if ( player == nullptr )
			continue;

		if ( FNullEnt( player->edict() ) )
			continue;

		if ( !player->IsPlayer() )
			continue;

		if ( !player->IsConnected() )
			continue;

		if ( func( player ) == false )
		{
			isComplete = false;
			break;
		}
	}

	func.OnEndIteration( isComplete );

	return isComplete;
}

//--------------------------------------------------------------------------------------------------------
class CTraceFilterOnlyClassname : public CTraceFilterSimple
{
public:
	CTraceFilterOnlyClassname( const IHandleEntity *passentity, const char *pchClassname, int collisionGroup ) :
		CTraceFilterSimple( passentity, collisionGroup ), m_pchClassname( pchClassname )
	{
	}

	virtual bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
	{
		CBaseEntity *pEntity = EntityFromEntityHandle( pHandleEntity );
		if ( !pEntity )
			return false;

		return FClassnameIs( pEntity, m_pchClassname ) && CTraceFilterSimple::ShouldHitEntity( pHandleEntity, contentsMask );
	}

private:

	const char *m_pchClassname;
};
