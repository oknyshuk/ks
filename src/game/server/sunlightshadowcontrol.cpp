//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Sunlight shadow control entity.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "reflect_datamap.h"
#include "reflect_sendtable.h"
#include "reflect_annotations.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//------------------------------------------------------------------------------
// FIXME: This really should inherit from something	more lightweight
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
// Purpose : Sunlight shadow control entity
//------------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_SunlightShadowControl", .base = false } ]]
      CSunlightShadowControl : public CBaseEntity
{
public:
	DECLARE_CLASS( CSunlightShadowControl, CBaseEntity );

	CSunlightShadowControl();

	void Spawn( void );
	bool KeyValue( const char *szKeyName, const char *szValue );
	virtual bool GetKeyValue( const char *szKeyName, char *szValue, int iMaxLen );
	int  UpdateTransmitState();

	// Inputs
	[[= ks::reflect::Input{ .name = "SetAngles", .type = FIELD_STRING } ]] void	InputSetAngles( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void	InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void	InputDisable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetTexture", .type = FIELD_STRING } ]] void	InputSetTexture( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "EnableShadows", .type = FIELD_BOOLEAN } ]] void	InputSetEnableShadows( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "LightColor", .type = FIELD_COLOR32 } ]] void	InputSetLightColor( inputdata_t &inputdata );

	virtual int	ObjectCaps( void ) { return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

private:
	CNetworkVector( m_shadowDirection, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE, .enc = ks::reflect::ENC_VECTOR } ]] );

	CNetworkVar( bool, m_bEnabled, [[= ks::reflect::Net{} ]] [[= ks::reflect::Key{ .name = "enabled" } ]] );
	[[= ks::reflect::Key{ .name = "StartDisabled" } ]] bool m_bStartDisabled;

	CNetworkString( m_TextureName, MAX_PATH, [[= ks::reflect::Net{} ]] );
	CNetworkColor32( m_LightColor, [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Proxy<SendProxy_Color32ToInt32, ks::reflect::WIRE_SEND>{} ]] );
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
	Q_strcpy( m_TextureName.GetForModify(), "effects/flashlight001" );
	m_LightColor.Init( 255, 255, 255, 1 );
	m_flColorTransitionTime = 0.5f;
	m_flSunDistance = 10000.0f;
	m_flFOV = 5.0f;
	m_bEnableShadows = false;
}


//------------------------------------------------------------------------------
// Purpose : Send even though we don't have a model
//------------------------------------------------------------------------------
int CSunlightShadowControl::UpdateTransmitState()
{
	// ALWAYS transmit to all clients.
	return SetTransmitState( FL_EDICT_ALWAYS );
}


bool CSunlightShadowControl::KeyValue( const char *szKeyName, const char *szValue )
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
		Q_strcpy( m_TextureName.GetForModify(), szValue );
	}

	return BaseClass::KeyValue( szKeyName, szValue );
}

bool CSunlightShadowControl::GetKeyValue( const char *szKeyName, char *szValue, int iMaxLen )
{
	if ( FStrEq( szKeyName, "color" ) )
	{
		Q_snprintf( szValue, iMaxLen, "%d %d %d %d", m_LightColor.GetR(), m_LightColor.GetG(), m_LightColor.GetB(), m_LightColor.GetA() );
		return true;
	}
	else if ( FStrEq( szKeyName, "texturename" ) )
	{
		Q_snprintf( szValue, iMaxLen, "%s", m_TextureName.Get() );
		return true;
	}
	return BaseClass::GetKeyValue( szKeyName, szValue, iMaxLen );
}

//------------------------------------------------------------------------------
// Purpose :
//------------------------------------------------------------------------------
void CSunlightShadowControl::Spawn( void )
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
	Q_strcpy( m_TextureName.GetForModify(), inputdata.value.String() );
}

void CSunlightShadowControl::InputSetEnableShadows( inputdata_t &inputdata )
{
	m_bEnableShadows = inputdata.value.Bool();
}

void CSunlightShadowControl::InputSetLightColor( inputdata_t &inputdata )
{
	m_LightColor = inputdata.value.Color32();
}
