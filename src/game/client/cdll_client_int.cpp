// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

#include "rocketui/rmlui.h"
#include "predictable_entity.h"
#include "cdll_util.h"
#include "util_shared.h"
#include "baseentity_shared.h"
#include "precache_register.h"
#include "c_basecombatweapon.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "c_baseplayer.h"
#include "vphysics_interface.h"
#include "cdll_client_int.h"
#include "engine/ivmodelinfo.h"
#include "cdll_int.h"
#include <crtmemdebug.h>
import ks.vstdlib;
import std;
#include "clientui.h"
#include "clientmode.h"
#include "iinput.h"
#include "iviewrender.h"
#include "ivieweffects.h"
#include "ivmodemanager.h"
#include "prediction.h"
#include "clientsideeffects.h"
#include "particlemgr.h"
#include "smoke_fog_overlay.h"
#include "iengineui.h"
#include "iefx.h"
#include "enginesprite.h"
#include "voice_status.h"
#include "filesystem.h"
#include "c_te_legacytempents.h"
#include "c_rope.h"
#include "engine/ishadowmgr.h"
#include "engine/IStaticPropMgr.h"
#include "hud_basechat.h"
#include "view_shared.h"
#include "env_wind_shared.h"
#include "detailobjectsystem.h"
#include "soundenvelope.h"
#include "c_basetempentity.h"
#include "materialsystem/imaterialsystemstub.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "c_soundscape.h"
#include "engine/ivdebugoverlay.h"
#include "uicenterprint.h"
#include "iviewrender_beams.h"
#include "engine/IEngineTrace.h"
#include "engine/ivmodelinfo.h"
#include "physics.h"
#include "usermessages.h"
#include "gamestringpool.h"
#include "c_user_message_register.h"
#include "IGameUIFuncs.h"
#include "igameevents.h"
#include "datacache/idatacache.h"
#include "datacache/imdlcache.h"
#include "kbutton.h"
#include "gamerules_register.h"
#include "game/client/iviewport.h"
#include "bitmap/tgawriter.h"
#include "c_world.h"
#include "perfvisualbenchmark.h"	
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "colorcorrectionmgr.h"
#include "physpropclientside.h"
#include "game/client/IGameClientExports.h"
#include "client_factorylist.h"
#include "ragdoll_shared.h"
#include "view_scene.h"
#include "iclientmode.h"
#include "con_nprint.h"
#include "inputsystem/iinputsystem.h"
#include "scenefilecache/ISceneFileCache.h"
#include "tier3/tier3.h"
#include "avi/iavi.h"
#include "avi/iquicktime.h"
#include "toolframework_client.h"
#include "hltvcamera.h"
#include "hltvreplaysystem.h"
#include "matchmaking/imatchframework.h"
#include "cdll_bounded_cvars.h"
#include "GameStats.h"
#include "videocfg/videocfg.h"
#include "tier2/tier2_logging.h"
#include "Sprite.h"
#include "gametypes/igametypes.h"
#include "c_keyvalue_saver.h"
#include "c_team.h"
#include "cs_gamerules.h"
#include "c_cs_player.h"
#include "cstrike15/RocketUI/rkconsole.h"
#include "cs_item_inventory.h"

#include "mumble.h"

#include "vscript/ivscript.h"
#include "activitylist.h"
#include "eventlist.h"

#include "game_controls/igameuisystemmgr.h"

#include "imaterialproxydict.h"
#include "engine/iblackbox.h"
#include "c_rumble.h"

#include "achievements_and_stats_interface.h"

#include "hud.h"



#include "keybindinglistener.h"


#include "viewrender.h"

#include "irendertorthelperobject.h"

extern IClientMode *GetClientModeNormal();

LoggingChannelID_t LOG_CONSOLE = LoggingSystem_RegisterLoggingChannel( "Console", nullptr );;

