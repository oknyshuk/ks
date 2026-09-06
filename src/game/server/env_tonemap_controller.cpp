//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "reflect_datamap.h"
#include "reflect_sendtable.h"
#include "reflect_annotations.h"
#include "env_tonemap_controller.h"
#include "baseentity.h"
#include "entityoutput.h"
#include "convar.h"
#include "triggers.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"
// Spawn Flags
#define SF_TONEMAP_MASTER			0x0001

// 0 - eyes fully closed / fully black
// 1 - nominal 
// 16 - eyes wide open / fully white

//-----------------------------------------------------------------------------
// Purpose: Entity that controls player's tonemap
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_EnvTonemapController" } ]]
      CEnvTonemapController : public CPointEntity
{
	DECLARE_CLASS( CEnvTonemapController, CPointEntity );
public:
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	CEnvTonemapController();

	void	Spawn( void );
	int		UpdateTransmitState( void );

	bool	IsMaster( void ) const					{ return HasSpawnFlags( SF_TONEMAP_MASTER ); }

	// Inputs
	[[= ks::reflect::Input{ .name = "SetTonemapRate", .type = FIELD_FLOAT } ]] void	InputSetTonemapRate( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetAutoExposureMin", .type = FIELD_FLOAT } ]] void	InputSetAutoExposureMin( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetAutoExposureMax", .type = FIELD_FLOAT } ]] void	InputSetAutoExposureMax( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "UseDefaultAutoExposure", .type = FIELD_VOID } ]] void	InputUseDefaultAutoExposure( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetBloomScale", .type = FIELD_FLOAT } ]] void	InputSetBloomScale( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "UseDefaultBloomScale", .type = FIELD_VOID } ]] void	InputUseDefaultBloomScale( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetBloomScaleRange", .type = FIELD_FLOAT } ]] void	InputSetBloomScaleRange( inputdata_t &inputdata );

	[[= ks::reflect::Input{ .name = "SetBloomExponent", .type = FIELD_FLOAT } ]] void	InputSetBloomExponent( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetBloomSaturation", .type = FIELD_FLOAT } ]] void	InputSetBloomSaturation( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetTonemapPercentTarget", .type = FIELD_FLOAT } ]] void	InputSetTonemapPercentTarget( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetTonemapPercentBrightPixels", .type = FIELD_FLOAT } ]] void	InputSetTonemapPercentBrightPixels( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetTonemapMinAvgLum", .type = FIELD_FLOAT } ]] void	InputSetTonemapMinAvgLum( inputdata_t &inputdata );

