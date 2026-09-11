//========= Copyright (c) 1996-2007, Valve Corporation, All rights reserved. ====
//
// An entity that allows level designer control over the fog parameters.
//
//=============================================================================

#include "cbase.h"
#include "reflect_datamap.h"
#include "reflect_sendtable.h"
#include "reflect_annotations.h"
#include "postprocesscontroller.h"
#include "entityinput.h"
#include "entityoutput.h"
#include "eventqueue.h"
#include "player.h"
#include "world.h"
#include "ndebugoverlay.h"
#include "triggers.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CPostProcessSystem s_PostProcessSystem( "PostProcessSystem" );

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CPostProcessSystem *PostProcessSystem( void )
{
	return &s_PostProcessSystem;
}


LINK_ENTITY_TO_CLASS( postprocess_controller, CPostProcessController );

IMPLEMENT_REFLECT_DATAMAP( CPostProcessController )

IMPLEMENT_REFLECT_SERVERCLASS( CPostProcessController, DT_PostProcessController )


CPostProcessController::CPostProcessController()
:	m_bMaster( false )
{	
}

CPostProcessController::~CPostProcessController()
{
}

void CPostProcessController::Spawn( void )
{
	BaseClass::Spawn();

	m_bMaster = IsMaster();
}

//-----------------------------------------------------------------------------
// Activate!
//-----------------------------------------------------------------------------
void CPostProcessController::Activate( ) 
{
	BaseClass::Activate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CPostProcessController::UpdateTransmitState()
{
	return SetTransmitState( FL_EDICT_ALWAYS );
}

void CPostProcessController::InputSetFadeTime( inputdata_t &inputdata )
{
	m_flPostProcessParameters.Set( PPPN_FADE_TIME, inputdata.value.Float() );
}

void CPostProcessController::InputSetLocalContrastStrength( inputdata_t &inputdata )
{
	m_flPostProcessParameters.Set( PPPN_LOCAL_CONTRAST_STRENGTH, inputdata.value.Float() );
}

void CPostProcessController::InputSetLocalContrastEdgeStrength( inputdata_t &inputdata )
{
	m_flPostProcessParameters.Set( PPPN_LOCAL_CONTRAST_EDGE_STRENGTH, inputdata.value.Float() );
}

void CPostProcessController::InputSetVignetteStart( inputdata_t &inputdata )
{
	m_flPostProcessParameters.Set( PPPN_VIGNETTE_START, inputdata.value.Float() );
}

void CPostProcessController::InputSetVignetteEnd( inputdata_t &inputdata )
{
	m_flPostProcessParameters.Set( PPPN_VIGNETTE_END, inputdata.value.Float() );
}

void CPostProcessController::InputSetVignetteBlurStrength( inputdata_t &inputdata )
{
	m_flPostProcessParameters.Set( PPPN_VIGNETTE_BLUR_STRENGTH, inputdata.value.Float() );
}

void CPostProcessController::InputSetFadeToBlackStrength( inputdata_t &inputdata )
{
	m_flPostProcessParameters.Set( PPPN_FADE_TO_BLACK_STRENGTH, inputdata.value.Float() );
}

void CPostProcessController::InputSetDepthBlurFocalDistance( inputdata_t &inputdata )
{
	m_flPostProcessParameters.Set( PPPN_DEPTH_BLUR_FOCAL_DISTANCE, inputdata.value.Float() );
}

void CPostProcessController::InputSetDepthBlurStrength( inputdata_t &inputdata )
{
	m_flPostProcessParameters.Set( PPPN_DEPTH_BLUR_STRENGTH, inputdata.value.Float() );
}

void CPostProcessController::InputSetScreenBlurStrength( inputdata_t &inputdata )
{
	m_flPostProcessParameters.Set( PPPN_SCREEN_BLUR_STRENGTH, inputdata.value.Float() );
}

void CPostProcessController::InputSetFilmGrainStrength( inputdata_t &inputdata )
{
	m_flPostProcessParameters.Set( PPPN_FILM_GRAIN_STRENGTH, inputdata.value.Float() );
}

//-----------------------------------------------------------------------------
// Purpose: Clear out the PostProcess controller.
//-----------------------------------------------------------------------------
void CPostProcessSystem::LevelInitPreEntity( void )
{
	m_hMasterController = nullptr;
	ListenForGameEvent( "round_start" );
}

//-----------------------------------------------------------------------------
// Purpose: Find the master controller.  If no controller is 
//			set as Master, use the first controller found.
//-----------------------------------------------------------------------------
void CPostProcessSystem::InitMasterController( void )
{
	CPostProcessController *pPostProcessController = nullptr;

	do
	{
		pPostProcessController = dynamic_cast<CPostProcessController*>( gEntList.FindEntityByClassname( pPostProcessController, "postprocess_controller" ) );
		if ( pPostProcessController )
		{
			if ( m_hMasterController.Get() == nullptr )
			{
				m_hMasterController = pPostProcessController;
			}
			else
			{
				if ( pPostProcessController->IsMaster() )
				{
					m_hMasterController = pPostProcessController;
				}
			}
		}
	} while ( pPostProcessController );
}

//-----------------------------------------------------------------------------
// Purpose: On a multiplayer map restart, re-find the master controller.
//-----------------------------------------------------------------------------
void CPostProcessSystem::FireGameEvent( IGameEvent *pEvent )
{
	InitMasterController();
}

//-----------------------------------------------------------------------------
// Purpose: On level load find the master PostProcess controller.  If no controller is 
//			set as Master, use the first PostProcess controller found.
//-----------------------------------------------------------------------------
void CPostProcessSystem::LevelInitPostEntity( void )
{
	InitMasterController();

	// HACK: Singleplayer games don't get a call to CBasePlayer::Spawn on level transitions.
	// CBasePlayer::Activate is called before this is called so that's too soon to set up the PostProcess controller.
	// We don't have a hook similar to Activate that happens after LevelInitPostEntity
	// is called, or we could just do this in the player itself.
	if ( gpGlobals->maxClients == 1 )
	{
		CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
		if ( pPlayer && ( pPlayer->m_hPostProcessCtrl.Get() == nullptr ) )
		{
			pPlayer->InitPostProcessController();
		}
	}
}