// IF YOU ADD AN INTERFACE, EXTERN IT IN THE HEADER FILE.
IVEngineClient	*engine = nullptr;
IVModelRender *modelrender = nullptr;
IVEfx *effects = nullptr;
IVRenderView *render = nullptr;
IVDebugOverlay *debugoverlay = nullptr;
IMaterialSystemStub *materials_stub = nullptr;
IDataCache *datacache = nullptr;
IVModelInfoClient *modelinfo = nullptr;
IEngineUI *engineui = nullptr;
INetworkStringTableContainer *networkstringtable = nullptr;
ISpatialPartition* partition = nullptr;
IFileSystem *filesystem = nullptr;
IShadowMgr *shadowmgr = nullptr;
IStaticPropMgrClient *staticpropmgr = nullptr;
IEngineSound *enginesound = nullptr;
IUniformRandomStream *random = nullptr;
static CGaussianRandomStream s_GaussianRandomStream;
CGaussianRandomStream *randomgaussian = &s_GaussianRandomStream;
ISharedGameRules *sharedgamerules = nullptr;
IEngineTrace *enginetrace = nullptr;
IFileLoggingListener *filelogginglistener = nullptr;
IGameUIFuncs *gameuifuncs = nullptr;
IGameEventManager2 *gameeventmanager = nullptr;
ISoundEmitterSystemBase *soundemitterbase = nullptr;
IInputSystem *inputsystem = nullptr;
ISceneFileCache *scenefilecache = nullptr;
IAvi *avi = nullptr;
IBik *bik = nullptr;
IQuickTime *pQuicktime = nullptr;
IRenderToRTHelper *g_pRenderToRTHelper = nullptr;

IUploadGameStats *gamestatsuploader = nullptr;
IBlackBox *blackboxrecorder = nullptr;

AchievementsAndStatsInterface* g_pAchievementsAndStatsInterface = nullptr;

IScriptManager *scriptmanager = nullptr;

IGameSystem *SoundEmitterSystem();
IGameSystem *ToolFrameworkClientSystem();
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
class CHudChat;
class CViewRender;

static C_BaseEntityClassList *s_pClassLists = nullptr;
C_BaseEntityClassList::C_BaseEntityClassList()
{
	m_pNextClassList = s_pClassLists;
	s_pClassLists = this;
}
C_BaseEntityClassList::~C_BaseEntityClassList()
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


class IMoveHelper;

static ConVar s_CV_ShowParticleCounts("showparticlecounts", "0", 0, "Display number of particles drawn per frame");
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

