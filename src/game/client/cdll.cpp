// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

module;
#include <stddef.h>
#include <string.h>
#include <crtmemdebug.h>
#include <ctype.h>
#include <stdarg.h>
#include <wchar.h>
#include "IGameUIFuncs.h"
#include "cdll_int.h"
#include "client_class.h"
#include "datacache/iprecachesystem.h"
#include "eiface.h"
#include "engine/IEngineSound.h"
#include "engine/IEngineTrace.h"
#include "engine/IStaticPropMgr.h"
#include "engine/iblackbox.h"
#include "engine/ivdebugoverlay.h"
#include "game/client/IGameClientExports.h"
#include "game/client/iviewport.h"
#include "gametypes.h"
#include "icliententity.h"
#include "iefx.h"
#include "iengineui.h"
#include "inputsystem/iinputsystem.h"
#include "irendertorthelperobject.h"
#include "ivrenderview.h"
#include "matchmaking/imatchframework.h"
#include "materialsystem/imaterialproxy.h"
#include "networkstringtabledefs.h"
#include "particles/particles.h"
#include "scenefilecache/ISceneFileCache.h"
#include "shake.h"
#include "soundinfo.h"
#include "tier2/tier2_logging.h"
#include "videocfg/videocfg.h"
#include "view_shared.h"
#include "vphysics_interface.h"
#include "vscript/ivscript.h"
#include "baseentity.h"
#include "cdll_client_int.h"
#include "predictioncopy.h"
#include "client_textmessage.h"
#include "edict.h"
#include "engine/ivmodelinfo.h"
#include "localize/ilocalize.h"
#include "model_types.h"
#include "utils/common/scriplib.h"
#include "worldsize.h"
module ks.client;

import std;
import ks.core.basetypes;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.convar_serverbounded;
import ks.tier1.cvar;
import ks.tier1.iconvar;
import ks.tier1.icvar;
import ks.tier1.interfaces;
import ks.tier1.reflect_wirevar;
import :props;
import :util;
import :precache;
import :achievements_and_stats_interface;
import :baseanimatedtextureproxy;
import :baseentity;
import :sharedvar;
import ks.core.cmdline;
import ks.core.dbg;
import ks.core.platform;
import ks.core.random;
import ks.core.scratcharena;
import ks.core.threadtools;
import ks.core.types;
import ks.core.vprof;
import ks.game.entity_cast;
import ks.math.mathlib;
import ks.math.ssemath;
import ks.math.vector;
import ks.tier1.bitbuf;
import ks.tier1.functors;
import ks.tier1.iappsystem;
import ks.tier1.createinterface;
import ks.tier1.intern_pool;
import ks.tier1.interpolatedvar;
import ks.tier1.keyvalues;
import ks.tier1.map_ops;
import ks.tier1.registry;
import ks.tier1.strtools;
import ks.tier1.vector_ops;
import ks.vstdlib.jobthread;
import :particlemgr;
import :iinput;
import :cstrike15.RocketUI.rkconsole;
import :cstrike15.cs_player;
import :game_controls.baseviewport;
import ks.game.shareddefs;
import :triggers;
import :cstrike15.cs_player_shared;
import :cstrike15.cs_shareddefs;
import ks.game.gamemovement;
import ks.game.mapentities_shared;
import :noopanimcontroller;
import ks.core.wchartypes;
import ks.math.compressed_vector;
import ks.tier1.text_writer;
import ks.tier1.timeutils;
import ks.tier1.uniqueid;

using namespace ::ks::reflect;

bool g_bForceCLPredictOff = false;
bool g_bSpectatingForceCLPredictOff = false;
// ------------------------------------------------------------------------------------------ //
// cl_predict.
// ------------------------------------------------------------------------------------------ //

static float CBoundedCvar_Predict_GetFloat( const ConVar_ServerBounded &bounded )
{
	// Used temporarily for CS kill cam.
	if ( g_bForceCLPredictOff || g_bSpectatingForceCLPredictOff )
		return 0;

	static const ConVar *pClientPredict = g_pCVar->FindVar( "sv_client_predict" );
	if ( pClientPredict && pClientPredict->GetInt() != -1 )
	{
		// Ok, the server wants to control this value.
		return pClientPredict->GetFloat();
	}
	else
	{
		return bounded.GetBaseFloatValue();
	}
}

static ConVar_ServerBounded cl_predict_var( CBoundedCvar_Predict_GetFloat,
	"cl_predict", 
	"1.0", 
	FCVAR_USERINFO | FCVAR_NOT_CONNECTED, 
	"Perform client side prediction." );
ConVar_ServerBounded *cl_predict = &cl_predict_var;

// ------------------------------------------------------------------------------------------ //
// cl_interp_ratio.
// ------------------------------------------------------------------------------------------ //

static float CBoundedCvar_InterpRatio_GetFloat( const ConVar_ServerBounded &bounded )
{
	static const ConVar *pMin = g_pCVar->FindVar( "sv_client_min_interp_ratio" );
	static const ConVar *pMax = g_pCVar->FindVar( "sv_client_max_interp_ratio" );

	float flBaseFloatValue = bounded.GetBaseFloatValue();
	if ( engine->IsPlayingDemo() && engine->GetDemoPlaybackParameters() )
		flBaseFloatValue = 2.0f; // use the default value when playing demos

	if ( pMin && pMax && pMin->GetFloat() != -1 )
	{
		return clamp( flBaseFloatValue, pMin->GetFloat(), pMax->GetFloat() );
	}
	else
	{
		return flBaseFloatValue;
	}
}

static ConVar_ServerBounded cl_interp_ratio_var( CBoundedCvar_InterpRatio_GetFloat,
	"cl_interp_ratio", 
	"2.0", 
	FCVAR_USERINFO | FCVAR_NOT_CONNECTED, 
	"Sets the interpolation amount (final amount is cl_interp_ratio / cl_updaterate)." );
ConVar_ServerBounded *cl_interp_ratio = &cl_interp_ratio_var;

// ------------------------------------------------------------------------------------------ //
// cl_interp
// ------------------------------------------------------------------------------------------ //

static float CBoundedCvar_Interp_GetFloat( const ConVar_ServerBounded &bounded )
{
	static const ConVar *pUpdateRate = g_pCVar->FindVar( "cl_updaterate" );
	static const ConVar *pMin = g_pCVar->FindVar( "sv_client_min_interp_ratio" );
	
	float flBaseFloatValue = bounded.GetBaseFloatValue();
	if ( engine->IsPlayingDemo() && engine->GetDemoPlaybackParameters() && pUpdateRate )
		flBaseFloatValue = 2.0f / pUpdateRate->GetFloat(); // use a default value 2/updaterate when playing demos

	if ( pUpdateRate && pMin && pMin->GetFloat() != -1 )
	{
		return std::max( flBaseFloatValue, pMin->GetFloat() / pUpdateRate->GetFloat() );
	}
	else
	{
		return flBaseFloatValue;
	}
}

static ConVar_ServerBounded cl_interp_var( CBoundedCvar_Interp_GetFloat,
	"cl_interp", 
	"0.03125",		// 2 / 102.4
	FCVAR_USERINFO | FCVAR_NOT_CONNECTED, 
	"Sets the interpolation amount (bounded on low side by server interp ratio settings).", true, 0.0f, true, 0.5f );
ConVar_ServerBounded *cl_interp = &cl_interp_var;

float GetClientInterpAmount()
{
	static const ConVar *pUpdateRate = g_pCVar->FindVar( "cl_updaterate" );
	if ( pUpdateRate )
	{
		// #define FIXME_INTERP_RATIO
		float interp = cl_interp_ratio->GetFloat() / pUpdateRate->GetFloat();
		return interp;
	}
	else
	{
		contract_assert( false && "GetInterpolationAmount: can't get cl_updaterate cvar." );	
		return 0.1f;
	}
}

class AchievementsAndStatsInterface;
class CEngineSprite;
class CBaseAnimating;
class CBaseEntity;
class CBaseFlex;
class CBasePlayer;
class CCSPlayer;
class IClientMode;
class IGameSystem;
struct kbutton_t;

using namespace ::ks::reflect;

extern IClientMode *GetClientModeNormal();

// IF YOU ADD AN INTERFACE, EXTERN IT IN THE HEADER FILE.
IVEngineClient	*engine = nullptr;
IVModelRender *modelrender = nullptr;
IVEfx *effects = nullptr;
IVRenderView *render = nullptr;
IVDebugOverlay *debugoverlay = nullptr;
IDataCache *datacache = nullptr;
IVModelInfoClient *modelinfo = nullptr;
IEngineUI *engineui = nullptr;
INetworkStringTableContainer *networkstringtable = nullptr;
ISpatialPartition* partition = nullptr;
IFileSystem *filesystem = nullptr;
IShadowMgr *shadowmgr = nullptr;
IStaticPropMgrClient *staticpropmgr = nullptr;
IEngineSound *enginesound = nullptr;
CUniformRandomStream *random_valve = nullptr;
static CGaussianRandomStream s_GaussianRandomStream;
CGaussianRandomStream *randomgaussian = &s_GaussianRandomStream;
ISharedGameRules *sharedgamerules = nullptr;
IEngineTrace *enginetrace = nullptr;
CFileLoggingListener *filelogginglistener = nullptr;
IGameUIFuncs *gameuifuncs = nullptr;
IGameEventManager2 *gameeventmanager = nullptr;
ISoundEmitterSystemBase *soundemitterbase = nullptr;
IInputSystem *inputsystem = nullptr;
ISceneFileCache *scenefilecache = nullptr;
IRenderToRTHelper *g_pRenderToRTHelper = nullptr;

IUploadGameStats *gamestatsuploader = nullptr;
IBlackBox *blackboxrecorder = nullptr;

AchievementsAndStatsInterface* g_pAchievementsAndStatsInterface = nullptr;

IScriptManager *scriptmanager = nullptr;

IGameSystem *SoundEmitterSystem();
IViewRender *GetViewRenderInstance();

bool g_bEngineIsHLTV = false;

static bool g_bRequestCacheUsedMaterials = false;
void RequestCacheUsedMaterials()
{
	g_bRequestCacheUsedMaterials = true;
}

void ProcessCacheUsedMaterials()
{
	if ( !g_bRequestCacheUsedMaterials )
		return;

	g_bRequestCacheUsedMaterials = false;
	if ( materials )
	{
        materials->CacheUsedMaterials();
	}
}

static bool g_bHeadTrackingEnabled = false;

bool IsHeadTrackingEnabled()
{
	return false;
}

// String tables
INetworkStringTable *g_pStringTableParticleEffectNames = nullptr;
INetworkStringTable *g_pStringTableExtraParticleFiles = nullptr;
INetworkStringTable *g_StringTableEffectDispatch = nullptr;
INetworkStringTable *g_pStringTableMaterials = nullptr;
INetworkStringTable *g_pStringTableInfoPanel = nullptr;
INetworkStringTable *g_pStringTableClientSideChoreoScenes = nullptr;
INetworkStringTable *g_pStringTableMovies = nullptr;

static CGlobalVarsBase dummyvars( true );
// So stuff that might reference gpGlobals during DLL initialization won't have a NULL pointer.
// Once the engine calls Init on this DLL, this pointer gets assigned to the shared data in the engine
CGlobalVarsBase *gpGlobals = &dummyvars;

static CBaseEntityClassList *s_pClassLists = nullptr;
CBaseEntityClassList::CBaseEntityClassList()
{
	m_pNextClassList = s_pClassLists;
	s_pClassLists = this;
}
CBaseEntityClassList::~CBaseEntityClassList()
{
}

// Any entities that want an OnDataChanged during simulation register for it here.
class CDataChangedEvent
{
public:
	CDataChangedEvent() {}
	CDataChangedEvent( IClientNetworkable *ent, DataUpdateType_t updateType, int *pStoredEvent )
	{
		if ( ent != nullptr )
		{
			m_nEntityIndex = ent->entindex();
		}
		else
		{
			m_nEntityIndex = -1;
		}

		m_UpdateType = updateType;
		m_pStoredEvent = pStoredEvent;
	}

	IClientNetworkable *GetEntity()
	{
		if ( m_nEntityIndex == -1 )
			return nullptr;

		return ClientEntityList().GetClientNetworkable( m_nEntityIndex );
	}

	int                  m_nEntityIndex;
	DataUpdateType_t	m_UpdateType;
	int					*m_pStoredEvent;
};

// Stored event ids are indices; cleared events become tombstones so ids stay valid until the list is flushed.
std::vector<CDataChangedEvent> g_DataChangedEvents;
std::vector<CDataChangedEvent> g_DataChangedPostEvents;

static int CountPendingDataChangedEvents()
{
	return (int)std::ranges::count_if( g_DataChangedEvents, []( const CDataChangedEvent &e ) { return e.m_pStoredEvent != nullptr; } );
}
bool g_bDataChangedPostEventsAllowed = false;

static ConVar s_CV_ShowParticleCounts(side_registry(), "showparticlecounts", "0", 0, "Display number of particles drawn per frame");
// static ConVar s_cl_team("cl_team", "default", FCVAR_ARCHIVE, "Default team when joining a game");
// static ConVar s_cl_class("cl_class", "default", FCVAR_ARCHIVE, "Default class when joining a game");

// Physics system
bool g_bLevelInitialized;
bool g_bTextMode = false;
ClientFrameStage_t g_CurFrameStage = FRAME_UNDEFINED;

static ConVar *g_pcv_ThreadMode = nullptr;

// implements ACTIVE_SPLITSCREEN_PLAYER_GUARD (cdll_client_int.h)
CSetActiveSplitScreenPlayerGuard::CSetActiveSplitScreenPlayerGuard( char const *pchContext, int nLine ) :
	CUIScreenSizeSplitScreenPlayerGuard( false, engine->GetActiveSplitScreenPlayerSlot(), engine->GetActiveSplitScreenPlayerSlot() )
{
	m_bChanged = true;
	m_pchContext = pchContext;
	m_nLine = nLine;
	m_nSaveSlot = engine->SetActiveSplitScreenPlayerSlot( -1 );
	m_bSaveGetLocalPlayerAllowed = engine->SetLocalPlayerIsResolvable( pchContext, nLine, false );
}

CSetActiveSplitScreenPlayerGuard::CSetActiveSplitScreenPlayerGuard( char const *pchContext, int nLine, int slot, int nOldSlot, bool bSetUIScreenSize ) :
	CUIScreenSizeSplitScreenPlayerGuard( bSetUIScreenSize, slot, nOldSlot )
{
	if ( nOldSlot == slot && engine->IsLocalPlayerResolvable() )
	{
		m_bChanged = false;
		return;
	}

	m_bChanged = true;
	m_pchContext = pchContext;
	m_nLine = nLine;
	m_nSaveSlot = engine->SetActiveSplitScreenPlayerSlot( slot >= 0 ? slot : 0 );
	m_bSaveGetLocalPlayerAllowed = engine->SetLocalPlayerIsResolvable( pchContext, nLine, slot >= 0 );
}

CSetActiveSplitScreenPlayerGuard::CSetActiveSplitScreenPlayerGuard( char const *pchContext, int nLine, CBaseEntity *pEntity, int nOldSlot, bool bSetUIScreenSize ) :
	CUIScreenSizeSplitScreenPlayerGuard( bSetUIScreenSize, pEntity, nOldSlot )
{
	int slot = CBasePlayer::GetSplitScreenSlotForPlayer( pEntity );
	if ( slot == -1 )
	{
		m_bChanged = false;
		return;
	}

	if ( nOldSlot == slot && engine->IsLocalPlayerResolvable())
	{
		m_bChanged = false;
		return;
	}

	m_bChanged = true;
	m_pchContext = pchContext;
	m_nLine = nLine;
	m_nSaveSlot = engine->SetActiveSplitScreenPlayerSlot( slot >= 0 ? slot : 0 );
	m_bSaveGetLocalPlayerAllowed = engine->SetLocalPlayerIsResolvable( pchContext, nLine, slot >= 0 );
}

CSetActiveSplitScreenPlayerGuard::~CSetActiveSplitScreenPlayerGuard()
{
	if ( !m_bChanged )
		return;

	engine->SetActiveSplitScreenPlayerSlot( m_nSaveSlot );
	engine->SetLocalPlayerIsResolvable( m_pchContext, m_nLine, m_bSaveGetLocalPlayerAllowed );
}

static std::set< const char *, ks::cstr_less > g_Hacks;

static void cl_dumpsplithacks( const CCommand &args );
static ConCommand cl_dumpsplithacks_command( side_registry(), "cl_dumpsplithacks", cl_dumpsplithacks, "Dump split screen workarounds." );
static void cl_dumpsplithacks( const CCommand &args )
{
	for ( const char *pszHack : g_Hacks )
	{
		Msg( "%s\n", pszHack );
	}
}

CHackForGetLocalPlayerAccessAllowedGuard::CHackForGetLocalPlayerAccessAllowedGuard( char const *pszContext, bool bOldState )
{
	if ( bOldState )
	{
		m_bChanged = false;
		return;
	}

	m_bChanged = true;
	m_pszContext = pszContext;
	g_Hacks.insert( pszContext );
	m_bSaveGetLocalPlayerAllowed = engine->SetLocalPlayerIsResolvable( pszContext, 0, true );
}

CHackForGetLocalPlayerAccessAllowedGuard::~CHackForGetLocalPlayerAccessAllowedGuard()
{
	if ( !m_bChanged )
		return;
	engine->SetLocalPlayerIsResolvable( m_pszContext, 0, m_bSaveGetLocalPlayerAllowed );
}

CUIScreenSizeSplitScreenPlayerGuard::CUIScreenSizeSplitScreenPlayerGuard( bool bActive, int slot, int nOldSlot )
{
	m_bNoRestore = true;
}

CUIScreenSizeSplitScreenPlayerGuard::CUIScreenSizeSplitScreenPlayerGuard( bool bActive, CBaseEntity *pEntity, int nOldSlot )
{
	m_bNoRestore = true;
}

CUIScreenSizeSplitScreenPlayerGuard::~CUIScreenSizeSplitScreenPlayerGuard()
{
}

