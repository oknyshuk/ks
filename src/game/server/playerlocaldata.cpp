// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2007 Valve Corporation

module;
#include <string.h>
#include <math.h>
#include "dt_send.h"
#include "edict.h"
#include "networkvar.h"
#include "reflect.h"
#include "reflect_datamap.h"
#include "reflect_sendtable.h"
#include "eiface.h"
#include "GameUI/IGameUI.h"
#include "declare_class.h"
#include "igameevents.h"
#include "responserules/response_types.h"
#include "studio.h"
#include "vphysics_interface.h"
#include "baseentity.h"
#include "entity_dispatch.h"
#include "entity_dispatch_list.h"
#include "entity_kinds_dump.h"
#include "util.h"
#include "server_class.h"
#include "worldsize.h"
#include "shareddefs.h"
#include "vscript/ivscript.h"
#include "datacache/iprecachesystem.h"
#include "datacache/iresourceaccesscontrol.h"
#include "filesystem.h"
#include "predictable_entity.h"
module ks.server;

import std;
import ks.core.vprof;
import ks.math.compressed_vector;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.interfaces;
import ks.tier1.reflect_wirevar;
import ks.tier1.vector_ops;
import :sharedvar;
import :vscript;
import :fish;
import :baseentity;
import :player;
import :nav_4;
import :entity_2;
import ks.game.shareddefs;
import :cstrike15.bot.bot;
import :entityutil;
import ks.game.gamemovement;
import ks.game.mapentities_shared;
import ks.tier1.convar;
import ks.tier1.createinterface;
import ks.tier1.keyvalues;
import ks.tier1.registry;
import :entity_dispatch;
import :entity_kind;
import ks.tier1.strtools;
import ks.core.cmdline;
import ks.core.dbg;
import ks.core.basetypes;
import ks.core.random;
import ks.game.beam_flags;
import ks.math.mathlib;
import ks.core.scratcharena;
import ks.math.vmatrix;
import ks.math.ssemath;
import ks.tier1.iconvar;
import ks.math.vector2d;
import ks.core.threadtools;
import ks.game.postprocess_shared;
import :cstrike15.cs_player;

class CBaseEntity;
class CBasePlayer;
class CSkyCamera;
struct fogparams_t;

using namespace ::ks::reflect;

//=============================================================================

IMPLEMENT_REFLECT_TABLE( CPlayerLocalData, DT_Local );

IMPLEMENT_REFLECT_DATAMAP_SIMPLE( fogplayerparams_t )

IMPLEMENT_REFLECT_DATAMAP_SIMPLE( fogparams_t )

IMPLEMENT_REFLECT_DATAMAP_SIMPLE( sky3dparams_t )

IMPLEMENT_REFLECT_DATAMAP_SIMPLE( audioparams_t )

IMPLEMENT_REFLECT_DATAMAP_SIMPLE( CPlayerLocalData )

//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
CPlayerLocalData::CPlayerLocalData()
{
	m_pfnStateChanged = []( CPlayerLocalMoveCore *pCore, void *pVar ) {
		CPlayerLocalData *pThis = static_cast<CPlayerLocalData *>( pCore );
		if ( pVar ) pThis->NetworkStateChanged( pVar );
		else pThis->NetworkStateChanged();
	};
	std::memset( m_chAreaBits.m_Value, 0, sizeof( m_chAreaBits.m_Value ) );
	memset( m_chAreaPortalBits.m_Value, 0, sizeof( m_chAreaPortalBits.m_Value ) );

	m_iHideHUD = 0;
	m_flFOVRate = 0.0f;

	m_vecOverViewpoint.Zero();

	m_bDucked = false;
	m_bDucking = false;
	m_flLastDuckTime = -1.0f;
	m_bInDuckJump = false;
	m_nDuckTimeMsecs = 0; // REI: LEGACY, should be removed
	m_nDuckJumpTimeMsecs = 0; // REI: LEGACY, should be removed
	m_nJumpTimeMsecs = 0;
	m_nStepside = 0;
	m_flFallVelocity = 0.0f;
	m_nOldButtons = 0;
	m_pOldSkyCamera = nullptr;

	// $$$REI What's the safe way to initialize these things in constructor?
	m_viewPunchAngle.m_Value.Init();
	m_aimPunchAngle.m_Value.Init();
	m_aimPunchAngleVel.m_Value.Init();

	m_bDrawViewmodel = true;
	m_bWearingSuit = false;
	m_bPoisoned = false;
	m_flStepSize = 0.0f;
	m_bAllowAutoMovement = true;

	m_bAutoAimTarget = false;

	// m_skybox3d?
	// m_PlayerFog?
	// m_fog?

	// other fields on m_audio?  should this have a constructor too?
	m_audio.soundscapeIndex = 0;
	m_audio.localBits = 0;
	m_audio.entIndex = 0;

	m_bSlowMovement = false;
	m_fTBeamEndTime = 0.0f;
}

static Vector GetPlayerViewPosition( CBasePlayer *pl )
{
	CBaseEntity *pView = pl->GetViewEntity();
	if ( !pView )
	{
		pView = pl;
	}
	return pView->EyePosition();
}

