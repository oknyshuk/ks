#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "iclientnetworkable.h"
#include "cdll_int.h"
#include "shareddefs.h"
#include "eiface.h"
#include "tier3/tier3.h"
#include "tier2/tier2_logging.h"
#include "videocfg/videocfg.h"

class ISharedGameRules;
class C_BaseAnimating;
class C_BaseEntity;
class AchievementsAndStatsInterface;

extern IVEfx *effects;
extern IVRenderView *render;
extern IEngineUI *engineui;
extern IRenderToRTHelper *g_pRenderToRTHelper;
extern IVModelRender *modelrender;
extern IVEngineClient	*engine;
extern IVModelRender *modelrender;
extern IVDebugOverlay *debugoverlay;
extern IVModelInfoClient *modelinfo;
extern ISpatialPartition* partition;
extern IBaseClientDLL *clientdll;
extern IFileSystem *filesystem;
extern IStaticPropMgrClient *staticpropmgr;
extern IShadowMgr *shadowmgr;
extern IEngineSound *enginesound;
extern IEngineTrace *enginetrace;
extern CFileLoggingListener	*filelogginglistener;
extern IGameUIFuncs *gameuifuncs;
extern IGameEventManager2 *gameeventmanager;
extern CGlobalVarsBase *gpGlobals;
extern IInputSystem *inputsystem;
extern ISceneFileCache *scenefilecache;
extern IUploadGameStats *gamestatsuploader;
extern ISoundEmitterSystemBase *soundemitterbase;


extern AchievementsAndStatsInterface* g_pAchievementsAndStatsInterface;


// Returns the CPU/GPU level
CPULevel_t GetCPULevel();
// Returns the actual value of the CPU level convar, even on the 360
CPULevel_t GetActualCPULevel();
GPULevel_t GetGPULevel();
void ConfigureCurrentSystemLevel();

// Set to true between LevelInit and LevelShutdown.
extern bool	g_bLevelInitialized;
extern bool g_bTextMode;
extern bool g_bEngineIsHLTV;


// Returns true if a new OnDataChanged event is registered for this frame.
bool AddDataChangeEvent( IClientNetworkable *ent, DataUpdateType_t updateType, int *pStoredEvent );

void ClearDataChangedEvent( int iStoredEvent );
void PurgeOnDataChangedEvents(); 

//-----------------------------------------------------------------------------
// Precaches a material
//-----------------------------------------------------------------------------
void PrecacheMaterial( const char *pMaterialName );

//-----------------------------------------------------------------------------
// Converts a previously precached material into an index
//-----------------------------------------------------------------------------
int GetMaterialIndex( const char *pMaterialName );

//-----------------------------------------------------------------------------
// Converts precached material indices into strings
//-----------------------------------------------------------------------------
const char *GetMaterialNameFromIndex( int nIndex );

//-----------------------------------------------------------------------------
// Precache-related methods for particle systems
//-----------------------------------------------------------------------------
int PrecacheParticleSystem( const char *pParticleSystemName );
int GetParticleSystemIndex( const char *pParticleSystemName );
const char *GetParticleSystemNameFromIndex( int nIndex );

//-----------------------------------------------------------------------------
// Precache-related methods for movies
//-----------------------------------------------------------------------------
void PrecacheMovie( const char *pMovieName );
int GetMovieIndex( const char *pMovieName );
const char *GetMovieNameFromIndex( int nIndex );

//-----------------------------------------------------------------------------
// Precache-related methods for effects
//-----------------------------------------------------------------------------
void PrecacheEffect( const char *pEffectName );

//-----------------------------------------------------------------------------
// Called during bone setup to test perf
//-----------------------------------------------------------------------------
void TrackBoneSetupEnt( C_BaseAnimating *pEnt );

bool IsEngineThreaded();

class CUIScreenSizeSplitScreenPlayerGuard
{
public:
	CUIScreenSizeSplitScreenPlayerGuard( bool bActive, int slot, int nOldSlot );
	CUIScreenSizeSplitScreenPlayerGuard( bool bActive, C_BaseEntity *pEntity, int nOldSlot );
	~CUIScreenSizeSplitScreenPlayerGuard();
private:

	bool m_bNoRestore;
	bool m_bOldSetting;
	int m_nOldSize[ 2 ];
};

