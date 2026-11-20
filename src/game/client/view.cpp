// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

module;
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "bitmap/tgawriter.h"
#include "cdll_int.h"
#include "edict.h"
#include "engine/SndInfo.h"
#include "view_shared.h"
#include "cdll_client_int.h"
#include "engine/IClientLeafSystem.h"
#include "engine/IEngineTrace.h"
#include "iclientrenderable.h"
#include "vphysics_interface.h"
#include "beam_shared.h"
#include "beamdraw.h"
#include "con_nprint.h"
#include "engine/ivmodelinfo.h"
#include "reflect.h"
#include "shake.h"
#include "usermessages_schema.h"
#include "hud_macros.h"
#include "reflect_datamap.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/itexture.h"
module ks.client;

import std;
import ks.core.basetypes;
import ks.core.cmdline;
import ks.core.dbg;
import ks.core.memalloc;
import ks.core.platform;
import ks.core.random;
import ks.core.vprof;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.ssemath;
import ks.math.vector;
import ks.math.vmatrix;
import ks.math.vplane;
import ks.platform;
import ks.simd;
import ks.tier1.byte_writer;
import ks.tier1.convar;
import ks.tier1.cvar;
import ks.tier1.exprevaluator;
import ks.tier1.iconvar;
import ks.tier1.icvar;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.keyvalues;
import ks.tier1.kvpacker;
import ks.tier1.reflect_wirevar;
import ks.tier1.registry;
import ks.tier1.strtools;
import ks.tier1.timeutils;
import ks.tier1.uniqueid;
import :baseanimatedtextureproxy;
import :props;
import :iinput;
import :baseentity;
import :sharedvar;
import :util;
import :particlemgr;
import :cstrike15.cs_player;
import :precache;
import ks.game.shareddefs;
import :achievements_and_stats_interface;
import :cstrike15.cs_player_shared;
import ks.game.gamemovement;
import ks.game.mapentities_shared;
import :noopanimcontroller;
import ks.core.types;
import ks.game.beam_flags;
import ks.game.tempentity;
import ks.math.pfns;
import ks.math.transform;
import ks.core.wchartypes;
import ks.tier1.bitbuf;
import ks.tier1.vector_ops;

class CViewRender;
class CBaseEntity;
class CBasePlayer;
class CCSPlayer;

using namespace ::ks::reflect;

// Console GPU alignment stub (not used on PC)
#ifndef GPU_RESOLVE_ALIGNMENT
#define GPU_RESOLVE_ALIGNMENT 1
#endif

#define USE_MONITORS

extern ConVar default_fov;
extern bool g_bRenderingScreenshot;

inline constexpr int SAVEGAME_SCREENSHOT_WIDTH = 256;
inline constexpr int SAVEGAME_SCREENSHOT_HEIGHT = 256;

extern ConVar sensitivity;

ConVar zoom_sensitivity_ratio_joystick( side_registry(), "zoom_sensitivity_ratio_joystick", "1.0", FCVAR_ARCHIVE | FCVAR_SS, "Additional controller sensitivity scale factor applied when FOV is zoomed in." );
ConVar zoom_sensitivity_ratio_mouse( side_registry(), "zoom_sensitivity_ratio_mouse", "1.0", FCVAR_ARCHIVE | FCVAR_ARCHIVE_GAMECONSOLE | FCVAR_SS, "Additional mouse sensitivity scale factor applied when FOV is zoomed in." );

// Each MOD implements GetViewRenderInstance() and provides either a default object or a subclassed object!!!
IViewRender *view = nullptr;	// set in cldll_client_init.cpp if no mod creates their own

static Vector g_vecRenderOrigin[ MAX_SPLITSCREEN_PLAYERS ];
static QAngle g_vecRenderAngles[ MAX_SPLITSCREEN_PLAYERS ];
static Vector g_vecPrevRenderOrigin[ MAX_SPLITSCREEN_PLAYERS ];	// Last frame's render origin
static QAngle g_vecPrevRenderAngles[ MAX_SPLITSCREEN_PLAYERS ]; // Last frame's render angles
static Vector g_vecVForward[ MAX_SPLITSCREEN_PLAYERS ], g_vecVRight[ MAX_SPLITSCREEN_PLAYERS ], g_vecVUp[ MAX_SPLITSCREEN_PLAYERS ];
static VMatrix g_matCamInverse[ MAX_SPLITSCREEN_PLAYERS ];

extern ConVar cl_forwardspeed;

static ConVar v_centermove( side_registry(), "v_centermove", "0.15");
static ConVar v_centerspeed( side_registry(), "v_centerspeed","500" );

// 54 degrees approximates a 35mm camera - we determined that this makes the viewmodels
// and motions look the most natural.
ConVar v_viewmodel_fov( side_registry(), "viewmodel_fov", "54", FCVAR_ARCHIVE );

ConVar mat_viewportscale( side_registry(), "mat_viewportscale", "1.0", FCVAR_CHEAT, "Scale down the main viewport (to reduce GPU impact on CPU profiling)",
								  true, (1.0f / 640.0f), true, 1.0f );
ConVar mat_viewportupscale( side_registry(), "mat_viewportupscale", "1", FCVAR_CHEAT, "Scale the viewport back up" );
ConVar cl_leveloverview( side_registry(), "cl_leveloverview", "0", FCVAR_CHEAT );

ConVar r_mapextents( side_registry(), "r_mapextents", "16384", FCVAR_CHEAT, 
						   "Set the max dimension for the map.  This determines the far clipping plane" );

static ConVar cl_camera_follow_bone_index( side_registry(), "cl_camera_follow_bone_index"  , "-2", FCVAR_CHEAT, "Index of the bone to follow.  -2 == disabled.  -1 == root bone.  0+ is bone index." );
Vector g_cameraFollowPos;

// UNDONE: Delete this or move to the material system?
ConVar	gl_clear( side_registry(), "gl_clear", "0",
	0
);
ConVar	gl_clear_randomcolor( side_registry(), "gl_clear_randomcolor", "0", FCVAR_CHEAT, "Clear the back buffer to random colors every frame. Helps spot open seams in geometry." );

static ConVar r_farz( side_registry(), "r_farz", "-1", FCVAR_CHEAT, "Override the far clipping plane. -1 means to use the value in env_fog_controller." );
static ConVar cl_demoviewoverride( side_registry(), "cl_demoviewoverride", "0", 0, "Override view during demo playback" );

static Vector s_DemoView;
static QAngle s_DemoAngle;

static void CalcDemoViewOverride( Vector &origin, QAngle &angles )
{
	engine->SetViewAngles( s_DemoAngle );

	input->ExtraMouseSample( gpGlobals->absoluteframetime, true );

	engine->GetViewAngles( s_DemoAngle );

	Vector forward, right, up;

	AngleVectors( s_DemoAngle, &forward, &right, &up );

	float speed = gpGlobals->absoluteframetime * cl_demoviewoverride.GetFloat() * 320;
	
	s_DemoView += speed * input->KeyState (&in_forward) * forward  ;
	s_DemoView -= speed * input->KeyState (&in_back) * forward ;

	s_DemoView += speed * input->KeyState (&in_moveright) * right ;
	s_DemoView -= speed * input->KeyState (&in_moveleft) * right ;

	origin = s_DemoView;
	angles = s_DemoAngle;
}

CViewSetup &CViewRender::GetView(int nSlot /*= -1*/)
{
	contract_assert( m_bAllowViewAccess );
	if ( nSlot == -1 )
	{
		ASSERT_LOCAL_PLAYER_RESOLVABLE();
		return m_UserView[ GET_ACTIVE_SPLITSCREEN_SLOT() ];
	}
	return m_UserView[ nSlot ];
}

const CViewSetup &CViewRender::GetView(int nSlot /*= -1*/) const
{
	contract_assert( m_bAllowViewAccess );
	if ( nSlot == -1 )
	{
		ASSERT_LOCAL_PLAYER_RESOLVABLE();
		return m_UserView[ GET_ACTIVE_SPLITSCREEN_SLOT() ];
	}
	return m_UserView[ nSlot ];
}

//-----------------------------------------------------------------------------
// Accessors to return the main view (where the player's looking)
//-----------------------------------------------------------------------------
const Vector &MainViewOrigin( int nSlot )
{
	return g_vecRenderOrigin[ nSlot ];
}

const QAngle &MainViewAngles( int nSlot )
{
	return g_vecRenderAngles[ nSlot ];
}

const Vector &MainViewForward( int nSlot )
{
	return g_vecVForward[ nSlot ];
}

const Vector &MainViewRight( int nSlot )
{
	return g_vecVRight[ nSlot ];
}

const Vector &MainViewUp( int nSlot )
{
	return g_vecVUp[ nSlot ];
}

const VMatrix &MainWorldToViewMatrix( int nSlot )
{
	return g_matCamInverse[ nSlot ];
}

const Vector &PrevMainViewOrigin( int nSlot )
{
	return g_vecPrevRenderOrigin[ nSlot ];
}

const QAngle &PrevMainViewAngles( int nSlot )
{
	return g_vecPrevRenderAngles[ nSlot ];
}

//-----------------------------------------------------------------------------
// Compute the world->camera transform
//-----------------------------------------------------------------------------
void ComputeCameraVariables( const Vector &vecOrigin, const QAngle &vecAngles, 
	Vector *pVecForward, Vector *pVecRight, Vector *pVecUp, VMatrix *pMatCamInverse )
{
	// Compute view bases
	AngleVectors( vecAngles, pVecForward, pVecRight, pVecUp );

	for (int i = 0; i < 3; ++i)
	{
		(*pMatCamInverse)[0][i] = (*pVecRight)[i];	
		(*pMatCamInverse)[1][i] = (*pVecUp)[i];	
		(*pMatCamInverse)[2][i] = -(*pVecForward)[i];	
		(*pMatCamInverse)[3][i] = 0.0F;
	}
	(*pMatCamInverse)[0][3] = -DotProduct( *pVecRight, vecOrigin );
	(*pMatCamInverse)[1][3] = -DotProduct( *pVecUp, vecOrigin );
	(*pMatCamInverse)[2][3] =  DotProduct( *pVecForward, vecOrigin );
	(*pMatCamInverse)[3][3] = 1.0F;
}

bool R_CullSphere(
	VPlane const *pPlanes,
	int nPlanes,
	Vector const *pCenter,
	float radius)
{
	for(int i=0; i < nPlanes; i++)
		if(pPlanes[i].DistTo(*pCenter) < -radius)
			return true;
	
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
static void StartPitchDrift( void )
{
	view->StartPitchDrift();
}

static ConCommand centerview( side_registry(), "centerview", StartPitchDrift );

//-----------------------------------------------------------------------------
// Purpose: Initializes all view systems
//-----------------------------------------------------------------------------
extern CMaterialReference g_material_WriteZ;
void CViewRender::Init( void )
{
	memset( &m_PitchDrift, 0, sizeof( m_PitchDrift ) );

	m_bDrawOverlay = false;

	m_pDrawEntities		= cvar->FindVar( "r_drawentities" );
	m_pDrawBrushModels	= cvar->FindVar( "r_drawbrushmodels" );

	beams->InitBeams();
	tempents->Init();

	m_TranslucentSingleColor.Init( "debug/debugtranslucentsinglecolor", TEXTURE_GROUP_OTHER );
	m_ModulateSingleColor.Init( "engine/modulatesinglecolor", TEXTURE_GROUP_OTHER );
	m_WhiteMaterial.Init( "vgui/white", TEXTURE_GROUP_OTHER );
	
	g_material_WriteZ.Init( "engine/writez", TEXTURE_GROUP_OTHER );

	for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS ; ++i )
	{
		g_vecRenderOrigin[ i ].Init();
		g_vecRenderAngles[ i ].Init();
		g_vecPrevRenderOrigin[ i ].Init();
		g_vecPrevRenderAngles[ i ].Init();
		g_vecVForward[ i ].Init();
		g_vecVRight[ i ].Init();
		g_vecVUp[ i ].Init();
		g_matCamInverse[ i ].Identity();
	}
}

CMaterialReference &CViewRender::GetWhite()
{
	return m_WhiteMaterial;
}

//-----------------------------------------------------------------------------
// Purpose: Called once per level change
//-----------------------------------------------------------------------------
void CViewRender::LevelInit( void )
{
	beams->ClearBeams();
	tempents->Clear();

	m_BuildWorldListsNumber = 0;
	m_BuildRenderableListsNumber = 0;

	for ( int i = 0 ; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		m_FreezeParams[ i ].m_bTakeFreezeFrame = false;
		m_FreezeParams[ i ].m_flFreezeFrameUntil = 0;
	}

	// Clear our overlay materials
	m_ScreenOverlayMaterial.Init( nullptr );

	// Init all IScreenSpaceEffects
	g_pScreenSpaceEffects->InitScreenSpaceEffects( );

	InitFadeData();
}

//-----------------------------------------------------------------------------
// Purpose: Called once per level change
//-----------------------------------------------------------------------------
void CViewRender::LevelShutdown( void )
{
	g_pScreenSpaceEffects->ShutdownScreenSpaceEffects( );
}

//-----------------------------------------------------------------------------
// Purpose: Called at shutdown
//-----------------------------------------------------------------------------
void CViewRender::Shutdown( void )
{

	m_TranslucentSingleColor.Shutdown();
	m_ModulateSingleColor.Shutdown();
	m_ScreenOverlayMaterial.Shutdown();
	m_UnderWaterOverlayMaterial.Shutdown();
	m_WhiteMaterial.Shutdown();

	beams->ShutdownBeams();
	tempents->Shutdown();
}

//-----------------------------------------------------------------------------
// Returns the worldlists build number
//-----------------------------------------------------------------------------

int CViewRender::BuildWorldListsNumber( void ) const
{
	return m_BuildWorldListsNumber;
}

