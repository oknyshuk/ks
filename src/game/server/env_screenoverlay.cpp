//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Entity to control screen overlays on a player
//
//=============================================================================//

#include "cbase.h"
#include "reflect_datamap.h"
#include "reflect_sendtable.h"
#include "reflect_annotations.h"
#include "shareddefs.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// named by the Proxy<> annotation below, so it must be declared before the class.
void SendProxy_String_tToString( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_EnvScreenOverlay" } ]]
      CEnvScreenOverlay : public CPointEntity
{
	DECLARE_CLASS( CEnvScreenOverlay, CPointEntity );
public:
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	CEnvScreenOverlay();

	// Always transmit to clients
	virtual int UpdateTransmitState();
	virtual void Spawn( void );
	virtual void Precache( void );

	[[= ks::reflect::Input{ .name = "StartOverlays", .type = FIELD_VOID } ]] void	InputStartOverlay( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "StopOverlays", .type = FIELD_VOID } ]] void	InputStopOverlay( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SwitchOverlay", .type = FIELD_INTEGER } ]] void	InputSwitchOverlay( inputdata_t &inputdata );

	void	SetActive( bool bActive ) { m_bIsActive = bActive; }
	
protected:
	CNetworkArray( string_t, m_iszOverlayNames, MAX_SCREEN_OVERLAYS, [[= ks::reflect::Key{ .name = "OverlayName10", .index = 9 } ]]  [[= ks::reflect::Key{ .name = "OverlayName9", .index = 8 } ]]  [[= ks::reflect::Key{ .name = "OverlayName8", .index = 7 } ]]  [[= ks::reflect::Key{ .name = "OverlayName7", .index = 6 } ]]  [[= ks::reflect::Key{ .name = "OverlayName6", .index = 5 } ]]  [[= ks::reflect::Key{ .name = "OverlayName5", .index = 4 } ]]  [[= ks::reflect::Key{ .name = "OverlayName4", .index = 3 } ]]  [[= ks::reflect::Key{ .name = "OverlayName3", .index = 2 } ]]  [[= ks::reflect::Key{ .name = "OverlayName2", .index = 1 } ]]  [[= ks::reflect::Key{ .name = "OverlayName1", .index = 0 } ]]
	               [[= ks::reflect::Net{ .enc = ks::reflect::ENC_STRING, .varlen = true } ]]
	               [[= ks::reflect::Proxy<SendProxy_String_tToString, ks::reflect::WIRE_SEND>{} ]] );
	CNetworkArray( float, m_flOverlayTimes, MAX_SCREEN_OVERLAYS, [[= ks::reflect::Key{ .name = "OverlayTime10", .index = 9 } ]]  [[= ks::reflect::Key{ .name = "OverlayTime9", .index = 8 } ]]  [[= ks::reflect::Key{ .name = "OverlayTime8", .index = 7 } ]]  [[= ks::reflect::Key{ .name = "OverlayTime7", .index = 6 } ]]  [[= ks::reflect::Key{ .name = "OverlayTime6", .index = 5 } ]]  [[= ks::reflect::Key{ .name = "OverlayTime5", .index = 4 } ]]  [[= ks::reflect::Key{ .name = "OverlayTime4", .index = 3 } ]]  [[= ks::reflect::Key{ .name = "OverlayTime3", .index = 2 } ]]  [[= ks::reflect::Key{ .name = "OverlayTime2", .index = 1 } ]]  [[= ks::reflect::Key{ .name = "OverlayTime1", .index = 0 } ]]
	               [[= ks::reflect::Net{ .bits = 11, .low = -1.0f, .high = 63.0f, .flags = SPROP_ROUNDDOWN, .varlen = true } ]] );
	CNetworkVar( float, m_flStartTime, [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( int, m_iDesiredOverlay, [[= ks::reflect::Net{ .bits = 5 } ]] );
	CNetworkVar( bool, m_bIsActive, [[= ks::reflect::Net{} ]] );
};

LINK_ENTITY_TO_CLASS( env_screenoverlay, CEnvScreenOverlay );

IMPLEMENT_REFLECT_DATAMAP( CEnvScreenOverlay )

void SendProxy_String_tToString( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID )
{
	string_t *pString = (string_t*)pData;
	pOut->m_pString = (char*)STRING( *pString );
}

IMPLEMENT_REFLECT_SERVERCLASS( CEnvScreenOverlay, DT_EnvScreenOverlay )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEnvScreenOverlay::CEnvScreenOverlay( void )
{
	m_flStartTime = 0;
	m_iDesiredOverlay = 0;
	m_bIsActive = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvScreenOverlay::Spawn( void )
{
	Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvScreenOverlay::Precache( void )
{
	for ( int i = 0; i < 10; i++ )
	{
		if ( m_iszOverlayNames[i] == NULL_STRING )
			continue;

		PrecacheMaterial( STRING( m_iszOverlayNames[i] ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CEnvScreenOverlay::InputStartOverlay( inputdata_t &inputdata )
{
	if ( m_iszOverlayNames[0] == NULL_STRING )
	{
		Warning("env_screenoverlay %s has no overlays to display.\n", STRING(GetEntityName()) );
		return;
	}

	m_flStartTime = gpGlobals->curtime;
	m_bIsActive = true;

	// Turn off any other screen overlays out there
	CBaseEntity *pEnt = nullptr;
	while ( (pEnt = gEntList.FindEntityByClassname( pEnt, "env_screenoverlay" )) != nullptr )
	{
		if ( pEnt != this )
		{
			CEnvScreenOverlay *pOverlay = assert_cast<CEnvScreenOverlay*>(pEnt);
			pOverlay->SetActive( false );
		}
	}
}

int CEnvScreenOverlay::UpdateTransmitState()
{
	return SetTransmitState( FL_EDICT_ALWAYS );
}


void CEnvScreenOverlay::InputSwitchOverlay( inputdata_t &inputdata )
{
	int iNewOverlay = inputdata.value.Int() - 1;
	iNewOverlay = abs( iNewOverlay );

	if ( m_iszOverlayNames[iNewOverlay] == NULL_STRING )
	{
		Warning("env_screenoverlay %s has no overlays to display.\n", STRING(GetEntityName()) );
		return;
	}

	m_iDesiredOverlay = iNewOverlay;
	m_flStartTime = gpGlobals->curtime;
}

void CEnvScreenOverlay::InputStopOverlay( inputdata_t &inputdata )
{
	if ( m_iszOverlayNames[0] == NULL_STRING )
	{
		Warning("env_screenoverlay %s has no overlays to display.\n", STRING(GetEntityName()) );
		return;
	}

	m_flStartTime = -1;
	m_bIsActive = false; 
}

// ====================================================================================
//
//  Screen-space effects
//
// ====================================================================================

class [[= ks::reflect::NetTable{ .name = "DT_EnvScreenEffect" } ]]
      CEnvScreenEffect : public CPointEntity
{
	DECLARE_CLASS( CEnvScreenEffect, CPointEntity );
public:
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	// We always want to be sent to the client
	CEnvScreenEffect( void ) { 	AddEFlags( EFL_FORCE_CHECK_TRANSMIT ); }
	virtual int UpdateTransmitState( void )	{ return SetTransmitState( FL_EDICT_ALWAYS ); }
	virtual void Spawn( void );
	virtual void Precache( void );

private:

	[[= ks::reflect::Input{ .name = "StartEffect", .type = FIELD_FLOAT } ]] void InputStartEffect( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "StopEffect", .type = FIELD_FLOAT } ]] void InputStopEffect( inputdata_t &inputdata );

	CNetworkVar( float, m_flDuration, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( int, m_nType, [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "type" } ]] );
};

LINK_ENTITY_TO_CLASS( env_screeneffect, CEnvScreenEffect );

// CEnvScreenEffect
IMPLEMENT_REFLECT_DATAMAP( CEnvScreenEffect )

IMPLEMENT_REFLECT_SERVERCLASS( CEnvScreenEffect, DT_EnvScreenEffect )

void CEnvScreenEffect::Spawn( void )
{
	Precache();
}

void CEnvScreenEffect::Precache( void )
{
	PrecacheMaterial( "effects/stun" );
	PrecacheMaterial( "effects/introblur" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvScreenEffect::InputStartEffect( inputdata_t &inputdata )
{
	// Take the duration as our value
	m_flDuration = inputdata.value.Float();

	EntityMessageBegin( this );
		WRITE_BYTE( 0 );
	MessageEnd();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvScreenEffect::InputStopEffect( inputdata_t &inputdata )
{
	m_flDuration = inputdata.value.Float();

	// Send the stop notification
	EntityMessageBegin( this );
		WRITE_BYTE( 1 );
	MessageEnd();
}
