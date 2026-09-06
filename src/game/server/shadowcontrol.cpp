//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Shadow control entity.
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
// Purpose : Shadow control entity
//------------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_ShadowControl", .base = false } ]]
      CShadowControl : public CBaseEntity
{
public:
	DECLARE_CLASS( CShadowControl, CBaseEntity );

	CShadowControl();

	void Spawn( void );
	bool KeyValue( const char *szKeyName, const char *szValue );
	int  UpdateTransmitState();
	[[= ks::reflect::Input{ .name = "SetAngles", .type = FIELD_STRING } ]] void InputSetAngles( inputdata_t &inputdata );

	virtual int	ObjectCaps( void ) { return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

private:
	CNetworkVector( m_shadowDirection, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE, .enc = ks::reflect::ENC_VECTOR } ]] [[= ks::reflect::Key{ .name = "direction", .input = true } ]] );
	CNetworkColor32( m_shadowColor, [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Proxy<SendProxy_Color32ToInt32, ks::reflect::WIRE_SEND>{} ]] [[= ks::reflect::Key{ .name = "color", .input = true } ]] );
	CNetworkVar( float, m_flShadowMaxDist, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "distance" } ]] [[= ks::reflect::Key{ .name = "SetDistance", .input = true } ]] );
	CNetworkVar( bool, m_bDisableShadows, [[= ks::reflect::Net{} ]] [[= ks::reflect::Key{ .name = "disableallshadows" } ]] [[= ks::reflect::Key{ .name = "SetShadowsDisabled", .input = true } ]] );
	CNetworkVar( bool, m_bEnableLocalLightShadows, [[= ks::reflect::Net{} ]] [[= ks::reflect::Key{ .name = "enableshadowsfromlocallights" } ]] [[= ks::reflect::Key{ .name = "SetShadowsFromLocalLightsEnabled", .input = true } ]] );
};

LINK_ENTITY_TO_CLASS(shadow_control, CShadowControl);

IMPLEMENT_REFLECT_DATAMAP( CShadowControl )


IMPLEMENT_REFLECT_SERVERCLASS( CShadowControl, DT_ShadowControl )


CShadowControl::CShadowControl()
{
	m_shadowDirection.Init( 0.2, 0.2, -2 );
	m_flShadowMaxDist = 50.0f;
	m_shadowColor.Init( 64, 64, 64, 0 );
	m_bDisableShadows = false;
	m_bEnableLocalLightShadows = false;
}


//------------------------------------------------------------------------------
// Purpose : Send even though we don't have a model
//------------------------------------------------------------------------------
int CShadowControl::UpdateTransmitState()
{
	// ALWAYS transmit to all clients.
	return SetTransmitState( FL_EDICT_ALWAYS );
}


bool CShadowControl::KeyValue( const char *szKeyName, const char *szValue )
{
	if ( FStrEq( szKeyName, "color" ) )
	{
		color32 tmp;
		V_StringToColor32( &tmp, szValue );
		m_shadowColor = tmp;
		return true;
	}

	if ( FStrEq( szKeyName, "angles" ) )
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

	// For backward compatibility...
	if ( FStrEq( szKeyName, "direction" ) )
	{
		// Only use this if angles haven't been set...
		if ( fabs(m_shadowDirection->LengthSqr() - 1.0f) > 1e-3 )
		{
			Vector vTemp;
			UTIL_StringToVector( vTemp.Base(), szValue );
			m_shadowDirection = vTemp;
		}
		return true;
	}

	return BaseClass::KeyValue( szKeyName, szValue );
}

//------------------------------------------------------------------------------
// Purpose :
//------------------------------------------------------------------------------
void CShadowControl::Spawn( void )
{
	Precache();
	SetSolid( SOLID_NONE );
}

//------------------------------------------------------------------------------
// Input values
//------------------------------------------------------------------------------
void CShadowControl::InputSetAngles( inputdata_t &inputdata )
{
	const char *pAngles = inputdata.value.String();

	QAngle angles;
	UTIL_StringToVector( angles.Base(), pAngles );

	Vector vTemp;
	AngleVectors( angles, &vTemp );
	m_shadowDirection = vTemp;
}