CUIAbsPosSplitScreenPlayerGuard::CUIAbsPosSplitScreenPlayerGuard( int slot, int nOldSlot, bool bInvert /*=false*/ )
{
	m_bNoRestore = true;
}

CUIAbsPosSplitScreenPlayerGuard::~CUIAbsPosSplitScreenPlayerGuard()
{
}

//-----------------------------------------------------------------------------
// Purpose: interface for gameui to modify voice bans
//-----------------------------------------------------------------------------
class CGameClientExports : public IGameClientExports
{
public:
	// ingame voice manipulation
	bool IsPlayerGameVoiceMuted(int playerIndex)
	{
		return GetClientVoiceMgr()->IsPlayerBlocked(playerIndex);
	}

	void MutePlayerGameVoice(int playerIndex)
	{
		if ( !GetClientVoiceMgr()->IsPlayerBlocked( playerIndex ) )
		{
			GetClientVoiceMgr()->SetPlayerBlockedState( playerIndex );
		}
	}

	void UnmutePlayerGameVoice(int playerIndex)
	{
		if ( GetClientVoiceMgr()->IsPlayerBlocked( playerIndex ) )
		{
			GetClientVoiceMgr()->SetPlayerBlockedState( playerIndex );
		}
	}
	
	void OnGameUIActivated( void )
	{
		IGameEvent *event = gameeventmanager->CreateEvent( "gameui_activated" );
		if ( event )
		{
			gameeventmanager->FireEventClientSide( event );
		}
	}

	void OnGameUIHidden( void )
	{
		IGameEvent *event = gameeventmanager->CreateEvent( "gameui_hidden" );
		if ( event )
		{
			gameeventmanager->FireEventClientSide( event );
		}
	}

	// if true, the gameui applies the blur effect
	bool ClientWantsBlurEffect( void )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( 0 );
		if ( GetViewPortInterface()->GetActivePanel() && GetViewPortInterface()->GetActivePanel()->WantsBackgroundBlurred() )
			return true;

		return false;
	}
	
    void DisplayAchievementPanel()
    {
        if (g_pAchievementsAndStatsInterface)
        {
            g_pAchievementsAndStatsInterface->DisplayPanel();
        }
    }

    void ShutdownAchievementPanel()
    {
        if (g_pAchievementsAndStatsInterface)
        {
            g_pAchievementsAndStatsInterface->ReleasePanel();
        }
    }

	int GetAchievementsPanelMinWidth( void ) const
	{
        if ( g_pAchievementsAndStatsInterface )
        {
            return g_pAchievementsAndStatsInterface->GetAchievementsPanelMinWidth();
        }

		return 0;
	}
};

static CGameClientExports s_Singleton_CGameClientExports;
static InterfaceReg s_Reg_CGameClientExports_IGameClientExports( side_registry(), []() -> void * { return static_cast<IGameClientExports *>( &s_Singleton_CGameClientExports ); }, GAMECLIENTEXPORTS_INTERFACE_VERSION );;

class CClientDLLSharedAppSystems : public IClientDLLSharedAppSystems
{
public:
	CClientDLLSharedAppSystems()
	{
		AddAppSystem( "engine" , SOUNDEMITTERSYSTEM_INTERFACE_VERSION );
		AddAppSystem( "engine", SCENE_FILE_CACHE_INTERFACE_VERSION );
		
	}

	virtual int	Count()
	{
		return std::ssize( m_Systems );
	}
	virtual char const *GetDllName( int idx )
	{
		return m_Systems[ idx ].m_pModuleName;
	}
	virtual char const *GetInterfaceName( int idx )
	{
		return m_Systems[ idx ].m_pInterfaceName;
	}
private:
	void AddAppSystem( char const *moduleName, char const *interfaceName )
	{
		AppSystemInfo_t sys;
		sys.m_pModuleName = moduleName;
		sys.m_pInterfaceName = interfaceName;
		m_Systems.push_back( sys );
	}

	std::vector< AppSystemInfo_t >	m_Systems;
};

static CClientDLLSharedAppSystems s_Singleton_CClientDLLSharedAppSystems;
static InterfaceReg s_Reg_CClientDLLSharedAppSystems_IClientDLLSharedAppSystems( side_registry(), []() -> void * { return static_cast<IClientDLLSharedAppSystems *>( &s_Singleton_CClientDLLSharedAppSystems ); }, CLIENT_DLL_SHARED_APPSYSTEMS );;

//-----------------------------------------------------------------------------
// Helper interface for voice.
//-----------------------------------------------------------------------------
class CHLVoiceStatusHelper : public IVoiceStatusHelper
{
public:
	virtual void GetPlayerTextColor(int entindex, int color[3])
	{
		color[0] = color[1] = color[2] = 128;
	}

	virtual void UpdateCursorState()
	{
	}

	virtual bool			CanShowSpeakerLabels()
	{
		return true;
	}
};
static CHLVoiceStatusHelper g_VoiceStatusHelper;

//-----------------------------------------------------------------------------
// Code to display which entities are having their bones setup each frame.
//-----------------------------------------------------------------------------

ConVar cl_extrapolate_amount( side_registry(), "cl_extrapolate_amount", "0.25", FCVAR_CHEAT, "Set how many seconds the client will extrapolate entities for." );
ConVar cl_ShowBoneSetupEnts( side_registry(), "cl_ShowBoneSetupEnts", "0", 0, "Show which entities are having their bones setup each frame." );

class CBoneSetupEnt
{
public:
	char m_ModelName[128];
	int m_Index;
	int m_Count;
};

std::map< int, CBoneSetupEnt > g_BoneSetupEnts;	// keyed by m_Index

void TrackBoneSetupEnt( CBaseAnimating *pEnt )
{
}

void DisplayBoneSetupEnts()
{
}

//-----------------------------------------------------------------------------
// Purpose: engine to client .dll interface
//-----------------------------------------------------------------------------
class CHLClient : public IBaseClientDLL
{
public:
	CHLClient();

	virtual int						Connect( CreateInterfaceFn appSystemFactory, CGlobalVarsBase *pGlobals );
	virtual void                    Disconnect();
	virtual int						Init( CreateInterfaceFn appSystemFactory, CGlobalVarsBase *pGlobals );

	virtual void					PostInit();
	virtual void					Shutdown( void );

	virtual void					LevelInitPreEntity( const char *pMapName );
	virtual void					LevelInitPostEntity();
	virtual void					LevelShutdown( void );

	virtual ClientClass				*GetAllClasses( void );

	virtual int						HudVidInit( void );
	virtual void					HudProcessInput( bool bActive );
	virtual void					HudUpdate( bool bActive );
	virtual void					HudReset( void );
	virtual void					HudText( const char * message );

	// Mouse Input Interfaces
	virtual void					IN_ActivateMouse( void );
	virtual void					IN_DeactivateMouse( void );
	virtual void					IN_Accumulate( void );
	virtual void					IN_ClearStates( void );
	virtual bool					IN_IsKeyDown( const char *name, bool& isdown );
	// Raw signal
	virtual int						IN_KeyEvent( int eventcode, ButtonCode_t keynum, const char *pszCurrentBinding );
	virtual void					IN_SetSampleTime( float frametime );
	// Create movement command
	virtual void					CreateMove ( int sequence_number, float input_sample_frametime, bool active );
	virtual void					ExtraMouseSample( float frametime, bool active );
	virtual bool					WriteUsercmdDeltaToBuffer( int nSlot, bf_write *buf, int from, int to, bool isnewcommand );	
	virtual void					EncodeUserCmdToBuffer( int nSlot, bf_write& buf, int slot );
	virtual void					DecodeUserCmdFromBuffer( int nSlot, bf_read& buf, int slot );

	virtual void					View_Render( vrect_t *rect );
	virtual void					RenderView( const CViewSetup &view, int nClearFlags, int whatToDraw );
	virtual void					View_Fade( ScreenFade_t *pSF );
	
	virtual void					SetCrosshairAngle( const QAngle& angle );

	virtual void					InitSprite( void *pSprite, const char *loadname );
	virtual void					ShutdownSprite( void *pSprite );

	virtual int						GetSpriteSize( void ) const;

	virtual void					VoiceStatus( int entindex, int iSsSlot, qboolean bTalking );
	
	virtual bool					PlayerAudible( int iPlayerIndex );

	virtual void					InstallStringTableCallback( const char *tableName );

	virtual void					FrameStageNotify( ClientFrameStage_t curStage );

	virtual bool					DispatchUserMessage( int msg_type, int32 nFlags, int size, const void *msg );

	virtual void			DispatchOnRestore();
	virtual void			WriteSaveGameScreenshot( const char *pFilename );

	// Given a list of "S(wavname) S(wavname2)" tokens, look up the localized text and emit
	//  the appropriate close caption if running with closecaption = 1
	virtual void			EmitSentenceCloseCaption( char const *tokenstream );
	virtual void			EmitCloseCaption( char const *captionname, float duration );

	virtual CStandardRecvProxies* GetStandardRecvProxies();

	virtual bool			CanRecordDemo( char *errorMsg, int length ) const;
	virtual bool			CanStopRecordDemo( char *errorMsg, int length ) const;

	virtual void			OnDemoRecordStart( char const* pDemoBaseName );
	virtual void			OnDemoRecordStop();
	virtual void			OnDemoPlaybackStart( char const* pDemoBaseName );
	virtual void			OnDemoPlaybackRestart();
	virtual void			OnDemoPlaybackStop();
	virtual void			SetDemoPlaybackHighlightXuid( uint64 xuid, bool bLowlights );
	virtual void			ShowHighlightSkippingMessage( bool bState, int nCurrentTick = 0, int nTickStart = 0, int nTickStop = 0 );

	virtual void			RecordDemoPolishUserInput( int nCmdIndex );

	// Cache replay ragdolls
	virtual bool			CacheReplayRagdolls( const char* pFilename, int nStartTick );

	// Send a message to the replay browser
	virtual void			ReplayUI_SendMessage( KeyValues *pMsg );

	// Get the client replay factory
	virtual IReplayFactory *GetReplayFactory();

	// Clear out the local player's replay pointer so it doesn't get deleted
	virtual void			ClearLocalPlayerReplayPtr();

	virtual bool			ShouldDrawDropdownConsole();

	// Get client screen dimensions
	virtual int				GetScreenWidth();
	virtual int				GetScreenHeight();

	// save game screenshot writing
	virtual void			WriteSaveGameScreenshotOfSize( const char *pFilename, int width, int height, bool bCreatePowerOf2Padded/*=false*/, bool bWriteVTF/*=false*/ );

	// Write a .VTF screenshot to disk for the replay system
	virtual void			WriteReplayScreenshot( WriteReplayScreenshotParams_t &params );

	// Reallocate memory for replay screenshots - called if user changes resolution or if the convar "replay_screenshotresolution" changes
	virtual void			UpdateReplayScreenshotCache();

	// Gets the location of the player viewpoint
	virtual bool			GetPlayerView( CViewSetup &playerView );

	virtual bool			ShouldHideLoadingPlaque( void );

	virtual void			InvalidateMdlCache();

	virtual void			OnActiveSplitscreenPlayerChanged( int nNewSlot );
	virtual void			OnSplitScreenStateChanged();
	virtual int 			GetSpectatorTarget( ClientDLLObserverMode_t* pObserverMode );
	virtual void			CenterStringOff();

	virtual void			OnScreenSizeChanged( int nOldWidth, int nOldHeight );
	virtual IMaterialProxy *InstantiateMaterialProxy( const char *proxyName );

	virtual void			MarkEntitiesAsTouching( IClientEntity *e1, IClientEntity *e2 );
	virtual void			OnKeyBindingChanged( ButtonCode_t buttonCode, char const *pchKeyName, char const *pchNewBinding );
	virtual bool			HandleGameUIEvent( const InputEvent_t &event );
	
	virtual bool			GetSoundSpatialization( SpatializationInfo_t& info );

public:
	void					PrecacheMaterial( const char *pMaterialName );
	void					PrecacheMovie( const char *pMovieName );

	virtual void			SetBlurFade( float scale );
	
	virtual void			ResetHudCloseCaption();

	virtual void			Hud_SaveStarted();

	virtual void			ShutdownMovies();

	virtual void			GetStatus( char *buffer, int bufsize );

	virtual bool			IsChatRaised( void );
	virtual bool			IsRadioPanelRaised( void );
	virtual bool			IsBindMenuRaised( void );
	virtual bool			IsTeamMenuRaised( void );
	virtual bool			IsLoadingScreenRaised( void );

	virtual bool			IsBuildWRThreaded( void );
	virtual void			QueueBuildWorldListJob( CJob* pJob );
	virtual void			CacheFrustumData( const Frustum_t& frustum, const std::vector< Frustum_t >& aeraFrustums );
	virtual const Frustum_t* GetBuildViewFrustum( void ) const;
	virtual const std::vector< Frustum_t >* GetBuildViewAeraFrustums( void ) const;

	virtual void RecordUIEvent( const char* szEvent );
	virtual void OnHltvReplay( const ks::net::CSVCMsg_HltvReplay  &msg ) override { g_HltvReplaySystem.OnHltvReplay( msg ); }
	virtual void OnHltvReplayTick() override { g_HltvReplaySystem.OnHltvReplayTick(); }
	virtual int GetHltvReplayDelay() override { return g_HltvReplaySystem.GetHltvReplayDelay(); }
	virtual void OnDemoPlaybackTimeJump();

	// Inventory access
	virtual float FindInventoryItemWithMaxAttributeValue( char const *szItemType, char const *szAttrClass );

	// Overwatchsupport for engine
	virtual bool ValidateSignedEvidenceHeader( char const *szKey, void const *pvHeader, CDemoPlaybackParameters_t *pPlaybackParameters );
	virtual void PrepareSignedEvidenceData( void *pvData, int numBytes, CDemoPlaybackParameters_t const *pPlaybackParameters );
	virtual bool ShouldSkipEvidencePlayback( CDemoPlaybackParameters_t const *pPlaybackParameters );

	virtual bool IsConnectedUserInfoChangeAllowed( ConVar *pCvar );
	virtual void OnCommandDuringPlayback( char const *cmd );

	virtual void RetireAllPlayerDecals( bool bRenderContextValid );

	virtual void EngineGotvSyncPacket( const ks::net::CEngineGotvSyncPacket *pPkt ) override;

	virtual void OnTickPre( int tickcount ) override;

	virtual int GetInEyeEntity() const;

private:
	void					UncacheAllMaterials();
	void					UncacheAllMovies();
	void					ResetStringTablePointers();

	std::set< IMaterial * >	m_CachedMaterials;
	ks::intern_pool				m_CachedMovies;
};

CHLClient gHLClient;
IBaseClientDLL *clientdll = &gHLClient;

static InterfaceReg s_Reg_CHLClient_IBaseClientDLL( side_registry(), []() -> void * { return static_cast<IBaseClientDLL *>( &gHLClient ); }, CLIENT_DLL_INTERFACE_VERSION );;

//-----------------------------------------------------------------------------
// Precaches a material
//-----------------------------------------------------------------------------
void PrecacheMaterial( const char *pMaterialName )
{
	gHLClient.PrecacheMaterial( pMaterialName );
}

//-----------------------------------------------------------------------------
// Converts a previously precached material into an index
//-----------------------------------------------------------------------------
int GetMaterialIndex( const char *pMaterialName )
{
	if (pMaterialName)
	{
		int nIndex = g_pStringTableMaterials->FindStringIndex( pMaterialName );
		contract_assert( nIndex >= 0 );
		if (nIndex >= 0)
			return nIndex;
	}

	// This is the invalid string index
	return 0;
}

//-----------------------------------------------------------------------------
// Converts precached material indices into strings
//-----------------------------------------------------------------------------
const char *GetMaterialNameFromIndex( int nIndex )
{
	if (nIndex != (g_pStringTableMaterials->GetMaxStrings() - 1))
	{
		return g_pStringTableMaterials->GetString( nIndex );
	}
	else
	{
		return nullptr;
	}
}

//-----------------------------------------------------------------------------
// Precaches a movie
//-----------------------------------------------------------------------------
void PrecacheMovie( const char *pMovieName )
{
	gHLClient.PrecacheMovie( pMovieName );
}

//-----------------------------------------------------------------------------
// Converts a previously precached movie into an index
//-----------------------------------------------------------------------------
int GetMovieIndex( const char *pMovieName )
{
	if ( pMovieName )
	{
		int nIndex = g_pStringTableMovies->FindStringIndex( pMovieName );
		contract_assert( nIndex >= 0 );
		if ( nIndex >= 0 )
		{
			return nIndex;
		}
	}

	// This is the invalid string index
	return 0;
}

//-----------------------------------------------------------------------------
// Converts precached movie indices into strings
//-----------------------------------------------------------------------------
const char *GetMovieNameFromIndex( int nIndex )
{
	if ( nIndex != ( g_pStringTableMovies->GetMaxStrings() - 1 ) )
	{
		return g_pStringTableMovies->GetString( nIndex );
	}
	else
	{
		return nullptr;
	}
}

//-----------------------------------------------------------------------------
// Precaches a particle system
//-----------------------------------------------------------------------------
int PrecacheParticleSystem( const char *pParticleSystemName )
{
	int nIndex = g_pStringTableParticleEffectNames->AddString( false, pParticleSystemName );
	g_pParticleSystemMgr->PrecacheParticleSystem( nIndex, pParticleSystemName );
	return nIndex;
}

//-----------------------------------------------------------------------------
// Converts a previously precached particle system into an index
//-----------------------------------------------------------------------------
int GetParticleSystemIndex( const char *pParticleSystemName )
{
	if ( pParticleSystemName )
	{
		int nIndex = g_pStringTableParticleEffectNames->FindStringIndex( pParticleSystemName );
		if ( nIndex != INVALID_STRING_INDEX )
			return nIndex;
		DevWarning("Client: Missing precache for particle system \"%s\"!\n", pParticleSystemName );
	}

	// This is the invalid string index
	return 0;
}

//-----------------------------------------------------------------------------
// Converts precached particle system indices into strings
//-----------------------------------------------------------------------------
const char *GetParticleSystemNameFromIndex( int nIndex )
{
	if ( nIndex < g_pStringTableParticleEffectNames->GetMaxStrings() )
		return g_pStringTableParticleEffectNames->GetString( nIndex );
	return "error";
}

