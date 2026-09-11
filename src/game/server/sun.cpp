//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "reflect_datamap.h"
#include "reflect_sendtable.h"
#include "reflect_annotations.h"
#include "baseentity.h"
#include "sendproxy.h"
#include "sun_shared.h"
#include "map_utils.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class [[= ks::reflect::NetTable{ .name = "DT_Sun", .base = false } ]]
      [[= ks::reflect::From<"m_clrRender", ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED }, SendProxy_Color32ToInt32>{} ]]
      CSun : public CBaseEntity
{
public:
	DECLARE_CLASS( CSun, CBaseEntity );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CSun();

	virtual void	Activate();

	// Input handlers
	[[= ks::reflect::Input{ .name = "TurnOn", .type = FIELD_VOID } ]] void InputTurnOn( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "TurnOff", .type = FIELD_VOID } ]] void InputTurnOff( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetColor", .type = FIELD_COLOR32 } ]] void InputSetColor( inputdata_t &inputdata );

	virtual int UpdateTransmitState();

public:
	CNetworkVector( m_vDirection, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NORMAL, .enc = ks::reflect::ENC_VECTOR } ]] );
	
	[[= ks::reflect::Key{ .name = "material" } ]] string_t	m_strMaterial;
	[[= ks::reflect::Key{ .name = "overlaymaterial" } ]] string_t	m_strOverlayMaterial;

	[[= ks::reflect::Key{ .name = "use_angles" } ]] int		m_bUseAngles;
	[[= ks::reflect::Key{ .name = "pitch" } ]] float	m_flPitch;
	[[= ks::reflect::Key{ .name = "angle" } ]] float	m_flYaw;
	
	CNetworkVar( int, m_nSize, [[= ks::reflect::Net{ .bits = 10, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "size" } ]] );		// Size of the main core image
	CNetworkVar( int, m_nOverlaySize, [[= ks::reflect::Net{ .bits = 10, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "overlaysize" } ]] ); // Size for the glow overlay
	CNetworkVar( color32, m_clrOverlay, [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Proxy<SendProxy_Color32ToInt32, ks::reflect::WIRE_SEND>{} ]] [[= ks::reflect::Key{ .name = "overlaycolor" } ]] );
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

void CSun::Activate()
{
	BaseClass::Activate();

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

int CSun::UpdateTransmitState()
{
	return SetTransmitState( FL_EDICT_ALWAYS );
}