class CSetActiveSplitScreenPlayerGuard : public CUIScreenSizeSplitScreenPlayerGuard
{
public:
	CSetActiveSplitScreenPlayerGuard( char const *pchContext, int nLine );
	CSetActiveSplitScreenPlayerGuard( char const *pchContext, int nLine, int slot, int nOldSlot, bool bSetUIScreenSize );
	CSetActiveSplitScreenPlayerGuard( char const *pchContext, int nLine, C_BaseEntity *pEntity, int nOldSlot, bool bSetUIScreenSize );
	~CSetActiveSplitScreenPlayerGuard();
private:
	bool	m_bChanged;
	char const *m_pchContext;
	int m_nLine;
	int	 m_nSaveSlot;
	bool m_bSaveGetLocalPlayerAllowed;
};

class CHackForGetLocalPlayerAccessAllowedGuard
{
public:
	CHackForGetLocalPlayerAccessAllowedGuard( char const *pszContext, bool bOldSlot );
	~CHackForGetLocalPlayerAccessAllowedGuard();
private:
	bool m_bChanged;
	char const *m_pszContext;
	bool m_bSaveGetLocalPlayerAllowed;
};

class CUIAbsPosSplitScreenPlayerGuard
{
public:
	CUIAbsPosSplitScreenPlayerGuard( int slot, int nOldSlot, bool bInvert = false );
	~CUIAbsPosSplitScreenPlayerGuard();
private:
	bool m_bNoRestore;
};

int FirstValidSplitScreenSlot();
int NextValidSplitScreenSlot( int i );
bool IsValidSplitScreenSlot( int i );
void IterateRemoteSplitScreenViewSlots_Push( bool bSet ); //some split screen loops should be made aware of remote views we'd like to use
void IterateRemoteSplitScreenViewSlots_Pop( void );
class C_BasePlayer;
void AddRemoteSplitScreenViewPlayer( C_BasePlayer *pPlayer ); //adds a nonlocal player as a view we'd like to see in split screen
void RemoveRemoteSplitScreenViewPlayer( C_BasePlayer *pPlayer ); //removes a nonlocal player view from split screen
C_BasePlayer *GetSplitScreenViewPlayer( int nSlot ); //allows return of split screen views of nonlocal players
bool IsLocalSplitScreenPlayer( int nSlot ); //true is a full splitscreen player. false is just a view of a remote player


#define UI_SCREENSIZE_SPLITSCREEN_GUARD( slot ) 
#define ACTIVE_SPLITSCREEN_PLAYER_GUARD( slot )
#define ACTIVE_SPLITSCREEN_PLAYER_GUARD_ENT( entity )

#define ACTIVE_SPLITSCREEN_PLAYER_GUARD_UI( slot )
#define ACTIVE_SPLITSCREEN_PLAYER_GUARD_ENT_VGUI( entity )

#define HACK_GETLOCALPLAYER_GUARD( desc )
#define UI_ABSPOS_SPLITSCREEN_GUARD( slot )
#define UI_ABSPOS_SPLITSCREEN_GUARD_INVERT( slot )

#define FOR_EACH_VALID_SPLITSCREEN_PLAYER( iteratorName ) for ( int iteratorName = 0; iteratorName == 0; ++iteratorName )

#define ASSERT_LOCAL_PLAYER_RESOLVABLE()
#define ASSERT_LOCAL_PLAYER_NOT_RESOLVABLE() 
#define GET_ACTIVE_SPLITSCREEN_SLOT() ( 0 )

[[gnu::always_inline]] inline uint32 ComputeSplitscreenRenderingFlags( IClientRenderable *pRenderable )
{
	return 0xFFFFFFFF;
}

inline C_BasePlayer *GetSplitScreenViewPlayer( void ) { return GetSplitScreenViewPlayer( GET_ACTIVE_SPLITSCREEN_SLOT() ); };
inline bool IsLocalSplitScreenPlayer( void ) { return IsLocalSplitScreenPlayer( GET_ACTIVE_SPLITSCREEN_SLOT() ); };

// Returns XBX_GetUserId( GET_ACTIVE_SPLITSCREEN_SLOT() )

#define XBX_GetPrimaryUserId() _Use_XBX_GetActiveUserId_Instead