//-----------------------------------------------------------------------------
// Purpose: Start moving pitch toward ideal
//-----------------------------------------------------------------------------
void CViewRender::StartPitchDrift (void)
{
	if ( m_PitchDrift.laststop == gpGlobals->curtime )
	{
		// Something else is blocking the drift.
		return;		
	}

	if ( m_PitchDrift.nodrift || !m_PitchDrift.pitchvel )
	{
		m_PitchDrift.pitchvel	= v_centerspeed.GetFloat();
		m_PitchDrift.nodrift	= false;
		m_PitchDrift.driftmove	= 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CViewRender::StopPitchDrift (void)
{
	m_PitchDrift.laststop	= gpGlobals->curtime;
	m_PitchDrift.nodrift	= true;
	m_PitchDrift.pitchvel	= 0;
}

//-----------------------------------------------------------------------------
// Purpose: Moves the client pitch angle towards cl.idealpitch sent by the server.
// If the user is adjusting pitch manually, either with lookup/lookdown,
//   mlook and mouse, or klook and keyboard, pitch drifting is constantly stopped.
//-----------------------------------------------------------------------------
void CViewRender::DriftPitch (void)
{
	float		delta, move;

	CBasePlayer *player = CBasePlayer::GetLocalPlayer();
	if ( !player )
		return;

	if ( g_bEngineIsHLTV || ( player->GetGroundEntity() == nullptr ) || engine->IsPlayingDemo() )
	{
		m_PitchDrift.driftmove = 0;
		m_PitchDrift.pitchvel = 0;
		return;
	}

	// Don't count small mouse motion
	if ( m_PitchDrift.nodrift )
	{
		if ( fabs( input->GetLastForwardMove() ) < cl_forwardspeed.GetFloat() )
		{
			m_PitchDrift.driftmove = 0;
		}
		else
		{
			m_PitchDrift.driftmove += gpGlobals->frametime;
		}
	
		if ( m_PitchDrift.driftmove > v_centermove.GetFloat() )
		{
			StartPitchDrift ();
		}
		return;
	}
	
	// How far off are we
	delta = prediction->GetIdealPitch( player->GetSplitScreenPlayerSlot() ) - player->GetAbsAngles()[ PITCH ];
	if ( !delta )
	{
		m_PitchDrift.pitchvel = 0;
		return;
	}

	// Determine movement amount
	move = gpGlobals->frametime * m_PitchDrift.pitchvel;
	// Accelerate
	m_PitchDrift.pitchvel += gpGlobals->frametime * v_centerspeed.GetFloat();
	
	// Move predicted pitch appropriately
	if (delta > 0)
	{
		if ( move > delta )
		{
			m_PitchDrift.pitchvel = 0;
			move = delta;
		}
		player->SetLocalAngles( player->GetLocalAngles() + QAngle( move, 0, 0 ) );
	}
	else if ( delta < 0 )
	{
		if ( move > -delta )
		{
			m_PitchDrift.pitchvel = 0;
			move = -delta;
		}
		player->SetLocalAngles( player->GetLocalAngles() - QAngle( move, 0, 0 ) );
	}
}

// This is called by cdll_client_int to setup view model origins. This has to be done before
// simulation so entities can access attachment points on view models during simulation.
void CViewRender::OnRenderStart()
{
	CVProfScope _( "CViewRender::OnRenderStart", 2, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	IterateRemoteSplitScreenViewSlots_Push( true );
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD_UI( hh );

		// This will fill in one of the m_UserView[ hh ] slots
		SetUpView();

		// Adjust mouse sensitivity based upon the current FOV
		CCSPlayer *player = CCSPlayer::GetLocalCSPlayer();
		if ( player )
		{
			default_fov.SetValue( player->m_iDefaultFOV );

			//Update our FOV, including any zooms going on
			int iDefaultFOV = default_fov.GetInt();
			int	localFOV	= player->GetFOV();
			int min_fov		= player->GetMinFOV();
			bool bZoomed = (  player->GetFOV() != player->GetDefaultFOV() );

			// 'scoped' mode is handled by the ironsight system
/*
#if defined( CSTRIKE15 )
			bool bScoped = player->m_bIsScoped;
#else
			bool bScoped = bZoomed;
#endif
*/

			// Don't let it go too low
			localFOV = std::max( min_fov, localFOV );

			GetHud().m_flFOVSensitivityAdjust = 1.0f;

			if ( GetHud().m_flMouseSensitivityFactor )
			{
				GetHud().m_flMouseSensitivity = sensitivity.GetFloat() * GetHud().m_flMouseSensitivityFactor;
			}
			else
			{
				// No override, don't use huge sensitivity
				if ( bZoomed == false /*&& bScoped == false*/ )
				{
					// reset to saved sensitivity
					GetHud().m_flMouseSensitivity = 0;
				}
				else
				{  
					// Set a new sensitivity that is proportional to the change from the FOV default and scaled
					//  by a separate compensating factor
					if ( iDefaultFOV == 0 )
					{
						contract_assert( 0 ); // would divide by zero, something is broken with iDefatulFOV
						iDefaultFOV = 1;
					}
					float zoomSensitivity = zoom_sensitivity_ratio_mouse.GetFloat();
					if( input->ControllerModeActive() )
					{
						zoomSensitivity = zoom_sensitivity_ratio_joystick.GetFloat();
					}

					GetHud().m_flFOVSensitivityAdjust = 
						((float)localFOV / (float)iDefaultFOV) * // linear fov downscale
						zoomSensitivity; // sensitivity scale factor
					GetHud().m_flMouseSensitivity = GetHud().m_flFOVSensitivityAdjust * sensitivity.GetFloat(); // regular sensitivity
				}
			}
		}
	}

	// Setup the frustum cache for this frame.
	m_bAllowViewAccess = true;
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( iSlot )
	{
		const CViewSetup &view = GetView( iSlot );
		FrustumCache()->Add( &view, iSlot );
	}
	FrustumCache()->SetUpdated();
	m_bAllowViewAccess = false;

	IterateRemoteSplitScreenViewSlots_Pop();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : const CViewSetup
//-----------------------------------------------------------------------------
const CViewSetup *CViewRender::GetViewSetup( void ) const
{
	return &m_CurrentView;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : const CViewSetup
//-----------------------------------------------------------------------------
const CViewSetup *CViewRender::GetPlayerViewSetup( int nSlot /*= -1*/ ) const
{   
	// NOTE:  This code path doesn't require m_bAllowViewAccess == true!!!
	if ( nSlot == -1 )
	{
		ASSERT_LOCAL_PLAYER_RESOLVABLE();
		return &m_UserView[ GET_ACTIVE_SPLITSCREEN_SLOT() ];
	}
	return &m_UserView[ nSlot ];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CViewRender::DisableVis( void )
{
	m_bForceNoVis = true;
}

//-----------------------------------------------------------------------------
// Gets znear + zfar
//-----------------------------------------------------------------------------
float CViewRender::GetZNear()
{

	// Compute aspect ratio
	bool bMegaWideAspectRatio = ( ( float( m_CurrentView.width ) / float( m_CurrentView.height + 1 ) ) > 2.0f );
	if ( !bMegaWideAspectRatio )
	{
		static ConVarRef r_aspectratio( "r_aspectratio" );
		if ( r_aspectratio.GetFloat() > 2.0f )	// If client is overriding aspect ratio then force znear as well
			bMegaWideAspectRatio = true;
	}
	return bMegaWideAspectRatio ? 1 : VIEW_NEARZ;
}

float CViewRender::GetZFar()
{
	// Initialize view structure with default values
	float farZ;
	if ( r_farz.GetFloat() < 1 )
	{
		// Use the far Z from the map's parameters.
		farZ = r_mapextents.GetFloat() * 1.73205080757f;
		
		CBasePlayer *pPlayer = CBasePlayer::GetLocalPlayer();
		if( pPlayer && pPlayer->GetFogParams() )
		{
			if ( pPlayer->GetFogParams()->farz > 0 )
			{
				farZ = pPlayer->GetFogParams()->farz;
			}
		}
	}
	else
	{
		farZ = r_farz.GetFloat();
	}

	return farZ;
}

//-----------------------------------------------------------------------------
// Sets up the view parameters
//-----------------------------------------------------------------------------
void CViewRender::SetUpView()
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

	m_bAllowViewAccess = true;
	CVProfScope _( "CViewRender::SetUpView", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	// Initialize view structure with default values
	float farZ = GetZFar();

	CViewSetup &view = GetView();

	view.zFar				= farZ;
	view.zFarViewmodel	= farZ;
	// UNDONE: Make this farther out? 
	//  closest point of approach seems to be view center to top of crouched box
	view.zNear			= GetZNear();
	view.zNearViewmodel	= 1;
	view.fov				= default_fov.GetFloat();

	view.m_bOrtho			= false;

	// Enable spatial partition access to edicts
	::partition->SuppressLists( PARTITION_ALL_CLIENT_EDICTS, false );

	CBasePlayer *pPlayer = CBasePlayer::GetLocalPlayer();

	bool bNoViewEnt = false;
	if( pPlayer == nullptr )
	{
		pPlayer = GetSplitScreenViewPlayer( nSlot );
		bNoViewEnt = true;
	}

	if ( g_bEngineIsHLTV )
	{
		HLTVCamera()->CalcView( &view );
	}
	else
	{
		// FIXME: Are there multiple views? If so, then what?
		// FIXME: What happens when there's no player?
		if (pPlayer)
		{
			pPlayer->CalcView( view.origin, view.angles, view.zNear, view.zFar, view.fov );

			// If we are looking through another entities eyes, then override the angles/origin for GetView()
			int viewentity = render->GetViewEntity();

			if ( !bNoViewEnt && !g_nKillCamMode && (pPlayer->entindex() != viewentity) )
			{
				CBaseEntity *ve = cl_entitylist->GetEnt( viewentity );
				if ( ve )
				{
					VectorCopy( ve->GetAbsOrigin(), view.origin );
					VectorCopy( ve->GetAbsAngles(), view.angles );

					GetViewEffects()->ApplyShake( view.origin, view.angles, 1.0 );
				}
			}

			pPlayer->CalcViewModelView( view.origin, view.angles );

			// Is this the proper place for this code?
			if ( cl_camera_follow_bone_index.GetInt() >= -1 && input->CAM_IsThirdPerson() )
			{
				VectorCopy( g_cameraFollowPos, view.origin );
			}
		}

		// Even if the engine is paused need to override the view
		// for keeping the camera control during pause.
		GetClientMode()->OverrideView( &GetView() );
	}

	CBasePlayer *pCameraMan = nullptr;
	if ( g_bEngineIsHLTV )
		pCameraMan = HLTVCamera()->GetCameraMan();

	CCSPlayer *pCSPlayer = ( g_bEngineIsHLTV && pCameraMan ) ? static_cast< CCSPlayer* >( pCameraMan ) : CCSPlayer::GetLocalCSPlayer();
	if ( pCSPlayer && pCSPlayer->ShouldInterpolateObserverChanges() )
	{
		pCSPlayer->InterpolateObserverView( view.origin, view.angles );
		pCSPlayer->CalcViewModelView( view.origin, view.angles );
	}

	if ( engine->IsPlayingDemo() )
	{
		if ( cl_demoviewoverride.GetFloat() > 0.0f )
		{
			// Retreive view angles from engine ( could have been set in IN_AdjustAngles above )
			CalcDemoViewOverride( view.origin, view.angles );
		}
		else
		{
			s_DemoView = view.origin;
			s_DemoAngle = view.angles;
		}
	}

	// Disable spatial partition access
	::partition->SuppressLists( PARTITION_ALL_CLIENT_EDICTS, true );

	//Find the offset our current FOV is from the default value
	float flFOVOffset = pPlayer ? ( pPlayer->GetDefaultFOV() - view.fov ) : 0.0f;

	//Adjust the viewmodel's FOV to move with any FOV offsets on the viewer's end
	view.fovViewmodel = GetClientMode()->GetViewModelFOV() - flFOVOffset;

	// Compute the world->main camera transform
	ComputeCameraVariables( view.origin, view.angles, 
		&g_vecVForward[ nSlot ], &g_vecVRight[ nSlot ], &g_vecVUp[ nSlot ], &g_matCamInverse[ nSlot ] );

	// set up the hearing origin...
	AudioState_t audioState;
	audioState.m_Origin = view.origin;
	audioState.m_Angles = view.angles;
	audioState.m_bIsUnderwater = pPlayer && pPlayer->AudioStateIsUnderwater( view.origin );

	view.origin = audioState.m_Origin;
	view.angles = audioState.m_Angles;

	GetClientMode()->OverrideAudioState( &audioState );
	engine->SetAudioState( audioState );

	g_vecPrevRenderOrigin[ nSlot ] = g_vecRenderOrigin[ nSlot ];
	g_vecPrevRenderAngles[ nSlot ] = g_vecRenderAngles[ nSlot ];
	g_vecRenderOrigin[ nSlot ] = view.origin;
	g_vecRenderAngles[ nSlot ] = view.angles;

	m_bAllowViewAccess = false;
}

void CViewRender::WriteSaveGameScreenshotOfSize( const char *pFilename, int width, int height )
{
	g_bRenderingScreenshot = true;
	m_bAllowViewAccess = true;

	{
		CMatRenderContextPtr pRenderContext( materials );
		pRenderContext->MatrixMode( MATERIAL_PROJECTION );
		pRenderContext->PushMatrix();
	
		pRenderContext->MatrixMode( MATERIAL_VIEW );
		pRenderContext->PushMatrix();
				
		// Push back buffer on the stack with small viewport
		pRenderContext->PushRenderTargetAndViewport( nullptr, 0, 0, width, height );

		// render out to the backbuffer
		CViewSetup viewSetup = GetView();
		viewSetup.x = 0;
		viewSetup.y = 0;
		viewSetup.width = width;
		viewSetup.height = height;
		viewSetup.fov = ScaleFOVByWidthRatio( GetView().fov, ( (float)width / (float)height ) / ( 4.0f / 3.0f ) );
		viewSetup.m_bRenderToSubrectOfLargerScreen = true;

		float flSavedBlurFade = GetClientMode()->GetBlurFade();
		GetClientMode()->SetBlurFade( 0.0f );
	
		// draw out the scene
		// Don't draw the HUD or the viewmodel
		// Note: The original code here just cleared DEPTH and COLOR, but this was somehow failing to clear the depth buffer before the Z-Prepass in non-water views on X360, corrupting the screenshot.
		RenderView( viewSetup, viewSetup, VIEW_CLEAR_DEPTH | VIEW_CLEAR_STENCIL, 0 );

		GetClientMode()->SetBlurFade( flSavedBlurFade );

		// restore our previous state
		pRenderContext->PopRenderTargetAndViewport();

		pRenderContext->MatrixMode( MATERIAL_PROJECTION );
		pRenderContext->PopMatrix();

		pRenderContext->MatrixMode( MATERIAL_VIEW );
		pRenderContext->PopMatrix();

		pRenderContext->AntiAliasingHint( AA_HINT_MESHES );
	}
	
	// FIXME: Disable material system threading, to better emulate how regular screenshots are made.
	// The API requires us to know the nMaterialSystemThread index, which is only available in engine, and I don't 
	// want to modify IMaterialSystem just to get savegame screenshots to work. Find another way.
	int nMaterialSystemThread = 0;
	if ( CommandLine()->FindParm( "-swapcores" ) )
	{
		nMaterialSystemThread = 1;
	}
	bool bThreadingEnabled = materials->AllowThreading( false, nMaterialSystemThread );
		
	// get the data from the backbuffer and save to disk
	// bitmap bits
	unsigned char *pImage = ( unsigned char * )MemAlloc_Alloc( width * 3 * height );

	{
		CMatRenderContextPtr pRenderContext( materials );

		// Get Bits from the material system
		pRenderContext->ReadPixels( 0, 0, width, height, pImage, IMAGE_FORMAT_RGB888 );
	}

	materials->AllowThreading( bThreadingEnabled, nMaterialSystemThread );

	// allocate a buffer to write the tga into
	int iMaxTGASize = 1024 + (width * height * 4);
	void *pTGA = new char[ iMaxTGASize ];
	ks::byte_writer buffer( pTGA, iMaxTGASize );

	if( !TGAWriter::WriteToBuffer( pImage, buffer, width, height, IMAGE_FORMAT_RGB888, IMAGE_FORMAT_RGB888 ) )
	{
		Error( "Couldn't write bitmap data snapshot.\n" );
	}
	
	g_pMemAlloc->Free( pImage );

	// async write to disk (this will take ownership of the memory)
	char szPathedFileName[k_cMaxPath];
	V_snprintf( szPathedFileName, sizeof(szPathedFileName), "//MOD/%s", pFilename );

	filesystem->AsyncWrite( szPathedFileName, buffer.data(), buffer.size(), true );

	g_bRenderingScreenshot = false;
	m_bAllowViewAccess = false;
}

//-----------------------------------------------------------------------------
// Purpose: takes a screenshot of the save game
//-----------------------------------------------------------------------------
void CViewRender::WriteSaveGameScreenshot( const char *pFilename )
{
	WriteSaveGameScreenshotOfSize( pFilename, SAVEGAME_SCREENSHOT_WIDTH, SAVEGAME_SCREENSHOT_HEIGHT );
}

float ScaleFOVByWidthRatio( float fovDegrees, float ratio )
{
	float halfAngleRadians = fovDegrees * ( 0.5f * std::numbers::pi / 180.0f );
	float t = std::tan( halfAngleRadians );
	t *= ratio;
	float retDegrees = ( 180.0f / std::numbers::pi ) * std::atan( t );
	return retDegrees * 2.0f;
}

//-----------------------------------------------------------------------------
// Purpose: Sets view parameters for level overview mode
// Input  : *rect - 
//-----------------------------------------------------------------------------
void CViewRender::SetUpOverView()
{
	static int oldCRC = 0;

	GetView().m_bOrtho = true;

	float aspect = (float)GetView().width/(float)GetView().height;

	int size_y = 0;

	if( cl_leveloverview.GetInt() < 0 )
	{
		// Fit the level into ortho view bounds
		float fWorldWidth = GetClientWorldEntity()->m_WorldMaxs.x - GetClientWorldEntity()->m_WorldMins.x;
		float fWorldHeight = GetClientWorldEntity()->m_WorldMaxs.y - GetClientWorldEntity()->m_WorldMins.y;
		if( fWorldWidth / aspect < fWorldHeight )
		{
			size_y = 1.05f * fWorldHeight;
		}
		else
		{
			size_y = ( 1.05f * fWorldWidth ) / aspect;
			if( size_y < fWorldHeight )
			{
				Msg("Bad news bears!\n");
			}

		}
		GetView().origin.x = GetClientWorldEntity()->m_WorldMins.x + fWorldWidth / 2;
		GetView().origin.y = GetClientWorldEntity()->m_WorldMins.y + fWorldHeight / 2;
		GetView().origin.z = GetClientWorldEntity()->GetAbsOrigin().z + GetClientWorldEntity()->m_WorldMaxs.z;
	}
	else
	{
		size_y = 1024.0f * cl_leveloverview.GetFloat(); // scale factor, 1024 = OVERVIEW_MAP_SIZE
	}

	int	size_x = size_y * aspect;	// standard screen aspect 
	GetView().origin.x -= size_x / 2;
	GetView().origin.y += size_y / 2;

	GetView().m_OrthoLeft   = 0;
	GetView().m_OrthoTop    = -size_y;
	GetView().m_OrthoRight  = size_x;
	GetView().m_OrthoBottom = 0;

	GetView().angles = QAngle( 90, 90, 0 );

	// simple movement detector, show position if moved
	int newCRC = GetView().origin.x + GetView().origin.y + GetView().origin.z;
	if ( newCRC != oldCRC )
	{
		Msg( "Overview: scale %.2f, pos_x %.0f, pos_y %.0f\n", cl_leveloverview.GetFloat(),
			GetView().origin.x, GetView().origin.y );
		oldCRC = newCRC;
	}

	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->ClearColor4ub( 0, 255, 0, 255 );

	// render->DrawTopView( true );
}

void CViewRender::SetUpChaseOverview()
{
// 	CBasePlayer *pPlayer = UTIL_PlayerByIndex( m_iTarget1 );
// 
// 	if ( !pPlayer )
// 		return;

//	GetView().origin = m_vCamOrigin;
//	GetView().angles = m_aCamAngle;
//	GetView().fov = m_flFOV;

	float flTransTime = 1.0f;
	if ( m_flNextIdealOverviewScaleUpdate < gpGlobals->curtime )
	{
		m_flOldChaseOverviewScale = m_flIdealChaseOverviewScale;
		m_flIdealChaseOverviewScale = HLTVCamera()->GetIdealOverviewScale();	
		m_flNextIdealOverviewScaleUpdate = gpGlobals->curtime + flTransTime;
	}

	float flInterp = clamp( ( gpGlobals->curtime - (m_flNextIdealOverviewScaleUpdate - flTransTime) ) / flTransTime, 0.0f, 1.0f );
	float flScale = m_flOldChaseOverviewScale + ((m_flIdealChaseOverviewScale-m_flOldChaseOverviewScale) * flInterp);

	GetView().m_bOrtho = true;
	float aspect = (float)GetView().width/(float)GetView().height;

	int size_y = 1024.0f * flScale; // scale factor, 1024 = OVERVIEW_MAP_SIZE
	int	size_x = size_y * aspect;	// standard screen aspect 
	GetView().origin.x -= size_x / 2;
	GetView().origin.y += size_y / 2;
	GetView().angles = QAngle( 90, 90, 0 );

	GetView().m_OrthoLeft   = 0;
	GetView().m_OrthoTop    = -size_y;
	GetView().m_OrthoRight  = size_x;
	GetView().m_OrthoBottom = 0;

	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->ClearColor4ub( 0, 255, 0, 255 );
}

//-----------------------------------------------------------------------------
// Purpose: Render current view into specified rectangle
// Input  : *rect - 
//-----------------------------------------------------------------------------
ConVar ss_debug_draw_player( side_registry(), "ss_debug_draw_player", "-1", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );
void CViewRender::Render( vrect_t *rect )
{
	CVProfScope _( "CViewRender::Render", 0, "CViewRender::Render", false, BUDGETFLAG_OTHER );

	m_bAllowViewAccess = true;

	engine->EngineStats_BeginFrame();

	// Assume normal vis
	m_bForceNoVis			= false;

	float flViewportScale = mat_viewportscale.GetFloat();

	vrect_t engineRect = *rect;

	IterateRemoteSplitScreenViewSlots_Push( true );
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD_UI( hh );

		CViewSetup &view = GetView( hh );

		float engineAspectRatio = engine->GetScreenAspectRatio( view.width, view.height );

		// Using this API gives us a chance to "inset" the 3d views as needed for splitscreen
		int insetX, insetY;
		UI_GetEngineRenderBounds( hh, view.x, view.y, view.width, view.height, insetX, insetY );
			
		float aspectRatio = engineAspectRatio * 0.75f;	 // / (4/3)
		view.fov = ScaleFOVByWidthRatio( view.fov,  aspectRatio );
		view.fovViewmodel = ScaleFOVByWidthRatio( view.fovViewmodel, aspectRatio );

		// Let the client mode hook stuff.
		GetClientMode()->PreRender( &view );
		GetClientMode()->AdjustEngineViewport( view.x, view.y, view.width, view.height );

		view.m_nUnscaledX = view.x;
		view.m_nUnscaledY = view.y;
		view.m_nUnscaledWidth = view.width;
		view.m_nUnscaledHeight = view.height;

		view.width *= flViewportScale;
		view.height *= flViewportScale;

		view.m_flAspectRatio = ( engineAspectRatio > 0.0f ) ? engineAspectRatio : ( (float)view.width / (float)view.height );

		int nClearFlags = VIEW_CLEAR_DEPTH | VIEW_CLEAR_STENCIL;

		if ( gl_clear_randomcolor.GetBool() )
		{
			CMatRenderContextPtr pRenderContext( materials );
			pRenderContext->ClearColor3ub( RandomInt(0, 255), RandomInt(0, 255), RandomInt(0, 255) );
			pRenderContext->ClearBuffers( true, false, false );
			pRenderContext->Release();
		}
		else if ( gl_clear.GetBool() )
		{
			nClearFlags |= VIEW_CLEAR_COLOR;
		}

		// Determine if we should draw view model ( client mode override )
		bool drawViewModel = GetClientMode()->ShouldDrawViewModel();
		// Apply any player specific overrides
		CBasePlayer *pPlayer = CBasePlayer::GetLocalPlayer();
		if ( pPlayer )
		{
			// Override view model if necessary
			if ( !pPlayer->m_Local.m_bDrawViewmodel )
			{
				drawViewModel = false;
			}
		}

		if ( cl_leveloverview.GetInt() != 0 || input->CAM_IsThirdPersonOverview() )
		{
			if ( cl_leveloverview.GetInt() != 0 )
				SetUpOverView();
			else
				SetUpChaseOverview();

			nClearFlags |= VIEW_CLEAR_COLOR;
			drawViewModel = false;
		}

		render->SetMainView( view.origin, view.angles );

		int flags = (pPlayer == nullptr) ? 0 : RENDERVIEW_DRAWHUD;
		if ( drawViewModel )
		{
			flags |= RENDERVIEW_DRAWVIEWMODEL;
		}

		// This is the hook for per-split screen player views
		CBaseEntity::PreRenderEntities( hh );

		if ( ( ss_debug_draw_player.GetInt() < 0 ) || ( hh == ss_debug_draw_player.GetInt() ) )
		{
			CViewSetup hudViewSetup;
			UI_GetHudBounds( hh, hudViewSetup.x, hudViewSetup.y, hudViewSetup.width, hudViewSetup.height );
			RenderView( view, hudViewSetup, nClearFlags, flags );
		}

		GetClientMode()->PostRender();
	}
	IterateRemoteSplitScreenViewSlots_Pop();

	engine->EngineStats_EndFrame();

	// Render the new-style embedded UI
	// TODO: when embedded UI will be used for HUD, we will need it to maintain
	// a separate screen for HUD and a separate screen stack for pause menu & main menu.
	// for now only render embedded UI in pause menu & main menu

	// Draw all of the UI stuff "fullscreen"
	{
		{
			CMatRenderContextPtr pRenderContext( materials );
#if PIX_ENABLE
			{
				( pRenderContext, "VGui UI" );
			}
#endif

			CViewSetup view2d;
			view2d.x				= rect->x;
			view2d.y				= rect->y;
			view2d.width			= rect->width;
			view2d.height			= rect->height;
			render->Push2DView( pRenderContext, view2d, 0, nullptr, GetFrustum() );
			{
				// The engine here is trying to access CurrentView() etc. which is bogus
				ACTIVE_SPLITSCREEN_PLAYER_GUARD( 0 );
				render->PopView( pRenderContext, GetFrustum() );
			}
			pRenderContext->Flush();
			pRenderContext.SafeRelease();
		}
	}

	m_bAllowViewAccess = false;
}

static void GetPos( const CCommand &args, Vector &vecOrigin, QAngle &angles )
{
	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
	vecOrigin = MainViewOrigin(nSlot);
	angles = MainViewAngles(nSlot);
	if ( ( args.ArgC() == 2 && atoi( args[1] ) == 2 ) || FStrEq( args[0], "getpos_exact" ) )
	{
		CBasePlayer *pPlayer = CBasePlayer::GetLocalPlayer();
		if ( pPlayer )
		{
			vecOrigin = pPlayer->GetAbsOrigin();
			angles = pPlayer->GetAbsAngles();
		}
	}
}

static void spec_pos( const CCommand &args );
static ConCommand spec_pos_command( side_registry(), "spec_pos", spec_pos, "dump position and angles to the console" );
static void spec_pos( const CCommand &args )
{
	Vector vecOrigin;
	QAngle angles;
	GetPos( args, vecOrigin, angles );
	Warning( "%.1f %.1f %.1f %.1f %.1f\n", vecOrigin.x, vecOrigin.y, 
		vecOrigin.z, angles.x, angles.y );
}

static void getpos( const CCommand &args );
static ConCommand getpos_command( side_registry(), "getpos", getpos, "dump position and angles to the console" );
static void getpos( const CCommand &args )
{
	Vector vecOrigin;
	QAngle angles;
	GetPos( args, vecOrigin, angles );

	const char *pCommand1 = "setpos";
	const char *pCommand2 = "setang";
	if ( ( args.ArgC() == 2 && atoi( args[1] ) == 2 ) || FStrEq( args[0], "getpos_exact" ) )
	{
		pCommand1 = "setpos_exact";
		pCommand2 = "setang_exact";
	}

	Warning( "%s %f %f %f;", pCommand1, vecOrigin.x, vecOrigin.y, vecOrigin.z );
	Warning( "%s %f %f %f\n", pCommand2, angles.x, angles.y, angles.z );
}

ConCommand getpos_exact( side_registry(), "getpos_exact", getpos, "dump origin and angles to the console" );

static void PlayDistance_Callback( ConVar *pConVar, const char *pOldString, float flOldValue )
{
	// diabled for now, reenable if we have specific things we need to change for "play distance"
/*
	ConVarRef var( pConVar );

	if ( var.GetFloat() == 1  )
	{
		// 2ft							
		ConVarRef m_hudscaling( "hud_scaling" );
		m_hudscaling.SetValue( 0.75f );

		ConVarRef m_vmfov( "viewmodel_fov" );
		m_vmfov.SetValue( 60 );

		ConVarRef m_vmoffsetx( "viewmodel_offset_x" );
		m_vmoffsetx.SetValue( 1 );

		ConVarRef m_vmoffsety( "viewmodel_offset_y" );
		m_vmoffsety.SetValue( 1 );

		ConVarRef m_vmoffsetz( "viewmodel_offset_z" );
		m_vmoffsetz.SetValue( -1 );
	}
	else if ( var.GetFloat() == 2  )
	{
		//10ft
		ConVarRef m_hudscaling( "hud_scaling" );
		m_hudscaling.SetValue( 1.0f );

		ConVarRef m_vmfov( "viewmodel_fov" );
		m_vmfov.SetValue( 54 );

		ConVarRef m_vmoffsetx( "viewmodel_offset_x" );
		m_vmoffsetx.SetValue( 0 );

		ConVarRef m_vmoffsety( "viewmodel_offset_y" );
		m_vmoffsety.SetValue( 0 );

		ConVarRef m_vmoffsetz( "viewmodel_offset_z" );
		m_vmoffsetz.SetValue( 0 );
	}
	else
	{
		Warning( "Valid options: 1 = 2ft, 2 = 10ft.\n" );
	}
*/
}

static ConVar play_distance( side_registry(), "play_distance", "1", FCVAR_ARCHIVE | FCVAR_ARCHIVE_GAMECONSOLE, "Set to 1:\"2 foot\" or 2:\"10 foot\" presets.", PlayDistance_Callback );

/*
static void HudPresetSize_Callback( ConVar *pConVar, const char *pOldString, float flOldValue )
{
	ConVarRef var( pConVar );

	if ( var.GetFloat() == 1  )
	{
		// 2ft							
		ConVarRef m_hudscaling( "hud_scaling" );
		m_hudscaling.SetValue( 0.75f );
	}
	else if ( var.GetFloat() == 2  )
	{
		//10ft
		ConVarRef m_hudscaling( "hud_scaling" );
		m_hudscaling.SetValue( 0.85f );
	}
	else if ( var.GetFloat() == 3  )
	{
		//10ft
		ConVarRef m_hudscaling( "hud_scaling" );
		m_hudscaling.SetValue( 1.0f );
	}
	else
	{
		Warning( "Valid options: 1 = 2ft, 2 = 10ft.\n" );
	}

}

static ConVar play_distance( side_registry(), "play_distance", "2", FCVAR_ARCHIVE | FCVAR_ARCHIVE_GAMECONSOLE, "1:\"Small\", 2:\"Medium\", 3:\"Couch\" ", HudPresetSize_Callback );
*/

static void ViewmodelPresetPos_Callback( ConVar *pConVar, const char *pOldString, float flOldValue )
{
	ConVarRef var( pConVar );

	if ( var.GetFloat() == 1  )
	{
		ConVarRef m_vmfov( "viewmodel_fov" );
		m_vmfov.SetValue( 60 );

		ConVarRef m_vmoffsetx( "viewmodel_offset_x" );
		m_vmoffsetx.SetValue( 1 );

		ConVarRef m_vmoffsety( "viewmodel_offset_y" );
		m_vmoffsety.SetValue( 1 );

		ConVarRef m_vmoffsetz( "viewmodel_offset_z" );
		m_vmoffsetz.SetValue( -1 );
	}
	else if ( var.GetFloat() == 2  )
	{
		ConVarRef m_vmfov( "viewmodel_fov" );
		m_vmfov.SetValue( 54 );

		ConVarRef m_vmoffsetx( "viewmodel_offset_x" );
		m_vmoffsetx.SetValue( 0 );

		ConVarRef m_vmoffsety( "viewmodel_offset_y" );
		m_vmoffsety.SetValue( 0 );

		ConVarRef m_vmoffsetz( "viewmodel_offset_z" );
		m_vmoffsetz.SetValue( 0 );
	}
	else if ( var.GetFloat() == 3  )
	{
		ConVarRef m_vmfov( "viewmodel_fov" );
		m_vmfov.SetValue( 68 );

		ConVarRef m_vmoffsetx( "viewmodel_offset_x" );
		m_vmoffsetx.SetValue( 2.5f );

		ConVarRef m_vmoffsety( "viewmodel_offset_y" );
		m_vmoffsety.SetValue( 0 );

		ConVarRef m_vmoffsetz( "viewmodel_offset_z" );
		m_vmoffsetz.SetValue( -1.5f );
	}
	else
	{
		Warning( "Valid options: 1 = Desktop, 2 = Couch, 3 = Classic.\n" );
	}

}

static ConVar viewmodel_presetpos( side_registry(), "viewmodel_presetpos", "1", FCVAR_ARCHIVE | FCVAR_ARCHIVE_GAMECONSOLE, "1:\"Desktop\", 2:\"Couch\", 3:\"Classic\" ", ViewmodelPresetPos_Callback );

#undef GPU_RESOLVE_ALIGNMENT
#undef USE_MONITORS

struct BeamTrail_t;
class Beam_t;
class CBaseAnimating;
class CBaseEntity;
class CBaseViewModel;
class CBeam;

using namespace ::ks::reflect;

ConVar r_DrawBeams( side_registry(), "r_DrawBeams", "1", FCVAR_CHEAT, "0=Off, 1=Normal, 2=Wireframe" );

bool g_BeamCreationAllowed = false;

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : state - 
//-----------------------------------------------------------------------------
void SetBeamCreationAllowed( bool state )
{
	g_BeamCreationAllowed = state;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool BeamCreationAllowed( void )
{
	return g_BeamCreationAllowed;
}

//-----------------------------------------------------------------------------
// Purpose: Implements beam rendering apis
//-----------------------------------------------------------------------------
class CViewRenderBeams : public IViewRenderBeams
{
// Construction
public:
						CViewRenderBeams( void );
	virtual				~CViewRenderBeams( void );

// Implement IViewRenderBeams
public:
	virtual	void		InitBeams( void );
	virtual	void		ShutdownBeams( void );
	virtual	void		ClearBeams( void );

	// Updates the state of the temp ent beams
	virtual void		UpdateTempEntBeams();

	virtual void		DrawBeam( CBeam* pbeam, const RenderableInstance_t &instance, ITraceFilter *pEntityBeamTraceFilter = nullptr );
	virtual void		DrawBeam( Beam_t *pbeam );

	virtual	void		KillDeadBeams( CBaseEntity *pDeadEntity );

	virtual Beam_t		*CreateBeamEnts( BeamInfo_t &beamInfo );
	virtual Beam_t		*CreateBeamEntPoint( BeamInfo_t &beamInfo );
	virtual	Beam_t		*CreateBeamPoints( BeamInfo_t &beamInfo );
	virtual Beam_t		*CreateBeamRing( BeamInfo_t &beamInfo );
	virtual Beam_t		*CreateBeamRingPoint( BeamInfo_t &beamInfo );
	virtual Beam_t		*CreateBeamCirclePoints( BeamInfo_t &beamInfo );
	virtual Beam_t		*CreateBeamFollow( BeamInfo_t &beamInfo );

	virtual	void		CreateBeamEnts( int startEnt, int endEnt, int modelIndex, int haloIndex, float haloScale, 
							float life, float width, float endWidth, float fadeLength, float amplitude, 
							float brightness, float speed, int startFrame, 
							float framerate, float r, float g, float b, int type = -1 );
	virtual	void		CreateBeamEntPoint( int	nStartEntity, const Vector *pStart, int nEndEntity, const Vector* pEnd,
							int modelIndex, int haloIndex, float haloScale, 
							float life, float width, float endWidth, float fadeLength, float amplitude, 
							float brightness, float speed, int startFrame, 
							float framerate, float r, float g, float b );
	virtual	void		CreateBeamPoints( Vector& start, Vector& end, int modelIndex, int haloIndex, float haloScale,   
							float life, float width, float endWidth, float fadeLength, float amplitude, 
							float brightness, float speed, int startFrame, 
							float framerate, float r, float g, float b );
	virtual	void		CreateBeamRing( int startEnt, int endEnt, int modelIndex, int haloIndex, float haloScale,   
							float life, float width, float endWidth, float fadeLength, float amplitude, 
							float brightness, float speed, int startFrame, 
							float framerate, float r, float g, float b, int flags );
	virtual void		CreateBeamRingPoint( const Vector& center, float start_radius, float end_radius, int modelIndex, int haloIndex, float haloScale,   
							float life, float width, float m_nEndWidth, float m_nFadeLength, float amplitude, 
							float brightness, float speed, int startFrame, 
							float framerate, float r, float g, float b, int flags );
	virtual	void		CreateBeamCirclePoints( int type, Vector& start, Vector& end, 
							int modelIndex,  int haloIndex,  float haloScale, float life, float width, 
							float endWidth, float fadeLength, float amplitude, float brightness, float speed, 
							int startFrame, float framerate, float r, float g, float b );
	virtual	void		CreateBeamFollow( int startEnt, int modelIndex, int haloIndex, float haloScale,   
							float life, float width, float endWidth, float fadeLength, float r, float g, float b, 
							float brightness );

	virtual void		FreeBeam( Beam_t *pBeam ) { BeamFree( pBeam ); }
	virtual void		UpdateBeamInfo( Beam_t *pBeam, BeamInfo_t &beamInfo );

private:
	void					FreeDeadTrails( BeamTrail_t **trail );
	void					UpdateBeam( Beam_t *pbeam, float frametime, CBeam *pcbeam = nullptr );
	void					DrawBeamWithHalo( Beam_t* pbeam,int frame,int rendermode,float *color, float *srcColor, const model_t *sprite,const model_t *halosprite, float flHDRColorScale );
	void					DrawBeamFollow( const model_t* pSprite, Beam_t *pbeam, int frame, int rendermode, float frametime, const float* color, float flHDRColorScale = 1.0f );
	void					DrawLaser( Beam_t* pBeam, int frame, int rendermode, float* color, model_t const* sprite, model_t const* halosprite, float flHDRColorScale = 1.0f );
	void					DrawTesla( Beam_t* pBeam, int frame, int rendermode, float* color, model_t const* sprite, float flHDRColorScale = 1.0f );

	bool					RecomputeBeamEndpoints( Beam_t *pbeam );

	int						CullBeam( const Vector &start, const Vector &end, int pvsOnly );

	// special case clipping to geometry behavior
	void					ClipBeam( CBeam *pcbeam, Beam_t *pbeamt );

	// Creation
	Beam_t					*CreateGenericBeam( BeamInfo_t &beamInfo );
	void					SetupBeam( Beam_t *pBeam, const BeamInfo_t &beamInfo );
	void					SetBeamAttributes( Beam_t *pBeam, const BeamInfo_t &beamInfo );
	
	// Memory Alloc/Free
	Beam_t*					BeamAlloc( bool bRenderable );
	void					BeamFree( Beam_t* pBeam );

// DATA
private:
	enum
	{

		// default max # of particles at one time
		DEFAULT_PARTICLES	= 2048,

		// no fewer than this no matter what's on the command line
		MIN_PARTICLES		= 512,	

		// Maximum length of the free list.
		BEAM_FREELIST_MAX	= 32
	};

	Beam_t					*m_pActiveBeams;
	Beam_t					*m_pFreeBeams;
	int						m_nBeamFreeListLength;

	BeamTrail_t				*m_pBeamTrails;
	BeamTrail_t				*m_pActiveTrails;
	BeamTrail_t				*m_pFreeTrails;
	int						m_nNumBeamTrails;
};

// Expose interface to rest of client .dll
static CViewRenderBeams s_ViewRenderBeams;
IViewRenderBeams *beams = ( IViewRenderBeams * )&s_ViewRenderBeams;

CUniformRandomStream beamRandom;
//-----------------------------------------------------------------------------
// Global methods
//-----------------------------------------------------------------------------

//		freq2 += step * 0.1;
// Fractal noise generator, power of 2 wavelength
static void Noise( float *noise, int divs, float scale )
{
	int div2;
	
	div2 = divs >> 1;

	if ( divs < 2 )
		return;

	// Noise is normalized to +/- scale
	noise[ div2 ] = (noise[0] + noise[divs]) * 0.5 + scale * beamRandom.RandomFloat(-1, 1);
	if ( div2 > 1 )
	{
		Noise( &noise[div2], div2, scale * 0.5 );
		Noise( noise, div2, scale * 0.5 );
	}
}

static void SineNoise( float *noise, int divs )
{
	int i;
	float freq;
	float step = std::numbers::pi / (float)divs;

	freq = 0;
	for ( i = 0; i < divs; i++ )
	{
		noise[i] = std::sin( freq );
		freq += step;
	}
}

bool ComputeBeamEntPosition( CBaseEntity *pEnt, int nAttachment, bool bInterpretAttachmentIndexAsHitboxIndex, Vector& pt )
{
	// NOTE: This will *leave* the pt at its current value, essential for
	// beam follow ents what want to stick around a little after their ent has died
	if (!pEnt)
		return false;

	if ( !bInterpretAttachmentIndexAsHitboxIndex )
	{
		if ( pEnt->GetAttachment( nAttachment, pt ) )
			return true;
	}
	else
	{
		CBaseAnimating *pAnimating = pEnt->GetBaseAnimating();
		if ( pAnimating )
		{
			studiohdr_t *pStudioHdr = modelinfo->GetStudiomodel( pAnimating->GetModel() );
			if (pStudioHdr)
			{
				mstudiohitboxset_t *set = pStudioHdr->pHitboxSet( pAnimating->GetHitboxSet() );
				if ( set && (set->numhitboxes >= nAttachment) && (nAttachment > 0) )
				{
					matrix3x4_t	*hitboxbones[MAXSTUDIOBONES];
					if ( pAnimating->HitboxToWorldTransforms( hitboxbones ) )
					{
						int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

						mstudiobbox_t *pHitbox = set->pHitbox( nAttachment - 1 );
						Vector vecViewPt = MainViewOrigin(nSlot);
						Vector vecLocalViewPt;
						VectorITransform( vecViewPt, *hitboxbones[ pHitbox->bone ], vecLocalViewPt );

						Vector vecLocalClosestPt;
						CalcClosestPointOnAABB( pHitbox->bbmin, pHitbox->bbmax, vecLocalViewPt, vecLocalClosestPt );

						VectorTransform( vecLocalClosestPt, *hitboxbones[ pHitbox->bone ], pt );

//						MatrixGetColumn( *hitboxbones[ pHitbox->bone ], 3, pt );
						return true;
					}
				}
			}
		}
	}

	// Player origins are at their feet
	if ( pEnt->IsPlayer() )
	{
		pt = pEnt->WorldSpaceCenter();
	}
	else
	{
		VectorCopy( pEnt->GetRenderOrigin(), pt );
	}

	return true;
}

//-----------------------------------------------------------------------------
//
// Methods of Beam_t
//
//-----------------------------------------------------------------------------

Beam_t::Beam_t()
{
	Reset();
}

void Beam_t::Reset()
{
	m_Mins.Init(0,0,0);
	m_Maxs.Init(0,0,0);
	type = 0;
	flags = 0;
	trail = 0;
	m_hRenderHandle = INVALID_CLIENT_RENDER_HANDLE;
	m_bCalculatedNoise = false;
	m_queryHandleHalo = nullptr;
	m_flHDRColorScale = 1.0f;
}

const Vector& Beam_t::GetRenderOrigin( void )
{
	if ((type == TE_BEAMRING) || (type == TE_BEAMRINGPOINT))
	{
		// return the center of the ring
		static Vector org;
		VectorMA( attachment[0], 0.5f, delta, org );
		return org;
	}

	return attachment[0];
}

const QAngle& Beam_t::GetRenderAngles( void )
{
	return vec3_angle;
}

const matrix3x4_t &Beam_t::RenderableToWorldTransform()
{
	static matrix3x4_t mat;
	SetIdentityMatrix( mat );
	PositionMatrix( GetRenderOrigin(), mat );
	return mat;
}

void Beam_t::GetRenderBounds( Vector& mins, Vector& maxs )
{
	VectorCopy( m_Mins, mins );
	VectorCopy( m_Maxs, maxs );
}

void Beam_t::ComputeBounds( )
{
	switch( type )
	{
	case TE_BEAMSPLINE:
		{
			// Here, we gotta look at all the attachments....
			Vector attachmentDelta;
			m_Mins.Init( 0,0,0 );
			m_Maxs.Init( 0,0,0 );

			for (int i=1; i < numAttachments; i++)
			{
				VectorSubtract( attachment[i], attachment[0], attachmentDelta );
				m_Mins = m_Mins.Min( attachmentDelta );
				m_Maxs = m_Maxs.Max( attachmentDelta );
			}
		}
		break;

	case TE_BEAMDISK:
	case TE_BEAMCYLINDER:
		{
			// FIXME: This isn't quite right for the cylinder

			// Here, delta[2] is the radius
			int radius = delta[2];
			m_Mins.Init( -radius, -radius, -radius );
			m_Maxs.Init( radius, radius, radius );
		}
		break;

	case TE_BEAMRING:
	case TE_BEAMRINGPOINT:
		{
			int radius = delta.Length() * 0.5f;
			m_Mins.Init( -radius, -radius, -radius );
			m_Maxs.Init( radius, radius, radius );
		}
		break;

	case TE_BEAMPOINTS:
	default:
		{
			// Just use the delta
			for (int i = 0; i < 3; ++i)
			{
				if (delta[i] > 0.0f)
				{
					m_Mins[i] = 0.0f;
					m_Maxs[i] = delta[i];
				}
				else
				{
					m_Mins[i] = delta[i];
					m_Maxs[i] = 0.0f;
				}
			}
		}
		break;
	}

	// Deal with beam follow
	Vector org = GetRenderOrigin();
	Vector followDelta;
	BeamTrail_t* pFollow = trail;
	while (pFollow)
	{
		VectorSubtract( pFollow->org, org, followDelta );
		m_Mins = m_Mins.Min( followDelta );
		m_Maxs = m_Maxs.Max( followDelta );

		pFollow = pFollow->next;
	}
}

bool Beam_t::ShouldDraw( void )
{
	return true;
}

extern bool g_bRenderingScreenshot;
extern ConVar r_drawviewmodel;

int Beam_t::DrawModel( int flags, const RenderableInstance_t &instance )
{
	// Tracker 16432:  If rendering a savegame screenshot don't draw beams 
	//   who have viewmodels as their attached entity
	if ( g_bRenderingScreenshot || !r_drawviewmodel.GetBool() )
	{
		// If the beam is attached
		for (int i=0;i<MAX_BEAM_ENTS;i++)
		{
			CBaseViewModel *vm = ToBaseViewModel(entity[i].Get());
			if ( vm )
			{
				return 0;
			}
		}
	}

	s_ViewRenderBeams.DrawBeam( this );
	return 0;
}

//-----------------------------------------------------------------------------
//
// Implementation of CViewRenderBeams
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Constructor, destructor: 
//-----------------------------------------------------------------------------

CViewRenderBeams::CViewRenderBeams( void ) : m_pBeamTrails(0)
{
	m_pFreeBeams = nullptr;
	m_pActiveBeams = nullptr;
	m_nBeamFreeListLength = 0;
}

CViewRenderBeams::~CViewRenderBeams( void )
{
	ClearBeams();
}

//-----------------------------------------------------------------------------
// Purpose: Initialize beam system and beam trails for follow beams
//-----------------------------------------------------------------------------
void CViewRenderBeams::InitBeams( void )
{
	int p = CommandLine()->ParmValue("-particles", -1);
	if ( p >= 0 )
	{
		m_nNumBeamTrails = std::max<int>( p, MIN_PARTICLES );
	}
	else
	{
		m_nNumBeamTrails = DEFAULT_PARTICLES;
	}
	
	m_pBeamTrails = (BeamTrail_t *)new BeamTrail_t[ m_nNumBeamTrails ];
	contract_assert( m_pBeamTrails );

	// Clear them out
	ClearBeams();
}

//-----------------------------------------------------------------------------
// Purpose: Clear out all beams
//-----------------------------------------------------------------------------
void CViewRenderBeams::ClearBeams( void )
{
	Beam_t *next = nullptr;
	for( ; m_pActiveBeams; m_pActiveBeams = next )
	{
		next = m_pActiveBeams->next;
		delete m_pActiveBeams;
	}

	for( ; m_pFreeBeams; m_pFreeBeams = next )
	{
		next = m_pFreeBeams->next;
		delete m_pFreeBeams;
	}

	m_nBeamFreeListLength = 0;

	if ( m_nNumBeamTrails )
	{
		// Also clear any particles used by beams
		m_pFreeTrails = &m_pBeamTrails[0];
		m_pActiveTrails = nullptr;

		for (int i=0 ;i<m_nNumBeamTrails ; i++)
		{
			m_pBeamTrails[i].next = &m_pBeamTrails[i+1];
		}
		m_pBeamTrails[m_nNumBeamTrails-1].next = nullptr;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Shut down beam system
//-----------------------------------------------------------------------------

void CViewRenderBeams::ShutdownBeams( void )
{
	if (m_pBeamTrails)
	{
		delete[] m_pBeamTrails;
		m_pActiveTrails = nullptr;
		m_pBeamTrails = nullptr;
		m_pFreeTrails = nullptr;
		m_nNumBeamTrails = 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Try and allocate a free beam
// Output : Beam_t
//-----------------------------------------------------------------------------
Beam_t *CViewRenderBeams::BeamAlloc( bool bRenderable )
{
	Beam_t*	pBeam = nullptr; 

	if ( m_pFreeBeams )
	{
		pBeam = m_pFreeBeams;
		m_pFreeBeams  = pBeam->next;
		m_nBeamFreeListLength--;
	}
	else
	{
		pBeam = new Beam_t();
		if( !pBeam )
		{
			DevMsg( "ERROR: failed to alloc Beam_t!\n" );
			contract_assert( pBeam );
		}
	}
	pBeam->next		= m_pActiveBeams;
	m_pActiveBeams	= pBeam;

	if ( bRenderable )
	{
		// Hook it into the rendering system...
		ClientLeafSystem()->AddRenderable( pBeam, false, RENDERABLE_IS_TRANSLUCENT, RENDERABLE_MODEL_ENTITY );
	}
	else
	{
		pBeam->m_hRenderHandle = INVALID_CLIENT_RENDER_HANDLE;
	}

	return pBeam;
}

//-----------------------------------------------------------------------------
// Purpose: Free the beam.
//-----------------------------------------------------------------------------
void CViewRenderBeams::BeamFree( Beam_t* pBeam )
{
	// Free particles that have died off.
	FreeDeadTrails( &pBeam->trail );

	// Remove it from the rendering system...
	ClientLeafSystem()->RemoveRenderable( pBeam->m_hRenderHandle );

	// Clear us out
	pBeam->Reset();

	if( m_nBeamFreeListLength < BEAM_FREELIST_MAX )
	{
		m_nBeamFreeListLength++;

		// Now link into free list;
		pBeam->next = m_pFreeBeams;
		m_pFreeBeams = pBeam;
	}
	else
	{
		delete pBeam;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Iterates through active list and kills beams associated with deadEntity
// Input  : deadEntity - 
//-----------------------------------------------------------------------------
void CViewRenderBeams::KillDeadBeams( CBaseEntity *pDeadEntity )
{
	Beam_t *pbeam;
	Beam_t *pnewlist;
	Beam_t *pnext;
	BeamTrail_t *pHead;  // Build a new list to replace m_pActiveBeams.

	pbeam    = m_pActiveBeams;  // Old list.
	pnewlist = nullptr;           // New list.
	
	while (pbeam)
	{
		pnext = pbeam->next;
		if (pbeam->entity[0] != pDeadEntity )   // Link into new list.
		{
			pbeam->next = pnewlist;
			pnewlist = pbeam;

			pbeam = pnext;
			continue;
		}

		pbeam->flags &= ~(FBEAM_STARTENTITY | FBEAM_ENDENTITY);
		if ( pbeam->type != TE_BEAMFOLLOW )
		{
			// Die Die Die!
			pbeam->die = gpGlobals->curtime - 0.1;  

			// Kill off particles
			pHead = pbeam->trail;
			while (pHead)
			{
				pHead->die = gpGlobals->curtime - 0.1;
				pHead = pHead->next;
			}

			// Free the beam
			BeamFree( pbeam );
		}
		else
		{
			// Stay active
			pbeam->next = pnewlist;
			pnewlist = pbeam;
		}
		pbeam = pnext;
	}

	// We now have a new list with the bogus stuff released.
	m_pActiveBeams = pnewlist;
}

//-----------------------------------------------------------------------------
// Purpose: Fill in values to beam structure.
//   Input: pBeam -
//          beamInfo -
//-----------------------------------------------------------------------------
void CViewRenderBeams::SetupBeam( Beam_t *pBeam, const BeamInfo_t &beamInfo )
{
	const model_t *pSprite = modelinfo->GetModel( beamInfo.m_nModelIndex );
	if ( !pSprite )
		return;

	pBeam->type				= ( beamInfo.m_nType < 0 ) ? TE_BEAMPOINTS : beamInfo.m_nType;
	pBeam->modelIndex		= beamInfo.m_nModelIndex;
	pBeam->haloIndex		= beamInfo.m_nHaloIndex;
	pBeam->haloScale		= beamInfo.m_flHaloScale;
	pBeam->frame			= 0;
	pBeam->frameRate		= 0;
	pBeam->frameCount		= modelinfo->GetModelFrameCount( pSprite );
	pBeam->freq				= gpGlobals->curtime * beamInfo.m_flSpeed;
	pBeam->die				= gpGlobals->curtime + beamInfo.m_flLife;
	pBeam->width			= beamInfo.m_flWidth;
	pBeam->endWidth			= beamInfo.m_flEndWidth;
	pBeam->fadeLength		= beamInfo.m_flFadeLength;
	pBeam->amplitude		= beamInfo.m_flAmplitude;
	pBeam->brightness		= beamInfo.m_flBrightness;
	pBeam->speed			= beamInfo.m_flSpeed;
	pBeam->life				= beamInfo.m_flLife;
	pBeam->flags			= beamInfo.m_nFlags;

	VectorCopy( beamInfo.m_vecStart, pBeam->attachment[0] );
	VectorCopy( beamInfo.m_vecEnd, pBeam->attachment[1] );
	VectorSubtract( beamInfo.m_vecEnd, beamInfo.m_vecStart, pBeam->delta );
	contract_assert( pBeam->delta.IsValid() );

	if ( beamInfo.m_nSegments == -1 )
	{
		if ( pBeam->amplitude >= 0.50 )
		{
			pBeam->segments = VectorLength( pBeam->delta ) * 0.25 + 3; // one per 4 pixels
		}
		else
		{
			pBeam->segments = VectorLength( pBeam->delta ) * 0.075 + 3; // one per 16 pixels
		}
	}
	else
	{
		pBeam->segments = beamInfo.m_nSegments;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set beam color and frame data.
//	 Input: pBeam -
//          beamInfo -
//-----------------------------------------------------------------------------
void CViewRenderBeams::SetBeamAttributes( Beam_t *pBeam, const BeamInfo_t &beamInfo )
{
	pBeam->frame	 = ( float )beamInfo.m_nStartFrame;
	pBeam->frameRate = beamInfo.m_flFrameRate;
	pBeam->flags |= beamInfo.m_nFlags;

	pBeam->r = beamInfo.m_flRed;
	pBeam->g = beamInfo.m_flGreen;
	pBeam->b = beamInfo.m_flBlue;
}

//-----------------------------------------------------------------------------
// Purpose: Cull beam by bbox
// Input  : *start - 
//			*end - 
//			pvsOnly - 
// Output : int
//-----------------------------------------------------------------------------

int CViewRenderBeams::CullBeam( const Vector &start, const Vector &end, int pvsOnly )
{
	Vector mins, maxs;
	int i;

	for ( i = 0; i < 3; i++ )
	{
		if ( start[i] < end[i] )
		{
			mins[i] = start[i];
			maxs[i] = end[i];
		}
		else
		{
			mins[i] = end[i];
			maxs[i] = start[i];
		}
		
		// Don't let it be zero sized
		if ( mins[i] == maxs[i] )
		{
			maxs[i] += 1;
		}
	}

	// Check bbox
	if ( engine->IsBoxVisible( mins, maxs ) )
	{
		if ( pvsOnly || !engine->CullBox( mins, maxs ) )
		{
			// Beam is visible
			return 1;	
		}
	}

	// Beam is not visible
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Allocate and setup a generic beam.
//   Input: beamInfo -
//  Output: Beam_t
//-----------------------------------------------------------------------------
Beam_t *CViewRenderBeams::CreateGenericBeam( BeamInfo_t &beamInfo )
{

	Beam_t *pBeam = BeamAlloc( beamInfo.m_bRenderable );
	if ( !pBeam )
		return nullptr;

	// In case we fail.
	pBeam->die = gpGlobals->curtime;

	// Need a valid model.
	if ( beamInfo.m_nModelIndex < 0 )
		return nullptr;

	// Set it up
	SetupBeam( pBeam, beamInfo );

	return pBeam;	
}

//-----------------------------------------------------------------------------
// Purpose: Create a beam between two ents
// Input  : startEnt - 
//			endEnt - 
//			modelIndex - 
//			life - 
//			width - 
//			amplitude - 
//			brightness - 
//			speed - 
//			startFrame - 
//			framerate - 
//			BEAMENT_ENTITY(startEnt - 
// Output : Beam_t
//-----------------------------------------------------------------------------
void CViewRenderBeams::CreateBeamEnts( int startEnt, int endEnt, int modelIndex, 
	int haloIndex, float haloScale, float life, float width, float endWidth, 
	float fadeLength, float amplitude, float brightness, float speed, 
	int startFrame, float framerate, float r, float g, float b, int type )
{
	BeamInfo_t beamInfo;

	beamInfo.m_nType = type;
	beamInfo.m_pStartEnt = cl_entitylist->GetEnt( BEAMENT_ENTITY( startEnt ) );
	beamInfo.m_nStartAttachment = BEAMENT_ATTACHMENT( startEnt );
	beamInfo.m_pEndEnt = cl_entitylist->GetEnt( BEAMENT_ENTITY( endEnt ) );
	beamInfo.m_nEndAttachment = BEAMENT_ATTACHMENT( endEnt );
	beamInfo.m_nModelIndex = modelIndex;
	beamInfo.m_nHaloIndex = haloIndex;
	beamInfo.m_flHaloScale = haloScale;
	beamInfo.m_flLife = life;
	beamInfo.m_flWidth = width;
	beamInfo.m_flEndWidth = endWidth;
	beamInfo.m_flFadeLength = fadeLength;
	beamInfo.m_flAmplitude = amplitude;
	beamInfo.m_flBrightness = brightness;
	beamInfo.m_flSpeed = speed;
	beamInfo.m_nStartFrame = startFrame;
	beamInfo.m_flFrameRate = framerate;
	beamInfo.m_flRed = r;
	beamInfo.m_flGreen = g;
	beamInfo.m_flBlue = b;

	CreateBeamEnts( beamInfo );
}

//-----------------------------------------------------------------------------
// Purpose: Create a beam between two entities.
//-----------------------------------------------------------------------------
Beam_t *CViewRenderBeams::CreateBeamEnts( BeamInfo_t &beamInfo )
{
	// Don't start temporary beams out of the PVS
	if ( beamInfo.m_flLife != 0 && 
		 ( !beamInfo.m_pStartEnt || beamInfo.m_pStartEnt->GetModel() == nullptr || 
		   !beamInfo.m_pEndEnt || beamInfo.m_pEndEnt->GetModel() == nullptr) )
	{
		return nullptr;
	}

	beamInfo.m_vecStart = vec3_origin;
	beamInfo.m_vecEnd = vec3_origin;

	Beam_t *pBeam = CreateGenericBeam( beamInfo );
	if ( !pBeam )
		return nullptr;

	pBeam->type = ( beamInfo.m_nType < 0 ) ? TE_BEAMPOINTS : beamInfo.m_nType;
	pBeam->flags = FBEAM_STARTENTITY | FBEAM_ENDENTITY;

	pBeam->entity[0] = beamInfo.m_pStartEnt;
	pBeam->attachmentIndex[0] = beamInfo.m_nStartAttachment;
	pBeam->entity[1] = beamInfo.m_pEndEnt;
	pBeam->attachmentIndex[1] = beamInfo.m_nEndAttachment;

	// Attributes.
	SetBeamAttributes( pBeam, beamInfo );
	if ( beamInfo.m_flLife == 0 )
	{
		pBeam->flags |= FBEAM_FOREVER;
	}

	UpdateBeam( pBeam, 0 );

	return pBeam;
}

//-----------------------------------------------------------------------------
// Purpose: Creates a beam between an entity and a point
// Input  : startEnt - 
//			*end - 
//			modelIndex - 
//			life - 
//			width - 
//			amplitude - 
//			brightness - 
//			speed - 
//			startFrame - 
//			framerate - 
//			r - 
//			g - 
//			b - 
// Output : Beam_t
//-----------------------------------------------------------------------------
void CViewRenderBeams::CreateBeamEntPoint( int nStartEntity, const Vector *pStart, int nEndEntity, const Vector* pEnd,
										   int modelIndex, int haloIndex, float haloScale, float life,  float width, 
										   float endWidth, float fadeLength, float amplitude, float brightness, float speed, int startFrame, 
										   float framerate, float r, float g, float b )
{
	BeamInfo_t beamInfo;

	if ( nStartEntity <= 0 )
	{
		beamInfo.m_vecStart = pStart ? *pStart : vec3_origin;
		beamInfo.m_pStartEnt = nullptr;
	}
	else
	{
		beamInfo.m_pStartEnt = cl_entitylist->GetEnt( BEAMENT_ENTITY( nStartEntity ) );
		beamInfo.m_nStartAttachment = BEAMENT_ATTACHMENT( nStartEntity );

		// Don't start beams out of the PVS
		if ( !beamInfo.m_pStartEnt )
			return;
	}

	if ( nEndEntity <= 0 )
	{
		beamInfo.m_vecEnd = pEnd ? *pEnd : vec3_origin;
		beamInfo.m_pEndEnt = nullptr;
	}
	else
	{
		beamInfo.m_pEndEnt = cl_entitylist->GetEnt( BEAMENT_ENTITY( nEndEntity ) );
		beamInfo.m_nEndAttachment = BEAMENT_ATTACHMENT( nEndEntity );

		// Don't start beams out of the PVS
		if ( !beamInfo.m_pEndEnt )
			return;
	}

	beamInfo.m_nModelIndex = modelIndex;
	beamInfo.m_nHaloIndex = haloIndex;
	beamInfo.m_flHaloScale = haloScale;
	beamInfo.m_flLife = life;
	beamInfo.m_flWidth = width;
	beamInfo.m_flEndWidth = endWidth;
	beamInfo.m_flFadeLength = fadeLength;
	beamInfo.m_flAmplitude = amplitude;
	beamInfo.m_flBrightness = brightness;
	beamInfo.m_flSpeed = speed;
	beamInfo.m_nStartFrame = startFrame;
	beamInfo.m_flFrameRate = framerate;
	beamInfo.m_flRed = r;
	beamInfo.m_flGreen = g;
	beamInfo.m_flBlue = b;

	CreateBeamEntPoint( beamInfo );
}

//-----------------------------------------------------------------------------
// Purpose: Creates a beam between an entity and a point.
//-----------------------------------------------------------------------------
Beam_t *CViewRenderBeams::CreateBeamEntPoint( BeamInfo_t &beamInfo )
{
	if ( beamInfo.m_flLife != 0 )
	{
		if ( beamInfo.m_pStartEnt && beamInfo.m_pStartEnt->GetModel() == nullptr )
			return nullptr;

		if ( beamInfo.m_pEndEnt && beamInfo.m_pEndEnt->GetModel() == nullptr )
			return nullptr;
	}

	// Model index.
	if ( ( beamInfo.m_pszModelName ) && ( beamInfo.m_nModelIndex == -1 ) )
	{
		beamInfo.m_nModelIndex = modelinfo->GetModelIndex( beamInfo.m_pszModelName );
	}

	if ( ( beamInfo.m_pszHaloName ) && ( beamInfo.m_nHaloIndex == -1 ) )
	{
		beamInfo.m_nHaloIndex = modelinfo->GetModelIndex( beamInfo.m_pszHaloName );
	}

	Beam_t *pBeam = CreateGenericBeam( beamInfo );
	if ( !pBeam )
		return nullptr;

	pBeam->type = TE_BEAMPOINTS;
	pBeam->flags = 0;

	if ( beamInfo.m_pStartEnt )
	{
		pBeam->flags |= FBEAM_STARTENTITY;
		pBeam->entity[0] = beamInfo.m_pStartEnt;
		pBeam->attachmentIndex[0] = beamInfo.m_nStartAttachment;
		beamInfo.m_vecStart = vec3_origin;
	}
	if ( beamInfo.m_pEndEnt )
	{
		pBeam->flags |= FBEAM_ENDENTITY;
		pBeam->entity[1] = beamInfo.m_pEndEnt;
		pBeam->attachmentIndex[1] = beamInfo.m_nEndAttachment;
		beamInfo.m_vecEnd = vec3_origin;
	}

	SetBeamAttributes( pBeam, beamInfo );
	if ( beamInfo.m_flLife == 0 )
	{
		pBeam->flags |= FBEAM_FOREVER;
	}

	UpdateBeam( pBeam, 0 );
	return pBeam;
}

//-----------------------------------------------------------------------------
// Purpose: Creates a beam between two points
// Input  : *start - 
//			*end - 
//			modelIndex - 
//			life - 
//			width - 
//			amplitude - 
//			brightness - 
//			speed - 
//			startFrame - 
//			framerate - 
//			r - 
//			g - 
//			b - 
// Output : Beam_t
//-----------------------------------------------------------------------------
void CViewRenderBeams::CreateBeamPoints( Vector& start, Vector& end, int modelIndex, int haloIndex, float haloScale, float life,  float width, 
										 float endWidth, float fadeLength,float amplitude, float brightness, float speed, int startFrame, 
										 float framerate, float r, float g, float b )
{
	BeamInfo_t beamInfo;
	
	beamInfo.m_vecStart = start;
	beamInfo.m_vecEnd = end;
	beamInfo.m_nModelIndex = modelIndex;
	beamInfo.m_nHaloIndex = haloIndex;
	beamInfo.m_flHaloScale = haloScale;
	beamInfo.m_flLife = life;
	beamInfo.m_flWidth = width;
	beamInfo.m_flEndWidth = endWidth;
	beamInfo.m_flFadeLength = fadeLength;
	beamInfo.m_flAmplitude = amplitude;
	beamInfo.m_flBrightness = brightness;
	beamInfo.m_flSpeed = speed;
	beamInfo.m_nStartFrame = startFrame;
	beamInfo.m_flFrameRate = framerate;
	beamInfo.m_flRed = r;
	beamInfo.m_flGreen = g;
	beamInfo.m_flBlue = b;

	CreateBeamPoints( beamInfo );
}

//-----------------------------------------------------------------------------
// Purpose: Creates a beam between two points.
//-----------------------------------------------------------------------------
Beam_t *CViewRenderBeams::CreateBeamPoints( BeamInfo_t &beamInfo )
{
	// Don't start temporary beams out of the PVS
	if ( beamInfo.m_flLife != 0 && !CullBeam( beamInfo.m_vecStart, beamInfo.m_vecEnd, 1 ) )
		return nullptr;

	// Model index.
	if ( ( beamInfo.m_pszModelName ) && ( beamInfo.m_nModelIndex == -1 ) )
	{
		beamInfo.m_nModelIndex = modelinfo->GetModelIndex( beamInfo.m_pszModelName );
	}

	if ( ( beamInfo.m_pszHaloName ) && ( beamInfo.m_nHaloIndex == -1 ) )
	{
		beamInfo.m_nHaloIndex = modelinfo->GetModelIndex( beamInfo.m_pszHaloName );
	}

	// Create the new beam.
	Beam_t *pBeam = CreateGenericBeam( beamInfo );
	if ( !pBeam )
		return nullptr;

	// Set beam initial state.
	SetBeamAttributes( pBeam, beamInfo );
	if ( beamInfo.m_flLife == 0 )
	{
		pBeam->flags |= FBEAM_FOREVER;
	}

	return pBeam;
}

//-----------------------------------------------------------------------------
// Purpose: Creates a circular beam between two points
// Input  : type - 
//			*start - 
//			*end - 
//			modelIndex - 
//			life - 
//			width - 
//			amplitude - 
//			brightness - 
//			speed - 
//			startFrame - 
//			framerate - 
//			r - 
//			g - 
//			b - 
// Output : Beam_t
//-----------------------------------------------------------------------------
void CViewRenderBeams::CreateBeamCirclePoints( int type, Vector& start, Vector& end, int modelIndex, int haloIndex, float haloScale, float life, float width, 
											   float endWidth, float fadeLength,float amplitude, float brightness, float speed, int startFrame, 
											   float framerate, float r, float g, float b )
{
	BeamInfo_t beamInfo;
	
	beamInfo.m_nType = type;
	beamInfo.m_vecStart = start;
	beamInfo.m_vecEnd = end;
	beamInfo.m_nModelIndex = modelIndex;
	beamInfo.m_nHaloIndex = haloIndex;
	beamInfo.m_flHaloScale = haloScale;
	beamInfo.m_flLife = life;
	beamInfo.m_flWidth = width;
	beamInfo.m_flEndWidth = endWidth;
	beamInfo.m_flFadeLength = fadeLength;
	beamInfo.m_flAmplitude = amplitude;
	beamInfo.m_flBrightness = brightness;
	beamInfo.m_flSpeed = speed;
	beamInfo.m_nStartFrame = startFrame;
	beamInfo.m_flFrameRate = framerate;
	beamInfo.m_flRed = r;
	beamInfo.m_flGreen = g;
	beamInfo.m_flBlue = b;

	CreateBeamCirclePoints( beamInfo );
}

//-----------------------------------------------------------------------------
// Purpose: Creates a circular beam between two points.
//-----------------------------------------------------------------------------
Beam_t *CViewRenderBeams::CreateBeamCirclePoints( BeamInfo_t &beamInfo )
{
	Beam_t *pBeam = CreateGenericBeam( beamInfo );
	if ( !pBeam )
		return nullptr;

	pBeam->type = beamInfo.m_nType;

	SetBeamAttributes( pBeam, beamInfo );
	if ( beamInfo.m_flLife == 0 )
	{
		pBeam->flags |= FBEAM_FOREVER;
	}

	return pBeam;
}

//-----------------------------------------------------------------------------
// Purpose: Create a beam which follows an entity
// Input  : startEnt - 
//			modelIndex - 
//			life - 
//			width - 
//			r - 
//			g - 
//			b - 
//			brightness - 
// Output : Beam_t
//-----------------------------------------------------------------------------
void CViewRenderBeams::CreateBeamFollow( int startEnt, int modelIndex, int haloIndex, float haloScale, float life, float width, float endWidth, 
										 float fadeLength, float r, float g, float b, float brightness )
{
	BeamInfo_t beamInfo;

	beamInfo.m_pStartEnt = cl_entitylist->GetEnt( BEAMENT_ENTITY( startEnt ) );
	beamInfo.m_nStartAttachment = BEAMENT_ATTACHMENT( startEnt );
	beamInfo.m_nModelIndex = modelIndex;
	beamInfo.m_nHaloIndex = haloIndex;
	beamInfo.m_flHaloScale = haloScale;
	beamInfo.m_flLife = life;
	beamInfo.m_flWidth = width;
	beamInfo.m_flEndWidth = endWidth;
	beamInfo.m_flFadeLength = fadeLength;
	beamInfo.m_flBrightness = brightness;
	beamInfo.m_flRed = r;
	beamInfo.m_flGreen = g;
	beamInfo.m_flBlue = b;
	beamInfo.m_flAmplitude = life;

	CreateBeamFollow( beamInfo );
}

//-----------------------------------------------------------------------------
// Purpose: Create a beam which follows an entity.
//-----------------------------------------------------------------------------
Beam_t *CViewRenderBeams::CreateBeamFollow( BeamInfo_t &beamInfo )
{
	beamInfo.m_vecStart = vec3_origin;
	beamInfo.m_vecEnd = vec3_origin;
	beamInfo.m_flSpeed = 1.0f;
	Beam_t *pBeam = CreateGenericBeam( beamInfo );
	if ( !pBeam )
		return nullptr;

	pBeam->type = TE_BEAMFOLLOW;
	pBeam->flags = FBEAM_STARTENTITY;
	pBeam->entity[0] = beamInfo.m_pStartEnt;
	pBeam->attachmentIndex[0] = beamInfo.m_nStartAttachment;

	beamInfo.m_flFrameRate = 1.0f;
	beamInfo.m_nStartFrame = 0;

	SetBeamAttributes( pBeam, beamInfo );

	UpdateBeam( pBeam, 0 );

	return pBeam;
}

//-----------------------------------------------------------------------------
// Purpose: Create a beam ring between two entities
// Input  : startEnt - 
//			endEnt - 
//			modelIndex - 
//			life - 
//			width - 
//			amplitude - 
//			brightness - 
//			speed - 
//			startFrame - 
//			framerate - 
//			startEnt - 
// Output : Beam_t
//-----------------------------------------------------------------------------
void CViewRenderBeams::CreateBeamRingPoint( const Vector& center, float start_radius, float end_radius, 
					   int modelIndex, int haloIndex, float haloScale, float life, float width, float endWidth, 
					   float fadeLength, float amplitude, float brightness, float speed, int startFrame, float framerate, 
					   float r, float g, float b, int nFlags )
{
	BeamInfo_t beamInfo;
	
	beamInfo.m_nModelIndex = modelIndex;
	beamInfo.m_nHaloIndex = haloIndex;
	beamInfo.m_flHaloScale = haloScale;
	beamInfo.m_flLife = life;
	beamInfo.m_flWidth = width;
	beamInfo.m_flEndWidth = endWidth;
	beamInfo.m_flFadeLength = fadeLength;
	beamInfo.m_flAmplitude = amplitude;
	beamInfo.m_flBrightness = brightness;
	beamInfo.m_flSpeed = speed;
	beamInfo.m_nStartFrame = startFrame;
	beamInfo.m_flFrameRate = framerate;
	beamInfo.m_flRed = r;
	beamInfo.m_flGreen = g;
	beamInfo.m_flBlue = b;
	beamInfo.m_vecCenter = center;
	beamInfo.m_flStartRadius = start_radius;
	beamInfo.m_flEndRadius = end_radius;
	beamInfo.m_nFlags = nFlags;

	CreateBeamRingPoint( beamInfo );
}

//-----------------------------------------------------------------------------
// Purpose: Create a beam ring between two entities
//   Input: beamInfo -
//-----------------------------------------------------------------------------
Beam_t *CViewRenderBeams::CreateBeamRingPoint( BeamInfo_t &beamInfo )
{
	// ??
	Vector endpos = beamInfo.m_vecCenter;

	beamInfo.m_vecStart = beamInfo.m_vecCenter;
	beamInfo.m_vecEnd = beamInfo.m_vecCenter;

	Beam_t *pBeam = CreateGenericBeam( beamInfo );
	if ( !pBeam )
		return nullptr;

	pBeam->type = TE_BEAMRINGPOINT;
	pBeam->start_radius = beamInfo.m_flStartRadius;
	pBeam->end_radius = beamInfo.m_flEndRadius;
	pBeam->attachment[2] = beamInfo.m_vecCenter;

	SetBeamAttributes( pBeam, beamInfo );
	if ( beamInfo.m_flLife == 0 )
	{
		pBeam->flags |= FBEAM_FOREVER;
	}

	return pBeam;
}

//-----------------------------------------------------------------------------
// Purpose: Create a beam ring between two entities
// Input  : startEnt - 
//			endEnt - 
//			modelIndex - 
//			life - 
//			width - 
//			amplitude - 
//			brightness - 
//			speed - 
//			startFrame - 
//			framerate - 
//			startEnt - 
// Output : Beam_t
//-----------------------------------------------------------------------------
void CViewRenderBeams::CreateBeamRing( int startEnt, int endEnt, int modelIndex, int haloIndex, float haloScale, float life, float width, float endWidth, float fadeLength, 
					   float amplitude, float brightness, float speed, int startFrame, float framerate, 
					   float r, float g, float b, int flags )
{
	BeamInfo_t beamInfo;

	beamInfo.m_pStartEnt = cl_entitylist->GetEnt( BEAMENT_ENTITY( startEnt ) );
	beamInfo.m_nStartAttachment = BEAMENT_ATTACHMENT( startEnt );
	beamInfo.m_pEndEnt = cl_entitylist->GetEnt( BEAMENT_ENTITY( endEnt ) );
	beamInfo.m_nEndAttachment = BEAMENT_ATTACHMENT( endEnt );
	beamInfo.m_nModelIndex = modelIndex;
	beamInfo.m_nHaloIndex = haloIndex;
	beamInfo.m_flHaloScale = haloScale;
	beamInfo.m_flLife = life;
	beamInfo.m_flWidth = width;
	beamInfo.m_flEndWidth = endWidth;
	beamInfo.m_flFadeLength = fadeLength;
	beamInfo.m_flAmplitude = amplitude;
	beamInfo.m_flBrightness = brightness;
	beamInfo.m_flSpeed = speed;
	beamInfo.m_nStartFrame = startFrame;
	beamInfo.m_flFrameRate = framerate;
	beamInfo.m_flRed = r;
	beamInfo.m_flGreen = g;
	beamInfo.m_flBlue = b;
	beamInfo.m_nFlags = flags;

	CreateBeamRing( beamInfo );
}

//-----------------------------------------------------------------------------
// Purpose: Create a beam ring between two entities.
//   Input: beamInfo -
//-----------------------------------------------------------------------------
Beam_t *CViewRenderBeams::CreateBeamRing( BeamInfo_t &beamInfo )
{
	// Don't start temporary beams out of the PVS
	if ( beamInfo.m_flLife != 0 && 
		 ( !beamInfo.m_pStartEnt || beamInfo.m_pStartEnt->GetModel() == nullptr || 
		   !beamInfo.m_pEndEnt || beamInfo.m_pEndEnt->GetModel() == nullptr ) )
	{
		return nullptr;
	}

	beamInfo.m_vecStart = vec3_origin;
	beamInfo.m_vecEnd = vec3_origin;
	Beam_t *pBeam = CreateGenericBeam( beamInfo );
	if ( !pBeam )
		return nullptr;

	pBeam->type = TE_BEAMRING;
	pBeam->flags = FBEAM_STARTENTITY | FBEAM_ENDENTITY;
	pBeam->entity[0] = beamInfo.m_pStartEnt;
	pBeam->attachmentIndex[0] = beamInfo.m_nStartAttachment;
	pBeam->entity[1] = beamInfo.m_pEndEnt;
	pBeam->attachmentIndex[1] = beamInfo.m_nEndAttachment;

	SetBeamAttributes( pBeam, beamInfo );
	if ( beamInfo.m_flLife == 0 )
	{
		pBeam->flags |= FBEAM_FOREVER;
	}

	UpdateBeam( pBeam, 0 );

	return pBeam;
}

//-----------------------------------------------------------------------------
// Purpose: Free dead trails associated with beam
// Input  : **ppparticles - 
//-----------------------------------------------------------------------------
void CViewRenderBeams::FreeDeadTrails( BeamTrail_t **trail )
{
	BeamTrail_t *kill;
	BeamTrail_t *p;

	// kill all the ones hanging direcly off the base pointer
	for ( ;; ) 
	{
		kill = *trail;
		if (kill && kill->die < gpGlobals->curtime)
		{
			*trail = kill->next;
			kill->next = m_pFreeTrails;
			m_pFreeTrails = kill;
			continue;
		}
		break;
	}

	// kill off all the others
	for (p=*trail ; p ; p=p->next)
	{
		for ( ;; )
		{
			kill = p->next;
			if (kill && kill->die < gpGlobals->curtime)
			{
				p->next = kill->next;
				kill->next = m_pFreeTrails;
				m_pFreeTrails = kill;
				continue;
			}
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Updates beam state
//-----------------------------------------------------------------------------
void CViewRenderBeams::UpdateBeam( Beam_t *pbeam, float frametime, CBeam *pcbeam )
{
	if ( pbeam->modelIndex < 0 )
	{
		pbeam->die = gpGlobals->curtime;
		return;
	}

	// if we are paused, force random numbers used by noise to generate the same value every frame
	if ( frametime == 0.0f )
	{
		beamRandom.SetSeed( (int)gpGlobals->curtime );
	}

	// If FBEAM_ONLYNOISEONCE is set, we don't want to move once we've first calculated noise
	if ( !(pbeam->flags & FBEAM_ONLYNOISEONCE ) )
	{
		pbeam->freq += frametime;
	}
	else
	{
		pbeam->freq += frametime * beamRandom.RandomFloat(1,2);
	}

	// OPTIMIZE: Do this every frame?
	// UNDONE: Do this differentially somehow?
	// Generate fractal noise
	pbeam->rgNoise[0] = 0;
	pbeam->rgNoise[NOISE_DIVISIONS] = 0;
	if ( pbeam->amplitude != 0 )
	{
		if ( !(pbeam->flags & FBEAM_ONLYNOISEONCE ) || !pbeam->m_bCalculatedNoise )
		{
			if ( pbeam->flags & FBEAM_SINENOISE )
			{
				SineNoise( pbeam->rgNoise, NOISE_DIVISIONS );
			}
			else
			{
				Noise( pbeam->rgNoise, NOISE_DIVISIONS, 1.0 );
			}

			pbeam->m_bCalculatedNoise = true;
		}
	}

	// update end points
	if ( pbeam->flags & (FBEAM_STARTENTITY|FBEAM_ENDENTITY) )
	{
		// Makes sure attachment[0] + attachment[1] are valid
		if (!RecomputeBeamEndpoints( pbeam ))
			return;

		// clip if requested
		if ( pcbeam && pcbeam->GetClipStyle() != CBeam::kNOCLIP )
		{
			ClipBeam( pcbeam, pbeam );
		}

		// Compute segments from the new endpoints
		VectorSubtract( pbeam->attachment[1], pbeam->attachment[0], pbeam->delta );
		if ( pbeam->amplitude >= 0.50 )
			pbeam->segments = VectorLength( pbeam->delta ) * 0.25 + 3; // one per 4 pixels
		else
			pbeam->segments = VectorLength( pbeam->delta ) * 0.075 + 3; // one per 16 pixels
	}

	// Get position data for spline beam
	switch ( pbeam->type )
	{
	case TE_BEAMSPLINE:
		{
			// Why isn't attachment[0] being computed?
			for (int i=1; i < pbeam->numAttachments; i++)
			{
				if (!ComputeBeamEntPosition( pbeam->entity[i], pbeam->attachmentIndex[i], (pbeam->flags & FBEAM_USE_HITBOXES) != 0, pbeam->attachment[i] ))
				{
					// This should never happen, but if for some reason the attachment doesn't exist, 
					// as a safety measure copy in the location of the previous attachment point (rather than bailing)
					VectorCopy( pbeam->attachment[i-1], pbeam->attachment[i] );
				}
			}
		}
		break;

	case TE_BEAMRINGPOINT:
		{
			// 
			float dr = pbeam->end_radius - pbeam->start_radius;
			if ( dr != 0.0f )
			{
				float frac = 1.0f;
				// Go some portion of the way there based on life
				float remaining = pbeam->die - gpGlobals->curtime;
				if ( remaining < pbeam->life && pbeam->life > 0.0f )
				{
					frac = remaining / pbeam->life;
				}
				frac = std::min( 1.0f, frac );
				frac = std::max( 0.0f, frac );

				frac = 1.0f - frac;

				// Start pos
				Vector endpos = pbeam->attachment[ 2 ];
				endpos.x += ( pbeam->start_radius + frac * dr ) / 2.0f;
				Vector startpos = pbeam->attachment[ 2 ];
				startpos.x -= ( pbeam->start_radius + frac * dr ) / 2.0f;

				pbeam->attachment[ 0 ] = startpos;
				pbeam->attachment[ 1 ] = endpos;

				VectorSubtract( pbeam->attachment[1], pbeam->attachment[0], pbeam->delta );
				if (pbeam->amplitude >= 0.50)
					pbeam->segments = VectorLength( pbeam->delta ) * 0.25 + 3; // one per 4 pixels
				else
					pbeam->segments = VectorLength( pbeam->delta ) * 0.075 + 3; // one per 16 pixels

			}
		}
		break;

	case TE_BEAMPOINTS:
		// UNDONE: Build culling volumes for other types of beams
		if ( !CullBeam( pbeam->attachment[0], pbeam->attachment[1], 0 ) )
			return;
		break;
	}

	// update life cycle
	pbeam->t = pbeam->freq + (pbeam->die - gpGlobals->curtime);
	if (pbeam->t != 0)
	{
		pbeam->t = pbeam->freq / pbeam->t;
	}
	else
	{
		pbeam->t = 1.0f;
	}

	// ------------------------------------------
	// check for zero fadeLength (means no fade)
	// ------------------------------------------
	if (pbeam->fadeLength == 0)
	{
		contract_assert( pbeam->delta.IsValid() );
		pbeam->fadeLength = pbeam->delta.Length();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Update beams created by temp entity system
//-----------------------------------------------------------------------------
void CViewRenderBeams::UpdateTempEntBeams( void )
{
	CVProfScope _( "UpdateTempEntBeams", 2, VPROF_BUDGETGROUP_CLIENT_SIM, false, BUDGETFLAG_CLIENT );
	if ( !m_pActiveBeams )
		return;

	// Get frame time
	float frametime = gpGlobals->frametime;

	if ( frametime == 0.0f )
		return;

	// Draw temporary entity beams
	Beam_t* pPrev = 0;
	Beam_t* pNext;
	for ( Beam_t* pBeam = m_pActiveBeams; pBeam ; pBeam = pNext )
	{
		// Need to store the next one since we may delete this one
		pNext = pBeam->next;

		// Retire old beams
		if ( !(pBeam->flags & FBEAM_FOREVER) && 
			pBeam->die <= gpGlobals->curtime )
		{
			// Reset links
			if ( pPrev )
			{
				pPrev->next = pNext;
			}
			else
			{
				m_pActiveBeams = pNext;
			}

			// Free the beam
			BeamFree( pBeam );

			pBeam = nullptr;
			continue;
		}

		// Update beam state
		UpdateBeam( pBeam, frametime );

		// Compute bounds for the beam
		pBeam->ComputeBounds();

		// Indicates the beam moved
		if ( pBeam->m_hRenderHandle != INVALID_CLIENT_RENDER_HANDLE )
		{
			ClientLeafSystem()->RenderableChanged( pBeam->m_hRenderHandle );
		}

		pPrev = pBeam;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Draw helper for beam follow beams
// Input  : *pbeam - 
//			frametime - 
//			*color - 
//-----------------------------------------------------------------------------
void CViewRenderBeams::DrawBeamFollow( const model_t* pSprite, Beam_t *pbeam, 
	int frame, int rendermode, float frametime, const float* color, float flHDRColorScale )
{
	BeamTrail_t		*particles;
	BeamTrail_t		*pnew;
	float			div;
	Vector			delta;
	
	Vector			screenLast;
	Vector			screen;

	FreeDeadTrails( &pbeam->trail );

	particles = pbeam->trail;
	pnew = nullptr;

	div = 0;
	if ( pbeam->flags & FBEAM_STARTENTITY )
	{
		if (particles)
		{
			VectorSubtract( particles->org, pbeam->attachment[0], delta );
			div = VectorLength( delta );
			if (div >= 32 && m_pFreeTrails)
			{
				pnew = m_pFreeTrails;
				m_pFreeTrails = pnew->next;
			}
		}
		else if (m_pFreeTrails)
		{
			pnew = m_pFreeTrails;
			m_pFreeTrails = pnew->next;
			div = 0;
		}
	}

	if (pnew)
	{
		VectorCopy( pbeam->attachment[0], pnew->org );
		pnew->die = gpGlobals->curtime + pbeam->amplitude;
		VectorCopy( vec3_origin, pnew->vel );

		pbeam->die = gpGlobals->curtime + pbeam->amplitude;
		pnew->next = particles;
		pbeam->trail = pnew;
		particles = pnew;
	}

	if (!particles)
	{
		return;
	}
	if (!pnew && div != 0)
	{
		VectorCopy( pbeam->attachment[0], delta );
		debugoverlay->ScreenPosition( pbeam->attachment[0], screenLast );
		debugoverlay->ScreenPosition( particles->org, screen );
	}
	else if (particles && particles->next)
	{
		VectorCopy( particles->org, delta );
		debugoverlay->ScreenPosition( particles->org, screenLast );
		debugoverlay->ScreenPosition( particles->next->org, screen );
		particles = particles->next;
	}
	else
	{
		return;
	}

	// Draw it
	::DrawBeamFollow( pSprite, pbeam->trail, frame, rendermode, delta, screen, screenLast, 
		pbeam->die, pbeam->attachment[0], pbeam->flags, pbeam->width, 
		pbeam->amplitude, pbeam->freq, (float*)color );
	
	// Drift popcorn trail if there is a velocity
	particles = pbeam->trail;
	while (particles)
	{
		VectorMA( particles->org, frametime, particles->vel, particles->org );
		particles = particles->next;
	}
}

//------------------------------------------------------------------------------
// Purpose : Draw beam with a halo
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CViewRenderBeams::DrawBeamWithHalo(	Beam_t*			pbeam, 
											int				frame,
											int				rendermode,
											float*			color,
											float*			srcColor,
											const model_t	*sprite,
											const model_t	*halosprite,
											float			flHDRColorScale )
{
	Vector beamDir = pbeam->attachment[1] - pbeam->attachment[0];
	VectorNormalize( beamDir );
	
	Vector localDir = CurrentViewOrigin() - pbeam->attachment[0];
	VectorNormalize( localDir );
	
	float dotpr = DotProduct( beamDir, localDir );
	float fade;

	if ( dotpr < 0.0f )
	{
		fade = 0;
	}
	else
	{
		fade = dotpr * 2.0f;
	}

	float	distToLine;
	Vector	out;

	// Find out how close we are to the "line" of the spotlight
	CalcClosestPointOnLine( CurrentViewOrigin(), pbeam->attachment[0], pbeam->attachment[0] + ( beamDir * 2 ), out, &distToLine );

	distToLine = ( CurrentViewOrigin() - out ).Length();

	float scaleColor[3];
	float dotScale = 1.0f;
	
	// Use beam width
	float distThreshold = pbeam->width * 4.0f;

	if ( distToLine < distThreshold )
	{
		dotScale = RemapVal( distToLine, distThreshold, pbeam->width, 1.0f, 0.0f );
		dotScale = clamp( dotScale, 0, 1 );
	}

	scaleColor[0] = color[0] * dotScale;
	scaleColor[1] = color[1] * dotScale;
	scaleColor[2] = color[2] * dotScale;

	if( pbeam->flags & FBEAM_HALOBEAM )
	{
		DrawSegs( NOISE_DIVISIONS, pbeam->rgNoise, sprite, frame, rendermode, pbeam->attachment[0],
			pbeam->delta, pbeam->width, pbeam->endWidth, pbeam->amplitude, pbeam->freq, pbeam->speed,
			pbeam->segments, pbeam->flags, scaleColor, pbeam->fadeLength, flHDRColorScale );
	}
	else
	{
		// Draw primary beam just shy of its end so it doesn't clip
		DrawSegs( NOISE_DIVISIONS, pbeam->rgNoise, sprite, frame, rendermode, pbeam->attachment[0], 
			pbeam->delta, pbeam->width, pbeam->width, pbeam->amplitude, pbeam->freq, pbeam->speed, 
			2, pbeam->flags, scaleColor, pbeam->fadeLength, flHDRColorScale );
	}

	Vector vSource = pbeam->attachment[0];

	pixelvis_queryparams_t params;
	params.Init( vSource, pbeam->m_haloProxySize );

	float haloFractionVisible = PixelVisibility_FractionVisible( params, pbeam->m_queryHandleHalo );
	if ( fade && haloFractionVisible > 0.0f )
	{
		//NOTENOTE: This is kinda funky when moving away and to the backside -- jdw
		float haloScale = RemapVal( distToLine, distThreshold, pbeam->width*0.5f, 1.0f, 2.0f );

		haloScale = clamp( haloScale, 1.0f, 2.0f );

		haloScale *= pbeam->haloScale;
		
		float colorFade = fade*fade;
		colorFade = clamp( colorFade, 0, 1 );

		float haloColor[3];
		VectorScale( color, colorFade * haloFractionVisible, haloColor );

		BeamDrawHalo( halosprite, frame, kRenderGlow, vSource, haloScale, haloColor, flHDRColorScale );
	}
}

//------------------------------------------------------------------------------
// Purpose : Draw a beam based upon the viewpoint
//------------------------------------------------------------------------------
void CViewRenderBeams::DrawLaser( Beam_t *pbeam, int frame, int rendermode, float *color, const model_t *sprite, const model_t *halosprite, float flHDRColorScale )
{
	float	color2[3];
	VectorCopy( color, color2 );

	Vector vecForward;
	Vector	beamDir	= pbeam->attachment[1] - pbeam->attachment[0];
	VectorNormalize( beamDir );
	AngleVectors( CurrentViewAngles(), &vecForward );
	float flDot = DotProduct(beamDir, vecForward);

	// abort if the player's looking along it away from the source
	if ( flDot > 0 )
	{
		return;
	}
	else
	{
		// Fade the beam if the player's not looking at the source
		float flFade = std::pow( flDot, 10 );

		// Fade the beam based on the player's proximity to the beam
		Vector localDir = CurrentViewOrigin() - pbeam->attachment[0];
		flDot = DotProduct( beamDir, localDir );
		Vector vecProjection = flDot * beamDir;
		float flDistance = ( localDir - vecProjection ).Length();

		if ( flDistance > 30 )
		{
			flDistance = 1 - ((flDistance - 30) / 64);
			if ( flDistance <= 0 )
			{
				flFade = 0;
			}
			else
			{
				flFade *= pow( flDistance, 3 );
			}
		}

		if (flFade < (1.0f / 255.0f))
			return;

		VectorScale( color2, flFade, color2 );

		//engine->Con_NPrintf( 6, "Fade: %f", flFade );
		//engine->Con_NPrintf( 7, "Dist: %f", flDistance );
	}

	DrawSegs( NOISE_DIVISIONS, pbeam->rgNoise, sprite, frame, rendermode, pbeam->attachment[0], pbeam->delta, pbeam->width, pbeam->endWidth, pbeam->amplitude, pbeam->freq, pbeam->speed, pbeam->segments, pbeam->flags, color2, pbeam->fadeLength);
}

//------------------------------------------------------------------------------
// Purpose : Draw a fibrous tesla beam
//------------------------------------------------------------------------------
void CViewRenderBeams::DrawTesla( Beam_t *pbeam, int frame, int rendermode, float *color, const model_t *sprite, float flHDRColorScale )
{
	DrawTeslaSegs( NOISE_DIVISIONS, pbeam->rgNoise, sprite, frame, rendermode, pbeam->attachment[0], pbeam->delta, pbeam->width, pbeam->endWidth, pbeam->amplitude, pbeam->freq, pbeam->speed, pbeam->segments, pbeam->flags, color, pbeam->fadeLength, flHDRColorScale );
}

//-----------------------------------------------------------------------------
// Purpose: Draw all beam entities
// Input  : *pbeam - 
//			frametime - 
//-----------------------------------------------------------------------------
void CViewRenderBeams::DrawBeam( Beam_t *pbeam )
{
	contract_assert( pbeam->delta.IsValid() );

	if ( !r_DrawBeams.GetInt() )
		return;

	// Don't draw really short beams
	if (pbeam->delta.Length() < 0.1)
	{
		return;
	}

	const model_t	*sprite;
	const model_t	*halosprite = nullptr;

	if ( pbeam->modelIndex < 0 )
	{
		pbeam->die = gpGlobals->curtime;
		return;
	}
	
	sprite = modelinfo->GetModel( pbeam->modelIndex );
	if ( !sprite )
	{
		return;
	}
	
	halosprite = modelinfo->GetModel( pbeam->haloIndex );

	int frame = ( ( int )( pbeam->frame + gpGlobals->curtime * pbeam->frameRate) % pbeam->frameCount );
	int rendermode = ( pbeam->flags & FBEAM_SOLID ) ? kRenderNormal : kRenderTransAdd;

	// set color
	float srcColor[3];
	float color[3];

	srcColor[0] = pbeam->r;
	srcColor[1] = pbeam->g;
	srcColor[2] = pbeam->b;
	if ( pbeam->flags & FBEAM_FADEIN )
	{
		VectorScale( srcColor, pbeam->t, color );
	}
	else if ( pbeam->flags & FBEAM_FADEOUT )
	{
		VectorScale( srcColor, ( 1.0f - pbeam->t ), color );
	}
	else
	{
		VectorCopy( srcColor, color );
	}

	VectorScale( color, (1/255.0), color );
	VectorCopy( color, srcColor );
	VectorScale( color, ((float)pbeam->brightness / 255.0), color );

	switch( pbeam->type )
	{
	case TE_BEAMDISK:
		DrawDisk( NOISE_DIVISIONS, pbeam->rgNoise, sprite, frame, rendermode, 
			pbeam->attachment[0], pbeam->delta, pbeam->width, pbeam->amplitude, 
			pbeam->freq, pbeam->speed, pbeam->segments, color, pbeam->m_flHDRColorScale );
		break;

	case TE_BEAMCYLINDER:
		DrawCylinder( NOISE_DIVISIONS, pbeam->rgNoise, sprite, frame, rendermode, 
			pbeam->attachment[0], pbeam->delta, pbeam->width, pbeam->amplitude, 
			pbeam->freq, pbeam->speed, pbeam->segments, color, pbeam->m_flHDRColorScale );
		break;

	case TE_BEAMPOINTS:
		{
			if (halosprite)
			{	
				DrawBeamWithHalo( pbeam, frame, rendermode, color, srcColor, sprite, halosprite, pbeam->m_flHDRColorScale );
			}
			else
			{
				DrawSegs( NOISE_DIVISIONS, pbeam->rgNoise, sprite, frame, rendermode, 
					pbeam->attachment[0], pbeam->delta, pbeam->width, pbeam->endWidth, 
					pbeam->amplitude, pbeam->freq, pbeam->speed, pbeam->segments, 
					pbeam->flags, color, pbeam->fadeLength, pbeam->m_flHDRColorScale );
			}
		}
		break;

	case TE_BEAMFOLLOW:
		DrawBeamFollow( sprite, pbeam, frame, rendermode, gpGlobals->frametime, color, pbeam->m_flHDRColorScale );
		break;

	case TE_BEAMRING:
	case TE_BEAMRINGPOINT:
		DrawRing( NOISE_DIVISIONS, pbeam->rgNoise, Noise, sprite, frame, rendermode, 
			pbeam->attachment[0], pbeam->delta, pbeam->width, pbeam->amplitude, 
			pbeam->freq, pbeam->speed, pbeam->segments, color, pbeam->m_flHDRColorScale );
		break;

	case TE_BEAMSPLINE:
		DrawSplineSegs( NOISE_DIVISIONS, pbeam->rgNoise, sprite, halosprite,
			pbeam->haloScale, frame, rendermode, pbeam->numAttachments,
			pbeam->attachment, pbeam->width, pbeam->endWidth, pbeam->amplitude,
			pbeam->freq, pbeam->speed, pbeam->segments, pbeam->flags, color, pbeam->fadeLength, pbeam->m_flHDRColorScale );
		break;

	case TE_BEAMLASER:
		DrawLaser( pbeam, frame, rendermode, color, sprite, halosprite, pbeam->m_flHDRColorScale );
		break;

	case TE_BEAMTESLA:
		DrawTesla( pbeam, frame, rendermode, color, sprite, pbeam->m_flHDRColorScale );
		break;

	default:
		DevWarning( 1, "CViewRenderBeams::DrawBeam:  Unknown beam type %i\n", pbeam->type );
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Update the beam 
//-----------------------------------------------------------------------------
void CViewRenderBeams::UpdateBeamInfo( Beam_t *pBeam, BeamInfo_t &beamInfo )
{
	pBeam->attachment[0] = beamInfo.m_vecStart;
	pBeam->attachment[1] = beamInfo.m_vecEnd;
	pBeam->delta = beamInfo.m_vecEnd - beamInfo.m_vecStart;

	contract_assert( pBeam->delta.IsValid() );

	SetBeamAttributes( pBeam, beamInfo );
}

//-----------------------------------------------------------------------------
// Recomputes beam endpoints..
//-----------------------------------------------------------------------------
bool CViewRenderBeams::RecomputeBeamEndpoints( Beam_t *pbeam )
{
	if ( pbeam->flags & FBEAM_STARTENTITY )
	{
		if (ComputeBeamEntPosition( pbeam->entity[0], pbeam->attachmentIndex[0], (pbeam->flags & FBEAM_USE_HITBOXES) != 0, pbeam->attachment[0] ))
		{
			pbeam->flags |= FBEAM_STARTVISIBLE;
		}
		else if (! (pbeam->flags & FBEAM_FOREVER))
		{
			pbeam->flags &= ~(FBEAM_STARTENTITY);
		}
		else
		{
			// DevWarning( 1,"can't find start entity\n");
//			return false;
		}

		// If we've never seen the start entity, don't display
		if ( !(pbeam->flags & FBEAM_STARTVISIBLE) )
			return false;
	}

	if ( pbeam->flags & FBEAM_ENDENTITY )
	{
		if (ComputeBeamEntPosition( pbeam->entity[1], pbeam->attachmentIndex[1], (pbeam->flags & FBEAM_USE_HITBOXES) != 0, pbeam->attachment[1] ))
		{
			pbeam->flags |= FBEAM_ENDVISIBLE;
		}
		else if (! (pbeam->flags & FBEAM_FOREVER))
		{
			pbeam->flags &= ~(FBEAM_ENDENTITY);
			pbeam->die = gpGlobals->curtime;
			return false;
		}
		else
		{
			return false;
		}

		// If we've never seen the end entity, don't display
		if ( !(pbeam->flags & FBEAM_ENDVISIBLE) )
			return false;
	}

	return true;
}

ConVar cl_beam_test_traces( side_registry(), "cl_beam_test_traces", "0", FCVAR_DEVELOPMENTONLY, "Enable debug overlay on traces that determine where the client-side visible env_beam is drawn. Has no bearing on the server-side damage-causing part of the beam." );
static inline bool BeamDebugOverlay() { return cl_beam_test_traces.GetBool(); }

void CViewRenderBeams::ClipBeam( CBeam * __restrict__ pcbeam, Beam_t * __restrict__ pbeam )
{
	// Assert( pbeam->GetClipStyle() != CBeam::kNOCLIP );
	int colmask = 0;
	int colgroup = COLLISION_GROUP_NONE;
	switch ( pcbeam->GetClipStyle() )
	{
	case CBeam::kGEOCLIP:
		colmask = CONTENTS_SOLID; // lasers go through gates and windows.
		break;
	case CBeam::kMODELCLIP:
		colmask = MASK_SOLID; 
		break;
	default:
		contract_assert( false && "Unknown beam clipping type %d\n" );
		return;
	}

	trace_t tr;
	Vector &vstart = pbeam->attachment[0];
	Vector &vend = pbeam->attachment[1];
	// start the trace from a few inches ahead of the start position (in case of coplanarity)
	// use a fast estimated normalize ( i'll push this into mathlib later )
	Vector delta = vend - vstart;
	delta *= 8.0f * FastRSqrtFast((delta).LengthSqr()) ;
	if ( BeamDebugOverlay() )
	{
		NDebugOverlay::Line( vstart + delta, vend, 255, 255, 0, true, 0.2f );
	}

	UTIL_TraceLine( vstart + delta , vend, colmask, nullptr, colgroup, &tr );

	if ( tr.fraction < 1.0f )
	{
		// move the endpoint to wherever the trace stopped
		// if ( test_spam.GetBool() ) Msg( "(%s) %s\n", tr.startsolid ? "x" : " ", tr.Ent<CBaseEntity>()->GetDebugName() );
		if ( BeamDebugOverlay() )
			NDebugOverlay::Cross( tr.endpos, 8, 255, 255, 0, false, 0.2f );

		vend = tr.endpos;
	}
}

//-----------------------------------------------------------------------------
// Draws a single beam
//-----------------------------------------------------------------------------
void CViewRenderBeams::DrawBeam( CBeam* pbeam, const RenderableInstance_t &instance, ITraceFilter *pEntityBeamTraceFilter )
{
	if ( !r_DrawBeams.GetInt() )
		return;

	Beam_t beam;

	// Set up the beam.
	int beamType = pbeam->GetType();

	BeamInfo_t beamInfo;
	beamInfo.m_vecStart = pbeam->GetAbsStartPos();
	beamInfo.m_vecEnd = pbeam->GetAbsEndPos();
	beamInfo.m_pStartEnt = beamInfo.m_pEndEnt = nullptr;
	beamInfo.m_nModelIndex = pbeam->GetModelIndex();
	beamInfo.m_nHaloIndex = pbeam->m_nHaloIndex;
	beamInfo.m_flHaloScale = pbeam->m_fHaloScale;
	beamInfo.m_flLife = 0;
	beamInfo.m_flWidth = pbeam->GetWidth();
	beamInfo.m_flEndWidth = pbeam->GetEndWidth();
	beamInfo.m_flFadeLength = pbeam->GetFadeLength();
	beamInfo.m_flAmplitude = pbeam->GetNoise();
	beamInfo.m_flBrightness = instance.m_nAlpha * (pbeam->GetRenderAlpha() / 255.0f);
	beamInfo.m_flSpeed = pbeam->GetScrollRate();
	beamInfo.m_nFlags = pbeam->GetBeamFlags();

	if ( pbeam->GetBeamFlags() & FBEAM_REVERSED )
	{
		V_swap( beamInfo.m_vecStart, beamInfo.m_vecEnd );
		V_swap( beamInfo.m_flWidth, beamInfo.m_flEndWidth );
	}

	SetupBeam( &beam, beamInfo );

	beamInfo.m_nStartFrame = pbeam->m_fStartFrame;
	beamInfo.m_flFrameRate = pbeam->m_flFrameRate;
	beamInfo.m_flRed = pbeam->GetRenderColorR();
	beamInfo.m_flGreen = pbeam->GetRenderColorG();
	beamInfo.m_flBlue = pbeam->GetRenderColorB();

	SetBeamAttributes( &beam, beamInfo );

	if ( pbeam->m_nHaloIndex > 0 )
	{
		// HACKHACK: heuristic to estimate proxy size.  Revisit this!
		float size = 1.0f + (pbeam->m_fHaloScale * pbeam->m_fWidth / pbeam->m_fEndWidth);
		size = clamp( size, 1.0f, 8.0f );
		beam.m_queryHandleHalo = &pbeam->m_queryHandleHalo;
		beam.m_haloProxySize = size;
	}
	else
	{
		beam.m_queryHandleHalo = nullptr;
	}

	// Handle code from relinking.
	switch( beamType )
	{
		case BEAM_ENTS:
		{
			beam.type			= TE_BEAMPOINTS;
			beam.flags			= FBEAM_STARTENTITY | FBEAM_ENDENTITY;
			beam.entity[0]		= pbeam->m_hAttachEntity[0];
			beam.attachmentIndex[0]	= pbeam->m_nAttachIndex[0];
			beam.entity[1]		= pbeam->m_hAttachEntity[1];
			beam.attachmentIndex[1]	= pbeam->m_nAttachIndex[1];
			beam.numAttachments	= pbeam->m_nNumBeamEnts;
			break;
		}
		case BEAM_LASER:
		{
			beam.type			= TE_BEAMLASER;
			beam.flags			= FBEAM_STARTENTITY | FBEAM_ENDENTITY;
			beam.entity[0]		= pbeam->m_hAttachEntity[0];
			beam.attachmentIndex[0]	= pbeam->m_nAttachIndex[0];
			beam.entity[1]		= pbeam->m_hAttachEntity[1];
			beam.attachmentIndex[1]	= pbeam->m_nAttachIndex[1];
			beam.numAttachments	= pbeam->m_nNumBeamEnts;
			break;
		}
		case BEAM_SPLINE:
		{
			beam.type			= TE_BEAMSPLINE;
			beam.flags			= FBEAM_STARTENTITY | FBEAM_ENDENTITY;
			beam.numAttachments	= pbeam->m_nNumBeamEnts;
			for (int i=0;i<beam.numAttachments;i++)
			{
				beam.entity[i]		= pbeam->m_hAttachEntity[i];
				beam.attachmentIndex[i]	= pbeam->m_nAttachIndex[i];
			}
			break;
		}
		case BEAM_ENTPOINT:
		{
			beam.type			= TE_BEAMPOINTS;
			beam.flags = 0;
			beam.entity[0]		= pbeam->m_hAttachEntity[0];
			beam.attachmentIndex[0]	= pbeam->m_nAttachIndex[0];
			beam.entity[1]		= pbeam->m_hAttachEntity[1];
			beam.attachmentIndex[1]	= pbeam->m_nAttachIndex[1];
			if ( beam.entity[0].Get() )
			{
				beam.flags |= FBEAM_STARTENTITY;
			}
			if ( beam.entity[1].Get() )
			{
				beam.flags |= FBEAM_ENDENTITY;
			}
			beam.numAttachments	= pbeam->m_nNumBeamEnts;
			break;
		}
		case BEAM_POINTS:
			// Already set up
			break;
	}

	beam.flags |= pbeam->GetBeamFlags() & (FBEAM_SINENOISE|FBEAM_SOLID|FBEAM_SHADEIN|FBEAM_SHADEOUT|FBEAM_NOTILE);

	if ( beam.entity[0] )
	{
		// don't draw viewmodel effects in reflections
		if ( CurrentViewID() == VIEW_REFLECTION )
		{
			if ( g_pClientLeafSystem->IsRenderingWithViewModels( beam.entity[0]->RenderHandle() ) )
				return;
		}
	}

	beam.m_flHDRColorScale = pbeam->GetHDRColorScale();

	// per-frame update
	UpdateBeam( &beam, gpGlobals->frametime, pbeam  );

	// draw!
	DrawBeam( &beam );
}

using namespace ::ks::reflect;

extern IntroData_t *g_pIntroData;

// Arbitrary limit so that bad entity logic on the server can't consume tons of memory on the client.
static constexpr auto MAX_SHAKES = 32;

//-----------------------------------------------------------------------------
// Purpose: Screen fade variables
//-----------------------------------------------------------------------------
struct screenfade_t
{
	float		Speed;		// How fast to fade (tics / second) (+ fade in, - fade out)
	float		End;		// When the fading hits maximum
	float		Reset;		// When to reset to not fading (for fadeout and hold)
	byte		r, g, b, alpha;	// Fade color
	int			Flags;		// Fading flags

	DECLARE_SIMPLE_DATADESC();
};

IMPLEMENT_REFLECT_DATAMAP_SIMPLE( screenfade_t )

//-----------------------------------------------------------------------------
// Purpose: Screen shake variables
//-----------------------------------------------------------------------------
struct screenshake_t 
{
	float	endtime;
	float	duration;
	float	amplitude;
	float	frequency;
	float	nextShake;
	Vector	offset;
	float	angle;
	int		command;
	Vector  direction; // used only by kSHAKE_DIRECTIONAL

	// there are different types of screenshake -- 
	// eventually these different types could become
	// proper classes, but given the existing infrastructure
	// for transmitting screenshakes, right now it's just 
	// easier to use an enum and switch.
	enum ShakeType_t 
	{
		kSHAKE_BASIC,  ///< the original screenshake mechanism, a random offset selected every few frames.
		kSHAKE_DIRECTIONAL,  ///< a pseudo-damped-spring-ish punch to a specific screen space direction.
	};
	uint8 nShakeType; // actually a ShakeType_t, packed into eight bits (the datadesc system doesn't like bitfields)

	screenshake_t() : nShakeType(kSHAKE_BASIC) {};  // nothing else is explicitly initialized

	DECLARE_SIMPLE_DATADESC();
};

IMPLEMENT_REFLECT_DATAMAP_SIMPLE( screenshake_t )

void CC_Shake_Stop();
//-----------------------------------------------------------------------------
// Purpose: Screen tilt variables
//-----------------------------------------------------------------------------
struct screentilt_t 
{
	bool	easeInOut;
	QAngle	angle;
	float	starttime;
	float	endtime;
	float	duration;
	float	tiltTime;
	Vector	offset;
	int		command;

	DECLARE_SIMPLE_DATADESC();
};

IMPLEMENT_REFLECT_DATAMAP_SIMPLE( screentilt_t )

//-----------------------------------------------------------------------------
// Purpose: Implements the view effects interface for the client .dll
//-----------------------------------------------------------------------------
class CViewEffects : public IViewEffects
{
public:

	~CViewEffects()
	{
		ClearAllFades();
	}

	virtual void	Init( void );
	virtual void	LevelInit( void );
	virtual void	GetFadeParams( byte *r, byte *g, byte *b, byte *a, bool *blend );
	virtual void	CalcShake( void );
	virtual void	ApplyShake( Vector& origin, QAngle& angles, float factor );
	virtual void	CalcTilt( void );
	virtual void	ApplyTilt( QAngle& angles, float factor );

	virtual void	Shake( const ScreenShake_t &data );
	virtual void	Tilt( ScreenTilt_t &data );
	virtual void	Fade( ScreenFade_t &data );
	virtual void	ClearPermanentFades( void );
	virtual void	FadeCalculate( void );
	virtual void	ClearAllFades( void );

	// Save / Restore

	CUserMessageBinder m_UMCMsgShake;
	CUserMessageBinder m_UMCMsgFade;

private:

	void ClearAllShakes();
	screenshake_t *FindLongestShake();

	void ClearAllTilts();
	screentilt_t *FindLongestTilt();

	// helper subfunctions used inside CalcShake
	void CalcShake_Basic( screenshake_t * pShake, float * __restrict__ pflRumbleAngle );
	void CalcShake_Directional( screenshake_t * pShake, float * __restrict__ pflRumbleAngle );

	std::vector< screenfade_t * >	m_FadeList;

	std::vector< screenshake_t * >	m_ShakeList;
	Vector m_vecShakeAppliedOffset;
	float m_flShakeAppliedAngle;

	std::vector< screentilt_t * >	m_TiltList;
	QAngle m_vecTiltAppliedAngle;

	int							m_FadeColorRGBA[4];
	bool						m_bModulate;

	friend void CC_Shake_Stop();
};

static CViewEffects g_ViewEffects[ MAX_SPLITSCREEN_PLAYERS ];

IViewEffects *GetViewEffects()
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	return &g_ViewEffects[ GET_ACTIVE_SPLITSCREEN_SLOT() ];
}

static CViewEffects &GetCViewEffects()
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	return g_ViewEffects[ GET_ACTIVE_SPLITSCREEN_SLOT() ];
}

// Callback function to call at end of screen m_Fade.
static int s_nCallbackParameter;
static void ( *s_pfnFadeDoneCallback )( int parm1 );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pszName - 
//			iSize - 
//			*pbuf - 
// Output : static int
//-----------------------------------------------------------------------------
bool __MsgFunc_Shake( const ks::net::CCSUsrMsg_Shake &msg )
{
	ScreenShake_t shake;

	shake.command	= (ShakeCommand_t)( msg.command.get() );
	shake.amplitude = msg.local_amplitude;
	shake.frequency = msg.frequency;
	shake.duration	= msg.duration;

	GetCViewEffects().Shake( shake );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pszName - 
//			iSize - 
//			*pbuf - 
// Output : static int
//-----------------------------------------------------------------------------
void __MsgFunc_ShakeDir( bf_read &msg )
{
	ScreenShake_t shake;

	shake.command	= (ShakeCommand_t)msg.ReadByte();
	shake.amplitude = msg.ReadFloat();
	shake.frequency = msg.ReadFloat();
	shake.duration	= msg.ReadFloat();
	msg.ReadBitVec3Normal( shake.direction );

	GetCViewEffects().Shake( shake );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pszName - 
//			iSize - 
//			*pbuf - 
// Output : static int
//-----------------------------------------------------------------------------
void __MsgFunc_Tilt( bf_read &msg )
{
	ScreenTilt_t tilt;

	Vector vecAngle;

	tilt.command = msg.ReadByte();
	tilt.easeInOut = msg.ReadByte() ? true : false;
	tilt.angle.x = msg.ReadFloat();
	tilt.angle.y = msg.ReadFloat();
	tilt.angle.z = msg.ReadFloat();
	tilt.duration = msg.ReadFloat();
	tilt.time = msg.ReadFloat();

	GetCViewEffects().Tilt( tilt );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pszName - 
//			iSize - 
//			*pbuf - 
// Output : static int
//-----------------------------------------------------------------------------
bool __MsgFunc_Fade( const ks::net::CCSUsrMsg_Fade &msg )
{
	ScreenFade_t fade;

	fade.duration = msg.duration; // fade lasts this long
	fade.holdTime = msg.hold_time; // fade lasts this long
	fade.fadeFlags = msg.flags; // fade type (in / out)
	fade.r = msg.clr->r; // fade red
	fade.g = msg.clr->g; // fade green
	fade.b = msg.clr->b; // fade blue
	fade.a = msg.clr->a; // fade blue

	GetCViewEffects().Fade( fade );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CViewEffects::Init( void )
{
	HOOK_MESSAGE( Shake );
	HOOK_MESSAGE( Fade );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CViewEffects::LevelInit( void )
{
	ClearAllShakes();
	ClearAllTilts();
	ClearAllFades();
}

static ConVar shake_show( side_registry(), "shake_show", "0", 0, "Displays a list of the active screen shakes." );

//-----------------------------------------------------------------------------
// Purpose: Stops all active screen shakes.
//-----------------------------------------------------------------------------
void CC_Shake_Stop()
{
	GetCViewEffects().ClearAllShakes();
}
static ConCommand shake_stop(side_registry(), "shake_stop", CC_Shake_Stop, "Stops all active screen shakes.\n", FCVAR_CHEAT );

//-----------------------------------------------------------------------------
// Purpose: Test a punch-type screen shake
//-----------------------------------------------------------------------------
void CC_Shake_TestPunch( const CCommand &args )
{
	if ( args.ArgC() < 7 )
	{
		Msg("Usage: %s x y z f a d\n"
			"where x,y,z are direction of screen punch\n"
			"      f     is  frequency (1 means three bounces before settling)\n"
			"      a     is  amplitude\n"
			"      d     is  duration\n", 
			args[0] );
	}

	const float x = atof( args[1] );
	const float y = atof( args[2] );
	const float z = atof( args[3] );
	const float f = atof( args[4] );
	const float a = atof( args[5] );
	const float d = atof( args[6] );

	ScreenShake_t shake;
	shake.command = SHAKE_START;
	shake.amplitude = a;
	shake.frequency = f;
	shake.duration  = d;
	shake.direction = Vector(x,y,z);

	GetCViewEffects().Shake(shake);
}

static ConCommand shake_testpunch(side_registry(), "shake_testpunch", CC_Shake_TestPunch, "Test a punch-style screen shake.\n", FCVAR_CHEAT );

//-----------------------------------------------------------------------------
// Purpose: Apply noise to the eye position.
// UNDONE: Feedback a bit of this into the view model position.  It shakes too much
//-----------------------------------------------------------------------------
void CViewEffects::CalcShake( void )
{

	// We'll accumulate the aggregate shake for this frame into these data members.
	m_vecShakeAppliedOffset.Init(0, 0, 0);
	m_flShakeAppliedAngle = 0;
	float flRumbleAngle = 0;

	bool bShow = shake_show.GetBool();

	int nShakeCount = std::ssize( m_ShakeList );

	for ( int nShake = nShakeCount - 1; nShake >= 0; nShake-- )
	{
		screenshake_t * __restrict__ pShake = m_ShakeList[ nShake ];

		if ( pShake->endtime == 0 )
		{
			// Shouldn't be any such shakes in the list.
			contract_assert( false && "A screenshake has null endtime in CViewEffects::CalcShake\n" );
			continue;
		}

		if ( ( gpGlobals->curtime > pShake->endtime ) || 
			pShake->duration <= 0 || 
			pShake->amplitude <= 0 || 
			pShake->frequency <= 0 )
		{
			// Retire this shake.
			delete m_ShakeList[ nShake ];
			ks::swap_remove( m_ShakeList, nShake );
			continue;
		}

		if ( bShow )
		{
			con_nprint_t np;
			np.time_to_live = 2.0f;
			np.fixed_width_font = true;
			np.color[0] = 1.0;
			np.color[1] = 0.8;
			np.color[2] = 0.1;
			np.index = nShake + 2;

			engine->Con_NXPrintf( &np, "%02d: dur(%8.2f) amp(%8.2f) freq(%8.2f)", nShake + 1, (double)pShake->duration, (double)pShake->amplitude, (double)pShake->frequency );
		}

		// select the appropriate behavior based on screenshake type
		switch ( pShake->nShakeType )
		{
		case screenshake_t::kSHAKE_BASIC:
			CalcShake_Basic( pShake, &flRumbleAngle );
			break;
		case screenshake_t::kSHAKE_DIRECTIONAL:
			CalcShake_Directional( pShake, &flRumbleAngle );
			break;
		default:
			contract_assert( false && "Unknown shake type %d\n" );
		}

	}

	// Feed this to the rumble system!
	UpdateScreenShakeRumble( XBX_GetActiveUserId(), flRumbleAngle );
}

void CViewEffects::CalcShake_Basic( screenshake_t * __restrict__ pShake, float * __restrict__ pflRumbleAngle )
{
	float	fraction, freq;

	if ( gpGlobals->curtime > pShake->nextShake )
	{
		// Higher frequency means we recalc the extents more often and perturb the display again
		pShake->nextShake = gpGlobals->curtime + (1.0f / pShake->frequency);

		// Compute random shake extents (the shake will settle down from this)
		for (int i = 0; i < 3; i++ )
		{
			pShake->offset[i] = random_valve->RandomFloat( -pShake->amplitude, pShake->amplitude );
		}

		pShake->angle = random_valve->RandomFloat( -pShake->amplitude*0.25, pShake->amplitude*0.25 );
	}

	// Ramp down amplitude over duration (fraction goes from 1 to 0 linearly with slope 1/duration)
	fraction = ( pShake->endtime - gpGlobals->curtime ) / pShake->duration;

	// Ramp up frequency over duration
	if ( fraction )
	{
		freq = (pShake->frequency / fraction);
	}
	else
	{
		freq = 0;
	}

	// square fraction to approach zero more quickly
	fraction *= fraction;

	// Sine wave that slowly settles to zero
	float angle = gpGlobals->curtime * freq;
	if ( angle > 1e8 )
	{
		angle = 1e8;
	}
	fraction = fraction * std::sin( angle );

	if( pShake->command != SHAKE_START_NORUMBLE )
	{
		// As long as this isn't a NO RUMBLE effect, then accumulate rumble
		*pflRumbleAngle += pShake->angle * fraction;
	}

	if( pShake->command != SHAKE_START_RUMBLEONLY )
	{
		// As long as this isn't a RUMBLE ONLY effect, then accumulate screen shake

		// Add to view origin
		m_vecShakeAppliedOffset += pShake->offset * fraction;

		// Add to roll
		m_flShakeAppliedAngle += pShake->angle * fraction;
	}

	// Drop amplitude a bit, less for higher frequency shakes
	pShake->amplitude -= pShake->amplitude * ( gpGlobals->frametime / (pShake->duration * pShake->frequency) );

}

void CViewEffects::CalcShake_Directional( screenshake_t * __restrict__ pShake, float * __restrict__ pflRumbleAngle )
{
	// a screen punch follows an equation of the form 
	// y = sin(fx) * ( 1 - x / (3pi) ) for x=0..3pi
	// where the duration is transformed to occupy 
	// the region 0..3pi
	// and the frequency can be any number (which controls the number of oscillations 
	// before lapsing out)

	// because of the resolution of this shake, it is performed every frame
	// (ignores nextShake)
	pShake->nextShake = gpGlobals->curtime + 0.001;
	float t = 1 - ( pShake->endtime - gpGlobals->curtime ) / pShake->duration; // t varies 0 .. 1 over life of shake
	float fraction = ( 1 - t ); // compiler will hopefully elide the double subtraction

	// transform the duration and so that x varies 0 .. 3PI over lifespan
	t *= ( 3 * std::numbers::pi ); // t varies 0 .. 3PI 

	const float x = t * pShake->frequency;
	const float y = std::sin(x) * fraction;

	// transform this -1..1 sinusoid by amplitude and direction
	pShake->offset = pShake->direction * ( pShake->amplitude * y );

	if( pShake->command != SHAKE_START_NORUMBLE )
	{
		// As long as this isn't a NO RUMBLE effect, then accumulate rumble
		*pflRumbleAngle += y;
	}

	if( pShake->command != SHAKE_START_RUMBLEONLY )
	{
		// As long as this isn't a RUMBLE ONLY effect, then accumulate screen shake

		// Add to view origin
		m_vecShakeAppliedOffset += pShake->offset ;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Apply the current screen shake to this origin/angles.  Factor is the amount to apply
//  This is so you can blend in part of the shake
// Input  : origin - 
//			angles - 
//			factor - 
//-----------------------------------------------------------------------------
void CViewEffects::ApplyShake( Vector& origin, QAngle& angles, float factor )
{
	VectorMA( origin, factor, m_vecShakeAppliedOffset, origin );
	angles.z += m_flShakeAppliedAngle * factor;
}

//-----------------------------------------------------------------------------
// Purpose: Apply noise to the eye position.
// UNDONE: Feedback a bit of this into the view model position.  It shakes too much
//-----------------------------------------------------------------------------
void CViewEffects::CalcTilt( void )
{
	m_vecTiltAppliedAngle.Init();

	int nTiltCount = std::ssize( m_TiltList );

	for ( int nTilt = nTiltCount - 1; nTilt >= 0; nTilt-- )
	{
		screentilt_t *pTilt = m_TiltList[ nTilt ];

		if ( pTilt->endtime == 0 )
		{
			// Shouldn't be any such tilts in the list.
			contract_assert( false );
			continue;
		}

		if ( ( gpGlobals->curtime > pTilt->endtime ) || 
			pTilt->duration <= 0 || pTilt->angle == QAngle( 0.0f, 0.0f, 0.0f ) )
		{
			// Retire this tilt.
			delete m_TiltList[ nTilt ];
			ks::swap_remove( m_TiltList, nTilt );
			continue;
		}

		float flInterp = ( gpGlobals->curtime - pTilt->starttime ) / pTilt->tiltTime;

		float flReturnInterp = ( pTilt->endtime - gpGlobals->curtime ) / pTilt->tiltTime;

		if ( flReturnInterp < flInterp )
		{
			flInterp = flReturnInterp;
		}

		float flSmoothInterp = clamp( flInterp, 0.0f, 1.0f );

		if ( pTilt->easeInOut )
		{
			// Do a smooth ease in and out
			flSmoothInterp = 1.0f - 0.5f * ( std::cosf( flSmoothInterp * std::numbers::pi ) + 1.0f );
		}

		// Accumulate world tilt
		m_vecTiltAppliedAngle += pTilt->angle * flSmoothInterp;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Apply the current screen shake to this origin/angles.  Factor is the amount to apply
//  This is so you can blend in part of the shake
// Input  : origin - 
//			angles - 
//			factor - 
//-----------------------------------------------------------------------------
void CViewEffects::ApplyTilt( QAngle& angles, float factor )
{
	if ( m_vecTiltAppliedAngle == QAngle( 0.0f, 0.0f, 0.0f ) )
	{
		// Fast out, no tilt to apply
		return;
	}

	matrix3x4_t matTilt;
	AngleIMatrix( m_vecTiltAppliedAngle, matTilt );

	matrix3x4_t matToWorld;
	AngleMatrix( angles, matToWorld );

	matrix3x4_t matTiltToWorld;
	ConcatTransforms( matTilt, matToWorld, matTiltToWorld);

	Vector vecForwardTilted, vecUpTilted;
	VectorTransform( Vector( 1.0f, 0.0, 0.0f ), matTiltToWorld, vecForwardTilted );
	VectorTransform( Vector( 0.0f, 0.0, 1.0f ), matTiltToWorld, vecUpTilted );

	QAngle anglesTilted;
	VectorAngles( vecForwardTilted, vecUpTilted, anglesTilted );

	angles = anglesTilted;
}

//-----------------------------------------------------------------------------
// Purpose: Zeros out all active screen shakes.
//-----------------------------------------------------------------------------
void CViewEffects::ClearAllShakes()
{
	int nShakeCount = std::ssize( m_ShakeList );
	for ( int i = 0; i < nShakeCount; i++ )
	{
		delete m_ShakeList[ i ];
	}

	m_ShakeList.clear(); m_ShakeList.shrink_to_fit();
}

//-----------------------------------------------------------------------------
// Purpose: Returns the shake with the longest duration. This is the shake we
//			use anytime we get an amplitude or frequency command, because the
//			most likely case is that we're modifying a shake with a long
//			duration rather than a brief shake caused by an explosion, etc.
//-----------------------------------------------------------------------------
screenshake_t *CViewEffects::FindLongestShake()
{
	screenshake_t *pLongestShake = nullptr;

	int nShakeCount = std::ssize( m_ShakeList );
	for ( int i = 0; i < nShakeCount; i++ )
	{
		screenshake_t *pShake = m_ShakeList[ i ];
		if ( pShake && ( !pLongestShake || ( pShake->duration > pLongestShake->duration ) ) )
		{
			pLongestShake = pShake;
		}
	}

	return pLongestShake;
}

//-----------------------------------------------------------------------------
// Purpose: Message hook to parse ScreenShake messages
// Input  : pszName - 
//			iSize - 
//			pbuf - 
// Output : 
//-----------------------------------------------------------------------------
void CViewEffects::Shake( const ScreenShake_t &data )
{
	if ( ( data.command == SHAKE_START || data.command == SHAKE_START_RUMBLEONLY ) && ( std::ssize( m_ShakeList ) < MAX_SHAKES ) )
	{
		screenshake_t * __restrict__ pNewShake = new screenshake_t; // ugh, should just make these a static array
			
		pNewShake->amplitude = data.amplitude;
		pNewShake->frequency = data.frequency;
		pNewShake->duration = data.duration;
		pNewShake->nextShake = 0;
		pNewShake->endtime = gpGlobals->curtime + data.duration;
		pNewShake->command = data.command;
		pNewShake->direction = data.direction;
		pNewShake->nShakeType = data.direction.IsZeroFast() ? screenshake_t::kSHAKE_BASIC : screenshake_t::kSHAKE_DIRECTIONAL;

		m_ShakeList.push_back( (screenshake_t *) pNewShake );
	}
	else if ( data.command == SHAKE_STOP)
	{
		ClearAllShakes();
	}
	else if ( data.command == SHAKE_AMPLITUDE )
	{
		// Look for the most likely shake to modify.
		screenshake_t * __restrict__ pShake = FindLongestShake();
		if ( pShake )
		{
			pShake->amplitude = data.amplitude;
		}
	}
	else if ( data.command == SHAKE_FREQUENCY )
	{
		// Look for the most likely shake to modify.
		screenshake_t * __restrict__ pShake = FindLongestShake();
		if ( pShake )
		{
			pShake->frequency = data.frequency;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Zeros out all active screen tilts.
//-----------------------------------------------------------------------------
void CViewEffects::ClearAllTilts()
{
	int nTiltCount = std::ssize( m_TiltList );
	for ( int i = 0; i < nTiltCount; i++ )
	{
		delete m_TiltList[ i ];
	}

	m_TiltList.clear(); m_TiltList.shrink_to_fit();
}

//-----------------------------------------------------------------------------
// Purpose: Returns the shake with the longest duration. This is the shake we
//			use anytime we get an amplitude or frequency command, because the
//			most likely case is that we're modifying a shake with a long
//			duration rather than a brief shake caused by an explosion, etc.
//-----------------------------------------------------------------------------
screentilt_t *CViewEffects::FindLongestTilt()
{
	screentilt_t *pLongestTilt = nullptr;

	int nTiltCount = std::ssize( m_TiltList );
	for ( int i = 0; i < nTiltCount; i++ )
	{
		screentilt_t *pTilt = m_TiltList[ i ];
		if ( pTilt && ( !pLongestTilt || ( pTilt->duration > pLongestTilt->duration ) ) )
		{
			pLongestTilt = pTilt;
		}
	}

	return pLongestTilt;
}

//-----------------------------------------------------------------------------
// Purpose: Message hook to parse ScreenTilt messages
// Input  : pszName - 
//			iSize - 
//			pbuf - 
// Output : 
//-----------------------------------------------------------------------------
void CViewEffects::Tilt( ScreenTilt_t &data )
{
	if ( ( data.command == SHAKE_START || data.command == SHAKE_START_RUMBLEONLY ) && ( std::ssize( m_ShakeList ) < MAX_SHAKES ) )
	{
		screentilt_t *pNewTilt = new screentilt_t;

		pNewTilt->easeInOut = data.easeInOut;
		pNewTilt->angle = data.angle;
		pNewTilt->duration = data.duration;
		pNewTilt->tiltTime = data.time;
		pNewTilt->starttime = gpGlobals->curtime;
		pNewTilt->endtime = pNewTilt->starttime + data.duration;
		pNewTilt->command = data.command;

		m_TiltList.push_back( pNewTilt );
	}
	else if ( data.command == SHAKE_STOP)
	{
		ClearAllTilts();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Message hook to parse ScreenFade messages
// Input  : *pszName - 
//			iSize - 
//			*pbuf - 
// Output : int
//-----------------------------------------------------------------------------
void CViewEffects::Fade( ScreenFade_t &data )
{
	// Create a new fade and append it to the list
	screenfade_t *pNewFade = new screenfade_t;
	pNewFade->End	= data.duration * (1.0f/(float)(1<<SCREENFADE_FRACBITS));
	pNewFade->Reset	= data.holdTime * (1.0f/(float)(1<<SCREENFADE_FRACBITS));
	pNewFade->r		= data.r;
	pNewFade->g		= data.g;
	pNewFade->b		= data.b;
	pNewFade->alpha	= data.a;
	pNewFade->Flags	= data.fadeFlags;
	pNewFade->Speed	= 0;

	// Calc fade speed
	if ( data.duration > 0 )
	{
		if ( data.fadeFlags & FFADE_OUT )
		{
			if ( pNewFade->End )
			{
				pNewFade->Speed = -(float)pNewFade->alpha / pNewFade->End;
			}

			pNewFade->End	+= gpGlobals->curtime;
			pNewFade->Reset	+= pNewFade->End;
		}
		else
		{
			if ( pNewFade->End )
			{
				pNewFade->Speed = (float)pNewFade->alpha / pNewFade->End;
			}

			pNewFade->Reset	+= gpGlobals->curtime;
			pNewFade->End	+= pNewFade->Reset;
		}
	}

	if ( data.fadeFlags & FFADE_PURGE )
	{
		ClearAllFades();
	}

	m_FadeList.push_back( pNewFade );
}

//-----------------------------------------------------------------------------
// Purpose: Compute the overall color & alpha of the fades
//-----------------------------------------------------------------------------
void CViewEffects::FadeCalculate( void )
{
	// Cycle through all fades and remove any that have finished (work backwards)
	int i;
	int iSize = std::ssize( m_FadeList );
	for (i = iSize-1; i >= 0; i-- )
	{
		screenfade_t *pFade = m_FadeList[i];

		// Keep pushing reset time out indefinitely
		if ( pFade->Flags & FFADE_STAYOUT )
		{
			pFade->Reset = gpGlobals->curtime + 0.1;
		}

		// All done?
		if ( ( gpGlobals->curtime > pFade->Reset ) && ( gpGlobals->curtime > pFade->End ) )
		{
			// User passed in a callback function, call it now
			if ( s_pfnFadeDoneCallback )
			{
				s_pfnFadeDoneCallback( s_nCallbackParameter );
				s_pfnFadeDoneCallback = nullptr;
				s_nCallbackParameter = 0;
			}

			// Remove this Fade from the list
			ks::erase_first( m_FadeList, pFade );
			delete pFade;
		}
	}

	m_bModulate = false;
	m_FadeColorRGBA[0] = m_FadeColorRGBA[1] = m_FadeColorRGBA[2] = m_FadeColorRGBA[3] = 0;

	// Cycle through all fades in the list and calculate the overall color/alpha
	for ( i = 0; i < std::ssize( m_FadeList ); i++ )
	{
		screenfade_t *pFade = m_FadeList[i];

		// Color
		m_FadeColorRGBA[0] += pFade->r;
		m_FadeColorRGBA[1] += pFade->g;
		m_FadeColorRGBA[2] += pFade->b;

		// Fading...
		int iFadeAlpha;
		if ( pFade->Flags & (FFADE_OUT|FFADE_IN) )
		{
			iFadeAlpha = pFade->Speed * ( pFade->End - gpGlobals->curtime );
			if ( pFade->Flags & FFADE_OUT )
			{
				iFadeAlpha += pFade->alpha;
			}
			iFadeAlpha = std::min<std::common_type_t<int, byte>>( iFadeAlpha, pFade->alpha );
			iFadeAlpha = std::max( 0, iFadeAlpha );
		}
		else
		{
			iFadeAlpha = pFade->alpha;
		}

		// Use highest alpha
		if ( iFadeAlpha > m_FadeColorRGBA[3] )
		{
			m_FadeColorRGBA[3] = iFadeAlpha;
		}

		// Modulate?
		if ( pFade->Flags & FFADE_MODULATE )
		{
			m_bModulate = true;
		}
	}

	// Divide colors
	if ( !m_FadeList.empty() )
	{
		m_FadeColorRGBA[0] /= std::ssize( m_FadeList );
		m_FadeColorRGBA[1] /= std::ssize( m_FadeList );
		m_FadeColorRGBA[2] /= std::ssize( m_FadeList );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Clear only the permanent fades in our fade list
//-----------------------------------------------------------------------------
void CViewEffects::ClearPermanentFades( void )
{
	int iSize = std::ssize( m_FadeList );
	for (int i =  iSize-1; i >= 0; i-- )
	{
		screenfade_t *pFade = m_FadeList[i];

		if ( pFade->Flags & FFADE_STAYOUT )
		{
			// Destroy this fade
			ks::erase_first( m_FadeList, pFade );
			delete pFade;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Purge & delete all fades in the queue
//-----------------------------------------------------------------------------
void CViewEffects::ClearAllFades( void )
{
	int iSize = std::ssize( m_FadeList );
	for (int i =  iSize-1; i >= 0; i-- )
	{
		delete m_FadeList[i];
	}
	m_FadeList.clear(); m_FadeList.shrink_to_fit();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : context - Which call to Render is this ( CViewSetup::context )
//			*r - 
//			*g - 
//			*b - 
//			*a - 
//			*blend - 
//-----------------------------------------------------------------------------
void CViewEffects::GetFadeParams( byte *r, byte *g, byte *b, byte *a, bool *blend )
{
	// If the intro is overriding our fade, use that instead
	if ( g_pIntroData && g_pIntroData->m_flCurrentFadeColor[3] )
	{
		*r = g_pIntroData->m_flCurrentFadeColor[0];
		*g = g_pIntroData->m_flCurrentFadeColor[1];
		*b = g_pIntroData->m_flCurrentFadeColor[2];
		*a = g_pIntroData->m_flCurrentFadeColor[3];
		*blend = false;
		return;
	}

	FadeCalculate();

	*r = m_FadeColorRGBA[0];
	*g = m_FadeColorRGBA[1];
	*b = m_FadeColorRGBA[2];
	*a = m_FadeColorRGBA[3];
	*blend = m_bModulate;
}

//====================================================================================================
// CLIENTSIDE VIEW EFFECTS SAVE/RESTORE 
//====================================================================================================
static short VIEWEFFECTS_SAVE_RESTORE_VERSION = 2;

//-----------------------------------------------------------------------------

using namespace ::ks::reflect;

// Purpose: Responsible for drawing the scene

//-----------------------------------------------------------------------------
// Convars related to controlling rendering
//-----------------------------------------------------------------------------
ConVar r_updaterefracttexture( side_registry(), "r_updaterefracttexture", "1", FCVAR_CHEAT );
ConVar r_depthoverlay( side_registry(), "r_depthoverlay", "0", FCVAR_CHEAT, "Replaces opaque objects with their grayscaled depth values. r_showz_power scales the output." );

// frame we last updated the refract texture in (compared to gpGlobals->framecount)
int g_viewscene_refractUpdateFrame = 0;

// an index which is incremented once per portal render
int g_nCurrentPortalRender = 0;
// the value of g_nCurrentPortalRender when we last updated the refract texture 
// (basically, portal renders invalidate the refract texture)
int g_nRefractUpdatePortalRender = 0;

bool g_bAllowMultipleRefractUpdatesPerScenePerFrame = false;

void ViewTransform( const Vector &worldSpace, Vector &viewSpace )
{
	const VMatrix &viewMatrix = engine->WorldToViewMatrix();
	Vector3DMultiplyPosition( viewMatrix, worldSpace, viewSpace );
}

//-----------------------------------------------------------------------------
// Purpose: UNDONE: Clean this up some, handle off-screen vertices
// Input  : *point - 
//			*screen - 
// Output : int
//-----------------------------------------------------------------------------
int ScreenTransform( const Vector& point, Vector& screen )
{
	// UNDONE: Clean this up some, handle off-screen vertices
	float w;
	const VMatrix &worldToScreen = engine->WorldToScreenMatrix();

	screen.x = worldToScreen[0][0] * point[0] + worldToScreen[0][1] * point[1] + worldToScreen[0][2] * point[2] + worldToScreen[0][3];
	screen.y = worldToScreen[1][0] * point[0] + worldToScreen[1][1] * point[1] + worldToScreen[1][2] * point[2] + worldToScreen[1][3];
	//	z		 = worldToScreen[2][0] * point[0] + worldToScreen[2][1] * point[1] + worldToScreen[2][2] * point[2] + worldToScreen[2][3];
	w		 = worldToScreen[3][0] * point[0] + worldToScreen[3][1] * point[1] + worldToScreen[3][2] * point[2] + worldToScreen[3][3];

	// Just so we have something valid here
	screen.z = 0.0f;

	bool behind;
	if( w < 0.001f )
	{
		behind = true;
		screen.x *= 100000;
		screen.y *= 100000;
	}
	else
	{
		behind = false;
		float invw = 1.0f / w;
		screen.x *= invw;
		screen.y *= invw;
	}

	return behind;
}

void UpdateFullScreenDepthTexture( void )
{
	if ( g_pMaterialSystemHardwareConfig->GetDXSupportLevel() <= 90 )
		return;

	ITexture *pDepthTex = GetFullFrameDepthTexture();
	CMatRenderContextPtr pRenderContext( materials );

	Rect_t viewportRect;
	pRenderContext->GetViewport( viewportRect.x, viewportRect.y, viewportRect.width, viewportRect.height );

	{
		pRenderContext->CopyRenderTargetToTextureEx( pDepthTex, -1, nullptr, nullptr );
	}

	pRenderContext->SetFullScreenDepthTextureValidityFlag( true );

	if ( r_depthoverlay.GetBool() )
	{
		IMaterial *pMaterial = materials->FindMaterial( "debug/showz", TEXTURE_GROUP_OTHER, true );
		IMaterialVar *BaseTextureVar = pMaterial->FindVar( "$basetexture", nullptr, false );
		IMaterialVar *pDepthInAlpha = nullptr;
		pDepthInAlpha = pMaterial->FindVar( "$ALPHADEPTH", nullptr, false );
		pDepthInAlpha->SetIntValue( 1 );
		
		BaseTextureVar->SetTextureValue( pDepthTex );

		pRenderContext->OverrideDepthEnable( true, false ); //don't write to depth, or else we'll never see translucents
		pRenderContext->DrawScreenSpaceQuad( pMaterial );
		pRenderContext->OverrideDepthEnable( false, true );
	}
}