//-----------------------------------------------------------------------------
// Precache-related methods for effects
//-----------------------------------------------------------------------------
void PrecacheEffect( const char *pEffectName )
{
	// Bring in dependent resources
	g_pPrecacheSystem->Cache( g_PrecacheHandler, DISPATCH_EFFECT, pEffectName, true, RESOURCE_LIST_INVALID, true );
}

//-----------------------------------------------------------------------------
// Returns true if host_thread_mode is set to non-zero (and engine is running in threaded mode)
//-----------------------------------------------------------------------------
bool IsEngineThreaded()
{
	if ( g_pcv_ThreadMode )
	{
		return g_pcv_ThreadMode->GetBool();
	}
	return false;
}

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CHLClient::CHLClient() 
{
	// Kinda bogus, but the logic in the engine is too convoluted to put it there
	g_bLevelInitialized = false;
}

extern IGameSystem *ViewportClientSystem();

// enable threaded init functions on x360
static ConVar cl_threaded_init(side_registry(), "cl_threaded_init", "0");

bool InitParticleManager()
{
	if (!ParticleMgr()->Init(MAX_TOTAL_PARTICLES, materials))
		return false;

	return true;
}

bool InitGameSystems( CreateInterfaceFn appSystemFactory )
{
	if (!UI_Startup( appSystemFactory ))
		return false;

	// Keep the spinner going.
	materials->RefreshFrontBufferNonInteractive();

	// Keep the spinner going.
	materials->RefreshFrontBufferNonInteractive();

	// Add the client systems.	

	// Client Leaf System has to be initialized first, since DetailObjectSystem uses it
	IGameSystem::Add( GameStringSystem() );
	IGameSystem::Add( g_pPrecacheRegister );
	IGameSystem::Add( SoundEmitterSystem() );
	IGameSystem::Add( ClientLeafSystem() );
	IGameSystem::Add( DetailObjectSystem() );
	IGameSystem::Add( ViewportClientSystem() );
	IGameSystem::Add( g_pClientShadowMgr );
	IGameSystem::Add( g_pColorCorrectionMgr );
	IGameSystem::Add( ClientThinkList() );
	IGameSystem::Add( ClientSoundscapeSystem() );
	IGameSystem::Add( PerfVisualBenchmark() );
	IGameSystem::Add( MumbleSystem() );

	ActivityList_Init();
	ActivityList_RegisterSharedActivities();
	EventList_Init();
	EventList_RegisterSharedEvents();

	modemanager->Init( );

	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD_UI( hh );
		GetClientMode()->InitViewport();

		if ( hh == 0 )
		{
			GetFullscreenClientMode()->InitViewport();
		}
	}

	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD_UI( hh );
		GetHud().Init();
	}

	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD_UI( hh );
		GetClientMode()->Init();

		if ( hh == 0 )
		{
			GetFullscreenClientMode()->Init();
		}
	}

	if ( !IGameSystem::InitAllSystems() )
		return false;

	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD_UI( hh );
		GetClientMode()->Enable();

		if ( hh == 0 )
		{
			GetFullscreenClientMode()->Enable();
		}
	}	

	// Each mod is required to implement this
	view = GetViewRenderInstance();
	if ( !view )
	{
		Error( "GetViewRenderInstance() must be implemented by game." );
	}

	view->Init();
	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD_UI( hh );
		GetViewEffects()->Init();
	}

	CBaseTempEntity::PrecacheTempEnts();

	input->Init_All();

	UI_CreateGlobalPanels();

	InitSmokeFogOverlay();

	// Register user messages..
	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		CUserMessageRegisterBase::RegisterAll();
	}

	ClientVoiceMgr_Init();

	// Embed voice status icons inside chat element
	{
		GetClientVoiceMgr()->Init( &g_VoiceStatusHelper, 0 );
	}

	if ( !PhysicsDLLInit( appSystemFactory ) )
		return false;

	ClientWorldFactoryInit();

	// Keep the spinner going.
	materials->RefreshFrontBufferNonInteractive();

	return true;
}

static void cl_modemanager_reload( const CCommand &args );
static ConCommand cl_modemanager_reload_command( side_registry(), "cl_modemanager_reload", cl_modemanager_reload, "Reloads the panel metaclasses for vgui screens." );
static void cl_modemanager_reload( const CCommand &args )
{
	modemanager->Init();
}

//CEG_PROTECT_FUNCTION( InitGameSystems );

//-----------------------------------------------------------------------------
// Purpose: Called when the DLL is first loaded.
// Input  : engineFactory - 
// Output : int
//-----------------------------------------------------------------------------
int CHLClient::Connect( CreateInterfaceFn appSystemFactory, CGlobalVarsBase *pGlobals )
{
	InitCRTMemDebug();
	MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f );

	// Hook up global variables
	gpGlobals = pGlobals;

	// The tier libraries are shared with the engine, which has connected them.

	// Initialize the console variables.
	ConVar_Register( side_registry().convars, FCVAR_CLIENTDLL );
	g_pExtrapolateAmount = &cl_extrapolate_amount;

	return true;
}

void CHLClient::Disconnect()
{
	ConVar_Unregister( side_registry().convars );
}

int CHLClient::Init( CreateInterfaceFn appSystemFactory, CGlobalVarsBase *pGlobals )
{
	COM_TimestampedLog( "ClientDLL factories - Start" );
	// We aren't happy unless we get all of our interfaces.
	// please don't collapse this into one monolithic boolean expression (impossible to debug)
	if ( (engine = (IVEngineClient *)appSystemFactory( VENGINE_CLIENT_INTERFACE_VERSION, nullptr )) == nullptr )
		return false;
	if ( (modelrender = (IVModelRender *)appSystemFactory( VENGINE_HUDMODEL_INTERFACE_VERSION, nullptr )) == nullptr )
		return false;
	if ( (effects = (IVEfx *)appSystemFactory( VENGINE_EFFECTS_INTERFACE_VERSION, nullptr )) == nullptr )
		return false;
	if ( (enginetrace = (IEngineTrace *)appSystemFactory( INTERFACEVERSION_ENGINETRACE_CLIENT, nullptr )) == nullptr )
		return false;
	if ( (filelogginglistener = (CFileLoggingListener *)appSystemFactory(FILELOGGINGLISTENER_INTERFACE_VERSION, nullptr)) == nullptr )
		return false;
	if ( (render = (IVRenderView *)appSystemFactory( VENGINE_RENDERVIEW_INTERFACE_VERSION, nullptr )) == nullptr )
		return false;
	if ( (debugoverlay = (IVDebugOverlay *)appSystemFactory( VDEBUG_OVERLAY_INTERFACE_VERSION, nullptr )) == nullptr )
		return false;
	if ( (datacache = (IDataCache*)appSystemFactory(DATACACHE_INTERFACE_VERSION, nullptr )) == nullptr )
		return false;
	if ( !mdlcache )
		return false;
	if ( (modelinfo = (IVModelInfoClient *)appSystemFactory(VMODELINFO_CLIENT_INTERFACE_VERSION, nullptr )) == nullptr )
		return false;
	if ( (engineui = (IEngineUI *)appSystemFactory(VENGINE_UI_VERSION, nullptr )) == nullptr )
		return false;
	if ( (networkstringtable = (INetworkStringTableContainer *)appSystemFactory(INTERFACENAME_NETWORKSTRINGTABLECLIENT,nullptr)) == nullptr )
		return false;
	if ( (::partition = (ISpatialPartition *)appSystemFactory(INTERFACEVERSION_SPATIALPARTITION, nullptr)) == nullptr )
		return false;
	if ( (shadowmgr = (IShadowMgr *)appSystemFactory(ENGINE_SHADOWMGR_INTERFACE_VERSION, nullptr)) == nullptr )
		return false;
	if ( (staticpropmgr = (IStaticPropMgrClient *)appSystemFactory(INTERFACEVERSION_STATICPROPMGR_CLIENT, nullptr)) == nullptr )
		return false;
	if ( (enginesound = (IEngineSound *)appSystemFactory(IENGINESOUND_CLIENT_INTERFACE_VERSION, nullptr)) == nullptr )
		return false;
	if ( (filesystem = (IFileSystem *)appSystemFactory(FILESYSTEM_INTERFACE_VERSION, nullptr)) == nullptr )
		return false;
	if ( (random_valve = (CUniformRandomStream *)appSystemFactory(VENGINE_CLIENT_RANDOM_INTERFACE_VERSION, nullptr)) == nullptr )
		return false;
	if ( (gameuifuncs = (IGameUIFuncs * )appSystemFactory( VENGINE_GAMEUIFUNCS_VERSION, nullptr )) == nullptr )
		return false;
	if ( (gameeventmanager = (IGameEventManager2 *)appSystemFactory(INTERFACEVERSION_GAMEEVENTSMANAGER2,nullptr)) == nullptr )
		return false;
	if ( (soundemitterbase = (ISoundEmitterSystemBase *)appSystemFactory(SOUNDEMITTERSYSTEM_INTERFACE_VERSION, nullptr)) == nullptr )
		return false;
	if ( (inputsystem = (IInputSystem *)appSystemFactory(INPUTSYSTEM_INTERFACE_VERSION, nullptr)) == nullptr )
		return false;
	if ( (scenefilecache = (ISceneFileCache *)appSystemFactory( SCENE_FILE_CACHE_INTERFACE_VERSION, nullptr )) == nullptr )
		return false;
	if ( (blackboxrecorder = (IBlackBox *)appSystemFactory(BLACKBOX_INTERFACE_VERSION, nullptr)) == nullptr )
		return false;

	if ( (g_pRenderToRTHelper = (IRenderToRTHelper *)appSystemFactory( RENDER_TO_RT_HELPER_INTERFACE_VERSION, nullptr )) == nullptr )
		return false;
	if ( !g_pRenderToRTHelper->Init() )
		return false;

	if ( ( g_pGameTypes = (IGameTypes *)appSystemFactory( VENGINE_GAMETYPES_VERSION, nullptr )) == nullptr )
		return false;

	if ( ( gamestatsuploader = (IUploadGameStats *)appSystemFactory( INTERFACEVERSION_UPLOADGAMESTATS, nullptr )) == nullptr )
		return false;

	if ( !g_pMatchFramework )
		return false;
	GameInstructor_Init();

	if ( !g_pMatchFramework )
		return false;
	if ( IMatchExtensions *pIMatchExtensions = g_pMatchFramework->GetMatchExtensions() )
		pIMatchExtensions->RegisterExtensionInterface(
		CLIENT_DLL_INTERFACE_VERSION, static_cast< IBaseClientDLL * >( this ) );

	if ( !CommandLine()->CheckParm( "-noscripting") )
	{
		scriptmanager = (IScriptManager *)appSystemFactory( VSCRIPT_INTERFACE_VERSION, nullptr );
	}

	factorylist_t factories;
	factories.appSystemFactory = appSystemFactory;
	FactoryList_Store( factories );

	COM_TimestampedLog( "soundemitterbase->Connect" );
	// Yes, both the client and game .dlls will try to Connect, the soundemittersystem.dll will handle this gracefully
	if ( !soundemitterbase->Connect( appSystemFactory ) )
	{
		return false;
	}

	if ( CommandLine()->FindParm( "-makedevshots" ) )
		g_MakingDevShots = true;

	if ( CommandLine()->FindParm( "-headtracking" ) )
		g_bHeadTrackingEnabled = true;

	if( !g_pMaterialSystemHardwareConfig )
		return false;

	// Hook up the gaussian random number generator
	s_GaussianRandomStream.AttachToStream( random_valve );

	g_pcv_ThreadMode = g_pCVar->FindVar( "host_thread_mode" );

	COM_TimestampedLog( "InitGameSystems" );

	bool bInitSuccess = false;
	if ( cl_threaded_init.GetBool() )
	{
		CJob *pGameJob = new CJob( CreateFunctor( InitParticleManager ) );
		g_pThreadPool->AddJob( pGameJob );
		bInitSuccess = InitGameSystems( appSystemFactory );

		// While the InitParticleManager job isn't finished, try to update the spinner
		while( !pGameJob->IsFinished() )
		{
			g_pMaterialSystem->RefreshFrontBufferNonInteractive();
			
			// Don't just update as fast as we can
			ThreadSleep( 50 );
		}
		pGameJob->Release();
	}
	else
	{
		COM_TimestampedLog( "ParticleMgr()->Init" );
		if (!ParticleMgr()->Init(MAX_TOTAL_PARTICLES, materials))
			return false;
		COM_TimestampedLog( "InitGameSystems - Start" );
		bInitSuccess = InitGameSystems( appSystemFactory );
		COM_TimestampedLog( "InitGameSystems - End" );
	}

	COM_TimestampedLog( "CBaseAnimating::InitBoneSetupThreadPool" );

	CBaseAnimating::InitBoneSetupThreadPool();

	// Load the game types.
	g_pGameTypes->Initialize();

	COM_TimestampedLog( "ClientDLL Init - Finish" );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Called after client & server DLL are loaded and all systems initialized
//-----------------------------------------------------------------------------
void CHLClient::PostInit()
{
	COM_TimestampedLog( "IGameSystem::PostInitAllSystems - Start" );
	IGameSystem::PostInitAllSystems();
	COM_TimestampedLog( "IGameSystem::PostInitAllSystems - Finish" );

	// Turn 7L rendering optimisations ON if "-rdropt" is specified on the command line
	if ( CommandLine()->FindParm( "-rdropt" ) != 0 )
	{
		engine->ExecuteClientCmd( "toggleRdrOpt" );
	}

	// Captures console output, and installs RocketUI's input hook for its lifetime.
	RkConsole().Initialize();
}

//-----------------------------------------------------------------------------
// Purpose: Called when the client .dll is being dismissed
//-----------------------------------------------------------------------------
void CHLClient::Shutdown( void )
{
	// RocketUI outlives the client DLL: this also clears its input hook.
	RkConsole().Shutdown();

	if ( g_pRenderToRTHelper )
	{
		g_pRenderToRTHelper->Shutdown();
	}

    if ( g_pAchievementsAndStatsInterface )
    {
        g_pAchievementsAndStatsInterface->ReleasePanel();
    }

	ActivityList_Free();
	EventList_Free();

	CBaseAnimating::ShutdownBoneSetupThreadPool();
	ClientWorldFactoryShutdown();

	ClientVoiceMgr_Shutdown();

	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD_UI( hh );
		GetClientMode()->Disable();

		if ( hh == 0 )
		{
			GetFullscreenClientMode()->Disable();
		}
	}

	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD_UI( hh );
		GetClientMode()->Shutdown();

		if ( hh == 0 )
		{
			GetFullscreenClientMode()->Shutdown();
		}
	}

	input->Shutdown_All();
	CBaseTempEntity::ClearDynamicTempEnts();
	TermSmokeFogOverlay();
	view->Shutdown();

	// Terminate particle manager first to destroy all active particle effects
	// before uncaching particle system definitions (which releases material references)
	ParticleMgr()->Term();
	g_pParticleSystemMgr->Shutdown();

	UncacheAllMovies();
	UncacheAllMaterials();

	IGameSystem::ShutdownAllSystems();

	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD_UI( hh );
		GetHud().Shutdown();
	}

	UI_Shutdown();

#ifdef USE_BLOBULATOR
	Blobulator::ShutdownBlob();
#endif // USE_BLOBULATOR

	ConVar_Unregister( side_registry().convars );

	gameeventmanager = nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: 
//  Called when the game initializes
//  and whenever the vid_mode is changed
//  so the HUD can reinitialize itself.
// Output : int
//-----------------------------------------------------------------------------
int CHLClient::HudVidInit( void )
{
	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		GetHud().VidInit();
	}

	GetClientVoiceMgr()->VidInit();

	return 1;
}

//-----------------------------------------------------------------------------
// Method used to allow the client to filter input messages before the 
// move record is transmitted to the server
//-----------------------------------------------------------------------------
void CHLClient::HudProcessInput( bool bActive )
{
	GetClientMode()->ProcessInput( bActive );
}

//-----------------------------------------------------------------------------
// Purpose: Called when shared data gets changed, allows dll to modify data
// Input  : bActive - 
//-----------------------------------------------------------------------------
void CHLClient::HudUpdate( bool bActive )
{
	float frametime = gpGlobals->frametime;

	GetClientVoiceMgr()->Frame( frametime );

	ASSERT_LOCAL_PLAYER_NOT_RESOLVABLE();

	FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		GetHud().UpdateHud( bActive );
	}

	ASSERT_LOCAL_PLAYER_NOT_RESOLVABLE();

	{
		CBaseAnimating::AutoAllowBoneAccess boneaccess( true, false ); 
		IGameSystem::UpdateAllSystems( frametime );
	}

	// run vgui animations

	// I don't think this is necessary any longer, but I will leave it until
	// I can check into this further.
	CBaseTempEntity::CheckDynamicTempEnts();
}

//-----------------------------------------------------------------------------
// Purpose: Called to restore to "non"HUD state.
//-----------------------------------------------------------------------------
void CHLClient::HudReset( void )
{
	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		GetHud().VidInit();
	}

	PhysicsReset();
}

//-----------------------------------------------------------------------------
// Purpose: Called to add hud text message
//-----------------------------------------------------------------------------
void CHLClient::HudText( const char * message )
{
}

//-----------------------------------------------------------------------------
// Handler for input events for the new game ui system
//-----------------------------------------------------------------------------
bool CHLClient::HandleGameUIEvent( const InputEvent_t &inputEvent )
{
	return false;
}