public:
	CNetworkVar( bool, m_bUseCustomAutoExposureMin, [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( bool, m_bUseCustomAutoExposureMax, [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( bool, m_bUseCustomBloomScale, [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( float, m_flCustomAutoExposureMin, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_flCustomAutoExposureMax, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_flCustomBloomScale, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_flCustomBloomScaleMinimum, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_flBloomExponent, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_flBloomSaturation, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_flTonemapPercentTarget, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_flTonemapPercentBrightPixels, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_flTonemapMinAvgLum, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_flTonemapRate, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
};

LINK_ENTITY_TO_CLASS( env_tonemap_controller, CEnvTonemapController );

IMPLEMENT_REFLECT_DATAMAP( CEnvTonemapController )

IMPLEMENT_REFLECT_SERVERCLASS( CEnvTonemapController, DT_EnvTonemapController )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEnvTonemapController::CEnvTonemapController()
{
	m_flBloomExponent = 2.5f;
	m_flBloomSaturation = 1.0f;
	m_flTonemapPercentTarget = 65.0f;
	m_flTonemapPercentBrightPixels = 2.0f;
	m_flTonemapMinAvgLum = 3.0f;
	m_flTonemapRate = 1.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvTonemapController::Spawn( void )
{
	SetSolid( SOLID_NONE );
	SetMoveType( MOVETYPE_NONE );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CEnvTonemapController::UpdateTransmitState()
{
	return SetTransmitState( FL_EDICT_ALWAYS );
}

//-----------------------------------------------------------------------------
// Purpose: set a base and minimum bloom scale
//-----------------------------------------------------------------------------
void CEnvTonemapController::InputSetBloomScaleRange( inputdata_t &inputdata )
{
	float bloom_max=1, bloom_min=1;
	int nargs=sscanf("%f %f",inputdata.value.String(), bloom_max, bloom_min );
	if (nargs != 2)
	{
		Warning("%s (%s) received SetBloomScaleRange input without 2 arguments. Syntax: <max bloom> <min bloom>\n", GetClassname(), GetDebugName() );
		return;
	}
	m_flCustomBloomScale=bloom_max;
	m_flCustomBloomScale=bloom_min;
}

//-----------------------------------------------------------------------------
// Purpose: Set the auto exposure min to the specified value
//-----------------------------------------------------------------------------
void CEnvTonemapController::InputSetAutoExposureMin( inputdata_t &inputdata )
{
	m_flCustomAutoExposureMin = inputdata.value.Float();
	m_bUseCustomAutoExposureMin = true;
}

//-----------------------------------------------------------------------------
// Purpose: Set the auto exposure max to the specified value
//-----------------------------------------------------------------------------
void CEnvTonemapController::InputSetAutoExposureMax( inputdata_t &inputdata )
{
	m_flCustomAutoExposureMax = inputdata.value.Float();
	m_bUseCustomAutoExposureMax = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvTonemapController::InputUseDefaultAutoExposure( inputdata_t &inputdata )
{
	m_bUseCustomAutoExposureMin = false;
	m_bUseCustomAutoExposureMax = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvTonemapController::InputSetBloomScale( inputdata_t &inputdata )
{
	m_flCustomBloomScale = inputdata.value.Float();
	m_flCustomBloomScaleMinimum = m_flCustomBloomScale;
	m_bUseCustomBloomScale = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvTonemapController::InputUseDefaultBloomScale( inputdata_t &inputdata )
{
	m_bUseCustomBloomScale = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvTonemapController::InputSetBloomExponent( inputdata_t &inputdata )
{
	m_flBloomExponent = inputdata.value.Float();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvTonemapController::InputSetBloomSaturation( inputdata_t &inputdata )
{
	m_flBloomSaturation = inputdata.value.Float();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvTonemapController::InputSetTonemapPercentTarget( inputdata_t &inputdata )
{
	m_flTonemapPercentTarget = inputdata.value.Float();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvTonemapController::InputSetTonemapPercentBrightPixels( inputdata_t &inputdata )
{
	m_flTonemapPercentBrightPixels= inputdata.value.Float();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvTonemapController::InputSetTonemapMinAvgLum( inputdata_t &inputdata )
{
	m_flTonemapMinAvgLum = inputdata.value.Float();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CEnvTonemapController::InputSetTonemapRate( inputdata_t &inputdata )
{
	m_flTonemapRate = inputdata.value.Float();
}

//--------------------------------------------------------------------------------------------------------
LINK_ENTITY_TO_CLASS( trigger_tonemap, CTonemapTrigger );

IMPLEMENT_REFLECT_DATAMAP( CTonemapTrigger )


//--------------------------------------------------------------------------------------------------------
void CTonemapTrigger::Spawn( void )
{
	AddSpawnFlags( SF_TRIGGER_ALLOW_CLIENTS );

	BaseClass::Spawn();
	InitTrigger();

	m_hTonemapController = gEntList.FindEntityByName( NULL, m_tonemapControllerName );
}


//--------------------------------------------------------------------------------------------------------
void CTonemapTrigger::StartTouch( CBaseEntity *other )
{
	if ( !PassesTriggerFilters( other ) )
		return;

	BaseClass::StartTouch( other );

	CBasePlayer *player = ToBasePlayer( other );
	if ( !player )
		return;

	player->OnTonemapTriggerStartTouch( this );
}


//--------------------------------------------------------------------------------------------------------
void CTonemapTrigger::EndTouch( CBaseEntity *other )
{
	if ( !PassesTriggerFilters( other ) )
		return;

	BaseClass::EndTouch( other );

	CBasePlayer *player = ToBasePlayer( other );
	if ( !player )
		return;

	player->OnTonemapTriggerEndTouch( this );
}


//-----------------------------------------------------------------------------
// Purpose: Clear out the tonemap controller.
//-----------------------------------------------------------------------------
void CTonemapSystem::LevelInitPreEntity( void )
{
	m_hMasterController = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: On level load find the master tonemap controller.  If no controller is 
//			set as Master, use the first tonemap controller found.
//-----------------------------------------------------------------------------
void CTonemapSystem::LevelInitPostEntity( void )
{
	// Overall master controller
	CEnvTonemapController *pTonemapController = NULL;
	do
	{
		pTonemapController = static_cast<CEnvTonemapController*>( gEntList.FindEntityByClassname( pTonemapController, "env_tonemap_controller" ) );
		if ( pTonemapController )
		{
			if ( m_hMasterController == NULL )
			{
				m_hMasterController = pTonemapController;
			}
			else
			{
				if ( pTonemapController->IsMaster() )
				{
					m_hMasterController = pTonemapController;
				}
			}
		}
	} while ( pTonemapController );

	
}


//--------------------------------------------------------------------------------------------------------
CTonemapSystem s_TonemapSystem( "TonemapSystem" );


//--------------------------------------------------------------------------------------------------------
CTonemapSystem *TheTonemapSystem( void )
{
	return &s_TonemapSystem;
}


//--------------------------------------------------------------------------------------------------------