void CPlayerLocalData::UpdateAreaBits( CBasePlayer *pl, unsigned char chAreaPortalBits[MAX_AREA_PORTAL_STATE_BYTES] )
{
	Vector origin = GetPlayerViewPosition(pl);

	unsigned char tempBits[32] = { 0 };

	static_assert( ( sizeof( tempBits ) % 4 ) == 0 );

	int nDWords = sizeof( tempBits ) >> 2;

	static_assert( sizeof( tempBits ) >= sizeof( ((CPlayerLocalData*)0)->m_chAreaBits ) );

	int i;
	int area = engine->GetArea( origin );
	engine->GetAreaBits( area, tempBits, sizeof( tempBits ) );

	std::vector< int > vecAreasSeen;
	vecAreasSeen.push_back( area );

	std::vector< CHandle< CBasePlayer > > &list = pl->GetSplitScreenAndPictureInPicturePlayers();
	for ( i = 0; i < std::ssize( list ); ++i )
	{
		CBasePlayer *pSplit = list[ i ];
		if ( !pSplit )
			continue;

		unsigned char tempBits2[32] = { 0 };
		Vector org2 = GetPlayerViewPosition(pSplit);
		int area2 = engine->GetArea( org2 );

		// Already merged this area in?
		if ( ks::find_index( vecAreasSeen, area2 ) != -1 )
			continue;

		vecAreasSeen.push_back( area2 );
		engine->GetAreaBits( area2, tempBits2, sizeof( tempBits2 ) );

		unsigned int *pBase = (unsigned int *)tempBits;
		unsigned int *pAdd = (unsigned int *)tempBits2;

		// Now merge them together
		for ( int j = 0; j < nDWords; ++j )
		{
			*pBase++ |= *pAdd++;
		}
	}

	for ( i=0; i < m_chAreaBits.Count(); i++ )
	{
		if ( tempBits[i] != m_chAreaBits[ i ] )
		{
			m_chAreaBits.Set( i, tempBits[i] );
		}
	}

	for ( i=0; i < MAX_AREA_PORTAL_STATE_BYTES; i++ )
	{
		if ( chAreaPortalBits[i] != m_chAreaPortalBits[i] )
		{
			m_chAreaPortalBits.Set( i, chAreaPortalBits[i] );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Fills in CClientData values for local player just before sending over wire
// Input  : player - 
//-----------------------------------------------------------------------------

void ClientData_Update( CBasePlayer *pl )
{
	// HACKHACK: for 3d skybox 
	// UNDONE: Support multiple sky cameras?
	CSkyCamera *pSkyCamera = GetCurrentSkyCamera();
	if ( pSkyCamera != pl->m_Local.m_pOldSkyCamera )
	{
		pl->m_Local.m_pOldSkyCamera = pSkyCamera;
		pl->m_Local.m_skybox3d.CopyFrom(pSkyCamera->m_skyboxData);
	}
	else if ( !pSkyCamera )
	{
		pl->m_Local.m_skybox3d.area = 255;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void UpdateAllClientData( void )
{
	CVProfScope _( "UpdateAllClientData", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	int i;
	CBasePlayer *pl;

	for ( i = 1; i <= gpGlobals->maxClients; i++ )
	{
		pl = ( CBasePlayer * )UTIL_PlayerByIndex( i );
		if ( !pl )
			continue;

		ClientData_Update( pl );
	}
}

// Purpose: This class allows the game dll to control what functions 3rd party plugins can
// call on clients.

//-----------------------------------------------------------------------------
// Purpose: An implementation 
//-----------------------------------------------------------------------------
class CPluginHelpersCheck : public IPluginHelpersCheck
{
public:
	virtual bool CreateMessage( const char *plugin, edict_t *pEntity, DIALOG_TYPE type, KeyValues *data );
};

CPluginHelpersCheck s_PluginCheck;
static InterfaceReg s_Reg_CPluginHelpersCheck_IPluginHelpersCheck( side_registry(), []() -> void * { return static_cast<IPluginHelpersCheck *>( &s_PluginCheck ); }, INTERFACEVERSION_PLUGINHELPERSCHECK );;

bool CPluginHelpersCheck::CreateMessage( const char *plugin, edict_t *pEntity, DIALOG_TYPE type, KeyValues *data )
{
	// return false here to disallow a plugin from running this command on this client
	return true;
}

struct inputdata_t;

using namespace ::ks::reflect;

// See interface.h/.cpp for specifics:  basically this ensures that we actually Sys_UnloadModule the dll and that we don't call Sys_LoadModule 
//  over and over again.
static CDllDemandLoader g_GameUI( "gameui" );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CPointBonusMapsAccessor : public CPointEntity
{
public:
	DECLARE_CLASS( CPointBonusMapsAccessor, CPointEntity );
	DECLARE_DATADESC();

	 void	Activate_Impl( void );

	[[= ks::reflect::Input{ .name = "Unlock", .type = FIELD_VOID } ]] void InputUnlock( inputdata_t& inputdata );
	[[= ks::reflect::Input{ .name = "Complete", .type = FIELD_VOID } ]] void InputComplete( inputdata_t& inputdata );
	[[= ks::reflect::Input{ .name = "Save", .type = FIELD_VOID } ]] void InputSave( inputdata_t& inputdata );

private:
	[[= ks::reflect::Key{ .name = "filename" } ]] string_t	m_String_tFileName;
	[[= ks::reflect::Key{ .name = "mapname" } ]] string_t	m_String_tMapName;
	IGameUI		*m_pGameUI;
};

IMPLEMENT_REFLECT_DATAMAP( CPointBonusMapsAccessor )

LINK_ENTITY_TO_CLASS( point_bonusmaps_accessor, CPointBonusMapsAccessor );

void CPointBonusMapsAccessor::Activate_Impl( void )
{
	BaseClass::Activate_Impl();

	CreateInterfaceFn gameUIFactory = g_GameUI.GetFactory();
	if ( gameUIFactory )
	{
		m_pGameUI = (IGameUI *) gameUIFactory(GAMEUI_INTERFACE_VERSION, nullptr );
	}
}

void CPointBonusMapsAccessor::InputUnlock( inputdata_t& inputdata )
{
}

void CPointBonusMapsAccessor::InputComplete( inputdata_t& inputdata )
{
	if ( m_pGameUI )
	{
		int iNumAdvancedComplete = 0;

		IGameEvent *event = gameeventmanager->CreateEvent( "advanced_map_complete" );
		if ( event )
		{
			event->SetInt( "numadvanced", iNumAdvancedComplete );
			gameeventmanager->FireEvent( event );
		}
	}
}

void CPointBonusMapsAccessor::InputSave( inputdata_t& inputdata )
{
}

void BonusMapChallengeUpdate( const char *pchFileName, const char *pchMapName, const char *pchChallengeName, int iBest )
{
	CreateInterfaceFn gameUIFactory = g_GameUI.GetFactory();
	if ( gameUIFactory )
	{
		IGameUI *pGameUI = (IGameUI *) gameUIFactory(GAMEUI_INTERFACE_VERSION, nullptr );
		if ( pGameUI )
		{
			int piNumMedals[ 3 ] = {0};

			IGameEvent *event = gameeventmanager->CreateEvent( "challenge_map_complete" );
			if ( event )
			{
				event->SetInt( "numbronze", piNumMedals[ 0 ] );
				event->SetInt( "numsilver", piNumMedals[ 1 ] );
				event->SetInt( "numgold", piNumMedals[ 2 ] );
				gameeventmanager->FireEvent( event );
			}
		}	
	}
}

void BonusMapChallengeNames( char *pchFileName, char *pchMapName, char *pchChallengeName )
{
}

void BonusMapChallengeObjectives( int &iBronze, int &iSilver, int &iGold )
{
}

class CBaseEntity;
class CPointCamera;
struct inputdata_t;

using namespace ::ks::reflect;

static constexpr auto CAM_THINK_INTERVAL = 0.05;

// Spawnflags
static constexpr auto SF_CAMERA_START_OFF = 0x01;

// UNDONE: Share properly with the client code!!!
static constexpr auto POINT_CAMERA_MSG_SETACTIVE = 1;

CEntityClassList<CPointCamera> g_PointCameraList;
template <> CPointCamera *CEntityClassList<CPointCamera>::m_pClassList = nullptr;

CPointCamera* GetPointCameraList()
{
	return g_PointCameraList.m_pClassList;
}

// These are already built into CBaseEntity
//	DEFINE_KEYFIELD( m_iName, FIELD_STRING, "targetname" ),
//	DEFINE_KEYFIELD( m_iParent, FIELD_STRING, "parentname" ),
//	DEFINE_KEYFIELD( m_target, FIELD_STRING, "target" ),

LINK_ENTITY_TO_CLASS( point_camera, CPointCamera );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPointCamera::~CPointCamera()
{
	g_PointCameraList.Remove( this );
}

CPointCamera::CPointCamera()
{
	// Set these to opposites so that it'll be sent the first time around.
	m_bActive = false;
	m_bIsOn = false;
	
	m_bFogEnable = false;

	g_PointCameraList.Insert( this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointCamera::Spawn_Impl( void )
{
	BaseClass::Spawn_Impl();

	if ( m_spawnflags & SF_CAMERA_START_OFF )
	{
		m_bIsOn = false;
	}
	else
	{
		m_bIsOn = true;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Override ShouldTransmit since we want to be sent even though we don't have a model, etc.
//			All that matters is if we are in the pvs.
//-----------------------------------------------------------------------------
int CPointCamera::UpdateTransmitState_Impl()
{
	if ( m_bActive )
	{
		return SetTransmitState( FL_EDICT_ALWAYS );
	}
	else
	{
		return SetTransmitState( FL_EDICT_DONTSEND );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointCamera::SetActive( bool bActive )
{
	// If the mapmaker's told the camera it's off, it enforces inactive state
	if ( !m_bIsOn )
	{
		bActive = false;
	}

	if ( m_bActive != bActive )
	{
		m_bActive = bActive;
		DispatchUpdateTransmitState();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointCamera::InputChangeFOV( inputdata_t &inputdata )
{
	// Parse the keyvalue data
	char parseString[255];

	V_strncpy(parseString, inputdata.value.String(), sizeof(parseString));

	// Get FOV
	char *pszParam = std::strtok(parseString," ");
	if(pszParam)
	{
		m_TargetFOV = std::atof( pszParam );
	}
	else
	{
		// Assume no change
		m_TargetFOV = m_FOV;
	}

	// Get Time
	float flChangeTime;
	pszParam = strtok(nullptr," ");
	if(pszParam)
	{
		flChangeTime = std::atof( pszParam );
	}
	else
	{
		// Assume 1 second.
		flChangeTime = 1.0;
	}

	m_DegreesPerSecond = ( m_TargetFOV - m_FOV ) / flChangeTime;

	SetThink( &CPointCamera::ChangeFOVThink );
	SetNextThink( gpGlobals->curtime );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointCamera::ChangeFOVThink( void )
{
	SetNextThink( gpGlobals->curtime + CAM_THINK_INTERVAL );

	float newFOV = m_FOV;

	newFOV += m_DegreesPerSecond * CAM_THINK_INTERVAL;

	if( m_DegreesPerSecond < 0 )
	{
		if( newFOV <= m_TargetFOV )
		{
			newFOV = m_TargetFOV;
			SetThink( nullptr );
		}
	}
	else
	{
		if( newFOV >= m_TargetFOV )
		{
			newFOV = m_TargetFOV;
			SetThink( nullptr );
		}
	}

	m_FOV = newFOV;
}

//-----------------------------------------------------------------------------
// Purpose: Turn this camera on, and turn all other cameras off
//-----------------------------------------------------------------------------
void CPointCamera::InputSetOnAndTurnOthersOff( inputdata_t &inputdata )
{
	CBaseEntity *pEntity = nullptr;
	while ((pEntity = gEntList.FindEntityByClassname( pEntity, "point_camera" )) != nullptr)
	{
		CPointCamera *pCamera = (CPointCamera*)pEntity;
		pCamera->InputSetOff( inputdata );
	}

	// Now turn myself on
	InputSetOn( inputdata );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointCamera::InputSetOn( inputdata_t &inputdata )
{
	m_bIsOn = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointCamera::InputSetOff( inputdata_t &inputdata )
{
	m_bIsOn = false;
	SetActive( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointCamera::InputForceActive( inputdata_t &inputdata )
{
	CBaseEntity *pEntity = nullptr;
	while ((pEntity = gEntList.FindEntityByClassname( pEntity, "point_camera" )) != nullptr)
	{
		CPointCamera *pCamera = (CPointCamera*)pEntity;
		pCamera->m_bActive = false;
	}

	// Now turn myself on
	InputSetOn( inputdata );

	m_bActive = true;

	UpdateTransmitState();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointCamera::InputForceInactive( inputdata_t &inputdata )
{
	m_bIsOn = false;
	SetActive( false );
	UpdateTransmitState();
}

IMPLEMENT_REFLECT_DATAMAP( CPointCamera )

IMPLEMENT_REFLECT_SERVERCLASS( CPointCamera, DT_PointCamera )

class CBaseEntity;
class CBasePlayer;

using namespace ::ks::reflect;

// Purpose: A camera entity that's used by the -makedevshots system to take
//			dev screenshots everytime the map is checked into source control.

int g_iDevShotCameraCount = 0;
static constexpr auto DEVSHOT_INITIAL_WAIT = 5; // Time after the level spawn before the first devshot camera takes it's shot
static constexpr auto DEVSHOT_INTERVAL = 5; // Time between each devshot camera taking it's shot

//-----------------------------------------------------------------------------
// Purpose: A camera entity that's used by the -makedevshots system to take
//			dev screenshots everytime the map is checked into source control.
//-----------------------------------------------------------------------------
class CPointDevShotCamera : public CBaseEntity
{
	DECLARE_CLASS( CPointDevShotCamera, CBaseEntity );
public:
	DECLARE_DATADESC();

	void Spawn_Impl( void );
	void DevShotThink_Setup( void );
	void DevShotThink_TakeShot( void );
	void DevShotThink_PostShot( void );

	// Always transmit to clients so they know where to move the view to
	int UpdateTransmitState_Impl();

private:
	[[= ks::reflect::Key{ .name = "cameraname" } ]] string_t	m_iszCameraName;
	[[= ks::reflect::Key{ .name = "FOV" } ]] int			m_iFOV;
};

IMPLEMENT_REFLECT_DATAMAP( CPointDevShotCamera )

LINK_ENTITY_TO_CLASS( point_devshot_camera, CPointDevShotCamera );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointDevShotCamera::Spawn_Impl( void )
{
	BaseClass::Spawn_Impl();

	// Remove this entity immediately if we're not making devshots
	if ( !CommandLine()->FindParm("-makedevshots") )
	{
		UTIL_Remove( this );
		return;
	}

	// Take a screenshot when it's my turn
	SetThink( &CPointDevShotCamera::DevShotThink_Setup );
	SetNextThink( gpGlobals->curtime + DEVSHOT_INITIAL_WAIT + (g_iDevShotCameraCount * DEVSHOT_INTERVAL) );

	g_iDevShotCameraCount++;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CPointDevShotCamera::UpdateTransmitState_Impl()
{
	// always transmit if currently used by a monitor
	return SetTransmitState( FL_EDICT_ALWAYS );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointDevShotCamera::DevShotThink_Setup( void )
{
	// Move the player to the devshot camera
	CBasePlayer *pPlayer = UTIL_GetLocalPlayerOrListenServerHost();
	if ( !pPlayer )
		return;

	// Hide stuff
	engine->ClientCommand( pPlayer->edict(), "developer 0" );
	engine->ClientCommand( pPlayer->edict(), "cl_drawhud 0" );
	engine->ClientCommand( pPlayer->edict(), "sv_cheats 1" );
	engine->ClientCommand( pPlayer->edict(), "god" );
	engine->ClientCommand( pPlayer->edict(), "notarget" );

	pPlayer->AddSolidFlags( FSOLID_NOT_SOLID );
	pPlayer->EnableControl(false);
	pPlayer->SetViewEntity( this );
	pPlayer->SetFOV( this, m_iFOV );

	// Hide the player's viewmodel
	if ( pPlayer->GetActiveWeapon() )
	{
		pPlayer->GetActiveWeapon()->AddEffects( EF_NODRAW );
	}

	DispatchUpdateTransmitState();

	// Now take the shot next frame
	SetThink( &CPointDevShotCamera::DevShotThink_TakeShot );
	SetNextThink( gpGlobals->curtime );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointDevShotCamera::DevShotThink_TakeShot( void )
{
	// Take the screenshot
	CBasePlayer *pPlayer = UTIL_GetLocalPlayerOrListenServerHost();
	if ( !pPlayer )
		return;

	engine->ClientCommand( pPlayer->edict(), "devshots_screenshot \"%s\"", STRING(m_iszCameraName) );

	// Now take the shot next frame
	SetThink( &CPointDevShotCamera::DevShotThink_PostShot );
	SetNextThink( gpGlobals->curtime + (DEVSHOT_INTERVAL - 1) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointDevShotCamera::DevShotThink_PostShot( void )
{
	// Take the screenshot
	CBasePlayer *pPlayer = UTIL_GetLocalPlayerOrListenServerHost();
	if ( !pPlayer )
		return;

	pPlayer->SetFOV( this, 0 );

	// If all cameras have taken their shots, move to the next map
	g_iDevShotCameraCount--;
	if ( !g_iDevShotCameraCount )
	{
		engine->ClientCommand( pPlayer->edict(), "devshots_nextmap" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Game system to detect maps without cameras in them, and move on
//-----------------------------------------------------------------------------
class CDevShotSystem : public CAutoGameSystemPerFrame
{
public:

	CDevShotSystem( char const *name ) : CAutoGameSystemPerFrame( name )
	{
	}

	virtual void LevelInitPreEntity()
	{
		m_bIssuedNextMapCommand = false;
		g_iDevShotCameraCount = 0;
		m_bParsedMapFile = false;
	}

	virtual void SafeRemoveIfDesired( void )
	{
		// If we're not making devshots, remove this system immediately
		if ( !CommandLine()->FindParm("-makedevshots") )
		{
			Remove( this );
			return;
		}
	}

	virtual void FrameUpdatePostEntityThink( void )
	{
		// Wait until we're all spawned in
		if ( gpGlobals->curtime < 5 )
			return;

		if ( m_bIssuedNextMapCommand )
			return;

		if ( !m_bParsedMapFile )
		{
			m_bParsedMapFile = true;

			// See if we've got a camera file to import cameras from
			char szFullName[512];
			V_snprintf(szFullName,sizeof(szFullName), "maps/%s.txt", STRING( gpGlobals->mapname ));
			KeyValues *pkvMapCameras = new KeyValues( "MapCameras" );
			if ( pkvMapCameras->LoadFromFile( filesystem, szFullName, "MOD" ) )
			{
				Warning( "Devshots: Loading point_devshot_camera positions from %s. \n", szFullName );

				// Get each camera, and add it to our list
				KeyValues *pkvCamera = pkvMapCameras->GetFirstSubKey();
				while ( pkvCamera )
				{
					// Get camera name
					const char *pCameraName = pkvCamera->GetName();

					// Make a camera, and move it to the position specified
					CPointDevShotCamera	*pCamera = (CPointDevShotCamera*)CreateEntityByName( "point_devshot_camera" );
					contract_assert( pCamera );
					pCamera->KeyValue( "cameraname", pCameraName );
					pCamera->KeyValue( "origin", pkvCamera->GetString( "origin", "0 0 0" ) );
					pCamera->KeyValue( "angles", pkvCamera->GetString( "angles", "0 0 0" ) );
					pCamera->KeyValue( "FOV", pkvCamera->GetString( "FOV", "75" ) );
					DispatchSpawn( pCamera );
					pCamera->Activate();

					// Move to next camera
					pkvCamera = pkvCamera->GetNextKey();
				}
			}

			if ( !g_iDevShotCameraCount )
			{
				Warning( "Devshots: No point_devshot_camera in %s. Moving to next map.\n", STRING( gpGlobals->mapname ) );

				CBasePlayer *pPlayer = UTIL_GetLocalPlayerOrListenServerHost();
				if ( pPlayer )
				{
					engine->ClientCommand( pPlayer->edict(), "devshots_nextmap" );
					m_bIssuedNextMapCommand = true;
					return;
				}
			}
		}
	}

private:
	bool	m_bIssuedNextMapCommand;
	bool	m_bParsedMapFile;
};

CDevShotSystem	DevShotSystem( "CDevShotSystem" );

class CBaseEntity;
class CBasePlayer;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: An entity that can be used to constrain the player's movement around it

static constexpr auto SF_TELEPORT_TO_SPAWN_POS = 0x00000001;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CPointPlayerMoveConstraint : public CBaseEntity
{
	DECLARE_CLASS( CPointPlayerMoveConstraint, CBaseEntity );
public:
	DECLARE_DATADESC();

	int		UpdateTransmitState_Impl( void );
	void	Activate_Impl( void );
	void	ConstraintThink( void );

	[[= ks::reflect::Input{ .name = "TurnOn", .type = FIELD_VOID } ]] void	InputTurnOn( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "TurnOff", .type = FIELD_VOID } ]] void	InputTurnOff( inputdata_t &inputdata );

private:
	[[= ks::reflect::Key{ .name = "radius" } ]] float				m_flRadius;	
	[[= ks::reflect::Key{ .name = "width" } ]] float				m_flConstraintWidth;
	[[= ks::reflect::Key{ .name = "speedfactor" } ]] float				m_flSpeedFactor;
	float				m_flRadiusSquared;	
	std::vector< EHANDLE >	m_hConstrainedPlayers;
	[[= ks::reflect::Key{ .name = "OnConstraintBroken" } ]] COutputEvent		m_OnConstraintBroken;
};

LINK_ENTITY_TO_CLASS( point_playermoveconstraint, CPointPlayerMoveConstraint );

IMPLEMENT_REFLECT_DATAMAP( CPointPlayerMoveConstraint )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CPointPlayerMoveConstraint::UpdateTransmitState_Impl()
{
	// ALWAYS transmit to all clients.
	return SetTransmitState( FL_EDICT_ALWAYS );
}

//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void CPointPlayerMoveConstraint::Activate_Impl( void )
{
	BaseClass::Activate_Impl();

	m_flRadiusSquared = (m_flRadius * m_flRadius);
}

//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void CPointPlayerMoveConstraint::InputTurnOn( inputdata_t &inputdata )
{
	// Find all players within our radius and constraint them
	float flRadius = m_flRadius;
	// If we're in singleplayer, blow the radius a bunch
	if ( gpGlobals->maxClients == 1 )
	{
		flRadius = MAX_COORD_RANGE;
	}
	CBaseEntity *pEntity = nullptr;
	while ( (pEntity = gEntList.FindEntityByClassnameWithin( pEntity, "player", GetLocalOrigin(), flRadius)) != nullptr )
	{
		CBasePlayer *pPlayer = ToBasePlayer( pEntity );
		contract_assert( pPlayer );
		
		// Only add him if he's not already constrained
		if ( ks::find_index( m_hConstrainedPlayers, pPlayer ) == -1 )
		{
			m_hConstrainedPlayers.push_back( pPlayer );

			pPlayer->ActivateMovementConstraint( this, GetAbsOrigin(), m_flRadius, m_flConstraintWidth, m_flSpeedFactor );
		}
	}

	// Only think if we found any
	if ( !m_hConstrainedPlayers.empty() )
	{
		SetThink( &CPointPlayerMoveConstraint::ConstraintThink );
		SetNextThink( gpGlobals->curtime + 0.1f );
	}
}

//------------------------------------------------------------------------------
// Purpose: Release all players we've constrained
//------------------------------------------------------------------------------
void CPointPlayerMoveConstraint::InputTurnOff( inputdata_t &inputdata )
{
	int iCount = std::ssize( m_hConstrainedPlayers );
	for ( int i = 0; i < iCount; i++ )
	{
		CBasePlayer *pPlayer = ToBasePlayer( m_hConstrainedPlayers[i] );
		if ( pPlayer )
		{
			pPlayer->DeactivateMovementConstraint();
		}
	}

	m_hConstrainedPlayers.clear(); m_hConstrainedPlayers.shrink_to_fit();
}

//-----------------------------------------------------------------------------
// Purpose: Check to see if any of our constrained players have broken the constraint
//-----------------------------------------------------------------------------
void CPointPlayerMoveConstraint::ConstraintThink( void )
{
	int iCount = std::ssize( m_hConstrainedPlayers );

	// Count backwards, because we might drop them if they've broken the constraint
	for ( int i = (iCount-1); i >= 0; i-- )
	{
		CBasePlayer *pPlayer = ToBasePlayer( m_hConstrainedPlayers[i] );
		if ( pPlayer )
		{
			float flDistanceSqr = (pPlayer->GetAbsOrigin() - GetAbsOrigin()).LengthSqr();
			if ( flDistanceSqr > m_flRadiusSquared )
			{
				// Break the constraint to this player
				pPlayer->DeactivateMovementConstraint();
				m_hConstrainedPlayers.erase( m_hConstrainedPlayers.begin() + i );

				// Fire the broken output
				m_OnConstraintBroken.FireOutput( this, pPlayer );
			}
		}
	}

	// Only keep thinking if we any left
	if ( !m_hConstrainedPlayers.empty() )
	{
		SetNextThink( gpGlobals->curtime + 0.1f );
	}
}

class CBaseAnimating;
class CBaseEntity;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Controls the pose parameters of a model

//-----------------------------------------------------------------------------
// SERVER CLASS
//-----------------------------------------------------------------------------

LINK_ENTITY_TO_CLASS( point_posecontroller, CPoseController );	

IMPLEMENT_REFLECT_DATAMAP( CPoseController )

IMPLEMENT_REFLECT_SERVERCLASS( CPoseController, DT_PoseController )

void CPoseController::Spawn_Impl( void )
{
	BaseClass::Spawn_Impl();

	// Talk to the client class when data changes
	AddEFlags( EFL_FORCE_CHECK_TRANSMIT );

	// Think to refresh the list of models
	SetThink( &CPoseController::Think );
	SetNextThink( gpGlobals->curtime + 1.0 );
}

void CPoseController::Think_Impl( void )
{
	if ( !m_bDisablePropLookup )
	{
		// Refresh the list of models
		BuildPropList();

		SetCurrentPose( m_fPoseValue );

		m_bDisablePropLookup = true;

		SetNextThink( gpGlobals->curtime + 1.0 );
	}
}

void CPoseController::BuildPropList( void )
{
	int iPropNum = 0;
	CBaseEntity *pEnt = gEntList.FindEntityByName( nullptr, m_iszPropName );

	while ( pEnt && iPropNum < MAX_POSE_CONTROLLED_PROPS )
	{
		CBaseAnimating *pProp = dynamic_cast<CBaseAnimating*>( pEnt );
		if ( pProp )
		{
			CDynamicProp *pDynamicProp = dynamic_cast<CDynamicProp*>( pProp );
			if ( pDynamicProp )
				pDynamicProp->PropSetSequence( 0 );

			if ( m_hProps[ iPropNum ] != pProp )
			{
				// Only set new handles (to avoid network spam)
				m_hProps.Set( iPropNum, pProp );
			}

			// Update the pose parameter index
			SetPoseIndex( iPropNum, pProp->LookupPoseParameter( m_iszPoseParameterName.ToCStr() ) );
			
			++iPropNum;
		}

		// Get the next entity with specified targetname
		pEnt = gEntList.FindEntityByName( pEnt, m_iszPropName );
	}

	// Nullify the remaining handles
	while ( iPropNum < MAX_POSE_CONTROLLED_PROPS )
	{
		if ( m_hProps[ iPropNum ] != nullptr )
			m_hProps.Set( iPropNum, INVALID_EHANDLE );

		++iPropNum;
	}

	SetNextThink( gpGlobals->curtime + 1.0 );
}

void CPoseController::BuildPoseIndexList( void )
{
	for ( int iPropNum = 0; iPropNum < MAX_POSE_CONTROLLED_PROPS; ++iPropNum )
	{
		CBaseAnimating *pProp = dynamic_cast<CBaseAnimating*>( m_hProps[ iPropNum ].Get() );

		if ( pProp )
		{
			// Update the pose parameter index
			SetPoseIndex( iPropNum, pProp->LookupPoseParameter( m_iszPoseParameterName.ToCStr() ) );
		}
	}
}

void CPoseController::SetPoseIndex( int i, int iValue )
{
	if ( iValue == -1 )
	{
		// Using this as invalid lets us network less bits
		iValue = MAXSTUDIOPOSEPARAM;
	}

	if ( m_chPoseIndex[ i ] != iValue )
	{
		// Only set a new index (to avoid network spam)
		m_chPoseIndex.Set( i, iValue );
	}
}

float CPoseController::GetPoseValue( void )
{
	return m_fPoseValue;
}

void CPoseController::SetProp( CBaseAnimating *pProp )
{
	// Control a prop directly by pointer
	if ( m_hProps[ 0 ] != pProp )
	{
		// Only set new handles (to avoid network spam)
		m_hProps.Set( 0, pProp );
	}

	// Update the pose parameter index
	SetPoseIndex( 0, pProp->LookupPoseParameter( m_iszPoseParameterName.ToCStr() ) );

	// Nullify the remaining handles
	for ( int iPropNum = 1; iPropNum < MAX_POSE_CONTROLLED_PROPS; ++iPropNum )
	{
		if ( m_hProps[ iPropNum ] != nullptr )
			m_hProps.Set( iPropNum, INVALID_EHANDLE );
	}

	m_bDisablePropLookup = false;
}

void CPoseController::SetPropName( const char *pName )
{
	m_iszPropName = MAKE_STRING( pName );

	BuildPropList();
}

void CPoseController::SetPoseParameterName( const char *pName )
{
	m_iszPoseParameterName = MAKE_STRING( pName );

	BuildPoseIndexList();
}

void CPoseController::SetPoseValue( float fValue )
{
	m_fPoseValue = clamp( fValue, 0.0f, 1.0f );

	// Force the client to set the current pose
	m_bPoseValueParity = !m_bPoseValueParity;

	SetCurrentPose( m_fPoseValue );
}

void CPoseController::SetInterpolationTime( float fValue )
{
	m_fInterpolationTime = clamp( fValue, 0.0f, MAX_POSE_INTERPOLATION_TIME );
}

void CPoseController::SetInterpolationWrap( bool bWrap )
{
	m_bInterpolationWrap = bWrap;
}

void CPoseController::SetCycleFrequency( float fValue )
{
	m_fCycleFrequency = clamp( fValue, -MAX_POSE_CYCLE_FREQUENCY, MAX_POSE_CYCLE_FREQUENCY );
}

void CPoseController::SetFModType( int nType )
{
	if ( nType < 0 || nType >= POSECONTROLLER_FMODTYPE_TOTAL )
		return;

	m_nFModType = static_cast<PoseController_FModType_t>(nType);
}

void CPoseController::SetFModTimeOffset( float fValue )
{
	m_fFModTimeOffset = clamp( fValue, -1.0f, 1.0f );
}

void CPoseController::SetFModRate( float fValue )
{
	m_fFModRate = clamp( fValue, -MAX_POSE_FMOD_RATE, MAX_POSE_FMOD_RATE );
}

void CPoseController::SetFModAmplitude( float fValue )
{
	m_fFModAmplitude = clamp( fValue, 0.0f, MAX_POSE_FMOD_AMPLITUDE );
}

void CPoseController::RandomizeFMod( float fExtremeness )
{
	fExtremeness = clamp( fExtremeness, 0.0f, 1.0f );

	SetFModType( RandomInt( 1, POSECONTROLLER_FMODTYPE_TOTAL - 1 ) );
	SetFModTimeOffset( RandomFloat( -1.0, 1.0f ) );
	SetFModRate( RandomFloat( fExtremeness * -MAX_POSE_FMOD_RATE, fExtremeness * MAX_POSE_FMOD_RATE ) );
	SetFModAmplitude( RandomFloat( 0.0f, fExtremeness * MAX_POSE_FMOD_AMPLITUDE ) );
}

void CPoseController::InputSetPoseParameterName( inputdata_t &inputdata )
{
	SetPoseParameterName( inputdata.value.String() );
}

void CPoseController::InputSetPoseValue( inputdata_t &inputdata )
{
	SetPoseValue( inputdata.value.Float() );
}

void CPoseController::InputSetInterpolationTime( inputdata_t &inputdata )
{
	SetInterpolationTime( inputdata.value.Float() );
}

void CPoseController::InputSetCycleFrequency( inputdata_t &inputdata )
{
	SetCycleFrequency( inputdata.value.Float() );
}

void CPoseController::InputSetFModType( inputdata_t &inputdata )
{
	SetFModType( inputdata.value.Int() );
}

void CPoseController::InputSetFModTimeOffset( inputdata_t &inputdata )
{
	SetFModTimeOffset( inputdata.value.Float() );
}

void CPoseController::InputSetFModRate( inputdata_t &inputdata )
{
	SetFModRate( inputdata.value.Float() );
}

void CPoseController::InputSetFModAmplitude( inputdata_t &inputdata )
{
	SetFModAmplitude( inputdata.value.Float() );
}

void CPoseController::InputRandomizeFMod( inputdata_t &inputdata )
{
	RandomizeFMod( inputdata.value.Float() );
}

void CPoseController::InputGetFMod( inputdata_t &inputdata )
{
	DevMsg( "FMod values for pose controller %s\nTYPE: %i\nTIME OFFSET: %f\nRATE: %f\nAMPLITUDE: %f\n", 
			STRING(GetEntityName()),
			m_nFModType.Get(), 
			m_fFModTimeOffset.Get(), 
			m_fFModRate.Get(), 
			m_fFModAmplitude.Get() );
}

void CPoseController::SetCurrentPose( float fCurrentPoseValue )
{
	for ( int iPropNum = 0; iPropNum < MAX_POSE_CONTROLLED_PROPS; ++iPropNum )
	{
		// Control each model's pose parameter
		CBaseAnimating *pProp = dynamic_cast<CBaseAnimating*>( m_hProps[ iPropNum ].Get() );

		if ( pProp )
		{
			float fPoseValueMin;
			float fPoseValueMax;

			// Map to the pose parameter's range
			pProp->GetPoseParameterRange( m_chPoseIndex[ iPropNum ], fPoseValueMin, fPoseValueMax );
			pProp->SetPoseParameter( m_chPoseIndex[ iPropNum ], fPoseValueMin + fCurrentPoseValue * ( fPoseValueMax - fPoseValueMin ) );
		}
	}
}

struct inputdata_t;

using namespace ::ks::reflect;

// Spawnflags
static constexpr auto SF_SPOTLIGHT_START_LIGHT_ON = 0x1;
static constexpr auto SF_SPOTLIGHT_NO_DYNAMIC_LIGHT = 0x2;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CPointSpotlight : public CPointEntity
{
	DECLARE_CLASS( CPointSpotlight, CPointEntity );
public:
	DECLARE_DATADESC();

	CPointSpotlight();

	void	Precache_Impl(void);
	void	Spawn_Impl(void);
	void Activate_Impl();

	void OnEntityEvent_Impl( EntityEvent_t event, void *pEventData );

private:
	public: int 	UpdateTransmitState_Impl();
	private:
	void	SpotlightThink(void);
	void	SpotlightUpdate(void);
	Vector	SpotlightCurrentPos(void);
	void	SpotlightCreate(void);
	void	SpotlightDestroy(void);

	// ------------------------------
	//  Inputs
	// ------------------------------
	[[= ks::reflect::Input{ .name = "LightOn", .type = FIELD_VOID } ]] void InputLightOn( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "LightOff", .type = FIELD_VOID } ]] void InputLightOff( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetColor", .type = FIELD_COLOR32 } ]] void InputSetColor( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "ForceUpdate", .type = FIELD_VOID } ]] void InputForceUpdate( inputdata_t &inputdata );

	// Creates the efficient spotlight 
	void CreateEfficientSpotlight();

	// Computes render info for a spotlight
	void ComputeRenderInfo();

private:
	bool	m_bSpotlightOn;
	bool	m_bEfficientSpotlight;
	Vector	m_vSpotlightTargetPos;
	Vector	m_vSpotlightCurrentPos;
	Vector	m_vSpotlightDir;
	int		m_nHaloSprite;
	CHandle<CBeam>			m_hSpotlight;
	CHandle<CSpotlightEnd>	m_hSpotlightTarget;
	
	[[= ks::reflect::Key{ .name = "SpotlightLength" } ]] float	m_flSpotlightMaxLength;
	float	m_flSpotlightCurLength;
	[[= ks::reflect::Key{ .name = "SpotlightWidth" } ]] float	m_flSpotlightGoalWidth;
	[[= ks::reflect::Key{ .name = "HDRColorScale" } ]] float	m_flHDRColorScale;

public:
	[[= ks::reflect::Key{ .name = "OnLightOn" } ]] COutputEvent m_OnOn;     ///< output fires when turned on
	[[= ks::reflect::Key{ .name = "OnLightOff" } ]] COutputEvent m_OnOff;    ///< output fires when turned off
};

IMPLEMENT_REFLECT_DATAMAP( CPointSpotlight )

LINK_ENTITY_TO_CLASS(point_spotlight, CPointSpotlight);

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPointSpotlight::CPointSpotlight()
{
	m_flHDRColorScale = 1.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointSpotlight::Precache_Impl(void)
{
	BaseClass::Precache_Impl();

	// Sprites.
	m_nHaloSprite = PrecacheModel("sprites/light_glow03.vmt");
	PrecacheModel( "sprites/glow_test02.vmt" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointSpotlight::Spawn_Impl(void)
{
	Precache();

	UTIL_SetSize( this,vec3_origin,vec3_origin );
	AddSolidFlags( FSOLID_NOT_SOLID );
	SetMoveType( MOVETYPE_NONE );
	m_bEfficientSpotlight = true;

	// Check for user error
	if (m_flSpotlightMaxLength <= 0)
	{
		DevMsg("%s (%s) has an invalid spotlight length <= 0, setting to 500\n", GetClassname(), GetDebugName() );
		m_flSpotlightMaxLength = 500;
	}
	if (m_flSpotlightGoalWidth <= 0)
	{
		DevMsg("%s (%s) has an invalid spotlight width <= 0, setting to 10\n", GetClassname(), GetDebugName() );
		m_flSpotlightGoalWidth = 10;
	}
	
	if (m_flSpotlightGoalWidth > MAX_BEAM_WIDTH )
	{
		DevMsg("%s (%s) has an invalid spotlight width %.1f (max %.1f).\n", GetClassname(), GetDebugName(), m_flSpotlightGoalWidth, MAX_BEAM_WIDTH );
		m_flSpotlightGoalWidth = MAX_BEAM_WIDTH; 
	}

	// ------------------------------------
	//	Init all class vars 
	// ------------------------------------
	m_vSpotlightTargetPos	= vec3_origin;
	m_vSpotlightCurrentPos	= vec3_origin;
	m_hSpotlight			= nullptr;
	m_hSpotlightTarget		= nullptr;
	m_vSpotlightDir			= vec3_origin;
	m_flSpotlightCurLength	= m_flSpotlightMaxLength;

	m_bSpotlightOn = HasSpawnFlags( SF_SPOTLIGHT_START_LIGHT_ON );

	SetThink( &CPointSpotlight::SpotlightThink );
	SetNextThink( gpGlobals->curtime + 0.1f );
}

//-----------------------------------------------------------------------------
// Computes render info for a spotlight
//-----------------------------------------------------------------------------
void CPointSpotlight::ComputeRenderInfo()
{
	// Fade out spotlight end if past max length.  
	if ( m_flSpotlightCurLength > 2*m_flSpotlightMaxLength )
	{
		m_hSpotlightTarget->SetRenderAlpha( 0 );
		m_hSpotlight->SetFadeLength( m_flSpotlightMaxLength );
	}
	else if ( m_flSpotlightCurLength > m_flSpotlightMaxLength )		
	{
		m_hSpotlightTarget->SetRenderAlpha( (1-((m_flSpotlightCurLength-m_flSpotlightMaxLength)/m_flSpotlightMaxLength)) );
		m_hSpotlight->SetFadeLength( m_flSpotlightMaxLength );
	}
	else
	{
		m_hSpotlightTarget->SetRenderAlpha( 1.0 );
		m_hSpotlight->SetFadeLength( m_flSpotlightCurLength );
	}

	// Adjust end width to keep beam width constant
	float flNewWidth = m_flSpotlightGoalWidth * (m_flSpotlightCurLength / m_flSpotlightMaxLength);
	flNewWidth = clamp(flNewWidth, 0, MAX_BEAM_WIDTH );
	m_hSpotlight->SetEndWidth(flNewWidth);

	// Adjust width of light on the end.  
	if ( FBitSet (m_spawnflags, SF_SPOTLIGHT_NO_DYNAMIC_LIGHT) )
	{
		m_hSpotlightTarget->m_flLightScale = 0.0;
	}
	else
	{
		// <<TODO>> - magic number 1.8 depends on sprite size
		m_hSpotlightTarget->m_flLightScale = 1.8*flNewWidth;
	}
}

//-----------------------------------------------------------------------------
// Creates the efficient spotlight 
//-----------------------------------------------------------------------------
void CPointSpotlight::CreateEfficientSpotlight()
{
	if ( m_hSpotlightTarget.Get() != nullptr )
		return;

	SpotlightCreate();
	m_vSpotlightCurrentPos = SpotlightCurrentPos();
	m_hSpotlightTarget->SetAbsOrigin( m_vSpotlightCurrentPos );
	m_hSpotlightTarget->m_vSpotlightOrg = GetAbsOrigin();
	VectorSubtract( m_hSpotlightTarget->GetAbsOrigin(), m_hSpotlightTarget->m_vSpotlightOrg, m_hSpotlightTarget->m_vSpotlightDir );
	m_flSpotlightCurLength = VectorNormalize( m_hSpotlightTarget->m_vSpotlightDir );
	m_hSpotlightTarget->SetMoveType( MOVETYPE_NONE );
	ComputeRenderInfo();

	m_OnOn.FireOutput( this, this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointSpotlight::Activate_Impl(void)
{
	BaseClass::Activate_Impl();

	if ( GetMoveParent() )
	{
		m_bEfficientSpotlight = false;
	}

	if ( m_bEfficientSpotlight )
	{
		if ( m_bSpotlightOn )
		{
			CreateEfficientSpotlight();
		}

		// Don't think
		SetThink( nullptr );
	}
}

//-------------------------------------------------------------------------------------
// Optimization to deal with spotlights
//-------------------------------------------------------------------------------------
void CPointSpotlight::OnEntityEvent_Impl( EntityEvent_t event, void *pEventData )
{
	if ( event == ENTITY_EVENT_PARENT_CHANGED )
	{
		if ( GetMoveParent() )
		{
			m_bEfficientSpotlight = false;
			if ( m_hSpotlightTarget )
			{
				m_hSpotlightTarget->SetMoveType( MOVETYPE_FLY );
			}
			SetThink( &CPointSpotlight::SpotlightThink );
			SetNextThink( gpGlobals->curtime + 0.1f );
		}
	}

	BaseClass::OnEntityEvent_Impl( event, pEventData );
}

//-------------------------------------------------------------------------------------
// Purpose : Send even though we don't have a model so spotlight gets proper position
// Input   :
// Output  :
//-------------------------------------------------------------------------------------
int CPointSpotlight::UpdateTransmitState_Impl()
{
	if ( m_bEfficientSpotlight )
		return SetTransmitState( FL_EDICT_DONTSEND );

	return SetTransmitState( FL_EDICT_PVSCHECK );
}

//-----------------------------------------------------------------------------
// Purpose: Plays the engine sound.
//-----------------------------------------------------------------------------
void CPointSpotlight::SpotlightThink( void )
{
	if ( GetMoveParent() )
	{
		SetNextThink( gpGlobals->curtime + TICK_INTERVAL );
	}
	else
	{
		SetNextThink( gpGlobals->curtime + 0.1f );
	}

	SpotlightUpdate();
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CPointSpotlight::SpotlightCreate(void)
{
	if ( m_hSpotlightTarget.Get() != nullptr )
		return;

	AngleVectors( GetAbsAngles(), &m_vSpotlightDir );

	trace_t tr;
	UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() + m_vSpotlightDir * m_flSpotlightMaxLength, MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr);

	m_hSpotlightTarget = (CSpotlightEnd*)CreateEntityByName( "spotlight_end" );
	m_hSpotlightTarget->Spawn();
	m_hSpotlightTarget->SetAbsOrigin( tr.endpos );
	m_hSpotlightTarget->SetOwnerEntity( this );
	m_hSpotlightTarget->m_clrRender = m_clrRender;
	m_hSpotlightTarget->m_Radius = m_flSpotlightMaxLength;

	if ( FBitSet (m_spawnflags, SF_SPOTLIGHT_NO_DYNAMIC_LIGHT) )
	{
		m_hSpotlightTarget->m_flLightScale = 0.0;
	}

	//m_hSpotlight = CBeam::BeamCreate( "sprites/spotlight.vmt", m_flSpotlightGoalWidth );
	m_hSpotlight = CBeam::BeamCreate( "sprites/glow_test02.vmt", m_flSpotlightGoalWidth );
	// Set the temporary spawnflag on the beam so it doesn't save (we'll recreate it on restore)
	m_hSpotlight->SetHDRColorScale( m_flHDRColorScale );
	m_hSpotlight->AddSpawnFlags( SF_BEAM_TEMPORARY );
	m_hSpotlight->SetColor( m_clrRender->r, m_clrRender->g, m_clrRender->b ); 
	m_hSpotlight->SetHaloTexture(m_nHaloSprite);
	m_hSpotlight->SetHaloScale(60);
	m_hSpotlight->SetEndWidth(m_flSpotlightGoalWidth);
	m_hSpotlight->SetBeamFlags( (FBEAM_SHADEOUT|FBEAM_NOTILE) );
	m_hSpotlight->SetBrightness( m_clrRender->a );
	m_hSpotlight->SetNoise( 0 );

	if ( m_bEfficientSpotlight )
	{
		m_hSpotlight->PointsInit( GetAbsOrigin(), m_hSpotlightTarget->GetAbsOrigin() );
	}
	else
	{
		m_hSpotlight->EntsInit( this, m_hSpotlightTarget );
	}
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
Vector CPointSpotlight::SpotlightCurrentPos(void)
{
	AngleVectors( GetAbsAngles(), &m_vSpotlightDir );

	//	Get beam end point.  Only collide with solid objects, not npcs
	trace_t tr;
	UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() + (m_vSpotlightDir * 2 * m_flSpotlightMaxLength), MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr );
	return tr.endpos;
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CPointSpotlight::SpotlightDestroy(void)
{
	if ( m_hSpotlight )
	{
		m_OnOff.FireOutput( this, this );

		UTIL_Remove(m_hSpotlight);
		UTIL_Remove(m_hSpotlightTarget);
	}
}

//------------------------------------------------------------------------------
// Purpose : Update the direction and position of my spotlight
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CPointSpotlight::SpotlightUpdate(void)
{
	// ---------------------------------------------------
	//  If I don't have a spotlight attempt to create one
	// ---------------------------------------------------
	if ( !m_hSpotlight )
	{
		if ( m_bSpotlightOn )
		{
			// Make the spotlight
			SpotlightCreate();
		}
		else
		{
			return;
		}
	}
	else if ( !m_bSpotlightOn )
	{
		SpotlightDestroy();
		return;
	}
	
	if ( !m_hSpotlightTarget )
	{
		DevWarning( "**Attempting to update point_spotlight but target ent is NULL\n" );
		SpotlightDestroy();
		SpotlightCreate();
		if ( !m_hSpotlightTarget )
			return;
	}

	m_vSpotlightCurrentPos = SpotlightCurrentPos();

	//  Update spotlight target velocity
	Vector vTargetDir;
	VectorSubtract( m_vSpotlightCurrentPos, m_hSpotlightTarget->GetAbsOrigin(), vTargetDir );
	float vTargetDist = vTargetDir.Length();

	// If we haven't moved at all, don't recompute
	if ( vTargetDist < 1 )
	{
		m_hSpotlightTarget->SetAbsVelocity( vec3_origin );
		return;
	}

	Vector vecNewVelocity = vTargetDir;
	VectorNormalize(vecNewVelocity);
	vecNewVelocity *= (10 * vTargetDist);

	// If a large move is requested, just jump to final spot as we probably hit a discontinuity
	if (vecNewVelocity.Length() > 200)
	{
		VectorNormalize(vecNewVelocity);
		vecNewVelocity *= 200;
		VectorNormalize(vTargetDir);
		m_hSpotlightTarget->SetAbsOrigin( m_vSpotlightCurrentPos );
	}
	m_hSpotlightTarget->SetAbsVelocity( vecNewVelocity );
	m_hSpotlightTarget->m_vSpotlightOrg = GetAbsOrigin();

	// Avoid sudden change in where beam fades out when cross disconinuities
	VectorSubtract( m_hSpotlightTarget->GetAbsOrigin(), m_hSpotlightTarget->m_vSpotlightOrg, m_hSpotlightTarget->m_vSpotlightDir );
	float flBeamLength	= VectorNormalize( m_hSpotlightTarget->m_vSpotlightDir );
	m_flSpotlightCurLength = (0.60*m_flSpotlightCurLength) + (0.4*flBeamLength);

	ComputeRenderInfo();

	//NDebugOverlay::Cross3D(GetAbsOrigin(),Vector(-5,-5,-5),Vector(5,5,5),0,255,0,true,0.1);
	//NDebugOverlay::Cross3D(m_vSpotlightCurrentPos,Vector(-5,-5,-5),Vector(5,5,5),0,255,0,true,0.1);
	//NDebugOverlay::Cross3D(m_vSpotlightTargetPos,Vector(-5,-5,-5),Vector(5,5,5),255,0,0,true,0.1);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointSpotlight::InputLightOn( inputdata_t &inputdata )
{
	if ( !m_bSpotlightOn )
	{
		m_bSpotlightOn = true;
		if ( m_bEfficientSpotlight )
		{
			CreateEfficientSpotlight();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointSpotlight::InputLightOff( inputdata_t &inputdata )
{
	if ( m_bSpotlightOn )
	{
		m_bSpotlightOn = false;
		if ( m_bEfficientSpotlight )
		{
			SpotlightDestroy();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set the beam's color
//-----------------------------------------------------------------------------
void CPointSpotlight::InputSetColor( inputdata_t &inputdata )
{
	if ( m_hSpotlight )
	{
		color32 clr = inputdata.value.Color32();
		m_hSpotlight->SetColor( clr.r, clr.g, clr.b );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Force update the spotlight
//-----------------------------------------------------------------------------
void CPointSpotlight::InputForceUpdate( inputdata_t &inputdata )
{
	SpotlightUpdate();
}

class CBaseEntity;
class CPointTemplate;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Point entity used to create templates out of other entities or groups of entities

//lwss - hardcode .nut file here instead of generating it. I don't think it will change as of right now.
//Generated with: `perl texttoarray.pl ../../game/server/spawn_helper.nut g_Script_spawn_helper > /tmp/file.h`
//#include "spawn_helper_nut.h"
static const unsigned char g_Script_spawn_helper[] = {
        0x2f,0x2f,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,
        0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,
        0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x0a,
        0x2f,0x2f,0x20,0x41,0x6e,0x20,0x73,0x70,0x61,0x77,0x6e,0x65,0x72,0x20,0x6f,0x6e,0x20,0x74,0x68,0x65,
        0x20,0x73,0x65,0x72,0x76,0x65,0x72,0x20,0x69,0x73,0x20,0x67,0x65,0x74,0x74,0x69,0x6e,0x67,0x20,0x72,
        0x65,0x61,0x64,0x79,0x20,0x74,0x6f,0x0a,0x2f,0x2f,0x20,0x70,0x72,0x65,0x73,0x70,0x61,0x77,0x6e,0x20,
        0x61,0x6e,0x20,0x65,0x6e,0x74,0x69,0x74,0x79,0x2e,0x20,0x49,0x74,0x20,0x63,0x61,0x6c,0x6c,0x73,0x20,
        0x74,0x68,0x69,0x73,0x20,0x66,0x75,0x6e,0x63,0x74,0x69,0x6f,0x6e,0x2c,0x20,0x73,0x65,0x6e,0x64,0x69,
        0x6e,0x67,0x20,0x75,0x73,0x0a,0x2f,0x2f,0x20,0x74,0x68,0x65,0x20,0x65,0x6e,0x74,0x69,0x74,0x79,0x20,
        0x74,0x68,0x61,0x74,0x20,0x69,0x74,0x27,0x73,0x20,0x70,0x72,0x65,0x70,0x61,0x72,0x69,0x6e,0x67,0x20,
        0x74,0x6f,0x20,0x73,0x70,0x61,0x77,0x6e,0x2e,0x20,0x0a,0x2f,0x2f,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,
        0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,
        0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,
        0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x0a,0x0a,0x66,0x75,0x6e,0x63,0x74,0x69,0x6f,0x6e,
        0x20,0x5f,0x5f,0x45,0x78,0x65,0x63,0x75,0x74,0x65,0x50,0x72,0x65,0x53,0x70,0x61,0x77,0x6e,0x28,0x20,
        0x65,0x6e,0x74,0x69,0x74,0x79,0x20,0x29,0x20,0x0a,0x7b,0x0a,0x09,0x5f,0x5f,0x45,0x6e,0x74,0x69,0x74,
        0x79,0x4d,0x61,0x6b,0x65,0x72,0x52,0x65,0x73,0x75,0x6c,0x74,0x20,0x3c,0x2d,0x20,0x7b,0x7d,0x0a,0x09,
        0x69,0x66,0x20,0x28,0x20,0x22,0x50,0x72,0x65,0x53,0x70,0x61,0x77,0x6e,0x49,0x6e,0x73,0x74,0x61,0x6e,
        0x63,0x65,0x22,0x20,0x69,0x6e,0x20,0x74,0x68,0x69,0x73,0x20,0x29,0x0a,0x09,0x7b,0x0a,0x09,0x09,0x6c,
        0x6f,0x63,0x61,0x6c,0x20,0x6f,0x76,0x65,0x72,0x72,0x69,0x64,0x65,0x73,0x20,0x3d,0x20,0x50,0x72,0x65,
        0x53,0x70,0x61,0x77,0x6e,0x49,0x6e,0x73,0x74,0x61,0x6e,0x63,0x65,0x28,0x20,0x65,0x6e,0x74,0x69,0x74,
        0x79,0x2e,0x47,0x65,0x74,0x43,0x6c,0x61,0x73,0x73,0x6e,0x61,0x6d,0x65,0x28,0x29,0x2c,0x20,0x65,0x6e,
        0x74,0x69,0x74,0x79,0x2e,0x47,0x65,0x74,0x4e,0x61,0x6d,0x65,0x28,0x29,0x20,0x29,0x3b,0x0a,0x09,0x09,
        0x6c,0x6f,0x63,0x61,0x6c,0x20,0x74,0x79,0x70,0x65,0x20,0x3d,0x20,0x74,0x79,0x70,0x65,0x6f,0x66,0x28,
        0x20,0x6f,0x76,0x65,0x72,0x72,0x69,0x64,0x65,0x73,0x20,0x29,0x3b,0x0a,0x09,0x09,0x69,0x66,0x20,0x28,
        0x20,0x74,0x79,0x70,0x65,0x20,0x3d,0x3d,0x20,0x22,0x74,0x61,0x62,0x6c,0x65,0x22,0x20,0x29,0x0a,0x09,
        0x09,0x7b,0x0a,0x09,0x09,0x09,0x66,0x6f,0x72,0x65,0x61,0x63,0x68,0x28,0x20,0x6b,0x65,0x79,0x2c,0x20,
        0x76,0x61,0x6c,0x75,0x65,0x20,0x69,0x6e,0x20,0x6f,0x76,0x65,0x72,0x72,0x69,0x64,0x65,0x73,0x20,0x29,
        0x0a,0x09,0x09,0x09,0x7b,0x0a,0x09,0x09,0x09,0x09,0x73,0x77,0x69,0x74,0x63,0x68,0x20,0x28,0x20,0x74,
        0x79,0x70,0x65,0x6f,0x66,0x28,0x20,0x76,0x61,0x6c,0x75,0x65,0x20,0x29,0x20,0x29,0x0a,0x09,0x09,0x09,
        0x09,0x7b,0x0a,0x09,0x09,0x09,0x09,0x63,0x61,0x73,0x65,0x20,0x22,0x73,0x74,0x72,0x69,0x6e,0x67,0x22,
        0x3a,0x0a,0x09,0x09,0x09,0x09,0x09,0x7b,0x0a,0x09,0x09,0x09,0x09,0x09,0x09,0x65,0x6e,0x74,0x69,0x74,
        0x79,0x2e,0x5f,0x5f,0x4b,0x65,0x79,0x56,0x61,0x6c,0x75,0x65,0x46,0x72,0x6f,0x6d,0x53,0x74,0x72,0x69,
        0x6e,0x67,0x28,0x20,0x6b,0x65,0x79,0x2c,0x20,0x76,0x61,0x6c,0x75,0x65,0x20,0x29,0x3b,0x0a,0x09,0x09,
        0x09,0x09,0x09,0x09,0x62,0x72,0x65,0x61,0x6b,0x3b,0x0a,0x09,0x09,0x09,0x09,0x09,0x7d,0x0a,0x09,0x09,
        0x09,0x09,0x63,0x61,0x73,0x65,0x20,0x22,0x69,0x6e,0x74,0x65,0x67,0x65,0x72,0x22,0x3a,0x0a,0x09,0x09,
        0x09,0x09,0x09,0x7b,0x0a,0x09,0x09,0x09,0x09,0x09,0x09,0x65,0x6e,0x74,0x69,0x74,0x79,0x2e,0x5f,0x5f,
        0x4b,0x65,0x79,0x56,0x61,0x6c,0x75,0x65,0x46,0x72,0x6f,0x6d,0x49,0x6e,0x74,0x28,0x20,0x6b,0x65,0x79,
        0x2c,0x20,0x76,0x61,0x6c,0x75,0x65,0x20,0x29,0x3b,0x0a,0x09,0x09,0x09,0x09,0x09,0x09,0x62,0x72,0x65,
        0x61,0x6b,0x3b,0x0a,0x09,0x09,0x09,0x09,0x09,0x7d,0x0a,0x09,0x09,0x09,0x09,0x63,0x61,0x73,0x65,0x20,
        0x22,0x66,0x6c,0x6f,0x61,0x74,0x22,0x3a,0x0a,0x09,0x09,0x09,0x09,0x63,0x61,0x73,0x65,0x20,0x22,0x62,
        0x6f,0x6f,0x6c,0x22,0x3a,0x0a,0x09,0x09,0x09,0x09,0x09,0x7b,0x0a,0x09,0x09,0x09,0x09,0x09,0x09,0x65,
        0x6e,0x74,0x69,0x74,0x79,0x2e,0x5f,0x5f,0x4b,0x65,0x79,0x56,0x61,0x6c,0x75,0x65,0x46,0x72,0x6f,0x6d,
        0x46,0x6c,0x6f,0x61,0x74,0x28,0x20,0x6b,0x65,0x79,0x2c,0x20,0x76,0x61,0x6c,0x75,0x65,0x2e,0x74,0x6f,
        0x66,0x6c,0x6f,0x61,0x74,0x28,0x29,0x20,0x29,0x3b,0x0a,0x09,0x09,0x09,0x09,0x09,0x09,0x62,0x72,0x65,
        0x61,0x6b,0x3b,0x0a,0x09,0x09,0x09,0x09,0x09,0x7d,0x0a,0x09,0x09,0x09,0x09,0x09,0x0a,0x09,0x09,0x09,
        0x09,0x63,0x61,0x73,0x65,0x20,0x22,0x56,0x65,0x63,0x74,0x6f,0x72,0x22,0x3a,0x0a,0x09,0x09,0x09,0x09,
        0x09,0x7b,0x0a,0x09,0x09,0x09,0x09,0x09,0x09,0x65,0x6e,0x74,0x69,0x74,0x79,0x2e,0x5f,0x5f,0x4b,0x65,
        0x79,0x56,0x61,0x6c,0x75,0x65,0x46,0x72,0x6f,0x6d,0x56,0x65,0x63,0x74,0x6f,0x72,0x28,0x20,0x6b,0x65,
        0x79,0x2c,0x20,0x76,0x61,0x6c,0x75,0x65,0x20,0x29,0x3b,0x0a,0x09,0x09,0x09,0x09,0x09,0x09,0x62,0x72,
        0x65,0x61,0x6b,0x0a,0x09,0x09,0x09,0x09,0x09,0x7d,0x0a,0x09,0x09,0x09,0x09,0x09,0x0a,0x09,0x09,0x09,
        0x09,0x64,0x65,0x66,0x61,0x75,0x6c,0x74,0x3a,0x0a,0x09,0x09,0x09,0x09,0x09,0x7b,0x0a,0x09,0x09,0x09,
        0x09,0x09,0x09,0x70,0x72,0x69,0x6e,0x74,0x6c,0x28,0x20,0x22,0x43,0x61,0x6e,0x6e,0x6f,0x74,0x20,0x75,
        0x73,0x65,0x20,0x22,0x20,0x2b,0x20,0x74,0x79,0x70,0x65,0x6f,0x66,0x28,0x20,0x76,0x61,0x6c,0x75,0x65,
        0x20,0x29,0x20,0x2b,0x20,0x22,0x20,0x61,0x73,0x20,0x61,0x20,0x6b,0x65,0x79,0x22,0x20,0x29,0x3b,0x0a,
        0x09,0x09,0x09,0x09,0x09,0x7d,0x0a,0x09,0x09,0x09,0x09,0x7d,0x0a,0x09,0x09,0x09,0x7d,0x0a,0x09,0x09,
        0x7d,0x0a,0x09,0x09,0x0a,0x09,0x09,0x69,0x66,0x20,0x28,0x20,0x74,0x79,0x70,0x65,0x20,0x3d,0x3d,0x20,
        0x22,0x62,0x6f,0x6f,0x6c,0x22,0x20,0x29,0x0a,0x09,0x09,0x7b,0x0a,0x09,0x09,0x09,0x72,0x65,0x74,0x75,
        0x72,0x6e,0x20,0x6f,0x76,0x65,0x72,0x72,0x69,0x64,0x65,0x73,0x3b,0x0a,0x09,0x09,0x7d,0x0a,0x09,0x7d,
        0x0a,0x7d,0x3b,0x0a,0x0a,0x66,0x75,0x6e,0x63,0x74,0x69,0x6f,0x6e,0x20,0x5f,0x5f,0x46,0x69,0x6e,0x69,
        0x73,0x68,0x53,0x70,0x61,0x77,0x6e,0x28,0x29,0x0a,0x7b,0x0a,0x09,0x5f,0x5f,0x45,0x6e,0x74,0x69,0x74,
        0x79,0x4d,0x61,0x6b,0x65,0x72,0x52,0x65,0x73,0x75,0x6c,0x74,0x20,0x3c,0x2d,0x20,0x6e,0x75,0x6c,0x6c,
        0x3b,0x0a,0x7d,0x0a,0x00
};
/**
//=========================================================
// An spawner on the server is getting ready to
// prespawn an entity. It calls this function, sending us
// the entity that it's preparing to spawn.
//=========================================================

function __ExecutePreSpawn( entity )
{
	__EntityMakerResult <- {}
	if ( "PreSpawnInstance" in this )
	{
		local overrides = PreSpawnInstance( entity.GetClassname(), entity.GetName() );
		local type = typeof( overrides );
		if ( type == "table" )
		{
			foreach( key, value in overrides )
			{
				switch ( typeof( value ) )
				{
				case "string":
					{
						entity.__KeyValueFromString( key, value );
						break;
					}
				case "integer":
					{
						entity.__KeyValueFromInt( key, value );
						break;
					}
				case "float":
				case "bool":
					{
						entity.__KeyValueFromFloat( key, value.tofloat() );
						break;
					}

				case "Vector":
					{
						entity.__KeyValueFromVector( key, value );
						break
					}

				default:
					{
						printl( "Cannot use " + typeof( value ) + " as a key" );
					}
				}
			}
		}

		if ( type == "bool" )
		{
			return overrides;
		}
	}
};

function __FinishSpawn()
{
	__EntityMakerResult <- null;
}
*/
//lwss end

static constexpr auto SF_POINTTEMPLATE_DONTREMOVETEMPLATEENTITIES = 0x0001;

// Level designers can suppress the uniquification of the spawned entity
// names with a spawnflag, provided they guarantee that only one instance
// of the entities will ever be spawned at a time.
static constexpr auto SF_POINTTEMPLATE_PRESERVE_NAMES = 0x0002;

LINK_ENTITY_TO_CLASS(point_template, CPointTemplate);

IMPLEMENT_REFLECT_DATAMAP_SIMPLE( template_t )

IMPLEMENT_REFLECT_DATAMAP( CPointTemplate )

//-----------------------------------------------------------------------------
// Purpose: A simple system to help precache point_template entities ... ywb
//-----------------------------------------------------------------------------
class CPointTemplatePrecacher : public CAutoGameSystem
{
public:

	CPointTemplatePrecacher( char const *name ) : CAutoGameSystem( name )
	{
	}

	void AddToPrecache( CPointTemplate *ent )
	{
		m_Ents.push_back( ent );
	}

	virtual void LevelInitPreEntity()
	{
		m_Ents.clear();
	}

	virtual void Shutdown()
	{
		m_Ents.clear();
	}

	void Precache()
	{
		int c = std::ssize( m_Ents );
		for ( int i = 0 ; i < c; ++i )
		{
			CPointTemplate *ent = m_Ents[ i ].Get();
			if ( ent )
			{
				ent->PerformPrecache();
			}
		}

		m_Ents.clear();
	}
private:

	std::vector< CHandle< CPointTemplate > >	m_Ents;
};

CPointTemplatePrecacher g_PointTemplatePrecacher( "CPointTemplatePrecacher" );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PrecachePointTemplates()
{
	g_PointTemplatePrecacher.Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointTemplate::Spawn_Impl( void )
{
	Precache();
	ScriptInstallPreSpawnHook();
}

void CPointTemplate::Precache_Impl()
{
	// We can't call precache right when we instance the template, we need to defer it until after all map entities have 
	//  been loaded, so add this template to a list which is cleared after map entity parsing is completed.
	g_PointTemplatePrecacher.AddToPrecache( this );
}

//-----------------------------------------------------------------------------
// Purpose: Level designers can suppress the uniquification of the spawned entity
//			names with a spawnflag, provided they guarantee that only one instance
//			of the entities will ever be spawned at a time.
//-----------------------------------------------------------------------------
bool CPointTemplate::AllowNameFixup()
{
	return !HasSpawnFlags( SF_POINTTEMPLATE_PRESERVE_NAMES );
}

//-----------------------------------------------------------------------------
// Purpose: Called at the start of template initialization for this point_template.
//			Find all the entities referenced by this point_template, which will 
//			then be turned into templates by the map-parsing code.
//-----------------------------------------------------------------------------
void CPointTemplate::StartBuildingTemplates( void )
{
	// Build our list of template entities
	for ( int i = 0; i < MAX_NUM_TEMPLATES; i++ )
	{
		if ( m_iszTemplateEntityNames[i] != NULL_STRING )
		{
			CBaseEntity	*pEntity = nullptr;
			int iOldNum = std::ssize( m_hTemplateEntities );
			// Add all the entities with the matching targetname
			while ( (pEntity = gEntList.FindEntityByName( pEntity, STRING(m_iszTemplateEntityNames[i]) )) != nullptr )
			{
				m_hTemplateEntities.push_back( pEntity );
			}

			// Useful mapmaker warning
			if ( iOldNum == std::ssize( m_hTemplateEntities ) )
			{
				Warning( "Couldn't find any entities named %s, which point_template %s is specifying.\n", STRING(m_iszTemplateEntityNames[i]), STRING(GetEntityName()) );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called at the end of template initialization for this point_template.
//			All of our referenced entities have now been destroyed.
//-----------------------------------------------------------------------------
void CPointTemplate::FinishBuildingTemplates( void )
{
	// Our template entities are now gone, deleted by the server post turning them into templates.
	m_hTemplateEntities.clear(); m_hTemplateEntities.shrink_to_fit();

	// Now tell the template system to hook up all the Entity I/O connections within our set of templates.
	Templates_ReconnectIOForGroup( this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointTemplate::AddTemplate( CBaseEntity *pEntity, const char *pszMapData, int nLen )
{
	// Add it to the template list
	int iIndex = Templates_Add( pEntity, pszMapData, nLen );
	if ( iIndex == -1 )
	{
		Warning( "point_template %s failed to add template.\n", STRING(GetEntityName()) );
		return;
	}

	template_t newTemplate;
	newTemplate.iTemplateIndex = iIndex;

	// Store the entity's origin & angles in a matrix in the template's local space
	VMatrix matTemplateToWorld, matWorldToTemplate, matEntityToWorld, matEntityToTemplate;
	matTemplateToWorld.SetupMatrixOrgAngles( GetAbsOrigin(), GetAbsAngles() );
	matTemplateToWorld.InverseTR( matWorldToTemplate );
	matEntityToWorld.SetupMatrixOrgAngles( pEntity->GetAbsOrigin(), pEntity->GetAbsAngles() );
	MatrixMultiply( matWorldToTemplate, matEntityToWorld, matEntityToTemplate );

	newTemplate.matEntityToTemplate = matEntityToTemplate;
	m_hTemplates.push_back( newTemplate );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CPointTemplate::ShouldRemoveTemplateEntities( void )
{
	return ( !(m_spawnflags & SF_POINTTEMPLATE_DONTREMOVETEMPLATEENTITIES) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CPointTemplate::GetNumTemplates( void )
{
	return std::ssize( m_hTemplates );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CPointTemplate::GetTemplateIndexForTemplate( int iTemplate )
{
	contract_assert( iTemplate < std::ssize( m_hTemplates ) );
	return m_hTemplates[iTemplate].iTemplateIndex;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CPointTemplate::GetNumTemplateEntities( void )
{
	return std::ssize( m_hTemplateEntities );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseEntity	*CPointTemplate::GetTemplateEntity( int iTemplateNumber )
{
	contract_assert( iTemplateNumber < std::ssize( m_hTemplateEntities ) );

	return m_hTemplateEntities[iTemplateNumber];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointTemplate::PerformPrecache()
{
	// Go through all our templated map data and precache all the entities in it
	int iTemplates = std::ssize( m_hTemplates );
	if ( !iTemplates )
	{
		Msg("Precache called on a point_template that has no templates: %s\n", STRING(GetEntityName()) );
		return;
	}

	// Tell the template system we're about to start a new template
	Templates_StartUniqueInstance();

	//HierarchicalSpawn_t *pSpawnList = (HierarchicalSpawn_t*)stackalloc( iTemplates * sizeof(HierarchicalSpawn_t) );

	int i;
	for ( i = 0; i < iTemplates; i++ )
	{
		//CBaseEntity *pEntity = NULL;
		char *pMapData;
		int iTemplateIndex = m_hTemplates[i].iTemplateIndex;

		// Some templates have Entity I/O connecting the entities within the template.
		// Unique versions of these templates need to be created whenever they're instanced.
		int nStringSize;
		if ( AllowNameFixup() && Templates_IndexRequiresEntityIOFixup( iTemplateIndex ) )
		{
			// This template requires instancing. 
			// Create a new mapdata block and ask the template system to fill it in with
			// a unique version (with fixed Entity I/O connections).
			pMapData = Templates_GetEntityIOFixedMapData( iTemplateIndex );
			
		}
		else
		{
			// Use the unmodified mapdata
			pMapData = (char*)STRING( Templates_FindByIndex( iTemplateIndex ) );
		}

		nStringSize = Templates_GetStringSize( iTemplateIndex );

		// Create the entity from the mapdata
		MapEntity_PrecacheEntity( pMapData, nStringSize );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Spawn the entities I contain
// Input  : &vecOrigin - 
//			&vecAngles - 
//			pEntities - 
//			pEntityMaker - The Entity Maker entity that invoked this call.
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CPointTemplate::CreateInstance( const Vector &vecOrigin, const QAngle &vecAngles, std::vector< CBaseEntity* > *pEntities, CBaseEntity *pEntityMaker, bool bCreateTime )
{
	ks::ScratchArena<> scratch;
	// Go through all our templated map data and spawn all the entities in it
	int iTemplates = std::ssize( m_hTemplates );
	if ( !iTemplates )
	{
		Msg("CreateInstance called on a point_template that has no templates: %s\n", STRING(GetEntityName()) );
		return false;
	}

	// Tell the template system we're about to start a new template
	Templates_StartUniqueInstance();

	HierarchicalSpawn_t *pSpawnList = scratch.Alloc<HierarchicalSpawn_t>( iTemplates );

	int i;
	for ( i = 0; i < iTemplates; i++ )
	{
		CBaseEntity *pEntity = nullptr;
		char *pMapData;
		int iTemplateIndex = m_hTemplates[i].iTemplateIndex;

		// Some templates have Entity I/O connecting the entities within the template.
		// Unique versions of these templates need to be created whenever they're instanced.
		if ( AllowNameFixup() && ( Templates_IndexRequiresEntityIOFixup( iTemplateIndex ) || m_ScriptScope.IsInitialized() ) )
		{
			// This template requires instancing. 
			// Create a new mapdata block and ask the template system to fill it in with
			// a unique version (with fixed Entity I/O connections).
			pMapData = Templates_GetEntityIOFixedMapData( iTemplateIndex );
		}
		else
		{
			// Use the unmodified mapdata
			pMapData = (char*)STRING( Templates_FindByIndex( iTemplateIndex ) );
		}

		// Create the entity from the mapdata
		MapEntity_ParseEntity( pEntity, pMapData, nullptr );
		if ( pEntity == nullptr )
		{
			Msg("Failed to initialize templated entity with mapdata: %s\n", pMapData );
			return false;
		}

		// Get a matrix that'll convert from world to the new local space
		VMatrix matNewTemplateToWorld, matStoredLocalToWorld;
		matNewTemplateToWorld.SetupMatrixOrgAngles( vecOrigin, vecAngles );
		MatrixMultiply( matNewTemplateToWorld, m_hTemplates[i].matEntityToTemplate, matStoredLocalToWorld );

		// Get the world origin & angles from the stored local coordinates
		Vector vecNewOrigin;
		QAngle vecNewAngles;
		vecNewOrigin = matStoredLocalToWorld.GetTranslation();
		MatrixToAngles( matStoredLocalToWorld, vecNewAngles );

		// Set its origin & angles
		pEntity->SetAbsOrigin( vecNewOrigin );
		pEntity->SetAbsAngles( vecNewAngles );

		if( AllowNameFixup() )
		{
			pEntity->MarkNeedsNamePurge();
		}

		if ( ScriptPreInstanceSpawn( &m_ScriptScope, pEntity, Templates_FindByIndex( iTemplateIndex ) ) )
		{
			pSpawnList[i].m_pEntity = pEntity;
		}
		else
		{
			pSpawnList[i].m_pEntity = nullptr;
			UTIL_RemoveImmediate( pEntity );
		}
		pSpawnList[i].m_nDepth = 0;
		pSpawnList[i].m_pDeferredParent = nullptr;
	}

	SpawnHierarchicalList( iTemplates, pSpawnList, true );

	// Set the time of creation for these entities.
	if ( bCreateTime )
	{
	}

	for ( i = 0; i < iTemplates; ++i )
	{
		if ( pSpawnList[i].m_pEntity )
		{
			pEntities->push_back( pSpawnList[i].m_pEntity );
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CPointTemplate::CreationComplete( const std::vector< CBaseEntity* > &entities )
{
	if ( entities.empty() )
		return;

	ScriptPostSpawn( &m_ScriptScope, (CBaseEntity **)entities.data(), std::ssize( entities ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CPointTemplate::InputForceSpawn( inputdata_t &inputdata )
{
	// Spawn our template
	std::vector< CBaseEntity* > hNewEntities;
	if ( !CreateInstance( GetAbsOrigin(), GetAbsAngles(), &hNewEntities ) )
		return;
	
	CreationComplete( hNewEntities );

	// Fire our output
	m_pOutputOnSpawned.FireOutput( this, this );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void ScriptInstallPreSpawnHook()
{
	if ( g_pScriptVM && !g_pScriptVM->ValueExists( "__ExecutePreSpawn" ) )
	{
		g_pScriptVM->Run( g_Script_spawn_helper );
	}
}

//-----------------------------------------------------------------------------
// Purpose:	This function is called after a spawner creates its child entity
//			but before the keyvalues are injected. This gives us an 
//			opportunity to change any keyvalues before the entity is 
//			configured and spawned. In this case, we see if there is a VScript
//			that wants to change anything about this entity. 
//-----------------------------------------------------------------------------
bool ScriptPreInstanceSpawn( CScriptScope *pScriptScope, CBaseEntity *pChild, string_t iszKeyValueData )
{
	if ( !pScriptScope->IsInitialized() )
	{
		return true;
	}

	if ( !pScriptScope->ValueExists( "PreSpawnInstance" ) )
	{
		return true;
	}

	ScriptVariant_t result;
	if ( pScriptScope->Call( "__ExecutePreSpawn", &result, ToHScript( pChild ) ) != SCRIPT_DONE )
		return true;

	if ( ( result.m_type == FIELD_BOOLEAN && !result.m_bool ) || ( result.m_type == FIELD_INTEGER && !result.m_int ) )
		return false;

	return true;

}

void ScriptPostSpawn( CScriptScope *pScriptScope, CBaseEntity **ppEntities, int nEntities )
{
	if ( !pScriptScope->IsInitialized() )
		return;

	HSCRIPT hPostSpawnFunc = pScriptScope->LookupFunction( "PostSpawn" );

	if ( !hPostSpawnFunc )
		return;

	ScriptVariant_t varEntityMakerResultTable;
	if ( g_pScriptVM->GetValue( *pScriptScope, "__EntityMakerResult", &varEntityMakerResultTable ) )
	{
		if ( varEntityMakerResultTable.m_type == FIELD_HSCRIPT )
		{
			HSCRIPT hEntityMakerResultTable = varEntityMakerResultTable.m_hScript;
			char szEntName[256];
			for ( int i = 0; i < nEntities; i++ )
			{
				V_strncpy( szEntName, ppEntities[i]->GetEntityNameAsCStr(), std::size( szEntName ) );
				char *pAmpersand = V_strrchr( szEntName, '&' );
				if ( pAmpersand )
					*pAmpersand = 0;
				g_pScriptVM->SetValue( hEntityMakerResultTable, szEntName, ToHScript( ppEntities[i] ) );
			}
			pScriptScope->Call( hPostSpawnFunc, nullptr, hEntityMakerResultTable );
			pScriptScope->Call( "__FinishSpawn" );
		}
		g_pScriptVM->ReleaseValue( varEntityMakerResultTable );
	}
	g_pScriptVM->ReleaseFunction( hPostSpawnFunc );
}

class CBaseEntity;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Used to fire events based on the orientation of a given entity.
//			Looks at its target's anglular velocity every frame and fires outputs
//			as the angular velocity passes a given threshold value.

enum
{
	AVELOCITY_SENSOR_NO_LAST_RESULT = -2
};

ConVar g_debug_angularsensor( side_registry(), "g_debug_angularsensor", "0", FCVAR_CHEAT );

class CPointAngularVelocitySensor : public CPointEntity
{
	DECLARE_CLASS( CPointAngularVelocitySensor, CPointEntity );

public:

	CPointAngularVelocitySensor();
	void Activate_Impl(void);
	void Spawn_Impl(void);
	void Think_Impl(void);

private:

	float SampleAngularVelocity(CBaseEntity *pEntity);
	int CompareToThreshold(CBaseEntity *pEntity, float flThreshold, bool bFireVelocityOutput);
	void FireCompareOutput(int nCompareResult, CBaseEntity *pActivator);
	void DrawDebugLines( void );

	// Input handlers
	[[= ks::reflect::Input{ .name = "Test", .type = FIELD_VOID } ]] void InputTest( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "TestWithInterval", .type = FIELD_VOID } ]] void InputTestWithInterval( inputdata_t &inputdata );

	EHANDLE m_hTargetEntity;				// Entity whose angles are being monitored.
	[[= ks::reflect::Key{ .name = "threshold" } ]] float m_flThreshold;					// The threshold angular velocity that we are looking for.
	int m_nLastCompareResult;				// The comparison result from our last measurement, expressed as -1, 0, or 1
	int m_nLastFireResult;					// The last result for which we fire the output.
	
	float m_flFireTime;
	[[= ks::reflect::Key{ .name = "fireinterval" } ]] float m_flFireInterval;
	float m_flLastAngVelocity;
	
	QAngle m_lastOrientation;

	[[= ks::reflect::Key{ .name = "axis" } ]] Vector m_vecAxis;
	[[= ks::reflect::Key{ .name = "usehelper" } ]] bool m_bUseHelper;

	// Outputs
	[[= ks::reflect::Key{ .name = "AngularVelocity" } ]] COutputFloat m_AngularVelocity;

	// Compare the target's angular velocity to the threshold velocity and fire the appropriate output.
	// These outputs are filtered by m_flFireInterval to ignore excessive oscillations.
	[[= ks::reflect::Key{ .name = "OnLessThan" } ]] COutputEvent m_OnLessThan;
	[[= ks::reflect::Key{ .name = "OnLessThanOrEqualTo" } ]] COutputEvent m_OnLessThanOrEqualTo;		
	[[= ks::reflect::Key{ .name = "OnGreaterThan" } ]] COutputEvent m_OnGreaterThan;			
	[[= ks::reflect::Key{ .name = "OnGreaterThanOrEqualTo" } ]] COutputEvent m_OnGreaterThanOrEqualTo;
	[[= ks::reflect::Key{ .name = "OnEqualTo" } ]] COutputEvent m_OnEqualTo;

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS(point_angularvelocitysensor, CPointAngularVelocitySensor);

IMPLEMENT_REFLECT_DATAMAP( CPointAngularVelocitySensor )

//-----------------------------------------------------------------------------
// Purpose: constructor provides default values
//-----------------------------------------------------------------------------
CPointAngularVelocitySensor::CPointAngularVelocitySensor()
{
	m_flFireInterval = 0.2f;
}

//-----------------------------------------------------------------------------
// Purpose: Called when spawning after parsing keyvalues.
//-----------------------------------------------------------------------------
void CPointAngularVelocitySensor::Spawn_Impl(void)
{
	m_flThreshold = fabs(m_flThreshold);
	m_nLastFireResult = AVELOCITY_SENSOR_NO_LAST_RESULT;
	m_nLastCompareResult = AVELOCITY_SENSOR_NO_LAST_RESULT;
	// m_flFireInterval = 0.2;
	m_lastOrientation = vec3_angle;
}

//-----------------------------------------------------------------------------
// Purpose: Called after all entities in the map have spawned.
//-----------------------------------------------------------------------------
void CPointAngularVelocitySensor::Activate_Impl(void)
{
	BaseClass::Activate_Impl();

	m_hTargetEntity = gEntList.FindEntityByName( nullptr, m_target );

	if (m_hTargetEntity)
	{
		SetNextThink( gpGlobals->curtime );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Draws magic lines...
//-----------------------------------------------------------------------------
void CPointAngularVelocitySensor::DrawDebugLines( void )
{
	if ( m_hTargetEntity )
	{
		Vector vForward, vRight, vUp;
		AngleVectors( m_hTargetEntity->GetAbsAngles(), &vForward, &vRight, &vUp );

		NDebugOverlay::Line( GetAbsOrigin(), GetAbsOrigin() + vForward * 64, 255, 0, 0, false, 0 );
		NDebugOverlay::Line( GetAbsOrigin(), GetAbsOrigin() + vRight * 64, 0, 255, 0, false, 0 );
		NDebugOverlay::Line( GetAbsOrigin(), GetAbsOrigin() + vUp * 64, 0, 0, 255, false, 0 );
	}

	if ( m_bUseHelper == true )
	{
		QAngle Angles;
		Vector vAxisForward, vAxisRight, vAxisUp;

		Vector vLine = m_vecAxis - GetAbsOrigin();

		VectorNormalize( vLine );

		VectorAngles( vLine, Angles );
		AngleVectors( Angles, &vAxisForward, &vAxisRight, &vAxisUp );

		NDebugOverlay::Line( GetAbsOrigin(), GetAbsOrigin() + vAxisForward * 64, 255, 0, 0, false, 0 );
		NDebugOverlay::Line( GetAbsOrigin(), GetAbsOrigin() + vAxisRight * 64, 0, 255, 0, false, 0 );
		NDebugOverlay::Line( GetAbsOrigin(), GetAbsOrigin() + vAxisUp * 64, 0, 0, 255, false, 0 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns the magnitude of the entity's angular velocity.
//-----------------------------------------------------------------------------
float CPointAngularVelocitySensor::SampleAngularVelocity(CBaseEntity *pEntity)
{
	if (pEntity->GetMoveType() == MOVETYPE_VPHYSICS)
	{
		IPhysicsObject *pPhys = pEntity->VPhysicsGetObject();
		if (pPhys != nullptr)
		{
			Vector vecVelocity;
			AngularImpulse vecAngVelocity;
			pPhys->GetVelocity(&vecVelocity, &vecAngVelocity);

			QAngle angles;
			pPhys->GetPosition( nullptr, &angles );

			float dt = gpGlobals->curtime - GetLastThink();
			if ( dt == 0 )
				dt = 0.1;

			// HACKHACK: We don't expect a real 'delta' orientation here, just enough of an error estimate to tell if this thing
			// is trying to move, but failing.
			QAngle delta = angles - m_lastOrientation;

			if ( ( delta.Length() / dt )  < ( vecAngVelocity.Length() * 0.01 ) )
			{
				return 0.0f;
			}
			m_lastOrientation = angles;

			if ( m_bUseHelper == false )
			{
				return vecAngVelocity.Length();
			}
			else
			{
				Vector vLine = m_vecAxis - GetAbsOrigin();
				VectorNormalize( vLine );

				Vector vecWorldAngVelocity;
				pPhys->LocalToWorldVector( &vecWorldAngVelocity, vecAngVelocity );
				float flDot = DotProduct( vecWorldAngVelocity, vLine );

				return flDot;
			}
		}
	}
	else
	{
		QAngle vecAngVel = pEntity->GetLocalAngularVelocity();
		float flMax = std::max(fabs(vecAngVel[PITCH]), fabs(vecAngVel[YAW]));

		return std::max(flMax, fabs(vecAngVel[ROLL]));
	}

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Compares the given entity's angular velocity to the threshold velocity.
// Input  : pEntity - Entity whose angular velocity is being measured.
//			flThreshold - 
// Output : Returns -1 if less than, 0 if equal to, or 1 if greater than the threshold.
//-----------------------------------------------------------------------------
int CPointAngularVelocitySensor::CompareToThreshold(CBaseEntity *pEntity, float flThreshold, bool bFireVelocityOutput)
{
	if (pEntity == nullptr)
	{
		return 0;
	}

	float flAngVelocity = SampleAngularVelocity(pEntity);

	if ( g_debug_angularsensor.GetBool() )
	{
		DrawDebugLines();
	}

	if (bFireVelocityOutput && (flAngVelocity != m_flLastAngVelocity))
	{
		m_AngularVelocity.Set(flAngVelocity, pEntity, this);
		m_flLastAngVelocity = flAngVelocity;
	}

	if (flAngVelocity > flThreshold)
	{
		return 1;
	}

	if (flAngVelocity == flThreshold)
	{
		return 0;
	}

	return -1;
}

//-----------------------------------------------------------------------------
// Called every frame to sense the angular velocity of the target entity.
// Output is filtered by m_flFireInterval to ignore excessive oscillations.
//-----------------------------------------------------------------------------
void CPointAngularVelocitySensor::Think_Impl(void)
{
	if (m_hTargetEntity != nullptr)
	{
		//
		// Check to see if the measure entity's angular velocity has been within
		// tolerance of the threshold for the given period of time.
		//
		int nCompare = CompareToThreshold(m_hTargetEntity, m_flThreshold, true);
		if (nCompare != m_nLastCompareResult)
		{
			// If we've oscillated back to where we last fired the output, don't
			// fire the same output again.
			if (nCompare == m_nLastFireResult)
			{
				m_flFireTime = 0;
			}
			else if (m_nLastCompareResult != AVELOCITY_SENSOR_NO_LAST_RESULT)
			{
				//
				// The value has changed -- reset the timer. We'll fire the output if
				// it stays at this value until the interval expires.
				//
				m_flFireTime = gpGlobals->curtime + m_flFireInterval;
			}
			
			m_nLastCompareResult = nCompare;
		}
		else if ((m_flFireTime != 0) && (gpGlobals->curtime >= m_flFireTime))
		{
			//
			// The compare result has held steady long enough -- time to
			// fire the output.
			//
			FireCompareOutput(nCompare, this);
			m_nLastFireResult = nCompare;
			m_flFireTime = 0;
		}

		SetNextThink( gpGlobals->curtime );
	}
}

//-----------------------------------------------------------------------------
// Fires the output after the fire interval if the velocity is stable. 
//-----------------------------------------------------------------------------
void CPointAngularVelocitySensor::InputTestWithInterval( inputdata_t &inputdata )
{
	if (m_hTargetEntity != nullptr)
	{
		m_flFireTime = gpGlobals->curtime + m_flFireInterval;
		m_nLastFireResult = AVELOCITY_SENSOR_NO_LAST_RESULT;
		m_nLastCompareResult = CompareToThreshold(m_hTargetEntity, m_flThreshold, true);

		SetNextThink( gpGlobals->curtime );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for forcing an instantaneous test of the condition.
//-----------------------------------------------------------------------------
void CPointAngularVelocitySensor::InputTest( inputdata_t &inputdata )
{
	int nCompareResult = CompareToThreshold(m_hTargetEntity, m_flThreshold, false);
	FireCompareOutput(nCompareResult, inputdata.pActivator);
}

//-----------------------------------------------------------------------------
// Purpose: Fires the appropriate output based on the given comparison result.
// Input  : nCompareResult - 
//			pActivator - 
//-----------------------------------------------------------------------------
void CPointAngularVelocitySensor::FireCompareOutput( int nCompareResult, CBaseEntity *pActivator )
{
	if (nCompareResult == -1)
	{
		m_OnLessThan.FireOutput(pActivator, this);
		m_OnLessThanOrEqualTo.FireOutput(pActivator, this);
	}
	else if (nCompareResult == 1)
	{
		m_OnGreaterThan.FireOutput(pActivator, this);
		m_OnGreaterThanOrEqualTo.FireOutput(pActivator, this);
	}
	else
	{
		m_OnEqualTo.FireOutput(pActivator, this);
		m_OnLessThanOrEqualTo.FireOutput(pActivator, this);
		m_OnGreaterThanOrEqualTo.FireOutput(pActivator, this);
	}
}

// ============================================================================
//
//  Simple velocity sensor
//
// ============================================================================

class CPointVelocitySensor : public CPointEntity
{
	DECLARE_CLASS( CPointVelocitySensor, CPointEntity );

public:

	void Spawn_Impl();
	void Activate_Impl( void );
	void Think_Impl( void );

private:

	void SampleVelocity( void );

	EHANDLE m_hTargetEntity;				// Entity whose angles are being monitored.
	[[= ks::reflect::Key{ .name = "axis" } ]] Vector	m_vecAxis;						// Axis along which to measure the speed.
	[[= ks::reflect::Key{ .name = "enabled" } ]] bool	m_bEnabled;						// Whether we're measuring or not

	// Outputs
	float m_fPrevVelocity; // stores velocity from last frame, so we only write the output if it has changed
	[[= ks::reflect::Key{ .name = "Velocity" } ]] COutputFloat m_Velocity;

	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void	InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void	InputDisable( inputdata_t &inputdata );

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( point_velocitysensor, CPointVelocitySensor );

IMPLEMENT_REFLECT_DATAMAP( CPointVelocitySensor )

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CPointVelocitySensor::Spawn_Impl()
{
	Vector vLine = m_vecAxis - GetAbsOrigin();
	VectorNormalize( vLine );
	m_vecAxis = vLine;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointVelocitySensor::Activate_Impl( void )
{
	BaseClass::Activate_Impl();

	m_hTargetEntity = gEntList.FindEntityByName( nullptr, m_target );
	
	if ( m_bEnabled && m_hTargetEntity )
	{
		SetNextThink( gpGlobals->curtime );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointVelocitySensor::InputEnable( inputdata_t &inputdata )
{
	// Don't interrupt us if we're already enabled
	if ( m_bEnabled )
		return;

	m_bEnabled = true;
	
	if ( m_hTargetEntity )
	{
		SetNextThink( gpGlobals->curtime );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointVelocitySensor::InputDisable( inputdata_t &inputdata )
{
	m_bEnabled = false;
}

//-----------------------------------------------------------------------------
// Purpose: Called every frame
//-----------------------------------------------------------------------------
void CPointVelocitySensor::Think_Impl( void )
{
	if ( m_hTargetEntity != nullptr && m_bEnabled )
	{
		SampleVelocity();
		SetNextThink( gpGlobals->curtime );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns the magnitude of the entity's angular velocity.
//-----------------------------------------------------------------------------
void CPointVelocitySensor::SampleVelocity( void )
{
	if ( m_hTargetEntity == nullptr )
		return;

	Vector vecVelocity;

	if ( m_hTargetEntity->GetMoveType() == MOVETYPE_VPHYSICS )
	{
		IPhysicsObject *pPhys = m_hTargetEntity->VPhysicsGetObject();
		if ( pPhys != nullptr )
		{
			pPhys->GetVelocity( &vecVelocity, nullptr );
		}
	}
	else
	{
		vecVelocity = m_hTargetEntity->GetAbsVelocity();
	}

	/*
	float flSpeed = VectorNormalize( vecVelocity );
	float flDot = ( m_vecAxis != vec3_origin ) ? DotProduct( vecVelocity, m_vecAxis ) : 1.0f;
	*/
	// We want the component of the velocity vector in the direction of the axis, which since the
	// axis is normalized is simply their dot product (eg V . A = |V|*|A|*cos(theta) )
	m_fPrevVelocity = ( m_vecAxis != vec3_origin ) ? DotProduct( vecVelocity, m_vecAxis ) : 1.0f;

	// if it's changed since the last frame, poke the output 
	if ( m_fPrevVelocity != m_Velocity.Get() )
	{
		m_Velocity.Set( m_fPrevVelocity, nullptr, nullptr );
	}
}

class CBaseEntity;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Used to fire events based on the orientation of a given entity.
//			Looks at its target's angles every frame and fires an output if its
//			target's forward vector points at a specified lookat entity for more
//			than a specified length of time.
//			It also fires an output whenever the target's angles change.

static constexpr auto SF_USE_TARGET_FACING = (1<<0); // Use the target entity's direction instead of position

class CPointAngleSensor : public CPointEntity
{
	DECLARE_CLASS(CPointAngleSensor, CPointEntity);
public:

	using BaseClass::KeyValue_Impl;
	bool KeyValue_Impl(const char *szKeyName, const char *szValue);
	void Activate_Impl(void);
	void Spawn_Impl(void);
	void Think_Impl(void);

	int DrawDebugTextOverlays_Impl(void);

protected:

	void Enable();
	void Disable();

	// Input handlers
	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void InputEnable(inputdata_t &inputdata);
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void InputDisable(inputdata_t &inputdata);
	[[= ks::reflect::Input{ .name = "Toggle", .type = FIELD_VOID } ]] void InputToggle(inputdata_t &inputdata);
	[[= ks::reflect::Input{ .name = "Test", .type = FIELD_VOID } ]] void InputTest(inputdata_t &inputdata);
	[[= ks::reflect::Input{ .name = "SetTargetEntity", .type = FIELD_STRING } ]] void InputSetTargetEntity(inputdata_t &inputdata);

	bool IsFacingWithinTolerance(CBaseEntity *pEntity, CBaseEntity *pTarget, float flTolerance, float *pflDot = nullptr);

	[[= ks::reflect::Key{ .name = "StartDisabled" } ]] bool m_bDisabled;				// When disabled, we do not think or fire outputs.
	[[= ks::reflect::Key{ .name = "lookatname" } ]] string_t m_nLookAtName;			// Name of the entity that the target must point at to fire the OnTrue output.

	EHANDLE m_hTargetEntity;		// Entity whose angles are being monitored.
	EHANDLE m_hLookAtEntity;		// Entity that the target must look at to fire the OnTrue output.

	[[= ks::reflect::Key{ .name = "duration" } ]] float m_flDuration;				// Time in seconds for which the entity must point at the target.
	float m_flDotTolerance;			// Degrees of error allowed to satisfy the condition, expressed as a dot product.
	float m_flFacingTime;			// The time at which the target entity pointed at the lookat entity.
	bool m_bFired;					// Latches the output so it only fires once per true.

	// Outputs
	[[= ks::reflect::Key{ .name = "OnFacingLookat" } ]] COutputEvent	m_OnFacingLookat;		// Fired when the target points at the lookat entity.
	[[= ks::reflect::Key{ .name = "OnNotFacingLookat" } ]] COutputEvent	m_OnNotFacingLookat;	// Fired in response to a Test input if the target is not looking at the lookat entity.
	[[= ks::reflect::Key{ .name = "TargetDir" } ]] COutputVector	m_TargetDir;
	[[= ks::reflect::Key{ .name = "FacingPercentage" } ]] COutputFloat	m_FacingPercentage;	// Normalize value representing how close the entity is to facing directly at the target

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS(point_anglesensor, CPointAngleSensor);

IMPLEMENT_REFLECT_DATAMAP( CPointAngleSensor )

//-----------------------------------------------------------------------------
// Purpose: Handles keyvalues that require special processing.
// Output : Returns true if handled, false if not.
//-----------------------------------------------------------------------------
bool CPointAngleSensor::KeyValue_Impl(const char *szKeyName, const char *szValue)
{
	if (FStrEq(szKeyName, "tolerance"))
	{
		float flTolerance = std::atof(szValue);
		m_flDotTolerance = std::cos(DEG2RAD(flTolerance));
	}
	else
	{
		return(BaseClass::KeyValue_Impl(szKeyName, szValue));
	}

	return(true);
}

//-----------------------------------------------------------------------------
// Purpose: Called when spawning after parsing keyvalues.
//-----------------------------------------------------------------------------
void CPointAngleSensor::Spawn_Impl(void)
{
	BaseClass::Spawn_Impl();
}

//-----------------------------------------------------------------------------
// Purpose: Called after all entities have spawned on new map or savegame load.
//-----------------------------------------------------------------------------
void CPointAngleSensor::Activate_Impl(void)
{
	BaseClass::Activate_Impl();

	if (!m_hTargetEntity)
	{
		m_hTargetEntity = gEntList.FindEntityByName( nullptr, m_target );
	}

	if (!m_hLookAtEntity && (m_nLookAtName != NULL_STRING))
	{
		m_hLookAtEntity = gEntList.FindEntityByName( nullptr, m_nLookAtName );
		if (!m_hLookAtEntity)
		{
			DevMsg(1, "Angle sensor '%s' could not find look at entity '%s'.\n", GetDebugName(), STRING(m_nLookAtName));
		}
	}

	// It's okay to not have a look at entity, it just means we measure and output the angles
	// of the target entity without testing them against the look at entity.
	if (!m_bDisabled && m_hTargetEntity)
	{
		SetNextThink( gpGlobals->curtime );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Determines if one entity is facing within a given tolerance of another
// Input  : pEntity - 
//			pTarget - 
//			flTolerance - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CPointAngleSensor::IsFacingWithinTolerance(CBaseEntity *pEntity, CBaseEntity *pTarget, float flTolerance, float *pflDot)
{
	if (pflDot)
	{
		*pflDot = 0;
	}
	
	if ((pEntity == nullptr) || (pTarget == nullptr))
	{
		return(false);
	}

	Vector forward;
	pEntity->GetVectors(&forward, nullptr, nullptr);

	Vector dir;
	// Use either our position relative to the target, or the target's raw facing
	if ( HasSpawnFlags( SF_USE_TARGET_FACING ) )
	{
		pTarget->GetVectors(&dir, nullptr, nullptr);
	}
	else
	{
		dir = pTarget->GetAbsOrigin() - pEntity->GetAbsOrigin();
		VectorNormalize(dir);
	}
		
	//
	// Larger dot product corresponds to a smaller angle.
	//
	float flDot = dir.Dot(forward);
	if (pflDot)
	{
		*pflDot = flDot;
	}

	if (flDot >= m_flDotTolerance)
	{	
		return(true);
	}

	return(false);
}

//-----------------------------------------------------------------------------
// Purpose: Called every frame.
//-----------------------------------------------------------------------------
void CPointAngleSensor::Think_Impl(void)
{
	if (m_hTargetEntity != nullptr)
	{
		Vector forward;
		m_hTargetEntity->GetVectors(&forward, nullptr, nullptr);
		m_TargetDir.Set(forward, this, this);

		if (m_hLookAtEntity != nullptr)
		{
			//
			// Check to see if the measure entity's forward vector has been within
			// given tolerance of the target entity for the given period of time.
			//
			float flDot;
			if (IsFacingWithinTolerance(m_hTargetEntity, m_hLookAtEntity, m_flDotTolerance, &flDot ))
			{
				if (!m_bFired)
				{
					if (!m_flFacingTime)
					{
						m_flFacingTime = gpGlobals->curtime;
					}

					if (gpGlobals->curtime >= m_flFacingTime + m_flDuration)
					{
						m_OnFacingLookat.FireOutput(this, this);
						m_bFired = true;
					}
				}
			}
			else 
			{
				// Reset the fired state
				if ( m_bFired )
				{
					m_bFired = false;
				}

				// Always reset the time when we've lost our facing
				m_flFacingTime = 0;
			}
			
			// Output the angle range we're in
			float flPerc = RemapValClamped( flDot, 1.0f, m_flDotTolerance, 1.0f, 0.0f );
			m_FacingPercentage.Set( flPerc, this, this );
		}

		SetNextThink( gpGlobals->curtime );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for forcing an instantaneous test of the condition.
//-----------------------------------------------------------------------------
void CPointAngleSensor::InputTest(inputdata_t &inputdata)
{
	if (IsFacingWithinTolerance(m_hTargetEntity, m_hLookAtEntity, m_flDotTolerance))
	{
		m_OnFacingLookat.FireOutput(inputdata.pActivator, this);
	}
	else
	{
		m_OnNotFacingLookat.FireOutput(inputdata.pActivator, this);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointAngleSensor::InputSetTargetEntity(inputdata_t &inputdata)
{
	if ((inputdata.value.String() == nullptr) || (inputdata.value.StringID() == NULL_STRING) || (inputdata.value.String()[0] == '\0'))
	{
		m_target = NULL_STRING;
		m_hTargetEntity = nullptr;
		SetNextThink( TICK_NEVER_THINK );
	}
	else
	{
		m_target = AllocPooledString(inputdata.value.String());
		m_hTargetEntity = gEntList.FindEntityByName( nullptr, m_target, nullptr, inputdata.pActivator, inputdata.pCaller );
		if (!m_bDisabled && m_hTargetEntity)
		{
			SetNextThink( gpGlobals->curtime );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointAngleSensor::InputEnable(inputdata_t &inputdata)
{
	Enable();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointAngleSensor::InputDisable(inputdata_t &inputdata)
{
	Disable();
}

//-----------------------------------------------------------------------------
// Purpose: I like separators between my functions.
//-----------------------------------------------------------------------------
void CPointAngleSensor::InputToggle(inputdata_t &inputdata)
{
	if (m_bDisabled)
	{
		Enable();
	}
	else
	{
		Disable();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointAngleSensor::Enable()
{
	m_bDisabled = false;
	if (m_hTargetEntity)
	{
		SetNextThink(gpGlobals->curtime);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointAngleSensor::Disable()
{
	m_bDisabled = true;
	SetNextThink(TICK_NEVER_THINK);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CPointAngleSensor::DrawDebugTextOverlays_Impl(void)
{
	int nOffset = BaseClass::DrawDebugTextOverlays_Impl();

	if (m_debugOverlays & OVERLAY_TEXT_BIT) 
	{
		float flDot;
		bool bFacing = IsFacingWithinTolerance(m_hTargetEntity, m_hLookAtEntity, m_flDotTolerance, &flDot);

		char tempstr[512];
		V_snprintf(tempstr, sizeof(tempstr), "delta ang (dot)    : %.2f (%f)", RAD2DEG(std::acos(flDot)), flDot);
		EntityText( nOffset, tempstr, 0);
		nOffset++;

		V_snprintf(tempstr, sizeof(tempstr), "tolerance ang (dot): %.2f (%f)", RAD2DEG(acos(m_flDotTolerance)), m_flDotTolerance);
		EntityText( nOffset, tempstr, 0);
		nOffset++;

		V_snprintf(tempstr, sizeof(tempstr), "facing: %s", bFacing ? "yes" : "no");
		EntityText( nOffset, tempstr, 0);
		nOffset++;
	}

	return nOffset;
}

// ====================================================================
//  Proximity sensor
// ====================================================================

static constexpr auto SF_PROXIMITY_TEST_AGAINST_AXIS = (1<<0);

class CPointProximitySensor : public CPointEntity
{
	DECLARE_CLASS( CPointProximitySensor, CPointEntity );

public:

	void Activate_Impl( void );

protected:

	public: void Think_Impl( void );
	protected:
	void Enable( void );
	void Disable( void );

	// Input handlers
	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void InputEnable(inputdata_t &inputdata);
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void InputDisable(inputdata_t &inputdata);
	[[= ks::reflect::Input{ .name = "Toggle", .type = FIELD_VOID } ]] void InputToggle(inputdata_t &inputdata);
	[[= ks::reflect::Input{ .name = "SetTargetEntity", .type = FIELD_STRING } ]] void InputSetTargetEntity(inputdata_t &inputdata);

private:

	[[= ks::reflect::Key{ .name = "StartDisabled" } ]] bool	m_bDisabled;			// When disabled, we do not think or fire outputs.
	EHANDLE m_hTargetEntity;		// Entity whose angles are being monitored.

	[[= ks::reflect::Key{ .name = "Distance" } ]] COutputFloat	m_Distance;

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( point_proximity_sensor, CPointProximitySensor );

IMPLEMENT_REFLECT_DATAMAP( CPointProximitySensor )

//-----------------------------------------------------------------------------
// Purpose: Called after all entities have spawned on new map or savegame load.
//-----------------------------------------------------------------------------
void CPointProximitySensor::Activate_Impl( void )
{
	BaseClass::Activate_Impl();

	if ( m_hTargetEntity == nullptr )
	{
		m_hTargetEntity = gEntList.FindEntityByName( nullptr, m_target );
	}

	if ( m_bDisabled == false && m_hTargetEntity != nullptr )
	{
		SetNextThink( gpGlobals->curtime );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointProximitySensor::InputSetTargetEntity(inputdata_t &inputdata)
{
	if ((inputdata.value.String() == nullptr) || (inputdata.value.StringID() == NULL_STRING) || (inputdata.value.String()[0] == '\0'))
	{
		m_target = NULL_STRING;
		m_hTargetEntity = nullptr;
		SetNextThink( TICK_NEVER_THINK );
	}
	else
	{
		m_target = AllocPooledString(inputdata.value.String());
		m_hTargetEntity = gEntList.FindEntityByName( nullptr, m_target, nullptr, inputdata.pActivator, inputdata.pCaller );
		if (!m_bDisabled && m_hTargetEntity)
		{
			SetNextThink( gpGlobals->curtime );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointProximitySensor::InputEnable( inputdata_t &inputdata )
{
	Enable();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointProximitySensor::InputDisable( inputdata_t &inputdata )
{
	Disable();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CPointProximitySensor::InputToggle( inputdata_t &inputdata )
{
	if ( m_bDisabled )
	{
		Enable();
	}
	else
	{
		Disable();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointProximitySensor::Enable( void )
{
	m_bDisabled = false;
	if ( m_hTargetEntity )
	{
		SetNextThink( gpGlobals->curtime );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointProximitySensor::Disable( void )
{
	m_bDisabled = true;
	SetNextThink( TICK_NEVER_THINK );
}

//-----------------------------------------------------------------------------
// Purpose: Called every frame
//-----------------------------------------------------------------------------
void CPointProximitySensor::Think_Impl( void )
{
	if ( m_hTargetEntity != nullptr )
	{
		Vector vecTestDir = ( m_hTargetEntity->GetAbsOrigin() - GetAbsOrigin() );
		float flDist = VectorNormalize( vecTestDir );

		// If we're only interested in the distance along a vector, modify the length the accomodate that
		if ( HasSpawnFlags( SF_PROXIMITY_TEST_AGAINST_AXIS ) )
		{
			Vector vecDir;
			GetVectors( &vecDir, nullptr, nullptr );

			float flDot = DotProduct( vecTestDir, vecDir );
			flDist *= fabs( flDot );
		}

		m_Distance.Set( flDist, this, this );
		SetNextThink( gpGlobals->curtime );
	}
}

class CBaseEntity;
class CTakeDamageInfo;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Implements hurting point entity

static const int SF_PHURT_START_ON			= 1;

class CPointHurt : public CPointEntity
{
	DECLARE_CLASS( CPointHurt, CPointEntity );

public:
	void	Spawn_Impl( void );
	void	Precache_Impl( void );
	void	HurtThink( void );

	// Input handlers
	[[= ks::reflect::Input{ .name = "TurnOn", .type = FIELD_VOID } ]] void InputTurnOn(inputdata_t &inputdata);
	[[= ks::reflect::Input{ .name = "TurnOff", .type = FIELD_VOID } ]] void InputTurnOff(inputdata_t &inputdata);
	[[= ks::reflect::Input{ .name = "Toggle", .type = FIELD_VOID } ]] void InputToggle(inputdata_t &inputdata);
	[[= ks::reflect::Input{ .name = "Hurt", .type = FIELD_VOID } ]] void InputHurt(inputdata_t &inputdata);
	
	DECLARE_DATADESC();

	[[= ks::reflect::Key{ .name = "Damage" } ]] int			m_nDamage;
	[[= ks::reflect::Key{ .name = "DamageType" } ]] int			m_bitsDamageType;
	[[= ks::reflect::Key{ .name = "DamageRadius" } ]] float		m_flRadius;
	[[= ks::reflect::Key{ .name = "DamageDelay" } ]] float		m_flDelay;
	[[= ks::reflect::Key{ .name = "DamageTarget" } ]] string_t	m_strTarget;
	EHANDLE		m_pActivator;
};

IMPLEMENT_REFLECT_DATAMAP( CPointHurt )

LINK_ENTITY_TO_CLASS( point_hurt, CPointHurt );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointHurt::Spawn_Impl(void)
{
	SetThink( nullptr );
	SetUse( nullptr );
		
	m_pActivator = nullptr;

	if ( HasSpawnFlags( SF_PHURT_START_ON ) )
	{
		SetThink( &CPointHurt::HurtThink );
	}

	SetNextThink( gpGlobals->curtime + 0.1f );
	
	if ( m_flRadius <= 0.0f )
	{
		m_flRadius = 128.0f;
	}

	if ( m_nDamage <= 0 )
	{
		m_nDamage = 2;
	}

	if ( m_flDelay <= 0 )
	{
		m_flDelay = 0.1f;
	}

	Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointHurt::Precache_Impl( void )
{
	BaseClass::Precache_Impl();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointHurt::HurtThink( void )
{
	if ( m_strTarget != NULL_STRING )
	{
		CBaseEntity	*pEnt = nullptr;
			
		CTakeDamageInfo info( this, m_pActivator, m_nDamage, m_bitsDamageType );
		while ( ( pEnt = gEntList.FindEntityByName( pEnt, m_strTarget, nullptr, m_pActivator ) ) != nullptr )
		{
			GuessDamageForce( &info, (pEnt->GetAbsOrigin() - GetAbsOrigin()), pEnt->GetAbsOrigin() );
			pEnt->TakeDamage( info );
		}
	}
	else
	{
		RadiusDamage( CTakeDamageInfo( this, this, m_nDamage, m_bitsDamageType ), GetAbsOrigin(), m_flRadius, CLASS_NONE, nullptr );
	}

	SetNextThink( gpGlobals->curtime + m_flDelay );
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for turning on the point hurt.
//-----------------------------------------------------------------------------
void CPointHurt::InputTurnOn( inputdata_t &data )
{
	SetThink( &CPointHurt::HurtThink );

	SetNextThink( gpGlobals->curtime + 0.1f );

	m_pActivator = data.pActivator;
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for turning off the point hurt.
//-----------------------------------------------------------------------------
void CPointHurt::InputTurnOff( inputdata_t &data )
{
	SetThink( nullptr );

	m_pActivator = data.pActivator;
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for toggling the on/off state of the point hurt.
//-----------------------------------------------------------------------------
void CPointHurt::InputToggle( inputdata_t &data )
{
	m_pActivator = data.pActivator;

	if ( m_pfnThink == (void (CBaseEntity::*)())&CPointHurt::HurtThink )
	{
		SetThink( nullptr );
	}
	else
	{
		SetThink( &CPointHurt::HurtThink );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for instantaneously hurting whatever is near us.
//-----------------------------------------------------------------------------
void CPointHurt::InputHurt( inputdata_t &data )
{
	m_pActivator = data.pActivator;

	HurtThink();
}

class CBaseCombatCharacter;
class CBaseEntity;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Teleports a named entity to a given position and restores
//			it's physics state

static constexpr auto SF_TELEPORT_INTO_DUCK = 0x00000002; ///< episodic only: player should be ducked after this teleport

class CPointTeleport : public CBaseEntity
{
	DECLARE_CLASS( CPointTeleport, CBaseEntity );
public:
	void	Activate_Impl( void );

	[[= ks::reflect::Input{ .name = "Teleport", .type = FIELD_VOID } ]] void InputTeleport( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "TeleportEntity", .type = FIELD_STRING } ]] void InputTeleportEntity( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "TeleportToCurrentPos", .type = FIELD_VOID } ]] void InputTeleportToCurrentPos( inputdata_t &inputdata );

	int	ObjectCaps_Impl( void )
	{ 
		return (BaseClass::ObjectCaps_Impl() & ~FCAP_ACROSS_TRANSITION); 
	}

private:
	
	void	DoTeleport( inputdata_t &inputdata, const Vector &vecOrigin, const QAngle &angRotation, bool bOverrideTarget = false );
	bool	EntityMayTeleport( CBaseEntity *pTarget );

	Vector m_vSaveOrigin;
	QAngle m_vSaveAngles;

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( point_teleport, CPointTeleport );

IMPLEMENT_REFLECT_DATAMAP( CPointTeleport )

//-----------------------------------------------------------------------------
// Returns true if the entity may be teleported
//-----------------------------------------------------------------------------
bool CPointTeleport::EntityMayTeleport( CBaseEntity *pTarget )
{
	if ( pTarget->GetMoveParent() != nullptr )
	{
		// Passengers in a vehicle are allowed to teleport; their behavior handles it
		CBaseCombatCharacter *pBCC = pTarget->MyCombatCharacterPointer();
		if ( pBCC == nullptr || ( pBCC != nullptr && true ) )
			return false;
	}

	return true;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void CPointTeleport::Activate_Impl( void )
{
	// Start with our origin point
	m_vSaveOrigin = GetAbsOrigin();
	m_vSaveAngles = GetAbsAngles();

	// Save off the spawn position of the target if instructed to do so
	if ( m_spawnflags & SF_TELEPORT_TO_SPAWN_POS )
	{
		CBaseEntity *pTarget = gEntList.FindEntityByName( nullptr, m_target );
		if ( pTarget )
		{
			// If teleport object is in a movement hierarchy, remove it first
			if ( EntityMayTeleport( pTarget ) )
			{
				// Save the points
				m_vSaveOrigin = pTarget->GetAbsOrigin();
				m_vSaveAngles = pTarget->GetAbsAngles();
			}
			else
			{
				Warning("ERROR: (%s) can't teleport object (%s) as it has a parent (%s)!\n",GetDebugName(),pTarget->GetDebugName(),pTarget->GetMoveParent()->GetDebugName());
				BaseClass::Activate_Impl();
				return;
			}
		}
		else
		{
			Warning("ERROR: (%s) target '%s' not found. Deleting.\n", GetDebugName(), STRING(m_target));
			UTIL_Remove( this );
			return;
		}
	}

	BaseClass::Activate_Impl();
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void CPointTeleport::InputTeleport( inputdata_t &inputdata )
{
	DoTeleport( inputdata, m_vSaveOrigin, m_vSaveAngles );
}

//------------------------------------------------------------------------------
// Purpose: Teleport the specified entity instead of the Teleporter's pre
//			determined entity.
//------------------------------------------------------------------------------
void CPointTeleport::InputTeleportEntity( inputdata_t &inputdata )
{
	DoTeleport( inputdata, m_vSaveOrigin, m_vSaveAngles, true );
}

//------------------------------------------------------------------------------
// Teleport the target to wherever the point_teleport entity is currently. The Teleport
// input teleports to the initial position of the point_teleport, so this input
// was added to avoid breaking old content.
//------------------------------------------------------------------------------
void CPointTeleport::InputTeleportToCurrentPos( inputdata_t &inputdata )
{
	if ( m_spawnflags & SF_TELEPORT_TO_SPAWN_POS )
	{
		// This is a nonsensical spawnflag in combination with this input.
		Warning( "%s: TeleportToCurrentPos input received; ignoring 'Teleport Home' spawnflag.\n", GetDebugName() );
	}

	DoTeleport( inputdata, GetAbsOrigin(), GetAbsAngles() );
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void CPointTeleport::DoTeleport( inputdata_t &inputdata, const Vector &vecOrigin, const QAngle &angRotation, bool bOverrideTarget )
{
	// Attempt to find the entity in question
	CBaseEntity *pTarget;
	if( bOverrideTarget )
	{
		// Use the inputdata to find the entity that the designer supplied in the parameter override 
		pTarget = gEntList.FindEntityByName( nullptr, inputdata.value.String(), this, inputdata.pActivator, inputdata.pCaller );
	}
	else
	{
		// Default behavior: Just find the entity that I am hardwired in Hammer to teleport.
		pTarget = gEntList.FindEntityByName( nullptr, m_target, this, inputdata.pActivator, inputdata.pCaller );
	}

	if ( pTarget == nullptr )
		return;

	// If teleport object is in a movement hierarchy, remove it first
	if ( EntityMayTeleport( pTarget ) == false )
	{
		Warning("ERROR: (%s) can't teleport object (%s) as it has a parent (%s)!\n",GetDebugName(),pTarget->GetDebugName(),pTarget->GetMoveParent()->GetDebugName());
		return;
	}

	// in episodic, we have a special spawn flag that forces Gordon into a duck

	pTarget->Teleport( &vecOrigin, &angRotation, nullptr );
}

class CBasePlayer;
class CPostProcessController;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: An entity that allows level designer control over the fog parameters.

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

void CPostProcessController::Spawn_Impl( void )
{
	BaseClass::Spawn_Impl();

	m_bMaster = IsMaster();
}

//-----------------------------------------------------------------------------
// Activate!
//-----------------------------------------------------------------------------
void CPostProcessController::Activate_Impl( ) 
{
	BaseClass::Activate_Impl();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CPostProcessController::UpdateTransmitState_Impl()
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

class CBaseEntity;

using namespace ::ks::reflect;

static const char *s_pPrecacheResourceTypeName[] =
{
	"vgui_resource",
	"material",		
	"model",			
	"scriptsound",		// NOTE: This is named this way for backward compat in reading from kv files	
	"particle_system",	
	"entity",
	"decal",
	"pmaterial",
	"dependency_file",
	"game_material_decals",
	"physics_gamesounds",
	"shared",
};

//-----------------------------------------------------------------------------
// Provides callback to do actual precaching of resources
//-----------------------------------------------------------------------------
class CPrecacheHandler
{
public:
	void CacheResource( PrecacheResourceType_t nType, const char *pName, bool bPrecache, ResourceList_t hResourceList, int *pIndex = nullptr );

private:
	void CacheResourceFile( const char *pFilename, bool bPrecache, ResourceList_t hResourceList );
	void PrecacheGameMaterialDecals( bool bPrecache, ResourceList_t hResourceList );
	void PrecachePhysicsSounds( const char *pName, bool bPrecache, ResourceList_t hResourceList );
};

//-----------------------------------------------------------------------------
// Singletons
//-----------------------------------------------------------------------------
static CPrecacheRegister	s_PrecacheRegister;
CPrecacheRegister *g_pPrecacheRegister = &s_PrecacheRegister;

static CPrecacheHandler		s_PrecacheHandler;
static auto s_CacheResource = []( PrecacheResourceType_t nType, const char *pName, bool bPrecache, ResourceList_t hResourceList, int *pIndex )
{
	s_PrecacheHandler.CacheResource( nType, pName, bPrecache, hResourceList, pIndex );
};
PrecacheHandler_t g_PrecacheHandler( s_CacheResource );

bool CPrecacheRegister::Init()
{ 
	return true; 
}

//-----------------------------------------------------------------------------
// Level startup, shutdown
//-----------------------------------------------------------------------------
void CPrecacheRegister::LevelInitPreEntity()
{
	COM_TimestampedLog( "LevelInitPreEntity - PreCache - Start" );
	g_pPrecacheSystem->Cache( g_PrecacheHandler, GLOBAL, nullptr, true, RESOURCE_LIST_INVALID, false );
	COM_TimestampedLog( "LevelInitPreEntity - PreCache - Finish" );

}

void CPrecacheRegister::LevelShutdownPostEntity()
{
	// FIXME: How to uncache all resources cached during the course of the level?
	g_pPrecacheSystem->UncacheAll( g_PrecacheHandler );

	if ( g_pResourceAccessControl )
	{
		g_pResourceAccessControl->DestroyAllResourceLists();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Precache game-specific models & sounds
//-----------------------------------------------------------------------------
void CPrecacheHandler::CacheResourceFile( const char *pFilename, bool bPrecache, ResourceList_t hResourceList )
{
	static_assert( std::size( s_pPrecacheResourceTypeName ) == PRECACHE_RESOURCE_TYPE_COUNT );

	KeyValues *pValues = new KeyValues( "ResourceFile" );

	if ( !pValues->LoadFromFile( g_pFullFileSystem, pFilename, "GAME" ) )
	{
		Warning( "Can't open %s for client precache info.", pFilename );
		pValues->deleteThis();
		return;
	}

	for ( KeyValues *pData = pValues->GetFirstSubKey(); pData != nullptr; pData = pData->GetNextKey() )
	{
		const char *pszType = pData->GetName();
		const char *pszFile = pData->GetString();

		if ( V_strlen( pszType ) == 0 || V_strlen( pszFile ) == 0 )
			continue;

		bool bFoundMatch = false;
		for ( int i = 0; i < PRECACHE_RESOURCE_TYPE_COUNT; ++i )
		{
			if ( !V_stricmp( pData->GetName(), s_pPrecacheResourceTypeName[i] ) )
			{
				CacheResource( (PrecacheResourceType_t)i, pszFile, bPrecache, hResourceList );
				bFoundMatch = true;
				break;
			}
		}

		if ( !bFoundMatch )
		{
			Warning( "Error in precache file \"%s\":\n", pFilename );
			Warning( "\tUnknown resource type specified \"%s\", value \"%s\"\n", pszType, pszFile );
		}
	}

	pValues->deleteThis();
}

//-----------------------------------------------------------------------------
// Precaches game material decals
//-----------------------------------------------------------------------------
void CPrecacheHandler::PrecacheGameMaterialDecals( bool bPrecache, ResourceList_t hResourceList )
{
}

void CPrecacheHandler::PrecachePhysicsSounds( const char *pName, bool bPrecache, ResourceList_t hResourceList )
{
	if ( !bPrecache )
		return;

	// precache the surface prop sounds
	bool bBulletSounds = !V_stricmp( pName, "BulletSounds" );
	bool bStepSounds = !V_stricmp( pName, "StepSounds" );
	bool bPhysicsImpactSounds = !V_stricmp( pName, "PhysicsImpactSounds" );

	for ( int i = 0; i < physprops->SurfacePropCount(); i++ )
	{
		surfacedata_t *pprop = physprops->GetSurfaceData( i );
		contract_assert( pprop );

		if ( bBulletSounds )
		{
			const char *pSoundName = physprops->GetString( pprop->sounds.bulletImpact );
			CacheResource( GAMESOUND, pSoundName, bPrecache, hResourceList, nullptr );
		}

		if ( bStepSounds )
		{
			const char *pSoundName = physprops->GetString( pprop->sounds.walkStepLeft );
			CacheResource( GAMESOUND, pSoundName, bPrecache, hResourceList, nullptr );
			pSoundName = physprops->GetString( pprop->sounds.walkStepRight );
			CacheResource( GAMESOUND, pSoundName, bPrecache, hResourceList, nullptr );
			pSoundName = physprops->GetString( pprop->sounds.runStepLeft );
			CacheResource( GAMESOUND, pSoundName, bPrecache, hResourceList, nullptr );
			pSoundName = physprops->GetString( pprop->sounds.runStepRight );
			CacheResource( GAMESOUND, pSoundName, bPrecache, hResourceList, nullptr );
		}

		if ( bPhysicsImpactSounds )
		{
			const char *pSoundName = physprops->GetString( pprop->sounds.impactSoft );
			CacheResource( GAMESOUND, pSoundName, bPrecache, hResourceList, nullptr );
			pSoundName = physprops->GetString( pprop->sounds.impactHard );
			CacheResource( GAMESOUND, pSoundName, bPrecache, hResourceList, nullptr );
			pSoundName = physprops->GetString( pprop->sounds.scrapeSmooth );
			CacheResource( GAMESOUND, pSoundName, bPrecache, hResourceList, nullptr );
			pSoundName = physprops->GetString( pprop->sounds.scrapeRough );
			CacheResource( GAMESOUND, pSoundName, bPrecache, hResourceList, nullptr );
			pSoundName = physprops->GetString( pprop->sounds.rolling );
			CacheResource( GAMESOUND, pSoundName, bPrecache, hResourceList, nullptr );
			pSoundName = physprops->GetString( pprop->sounds.breakSound );
			CacheResource( GAMESOUND, pSoundName, bPrecache, hResourceList, nullptr );
			pSoundName = physprops->GetString( pprop->sounds.strainSound );
			CacheResource( GAMESOUND, pSoundName, bPrecache, hResourceList, nullptr );
		}
	}
}

//-----------------------------------------------------------------------------
// Caches/uncaches resources
//-----------------------------------------------------------------------------
void CPrecacheHandler::CacheResource( PrecacheResourceType_t nType, const char *pName, 
	bool bPrecache, ResourceList_t hResourceList, int *pIndex )
{
	if ( bPrecache )
	{
		if ( pIndex )
		{
			*pIndex = 0;
		}

		switch( nType )
		{
		case VGUI_RESOURCE:
			break;

		case MATERIAL:
			PrecacheMaterial( pName ); 
			if ( pIndex )
			{
				*pIndex = GetMaterialIndex( pName );
			}
			if ( hResourceList != RESOURCE_LIST_INVALID )
			{
				g_pResourceAccessControl->AddResource( hResourceList, RESOURCE_MATERIAL, pName );
			}
			break;

		case PARTICLE_MATERIAL:
			{
				void* nIndex = 0;
				if ( pIndex )
				{
					void** pIndexMaterial = (void**)pIndex;

					*pIndexMaterial = nIndex;
				}
			}
			break;

		case GAME_MATERIAL_DECALS:
			PrecacheGameMaterialDecals( bPrecache, hResourceList );
			break;

		case PHYSICS_GAMESOUNDS:
			PrecachePhysicsSounds( pName, bPrecache, hResourceList );
			break;

		case DECAL:
			{
				int nIndex = UTIL_PrecacheDecal( pName, true );
				if ( pIndex )
				{
					*pIndex = nIndex;
				}
			}
			break;

		case MODEL:
			{
				int nIndex = CBaseEntity::PrecacheModel( pName );
				if ( pIndex )
				{
					*pIndex = nIndex;
				}
				if ( hResourceList != RESOURCE_LIST_INVALID )
				{
					g_pResourceAccessControl->AddResource( hResourceList, RESOURCE_MODEL, pName );
				}
			}
			break;

		case GAMESOUND:
			{
				int nIndex = CBaseEntity::PrecacheScriptSound( pName );
				if ( pIndex )
				{
					*pIndex = nIndex;
				}
				if ( hResourceList != RESOURCE_LIST_INVALID )
				{
					g_pResourceAccessControl->AddResource( hResourceList, RESOURCE_GAMESOUND, pName );
				}
			}
			break;

		case PARTICLE_SYSTEM:
			PrecacheParticleSystem( pName );
			if ( pIndex )
			{
				*pIndex = GetParticleSystemIndex( pName );
			}
			if ( hResourceList != RESOURCE_LIST_INVALID )
			{
				g_pResourceAccessControl->AddResource( hResourceList, RESOURCE_PARTICLE_SYSTEM, pName );
			}
			break;

		case ENTITY:
			UTIL_PrecacheOther( pName );
			break;

		case SHARED:
			g_pPrecacheSystem->Cache( g_PrecacheHandler, SHARED_SYSTEM, pName, bPrecache, hResourceList, false );
			break;

		case KV_DEP_FILE:
			CacheResourceFile( pName, bPrecache, hResourceList );
			break;
		}
		return;
	}

	// Blat out value
	if ( pIndex )
	{
		*pIndex = 0;
	}

	switch( nType )
	{
	case VGUI_RESOURCE:
		break;
	case MATERIAL:
		break;
	case MODEL:
		break;
	case GAMESOUND:
		break;
	case PARTICLE_SYSTEM:
		break;
	case ENTITY:
		break;
	case DECAL:
		break;
	case KV_DEP_FILE:
		break;
	}
}

class CBasePlayer;
class CIronSightController;
class CWeaponCSBase;

using namespace ::ks::reflect;

IMPLEMENT_NETWORKCLASS_ALIASED( PredictedViewModel, DT_PredictedViewModel )
LINK_ENTITY_TO_CLASS_ALIASED( predicted_viewmodel, PredictedViewModel );

IMPLEMENT_REFLECT_TABLE( CPredictedViewModel, DT_PredictedViewModel );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPredictedViewModel::CPredictedViewModel()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPredictedViewModel::~CPredictedViewModel()
{
}

//-----------------------------------------------------------------------------
// Purpose:  Adds head bob for off hand models
//-----------------------------------------------------------------------------
void CPredictedViewModel::AddViewModelBob( CBasePlayer *owner, Vector& eyePosition, QAngle& eyeAngles )
{
}

void CPredictedViewModel::CalcViewModelLag( Vector& origin, QAngle& angles, QAngle& /*original_angles*/ )
{
}

void CPredictedViewModel::ApplyViewModelPitchAndDip( CBasePlayer *owner, Vector& vecNewOrigin, QAngle& vecNewAngles )
{
	//orients and moves weapon to provide visual feedback on weapon accuracy and player motion (like jumping and landing)
}

void CPredictedViewModel::CalcViewModelView( CBasePlayer *owner, const Vector& eyePosition, const QAngle& eyeAngles )
{

	CWeaponCSBase *pWeapon = ( CWeaponCSBase * )GetOwningWeapon();
	if ( pWeapon )
	{
		CIronSightController* pIronSightController = pWeapon->GetIronSightController();
		if ( pIronSightController )
		{
			pIronSightController->UpdateIronSightAmount();

		}
	}

}

class CBaseAnimating;
struct inputdata_t;

using namespace ::ks::reflect;

class CProp_Hallucination : public CBaseAnimating
{
	[[= ks::reflect::NetTable{ .name = "DT_Prop_Hallucination" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CProp_Hallucination, CBaseAnimating );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CProp_Hallucination( void );
	void Precache_Impl( void );
	void Spawn_Impl( void );
	int DrawDebugTextOverlays_Impl( void );
	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void InputDisable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetVisibleTime", .type = FIELD_FLOAT } ]] void InputSetVisibleTime( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetRechargeTime", .type = FIELD_FLOAT } ]] void InputSetRechargeTime( inputdata_t &inputdata );

	CNetworkVar( bool, m_bEnabled, [[= ks::reflect::Net{} ]] );
	[[= ks::reflect::Key{ .name = "EnabledChance" } ]] float m_fStartEnabledChance; //0.0 - 100.0% chance that this hallucination will start enabled
	CNetworkVar( float, m_fVisibleTime, [[= ks::reflect::Net{ .bits = 32 } ]] [[= ks::reflect::Key{ .name = "VisibleTime" } ]] ); //how long in seconds this hallucination can remain on screen from first sighting
	CNetworkVar( float, m_fRechargeTime, [[= ks::reflect::Net{ .bits = 32 } ]] [[= ks::reflect::Key{ .name = "RechargeTime" } ]] ); //how long in seconds it takes the hallucination to recharge before becoming visible again. 0 to disable
};

IMPLEMENT_REFLECT_DATAMAP( CProp_Hallucination )

IMPLEMENT_REFLECT_SERVERCLASS( CProp_Hallucination, DT_Prop_Hallucination )

LINK_ENTITY_TO_CLASS( prop_hallucination, CProp_Hallucination );

CProp_Hallucination::CProp_Hallucination( void )
{
}

void CProp_Hallucination::Precache_Impl( void )
{
	BaseClass::Precache_Impl();
	PrecacheModel( m_ModelName.ToCStr() );
}

void CProp_Hallucination::Spawn_Impl( void )
{
	Precache();
	BaseClass::Spawn_Impl();
	SetModel( m_ModelName.ToCStr() );

	if( m_fStartEnabledChance > 0.0f )
	{
		m_bEnabled = RandomFloat() <= (m_fStartEnabledChance * 0.01f);	
	}
}

void CProp_Hallucination::InputEnable( inputdata_t &inputdata )
{
	m_bEnabled = true;
}

void CProp_Hallucination::InputDisable( inputdata_t &inputdata )
{
	m_bEnabled = false;
}

void CProp_Hallucination::InputSetVisibleTime( inputdata_t &inputdata )
{
	m_fVisibleTime = inputdata.value.Float();
}

void CProp_Hallucination::InputSetRechargeTime( inputdata_t &inputdata )
{
	m_fRechargeTime = inputdata.value.Float();
}

//-----------------------------------------------------------------------------
// Draw debug overlays
//-----------------------------------------------------------------------------
int CProp_Hallucination::DrawDebugTextOverlays_Impl()
{
	int text_offset = BaseClass::DrawDebugTextOverlays_Impl();

	if (m_debugOverlays & OVERLAY_TEXT_BIT) 
	{
		char tempstr[255];

		V_snprintf( tempstr, sizeof(tempstr), "%s", m_bEnabled ? "Enabled" : "Disabled" );
		EntityText( text_offset++, tempstr, 0 );

		V_snprintf( tempstr, sizeof(tempstr), "Start Enabled Chance: %f%%", m_fStartEnabledChance );
		EntityText( text_offset++, tempstr, 0 );

		V_snprintf( tempstr, sizeof(tempstr), "Visible Time: %fs", m_fVisibleTime.Get() );
		EntityText( text_offset++, tempstr, 0 );

		V_snprintf( tempstr, sizeof(tempstr), "Recharge Time: %fs", m_fRechargeTime.Get() );
		EntityText( text_offset++, tempstr, 0 );
	}
	
	return text_offset;
}