//-----------------------------------------------------------------------------
// Process any extra client specific, non-entity based sound positioning.
//-----------------------------------------------------------------------------
bool CHLClient::GetSoundSpatialization(  SpatializationInfo_t& info )
{

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHLClient::ShouldDrawDropdownConsole()
{

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : ClientClass
//-----------------------------------------------------------------------------
ClientClass *CHLClient::GetAllClasses( void )
{
	return g_pClientClassHead;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHLClient::IN_ActivateMouse( void )
{
	input->ActivateMouse();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHLClient::IN_DeactivateMouse( void )
{
	input->DeactivateMouse();
}

extern ConVar in_forceuser;
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHLClient::IN_Accumulate ( void )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( in_forceuser.GetInt() );
	input->AccumulateMouse( GET_ACTIVE_SPLITSCREEN_SLOT() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHLClient::IN_ClearStates ( void )
{
	input->ClearStates();
}

//-----------------------------------------------------------------------------
// Purpose: Engine can query for particular keys
// Input  : *name - 
//-----------------------------------------------------------------------------
bool CHLClient::IN_IsKeyDown( const char *name, bool& isdown )
{
	kbutton_t *key = input->FindKey( name );
	if ( !key )
	{
		return false;
	}
	
	isdown = ( key->GetPerUser().state & 1 ) ? true : false;

	// Found the key by name
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Engine can issue a key event
// Input  : eventcode - 
//			keynum - 
//			*pszCurrentBinding - 
// Output : int
//-----------------------------------------------------------------------------
int CHLClient::IN_KeyEvent( int eventcode, ButtonCode_t keynum, const char *pszCurrentBinding )
{
	return input->KeyEvent( eventcode, keynum, pszCurrentBinding );
}

void CHLClient::ExtraMouseSample( float frametime, bool active )
{
	bool bSave = CBaseEntity::IsAbsRecomputationsEnabled();
	CBaseEntity::EnableAbsRecomputations( true );

	// [mariod] - testing, see note in c_baseanimating.cpp
	//CBaseAnimating::EnableNewBoneSetupRequest( false );

	ABS_QUERY_GUARD( true );

	CBaseAnimating::AutoAllowBoneAccess boneaccess( true, false ); 

	MDLCACHE_CRITICAL_SECTION();
	input->ExtraMouseSample( frametime, active );

	// [mariod] - testing, see note in c_baseanimating.cpp
	//CBaseAnimating::EnableNewBoneSetupRequest( true );

	CBaseEntity::EnableAbsRecomputations( bSave );
}

void CHLClient::IN_SetSampleTime( float frametime )
{
	input->Joystick_SetSampleTime( frametime );
	input->IN_SetSampleTime( frametime );
}
//-----------------------------------------------------------------------------
// Purpose: Fills in usercmd_s structure based on current view angles and key/controller inputs
// Input  : frametime - timestamp for last frame
//			*cmd - the command to fill in
//			active - whether the user is fully connected to a server
//-----------------------------------------------------------------------------
void CHLClient::CreateMove ( int sequence_number, float input_sample_frametime, bool active )
{

	contract_assert( CBaseEntity::IsAbsRecomputationsEnabled() );
	contract_assert( CBaseEntity::IsAbsQueriesValid() );

	CBaseAnimating::AutoAllowBoneAccess boneaccess( true, false ); 

	// [mariod] - testing, see note in c_baseanimating.cpp
	//CBaseAnimating::EnableNewBoneSetupRequest( false );

	MDLCACHE_CRITICAL_SECTION();
	input->CreateMove( sequence_number, input_sample_frametime, active );

	// [mariod] - testing, see note in c_baseanimating.cpp
	//CBaseAnimating::EnableNewBoneSetupRequest( true );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *buf - 
//			from - 
//			to - 
//-----------------------------------------------------------------------------
bool CHLClient::WriteUsercmdDeltaToBuffer( int nSlot, bf_write *buf, int from, int to, bool isnewcommand )
{
	return input->WriteUsercmdDeltaToBuffer( nSlot, buf, from, to, isnewcommand );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : buf - 
//			buffersize - 
//-----------------------------------------------------------------------------
void CHLClient::EncodeUserCmdToBuffer( int nSlot, bf_write& buf, int slot )
{
	input->EncodeUserCmdToBuffer( nSlot, buf, slot );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : buf - 
//			buffersize - 
//			slot - 
//-----------------------------------------------------------------------------
void CHLClient::DecodeUserCmdFromBuffer( int nSlot, bf_read& buf, int slot )
{
	input->DecodeUserCmdFromBuffer( nSlot, buf, slot );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHLClient::View_Render( vrect_t *rect )
{
	CVProfScope _( "View_Render", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	// UNDONE: This gets hit at startup sometimes, investigate - will cause NaNs in calcs inside Render()
	if ( rect->width == 0 || rect->height == 0 )
		return;

	view->Render( rect );

	UpdatePerfStats();
}

//-----------------------------------------------------------------------------
// Gets the location of the player viewpoint
//-----------------------------------------------------------------------------
bool CHLClient::GetPlayerView( CViewSetup &playerView )
{
	playerView = *view->GetPlayerViewSetup();
	return true;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CHLClient::InvalidateMdlCache()
{
	CBaseAnimating *pAnimating;
	for ( CBaseEntity *pEntity = ClientEntityList().FirstBaseEntity(); pEntity; pEntity = ClientEntityList().NextBaseEntity(pEntity) )
	{
		pAnimating = pEntity->GetBaseAnimating();
		if ( pAnimating )
		{
			pAnimating->InvalidateMdlCache();
		}
	}
	CStudioHdr::CActivityToSequenceMapping::ResetMappings();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSF - 
//-----------------------------------------------------------------------------
void CHLClient::View_Fade( ScreenFade_t *pSF )
{
	if ( pSF != nullptr )
	{
		FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
		{
			ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
			GetViewEffects()->Fade( *pSF );
		}
	}
}

// CPU level
//-----------------------------------------------------------------------------
void ConfigureCurrentSystemLevel( );
void OnCPULevelChanged( ConVar *var, const char *pOldValue, float flOldValue )
{
	ConfigureCurrentSystemLevel();
}

static ConVar cpu_level( side_registry(), "cpu_level", "2", 0, "CPU Level - Default: High", OnCPULevelChanged );

CPULevel_t GetCPULevel()
{

	return GetActualCPULevel();
}

CPULevel_t GetActualCPULevel()
{
	// Should we cache system_level off during level init?
	CPULevel_t nSystemLevel = (CPULevel_t)clamp( cpu_level.GetInt(), 0, CPU_LEVEL_PC_COUNT-1 );
	return nSystemLevel;
}

//-----------------------------------------------------------------------------
// GPU level
//-----------------------------------------------------------------------------
void OnGPULevelChanged( ConVar *var, const char *pOldValue, float flOldValue )
{
	ConfigureCurrentSystemLevel();
}

static ConVar gpu_level( side_registry(), "gpu_level", "3", 0, "GPU Level - Default: High", OnGPULevelChanged );
GPULevel_t GetGPULevel()
{

	// Should we cache system_level off during level init?
	GPULevel_t nSystemLevel = (GPULevel_t)clamp( gpu_level.GetInt(), 0, GPU_LEVEL_PC_COUNT-1 );
	return nSystemLevel;
}

//-----------------------------------------------------------------------------
// System Memory level
//-----------------------------------------------------------------------------
void OnMemLevelChanged( ConVar *var, const char *pOldValue, float flOldValue )
{
	ConfigureCurrentSystemLevel();
}

static ConVar mem_level( side_registry(), "mem_level", "2", 0, "Memory Level - Default: High", OnMemLevelChanged );

MemLevel_t GetMemLevel()
{
	
	const int nMinMemLevel = 2;

	// Should we cache system_level off during level init?
	MemLevel_t nSystemLevel = (MemLevel_t)clamp( mem_level.GetInt(), nMinMemLevel, MEM_LEVEL_PC_COUNT-1 );
	return nSystemLevel;
}

//-----------------------------------------------------------------------------
// GPU Memory level
//-----------------------------------------------------------------------------
void OnGPUMemLevelChanged( ConVar *var, const char *pOldValue, float flOldValue )
{
	ConfigureCurrentSystemLevel();
}

static ConVar gpu_mem_level( side_registry(), "gpu_mem_level", "2", 0, "Memory Level - Default: High", OnGPUMemLevelChanged );

GPUMemLevel_t GetGPUMemLevel()
{

	// Should we cache system_level off during level init?
	GPUMemLevel_t nSystemLevel = (GPUMemLevel_t)clamp( gpu_mem_level.GetInt(), 0, GPU_MEM_LEVEL_PC_COUNT-1 );
	return nSystemLevel;
}

ConVar cl_disable_splitscreen_cpu_level_cfgs_in_pip( side_registry(), "cl_disable_splitscreen_cpu_level_cfgs_in_pip", "1", 0, "" );

void ConfigureCurrentSystemLevel()
{
	int nCPULevel = GetCPULevel();
	if ( nCPULevel == CPU_LEVEL_360 )
	{
		nCPULevel = CONSOLE_SYSTEM_LEVEL_360;
	}
	else if ( nCPULevel == CPU_LEVEL_PS3 )
	{
		nCPULevel = CONSOLE_SYSTEM_LEVEL_PS3;
	}

	int nGPULevel = GetGPULevel();
	if ( nGPULevel == GPU_LEVEL_360 )
	{
		nGPULevel = CONSOLE_SYSTEM_LEVEL_360;
	}
	else if ( nGPULevel == GPU_LEVEL_PS3 )
	{
		nGPULevel = CONSOLE_SYSTEM_LEVEL_PS3;
	}

	int nMemLevel = GetMemLevel();
	if ( nMemLevel == MEM_LEVEL_360 )
	{
		nMemLevel = CONSOLE_SYSTEM_LEVEL_360;
	}
	else if ( nMemLevel == MEM_LEVEL_PS3 )
	{
		nMemLevel = CONSOLE_SYSTEM_LEVEL_PS3;
	}

	int nGPUMemLevel = GetGPUMemLevel();
	if ( nGPUMemLevel == GPU_MEM_LEVEL_360 )
	{
		nGPUMemLevel = CONSOLE_SYSTEM_LEVEL_360;
	}
	else if ( nGPUMemLevel == GPU_MEM_LEVEL_PS3 )
	{
		nGPUMemLevel = CONSOLE_SYSTEM_LEVEL_PS3;
	}

	char szModName[32] = "csgo";

	bool bUIIsSplitscreen = UI_IsSplitScreen();
	if ( cl_disable_splitscreen_cpu_level_cfgs_in_pip.GetBool() && bUIIsSplitscreen && UI_IsSplitScreenPIP() )
	{
		bUIIsSplitscreen = false;
	}
	
	UpdateSystemLevel( nCPULevel, nGPULevel, nMemLevel, nGPUMemLevel, bUIIsSplitscreen, szModName );

	if ( engine )
	{
		engine->ConfigureSystemLevel( nCPULevel, nGPULevel );
	}

	CBaseEntity::UpdateVisibilityAllEntities();
	if ( view )
	{
		view->InitFadeData();
	}
}

//CEG_PROTECT_FUNCTION( ConfigureCurrentSystemLevel );

//-----------------------------------------------------------------------------
// Purpose: Per level init
//-----------------------------------------------------------------------------
void CHLClient::LevelInitPreEntity( char const* pMapName )
{
	// HACK: Bogus, but the logic is too complicated in the engine
	if (g_bLevelInitialized)
		return;
	g_bLevelInitialized = true;

	engine->TickProgressBar();

	// this is a "connected" state
	g_HltvReplaySystem.StopHltvReplay( );

	input->LevelInit();

	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		GetViewEffects()->LevelInit();
	}

	// Tell mode manager that map is changing
	modemanager->LevelInit( pMapName );
	ParticleMgr()->LevelInit();

	ClientVoiceMgr_LevelInit();

	CBaseTempEntity::ClearDynamicTempEnts();
	clienteffects->Flush();
	view->LevelInit();
	tempents->LevelInit();
	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		ResetToneMapping(1.0);
	}

	IGameSystem::LevelInitPreEntityAllSystems(pMapName);

    // Item system removed - no econ

	ResetWindspeed();

	//[msmith] Portal 2 wanted to turn off prediction for local clients.
	//         We want to keep it for CStrike15 (and probably other games) because of the noticable lag without it, particularly when firing a weapon.

	// Turn off prediction if we're in split screen because there are a lot of third person animation and render issues with prediction.
	if ( !engine->IsSplitScreenActive() )

	{
		if ( !cl_predict->GetInt() )
		{
			engine->ClientCmd( "cl_predict 1" );
		}
	}
	else
	{
		if ( cl_predict->GetInt() )
		{
			engine->ClientCmd( "cl_predict 0" );
		}
	}

	// Check low violence settings for this map
	g_RagdollLVManager.SetLowViolence( pMapName );

	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );

		engine->TickProgressBar();

		GetHud().LevelInit();

		if ( GetViewPortInterface() )
		{
			GetViewPortInterface()->LevelInit();
		}
	}

}

//-----------------------------------------------------------------------------
// Purpose: Per level init
//-----------------------------------------------------------------------------
void CHLClient::LevelInitPostEntity( )
{
	ABS_QUERY_GUARD( true );

	IGameSystem::LevelInitPostEntityAllSystems();
	CPhysPropClientside::RecreateAll();
	CSprite::RecreateAllClientside();

	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		GetCenterPrint()->Clear();
	}

	g_HltvReplaySystem.OnLevelInit();
}

//-----------------------------------------------------------------------------
// Purpose: Reset our global string table pointers
//-----------------------------------------------------------------------------
void CHLClient::ResetStringTablePointers()
{
	g_pStringTableParticleEffectNames = nullptr;
	g_StringTableEffectDispatch = nullptr;
	g_pStringTableMaterials = nullptr;
	g_pStringTableInfoPanel = nullptr;
	g_pStringTableClientSideChoreoScenes = nullptr;
	g_pStringTableMovies = nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: Per level de-init
//-----------------------------------------------------------------------------
void CHLClient::LevelShutdown( void )
{
	g_HltvReplaySystem.OnLevelShutdown();

	// HACK: Bogus, but the logic is too complicated in the engine
	if (!g_bLevelInitialized)
	{
		ResetStringTablePointers();
		return;
	}

	g_bLevelInitialized = false;

	// this is a "connected" state
	g_HltvReplaySystem.StopHltvReplay();

	// Disable abs recomputations when everything is shutting down
	CBaseEntity::EnableAbsRecomputations( false );

	// Level shutdown sequence.
	// First do the pre-entity shutdown of all systems
	IGameSystem::LevelShutdownPreEntityAllSystems();

	CSprite::DestroyAllClientside();
	CPhysPropClientside::DestroyAll();

	modemanager->LevelShutdown();

	// Remove temporary entities before removing entities from the client entity list so that the te_* may
	// clean up before hand.
	tempents->LevelShutdown();

	// Now release/delete the entities
	cl_entitylist->Release();

	CBaseEntityClassList *pClassList = s_pClassLists;
	while ( pClassList )
	{
		pClassList->LevelShutdown();
		pClassList = pClassList->m_pNextClassList;
	}

	// Now do the post-entity shutdown of all systems
	IGameSystem::LevelShutdownPostEntityAllSystems();

	view->LevelShutdown();
	beams->ClearBeams();
	ParticleMgr()->RemoveAllEffects();
	
	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		StopAllRumbleEffects( hh );
	}

	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		GetHud().LevelShutdown();
	}

	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		GetCenterPrint()->Clear();
	}

	ClientVoiceMgr_LevelShutdown();

	g_pParticleSystemMgr->LevelShutdown();
	g_pParticleSystemMgr->UncacheAllParticleSystems();

	UncacheAllMovies();
	UncacheAllMaterials();

	// string tables are cleared on disconnect from a server, so reset our global pointers to NULL
	ResetStringTablePointers();

	CStudioHdr::CActivityToSequenceMapping::ResetMappings();

}

//-----------------------------------------------------------------------------
// Purpose: Engine received crosshair offset ( autoaim )
// Input  : angle - 
//-----------------------------------------------------------------------------
void CHLClient::SetCrosshairAngle( const QAngle& angle )
{
}

//-----------------------------------------------------------------------------
// Purpose: Helper to initialize sprite from .spr semaphor
// Input  : *pSprite - 
//			*loadname - 
//-----------------------------------------------------------------------------
void CHLClient::InitSprite( void *pSprite, const char *loadname )
{
	if ( pSprite )
	{
		static_cast<CEngineSprite *>( pSprite )->Init( loadname );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSprite - 
//-----------------------------------------------------------------------------
void CHLClient::ShutdownSprite( void *pSprite )
{
	if ( pSprite )
	{
		static_cast<CEngineSprite *>( pSprite )->Shutdown();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Tells engine how much space to allocate for sprite objects
// Output : int
//-----------------------------------------------------------------------------
int CHLClient::GetSpriteSize( void ) const
{
	return sizeof( CEngineSprite );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : entindex - 
//			bTalking - 
//-----------------------------------------------------------------------------
void CHLClient::VoiceStatus( int entindex, int iSsSlot, qboolean bTalking )
{
	GetClientVoiceMgr()->UpdateSpeakerStatus( entindex, iSsSlot, !!bTalking );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : entindex - 
//-----------------------------------------------------------------------------
bool CHLClient::PlayerAudible( int iPlayerIndex )
{
	return GetClientVoiceMgr()->IsPlayerAudible( iPlayerIndex );
}

//-----------------------------------------------------------------------------
// Called when the string table for materials changes
//-----------------------------------------------------------------------------
void OnMaterialStringTableChanged( void *object, INetworkStringTable *stringTable, int stringNumber, const char *newString, void const *newData )
{
	// Make sure this puppy is precached
	gHLClient.PrecacheMaterial( newString );
	RequestCacheUsedMaterials();
}

//-----------------------------------------------------------------------------
// Called when the string table for movies changes
//-----------------------------------------------------------------------------
void OnMovieStringTableChanged( void *object, INetworkStringTable *stringTable, int stringNumber, const char *newString, void const *newData )
{
	// Make sure this puppy is precached
	gHLClient.PrecacheMovie( newString );
}

//-----------------------------------------------------------------------------
// Called when the string table for dispatch effects changes
//-----------------------------------------------------------------------------
void OnEffectStringTableChanged( void *object, INetworkStringTable *stringTable, int stringNumber, const char *newString, void const *newData )
{
	// Make sure this puppy is precached
	g_pPrecacheSystem->Cache( g_PrecacheHandler, DISPATCH_EFFECT, newString, true, RESOURCE_LIST_INVALID, true );
	RequestCacheUsedMaterials();
}

//-----------------------------------------------------------------------------
// Called when the string table for particle systems changes
//-----------------------------------------------------------------------------
void OnParticleSystemStringTableChanged( void *object, INetworkStringTable *stringTable, int stringNumber, const char *newString, void const *newData )
{
	// Make sure this puppy is precached
	g_pParticleSystemMgr->PrecacheParticleSystem( stringNumber, newString );
	RequestCacheUsedMaterials();
}

//-----------------------------------------------------------------------------
// Called when the string table for particle files changes
//-----------------------------------------------------------------------------
void OnPrecacheParticleFile( void *object, INetworkStringTable *stringTable, int stringNumber, const char *newString, void const *newData )
{
	g_pParticleSystemMgr->ShouldLoadSheets( true );
	g_pParticleSystemMgr->ReadParticleConfigFile( newString, true, false );
	g_pParticleSystemMgr->DecommitTempMemory();
}

//-----------------------------------------------------------------------------
// Purpose: Preload the string on the client (if single player it should already be in the cache from the server!!!)
// Input  : *object - 
//			*stringTable - 
//			stringNumber - 
//			*newString - 
//			*newData - 
//-----------------------------------------------------------------------------
void OnSceneStringTableChanged( void *object, INetworkStringTable *stringTable, int stringNumber, const char *newString, void const *newData )
{
}

//-----------------------------------------------------------------------------
// Purpose: Hook up any callbacks here, the table definition has been parsed but 
//  no data has been added yet
//-----------------------------------------------------------------------------
void CHLClient::InstallStringTableCallback( const char *tableName )
{
	// Here, cache off string table IDs
	if (!V_strcasecmp(tableName, "Materials"))
	{
		// Look up the id 
		g_pStringTableMaterials = networkstringtable->FindTable( tableName );

		// When the material list changes, we need to know immediately
		g_pStringTableMaterials->SetStringChangedCallback( nullptr, OnMaterialStringTableChanged );
	}
	else if ( !V_strcasecmp( tableName, "EffectDispatch" ) )
	{
		g_StringTableEffectDispatch = networkstringtable->FindTable( tableName );

		// When the material list changes, we need to know immediately
		g_StringTableEffectDispatch->SetStringChangedCallback( nullptr, OnEffectStringTableChanged );
	}
	else if ( !V_strcasecmp( tableName, "InfoPanel" ) )
	{
		g_pStringTableInfoPanel = networkstringtable->FindTable( tableName );
	}
	else if ( !V_strcasecmp( tableName, "Scenes" ) )
	{
		g_pStringTableClientSideChoreoScenes = networkstringtable->FindTable( tableName );
		networkstringtable->SetAllowClientSideAddString( g_pStringTableClientSideChoreoScenes, true );
		g_pStringTableClientSideChoreoScenes->SetStringChangedCallback( nullptr, OnSceneStringTableChanged );
	}
	else if ( !V_strcasecmp( tableName, "ParticleEffectNames" ) )
	{
		g_pStringTableParticleEffectNames = networkstringtable->FindTable( tableName );
		networkstringtable->SetAllowClientSideAddString( g_pStringTableParticleEffectNames, true );
		// When the particle system list changes, we need to know immediately
		g_pStringTableParticleEffectNames->SetStringChangedCallback( nullptr, OnParticleSystemStringTableChanged );
	}
	else if ( !V_strcasecmp( tableName, "ExtraParticleFilesTable" ) )
	{
		g_pStringTableExtraParticleFiles = networkstringtable->FindTable( tableName );
		networkstringtable->SetAllowClientSideAddString( g_pStringTableExtraParticleFiles, true );
		// When the particle system list changes, we need to know immediately
		g_pStringTableExtraParticleFiles->SetStringChangedCallback( nullptr, OnPrecacheParticleFile );
	}
	else if ( !V_strcasecmp( tableName, "Movies" ) )
	{
		// Look up the id 
		g_pStringTableMovies = networkstringtable->FindTable( tableName );

		// When the movie list changes, we need to know immediately
		g_pStringTableMovies->SetStringChangedCallback( nullptr, OnMovieStringTableChanged );
	}
	else
	{
		// Pass tablename to gamerules last if all other checks fail
		InstallStringTableCallback_GameRules( tableName );
	}
}

//-----------------------------------------------------------------------------
// Material precache
//-----------------------------------------------------------------------------
void CHLClient::PrecacheMaterial( const char *pMaterialName )
{
	ks::ScratchArena<> scratch;
	contract_assert( pMaterialName );

	int nLen = V_strlen( pMaterialName );
	char *pTempBuf = (char*)scratch.AllocBytes( nLen + 1 );
	memcpy( pTempBuf, pMaterialName, nLen + 1 );
	char *pFound = V_strstr( pTempBuf, ".vmt\0" );
	if ( pFound )
	{
		*pFound = 0;
	}
		
	IMaterial *pMaterial = materials->FindMaterial( pTempBuf, TEXTURE_GROUP_PRECACHED );
	if ( !IsErrorMaterial( pMaterial ) )
	{
		if ( m_CachedMaterials.insert( pMaterial ).second )
		{
			pMaterial->IncrementReferenceCount();
		}
	}
	else
	{
	}
}

void CHLClient::UncacheAllMaterials()
{
	for ( IMaterial *pMaterial : m_CachedMaterials )
	{
		pMaterial->DecrementReferenceCount();
	}
	m_CachedMaterials.clear();
}

//-----------------------------------------------------------------------------
// Movie precache
//-----------------------------------------------------------------------------
void CHLClient::PrecacheMovie( const char *pMovieName )
{
	if ( m_CachedMovies.contains( pMovieName ) )
	{
		// already precached
		return;
	}
	
	// hint the movie system to precache our movie resource
	m_CachedMovies.intern( pMovieName );
}

void CHLClient::UncacheAllMovies()
{
	m_CachedMovies.clear();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pszName - 
//			iSize - 
//			*pbuf - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------

bool CHLClient::DispatchUserMessage( int msg_type, int32 nPassthroughFlags, int size, const void *msg )
{
	return UserMessages()->DispatchUserMessage( msg_type, nPassthroughFlags, size, msg );
}

void SimulateEntities()
{
	CVProfScope _( "Client SimulateEntities", 0, VPROF_BUDGETGROUP_CLIENT_SIM, false, BUDGETFLAG_OTHER );

	// Service timer events (think functions).
  	ClientThinkList()->PerformThinkFunctions();

	CBaseEntity::SimulateEntities();
}

bool AddDataChangeEvent( IClientNetworkable *ent, DataUpdateType_t updateType, int *pStoredEvent )
{
	CVProfScope _( "AddDataChangeEvent", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	contract_assert( ent );

	if ( updateType == DATA_UPDATE_POST_UPDATE )
	{
		contract_assert( g_bDataChangedPostEventsAllowed );
		*pStoredEvent = (int)g_DataChangedPostEvents.size();
		g_DataChangedPostEvents.emplace_back( ent, updateType, pStoredEvent );
		return true;
	}

	// Make sure we don't already have an event queued for this guy.
	if ( *pStoredEvent >= 0 )
	{
		if ( uint( *pStoredEvent ) < uint( g_DataChangedEvents.size() ) )
		{

			// DATA_UPDATE_CREATED always overrides DATA_UPDATE_CHANGED.
			if ( updateType == DATA_UPDATE_CREATED )
				g_DataChangedEvents[ *pStoredEvent ].m_UpdateType = updateType;
		}
	
		return false;
	}
	else
	{
		*pStoredEvent = (int)g_DataChangedEvents.size();
		g_DataChangedEvents.emplace_back( ent, updateType, pStoredEvent );
		return true;
	}
}

void ClearDataChangedEvent( int iStoredEvent )
{
	if ( iStoredEvent != -1 && size_t( iStoredEvent ) < g_DataChangedEvents.size() )
	{
		CDataChangedEvent &event = g_DataChangedEvents[iStoredEvent];
		event.m_nEntityIndex = -1;
		event.m_pStoredEvent = nullptr;
	}
}

void ProcessOnDataChangedEvents()
{
	CVProfScope _( "ProcessOnDataChangedEvents", 1, VPROF_BUDGETGROUP_CLIENT_SIM, false, BUDGETFLAG_CLIENT );
	int nSave = GET_ACTIVE_SPLITSCREEN_SLOT();
	bool bSaveAccess = engine->SetLocalPlayerIsResolvable( __FILE__, __LINE__, false );

	g_bDataChangedPostEventsAllowed = true;
	// Indexed: OnDataChanged may queue more events
	for ( size_t i = 0; i < g_DataChangedEvents.size(); i++ )
	{
		CDataChangedEvent *pEvent = &g_DataChangedEvents[i];
		if ( !pEvent->m_pStoredEvent )
			continue;

		// Reset their stored event identifier.		
		*pEvent->m_pStoredEvent = -1;

		IClientNetworkable *pEntity = pEvent->GetEntity();
		if ( pEntity )
		{
			// Send the event.
			pEntity->OnDataChanged( pEvent->m_UpdateType );
		}
	}
	g_bDataChangedPostEventsAllowed = false;

	for ( size_t i = 0; i < g_DataChangedPostEvents.size(); i++ )
	{
		CDataChangedEvent *pEvent = &g_DataChangedPostEvents[ i ];

		// Reset their stored event identifier.		
		*pEvent->m_pStoredEvent = -1;

		IClientNetworkable *pEntity = pEvent->GetEntity();
		if ( pEntity )
		{
			// Send the event.
			pEntity->OnDataChanged( pEvent->m_UpdateType );
		}
	}

	//now run through the list again and initialize any predictables that are ready
	for ( size_t i = 0; i < g_DataChangedEvents.size(); i++ )
	{
		IClientNetworkable *pNetworkable = g_DataChangedEvents[i].GetEntity();
		if ( !pNetworkable )
			continue;
		
		IClientUnknown *pUnk = pNetworkable->GetIClientUnknown();
		if ( !pUnk )
			continue;

		CBaseEntity *pEntity = EntityCast<CBaseEntity>( pUnk->GetBaseEntity() );
		if ( !pEntity )
			continue;

		pEntity->CheckInitPredictable( "ProcessOnDataChangedEvents()" );
	}

	g_DataChangedEvents.clear();
	g_DataChangedPostEvents.clear();

	engine->SetActiveSplitScreenPlayerSlot( nSave );
	engine->SetLocalPlayerIsResolvable( __FILE__, __LINE__, bSaveAccess );
}

void PurgeOnDataChangedEvents()
{
	if ( int nPurging = CountPendingDataChangedEvents() )
	{
		DevMsg( "Purging %d data change events without processing\n", nPurging );
	}
	g_DataChangedEvents.clear();
}

void UpdateClientRenderableInPVSStatus()
{
	// Vis for this view should already be setup at this point.

	// For each client-only entity, notify it if it's newly coming into the PVS.
	std::list<CClientEntityList::CPVSNotifyInfo> &theList = ClientEntityList().GetPVSNotifiers();
	for ( CClientEntityList::CPVSNotifyInfo &info : theList )
	{
		CClientEntityList::CPVSNotifyInfo *pInfo = &info;

		if ( pInfo->m_InPVSStatus & INPVS_YES )
		{
			// Ok, this entity already thinks it's in the PVS. No need to notify it.
			// We need to set the INPVS_YES_THISFRAME flag if it's in this frame at all, so we 
			// don't tell the entity it's not in the PVS anymore at the end of the frame.
			if ( !( pInfo->m_InPVSStatus & INPVS_THISFRAME ) )
			{
				if ( g_pClientLeafSystem->IsRenderableInPVS( pInfo->m_pRenderable ) )
				{
					pInfo->m_InPVSStatus |= INPVS_THISFRAME;
				}
			}
		}
		else
		{
			// This entity doesn't think it's in the PVS yet. If it is now in the PVS, let it know.
			if ( g_pClientLeafSystem->IsRenderableInPVS( pInfo->m_pRenderable ) )
			{
				pInfo->m_InPVSStatus |= ( INPVS_YES | INPVS_THISFRAME | INPVS_NEEDSNOTIFY );
			}
		}
	}	
}

void UpdatePVSNotifiers()
{
	MDLCACHE_CRITICAL_SECTION();

	// At this point, all the entities that were rendered in the previous frame have INPVS_THISFRAME set
	// so we can tell the entities that aren't in the PVS anymore so.
	std::list<CClientEntityList::CPVSNotifyInfo> &theList = ClientEntityList().GetPVSNotifiers();
	for ( CClientEntityList::CPVSNotifyInfo &info : theList )
	{
		CClientEntityList::CPVSNotifyInfo *pInfo = &info;

		// If this entity thinks it's in the PVS, but it wasn't in the PVS this frame, tell it so.
		if ( pInfo->m_InPVSStatus & INPVS_YES )
		{
			if ( pInfo->m_InPVSStatus & INPVS_THISFRAME )
			{
				if ( pInfo->m_InPVSStatus & INPVS_NEEDSNOTIFY )
				{
					pInfo->m_pNotify->OnPVSStatusChanged( true );
				}
				// Clear it for the next time around.
				pInfo->m_InPVSStatus &= ~( INPVS_THISFRAME | INPVS_NEEDSNOTIFY );
			}
			else
			{
				pInfo->m_InPVSStatus &= ~INPVS_YES;
				pInfo->m_pNotify->OnPVSStatusChanged( false );
			}
		}
	}
}

void OnRenderStart()
{
	CMatRenderContextPtr pRenderContext( materials );

	( pRenderContext, "OnRenderStart" );
	CVProfScope _( "OnRenderStart", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	MDLCACHE_CRITICAL_SECTION();
	MDLCACHE_COARSE_LOCK();

	// [mariod] - testing, see note in c_baseanimating.cpp
	CBaseAnimating::EnableInvalidateBoneCache( true );

	::partition->SuppressLists( PARTITION_ALL_CLIENT_EDICTS, true );
	CBaseEntity::SetAbsQueriesValid( false );

	Rope_ResetCounters();

	// Interpolate server entities and move aiments.
	{
		PREDICTION_TRACKVALUECHANGESCOPE( "interpolation" );
		CBaseEntity::InterpolateServerEntities();
	}

	{
		// vprof node for this bloc of math
		CVProfScope _( "OnRenderStart: dirty bone caches", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
		// Invalidate any bone information.
		CBaseAnimating::InvalidateBoneCaches();
		CBaseFlex::InvalidateFlexCaches();

		CBaseEntity::SetAbsQueriesValid( true );
		CBaseEntity::EnableAbsRecomputations( true );

		// Enable access to all model bones except view models.
		// This is necessary for aim-ent computation to occur properly
		CBaseAnimating::PushAllowBoneAccess( true, false, "OnRenderStart->CViewRender::SetUpView" ); // pops in CViewRender::SetUpView

		// FIXME: This needs to be done before the player moves; it forces
		// aiments the player may be attached to to forcibly update their position
		CBaseEntity::MarkAimEntsDirty();
	}

	// Make sure the camera simulation happens before OnRenderStart, where it's used.
	// NOTE: the only thing that happens in CAM_Think is thirdperson related code.
	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		input->CAM_Think();
	}

	CBaseAnimating::PopBoneAccess( "OnRenderStart->CViewRender::SetUpView" ); // pops the (true, false) bone access set in OnRenderStart

	// Enable access to all model bones until rendering is done
	CBaseAnimating::PushAllowBoneAccess( true, true, "CViewRender::SetUpView->OnRenderEnd" ); // pop is in OnRenderEnd()

	// This will place all entities in the correct position in world space and in the KD-tree
	// NOTE: Doing this before view->OnRenderStart() because the player can be in hierarchy with
	// a client-side animated entity.  So the viewport position is dependent on this animation sometimes.
	CBaseAnimating::UpdateClientSideAnimations();

	// This will place the player + the view models + all parent
	// entities	at the correct abs position so that their attachment points
	// are at the correct location
	view->OnRenderStart();
	::partition->SuppressLists( PARTITION_ALL_CLIENT_EDICTS, false );

	// Process OnDataChanged events.
	ProcessOnDataChangedEvents();

	// Reset the overlay alpha. Entities can change the state of this in their think functions.
	g_SmokeFogOverlayAlpha = 0;	

	g_SmokeFogOverlayColor.Init( 0.0f, 0.0f, 0.0f );

	{
		// Simulate all the entities.
		( pRenderContext, "SimulateEntities" );
		SimulateEntities();
	}

	{
		( pRenderContext, "PhysicsSimulate" );
		PhysicsSimulate();
	}

	CBaseAnimating::ThreadedBoneSetup();

	{
		CVProfScope _( "Client TempEnts", 0, VPROF_BUDGETGROUP_CLIENT_SIM, false, BUDGETFLAG_CLIENT );
		// This creates things like temp entities.
		engine->FireEvents();

		// Update temp entities
		tempents->Update();

		// Update temp ent beams...
		beams->UpdateTempEntBeams();
		
		// Lock the frame from beam additions
		SetBeamCreationAllowed( false );
	}

	// Update particle effects (eventually, the effects should use Simulate() instead of having
	// their own update system).
	{
		CVProfScope _( "ParticleMgr()->Simulate", 0, VPROF_BUDGETGROUP_PARTICLE_SIMULATION, false, BUDGETFLAG_OTHER );
		ParticleMgr()->Simulate( gpGlobals->frametime );
	}

	// Now that the view model's position is setup and aiments are marked dirty, update
	// their positions so they're in the leaf system correctly.
	CBaseEntity::CalcAimEntPositions();

	// update dynamic light state. Necessary for light cache to work properly for d- and elights
	engine->UpdateDAndELights();

	// Finally, link all the entities into the leaf system right before rendering.
	CBaseEntity::AddVisibleEntities();

	g_pClientLeafSystem->RecomputeRenderableLeaves();
	g_pClientShadowMgr->ReprojectShadows();
	g_pClientShadowMgr->AdvanceFrame();
	g_pClientLeafSystem->DisableLeafReinsertion( true );
}

void OnRenderEnd()
{
	g_pClientLeafSystem->DisableLeafReinsertion( false );

	// Disallow access to bones (access is enabled in CViewRender::SetUpView).
	CBaseAnimating::PopBoneAccess( "CViewRender::SetUpView->OnRenderEnd" );
	
	// [mariod] - testing, see note in c_baseanimating.cpp
	CBaseAnimating::EnableInvalidateBoneCache( false );

	UpdatePVSNotifiers();

	DisplayBoneSetupEnts();
}

void CHLClient::FrameStageNotify( ClientFrameStage_t curStage )
{
	g_CurFrameStage = curStage;
	g_bEngineIsHLTV = engine->IsHLTV();

	switch( curStage )
	{
	default:
		break;

	case FRAME_RENDER_START:
		{
			CVProfScope _( "CHLClient::FrameStageNotify FRAME_RENDER_START", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

			engine->SetLocalPlayerIsResolvable( __FILE__, __LINE__, false );

			// Last thing before rendering, run simulation.
			OnRenderStart();
		}
		break;
		
	case FRAME_RENDER_END:
		{
			CVProfScope _( "CHLClient::FrameStageNotify FRAME_RENDER_END", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
			OnRenderEnd();

			engine->SetLocalPlayerIsResolvable( __FILE__, __LINE__, false );

			PREDICTION_SPEWVALUECHANGES();
		}
		break;
		
	case FRAME_NET_FULL_FRAME_UPDATE_ON_REMOVE:
		{
			ParticleMgr()->RemoveAllNewEffects(); // new effects linger past entity recreation, creating a flurry of asserts, that's not good
			contract_assert( CountPendingDataChangedEvents() == 0 );	// this may only happen when we didn't render anything between getting an update and then a full frame update 
			// NOTE: There are no entities at this point if it's a full frame update! 
			PurgeOnDataChangedEvents();
		}
		break;

	case FRAME_NET_UPDATE_START:
		{
			CVProfScope _( "CHLClient::FrameStageNotify FRAME_NET_UPDATE_START", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
			// disabled all recomputations while we update entities
			CBaseEntity::EnableAbsRecomputations( false );
			CBaseEntity::SetAbsQueriesValid( false );
			Interpolation_SetLastPacketTimeStamp( engine->GetLastTimeStamp() );
			::partition->SuppressLists( PARTITION_ALL_CLIENT_EDICTS, true );

			PREDICTION_STARTTRACKVALUE( "netupdate" );
		}
		break;
	case FRAME_NET_UPDATE_END:
		{
			ProcessCacheUsedMaterials();

			// reenable abs recomputation since now all entities have been updated
			CBaseEntity::EnableAbsRecomputations( true );
			CBaseEntity::SetAbsQueriesValid( true );
			::partition->SuppressLists( PARTITION_ALL_CLIENT_EDICTS, false );

			PREDICTION_ENDTRACKVALUE();
		}
		break;
	case FRAME_NET_UPDATE_POSTDATAUPDATE_START:
		{
			CVProfScope _( "CHLClient::FrameStageNotify FRAME_NET_UPDATE_POSTDATAUPDATE_START", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
			PREDICTION_STARTTRACKVALUE( "postdataupdate" );
		}
		break;
	case FRAME_NET_UPDATE_POSTDATAUPDATE_END:
		{
			if ( !CBasePlayer::GetLocalPlayer( 0 ) )
			{
				// this is only possible when we're playing back the past, when our killer hasn't been spawned yet
				if ( CBasePlayer *pPlayer = UTIL_PlayerByIndex( engine->GetLocalPlayer() ) )
				{
					pPlayer->SetAsLocalPlayer();
				}
				else
				{
					Warning( "No local player %d after full frame update\n", engine->GetLocalPlayer() );
					for ( int i = 0; i < MAX_PLAYERS; ++i )
						if ( CBasePlayer*p = UTIL_PlayerByIndex( i ) )
						{
							Msg( "Setting fallback player %s as local player\n", p->GetPlayerName() );
							p->SetAsLocalPlayer();
							break;
						}
				}
			}
			CVProfScope _( "CHLClient::FrameStageNotify FRAME_NET_UPDATE_POSTDATAUPDATE_END", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
			PREDICTION_ENDTRACKVALUE();
			// Let prediction copy off pristine data
			prediction->PostEntityPacketReceived();
			HLTVCamera()->PostEntityPacketReceived();
		}
		break;
	case FRAME_START:
		{
			// Mark the frame as open for client fx additions
			SetFXCreationAllowed( true );
			SetBeamCreationAllowed( true );
			CBaseEntity::CheckCLInterpChanged();
			engine->SetLocalPlayerIsResolvable( __FILE__, __LINE__, false );
			if ( !g_bEngineIsHLTV )
			{
				// if we stopped Hltv Replay abruptly for some reason (maybe an issue with the connection to the server) - just reset the Hltv Replay state
				g_HltvReplaySystem.StopHltvReplay();
			}
			g_HltvReplaySystem.Update();
		}
		break;
	}
}

static std::vector< EHANDLE > g_RestoredEntities;

void AddRestoredEntity( CBaseEntity *pEntity )
{
	if ( !pEntity )
		return;

	g_RestoredEntities.push_back( pEntity );
}

void CHLClient::DispatchOnRestore()
{
	for ( int i = 0; i < std::ssize( g_RestoredEntities ); i++ )
	{
		if ( g_RestoredEntities[i] != nullptr )
		{
			MDLCACHE_CRITICAL_SECTION();
			g_RestoredEntities[i]->OnRestore();
		}
	}
	g_RestoredEntities.clear();
}

void CHLClient::WriteSaveGameScreenshot( const char *pFilename )
{
	// Single player doesn't support split screen yet!!!
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( 0 );
	view->WriteSaveGameScreenshot( pFilename );
}

// Given a list of "S(wavname) S(wavname2)" tokens, look up the localized text and emit
//  the appropriate close caption if running with closecaption = 1
void CHLClient::EmitSentenceCloseCaption( char const *tokenstream )
{
}

void CHLClient::EmitCloseCaption( char const *captionname, float duration )
{
}

CStandardRecvProxies* CHLClient::GetStandardRecvProxies()
{
	return &g_StandardRecvProxies;
}

bool CHLClient::CanRecordDemo( char *errorMsg, int length ) const
{
	if ( GetClientModeNormal() )
	{
		return GetClientModeNormal()->CanRecordDemo( errorMsg, length );
	}

	return true;
}

bool CHLClient::CanStopRecordDemo( char *errorMsg, int length ) const
{
	if ( CSGameRules() )
	{
		if ( CSGameRules()->IsWarmupPeriod() )
			return true;	// can always stop in warmup

		if ( CSGameRules()->IsRoundOver() )
			return true;	// can stop when round is over

		if ( CSGameRules()->IsPlayingGunGameProgressive() )
			return true;	// can always stop during arms race

		if ( CSGameRules()->IsPlayingGunGameDeathmatch() )
			return true;	// can always stop during deathmatch

		if ( engine->IsClientLocalToActiveServer() )
			return true;	// listen server clients can always stop whenever

		V_strncpy( errorMsg, "Demo recording will stop as soon as the round is over.", length );
		CSGameRules()->MarkClientStopRecordAtRoundEnd( true );
		return false;
	}

	return true;
}

void CHLClient::OnDemoRecordStart( char const* pDemoBaseName )
{

	if ( CSGameRules() )
	{
		// If client was previously marked to stop recording at round end then mark it now as not requiring to stop
		CSGameRules()->MarkClientStopRecordAtRoundEnd( false );
	}
}

void CHLClient::OnDemoRecordStop()
{
}

void CHLClient::OnDemoPlaybackStart( char const* pDemoBaseName )
{

	KeyValues *pImportantEvents = new KeyValues( "DemoImportantEvents" );
	if ( pImportantEvents )
	{
		pImportantEvents->LoadFromFile( g_pFullFileSystem, "resource/DemoImportantEvents.res" );
		engine->SetDemoImportantEventData( pImportantEvents );
		pImportantEvents->deleteThis();
	}
	g_HltvReplaySystem.OnDemoPlayback( true );
}

void CHLClient::OnDemoPlaybackRestart()
{
	/* Removed for partner depot */
}

void CHLClient::OnDemoPlaybackStop()
{

	g_HltvReplaySystem.OnDemoPlayback( false );
}

void CHLClient::SetDemoPlaybackHighlightXuid( uint64 xuid, bool bLowlights )
{
	g_HltvReplaySystem.SetDemoPlaybackHighlightXuid( xuid, bLowlights );
}

void CHLClient::ShowHighlightSkippingMessage( bool bState, int nCurrentTick, int nTickStart, int nTickStop )
{
	g_HltvReplaySystem.SetDemoPlaybackFadeBrackets( nCurrentTick, nTickStart, nTickStop );
}

void CHLClient::RecordDemoPolishUserInput( int nCmdIndex )
{
}

bool CHLClient::CacheReplayRagdolls( const char* pFilename, int nStartTick )
{
	return false;
}

void CHLClient::ReplayUI_SendMessage( KeyValues *pMsg )
{
}

// Get the client replay factory
IReplayFactory *CHLClient::GetReplayFactory()
{
	return nullptr;
}

// Clear out the local player's replay pointer so it doesn't get deleted
void CHLClient::ClearLocalPlayerReplayPtr()
{
}

int CHLClient::GetScreenWidth()
{
	return ScreenWidth();
}

int CHLClient::GetScreenHeight()
{
	return ScreenHeight();
}

void CHLClient::WriteSaveGameScreenshotOfSize( const char *pFilename, int width, int height, bool bCreatePowerOf2Padded /* = false */, bool bWriteVTF /* = false */ )
{
	view->WriteSaveGameScreenshotOfSize( pFilename, width, height /*, bCreatePowerOf2Padded, bWriteVTF */ );
}

void CHLClient::WriteReplayScreenshot( WriteReplayScreenshotParams_t &params )
{
}

void CHLClient::UpdateReplayScreenshotCache()
{
}

// See RenderViewInfo_t
void CHLClient::RenderView( const CViewSetup &setup, int nClearFlags, int whatToDraw )
{
	CVProfScope _( "RenderView", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	view->RenderView( setup, setup, nClearFlags, whatToDraw );
}

bool CHLClient::ShouldHideLoadingPlaque( void )
{
	return false;

}

void CHLClient::OnActiveSplitscreenPlayerChanged( int nNewSlot )
{
}

void CHLClient::OnSplitScreenStateChanged()
{
	UI_OnSplitScreenStateChanged();
	IterateRemoteSplitScreenViewSlots_Push( true );
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( i )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD_UI( i );
		GetClientMode()->Layout();
		GetHud().OnSplitScreenStateChanged();
	}
	IterateRemoteSplitScreenViewSlots_Pop();

	GetFullscreenClientMode()->Layout( true );

	// Update visibility for all ents so that the second viewport for the split player guy looks right, etc.
	CBaseEntityIterator iterator;
	CBaseEntity *pEnt;
	while ( (pEnt = iterator.Next()) != nullptr )	
	{
		pEnt->UpdateVisibility();
	}
}

int CHLClient::GetSpectatorTarget( ClientDLLObserverMode_t* pObserverMode )
{
	if ( pObserverMode )
	{
		*pObserverMode = CLIENT_DLL_OBSERVER_NONE;
	}

	CCSPlayer *pPlayer = GetLocalOrInEyeCSPlayer();

	if ( pPlayer != nullptr )
	{
		return pPlayer->entindex();
	}

	return -1;
}

void CHLClient::CenterStringOff()
{
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( i )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( i );
		GetCenterPrint()->Clear();
	}

}

void CHLClient::OnScreenSizeChanged( int nOldWidth, int nOldHeight )
{
	// Tell split screen system
	UI_OnScreenSizeChanged();
}

IMaterialProxy *CHLClient::InstantiateMaterialProxy( const char *proxyName )
{
	return GetMaterialProxyDict().CreateProxy( proxyName );
}

// XBX_GetActiveUserId defined in platform.h

//-----------------------------------------------------------------------------
// Purpose: Marks entities as touching
// Input  : *e1 - 
//			*e2 - 
//-----------------------------------------------------------------------------
void CHLClient::MarkEntitiesAsTouching( IClientEntity *e1, IClientEntity *e2 )
{
	CBaseEntity *entity = EntityCast<CBaseEntity>( e1->GetBaseEntity() );
	CBaseEntity *entityTouched = EntityCast<CBaseEntity>( e2->GetBaseEntity() );
	if ( entity && entityTouched )
	{
		trace_t tr;
		UTIL_ClearTrace( tr );
		tr.endpos = (entity->GetAbsOrigin() + entityTouched->GetAbsOrigin()) * 0.5;
		entity->PhysicsMarkEntitiesAsTouching( entityTouched, tr );
	}
}

class CKeyBindingListenerMgr : public IKeyBindingListenerMgr
{
public:
	struct BindingListeners_t
	{
		BindingListeners_t()
		{
		}

		BindingListeners_t( const BindingListeners_t &other )
		{
			m_List.assign( other.m_List.begin(), other.m_List.end() );
		}

		std::vector< IKeyBindingListener * > m_List;
	};

	// Callback when button is bound
	virtual void AddListenerForCode( IKeyBindingListener *pListener, ButtonCode_t buttonCode )
	{
		std::vector< IKeyBindingListener * > &list = m_CodeListeners[ buttonCode ];
		if ( ks::find_index( list, pListener ) != -1 )
			return;
		list.push_back( pListener );
	}

	// Callback whenver binding is set to a button
	virtual void AddListenerForBinding( IKeyBindingListener *pListener, char const *pchBindingString )
	{
		std::vector< IKeyBindingListener * > &list = m_BindingListeners[ pchBindingString ].m_List;
		if ( ks::find_index( list, pListener ) != -1 )
			return;
		list.push_back( pListener );
	}

	virtual void RemoveListener( IKeyBindingListener *pListener )
	{
		for ( int i = 0; i < std::size( m_CodeListeners ); ++i )
		{
			std::vector< IKeyBindingListener * > &list = m_CodeListeners[ i ];
			ks::erase_first( list, pListener );
		}

		for ( auto &[binding, listeners] : m_BindingListeners )
		{
			std::vector< IKeyBindingListener * > &list = listeners.m_List;
			ks::erase_first( list, pListener );
		}
	}

	void OnKeyBindingChanged( ButtonCode_t buttonCode, char const *pchKeyName, char const *pchNewBinding )
	{
		int nSplitScreenSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

		std::vector< IKeyBindingListener * > &list = m_CodeListeners[ buttonCode ];
		for ( int i = 0 ; i < std::ssize( list ); ++i )
		{
			list[ i ]->OnKeyBindingChanged( nSplitScreenSlot, buttonCode, pchKeyName, pchNewBinding );
		}

		auto idx = m_BindingListeners.find( pchNewBinding );
		if ( idx != m_BindingListeners.end() )
		{
			std::vector< IKeyBindingListener * > &list = idx->second.m_List;
			for ( int i = 0 ; i < std::ssize( list ); ++i )
			{
				list[ i ]->OnKeyBindingChanged( nSplitScreenSlot, buttonCode, pchKeyName, pchNewBinding );
			}
		}
	}

private:
	std::vector< IKeyBindingListener * > m_CodeListeners[ BUTTON_CODE_COUNT ];
	ks::idict< BindingListeners_t > m_BindingListeners;
};

static CKeyBindingListenerMgr g_KeyBindingListenerMgr;

IKeyBindingListenerMgr *g_pKeyBindingListenerMgr = &g_KeyBindingListenerMgr;
void CHLClient::OnKeyBindingChanged( ButtonCode_t buttonCode, char const *pchKeyName, char const *pchNewBinding )
{
	g_KeyBindingListenerMgr.OnKeyBindingChanged( buttonCode, pchKeyName, pchNewBinding );
}

void CHLClient::SetBlurFade( float scale )
{
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		GetClientMode()->SetBlurFade( scale );
	}
}

void CHLClient::ResetHudCloseCaption()
{
}

void CHLClient::Hud_SaveStarted()
{
}

void CHLClient::ShutdownMovies()
{
	//UI_StopAllVideoPanels();
}

void CHLClient::GetStatus( char *buffer, int bufsize )
{
	UTIL_GetClientStatusText( buffer, bufsize );
}

bool CHLClient::IsChatRaised( void )
{
	return false;
}

bool CHLClient::IsRadioPanelRaised( void )
{
	return false;
}

bool CHLClient::IsBindMenuRaised( void )
{
    return false;
}

bool CHLClient::IsTeamMenuRaised( void )
{
	if ( !GetViewPortInterface() )
	{
		return false;
	}

	IViewPortPanel * pTeam = GetViewPortInterface()->FindPanelByName( PANEL_TEAM );
	if ( pTeam && pTeam->IsVisible() )
	{
		return true;
	}

	return false;
}

bool CHLClient::IsLoadingScreenRaised( void )
{
	return false;
}

bool CHLClient::IsBuildWRThreaded( void )
{
	return g_viewBuilder.GetBuildWRThreaded();
}

void CHLClient::QueueBuildWorldListJob( CJob* pJob )
{
	g_viewBuilder.QueueBuildWorldListJob( pJob );
}

void CHLClient::CacheFrustumData( const Frustum_t& frustum, const std::vector< Frustum_t >& aeraFrustums )
{
	g_viewBuilder.CacheFrustumData( frustum, aeraFrustums );
}

const Frustum_t* CHLClient::GetBuildViewFrustum( void ) const
{
	return g_viewBuilder.GetBuildViewFrustum();
}

const std::vector< Frustum_t >* CHLClient::GetBuildViewAeraFrustums( void ) const
{
	return g_viewBuilder.GetBuildViewAeraFrustums();
}

void CHLClient::RecordUIEvent( const char* szEvent )
{
	/* Removed for partner depot */
}

void CHLClient::OnDemoPlaybackTimeJump()
{
	if ( CHLTVCamera *pCamera = HLTVCamera() )
	{
		//Cbuf_AddText( Cbuf_GetCurrentPlayer(), "spec_prev;spec_mode 4;+showscores;-showscores\n" ); // go to the next/prev available player; just go to some player, if they've already connected
		pCamera->SetMode( 4 );
		pCamera->SpecNextPlayer( true );
	}
}

// Inventory access
float CHLClient::FindInventoryItemWithMaxAttributeValue( char const *szItemType, char const *szAttrClass )
{
	CCSPlayerInventory *pLocalInv = CSInventoryManager()->GetLocalCSInventory();
	return pLocalInv ? pLocalInv->FindInventoryItemWithMaxAttributeValue( szItemType, szAttrClass ) : -1.0f;
}

int CHLClient::GetInEyeEntity() const
{
	CCSPlayer* player = GetLocalOrInEyeCSPlayer();
	if (player != nullptr)
	{
		return player->entindex();
	}
	return -1;
}

bool CHLClient::ValidateSignedEvidenceHeader( char const *szKey, void const *pvHeader, CDemoPlaybackParameters_t *pPlaybackParameters )
{
	/* Removed for partner depot */
	return true;
}

void CHLClient::PrepareSignedEvidenceData( void *pvData, int numBytes, CDemoPlaybackParameters_t const *pPlaybackParameters )
{
	/* Removed for partner depot */
}

bool CHLClient::ShouldSkipEvidencePlayback( CDemoPlaybackParameters_t const *pPlaybackParameters )
{
	/* Removed for partner depot */
	return true;
}

bool CHLClient::IsConnectedUserInfoChangeAllowed( ConVar *pCvar )
{
	return CSGameRules() ? CSGameRules()->IsConnectedUserInfoChangeAllowed( nullptr ) : true;
}

void CHLClient::OnCommandDuringPlayback( char const *cmd )
{
	/* Removed for partner depot */
}

extern void OnPlayerDecalsLevelShutdown();
extern void OnPlayerDecalsUpdate();
void CHLClient::RetireAllPlayerDecals( bool bRenderContextValid )
{
	OnPlayerDecalsLevelShutdown();

	if ( bRenderContextValid )
	{
		// If the render context is valid (i.e. manual r_cleardecals)
		// then we should immediately update and reapply to avoid flickers
		OnPlayerDecalsUpdate();
	}
}

void CHLClient::EngineGotvSyncPacket( const ks::net::CEngineGotvSyncPacket *pPkt )
{
	/* Removed for partner depot */
}

void CHLClient::OnTickPre( int tickcount )
{
}

extern IViewRender *view;

class CEffectData;
class CBaseEntity;
class CBasePlayer;
class CCSPlayer;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// ConVars
//-----------------------------------------------------------------------------
bool g_MakingDevShots = false;
extern ConVar cl_leveloverview;

//-----------------------------------------------------------------------------
// Purpose: Performs a var args printf into a static return buffer
// Input  : *format - 
//			... - 
// Output : char
//-----------------------------------------------------------------------------
char *VarArgs( const char *format, ... )
{
	va_list		argptr;
	static char		string[1024];
	
	__builtin_va_start (argptr, format);
	V_vsnprintf (string, sizeof( string ), format,argptr);
	__builtin_va_end (argptr);

	return string;	
}
	
//-----------------------------------------------------------------------------
// Purpose: Returns true if the entity index corresponds to a player slot 
// Input  : index - 
// Output : bool
//-----------------------------------------------------------------------------
bool IsPlayerIndex( int index )
{
	return ( index >= 1 && index <= gpGlobals->maxClients ) ? true : false;
}

int GetLocalPlayerIndex( void )
{
	CBasePlayer * player = CBasePlayer::GetLocalPlayer();

	if ( player )
		return player->entindex();
	else
		return  0;	// game not started yet
}

bool IsLocalPlayerSpectator( void )
{
	CBasePlayer * player = CBasePlayer::GetLocalPlayer();

	if ( player )
		return player->IsObserver();
	else
		return false;	// game not started yet
}

int GetSpectatorMode( void )
{
	CBasePlayer * player = CBasePlayer::GetLocalPlayer();

	if ( player )
		return player->GetObserverMode();
	else
		return OBS_MODE_NONE;	// game not started yet
}

int GetSpectatorTarget( void )
{
	CBasePlayer * player = CBasePlayer::GetLocalPlayer();

	if ( player )
	{
		CBaseEntity * target = player->GetObserverTarget();

		if ( target )
			return target->entindex();
		else
			return 0;
	}
	else
	{
		return  0;	// game not started yet
	}
}

// this is meant to be called on a bot character
// note: This is quite similar to CCSPlayer::CanControlBot.  This is here since calling into the CCSPlayer was unrealistic in some code paths
// TODO: Fix this so it ACTUALLY calls CanControlBot, there are bugs otherwise (and will be more) do to them being different functions
bool CanControlSpectatedTarget( void )
{
	CCSPlayer * player = ( CCSPlayer* )CBasePlayer::GetLocalPlayer();

    if ( !player || ( player->GetPendingTeamNumber() != player->GetTeamNumber() ) )
        return false;

	CCSPlayer * target = dynamic_cast<CCSPlayer*>(player->GetObserverTarget());
	if ( !target )
		return false;

	if ( !target->IsAlive() )
		return false;
		
	if ( !target->IsOtherSameTeam( GetLocalPlayerTeam() ) || target->IsOtherEnemy( player ) )
		return false;

	CCS_PlayerResource *pCSPR = ( CCS_PlayerResource* )GameResources();
	int entIndex = target->entindex();
		
	bool targetIsFakePlayer = pCSPR->IsFakePlayer( entIndex );
	if ( !targetIsFakePlayer )
		return false;

	contract_assert( pCSPR && "Expected PlayerResources to exsist" );
	contract_assert( ( entIndex > 0 && entIndex <= MAX_PLAYERS ) && "Bad entity index for player" );

	// need to duplicate some of the checks that CanControlBot already does so they match up....	
	// Can't control a bot that is setting a bomb
	const CC4 *pC4 = dynamic_cast<CC4*>( target->GetActiveWeapon() );
	if ( pC4 && pC4->m_bStartedArming )
		return false;

	if ( CSGameRules() && CSGameRules()->IsRoundOver() ) 
		return false;

	if ( CSGameRules() && CSGameRules()->IsFreezePeriod() )
		return false;

	return  true;	
}

bool CanSeeSpectatorOnlyTools( void )
{
	CBasePlayer *pPlayer = CBasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return false;

	if ( pPlayer->IsHLTV() )
		return true;

	if ( pPlayer->IsSpectator() )
	{
		if ( sv_competitive_official_5v5.GetBool() )
			return true;

		if ( CSGameRules() && CSGameRules()->IsQueuedMatchmaking() )
			return true;
	}
		
	return false;
}

bool CanToggleXRayView( void )
{
	CBasePlayer *pPlayer = CBasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return false;

	if ( CanSeeSpectatorOnlyTools() )
		return true;

	return false;
}

int GetLocalPlayerTeam( void ) 
{ 
	CBasePlayer *pPlayer = CBasePlayer::GetLocalPlayer();
	
	if ( pPlayer )
		return pPlayer->GetTeamNumber(); 
	else
		return TEAM_UNASSIGNED;
}

//-----------------------------------------------------------------------------
// Purpose: Convert angles to -180 t 180 range
// Input  : angles - 
//-----------------------------------------------------------------------------
void NormalizeAngles( QAngle& angles )
{
	int i;
	
	// Normalize angles to -180 to 180 range
	for ( i = 0; i < 3; i++ )
	{
		if ( angles[i] > 180.0 )
		{
			angles[i] -= 360.0;
		}
		else if ( angles[i] < -180.0 )
		{
			angles[i] += 360.0;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Interpolate Euler angles using quaternions to avoid singularities
// Input  : start - 
//			end - 
//			output - 
//			frac - 
//-----------------------------------------------------------------------------
void InterpolateAngles( const QAngle& start, const QAngle& end, QAngle& output, float frac )
{
	Quaternion src, dest;

	// Convert to quaternions
	AngleQuaternion( start, src );
	AngleQuaternion( end, dest );

	Quaternion result;

	// Slerp
	QuaternionSlerp( src, dest, frac, result );

	// Convert to euler
	QuaternionAngles( result, output );
}

//-----------------------------------------------------------------------------
// Purpose: Simple linear interpolation
// Input  : frac - 
//			src - 
//			dest - 
//			output - 
//-----------------------------------------------------------------------------
void InterpolateVector( float frac, const Vector& src, const Vector& dest, Vector& output )
{
	int i;

	for ( i = 0; i < 3; i++ )
	{
		output[ i ] = src[ i ] + frac * ( dest[ i ] - src[ i ] );
	}
}

client_textmessage_t *TextMessageGet( const char *pName )
{ 
	return engine->TextMessageGet( pName );
}

//-----------------------------------------------------------------------------
// Purpose: ScreenHeight returns the height of the screen, in pixels
// Output : int
//-----------------------------------------------------------------------------
int ScreenHeight( void )
{
	int w, h;
	GetHudSize( w, h );
	return h;
}

//-----------------------------------------------------------------------------
// Purpose: ScreenWidth returns the width of the screen, in pixels
// Output : int
//-----------------------------------------------------------------------------
int ScreenWidth( void )
{
	int w, h;
	GetHudSize( w, h );
	return w;
}

//-----------------------------------------------------------------------------
// Purpose: Return the difference between two angles
// Input  : destAngle - 
//			srcAngle - 
// Output : float
//-----------------------------------------------------------------------------
float UTIL_AngleDiff( float destAngle, float srcAngle )
{
	float delta;

	delta = destAngle - srcAngle;
	if ( destAngle > srcAngle )
	{
		while ( delta >= 180 )
			delta -= 360;
	}
	else
	{
		while ( delta <= -180 )
			delta += 360;
	}
	return delta;
}

void UTIL_Bubbles( const Vector& mins, const Vector& maxs, int count )
{
	Vector mid =  (mins + maxs) * 0.5;

	float flHeight = UTIL_WaterLevel( mid,  mid.z, mid.z + 1024 );
	flHeight = flHeight - mins.z;

	CPASFilter filter( mid );

	int bubbles = modelinfo->GetModelIndex( "sprites/bubble.vmt" );

	te->Bubbles( filter, 0.0,
		&mins, &maxs, flHeight, bubbles, count, 8.0 );
}

void UTIL_ScreenShake( const Vector &center, float amplitude, float frequency, float duration, float radius, ShakeCommand_t eCommand, bool bAirShake )
{
	// Nothing for now
}

char TEXTURETYPE_Find( trace_t *ptr )
{
	surfacedata_t *psurfaceData = physprops->GetSurfaceData( ptr->surface.surfaceProps );

	return psurfaceData->game.material;
}

//-----------------------------------------------------------------------------
// Purpose: Make a tracer effect
//-----------------------------------------------------------------------------
void UTIL_Tracer( const Vector &vecStart, const Vector &vecEnd, int iEntIndex, int iAttachment, float flVelocity, bool bWhiz, char *pCustomTracerName )
{
	CEffectData data;
	data.m_vStart = vecStart;
	data.m_vOrigin = vecEnd;
	data.m_hEntity = ClientEntityList().EntIndexToHandle( iEntIndex );
	data.m_flScale = flVelocity;

	// Flags
	if ( bWhiz )
	{
		data.m_fFlags |= TRACER_FLAG_WHIZ;
	}
	if ( iAttachment != TRACER_DONT_USE_ATTACHMENT )
	{
		data.m_fFlags |= TRACER_FLAG_USEATTACHMENT;
		// Stomp the start, since it's not going to be used anyway
		data.m_vStart[0] = iAttachment;
	}

	// Fire it off
	if ( pCustomTracerName )
	{
		DispatchEffect( pCustomTracerName, data );
	}
	else
	{
		DispatchEffect( "Tracer", data );
	}
}

// Tried to include this in the actual trace function, but ran into
// linker issues.  Just dropped it here instead.  Michael Dorgan

static bool HasValidDirection(trace_t *pTrace)
{
	if(pTrace->fraction <= 0.0001f)
	{
		// Ok, there is a very, very strong chance our vectors are out of wack
		// Add a direct check against the actual start and end points to be sure.
		if( VectorsAreEqual( pTrace->startpos, pTrace->endpos, 0.0001f ) )
		{
			return false;
		}
	}

	return true;
}

//------------------------------------------------------------------------------
// Purpose : Creates both an decal and any associated impact effects (such
//			 as flecks) for the given iDamageType and the trace's end position
// Input   :
// Output  :
//------------------------------------------------------------------------------
void UTIL_ImpactTrace( trace_t *pTrace, int iDamageType, const char *pCustomImpactName )
{
	CBaseEntity *pEntity = pTrace->Ent<CBaseEntity>();

	// Is the entity valid, is the surface sky?
	if ( !pEntity || (pTrace->surface.flags & SURF_SKY) )
		return;

	if (pTrace->fraction >= 1.0f)
		return;

	// Very, very, very close range shots cause all sorts of issues with normal and correct
	// decal orientation.  Just skip them to prevent heartache.
	if ( !HasValidDirection(pTrace) )
		return;

	// don't decal nodraw surfaces
	if ( pTrace->surface.flags & SURF_NODRAW )
		return;

	pEntity->ImpactTrace( pTrace, iDamageType, pCustomImpactName );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int UTIL_PrecacheDecal( const char *name, bool preload )
{
	return effects->Draw_DecalIndexFromName( (char*)name );
}

extern int g_sModelIndexSmoke;

void UTIL_Smoke( const Vector &origin, const float scale, const float framerate )
{
	CPVSFilter filter( origin );
	te->Smoke( filter, 0.0f, &origin, g_sModelIndexSmoke, scale, framerate );
}

void UTIL_SetOrigin( CBaseEntity *entity, const Vector &vecOrigin )
{
	entity->SetLocalOrigin( vecOrigin );
}

//#define PRECACHE_OTHER_ONCE
// UNDONE: Do we need this to avoid doing too much of this?  Measure startup times and see

void UTIL_PrecacheOther( const char *szClassname )
{

	// Client should only do this once entities are coming down from server!!!
	// Assert( engine->IsConnected() );

	CBaseEntity	*pEntity = CreateEntityByName( szClassname );
	if ( !pEntity )
	{
		Warning( "NULL Ent in UTIL_PrecacheOther\n" );
		return;
	}
	
	if (pEntity)
	{
		pEntity->Precache( );
	}

	// Bye bye
	UTIL_Remove( pEntity );
}

static csurface_t	g_NullSurface = { "**empty**", 0 };
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void UTIL_SetTrace(trace_t& trace, const Ray_t& ray, CBaseEntity *ent, float fraction, int hitgroup, unsigned int contents, const Vector& normal, float intercept )
{
	trace.startsolid = (fraction == 0.0f);
	trace.fraction = fraction;
	VectorCopy( ray.m_Start, trace.startpos );
	VectorMA( ray.m_Start, fraction, ray.m_Delta, trace.endpos );
	VectorCopy( normal, trace.plane.normal );
	trace.plane.dist = intercept;
	trace.m_pEnt = CBaseEntity::Instance( ent );
	trace.hitgroup = hitgroup;
	trace.surface =	g_NullSurface;
	trace.contents = contents;
}

//-----------------------------------------------------------------------------
// Purpose: Get the x & y positions of a world position in screenspace
//			Returns true if it's onscreen
//-----------------------------------------------------------------------------
bool GetVectorInScreenSpace( Vector pos, int& iX, int& iY, Vector *vecOffset )
{
	Vector screen;

	// Apply the offset, if one was specified
	if ( vecOffset != nullptr )
		pos += *vecOffset;

	// Transform to screen space
	int x, y, screenWidth, screenHeight;
	int insetX, insetY;
	UI_GetEngineRenderBounds( GET_ACTIVE_SPLITSCREEN_SLOT(), x, y, screenWidth, screenHeight, insetX, insetY );

	// Transform to screen space
	int iFacing = ScreenTransform( pos, screen );

	iX = 0.5 * screen[0] * screenWidth;
	iY = -0.5 * screen[1] * screenHeight;
	iX += 0.5 * screenWidth;
	iY += 0.5 * screenHeight;	

	iX += insetX;
	iY += insetY;

	// Make sure the player's facing it
	if ( iFacing )
	{
		// We're actually facing away from the Target. Stomp the screen position.
		iX = -640;
		iY = -640;
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Get the x & y positions of an entity in screenspace
//			Returns true if it's onscreen
//-----------------------------------------------------------------------------
bool GetTargetInScreenSpace( CBaseEntity *pTargetEntity, int& iX, int& iY, Vector *vecOffset )
{
	return GetVectorInScreenSpace( pTargetEntity->WorldSpaceCenter(), iX, iY, vecOffset );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *player - 
//			msg_dest - 
//			*msg_name - 
//			*param1 - 
//			*param2 - 
//			*param3 - 
//			*param4 - 
//-----------------------------------------------------------------------------
void ClientPrint( CBasePlayer *player, int msg_dest, const char *msg_name, const char *param1 /*= NULL*/, const char *param2 /*= NULL*/, const char *param3 /*= NULL*/, const char *param4 /*= NULL*/ )
{
}

//-----------------------------------------------------------------------------
// Purpose: Pass in an array of pointers and an array size, it fills the array and returns the number inserted
// Input  : **pList - 
//			listMax - 
//			&mins - 
//			&maxs - 
//			flagMask - 
// Output : int
//-----------------------------------------------------------------------------
int UTIL_EntitiesInBox( CBaseEntity **pList, int listMax, const Vector &mins, const Vector &maxs, int flagMask, int partitionMask )
{
	CFlaggedEntitiesEnum boxEnum( pList, listMax, flagMask );
	::partition->EnumerateElementsInBox( partitionMask, mins, maxs, false, &boxEnum );
	
	return boxEnum.GetCount();

}

//-----------------------------------------------------------------------------
// Purpose: Pass in an array of pointers and an array size, it fills the array and returns the number inserted
// Input  : **pList - 
//			listMax - 
//			&center - 
//			radius - 
//			flagMask - 
// Output : int
//-----------------------------------------------------------------------------
int UTIL_EntitiesInSphere( CBaseEntity **pList, int listMax, const Vector &center, float radius, int flagMask, int partitionMask )
{
	CFlaggedEntitiesEnum sphereEnum( pList, listMax, flagMask );
	::partition->EnumerateElementsInSphere( partitionMask, center, radius, false, &sphereEnum );

	return sphereEnum.GetCount();

}

int	UTIL_RenderablesInBox( CBaseEntity** pList, int listMax, const Vector &mins, const Vector &maxs )
{
	return g_pClientLeafSystem->GetEntitiesInBox( pList, listMax, mins, maxs );
}

CEntitySphereQuery::CEntitySphereQuery( const Vector &center, float radius, int flagMask, int partitionMask )
{
	m_listIndex = 0;
	m_listCount = UTIL_EntitiesInSphere( m_pList, std::size( m_pList ), center, radius, flagMask, partitionMask );
}

CBaseEntity *CEntitySphereQuery::GetCurrentEntity()
{
	if ( m_listIndex < m_listCount )
		return m_pList[m_listIndex];
	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: Scans player names
//Passes the player name to be checked in a KeyValues pointer
//with the keyname "name"
// - replaces '&' with '&&' so they will draw in the scoreboard
// - replaces '#' at the start of the name with '*'
//-----------------------------------------------------------------------------

void UTIL_MakeSafeName( const char *oldName, char *newName, int newNameBufSize )
{
	int newpos = 0;

	for( const char *p=oldName; *p != 0 && newpos < newNameBufSize-1; p++ )
	{
		//check for a '#' char at the beginning
		if( p == oldName && *p == '#' )
		{
			newName[newpos] = '*';
			newpos++;
		}
		else if( *p == '%' )
		{
			// remove % chars
			newName[newpos] = '*';
			newpos++;
		}
		else if( *p == '&' )
		{
			//insert another & after this one
			if ( newpos+2 < newNameBufSize )
			{
				newName[newpos] = '&';
				newName[newpos+1] = '&';
				newpos+=2;
			}
		}
		else
		{
			newName[newpos] = *p;
			newpos++;
		}
	}
	newName[newpos] = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Scans player names and replaces characters that vgui won't
//          display properly
// Input  : *oldName - player name to be fixed up
// Output : *char - static buffer with the safe name
//-----------------------------------------------------------------------------

const char * UTIL_SafeName( const char *oldName )
{
	static char safeName[ MAX_PLAYER_NAME_LENGTH * 2 + 1 ];
	UTIL_MakeSafeName( oldName, safeName, sizeof( safeName ) );

	return safeName;
}

//-----------------------------------------------------------------------------
// Purpose: Looks up key bindings for commands and replaces them in string.
//			%<commandname>% will get replaced with its bound control, e.g. %attack2%
//			Input buffer sizes are in bytes rather than unicode character count
//			for consistency with other APIs.  If inbufsizebytes is 0 a NULL-terminated
//			input buffer is assumed, or you can pass the size of the input buffer if
//			not NULL-terminated.
//-----------------------------------------------------------------------------
void UTIL_ReplaceKeyBindings( const wchar_t *inbuf, int inbufsizebytes, wchar_t *outbuf, int outbufsizebytes )
{
	if ( !inbuf || !inbuf[0] )
		return;

	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

	// copy to a new buf if there are vars
	outbuf[0]=0;
	int pos = 0;
	const wchar_t *inbufend = nullptr;
	if ( inbufsizebytes > 0 )
	{
		inbufend = inbuf + ( inbufsizebytes / 2 );
	}

	while( inbuf != inbufend && *inbuf != 0 )
	{
		// check for variables
		if ( *inbuf == '%' )
		{
			++inbuf;

			const wchar_t *end = wcschr( inbuf, '%' );
			if ( end && ( end != inbuf ) ) // make sure we handle %% in the string, which should be treated in the output as %
			{
				wchar_t token[64];
				wcsncpy( token, inbuf, end - inbuf );
				token[end - inbuf] = 0;

				inbuf += end - inbuf;

				// lookup key names
				char binding[64];
				g_pLocalize->ConvertUnicodeToANSI( token, binding, sizeof(binding) );

				const char *key = engine->Key_LookupBindingEx( *binding == '+' ? binding + 1 : binding, nSlot );

				//!! change some key names into better names
				char friendlyName[64];
				bool bAddBrackets = false;
				if ( !key || !key[0] )
				{
					V_snprintf( friendlyName, sizeof(friendlyName), "%s", binding );
				}
				else
				{
					V_snprintf( friendlyName, sizeof(friendlyName), "%s", key );
					V_strupr( friendlyName );
				}
				
				wchar_t *locName = g_pLocalize->Find( friendlyName );
				if ( !locName || wcslen(locName) <= 0)
				{
					g_pLocalize->ConvertANSIToUnicode( friendlyName, token, sizeof(token) );

					outbuf[pos] = '\0';
					wcscat( outbuf, token );
					pos += wcslen(token);
				}
				else
				{
					outbuf[pos] = '\0';
					if ( bAddBrackets )
					{
						wcscat( outbuf, L"[" );
						pos += 1;
					}
					wcscat( outbuf, locName );
					pos += wcslen(locName);
					if ( bAddBrackets )
					{
						wcscat( outbuf, L"]" );
						pos += 1;
					}
				}
			}
			else
			{
				outbuf[pos] = *inbuf;
				++pos;
			}
		}
		else
		{
			outbuf[pos] = *inbuf;
			++pos;
		}

		++inbuf;
	}

	outbuf[pos] = '\0';
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *filename - 
//			*pLength - 
// Output : byte
//-----------------------------------------------------------------------------
byte *UTIL_LoadFileForMe( const char *filename, int *pLength )
{
	byte *buffer;

	FileHandle_t file;
	file = filesystem->Open( filename, "rb", "GAME" );
	if ( FILESYSTEM_INVALID_HANDLE == file )
	{
		if ( pLength ) *pLength = 0;
		return nullptr;
	}

	int size = filesystem->Size( file );
	buffer = new byte[ size + 1 ];
	if ( !buffer )
	{
		Warning( "UTIL_LoadFileForMe:  Couldn't allocate buffer of size %i for file %s\n", size + 1, filename );
		filesystem->Close( file );
		return nullptr;
	}
	filesystem->Read( buffer, size, file );
	filesystem->Close( file );

	// Ensure null terminator
	buffer[ size ] =0;

	if ( pLength )
	{
		*pLength = size;
	}

	return buffer;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *buffer - 
//-----------------------------------------------------------------------------
void UTIL_FreeFile( byte *buffer )
{
	delete[] buffer;
}

//-----------------------------------------------------------------------------
// Compute distance fade
//-----------------------------------------------------------------------------
static unsigned char ComputeDistanceFade( CBaseEntity *pEntity, float flMinDist, float flMaxDist )
{
	if ((flMinDist <= 0) && (flMaxDist <= 0))
		return 255;

	if( flMinDist > flMaxDist )
	{
		V_swap( flMinDist, flMaxDist );
	}

	// If a negative value is provided for the min fade distance, then base it off the max.
	if( flMinDist < 0 )
	{
		flMinDist = flMaxDist - 400;
		if( flMinDist < 0 )
		{
			flMinDist = 0;
		}
	}

	flMinDist *= flMinDist;
	flMaxDist *= flMaxDist;

	float flCurrentDistanceSq = CurrentViewOrigin().DistToSqr( pEntity->WorldSpaceCenter() );
	CBasePlayer *pLocal = CBasePlayer::GetLocalPlayer();
	if ( pLocal )
	{
		float flDistFactor = pLocal->GetFOVDistanceAdjustFactor();
		flCurrentDistanceSq *= flDistFactor * flDistFactor;
	}

	// If I'm inside the minimum range than don't resort to alpha trickery
	if ( flCurrentDistanceSq <= flMinDist )
		return 255;

	if ( flCurrentDistanceSq >= flMaxDist )
		return 0;

	// NOTE: Because of the if-checks above, flMinDist != flMinDist here
	float flFalloffFactor = 255.0f / (flMaxDist - flMinDist);
	int nAlpha = flFalloffFactor * (flMaxDist - flCurrentDistanceSq);
	return clamp( nAlpha, 0, 255 );
}

//-----------------------------------------------------------------------------
// Compute fade amount
//-----------------------------------------------------------------------------
unsigned char UTIL_ComputeEntityFade( CBaseEntity *pEntity, float flMinDist, float flMaxDist, float flFadeScale )
{
	unsigned char nAlpha = 255;

	// If we're taking devshots, don't fade props at all
	if ( g_MakingDevShots || cl_leveloverview.GetInt() != 0 || input->CAM_IsThirdPersonOverview() )
		return 255;

	{
		nAlpha = ComputeDistanceFade( pEntity, flMinDist, flMaxDist );

		// NOTE: This computation for the center + radius is invalid!
		// The center of the sphere is at the center of the OBB, which is not necessarily
		// at the render origin. But it should be close enough.
		Vector vecMins, vecMaxs;
		pEntity->GetRenderBounds( vecMins, vecMaxs );
		float flRadius = vecMins.DistTo( vecMaxs ) * 0.5f;

		Vector vecAbsCenter;
		if ( modelinfo->GetModelType( pEntity->GetModel() ) == mod_brush )
		{
			Vector vecRenderMins, vecRenderMaxs;
			pEntity->GetRenderBoundsWorldspace( vecRenderMins, vecRenderMaxs );
			VectorAdd( vecRenderMins, vecRenderMaxs, vecAbsCenter );
			vecAbsCenter *= 0.5f;
		}
		else
		{
			vecAbsCenter = pEntity->GetRenderOrigin();
		}

		unsigned char nGlobalAlpha = modelinfo->ComputeLevelScreenFade( vecAbsCenter, flRadius, flFadeScale );
		unsigned char nDistAlpha;

		if ( !engine->IsLevelMainMenuBackground() )
		{
			nDistAlpha = modelinfo->ComputeViewScreenFade( vecAbsCenter, flRadius, flFadeScale );
		}
		else
		{
			nDistAlpha = 255;
		}

		if ( nDistAlpha < nGlobalAlpha )
		{
			nGlobalAlpha = nDistAlpha;
		}

		if ( nGlobalAlpha < nAlpha )
		{
			nAlpha = nGlobalAlpha;
		}
	}

	return nAlpha;
}

//-----------------------------------------------------------------------------
// Purpose: Given a vector, clamps the scalar axes to MAX_COORD_FLOAT ranges from worldsize.h
// Input  : *pVecPos - 
//-----------------------------------------------------------------------------
void UTIL_BoundToWorldSize( Vector *pVecPos )
{
	contract_assert( pVecPos );
	for ( int i = 0; i < 3; ++i )
	{
		(*pVecPos)[ i ] = clamp( (*pVecPos)[ i ], MIN_COORD_FLOAT, MAX_COORD_FLOAT );
	}
}

inline constexpr const char *MAP_KEY_FILE_DIR = "media";
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBasePlayer* UTIL_PlayerByUserId( int userID )
{
	for (int i = 1; i<=gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );

		if ( !pPlayer )
			continue;

		if ( pPlayer->GetUserID() == userID )
		{
			return pPlayer;
		}
	}

	return nullptr;
}

CBaseEntity* UTIL_EntityFromUserMessageEHandle( long nEncodedEHandle )
{
	int nEntity, nSerialNum;
	if( nEncodedEHandle == INVALID_NETWORKED_EHANDLE_VALUE )
		return nullptr;

	nEntity = nEncodedEHandle & ((1 << MAX_EDICT_BITS) - 1);
	nSerialNum = nEncodedEHandle >> MAX_EDICT_BITS;

	EHANDLE hEntity( nEntity, nSerialNum );
	return hEntity.Get();
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void UTIL_ApproachTarget( float target, float increaseSpeed, float decreaseSpeed, float *val )
{
	if ( *val < target )
	{
		*val += gpGlobals->frametime*increaseSpeed;
		*val = std::min( *val, target );
	}
	else if ( *val > target )
	{
		*val -= gpGlobals->frametime*decreaseSpeed;
		*val = std::max( *val, target );
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void UTIL_ApproachTarget( const Vector &target, float increaseSpeed, float decreaseSpeed, Vector *val )
{
	UTIL_ApproachTarget( target.x, increaseSpeed, decreaseSpeed, &val->x );
	UTIL_ApproachTarget( target.y, increaseSpeed, decreaseSpeed, &val->y );
	UTIL_ApproachTarget( target.z, increaseSpeed, decreaseSpeed, &val->z );
}

//-----------------------------------------------------------------------------
// Purpose: Returns the filename to count map loads in
//-----------------------------------------------------------------------------
bool UTIL_GetMapLoadCountFileName( int iController, const char *pszFilePrependName, char *pszBuffer, int iBuflen )
{

	{
		V_snprintf( pszBuffer, iBuflen, "%s/%s", MAP_KEY_FILE_DIR, pszFilePrependName );
	}

	return true;
}

inline constexpr const char *MAP_KEY_FILE = "mapkeys.res";

void UTIL_IncrementMapKey( const char *pszCustomKey )
{
	int iController = -1;

	if ( !pszCustomKey )
		return;

	char szFilename[ k_cMaxPath ];
	if ( !UTIL_GetMapLoadCountFileName( iController, MAP_KEY_FILE, szFilename, k_cMaxPath ) )
		return;

	int iCount = 1;

	KeyValues *kvMapLoadFile = new KeyValues( MAP_KEY_FILE );
	if ( kvMapLoadFile )
	{
		kvMapLoadFile->LoadFromFile( g_pFullFileSystem, szFilename, "MOD" );

		char mapname[MAX_MAP_NAME];
		V_FileBase( engine->GetLevelName(), mapname, sizeof( mapname) );
		V_strlower( mapname );

		// Increment existing, or add a new one
		KeyValues *pMapKey = kvMapLoadFile->FindKey( mapname );
		if ( pMapKey )
		{
			iCount = pMapKey->GetInt( pszCustomKey, 0 ) + 1;
			pMapKey->SetInt( pszCustomKey, iCount );
		}
		else 
		{
			KeyValues *pNewKey = new KeyValues( mapname );
			if ( pNewKey )
			{
				pNewKey->SetString( pszCustomKey, "1" );
				kvMapLoadFile->AddSubKey( pNewKey );
			}
		}

		// Write it out

		// force create this directory incase it doesn't exist
		filesystem->CreateDirHierarchy( MAP_KEY_FILE_DIR, "MOD");

		ks::text_writer buf;
		kvMapLoadFile->RecursiveSaveToFile( buf, 0 );
		g_pFullFileSystem->WriteFile( szFilename, "MOD", buf.bytes() );

		kvMapLoadFile->deleteThis();
	}

}

int UTIL_GetMapKeyCount( const char *pszCustomKey )
{
	int iController = -1;

	if ( !pszCustomKey )
		return 0;

	char szFilename[ k_cMaxPath ];
	if ( !UTIL_GetMapLoadCountFileName( iController, MAP_KEY_FILE, szFilename, k_cMaxPath ) )
		return 0;

	int iCount = 0;

	KeyValues *kvMapLoadFile = new KeyValues( MAP_KEY_FILE );
	if ( kvMapLoadFile )
	{
		// create an empty file if none exists
		if ( !g_pFullFileSystem->FileExists( szFilename, "MOD" ) )
		{
			// force create this directory incase it doesn't exist
			filesystem->CreateDirHierarchy( MAP_KEY_FILE_DIR, "MOD");

			g_pFullFileSystem->WriteFile( szFilename, "MOD", std::span< const std::byte >() );
		}

		kvMapLoadFile->LoadFromFile( g_pFullFileSystem, szFilename, "MOD" );

		char mapname[MAX_MAP_NAME];
		V_FileBase( engine->GetLevelName(), mapname, sizeof( mapname) );
		V_strlower( mapname );

		KeyValues *pMapKey = kvMapLoadFile->FindKey( mapname );
		if ( pMapKey )
		{
			iCount = pMapKey->GetInt( pszCustomKey );
		}

		kvMapLoadFile->deleteThis();
	}

	return iCount;
}

bool UTIL_HasLoadedAnyMap()
{
	int iController = -1;

	char szFilename[ k_cMaxPath ];
	if ( !UTIL_GetMapLoadCountFileName( iController, MAP_KEY_FILE, szFilename, k_cMaxPath ) )
		return false;

	return g_pFullFileSystem->FileExists( szFilename, "MOD" );
}

wchar_t *UTIL_GetLocalizedKeyString( const char *command, const char *fmt, const wchar_t *arg1, const wchar_t *arg2, const wchar_t *arg3 )
{
	static wchar_t useString[4][256];
	static int index = 0;

	index = index + 1;
	if ( index > 3 )
		index = 0;

	const char *lowercaseKey = engine->Key_LookupBinding( command );
	if ( !lowercaseKey )
	{
		lowercaseKey = "<NOT BOUND>";
	}

	char szKey[64];
	V_strncpy( szKey, lowercaseKey, sizeof( szKey ) );
	for ( char *tmp = szKey; *tmp; ++tmp )
	{
		*tmp = toupper( *tmp );
	}

	wchar_t wszKey[64];
	g_pLocalize->ConvertANSIToUnicode( szKey,  wszKey, sizeof(wszKey) );

	int argCount = 1;
	if ( arg1 )
	{
		++argCount;
		if ( arg2 )
		{
			++argCount;
			if ( arg3 )
			{
				++argCount;
			}
		}
	}

	g_pLocalize->ConstructString( useString[index], sizeof( useString[index] ), g_pLocalize->Find( fmt ), argCount, wszKey, arg1, arg2, arg3 );
	return useString[index];
}

void UTIL_GetClientStatusText( char *buffer, int nSize )
{
	if ( !buffer || nSize==0 ) {return;}
	buffer[0] = 0;

	float g_flReadyToCheckForPCBootInvite = 0.0f;
	bool bStartupFinished = g_flReadyToCheckForPCBootInvite && ( ( Plat_FloatTime() - g_flReadyToCheckForPCBootInvite ) > 1.5f );
	if ( bStartupFinished )
		V_snprintf( buffer, nSize, "+" );
	if ( nSize <= 2 )
		return;

	CCS_PlayerResource *pCSPR = ( CCS_PlayerResource* )GameResources();
	if (pCSPR)
	{
		V_snprintf( buffer, nSize, "%sPlayers: ", ( bStartupFinished ? "+" : "" ) );
		for ( int i = 1; i <= MAX_PLAYERS; i++ )
		{
			if ( pCSPR->IsConnected(i) )
			{
				const char *name = pCSPR->GetPlayerName(i);
				if (name && name[0])
				{
					V_strncat( buffer, name, nSize );
					V_strncat( buffer, ", ", nSize );
				}
			}
		}
		buffer[nSize-1]=0;
	}
}

void UTIL_ClearTrace( trace_t &trace )
{
	memset( &trace, 0, sizeof(trace));
	trace.fraction = 1.f;
	trace.fractionleftsolid = 0;
	trace.surface = g_NullSurface;
}