CSetActiveSplitScreenPlayerGuard::CSetActiveSplitScreenPlayerGuard( char const *pchContext, int nLine, C_BaseEntity *pEntity, int nOldSlot, bool bSetUIScreenSize ) :
	CUIScreenSizeSplitScreenPlayerGuard( bSetUIScreenSize, pEntity, nOldSlot )
{
	int slot = C_BasePlayer::GetSplitScreenSlotForPlayer( pEntity );
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
static ConCommand cl_dumpsplithacks_command( "cl_dumpsplithacks", cl_dumpsplithacks, "Dump split screen workarounds." );
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

CUIScreenSizeSplitScreenPlayerGuard::CUIScreenSizeSplitScreenPlayerGuard( bool bActive, C_BaseEntity *pEntity, int nOldSlot )
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
static InterfaceReg s_Reg_CGameClientExports_IGameClientExports( []() -> void * { return static_cast<IGameClientExports *>( &s_Singleton_CGameClientExports ); }, GAMECLIENTEXPORTS_INTERFACE_VERSION );;

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
static InterfaceReg s_Reg_CClientDLLSharedAppSystems_IClientDLLSharedAppSystems( []() -> void * { return static_cast<IClientDLLSharedAppSystems *>( &s_Singleton_CClientDLLSharedAppSystems ); }, CLIENT_DLL_SHARED_APPSYSTEMS );;


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

ConVar cl_ShowBoneSetupEnts( "cl_ShowBoneSetupEnts", "0", 0, "Show which entities are having their bones setup each frame." );

class CBoneSetupEnt
{
public:
	char m_ModelName[128];
	int m_Index;
	int m_Count;
};

std::map< int, CBoneSetupEnt > g_BoneSetupEnts;	// keyed by m_Index


void TrackBoneSetupEnt( C_BaseAnimating *pEnt )
{
#ifdef _DEBUG
	if ( !cl_ShowBoneSetupEnts.GetInt() )
		return;

	CBoneSetupEnt ent;
	ent.m_Index = pEnt->entindex();
	auto i = g_BoneSetupEnts.find( ent.m_Index );
	if ( i == g_BoneSetupEnts.end() )
	{
		V_strncpy( ent.m_ModelName, modelinfo->GetModelName( pEnt->GetModel() ), sizeof( ent.m_ModelName ) );
		ent.m_Count = 1;
		g_BoneSetupEnts.emplace( ent.m_Index, ent );
	}
	else
	{
		i->second.m_Count++;
	}
#endif
}

void DisplayBoneSetupEnts()
{
#ifdef _DEBUG
	if ( !cl_ShowBoneSetupEnts.GetInt() )
		return;

	// the last entry is not shown
	auto itEnd = g_BoneSetupEnts.empty() ? g_BoneSetupEnts.end() : std::prev( g_BoneSetupEnts.end() );
	int nElements = (int)std::distance( g_BoneSetupEnts.begin(), itEnd );
		
	engine->Con_NPrintf( 0, "%d bone setup ents (name/count/entindex) ------------", nElements );

	con_nprint_s printInfo;
	printInfo.time_to_live = -1;
	printInfo.fixed_width_font = true;
	printInfo.color[0] = printInfo.color[1] = printInfo.color[2] = 1;
	
	printInfo.index = 2;
	for ( auto i = g_BoneSetupEnts.begin(); i != itEnd; ++i )
	{
		CBoneSetupEnt *pEnt = &i->second;
		
		if ( pEnt->m_Count >= 3 )
		{
			printInfo.color[0] = 1;
			printInfo.color[1] = printInfo.color[2] = 0;
		}
		else if ( pEnt->m_Count == 2 )
		{
			printInfo.color[0] = (float)200 / 255;
			printInfo.color[1] = (float)220 / 255;
			printInfo.color[2] = 0;
		}
		else
		{
			printInfo.color[0] = printInfo.color[0] = printInfo.color[0] = 1;
		}
		engine->Con_NXPrintf( &printInfo, "%25s / %3d / %3d", pEnt->m_ModelName, pEnt->m_Count, pEnt->m_Index );
		printInfo.index++;
	}

	g_BoneSetupEnts.clear();
#endif
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

	virtual void					InitSprite( CEngineSprite *pSprite, const char *loadname );
	virtual void					ShutdownSprite( CEngineSprite *pSprite );

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

	virtual bool IsConnectedUserInfoChangeAllowed( IConVar *pCvar );
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

static InterfaceReg s_Reg_CHLClient_IBaseClientDLL( []() -> void * { return static_cast<IBaseClientDLL *>( &gHLClient ); }, CLIENT_DLL_INTERFACE_VERSION );;


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
	g_pPrecacheSystem->Cache( g_pPrecacheHandler, DISPATCH_EFFECT, pEffectName, true, RESOURCE_LIST_INVALID, true );
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
static ConVar cl_threaded_init("cl_threaded_init", "0");

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
	IGameSystem::Add( ToolFrameworkClientSystem() );
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

	C_BaseTempEntity::PrecacheTempEnts();

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
static ConCommand cl_modemanager_reload_command( "cl_modemanager_reload", cl_modemanager_reload, "Reloads the panel metaclasses for vgui screens." );
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

	ConnectTier1Libraries( &appSystemFactory, 1 );
	ConnectTier2Libraries( &appSystemFactory, 1 );
	ConnectTier3Libraries( &appSystemFactory, 1 );

	// Initialize the console variables.
	ConVar_Register( FCVAR_CLIENTDLL );

	return true;
}


void CHLClient::Disconnect()
{
	ConVar_Unregister( );
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
	if ( (filelogginglistener = (IFileLoggingListener *)appSystemFactory(FILELOGGINGLISTENER_INTERFACE_VERSION, nullptr)) == nullptr )
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
	if ( (random = (IUniformRandomStream *)appSystemFactory(VENGINE_CLIENT_RANDOM_INTERFACE_VERSION, nullptr)) == nullptr )
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

	// Not fatal if the material system stub isn't around.
	materials_stub = (IMaterialSystemStub*)appSystemFactory( MATERIAL_SYSTEM_STUB_INTERFACE_VERSION, nullptr );

	if( !g_pMaterialSystemHardwareConfig )
		return false;

	// Hook up the gaussian random number generator
	s_GaussianRandomStream.AttachToStream( random );

	g_pcv_ThreadMode = g_pCVar->FindVar( "host_thread_mode" );



	COM_TimestampedLog( "InitGameSystems" );

	bool bInitSuccess = false;
	if ( cl_threaded_init.GetBool() )
	{
		CFunctorJob *pGameJob = new CFunctorJob( CreateFunctor( InitParticleManager ) );
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


	COM_TimestampedLog( "C_BaseAnimating::InitBoneSetupThreadPool" );

	C_BaseAnimating::InitBoneSetupThreadPool();

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

	C_BaseAnimating::ShutdownBoneSetupThreadPool();
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
	C_BaseTempEntity::ClearDynamicTempEnts();
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

	DisconnectTier3Libraries( );
	DisconnectTier2Libraries( );
	ConVar_Unregister();
	DisconnectTier1Libraries( );

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
		C_BaseAnimating::AutoAllowBoneAccess boneaccess( true, false ); 
		IGameSystem::UpdateAllSystems( frametime );
	}

	// run vgui animations

	// I don't think this is necessary any longer, but I will leave it until
	// I can check into this further.
	C_BaseTempEntity::CheckDynamicTempEnts();
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
	bool bSave = C_BaseEntity::IsAbsRecomputationsEnabled();
	C_BaseEntity::EnableAbsRecomputations( true );

	// [mariod] - testing, see note in c_baseanimating.cpp
	//C_BaseAnimating::EnableNewBoneSetupRequest( false );

	ABS_QUERY_GUARD( true );

	C_BaseAnimating::AutoAllowBoneAccess boneaccess( true, false ); 

	MDLCACHE_CRITICAL_SECTION();
	input->ExtraMouseSample( frametime, active );

	// [mariod] - testing, see note in c_baseanimating.cpp
	//C_BaseAnimating::EnableNewBoneSetupRequest( true );


	C_BaseEntity::EnableAbsRecomputations( bSave );
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

	contract_assert( C_BaseEntity::IsAbsRecomputationsEnabled() );
	contract_assert( C_BaseEntity::IsAbsQueriesValid() );

	C_BaseAnimating::AutoAllowBoneAccess boneaccess( true, false ); 

	// [mariod] - testing, see note in c_baseanimating.cpp
	//C_BaseAnimating::EnableNewBoneSetupRequest( false );

	MDLCACHE_CRITICAL_SECTION();
	input->CreateMove( sequence_number, input_sample_frametime, active );

	// [mariod] - testing, see note in c_baseanimating.cpp
	//C_BaseAnimating::EnableNewBoneSetupRequest( true );
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
	C_BaseAnimating *pAnimating;
	for ( C_BaseEntity *pEntity = ClientEntityList().FirstBaseEntity(); pEntity; pEntity = ClientEntityList().NextBaseEntity(pEntity) )
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
void OnCPULevelChanged( IConVar *var, const char *pOldValue, float flOldValue )
{
	ConfigureCurrentSystemLevel();
}


static ConVar cpu_level( "cpu_level", "2", 0, "CPU Level - Default: High", OnCPULevelChanged );

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
void OnGPULevelChanged( IConVar *var, const char *pOldValue, float flOldValue )
{
	ConfigureCurrentSystemLevel();
}

static ConVar gpu_level( "gpu_level", "3", 0, "GPU Level - Default: High", OnGPULevelChanged );
GPULevel_t GetGPULevel()
{

	// Should we cache system_level off during level init?
	GPULevel_t nSystemLevel = (GPULevel_t)clamp( gpu_level.GetInt(), 0, GPU_LEVEL_PC_COUNT-1 );
	return nSystemLevel;
}


//-----------------------------------------------------------------------------
// System Memory level
//-----------------------------------------------------------------------------
void OnMemLevelChanged( IConVar *var, const char *pOldValue, float flOldValue )
{
	ConfigureCurrentSystemLevel();
}

static ConVar mem_level( "mem_level", "2", 0, "Memory Level - Default: High", OnMemLevelChanged );

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
void OnGPUMemLevelChanged( IConVar *var, const char *pOldValue, float flOldValue )
{
	ConfigureCurrentSystemLevel();
}

static ConVar gpu_mem_level( "gpu_mem_level", "2", 0, "Memory Level - Default: High", OnGPUMemLevelChanged );

GPUMemLevel_t GetGPUMemLevel()
{

	// Should we cache system_level off during level init?
	GPUMemLevel_t nSystemLevel = (GPUMemLevel_t)clamp( gpu_mem_level.GetInt(), 0, GPU_MEM_LEVEL_PC_COUNT-1 );
	return nSystemLevel;
}

ConVar cl_disable_splitscreen_cpu_level_cfgs_in_pip( "cl_disable_splitscreen_cpu_level_cfgs_in_pip", "1", 0, "" );

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

	C_BaseEntity::UpdateVisibilityAllEntities();
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

	C_BaseTempEntity::ClearDynamicTempEnts();
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
	C_PhysPropClientside::RecreateAll();
	C_Sprite::RecreateAllClientside();

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

	C_Sprite::DestroyAllClientside();
	C_PhysPropClientside::DestroyAll();

	modemanager->LevelShutdown();

	// Remove temporary entities before removing entities from the client entity list so that the te_* may
	// clean up before hand.
	tempents->LevelShutdown();

	// Now release/delete the entities
	cl_entitylist->Release();

	C_BaseEntityClassList *pClassList = s_pClassLists;
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
void CHLClient::InitSprite( CEngineSprite *pSprite, const char *loadname )
{
	if ( pSprite )
	{
		pSprite->Init( loadname );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSprite - 
//-----------------------------------------------------------------------------
void CHLClient::ShutdownSprite( CEngineSprite *pSprite )
{
	if ( pSprite )
	{
		pSprite->Shutdown();
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
	g_pPrecacheSystem->Cache( g_pPrecacheHandler, DISPATCH_EFFECT, newString, true, RESOURCE_LIST_INVALID, true );
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

	C_BaseEntity::SimulateEntities();
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

		C_BaseEntity *pEntity = pUnk->GetBaseEntity();
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
	C_BaseAnimating::EnableInvalidateBoneCache( true );

	::partition->SuppressLists( PARTITION_ALL_CLIENT_EDICTS, true );
	C_BaseEntity::SetAbsQueriesValid( false );

	Rope_ResetCounters();

	// Interpolate server entities and move aiments.
	{
		PREDICTION_TRACKVALUECHANGESCOPE( "interpolation" );
		C_BaseEntity::InterpolateServerEntities();
	}

	{
		// vprof node for this bloc of math
		CVProfScope _( "OnRenderStart: dirty bone caches", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
		// Invalidate any bone information.
		C_BaseAnimating::InvalidateBoneCaches();
		C_BaseFlex::InvalidateFlexCaches();

		C_BaseEntity::SetAbsQueriesValid( true );
		C_BaseEntity::EnableAbsRecomputations( true );

		// Enable access to all model bones except view models.
		// This is necessary for aim-ent computation to occur properly
		C_BaseAnimating::PushAllowBoneAccess( true, false, "OnRenderStart->CViewRender::SetUpView" ); // pops in CViewRender::SetUpView

		// FIXME: This needs to be done before the player moves; it forces
		// aiments the player may be attached to to forcibly update their position
		C_BaseEntity::MarkAimEntsDirty();
	}

	// Make sure the camera simulation happens before OnRenderStart, where it's used.
	// NOTE: the only thing that happens in CAM_Think is thirdperson related code.
	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		input->CAM_Think();
	}

	C_BaseAnimating::PopBoneAccess( "OnRenderStart->CViewRender::SetUpView" ); // pops the (true, false) bone access set in OnRenderStart

	// Enable access to all model bones until rendering is done
	C_BaseAnimating::PushAllowBoneAccess( true, true, "CViewRender::SetUpView->OnRenderEnd" ); // pop is in OnRenderEnd()



	// This will place all entities in the correct position in world space and in the KD-tree
	// NOTE: Doing this before view->OnRenderStart() because the player can be in hierarchy with
	// a client-side animated entity.  So the viewport position is dependent on this animation sometimes.
	C_BaseAnimating::UpdateClientSideAnimations();

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

	C_BaseAnimating::ThreadedBoneSetup();

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
	C_BaseEntity::CalcAimEntPositions();

	// For entities marked for recording, post bone messages to IToolSystems
	if ( ToolsEnabled() )
	{
		C_BaseEntity::ToolRecordEntities();
	}

	// update dynamic light state. Necessary for light cache to work properly for d- and elights
	engine->UpdateDAndELights();

	// Finally, link all the entities into the leaf system right before rendering.
	C_BaseEntity::AddVisibleEntities();

	g_pClientLeafSystem->RecomputeRenderableLeaves();
	g_pClientShadowMgr->ReprojectShadows();
	g_pClientShadowMgr->AdvanceFrame();
	g_pClientLeafSystem->DisableLeafReinsertion( true );
}

void OnRenderEnd()
{
	g_pClientLeafSystem->DisableLeafReinsertion( false );

	// Disallow access to bones (access is enabled in CViewRender::SetUpView).
	C_BaseAnimating::PopBoneAccess( "CViewRender::SetUpView->OnRenderEnd" );
	
	// [mariod] - testing, see note in c_baseanimating.cpp
	C_BaseAnimating::EnableInvalidateBoneCache( false );

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
			C_BaseEntity::EnableAbsRecomputations( false );
			C_BaseEntity::SetAbsQueriesValid( false );
			Interpolation_SetLastPacketTimeStamp( engine->GetLastTimeStamp() );
			::partition->SuppressLists( PARTITION_ALL_CLIENT_EDICTS, true );

			PREDICTION_STARTTRACKVALUE( "netupdate" );
		}
		break;
	case FRAME_NET_UPDATE_END:
		{
			ProcessCacheUsedMaterials();

			// reenable abs recomputation since now all entities have been updated
			C_BaseEntity::EnableAbsRecomputations( true );
			C_BaseEntity::SetAbsQueriesValid( true );
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
			if ( !C_BasePlayer::GetLocalPlayer( 0 ) )
			{
				// this is only possible when we're playing back the past, when our killer hasn't been spawned yet
				if ( C_BasePlayer *pPlayer = UTIL_PlayerByIndex( engine->GetLocalPlayer() ) )
				{
					pPlayer->SetAsLocalPlayer();
				}
				else
				{
					Warning( "No local player %d after full frame update\n", engine->GetLocalPlayer() );
					for ( int i = 0; i < MAX_PLAYERS; ++i )
						if ( C_BasePlayer*p = UTIL_PlayerByIndex( i ) )
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
			C_BaseEntity::CheckCLInterpChanged();
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

void AddRestoredEntity( C_BaseEntity *pEntity )
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
	C_BaseEntityIterator iterator;
	C_BaseEntity *pEnt;
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

	C_CSPlayer *pPlayer = GetLocalOrInEyeCSPlayer();

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
	CBaseEntity *entity = e1->GetBaseEntity();
	CBaseEntity *entityTouched = e2->GetBaseEntity();
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
	if ( C_HLTVCamera *pCamera = HLTVCamera() )
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
	C_CSPlayer* player = GetLocalOrInEyeCSPlayer();
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

bool CHLClient::IsConnectedUserInfoChangeAllowed( IConVar *pCvar )
{
	return CSGameRules() ? CSGameRules()->IsConnectedUserInfoChangeAllowed( nullptr ) : true;
}

void CHLClient::OnCommandDuringPlayback( char const *cmd )
{
	/* Removed for partner depot */
}

void CHLClient::RetireAllPlayerDecals( bool bRenderContextValid )
{
	extern void OnPlayerDecalsLevelShutdown();
	OnPlayerDecalsLevelShutdown();

	if ( bRenderContextValid )
	{
		// If the render context is valid (i.e. manual r_cleardecals)
		// then we should immediately update and reapply to avoid flickers
		extern void OnPlayerDecalsUpdate();
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

//-----------------------------------------------------------------------------
// Purpose: interface from materialsystem to client, currently just for recording into tools
//-----------------------------------------------------------------------------
class CClientMaterialSystem : public IClientMaterialSystem
{
	virtual HTOOLHANDLE GetCurrentRecordingEntity()
	{
		if ( !ToolsEnabled() )
			return HTOOLHANDLE_INVALID;

		if ( !clienttools->IsInRecordingMode() )
			return HTOOLHANDLE_INVALID;

		const C_BaseEntity *pEnt = nullptr;
		if( m_pProxyData ) //dynamic_cast not possible with void *. Just going to have to search to verify that it actually is an entity
		{
			CClientEntityList &entList = ClientEntityList();
			C_BaseEntity *pIter = entList.FirstBaseEntity();
			while( pIter )
			{
				if( (pIter == m_pProxyData) || (pIter->GetClientRenderable() == m_pProxyData) )
				{
					pEnt = pIter;
					break;
				}
				
				pIter = entList.NextBaseEntity( pIter );
			}
		}
		
		if( !pEnt && (materials->GetThreadMode() == MATERIAL_SINGLE_THREADED) )
		{
			pEnt = view->GetCurrentlyDrawingEntity();
		}

		if ( !pEnt || !pEnt->IsToolRecording() )
			return HTOOLHANDLE_INVALID;

		return pEnt->GetToolHandle();
	}
	virtual void PostToolMessage( HTOOLHANDLE hEntity, KeyValues *pMsg )
	{
		ToolFramework_PostToolMessage( hEntity, pMsg );
	}
	virtual void SetMaterialProxyData( void *pProxyData )
	{
		m_pProxyData = pProxyData;
	}

	void *m_pProxyData;
};


//-----------------------------------------------------------------------------
// Singleton instance
//-----------------------------------------------------------------------------
static CClientMaterialSystem s_ClientMaterialSystem;
IClientMaterialSystem *g_pClientMaterialSystem = &s_ClientMaterialSystem;
static InterfaceReg s_Reg_CClientMaterialSystem_IClientMaterialSystem( []() -> void * { return static_cast<IClientMaterialSystem *>( &s_ClientMaterialSystem ); }, VCLIENTMATERIALSYSTEM_INTERFACE_VERSION );;
