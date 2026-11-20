// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2008 Valve Corporation
// Copyright (c) Valve Corporation

module;
#include <stddef.h>
#include "cam_thirdperson.h"
#include "IGameUIFuncs.h"
#include "cdll_int.h"
#include "eiface.h"
#include "engine/IEngineSound.h"
#include "engine/IEngineTrace.h"
#include "engine/IStaticPropMgr.h"
#include "engine/ivdebugoverlay.h"
#include "iclientnetworkable.h"
#include "iefx.h"
#include "iengineui.h"
#include "igameevents.h"
#include "inputsystem/iinputsystem.h"
#include "irendertorthelperobject.h"
#include "ivrenderview.h"
#include "scenefilecache/ISceneFileCache.h"
#include "tier2/tier2_logging.h"
#include "videocfg/videocfg.h"
#include "cdll_client_int.h"
#include "client_textmessage.h"
#include "cmodel.h"
#include "engine/ivmodelinfo.h"
#include "shake.h"
#include "vphysics_interface.h"
#include "cdll_util.h"
#include "iclientthinkable.h"
#include "client_thinklist.h"
#include "iclientalphaproperty.h"
#include "iclientrenderable.h"
#include "edict.h"
#include "client_class.h"
#include "declare_class.h"
#include "reflect_annotations.h"
#include "predictable_entity.h"
#include "const.h"
#include "materialsystem/imaterial.h"
#include "gamerules_register.h"
#include "string_t.h"
#include "materialsystem/imesh.h"
#include "materialsystem/itexture.h"
#include "rope_physics.h"
#include "rope_shared.h"
#include "dt_recv.h"
#include "dt_send.h"
#include "networkvar.h"
#include "view_shared.h"
#include "viewangleanim.h"
#include "usermessages_schema.h"
#include "filesystem.h"
#include "inputsystem/ButtonCode.h"
module ks.client:util;

import ks.math.compressed_vector;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.reflect_wirevar;
import ks.tier1.convar_serverbounded;
import ks.core.types;
import ks.tier1.interfaces;
import ks.core.basetypes;
import ks.core.color;
import ks.core.dbg;
import ks.core.platform;
import ks.core.wchartypes;
import :particlemgr;
import ks.tier1.createinterface;
import std;
import ks.tier1.slot_pool;
import ks.tier1.keyvalues;
import ks.tier1.map_ops;
import :baseentity;
import ks.tier1.exprevaluator;
import ks.tier1.convar;
import ks.math.mathlib;
import ks.game.shareddefs;
import ks.core.vprof;
import ks.tier1.intrusive_ptr;
import ks.core.random;
import ks.math.vector2d;
import ks.math.vmatrix;
import ks.tier1.bitbuf;
import ks.math.ssemath;
import ks.math.ssequaternion;
import ks.math.vplane;
import ks.tier1.cvar;
import ks.tier1.icvar;
import ks.game.postprocess_shared;
import ks.math.volumeculler;
import ks.tier1.iconvar;
import ks.tier1.refcount;
import ks.vstdlib.jobthread;
import :econ.econ_item_schema_minimal;

class CBasePlayer;

using namespace ::ks::reflect;


inline constexpr auto DIST_FORWARD = 0;
inline constexpr auto DIST_RIGHT = 1;
inline constexpr auto DIST_UP = 2;

//-------------------------------------------------- Constants

#define MAX_ANGLE_DIFF 10.0
#define PITCH_MAX 90.0
#define PITCH_MIN 0
#define YAW_MAX  135.0
#define YAW_MIN	 -135.0

inline constexpr auto CAMERA_UP_OFFSET = 25.0f;
inline constexpr auto CAMERA_OFFSET_LERP_TIME = 0.5f;
inline constexpr auto CAMERA_UP_OFFSET_LERP_TIME = 0.25f;

class CThirdPersonManager
{
public:

	CThirdPersonManager();
	void	SetCameraOffsetAngles( Vector vecOffset ) { m_vecCameraOffset = vecOffset; }
	Vector	GetCameraOffsetAngles( void ) { return m_vecCameraOffset; }

	void	SetDesiredCameraOffset( Vector vecOffset ) { m_vecDesiredCameraOffset = vecOffset; }
	Vector	GetDesiredCameraOffset( void );

	Vector	GetFinalCameraOffset( void );

	void	SetCameraOrigin( Vector vecOffset ) { m_vecCameraOrigin = vecOffset; }
	Vector	GetCameraOrigin( void ) { return m_vecCameraOrigin; }

	void	Update( void );

	void	PositionCamera( CBasePlayer *pPlayer, QAngle angles );

	void	UseCameraOffsets( bool bUse ) { m_bUseCameraOffsets = bUse; }
	bool	UsingCameraOffsets( void ) { return m_bUseCameraOffsets; }

	QAngle	GetCameraViewAngles( void ) { return m_ViewAngles; }

	Vector	GetDistanceFraction( void );

	bool	WantToUseGameThirdPerson( void );

	void	SetOverridingThirdPerson( bool bOverride ) { m_bOverrideThirdPerson = bOverride; }
	bool	IsOverridingThirdPerson( void ) { return m_bOverrideThirdPerson; }

	void	Init( void );

	void	SetForcedThirdPerson( bool bForced ) { m_bForced = bForced; }
	bool	GetForcedThirdPerson() const { return m_bForced; }

private:

	// What is the current camera offset from the view origin?
	Vector		m_vecCameraOffset;
	// Distances from the center
	Vector		m_vecDesiredCameraOffset;

	Vector m_vecCameraOrigin;

	bool	m_bUseCameraOffsets;

	QAngle	m_ViewAngles;

	float	m_flFraction;
	float	m_flUpFraction;

	float	m_flTargetFraction;
	float	m_flTargetUpFraction;

	bool	m_bOverrideThirdPerson;

	bool	m_bForced;

	float	m_flUpOffset;

	float	m_flLerpTime;
	float	m_flUpLerpTime;
};

extern CThirdPersonManager g_ThirdPersonManager;

#undef MAX_ANGLE_DIFF
#undef PITCH_MAX
#undef PITCH_MIN
#undef YAW_MAX
#undef YAW_MIN

// Purpose: Provides access to cvars that are bounded in the client DLL.

extern ConVar_ServerBounded *cl_predict;
extern ConVar_ServerBounded *cl_interp;

// Returns cl_interp_ratio / cl_updaterate.
float GetClientInterpAmount();

extern bool g_bForceCLPredictOff;	// If this is set to true, then prediction is forced off. Used temporarily for kill cam.

class AchievementsAndStatsInterface;
class CBaseAnimating;
class CBaseEntity;
class CBasePlayer;

class ISharedGameRules;

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
void TrackBoneSetupEnt( CBaseAnimating *pEnt );

bool IsEngineThreaded();

class CUIScreenSizeSplitScreenPlayerGuard
{
public:
	CUIScreenSizeSplitScreenPlayerGuard( bool bActive, int slot, int nOldSlot );
	CUIScreenSizeSplitScreenPlayerGuard( bool bActive, CBaseEntity *pEntity, int nOldSlot );
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
	CSetActiveSplitScreenPlayerGuard( char const *pchContext, int nLine, CBaseEntity *pEntity, int nOldSlot, bool bSetUIScreenSize );
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
void AddRemoteSplitScreenViewPlayer( CBasePlayer *pPlayer ); //adds a nonlocal player as a view we'd like to see in split screen
void RemoveRemoteSplitScreenViewPlayer( CBasePlayer *pPlayer ); //removes a nonlocal player view from split screen
CBasePlayer *GetSplitScreenViewPlayer( int nSlot ); //allows return of split screen views of nonlocal players
bool IsLocalSplitScreenPlayer( int nSlot ); //true is a full splitscreen player. false is just a view of a remote player

#define UI_SCREENSIZE_SPLITSCREEN_GUARD( slot ) 

#define ACTIVE_SPLITSCREEN_PLAYER_GUARD_ENT_VGUI( entity )

#define UI_ABSPOS_SPLITSCREEN_GUARD( slot )
#define UI_ABSPOS_SPLITSCREEN_GUARD_INVERT( slot )

[[gnu::always_inline]] inline uint32 ComputeSplitscreenRenderingFlags( IClientRenderable *pRenderable )
{
	return 0xFFFFFFFF;
}

inline CBasePlayer *GetSplitScreenViewPlayer( void ) { return GetSplitScreenViewPlayer( GET_ACTIVE_SPLITSCREEN_SLOT() ); };
inline bool IsLocalSplitScreenPlayer( void ) { return IsLocalSplitScreenPlayer( GET_ACTIVE_SPLITSCREEN_SLOT() ); };

// Returns XBX_GetUserId( GET_ACTIVE_SPLITSCREEN_SLOT() )

#define XBX_GetPrimaryUserId() _Use_XBX_GetActiveUserId_Instead

#undef UI_SCREENSIZE_SPLITSCREEN_GUARD
#undef ACTIVE_SPLITSCREEN_PLAYER_GUARD_ENT_VGUI
#undef UI_ABSPOS_SPLITSCREEN_GUARD
#undef UI_ABSPOS_SPLITSCREEN_GUARD_INVERT
#undef XBX_GetPrimaryUserId

class CBaseEntity;
class CBasePlayer;


extern bool g_MakingDevShots;

// ScreenHeight returns the height of the screen, in pixels
int		ScreenHeight( void );
// ScreenWidth returns the width of the screen, in pixels
int		ScreenWidth( void );

inline void ConvertNormalizedScreenSpaceToPixelScreenSpace( Vector &in )
{
	in.x = ( ( 1.0f + in.x ) * 0.5f ) * ScreenWidth();
	in.y = ScreenHeight() - ( ( ( 1.0f + in.y ) * 0.5f ) * ScreenHeight() );
}

float	UTIL_AngleDiff( float destAngle, float srcAngle );
void	UTIL_Bubbles( const Vector& mins, const Vector& maxs, int count );
void	UTIL_Smoke( const Vector &origin, const float scale, const float framerate );
void	UTIL_ImpactTrace( trace_t *pTrace, int iDamageType, const char *pCustomImpactName = nullptr );
int		UTIL_PrecacheDecal( const char *name, bool preload = false );
void	UTIL_EmitAmbientSound( CBaseEntity *entity, const Vector &vecOrigin, const char *samp, float vol, soundlevel_t soundlevel, int fFlags, int pitch );
void	UTIL_SetOrigin( CBaseEntity *entity, const Vector &vecOrigin );
void	UTIL_ScreenShake( const Vector &center, float amplitude, float frequency, float duration, float radius, ShakeCommand_t eCommand, bool bAirShake=false );
byte	*UTIL_LoadFileForMe( const char *filename, int *pLength );
void	UTIL_FreeFile( byte *buffer );
void	UTIL_MakeSafeName( const char *oldName, char *newName, int newNameBufSize );	///< Cleans up player names for putting in vgui controls (cleaned names can be up to original*2+1 in length)
const char *UTIL_SafeName( const char *oldName );	///< Wraps UTIL_MakeSafeName, and returns a static buffer
void	UTIL_ReplaceKeyBindings( const wchar_t *inbuf, int inbufsizebytes, wchar_t *outbuf, int outbufsizebytes );

void	UTIL_MessageText( CBasePlayer *player, const char *text, Color color = Color( 0, 0, 0, 0 ) );

// Fade out an entity based on distance fades
unsigned char UTIL_ComputeEntityFade( CBaseEntity *pEntity, float flMinDist, float flMaxDist, float flFadeScale );

client_textmessage_t	*TextMessageGet( const char *pName );

char	*VarArgs( const char *format, ... );
	
// Get the entity the local player is spectating (can be a player or a ragdoll entity).
int		GetSpectatorTarget();
bool	CanControlSpectatedTarget( void );
bool	CanSeeSpectatorOnlyTools( void );
bool	CanToggleXRayView( void );
int		GetSpectatorMode( void );
bool	IsPlayerIndex( int index );
int		GetLocalPlayerIndex( void );
int		GetLocalPlayerTeam( void );
bool	IsLocalPlayerSpectator( void );
void	NormalizeAngles( QAngle& angles );
void	InterpolateAngles( const QAngle& start, const QAngle& end, QAngle& output, float frac );
void	InterpolateVector( float frac, const Vector& src, const Vector& dest, Vector& output );

void UTIL_PrecacheOther( const char *szClassname );

void UTIL_SetTrace(trace_t& tr, const Ray_t& ray, CBaseEntity *edict, float fraction, int hitgroup, unsigned int contents, const Vector& normal, float intercept );

bool GetVectorInScreenSpace( Vector pos, int& iX, int& iY, Vector *vecOffset = nullptr );
bool GetTargetInScreenSpace( CBaseEntity *pTargetEntity, int& iX, int& iY, Vector *vecOffset = nullptr );

// prints messages through the HUD (stub in client .dll right now )
void ClientPrint( CBasePlayer *player, int msg_dest, const char *msg_name, const char *param1 = nullptr, const char *param2 = nullptr, const char *param3 = nullptr, const char *param4 = nullptr );

CBasePlayer* UTIL_PlayerByUserId( int userID );

CBaseEntity* UTIL_EntityFromUserMessageEHandle( long nEncodedEHandle );

// Pass in an array of pointers and an array size, it fills the array and returns the number inserted
int			UTIL_EntitiesInBox( CBaseEntity **pList, int listMax, const Vector &mins, const Vector &maxs, int flagMask, int partitionMask = PARTITION_CLIENT_NON_STATIC_EDICTS );
int			UTIL_EntitiesInSphere( CBaseEntity **pList, int listMax, const Vector &center, float radius, int flagMask, int partitionMask = PARTITION_CLIENT_NON_STATIC_EDICTS );

int			UTIL_RenderablesInBox( CBaseEntity** pList, int listMax, const Vector &mins, const Vector &maxs );

// make this a fixed size so it just sits on the stack
inline constexpr auto MAX_SPHERE_QUERY = 256;
class CEntitySphereQuery
{
public:
	// currently this builds the list in the constructor
	// UNDONE: make an iterative query of ISpatialPartition so we could
	// make queries like this optimal
	CEntitySphereQuery( const Vector &center, float radius, int flagMask=0, int partitionMask = PARTITION_CLIENT_NON_STATIC_EDICTS );
	CBaseEntity *GetCurrentEntity();
	inline void NextEntity() { m_listIndex++; }

private:
	int			m_listIndex;
	int			m_listCount;
	CBaseEntity *m_pList[MAX_SPHERE_QUERY];
};

CBaseEntity *CreateEntityByName( const char *className );
// creates an entity by name, and ensure it's correctness
// does not spawn the entity
// use the CREATE_ENTITY() macro which wraps this, instead of using it directly
template< class T >
T *_CreateEntity( T *newClass, const char *className )
{
	T *newEnt = dynamic_cast<T*>( CreateEntityByName(className) );
	if ( !newEnt )
	{
		Warning( "classname %s used to create wrong class type\n", className );
		contract_assert( 0 );
	}

	return newEnt;
}

#define CREATE_UNSAVED_ENTITY( newClass, className ) _CreateEntityTemplate( (newClass*)nullptr, className )

// Misc useful
inline bool FStrEq(const char *sz1, const char *sz2)
{
	return(stricmp(sz1, sz2) == 0);
}

// Given a vector, clamps the scalar axes to MAX_COORD_FLOAT ranges from worldsize.h
void UTIL_BoundToWorldSize( Vector *pVecPos );

void UTIL_ApproachTarget( float target, float increaseSpeed, float decreaseSpeed, float *val );
void UTIL_ApproachTarget( const Vector &target, float increaseSpeed, float decreaseSpeed, Vector *val );

// Returns true if the user has loaded any maps, false otherwise.
bool UTIL_HasLoadedAnyMap();

wchar_t *UTIL_GetLocalizedKeyString( const char *command, const char *fmt, const wchar_t *arg1 = nullptr, const wchar_t *arg2 = nullptr, const wchar_t *arg3 = nullptr );

void UTIL_GetClientStatusText( char *buffer, int bufsize );

void		UTIL_ClearTrace			( trace_t &trace );

#undef CREATE_UNSAVED_ENTITY

// Purpose: Hold definitions for all client animation events

//Animation event codes
inline constexpr auto CL_EVENT_MUZZLEFLASH0 = 5001;	// Muzzleflash on attachment 0
inline constexpr auto CL_EVENT_MUZZLEFLASH1 = 5011;	// Muzzleflash on attachment 1
inline constexpr auto CL_EVENT_MUZZLEFLASH2 = 5021;	// Muzzleflash on attachment 2
inline constexpr auto CL_EVENT_MUZZLEFLASH3 = 5031;	// Muzzleflash on attachment 3
inline constexpr auto CL_EVENT_SPARK0 = 5002;	// Spark on attachment 0
inline constexpr auto CL_EVENT_NPC_MUZZLEFLASH0 = 5003;	// Muzzleflash on attachment 0 for third person views
inline constexpr auto CL_EVENT_NPC_MUZZLEFLASH1 = 5013;	// Muzzleflash on attachment 1 for third person views
inline constexpr auto CL_EVENT_NPC_MUZZLEFLASH2 = 5023;	// Muzzleflash on attachment 2 for third person views
inline constexpr auto CL_EVENT_NPC_MUZZLEFLASH3 = 5033;	// Muzzleflash on attachment 3 for third person views
inline constexpr auto CL_EVENT_SOUND = 5004;	// Emit a sound // NOTE THIS MUST MATCH THE DEFINE AT CBaseEntity::PrecacheModel on the server!!!!!
inline constexpr auto CL_EVENT_EJECTBRASS1 = 6001;	// Eject a brass shell from attachment 1

inline constexpr auto CL_EVENT_DISPATCHEFFECT0 = 9001;	// Hook into a DispatchEffect on attachment 0
inline constexpr auto CL_EVENT_DISPATCHEFFECT1 = 9011;	// Hook into a DispatchEffect on attachment 1
inline constexpr auto CL_EVENT_DISPATCHEFFECT2 = 9021;	// Hook into a DispatchEffect on attachment 2
inline constexpr auto CL_EVENT_DISPATCHEFFECT3 = 9031;	// Hook into a DispatchEffect on attachment 3
inline constexpr auto CL_EVENT_DISPATCHEFFECT4 = 9041;	// Hook into a DispatchEffect on attachment 4
inline constexpr auto CL_EVENT_DISPATCHEFFECT5 = 9051;	// Hook into a DispatchEffect on attachment 5
inline constexpr auto CL_EVENT_DISPATCHEFFECT6 = 9061;	// Hook into a DispatchEffect on attachment 6
inline constexpr auto CL_EVENT_DISPATCHEFFECT7 = 9071;	// Hook into a DispatchEffect on attachment 7
inline constexpr auto CL_EVENT_DISPATCHEFFECT8 = 9081;	// Hook into a DispatchEffect on attachment 8
inline constexpr auto CL_EVENT_DISPATCHEFFECT9 = 9091;	// Hook into a DispatchEffect on attachment 9

// These two events are used by c_env_spritegroup.
// FIXME: Should this be local to c_env_spritegroup?
inline constexpr auto CL_EVENT_SPRITEGROUP_CREATE = 6002;
inline constexpr auto CL_EVENT_SPRITEGROUP_DESTROY = 6003;
inline constexpr auto CL_EVENT_FOOTSTEP_LEFT = 6004;
inline constexpr auto CL_EVENT_FOOTSTEP_RIGHT = 6005;
inline constexpr auto CL_EVENT_MFOOTSTEP_LEFT = 6006;	// Footstep sounds based on material underfoot.
inline constexpr auto CL_EVENT_MFOOTSTEP_RIGHT = 6007;
inline constexpr auto CL_EVENT_MFOOTSTEP_LEFT_LOUD = 6008;	// Loud material impact sounds from feet attachments
inline constexpr auto CL_EVENT_MFOOTSTEP_RIGHT_LOUD = 6009;

struct factorylist_t
{
	CreateInterfaceFn appSystemFactory;
};

// Store off the factories
void FactoryList_Store( const factorylist_t &sourceData );

// retrieve the stored factories
void FactoryList_Retrieve( factorylist_t &destData );

inline constexpr auto CLIENT_THINK_ALWAYS = -1293;
inline constexpr auto CLIENT_THINK_NEVER = -1;

class CClientThinkList : public IGameSystemPerFrame
{
public:

							CClientThinkList();
	virtual					~CClientThinkList();
	
	virtual char const		*Name() { return "CClientThinkList"; }
	virtual bool			IsPerFrame() { return true; }

	// Set the next time at which you want to think. You can also use
	// one of the CLIENT_THINK_ defines.
	void					SetNextClientThink( ClientEntityHandle_t hEnt, float nextTime );
	
	// Remove an entity from the think list.
	void					RemoveThinkable( ClientEntityHandle_t hEnt );

	// Use to initialize your think handles in IClientThinkables.
	ClientThinkHandle_t		GetInvalidThinkHandle();

	// This is called after network updating and before rendering.
	void					PerformThinkFunctions();

	// Call this to destroy a thinkable object - deletes the object post think.
	void					AddToDeleteList( ClientEntityHandle_t hEnt );	
	void					RemoveFromDeleteList( ClientEntityHandle_t hEnt );

// IClientSystem implementation.
public:

	virtual bool Init();
	virtual void PostInit() {};
	virtual void Shutdown();
	virtual void LevelInitPreEntity();
	virtual void LevelInitPostEntity() {}
	virtual void LevelShutdownPreEntity();
	virtual void LevelShutdownPostEntity();
	virtual void PreRender();
	virtual void PostRender() { }
	virtual void Update( float frametime );
	virtual void OnSave() {}
	virtual void OnRestore() {}
	virtual void SafeRemoveIfDesired() {}

private:
	struct ThinkEntry_t
	{
		ClientEntityHandle_t	m_hEnt;
		float					m_flNextClientThink;
		float					m_flLastClientThink;
		int						m_nIterEnum;
	};

	struct ThinkListChanges_t
	{
		ClientEntityHandle_t	m_hEnt;
		ClientThinkHandle_t		m_hThink;
		float					m_flNextTime;
	};

// Internal stuff.
private:
	void			SetNextClientThink( ClientThinkHandle_t hThink, float nextTime );
	void			RemoveThinkable( ClientThinkHandle_t hThink );
	void			PerformThinkFunction( ThinkEntry_t *pEntry, float curtime );
	ThinkEntry_t*	GetThinkEntry( ClientThinkHandle_t hThink );
	void			CleanUpDeleteList();

	// Add entity to frame think list
	void			AddEntityToFrameThinkList( ThinkEntry_t *pEntry, bool bAlwaysChain, int &nCount, ThinkEntry_t **ppFrameThinkList );

private:
	// Linked: list order is the think order; slot handles are handed out as ClientThinkHandle_t
	ks::linked_slot_pool<ThinkEntry_t, unsigned short>	m_ThinkEntries;

	std::vector< ClientEntityHandle_t >	m_aDeleteList;
	std::vector< ThinkListChanges_t >		m_aChangeList;

	// Makes sure the entries are thinked once per frame in the face of hierarchy
	int m_nIterEnum;
	bool m_bInThinkLoop;
};

// -------------------------------------------------------------------------------- //
// Inlines.
// -------------------------------------------------------------------------------- //

extern CClientThinkList g_ClientThinkList;
inline CClientThinkList* ClientThinkList()
{
	return &g_ClientThinkList;
}

//-----------------------------------------------------------------------------
// The client's own handle on the alpha property manager, which is the client leaf system (see
// clientleafsystem.cpp).
//-----------------------------------------------------------------------------
IClientAlphaPropertyMgr *ClientAlphaPropertyMgr();

inline constexpr auto CLIENT_ALPHA_DISTANCE_FADE_MODE_BIT_COUNT = 1;

//-----------------------------------------------------------------------------
// Implementation class
//-----------------------------------------------------------------------------
class CClientAlphaProperty : public IClientAlphaProperty
{
	// Inherited from IClientAlphaProperty
public:
	virtual IClientUnknown*	GetIClientUnknown();
	virtual void SetAlphaModulation( uint8 a );
	virtual void SetRenderFX( RenderFx_t nRenderFx, RenderMode_t nRenderMode, float flStartTime = std::numeric_limits<float>::max(), float flDuration = 0.0f );
	virtual void SetFade( float flGlobalFadeScale, float flDistFadeMinDist, float flDistFadeMaxDist );	
	virtual void SetDesyncOffset( int nOffset );
	virtual void EnableAlphaModulationOverride( bool bEnable );
	virtual void EnableShadowAlphaModulationOverride( bool bEnable );
	virtual void SetDistanceFadeMode( ClientAlphaDistanceFadeMode_t nFadeMode );

	// Other public methods
public:
	CClientAlphaProperty( );
	void Init( IClientUnknown *pUnk );

	// NOTE: Only the client shadow manager should ever call this method!
	void SetShadowHandle( ClientShadowHandle_t hShadowHandle );

	// Returns the current alpha modulation (no fades or render FX taken into account)
	uint8 GetAlphaModulation() const;

	// Compute the render alpha (after fades + render FX are applied)
	uint8 ComputeRenderAlpha( ) const;

	// Returns alpha fade
	float GetMinFadeDist() const;
	float GetMaxFadeDist() const;
	float GetGlobalFadeScale() const;

	// Should this ignore the Z buffer?
	bool IgnoresZBuffer( void ) const;

private:
	int ComputeRenderEffectBlend( int nRenderEffect ) const;

private:
	// NOTE: Be careful if you add data to this class.
	// It needs to be no more than 32 bytes, which it is right now
	// (remember the vtable adds 4 bytes). Try to restrict usage
	// to reserved areas or figure out a way of compressing existing fields
	IClientUnknown *m_pOuter;

	ClientShadowHandle_t m_hShadowHandle;
	uint16 m_nRenderFX : 5;
	uint16 m_nRenderMode : 4;
	uint16 m_bAlphaOverride : 1;
	uint16 m_bShadowAlphaOverride : 1;
	uint16 m_nDistanceFadeMode : CLIENT_ALPHA_DISTANCE_FADE_MODE_BIT_COUNT;
	uint16 m_nReserved : 4;

	uint16 m_nDesyncOffset;
	uint8 m_nAlpha;
	uint8 m_nReserved2;

	uint16 m_nDistFadeStart;
	uint16 m_nDistFadeEnd;

	float m_flFadeScale;
	float m_flRenderFxStartTime;
	float m_flRenderFxDuration;

	friend class CClientLeafSystem;
};

class CBaseEntity;

// Purpose: Client handler for instruction players how to play

typedef CHandle<CBaseEntity> EHANDLE;


#define DECLARE_LESSON( _lessonClassName, _baseLessonClassName ) \
	typedef _baseLessonClassName BaseClass;\
	typedef _lessonClassName ThisClass;\
	_lessonClassName( const char *pchName, bool bIsDefaultHolder, bool bIsOpenOpportunity, int nSplitScreenSlot )\
		: _baseLessonClassName( pchName, bIsDefaultHolder, bIsOpenOpportunity, nSplitScreenSlot )\
	{\
		Init();\
	}

enum LessonInstanceType
{
	LESSON_INSTANCE_MULTIPLE,
	LESSON_INSTANCE_SINGLE_OPEN,
	LESSON_INSTANCE_FIXED_REPLACE,
	LESSON_INSTANCE_SINGLE_ACTIVE,

	LESSON_INSTANCE_TYPE_TOTAL
};

// This is used to solve a problem where bots can take the place of a player, where on or the other don't have valid entities on the client at the same time
inline constexpr auto MAX_DELAYED_PLAYER_SWAPS = 8;

struct delayed_player_swap_t
{
	CHandle<CBaseEntity> *phHandleToChange;
	int iNewUserID;

	delayed_player_swap_t( void );
};

class CBaseLesson : public CGameEventListener
{
public:

	CBaseLesson( const char *pchName, bool bIsDefaultHolder, bool bIsOpenOpportunity, int nSplitScreenSlot );
	virtual ~CBaseLesson( void );

	void AddPrerequisite( const char *pchLessonName );

	const CGameInstructorSymbol& GetNameSymbol( void ) const { return m_stringName; }
	const char * GetName( void ) const { return m_stringName.String(); }
	int GetPriority( void ) const { return m_iPriority;	}
	const char * GetCloseReason( void ) const { return m_stringCloseReason.String(); }
	void SetCloseReason( const char *pchReason ) { m_stringCloseReason = pchReason; }

	CBaseLesson* GetRoot( void ) const { return m_pRoot; }
	void SetRoot( CBaseLesson *pRoot );
	const std::vector< CBaseLesson * >* GetChildren( void ) const { return &m_OpenOpportunities; }

	float GetInitTime( void ) { return m_fInitTime; }
	void SetStartTime( void ) { m_fStartTime = gpGlobals->curtime; }
	void ResetStartTime( void ) { m_fStartTime = 0.0f; m_bHasPlayedSound = false; }

	bool ShouldShowSpew( void );
	bool NoPriority( void ) const;
	bool IsDefaultHolder( void ) const { return m_bIsDefaultHolder; }
	bool IsOpenOpportunity( void ) const { return m_bIsOpenOpportunity; }
	bool IsLocked( void ) const;
	bool CanOpenWhenDead( void ) const { return m_bCanOpenWhenDead; }
	bool CanOpenOnceLearned( void ) const { return !m_bOnceLearnedNeverOpen; }
	bool IsUsableInMidair( void ) const { return m_bUsableInMidair; }
	bool IsInstructing( void ) const { return ( m_fStartTime > 0.0f ); }
	bool IsLearned( void ) const;
	bool PrerequisitesHaveBeenMet( void ) const;
	bool IsTimedOut( void );

	int InstanceType( void ) const { return m_iInstanceType; }
	const CGameInstructorSymbol& GetReplaceKeySymbol( void ) const { return m_stringReplaceKey; }
	const char* GetReplaceKey( void ) const { return m_stringReplaceKey.String(); }
	int GetFixedInstancesMax( void ) const { return m_iFixedInstancesMax; }
	bool ShouldReplaceOnlyWhenStopped( void ) const { return m_bReplaceOnlyWhenStopped; }
	void SetInstanceActive( bool bInstanceActive ) { m_bInstanceActive = bInstanceActive; }
	bool IsInstanceActive( void ) const { return m_bInstanceActive; }

	void ResetDisplaysAndSuccesses( void );
	bool IncDisplayCount( void );
	bool IncSuccessCount( void );
	void SetDisplayCount( int iDisplayCount ) { m_iDisplayCount = iDisplayCount; }
	void SetSuccessCount( int iSuccessCount ) { m_iSuccessCount = iSuccessCount; }
	int GetDisplayCount( void ) const { return m_iDisplayCount; }
	int GetSuccessCount( void ) const { return m_iSuccessCount; }
	int GetDisplayLimit( void ) const { return m_iDisplayLimit; }
	int GetSuccessLimit( void ) const { return m_iSuccessLimit; }

	void Init( void );	// NOT virtual, each constructor calls their own
	virtual void InitPrerequisites( void ) {};
	virtual void Start( void ) = 0;
	virtual void Stop( void ) = 0;
	virtual void OnOpen( void ) {};
	virtual void Update( void ) {};
	virtual void UpdateInactive( void ) {};

	virtual bool ShouldDisplay( void ) const { return true; }
	virtual bool IsVisible( void ) const { return true; }
	virtual bool WasDisplayed( void ) const { return m_bWasDisplayed ? true : false; }
	virtual void SwapOutPlayers( int iOldUserID, int iNewUserID ) {}
	virtual void TakePlaceOf( CBaseLesson *pLesson );

	int	 GetSplitScreenSlot() const { return m_nSplitScreenSlot; }

	const char *GetGroup() { return m_szLessonGroup.String(); }
	void	SetEnabled( bool bEnabled ) { m_bDisabled = !bEnabled; }

protected:

	void MarkSucceeded( void );
	void CloseOpportunity( const char *pchReason );
	bool DoDelayedPlayerSwaps( void ) const;

private:

	CBaseLesson							*m_pRoot;
	std::vector< CBaseLesson * >		m_OpenOpportunities;
	std::vector< const CBaseLesson * >	m_Prerequisites;

	CGameInstructorSymbol		m_stringCloseReason;

protected:

	CGameInstructorSymbol		m_stringName;

private:

	bool		m_bInstanceActive : 1;
	bool		m_bSuccessCounted : 1;
	bool		m_bIsDefaultHolder : 1;
	bool		m_bIsOpenOpportunity : 1;

protected:

	LessonInstanceType	m_iInstanceType;

	int						m_iPriority;
	CGameInstructorSymbol	m_stringReplaceKey;
	int						m_iFixedInstancesMax;
	bool					m_bReplaceOnlyWhenStopped;
	int						m_iTeam;
	bool					m_bOnlyKeyboard;
	bool					m_bOnlyGamepad;
	bool					m_bNoSplitscreen;

	int		m_iDisplayLimit;
	int		m_iDisplayCount;
	bool	m_bWasDisplayed;
	int		m_iSuccessLimit;
	int		m_iSuccessCount;
	int		m_nSplitScreenSlot;

	float	m_fLockDuration;
	float	m_fTimeout;
	float	m_fInitTime;
	float	m_fStartTime;
	float	m_fLockTime;
	float	m_fUpdateInterval;
	bool 	m_bHasPlayedSound;

	CGameInstructorSymbol m_szStartSound;
	CGameInstructorSymbol m_szLessonGroup;

	bool	m_bCanOpenWhenDead;
	bool	m_bBumpWithTimeoutWhenLearned;
	bool	m_bOnceLearnedNeverOpen;
	bool	m_bCanTimeoutWhileInactive;
	bool	m_bDisabled;
	bool	m_bUsableInMidair;

	// Right now we can only queue up 4 swaps...
	// this number can be increased if more entity handle scripted variables are added
	mutable delayed_player_swap_t	m_pDelayedPlayerSwap[ MAX_DELAYED_PLAYER_SWAPS ];
	mutable int						m_iNumDelayedPlayerSwaps;

public:

	// Colors for console spew in verbose mode
	static Color	m_rgbaVerboseHeader;
	static Color	m_rgbaVerbosePlain;
	static Color	m_rgbaVerboseName;
	static Color	m_rgbaVerboseOpen;
	static Color	m_rgbaVerboseClose;
	static Color	m_rgbaVerboseSuccess;
	static Color	m_rgbaVerboseUpdate;
};

class CTextLesson : public CBaseLesson
{
public:
	DECLARE_LESSON( CTextLesson, CBaseLesson );

	void Init( void );	// NOT virtual, each constructor calls their own
	virtual void Start( void );
	virtual void Stop( void );

protected:

	CGameInstructorSymbol	m_szDisplayText;
	CGameInstructorSymbol	m_szDisplayParamText;
	CGameInstructorSymbol	m_szBinding;
	CGameInstructorSymbol	m_szGamepadBinding;
};

class CIconLesson : public CTextLesson
{
public:
	DECLARE_LESSON( CIconLesson, CTextLesson );

	void Init( void ); 	// NOT virtual, each constructor calls their own
	virtual void Start( void );
	virtual void Stop( void );
	virtual void Update( void );
	virtual void UpdateInactive( void );

	virtual bool ShouldDisplay( void ) const;
	virtual bool IsVisible( void ) const;
	virtual void SwapOutPlayers( int iOldUserID, int iNewUserID );
	virtual void TakePlaceOf( CBaseLesson *pLesson );

	void SetLocatorBinding( CLocatorTarget * pLocatorTarget );

	const char *GetCaptionColorString()	{ return m_szCaptionColor.String(); }

	bool IsPresentComplete( void );
	void PresentStart( void );
	void PresentEnd( void );

private:
	virtual void UpdateLocatorTarget( CLocatorTarget *pLocatorTarget, CBaseEntity *pIconTarget );

protected:
	CHandle<CBaseEntity>		m_hIconTarget;
	CGameInstructorSymbol		m_szUITargetName;
	CGameInstructorSymbol		m_szUITargetLookup;
	int		m_nUITargetEdge;
	float	m_flUpOffset;
	float	m_flRelativeUpOffset;
	float	m_fFixedPositionX;
	float	m_fFixedPositionY;

	int		m_hLocatorTarget;
	int		m_iFlags;

	float	m_fRange;
	float	m_fCurrentDistance;
	float	m_fOnScreenStartTime;
	float	m_fUpdateDistanceTime;

	CGameInstructorSymbol	m_szOnscreenIcon;
	CGameInstructorSymbol	m_szOffscreenIcon;
	CGameInstructorSymbol	m_szCaptionColor;

	bool	m_bFixedPosition;
	bool	m_bNoIconTarget;
	bool	m_bAllowNodrawTarget;
	bool	m_bVisible;
	bool	m_bShowWhenOccluded;
	bool	m_bNoOffscreen;
	bool	m_bForceCaption;
};

enum LessonAction
{
	LESSON_ACTION_NONE,

	LESSON_ACTION_SCOPE_IN,
	LESSON_ACTION_SCOPE_OUT,
	LESSON_ACTION_CLOSE,
	LESSON_ACTION_SUCCESS,
	LESSON_ACTION_LOCK,
	LESSON_ACTION_PRESENT_COMPLETE,
	LESSON_ACTION_PRESENT_START,
	LESSON_ACTION_PRESENT_END,

	LESSON_ACTION_REFERENCE_OPEN,

	LESSON_ACTION_IS_MULTIPLAYER,

	LESSON_ACTION_SET,
	LESSON_ACTION_ADD,
	LESSON_ACTION_SUBTRACT,
	LESSON_ACTION_MULTIPLY,
	LESSON_ACTION_IS,
	LESSON_ACTION_LESS_THAN,
	LESSON_ACTION_HAS_PREFIX,
	LESSON_ACTION_HAS_BIT,
	LESSON_ACTION_BIT_COUNT_IS,
	LESSON_ACTION_BIT_COUNT_LESS_THAN,

	LESSON_ACTION_GET_DISTANCE,
	LESSON_ACTION_GET_ANGULAR_DISTANCE,
	LESSON_ACTION_GET_PLAYER_DISPLAY_NAME,
	LESSON_ACTION_CLASSNAME_IS,
	LESSON_ACTION_MODELNAME_IS,
	LESSON_ACTION_TEAM_IS,
	LESSON_ACTION_HEALTH_LESS_THAN,
	LESSON_ACTION_HEALTH_PERCENTAGE_LESS_THAN,
	LESSON_ACTION_GET_ACTIVE_WEAPON,
	LESSON_ACTION_WEAPON_IS,
	LESSON_ACTION_WEAPON_HAS,
	LESSON_ACTION_GET_ACTIVE_WEAPON_SLOT,
	LESSON_ACTION_GET_WEAPON_SLOT,
	LESSON_ACTION_GET_WEAPON_IN_SLOT,
	LESSON_ACTION_CLIP_PERCENTAGE_LESS_THAN,
	LESSON_ACTION_WEAPON_AMMO_LOW,
	LESSON_ACTION_WEAPON_AMMO_FULL,
	LESSON_ACTION_WEAPON_AMMO_EMPTY,
	LESSON_ACTION_WEAPON_CAN_USE,
	LESSON_ACTION_USE_TARGET_IS,
	LESSON_ACTION_GET_USE_TARGET,
	LESSON_ACTION_GET_POTENTIAL_USE_TARGET,

	// Enum continued in Mod_LessonAction
	LESSON_ACTION_MOD_START,
};

struct LessonElement_t
{
	int						iVariable;
	int						iParamVarIndex;
	int						iAction;
	_fieldtypes				paramType;
	CGameInstructorSymbol	szParam;
	bool					bNot : 1;
	bool					bOptionalParam : 1;

	LessonElement_t( int p_iVariable, int p_iAction, bool p_bNot, bool p_bOptionalParam, const char *pchParam, int p_iParamVarIndex, _fieldtypes p_paramType );

	LessonElement_t( const LessonElement_t &p_LessonElement );
};

struct LessonEvent_t
{
	std::vector< LessonElement_t >	elements;
	CGameInstructorSymbol			szEventName;
};

int LessonActionFromString( const char *pchName );

class CScriptedIconLesson : public CIconLesson
{
public:
	DECLARE_LESSON( CScriptedIconLesson, CIconLesson )
	DECLARE_SIMPLE_DATADESC();

	virtual ~CScriptedIconLesson( void );

	static void PreReadLessonsFromFile( void );
	static void Mod_PreReadLessonsFromFile( void );

	void Init( void );	// NOT virtual, each constructor calls their own
	virtual void InitPrerequisites( void );
	virtual void OnOpen( void );
	virtual void Update( void );

	virtual void SwapOutPlayers( int iOldUserID, int iNewUserID );

	void FireGameEvent( IGameEvent *event );
	virtual void ProcessOpenGameEvents( const CScriptedIconLesson *pRootLesson, const char *name, IGameEvent *event );
	virtual void ProcessCloseGameEvents( const CScriptedIconLesson *pRootLesson, const char *name, IGameEvent *event );
	virtual void ProcessSuccessGameEvents( const CScriptedIconLesson *pRootLesson, const char *name, IGameEvent *event );

	std::vector< LessonEvent_t >& GetOpenEvents( void ) { return m_OpenEvents; }
	std::vector< LessonEvent_t >& GetCloseEvents( void ) { return m_CloseEvents; }
	std::vector< LessonEvent_t >& GetSuccessEvents( void ) { return m_SuccessEvents; }
	std::vector< LessonEvent_t >& GetOnOpenEvents( void ) { return m_OnOpenEvents; }
	std::vector< LessonEvent_t >& GetUpdateEvents( void ) { return m_UpdateEvents; }

	bool ProcessElements( IGameEvent *event, const std::vector< LessonElement_t > *pElements );

private:
	void InitElementsFromKeys( std::vector< LessonElement_t > *pLessonElements, KeyValues *pKey );
	void InitElementsFromElements( std::vector< LessonElement_t > *pLessonElements, const std::vector< LessonElement_t > *pLessonElements2 );

	void InitFromKeys( KeyValues *pKey );

	bool ProcessElement( IGameEvent *event, const LessonElement_t *pLessonElement, bool bInFailedScope );

	bool ProcessElementAction( int iAction, bool bNot, const char *pchVarName, float &bVar, const CGameInstructorSymbol *pchParamName, float fParam );
	bool ProcessElementAction( int iAction, bool bNot, const char *pchVarName, int &bVar, const CGameInstructorSymbol *pchParamName, float fParam );
	bool ProcessElementAction( int iAction, bool bNot, const char *pchVarName, bool &bVar, const CGameInstructorSymbol *pchParamName, float fParam );
	bool ProcessElementAction( int iAction, bool bNot, const char *pchVarName, EHANDLE &hVar, const CGameInstructorSymbol *pchParamName, float fParam, CBaseEntity *pParam, const char *pchParam );
	bool ProcessElementAction( int iAction, bool bNot, const char *pchVarName, CGameInstructorSymbol *pchVar, const CGameInstructorSymbol *pchParamName, const char *pchParam );

	// Implemented per mod so they can have custom actions
	bool Mod_ProcessElementAction( int iAction, bool bNot, const char *pchVarName, EHANDLE &hVar, const CGameInstructorSymbol *pchParamName, float fParam, CBaseEntity *pParam, const char *pchParam, bool &bModHandled );

	LessonEvent_t * AddOpenEvent( void );
	LessonEvent_t * AddCloseEvent( void );
	LessonEvent_t * AddSuccessEvent( void );
	LessonEvent_t * AddOnOpenEvent( void );
	LessonEvent_t * AddUpdateEvent( void );

private:
	static ks::idict< int > LessonActionMap;

	EHANDLE					m_hLocalPlayer;
	float					m_fOutput;
	CHandle<CBaseEntity>	m_hEntity1;
	CHandle<CBaseEntity>	m_hEntity2;
	CGameInstructorSymbol	m_szString1;
	CGameInstructorSymbol	m_szString2;
	int						m_iInteger1;
	int						m_iInteger2;
	float					m_fFloat1;
	float					m_fFloat2;

	std::vector< CGameInstructorSymbol >	m_PrerequisiteNames;
	std::vector< LessonEvent_t >	m_OpenEvents;
	std::vector< LessonEvent_t >	m_CloseEvents;
	std::vector< LessonEvent_t >	m_SuccessEvents;
	std::vector< LessonEvent_t >	m_OnOpenEvents;
	std::vector< LessonEvent_t >	m_UpdateEvents;

	float					m_fUpdateEventTime;
	CScriptedIconLesson		*m_pDefaultHolder;

	int		m_iScopeDepth;

	// Need this to get offsets to scripted variables
	friend class LessonVariableInfo;
	friend int LessonActionFromString( const char *pchName );
};

#undef DECLARE_LESSON

class CParticleEffectBinding;
class CParticleRenderIterator;
class CParticleSimulateIterator;
class CBaseEntity;
class IParticleEffect;

using namespace ::ks::reflect;

// Particle system entities can derive from this to handle some of the mundane
// functionality of hooking into the engine's entity system.

class CBaseParticleEntity : public CBaseEntity
, public IParticleEffect
{
	[[= ks::reflect::NetTable{ .name = "DT_BaseParticleEntity" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CBaseParticleEntity, CBaseEntity );
	DECLARE_PREDICTABLE();
	DECLARE_NETWORKCLASS();

	CBaseParticleEntity();
	virtual ~CBaseParticleEntity();

	// CBaseEntity overrides.
public:
// Default IParticleEffect overrides.
public:

	virtual bool	ShouldSimulate() const { return m_bSimulate; }
	virtual void	SetShouldSimulate( bool bSim ) { m_bSimulate = bSim; }

	virtual void	SimulateParticles( CParticleSimulateIterator *pIterator );
	virtual void	RenderParticles( CParticleRenderIterator *pIterator );
	virtual const Vector & GetSortOrigin();
public:
	CParticleEffectBinding	m_ParticleEffect;

	virtual void	Activate();
	virtual void	Think();	

	// NOTE: Ths enclosed particle effect binding will do all the drawing
	virtual bool	ShouldDraw() { return false; }

	int				AllocateToolParticleEffectId();
	int				GetToolParticleEffectId() const;

private:
	int				m_nToolParticleEffectId;
	bool			m_bSimulate;

public:
	void			FollowEntity(CBaseEntity *pEntity);
	
	// UTIL_Remove will be called after the specified amount of time.
	// If you pass in -1, the entity will never go away automatically.
	void			SetLifetime(float lifetime);

private:
	CBaseParticleEntity( const CBaseParticleEntity & ); // not defined, not accessible
};

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
typedef struct wrect_s wrect_t;

//-----------------------------------------------------------------------------
// Purpose: Sprite Models
//-----------------------------------------------------------------------------
class CEngineSprite
{
	// NOTE: don't define a constructor or destructor so that this can be allocated
	// as before.
public:
	int GetWidth() { return m_width; }
	int GetHeight() { return m_height; }
	int GetNumFrames() { return m_numFrames; }
	IMaterial *GetMaterial( RenderMode_t nRenderMode ) { return m_material[nRenderMode]; }
	IMaterial *GetMaterial( RenderMode_t nRenderMode, int nFrame );
	void SetFrame( RenderMode_t nRenderMode, int nFrame );
	bool Init( const char *name );
	void Shutdown( void );
	void UnloadMaterial();
	void SetColor( float r, float g, float b );
	int GetOrientation( void );
	void GetHUDSpriteColor( float* color );
	float GetUp() { return up; }
	float GetDown() { return down; }
	float GetLeft() { return left; }
	float GetRight() { return right; }
	void DrawFrame( RenderMode_t nRenderMode, int frame, int x, int y, const wrect_t *prcSubRect );
	void DrawFrameOfSize( RenderMode_t nRenderMode, int frame, int x, int y, int iWidth, int iHeight, const wrect_t *prcSubRect);
	void GetTexCoordRange( float *pMinU, float *pMinV, float *pMaxU, float *pMaxV );

private:
	int m_width;
	int m_height;
	int m_numFrames;
	IMaterial *m_material[kRenderModeCount];
	int m_orientation;
	float m_hudSpriteColor[3];
	float up, down, left, right;
};

//=============================================================================
//
// Smoothly blends the timescale through an engine interface
//
class CGameTimescale : public CAutoGameSystemPerFrame
{
public:

	enum Interpolators_e
	{
		INTERPOLATOR_LINEAR,
		INTERPOLATOR_ACCEL,
		INTERPOLATOR_DEACCEL,
		INTERPOLATOR_EASE_IN_OUT,
	};

	// Creation/Destruction.
	CGameTimescale();
	~CGameTimescale();

	// Initialization/Shutdown.
	virtual bool Init();	
	virtual void Shutdown();

	virtual void Update( float frametime );

	// Level init, shutdown
	virtual void LevelInitPostEntity();
	virtual void LevelShutdownPostEntity();

	float GetCurrentTimescale( void ) const { return m_flCurrentTimescale; }
	float GetDesiredTimescale( void ) const { return m_flDesiredTimescale; }

	// Set the timescale to an exact value without doing a ramp in/out blend
	void SetCurrentTimescale( float flTimescale );

	// Sets the desired timescale and will automatically ramp in/out
	void SetDesiredTimescaleAtTime( float flDesiredTimescale, float flDurationRealTimeSeconds = 0.0f, Interpolators_e nInterpolatorType = INTERPOLATOR_LINEAR, float flStartBlendTime = 0.0f );
	void SetDesiredTimescale( float flDesiredTimescale, float flDurationRealTimeSeconds = 0.0f, Interpolators_e nInterpolatorType = INTERPOLATOR_LINEAR, float flDelayRealtime = 0.0f );

private:

	void UpdateTimescale( void );
	void ResetTimescale( void );

private:

	float m_flDesiredTimescale;
	float m_flCurrentTimescale;

	float m_flDurationRealTimeSeconds;
	Interpolators_e m_nInterpolatorType;

	float m_flStartTimescale;
	float m_flStartBlendTime;
	float m_flStartBlendRealtime;
};

CGameTimescale *GameTimescale();

class CBaseLesson;
class CBaseEntity;
class CBasePlayer;

// Purpose: Client handler for instruction players how to play

typedef CHandle<CBaseEntity> EHANDLE;



struct LessonGroupConVarToggle_t
{
	ConVarRef var;
	char szLessonGroupName[ 64 ];

	explicit LessonGroupConVarToggle_t( const char *pchConVarName ) :
		var( pchConVarName )
	{
	}
};

class CGameInstructor : public CAutoGameSystemPerFrame, public CGameEventListener
{
public:
	CGameInstructor();

	void SetSlot( int nSlot ) { m_nSplitScreenSlot = nSlot; }

	// Methods of IGameSystem
	virtual bool Init( void );
	virtual void Shutdown( void );
	virtual void Update( float frametime );

	void UpdateHiddenByOtherElements( void );
	bool Mod_HiddenByOtherElements( void );

	void FireGameEvent( IGameEvent *event );

	void DefineLesson( CBaseLesson *pLesson );

	const CBaseLesson * GetLesson( const char *pchLessonName );
	bool IsLessonOfSameTypeOpen( const CBaseLesson *pLesson ) const;

	// Save / Restore

	bool ReadSaveData( void );
	bool WriteSaveData( void );
	void KeyValueBuilder( KeyValues *pKeyValues );
	void RefreshDisplaysAndSuccesses( void );
	void ResetDisplaysAndSuccesses( void );
	void MarkDisplayed( const char *pchLessonName );
	void MarkSucceeded( const char *pchLessonName );

	void PlaySound( const char *pchSoundName );

	bool OpenOpportunity( CBaseLesson *pLesson );

	void DumpOpenOpportunities( void );

	KeyValues * GetScriptKeys( void );
	CBasePlayer * GetLocalPlayer( void );

	void EvaluateLessonsForGameRules( void );
	void SetLessonGroupEnabled( const char *pszGroup, bool bEnabled );

private:
	void FindErrors( void );

	bool UpdateActiveLesson( CBaseLesson *pLesson, const CBaseLesson *pRootLesson );
	void UpdateInactiveLesson( CBaseLesson *pLesson );

	CBaseLesson * GetLesson_Internal( const char *pchLessonName );

	void StopAllLessons( void );

	void CloseAllOpenOpportunities( void );
	void CloseOpportunity( CBaseLesson *pLesson );

	void ReadLessonsFromFile( const char *pchFileName );
	void InitLessonPrerequisites( void );

private:
	std::vector< CBaseLesson* >	m_Lessons;
	std::vector< CBaseLesson* >	m_OpenOpportunities;

	std::vector< LessonGroupConVarToggle_t > m_LessonGroupConVarToggles;

	KeyValues	*m_pScriptKeys;

	bool	m_bNoDraw;
	bool	m_bHiddenDueToOtherElements;

	int		m_iCurrentPriority;
	EHANDLE	m_hLastSpectatedPlayer;
	bool	m_bSpectatedPlayerChanged;

	char	m_szPreviousStartSound[ 128 ];
	float	m_fNextStartSoundTime;
	int		m_nSplitScreenSlot;

	bool	m_bHasLoadedSaveData;
	bool	m_bEnsureThatInitIsNotCalledMultipleTimes;
};

CGameInstructor &GetGameInstructor();

class CBaseCombatCharacter;
class CBaseCombatWeapon;
class CBaseEntity;
class CBasePlayer;

using namespace ::ks::reflect;

// Debug history should be disabled in release builds
//#define DISABLE_DEBUG_HISTORY

//#include "items.h"
class CItem;

extern ConVar sk_autoaim_mode;

// Autoaiming modes
enum
{
	AUTOAIM_NONE = 0,		// No autoaim at all.
	AUTOAIM_ON,				// Autoaim is on.
	AUTOAIM_ON_CONSOLE,		// Autoaim is on, including enhanced features for Console gaming (more assistance, etc)
};

// weapon respawning return codes
enum
{	
	GR_NONE = 0,
	
	GR_WEAPON_RESPAWN_YES,
	GR_WEAPON_RESPAWN_NO,
	
	GR_AMMO_RESPAWN_YES,
	GR_AMMO_RESPAWN_NO,
	
	GR_ITEM_RESPAWN_YES,
	GR_ITEM_RESPAWN_NO,

	GR_PLR_DROP_GUN_ALL,
	GR_PLR_DROP_GUN_ACTIVE,
	GR_PLR_DROP_GUN_NO,

	GR_PLR_DROP_AMMO_ALL,
	GR_PLR_DROP_AMMO_ACTIVE,
	GR_PLR_DROP_AMMO_NO,
};

// Player relationship return codes
enum
{
	GR_NOTTEAMMATE = 0,
	GR_TEAMMATE,
	GR_ENEMY,
	GR_ALLY,
	GR_NEUTRAL,
};

// This class has the data tables and gets the CGameRules data to the client.
class CGameRulesProxy : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_GameRulesProxy", .base = false } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CGameRulesProxy, CBaseEntity );
	DECLARE_NETWORKCLASS();

	CGameRulesProxy();
	~CGameRulesProxy();

	// UNDONE: Is this correct, Mike?
	// Don't carry these across a transition, they are recreated.
	virtual int	ObjectCaps( void ) { return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	// ALWAYS transmit to all clients.
	virtual int UpdateTransmitState( void );

	// CGameRules chains its NetworkStateChanged calls to here, since this
	// is the actual entity that will send the data.
	static void NotifyNetworkStateChanged();

private:
	
	static CGameRulesProxy *s_pGameRulesProxy;
};

class CGameRules : public CMemZeroOnNew, public CAutoGameSystemPerFrame
{
public:
	DECLARE_CLASS_GAMEROOT( CGameRules, CAutoGameSystemPerFrame );

	virtual char const *Name() { return "CGameRules"; }

	// Stuff shared between client and server.

	CGameRules(void);

	virtual ~CGameRules( void );

	virtual	bool	Init();

	// Damage Queries - these need to be implemented by the various subclasses (single-player, multi-player, etc).
	// The queries represent queries against damage types and properties.
	virtual bool	Damage_IsTimeBased( int iDmgType ) = 0;			// Damage types that are time-based.
	virtual bool	Damage_ShouldGibCorpse( int iDmgType ) = 0;		// Damage types that gib the corpse.
	virtual bool	Damage_ShowOnHUD( int iDmgType ) = 0;			// Damage types that have client HUD art.
	virtual bool	Damage_NoPhysicsForce( int iDmgType ) = 0;		// Damage types that don't have to supply a physics force & position.
	virtual bool	Damage_ShouldNotBleed( int iDmgType ) = 0;		// Damage types that don't make the player bleed.
	//Temp: These will go away once DamageTypes become enums.
	virtual int		Damage_GetTimeBased( void ) = 0;				// Actual bit-fields.
	virtual int		Damage_GetShouldGibCorpse( void ) = 0;
	virtual int		Damage_GetShowOnHud( void ) = 0;					
	virtual int		Damage_GetNoPhysicsForce( void )= 0;
	virtual int		Damage_GetShouldNotBleed( void ) = 0;

// Ammo Definitions
	//CAmmoDef* GetAmmoDef();

	virtual bool SwitchToNextBestWeapon( CBaseCombatCharacter *pPlayer, CBaseCombatWeapon *pCurrentWeapon ); // Switch to the next best weapon
	virtual CBaseCombatWeapon *GetNextBestWeapon( CBaseCombatCharacter *pPlayer, CBaseCombatWeapon *pCurrentWeapon ); // I can't use this weapon anymore, get me the next best one.
	virtual bool ShouldCollide( int collisionGroup0, int collisionGroup1 );

	virtual int DefaultFOV( void ) { return 90; }

	// This function is here for our CNetworkVars.
	inline void NetworkStateChanged()
	{
		// Forward the call to the entity that will send the data.
		CGameRulesProxy::NotifyNetworkStateChanged();
	}

	inline void NetworkStateChanged( void *pVar )
	{
		// Forward the call to the entity that will send the data.
		CGameRulesProxy::NotifyNetworkStateChanged();
	}

	// Get the view vectors for this mod.
	virtual const CViewVectors* GetViewVectors() const;

// Damage rules for ammo types
	virtual float GetAmmoDamage( CBaseEntity *pAttacker, CBaseEntity *pVictim, int nAmmoType );
    virtual float GetDamageMultiplier( void ) { return 1.0f; }    

// Functions to verify the single/multiplayer status of a game
	virtual bool IsMultiplayer( void ) = 0;// is this a multiplayer game? (either coop or deathmatch)

	virtual const unsigned char *GetEncryptionKey() { return nullptr; }

	virtual bool InRoundRestart( void ) { return false; }

	virtual bool CheckAchievementsEnabled( int iAchievementID ) { return true; }

	virtual void RegisterScriptFunctions( void ){ };

	virtual void ClientCommandKeyValues( edict_t *pEntity, KeyValues *pKeyValues ) {} 

	// IsConnectedUserInfoChangeAllowed allows to override FCVAR_NOT_CONNECTED rule when
	// player is on team spectator or team unassigned for example
	// Default and previous engine implementation will never allow FCVAR_NOT_CONNECTED cvars
	// to be changed while connected to a game server
	virtual bool IsConnectedUserInfoChangeAllowed( CBasePlayer *pPlayer ) { return false; }

	virtual bool IsBonusChallengeTimeBased( void );
	virtual bool AllowThirdPersonCamera() { return true; }
	
	virtual const char *GetGameTypeName( void ){ return nullptr; }
	virtual int GetGameType( void ){ return 0; }

	virtual bool ForceSplitScreenPlayersOnToSameTeam() { return true; }

	virtual bool IsTopDown() { return false; }
	virtual const QAngle& GetTopDownMovementAxis() { return vec3_angle; }

	// Assume the game doesn't care
	virtual int GetMaxHumanPlayers() const { return -1; }
};

extern ConVar g_Language;

//-----------------------------------------------------------------------------
// Gets us at the game rules
//-----------------------------------------------------------------------------

extern CGameRules *g_pGameRules;

inline CGameRules* GameRules()
{
	return g_pGameRules;
}

// Each game rules class must register using this in it's .cpp file.

class CGameRulesRegister
{
public:
	typedef void (*CreateGameRulesFn)();

	CGameRulesRegister( const char *pClassName, CreateGameRulesFn fn );

	// Allocates the gamerules object associated with this class.
	void CreateGameRules();

	static CGameRulesRegister* FindByName( const char *pName );

private:
	const char *m_pClassName;
	CreateGameRulesFn m_pFn;
	CGameRulesRegister *m_pNext;	// Links it into the global list.
	
	static CGameRulesRegister *s_pHead;

};

	// The client forwards this call so the game rules manager can create the appropriate
	// game rules class.
	void InstallStringTableCallback_GameRules( const char *tableName );

class IGameSystem;

// Purpose: Pool of all per-level strings. Allocates memory for strings,
//			consolodating duplicates. The memory is freed on behalf of clients
//			at level transition. Strings are of type string_t.


//-----------------------------------------------------------------------------
// String allocation
//-----------------------------------------------------------------------------
string_t AllocPooledString( const char *pszValue );
template <typename T1> [[gnu::always_inline]] inline auto AllocPooledStringConstant( T1 pszValue ) { return AllocPooledString( pszValue ); }
string_t FindPooledString( const char *pszValue );
void RemovePooledString( const char *pszValue );

#define AssertIsValidString( s )	contract_assert( s == NULL_STRING || s == FindPooledString( STRING(s) ) );
		 
//-----------------------------------------------------------------------------
// String system accessor
//-----------------------------------------------------------------------------
IGameSystem *GameStringSystem();

//-----------------------------------------------------------------------------
class CGameString
{
public:
	CGameString() : 
		m_iszString( NULL_STRING ), m_pszString( nullptr ), m_iSerial( 0 ) 
	{

	} 

	CGameString( const char *pszString, bool bCopy = false )  : 
		m_iszString( NULL_STRING ), m_pszString( nullptr ), m_iSerial( 0 ) 
	{ 
		Set( pszString, bCopy ); 
	}

	~CGameString();

	void Set( const char *pszString, bool bCopy = false );

	// This is only for temp variables!
	// If the global string pool changes this has no way to realloc the string!
	void SetFastNoCopy( string_t iszString )
	{
		m_iszString = iszString;
		m_iSerial = gm_iSerialNumber;
	}

	bool IsSerialNumberOutOfDate( void ) const
	{
		return m_iSerial != gm_iSerialNumber;
	}

	string_t Get() const
	{
		if ( m_iszString == NULL_STRING || IsSerialNumberOutOfDate() )
		{
			if ( !m_pszString )
				return NULL_STRING;

			m_iszString = AllocPooledString( m_pszString );
			m_iSerial = gm_iSerialNumber;
		}

		return m_iszString;
	}

	operator const char *() const
	{
		return ( m_pszString ) ? m_pszString : "";
	}

	static void IncrementSerialNumber()
	{
		gm_iSerialNumber++;
	}

private:
	mutable string_t m_iszString;
	const char *m_pszString;
	mutable int m_iSerial;
	bool m_bCopy;

	static int gm_iSerialNumber;

};

#undef AssertIsValidString

class CParticleRenderIterator;
class CParticleSimulateIterator;
struct Particle;


// Particle3D

class Particle3D : public Particle
{
public:
	Vector		m_vecVelocity;
	QAngle		m_vAngles;
	float		m_flAngSpeed;		// Is same on all axis

	// NOTE: This particle already takes the full 64-bytes.  So fade over a hardcoded 2 seconds instead of the entire lifetime
	float		GetFadeFraction() const { return m_flLifeRemaining >= 2.0f ? 1 : (m_flLifeRemaining * 0.5f); }
	bool		IsDead() const { return m_flLifeRemaining >= 0 ? false : true; }

	float		m_flLifeRemaining;	// How long it lives for.
public:
	
	byte		m_uchFrontColor[3];	
	byte		m_uchSize;			
	byte		m_uchBackColor[3];	
	byte		m_pad;				// Pad to 8 bytes.
};

//
// CSimple3DEmitter
//

class CSimple3DEmitter : public CSimpleEmitter
{
public:
	explicit CSimple3DEmitter( const char *pDebugName ) : CSimpleEmitter( pDebugName ) {}
	
	static ks::intrusive_ptr<CSimple3DEmitter>	Create( const char *pDebugName );

	virtual void RenderParticles( CParticleRenderIterator *pIterator );
	virtual void SimulateParticles( CParticleSimulateIterator *pIterator );

	CParticleCollision	m_ParticleCollision;

private:
	CSimple3DEmitter( const CSimple3DEmitter & );

};

class ParticleDraw;

// Lerp between two floating point numbers.
using ::FLerp;
inline float FLerp(float minVal, float maxVal, float t)
{
	return minVal + (maxVal - minVal) * t;
}

inline Vector VecLerp(const Vector &minVal, const Vector &maxVal, float t)
{
	return minVal + (maxVal - minVal) * t;
}

// Get a random floating point number between the two specified numbers.
inline float FRand(float minVal, float maxVal)
{
	return RandomFloat( minVal, maxVal );
}

// Apply velocity and acceleration to position and acceleration to velocity.
// If you're going to keep acceleration around, you should zero it out after calling this.
inline void PhysicallySimulate(Vector &pos, Vector &velocity, const Vector &acceleration, const float fTimeDelta)
{
	pos = pos + (velocity + (acceleration*fTimeDelta*0.5f)) * fTimeDelta;
	velocity = velocity + acceleration * fTimeDelta;
}

inline Vector GetGravityVector()
{
	return Vector(0, 0, -150);
}

// Render a quad on the screen where you pass in color and size.
// Color and alpha range is 0 to 254.9
// You also get an extra texture coordinate to pass in.
inline void RenderParticle_Color255SizeSpecularTCoord3(
	ParticleDraw* pDraw,									
	const Vector &pos,
	const Vector &color,
	const float alpha,
	const float size,
	const unsigned char *specular,
	const float tCoord
	)
{
	// Don't render totally transparent particles.
	if( alpha < 0.5f )
		return;

	CMeshBuilder *pBuilder = pDraw->GetMeshBuilder();
	if( !pBuilder )
		return;

	unsigned char ubColor[4];
	ubColor[0] = (unsigned char)RoundFloatToInt( color.x );
	ubColor[1] = (unsigned char)RoundFloatToInt( color.y );
	ubColor[2] = (unsigned char)RoundFloatToInt( color.z );
	ubColor[3] = (unsigned char)RoundFloatToInt( alpha );

	// Add the 4 corner vertices.
	pBuilder->Position3f( pos.x-size, pos.y-size, pos.z );
	pBuilder->Color4ubv( ubColor );
	pBuilder->TexCoord3f( 0, pDraw->m_pSubTexture->m_tCoordMins[0], pDraw->m_pSubTexture->m_tCoordMaxs[1], tCoord );
	pBuilder->Specular3ubv( specular );
	pBuilder->AdvanceVertex();

	pBuilder->Position3f( pos.x-size, pos.y+size, pos.z );
	pBuilder->Color4ubv( ubColor );
	pBuilder->TexCoord3f( 0, pDraw->m_pSubTexture->m_tCoordMins[0], pDraw->m_pSubTexture->m_tCoordMins[1], tCoord );
	pBuilder->Specular3ubv( specular );
 	pBuilder->AdvanceVertex();

	pBuilder->Position3f( pos.x+size, pos.y+size, pos.z );
	pBuilder->Color4ubv( ubColor );
	pBuilder->TexCoord3f( 0, pDraw->m_pSubTexture->m_tCoordMaxs[0], pDraw->m_pSubTexture->m_tCoordMins[1], tCoord );
	pBuilder->Specular3ubv( specular );
 	pBuilder->AdvanceVertex();

	pBuilder->Position3f( pos.x+size, pos.y-size, pos.z );
	pBuilder->Color4ubv( ubColor );
	pBuilder->TexCoord3f( 0, pDraw->m_pSubTexture->m_tCoordMaxs[0], pDraw->m_pSubTexture->m_tCoordMaxs[1], tCoord );
	pBuilder->Specular3ubv( specular );
 	pBuilder->AdvanceVertex();
}

// Render a quad on the screen where you pass in color and size.
// Color and alpha range is 0 to 254.9
inline void RenderParticle_Color255Size(
	ParticleDraw* pDraw,									
	const Vector &pos,
	const Vector &color,
	const float alpha,
	const float size)
{
	// Don't render totally transparent particles.
	if( alpha < 0.5f )
		return;

	CMeshBuilder *pBuilder = pDraw->GetMeshBuilder();
	if( !pBuilder )
		return;

	unsigned char ubColor[4];
	ubColor[0] = (unsigned char)RoundFloatToInt( color.x );
	ubColor[1] = (unsigned char)RoundFloatToInt( color.y );
	ubColor[2] = (unsigned char)RoundFloatToInt( color.z );
	ubColor[3] = (unsigned char)RoundFloatToInt( alpha );

	// Add the 4 corner vertices.
	pBuilder->Position3f( pos.x-size, pos.y-size, pos.z );
	pBuilder->Color4ubv( ubColor );
	pBuilder->TexCoord2f( 0, pDraw->m_pSubTexture->m_tCoordMins[0], pDraw->m_pSubTexture->m_tCoordMaxs[1] );
	pBuilder->AdvanceVertex();

	pBuilder->Position3f( pos.x-size, pos.y+size, pos.z );
	pBuilder->Color4ubv( ubColor );
	pBuilder->TexCoord2f( 0, pDraw->m_pSubTexture->m_tCoordMins[0], pDraw->m_pSubTexture->m_tCoordMins[1] );
 	pBuilder->AdvanceVertex();

	pBuilder->Position3f( pos.x+size, pos.y+size, pos.z );
	pBuilder->Color4ubv( ubColor );
	pBuilder->TexCoord2f( 0, pDraw->m_pSubTexture->m_tCoordMaxs[0], pDraw->m_pSubTexture->m_tCoordMins[1] );
 	pBuilder->AdvanceVertex();

	pBuilder->Position3f( pos.x+size, pos.y-size, pos.z );
	pBuilder->Color4ubv( ubColor );
	pBuilder->TexCoord2f( 0, pDraw->m_pSubTexture->m_tCoordMaxs[0], pDraw->m_pSubTexture->m_tCoordMaxs[1] );
 	pBuilder->AdvanceVertex();
}

// Render a quad on the screen where you pass in color and size.
// Color and alpha range is 0 to 254.9
inline void RenderParticle_Color255SizeNormal(
	ParticleDraw* pDraw,									
	const Vector &pos,
	const Vector &color,
	const float alpha,
	const float size,
	const Vector &vNormal )
{
	// Don't render totally transparent particles.
	if( alpha < 0.5f )
		return;

	CMeshBuilder *pBuilder = pDraw->GetMeshBuilder();
	if( !pBuilder )
		return;

	unsigned char ubColor[4];
	ubColor[0] = (unsigned char)RoundFloatToInt( color.x );
	ubColor[1] = (unsigned char)RoundFloatToInt( color.y );
	ubColor[2] = (unsigned char)RoundFloatToInt( color.z );
	ubColor[3] = (unsigned char)RoundFloatToInt( alpha );

	// Add the 4 corner vertices.
	pBuilder->Position3f( pos.x-size, pos.y-size, pos.z );
	pBuilder->Color4ubv( ubColor );
	pBuilder->TexCoord2f( 0, pDraw->m_pSubTexture->m_tCoordMins[0], pDraw->m_pSubTexture->m_tCoordMaxs[1] );
	pBuilder->Normal3fv( (float*)&vNormal );
	pBuilder->AdvanceVertex();

	pBuilder->Position3f( pos.x-size, pos.y+size, pos.z );
	pBuilder->Color4ubv( ubColor );
	pBuilder->TexCoord2f( 0, pDraw->m_pSubTexture->m_tCoordMins[0], pDraw->m_pSubTexture->m_tCoordMins[1] );
	pBuilder->Normal3fv( (float*)&vNormal );
 	pBuilder->AdvanceVertex();

	pBuilder->Position3f( pos.x+size, pos.y+size, pos.z );
	pBuilder->Color4ubv( ubColor );
	pBuilder->TexCoord2f( 0, pDraw->m_pSubTexture->m_tCoordMaxs[0], pDraw->m_pSubTexture->m_tCoordMins[1] );
	pBuilder->Normal3fv( (float*)&vNormal );
 	pBuilder->AdvanceVertex();

	pBuilder->Position3f( pos.x+size, pos.y-size, pos.z );
	pBuilder->Color4ubv( ubColor );
	pBuilder->TexCoord2f( 0, pDraw->m_pSubTexture->m_tCoordMaxs[0], pDraw->m_pSubTexture->m_tCoordMaxs[1] );
	pBuilder->Normal3fv( (float*)&vNormal );
 	pBuilder->AdvanceVertex();
}

// Render a quad on the screen where you pass in color and size.
// Color and alpha range is 0 to 254.9
// Angle is in radians.
inline void RenderParticle_Color255SizeNormalAngle(
	ParticleDraw* pDraw,									
	const Vector &pos,
	const Vector &color,
	const float alpha,
	const float size,
	const Vector &vNormal,
	const float angle )
{
	// Don't render totally transparent particles.
	if( alpha < 0.5f )
		return;

	CMeshBuilder *pBuilder = pDraw->GetMeshBuilder();
	if( !pBuilder )
		return;

	unsigned char ubColor[4];
	ubColor[0] = (unsigned char)RoundFloatToInt( color.x );
	ubColor[1] = (unsigned char)RoundFloatToInt( color.y );
	ubColor[2] = (unsigned char)RoundFloatToInt( color.z );
	ubColor[3] = (unsigned char)RoundFloatToInt( alpha );

	float ca = (float)std::cos(angle);
	float sa = (float)std::sin(angle);

	// Add the 4 corner vertices.
	pBuilder->Position3f( pos.x + (-ca + sa) * size, pos.y + (-sa - ca) * size, pos.z );
	pBuilder->Color4ubv( ubColor );
	pBuilder->TexCoord2f( 0, pDraw->m_pSubTexture->m_tCoordMins[0], pDraw->m_pSubTexture->m_tCoordMaxs[1] );
	pBuilder->Normal3fv( (float*)&vNormal );
	pBuilder->AdvanceVertex();

	pBuilder->Position3f( pos.x + (-ca - sa) * size, pos.y + (-sa + ca) * size, pos.z );
	pBuilder->Color4ubv( ubColor );
	pBuilder->TexCoord2f( 0, pDraw->m_pSubTexture->m_tCoordMins[0], pDraw->m_pSubTexture->m_tCoordMins[1] );
	pBuilder->Normal3fv( (float*)&vNormal );
 	pBuilder->AdvanceVertex();

	pBuilder->Position3f( pos.x + (ca - sa)  * size, pos.y + (sa + ca)  * size, pos.z );
	pBuilder->Color4ubv( ubColor );
	pBuilder->TexCoord2f( 0, pDraw->m_pSubTexture->m_tCoordMaxs[0], pDraw->m_pSubTexture->m_tCoordMins[1] );
	pBuilder->Normal3fv( (float*)&vNormal );
 	pBuilder->AdvanceVertex();

	pBuilder->Position3f( pos.x + (ca + sa)  * size, pos.y + (sa - ca)  * size, pos.z );
	pBuilder->Color4ubv( ubColor );
	pBuilder->TexCoord2f( 0, pDraw->m_pSubTexture->m_tCoordMaxs[0], pDraw->m_pSubTexture->m_tCoordMaxs[1] );
	pBuilder->Normal3fv( (float*)&vNormal );
 	pBuilder->AdvanceVertex();
}

// Render a quad on the screen where you pass in color and size.
inline void RenderParticle_ColorSize(
	ParticleDraw* pDraw,									
	const Vector &pos,
	const Vector &color,
	const float alpha,
	const float size
	)
{
	// Don't render totally transparent particles.
	if( alpha < 0.001f )
		return;

	CMeshBuilder *pBuilder = pDraw->GetMeshBuilder();
	if( !pBuilder )
		return;

	unsigned char ubColor[4];
	ubColor[0] = (unsigned char)RoundFloatToInt( color.x * 254.9f );
	ubColor[1] = (unsigned char)RoundFloatToInt( color.y * 254.9f );
	ubColor[2] = (unsigned char)RoundFloatToInt( color.z * 254.9f );
	ubColor[3] = (unsigned char)RoundFloatToInt( alpha * 254.9f );

	// Add the 4 corner vertices.
	pBuilder->Position3f( pos.x-size, pos.y-size, pos.z );
	pBuilder->Color4ubv( ubColor );
	pBuilder->TexCoord2f( 0, pDraw->m_pSubTexture->m_tCoordMins[0], pDraw->m_pSubTexture->m_tCoordMaxs[1] );
 	pBuilder->AdvanceVertex();

	pBuilder->Position3f( pos.x-size, pos.y+size, pos.z );
	pBuilder->Color4ubv( ubColor );
	pBuilder->TexCoord2f( 0, pDraw->m_pSubTexture->m_tCoordMins[0], pDraw->m_pSubTexture->m_tCoordMins[1] );
 	pBuilder->AdvanceVertex();

	pBuilder->Position3f( pos.x+size, pos.y+size, pos.z );
	pBuilder->Color4ubv( ubColor );
	pBuilder->TexCoord2f( 0, pDraw->m_pSubTexture->m_tCoordMaxs[0], pDraw->m_pSubTexture->m_tCoordMins[1] );
 	pBuilder->AdvanceVertex();

	pBuilder->Position3f( pos.x+size, pos.y-size, pos.z );
	pBuilder->Color4ubv( ubColor );
	pBuilder->TexCoord2f( 0, pDraw->m_pSubTexture->m_tCoordMaxs[0], pDraw->m_pSubTexture->m_tCoordMaxs[1] );
 	pBuilder->AdvanceVertex();
}

inline void RenderParticle_ColorSizeAngle(
	ParticleDraw* pDraw,									
	const Vector &pos,
	const Vector &color,
	const float alpha,
	const float size,
	const float angle)
{
	// Don't render totally transparent particles.
	if(alpha < 0.001f)
		return;

	CMeshBuilder *pBuilder = pDraw->GetMeshBuilder();
	if( !pBuilder )
		return;

	unsigned char ubColor[4];
	ubColor[0] = (unsigned char)RoundFloatToInt( color.x * 254.9f );
	ubColor[1] = (unsigned char)RoundFloatToInt( color.y * 254.9f );
	ubColor[2] = (unsigned char)RoundFloatToInt( color.z * 254.9f );
	ubColor[3] = (unsigned char)RoundFloatToInt( alpha * 254.9f );

	float sa, ca;
	SinCos(angle, &sa, &ca );

	pBuilder->Position3f( pos.x + (-ca + sa) * size, pos.y + (-sa - ca) * size, pos.z );
	pBuilder->Color4ubv( ubColor );
	pBuilder->TexCoord2f( 0, pDraw->m_pSubTexture->m_tCoordMins[0], pDraw->m_pSubTexture->m_tCoordMaxs[1] );
 	pBuilder->AdvanceVertex();

	pBuilder->Position3f( pos.x + (-ca - sa) * size, pos.y + (-sa + ca) * size, pos.z );
	pBuilder->Color4ubv( ubColor );
	pBuilder->TexCoord2f( 0, pDraw->m_pSubTexture->m_tCoordMins[0], pDraw->m_pSubTexture->m_tCoordMins[1] );
 	pBuilder->AdvanceVertex();

	pBuilder->Position3f( pos.x + (ca - sa)  * size, pos.y + (sa + ca)  * size, pos.z );
	pBuilder->Color4ubv( ubColor );
	pBuilder->TexCoord2f( 0, pDraw->m_pSubTexture->m_tCoordMaxs[0], pDraw->m_pSubTexture->m_tCoordMins[1] );
 	pBuilder->AdvanceVertex();

	pBuilder->Position3f( pos.x + (ca + sa)  * size, pos.y + (sa - ca)  * size, pos.z );
	pBuilder->Color4ubv( ubColor );
	pBuilder->TexCoord2f( 0, pDraw->m_pSubTexture->m_tCoordMaxs[0], pDraw->m_pSubTexture->m_tCoordMaxs[1] );
 	pBuilder->AdvanceVertex();
}

inline void RenderParticle_ColorSizeAngles(
	ParticleDraw* pDraw,									
	const Vector &pos,
	const Vector &color,
	const float alpha,
	const float size,
	const QAngle &angles)
{
	// Don't render totally transparent particles.
	if(alpha < 0.001f)
		return;

	CMeshBuilder *pBuilder = pDraw->GetMeshBuilder();
	if( !pBuilder )
		return;

	unsigned char ubColor[4];
	ubColor[0] = (unsigned char)RoundFloatToInt( color.x * 254.9f );
	ubColor[1] = (unsigned char)RoundFloatToInt( color.y * 254.9f );
	ubColor[2] = (unsigned char)RoundFloatToInt( color.z * 254.9f );
	ubColor[3] = (unsigned char)RoundFloatToInt( alpha * 254.9f );

	Vector vNorm,vWidth,vHeight;
	AngleVectors(angles,&vNorm,&vWidth,&vHeight);

	Vector vVertex = pos;
	pBuilder->Position3f( vVertex.x , vVertex.y , vVertex.z );
	pBuilder->Color4ubv( ubColor );
	pBuilder->Normal3f( vNorm.x, vNorm.y, vNorm.z );
	pBuilder->TexCoord2f( 0, pDraw->m_pSubTexture->m_tCoordMins[0], pDraw->m_pSubTexture->m_tCoordMaxs[1] );
 	pBuilder->AdvanceVertex();

	vVertex = vVertex + vWidth*size;
	pBuilder->Position3f( vVertex.x, vVertex.y, vVertex.z );
	pBuilder->Color4ubv( ubColor );
	pBuilder->Normal3f( vNorm.x, vNorm.y, vNorm.z );
	pBuilder->TexCoord2f( 0, pDraw->m_pSubTexture->m_tCoordMins[0], pDraw->m_pSubTexture->m_tCoordMins[1] );
 	pBuilder->AdvanceVertex();

	vVertex = vVertex + vHeight*size;
	pBuilder->Position3f( vVertex.x, vVertex.y , vVertex.z );
	pBuilder->Color4ubv( ubColor );
	pBuilder->Normal3f( vNorm.x, vNorm.y, vNorm.z );
	pBuilder->TexCoord2f( 0, pDraw->m_pSubTexture->m_tCoordMaxs[0], pDraw->m_pSubTexture->m_tCoordMins[1] );
 	pBuilder->AdvanceVertex();

	vVertex = vVertex - vWidth*size;
	pBuilder->Position3f( vVertex.x, vVertex.y, vVertex.z );
	pBuilder->Color4ubv( ubColor );
	pBuilder->Normal3f( vNorm.x, vNorm.y, vNorm.z );
	pBuilder->TexCoord2f( 0, pDraw->m_pSubTexture->m_tCoordMaxs[0], pDraw->m_pSubTexture->m_tCoordMaxs[1] );
 	pBuilder->AdvanceVertex();
}

inline float GetAlphaDistanceFade(
	const Vector &pos,
	const float fadeNearDist,
	const float fadeFarDist)
{
	if(-pos.z > fadeFarDist)
	{
		return 1;
	}
	else if(-pos.z > fadeNearDist)
	{
		return (-pos.z - fadeNearDist) / (fadeFarDist - fadeNearDist);
	}
	else
	{
		return 0;
	}
}

inline Vector WorldGetLightForPoint(const Vector &vPos, bool bClamp)
{
		return engine->GetLightForPoint(vPos, bClamp);
}

class CParticleMgr;
class ParticleDraw;

class CParticleSphereRenderer
{
public:

				CParticleSphereRenderer();

	// Initialize and tell it the material you'll be using.
	void		Init( CParticleMgr *pParticleMgr, IMaterial *pMaterial );
	
	// Pass this call through from your particle system too.
	void		StartRender( VMatrix &effectMatrix );

	// Call this to	render a spherical particle.
	void		RenderParticle( 
		ParticleDraw *pDraw, 
		const Vector &vOriginalPos,
		const Vector &vTransformedPos,
		float flAlpha,			// value 0 - 255.4
		float flParticleSize,
		float flAngle = 0.0f ); 

	void		RenderParticle_AddColor( 
		ParticleDraw *pDraw, 
		const Vector &vOriginalPos,
		const Vector &vTransformedPos,
		float flAlpha,				// value 0 - 255.4
		float flParticleSize,
		const Vector &vToAdd0to1	// Add this to the color (value 0-1)
		); 

	// Lighting is (base color) + (ambient / dist^2) + bump(directional / dist^2)
	const Vector& GetBaseColor() const;			// Specified as 0-1
	void SetBaseColor( const Vector &vColor );
	
	const CParticleLightInfo& GetAmbientLight() const;
	void SetAmbientLight( const CParticleLightInfo &info );
	
	const CParticleLightInfo& GetDirectionalLight() const;
	void SetDirectionalLight( const CParticleLightInfo &info );

private:

	void		AddLightColor( 
		Vector const *pPos,
		Vector const *pLightPos, 
		Vector const *pLightColor, 
		float flLightIntensity,
		Vector *pOutColor );

	inline void	ClampColor( Vector &vColor );

private:

	int m_iLastTickStartRenderCalled;	// Used for debugging.
	
	CParticleMgr *m_pParticleMgr;
	
	Vector			m_vBaseColor;
	CParticleLightInfo m_AmbientLight;
	CParticleLightInfo m_DirectionalLight;
	bool			m_bUsingPixelShaders;
};

// ------------------------------------------------------------------------ //
// Inlines.
// ------------------------------------------------------------------------ //

void CParticleSphereRenderer::ClampColor( Vector &vColor )
{
	float flMax = std::max( vColor.x, std::max( vColor.y, vColor.z ) );
	if( flMax > 1 )
	{
		vColor *= 255.0f / flMax;
	}
	else
	{
		vColor *= 255.0;
	}
}

class IGameSystem;

inline constexpr auto FPS_STABILIZE_TIME = 1.5;
inline constexpr auto FPS_STABILIZE_TIME_RELOAD_MATERIALS = 10.0;
inline constexpr auto FPS_MEASURE_TIME = 2.0;

class CPerfVisualBenchmark : public CBaseGameSystemPerFrame
{
	
public:
	CPerfVisualBenchmark();
	virtual ~CPerfVisualBenchmark();

public: // CBaseGameSystem overrides

	virtual char const *Name() { return "CPerfVisualBenchmark"; }

	virtual bool Init();
	virtual void PreRender( );

	void Start();
	void Stop();

private:
	void Print();
	struct RunInfo_t
	{
		const char *m_pVarName;
		const char *m_pOnVal;
		const char *m_pOffVal;
		const char *m_pDescription;
		float m_flStabilizeTime;
		float m_flFPS;
	};

private:
	std::vector< RunInfo_t > m_RunInfo;
	bool m_bIsOn;				//is this thing on?
	int m_iCurVar;				//what convar are we at?
	float m_flTimer;			//time since we started measuring the current convar
	float m_flStartMeasureTime;
	int m_nStartFrameCount;
	bool m_bSaveMouseEnable;	// remember this so that we can reset it after the benchmark
	bool m_bWaiting;
};

extern IGameSystem* PerfVisualBenchmark();

class CBaseEntity;

using namespace ::ks::reflect;

// Client-side CBasePlayer

class CPhysBox : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_PhysBox" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CPhysBox, CBaseEntity );
	DECLARE_CLIENTCLASS();

					CPhysBox();
	virtual			~CPhysBox();
	virtual ShadowType_t ShadowCastType();
	
public:
	[[= ks::reflect::Net{} ]] float			m_mass;	// TEST..
};

// Purpose: Define local cl_dll hooks to the renderable textures in material system

ITexture *GetPowerOfTwoFrameBufferTexture( void );
ITexture *GetFullFrameFrameBufferTexture( int textureIndex );
ITexture *GetWaterReflectionTexture( void );
ITexture *GetWaterRefractionTexture( void );
ITexture *GetFullscreenTexture( void );
ITexture *GetCameraTexture( void );
ITexture *GetFullFrameDepthTexture( void );
ITexture *GetCustomWeaponTexture();
ITexture *GetCustomWeaponTextureExp();

// SmallBufferHDRx=r16g16b16a16 quarter-sized texture
ITexture *GetSmallBufferHDR0( void );
ITexture *GetSmallBufferHDR1( void );

ITexture *GetSmallBuffer0( void );							// quarter-sized texture, same fmt as screen
ITexture *GetSmallBuffer1( void );							// quarter-sized texture, same fmt as screen

inline constexpr auto MAX_TEENY_TEXTURES = 3;

ITexture *GetTeenyTexture(int which);						// tiny 32x32 texture, always 8888

void ReleaseRenderTargets( void );

class CBaseAnimating;
class CBaseEntity;

using namespace ::ks::reflect;

void RecvProxy_RecomputeSprings( const CRecvProxyData *pData, void *pStruct, void *pOut );

struct RopeSegData_t;

inline constexpr auto MAX_ROPE_SUBDIVS = 8;
inline constexpr auto MAX_ROPE_SEGMENTS = (ROPE_MAX_SEGMENTS+(ROPE_MAX_SEGMENTS-1)*MAX_ROPE_SUBDIVS);

enum rope_type
{
	ROPE_TYPE_DEFAULT = 0,
	ROPE_TYPE_DEFUSECABLES,
};

//=============================================================================
class CRopeKeyframe : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_RopeKeyframe", .base = false } ]]
	[[= ks::reflect::From<"m_nChangeCount", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_iRopeMaterialModelIndex", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_hStartPoint", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_hEndPoint", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_iStartAttachment", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_iEndAttachment", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_fLockedPoints", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_Slack", ks::reflect::Net{}, RecvProxy_RecomputeSprings>{} ]]
	[[= ks::reflect::From<"m_RopeLength", ks::reflect::Net{}, RecvProxy_RecomputeSprings>{} ]]
	[[= ks::reflect::From<"m_RopeFlags", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_TextureScale", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_nSegments", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_bConstrainBetweenEndpoints", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_Subdiv", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_Width", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_flScrollSpeed", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_vecNetworkOrigin", ks::reflect::Net{ .wire = "m_vecOrigin" }>{} ]]
	[[= ks::reflect::From<"m_hNetworkMoveParent", ks::reflect::Net{ .wire = "moveparent" }, RecvProxy_IntToMoveParent>{} ]]
	[[= ks::reflect::From<"m_iParentAttachment", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_iDefaultRopeMaterialModelIndex", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_nMinCPULevel", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_nMaxCPULevel", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_nMinGPULevel", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_nMaxGPULevel", ks::reflect::Net{}>{} ]]
	static constexpr bool ks_class = true;

public:

	DECLARE_CLASS( CRopeKeyframe, CBaseEntity );
	DECLARE_CLIENTCLASS();

private:

	class CPhysicsDelegate : public CSimplePhysics::IHelper
	{
	public:
		virtual void	GetNodeForces( CSimplePhysics::CNode *pNodes, int iNode, Vector *pAccel );
		virtual void	ApplyConstraints( CSimplePhysics::CNode *pNodes, int nNodes );
	
		CRopeKeyframe	*m_pKeyframe;
	};

	friend class CPhysicsDelegate;

public:

					CRopeKeyframe();
					~CRopeKeyframe();

	// This can be used for client-only ropes.
	static CRopeKeyframe* Create(
		CBaseEntity *pStartEnt,
		CBaseEntity *pEndEnt,
		int iStartAttachment=0,
		int iEndAttachment=0,
		float ropeWidth = 2,
		const char *pMaterialName = "cable/cable",		// Note: whoever creates the rope must
														// use PrecacheModel for whatever material
														// it specifies here.
		int numSegments = 5,
		int ropeFlags = ROPE_SIMULATE,
		rope_type ropeType = ROPE_TYPE_DEFAULT
		);

	// Create a client-only rope and initialize it with the parameters from the KeyValues.
	static CRopeKeyframe* CreateFromKeyValues( CBaseAnimating *pEnt, KeyValues *pValues );

	// Find ropes (with both endpoints connected) that intersect this AABB. This is just an approximation.
	static int GetRopesIntersectingAABB( CRopeKeyframe **pRopes, int nMaxRopes, const Vector &vAbsMin, const Vector &vAbsMax );

	// Set the slack.
	void SetSlack( int slack );

	void SetRopeFlags( int flags );
	void AddRopeFlags( int flags );
	int GetRopeFlags() const;

	void SetupHangDistance( float flHangDist );

	// Change which entities the rope is connected to.
	void SetStartEntity( CBaseEntity *pEnt );
	void SetEndEntity( CBaseEntity *pEnt );

	CBaseEntity* GetStartEntity() const;
	CBaseEntity* GetEndEntity() const;

	// Hook the physics. Pass in your own implementation of CSimplePhysics::IHelper. The
	// default implementation is returned so you can call through to it if you want.
	CSimplePhysics::IHelper*	HookPhysics( CSimplePhysics::IHelper *pHook );

	// Attach to things (you can also just lock the endpoints down yourself if you hook the physics).

	// Client-only right now. This could be moved to the server if there was a good reason.
	void			SetColorMod( const Vector &vColorMod );

	// Use this when rope length and slack change to recompute the spring length.
	void			RecomputeSprings();

	void ShakeRope( const Vector &vCenter, float flRadius, float flMagnitude );

	// Get the attachment position of one of the endpoints.
	bool			GetEndPointPos( int iPt, Vector &vPos );

	// Get the rope material data.
	IMaterial		*GetSolidMaterial( void )		{ return m_pMaterial; }

	struct BuildRopeQueuedData_t
	{
		Vector	*m_pPredictedPositions;
		Vector	*m_pLightValues;
		int		m_iNodeCount;
		Vector	m_vColorMod;
		float	m_RopeLength;
		float	m_Slack;
	};

	void			BuildRope( RopeSegData_t *pRopeSegment, const Vector &vCurrentViewForward, const Vector &vCurrentViewOrigin, BuildRopeQueuedData_t *pQueuedData );

// CBaseEntity overrides.
public:

	virtual void	OnDataChanged( DataUpdateType_t updateType );
	virtual void	ClientThink();
	virtual int		DrawModel( int flags, const RenderableInstance_t &instance );
	virtual RenderableTranslucencyType_t ComputeTranslucencyType() { return RENDERABLE_IS_OPAQUE; }

	virtual bool	ShouldDraw();
	virtual const Vector& WorldSpaceCenter() const;

	// Specify ROPE_ATTACHMENT_START_POINT or ROPE_ATTACHMENT_END_POINT for the attachment.
	virtual	bool	GetAttachment( int number, Vector &origin, QAngle &angles );
	virtual bool	GetAttachment( int number, matrix3x4_t &matrix );
	virtual bool	GetAttachment( int number, Vector &origin );
	virtual bool	GetAttachmentVelocity( int number, Vector &originVel, Quaternion &angleVel );

private:
	
	void			FinishInit( const char *pMaterialName );

	void			RunRopeSimulation( float flSeconds );
	Vector			ConstrainNode( const Vector &vNormal, const Vector &vNodePosition, const Vector &vMidpiont, float fNormalLength );
	void			ConstrainNodesBetweenEndpoints( void );

	bool			AnyPointsMoved();

	bool			DidEndPointMove( int iPt );
	bool			DetectRestingState( bool &bApplyWind );

	void			UpdateBBox();
	bool			InitRopePhysics();
	
	bool			GetEndPointAttachment( int iPt, Vector &vPos, QAngle &angle );
	
	Vector			*GetRopeSubdivVectors( int *nSubdivs );
	void			CalcLightValues();

	void			ReceiveMessage( int classID, bf_read &msg );
	bool			CalculateEndPointAttachment( CBaseEntity *pEnt, int iAttachment, Vector &vPos, QAngle *pAngles );

	void			UpdateHolidayLights( void );

private:
	// Track which links touched something last frame. Used to prevent wind from gusting on them.
	std::bitset<ROPE_MAX_SEGMENTS>		m_LinksTouchingSomething;
	int								m_nLinksTouchingSomething;
	bool							m_bApplyWind;
	int								m_fPrevLockedPoints;	// Which points are locked down.
	int								m_iForcePointMoveCounter;

	// Used to control resting state.
	bool			m_bPrevEndPointPos[2];
	Vector			m_vPrevEndPointPos[2];

	float			m_flCurScroll;		// for scrolling texture.
	float			m_flScrollSpeed;

	int				m_RopeFlags;			// Combo of ROPE_ flags.
	int				m_iRopeMaterialModelIndex;	// Index of sprite model with the rope's material.
	int				m_iDefaultRopeMaterialModelIndex;	// Index of the default sprite model with the rope's material.

	CRopePhysics<ROPE_MAX_SEGMENTS>	m_RopePhysics;
	Vector			m_LightValues[ROPE_MAX_SEGMENTS]; // light info when the rope is created.

	int				m_nSegments;		// Number of segments.
	
	EHANDLE			m_hStartPoint;		// StartPoint/EndPoint are entities
	EHANDLE			m_hEndPoint;
	short			m_iStartAttachment;	// StartAttachment/EndAttachment are attachment points.
	short			m_iEndAttachment;

	int				m_Subdiv;			// Number of subdivions in between segments.

	int				m_RopeLength;		// Length of the rope, used for tension.
	int				m_Slack;			// Extra length the rope is given.
	float			m_TextureScale;		// pixels per inch
	
	int				m_fLockedPoints;	// Which points are locked down.
	int				m_nChangeCount;

	float				m_Width;

	CPhysicsDelegate	m_PhysicsDelegate;

	IMaterial		*m_pMaterial;

	int				m_TextureHeight;	// Texture height, for texture scale calculations.

	// Instantaneous force
	Vector			m_vecImpulse;
	Vector			m_vecPreviousImpulse;

	// Simulated wind gusts.
	float			m_flCurrentGustTimer;
	float			m_flCurrentGustLifetime;	// How long will the current gust last?

	float			m_flTimeToNextGust;			// When will the next wind gust be?
	Vector			m_vWindDir;					// What direction does the current gust go in?

	Vector			m_vColorMod;				// Color modulation on all verts?

	Vector			m_vCachedEndPointAttachmentPos[2];
	QAngle			m_vCachedEndPointAttachmentAngle[2];

	// In network table, can't bit-compress
	bool			m_bConstrainBetweenEndpoints;	// Simulated segment points won't stretch beyond the endpoints

	bool			m_bEndPointAttachmentPositionsDirty : 1;
	bool			m_bEndPointAttachmentAnglesDirty : 1;
	bool			m_bNewDataThisFrame : 1;			// Set to true in OnDataChanged so that we simulate that frame
	bool			m_bPhysicsInitted : 1;				// It waits until all required entities are 
	// present to start simulating and rendering.

	rope_type		m_ropeType;

	friend class CRopeManager;
};

// Profiling info.
void Rope_ResetCounters();
//void Rope_ShowRSpeeds();

//=============================================================================
//
// Rope Manager
//
class IRopeManager
{
public:
	virtual						~IRopeManager() {}
	virtual void				ResetRenderCache( void ) = 0;
	virtual void				AddToRenderCache( CRopeKeyframe *pRope ) = 0;
	virtual void				DrawRenderCache( IMatRenderContext *pRenderContext, bool bShadowDepth ) = 0;
	virtual void				SetHolidayLightMode( bool bHoliday ) = 0;
	virtual bool				IsHolidayLightMode( void ) = 0;
	virtual int					GetHolidayLightStyle( void ) = 0;
};

IRopeManager *RopeManager();

// Purpose: Rumble effects mixer for XBox

extern void RumbleEffect( int userID, unsigned char effectIndex, unsigned char rumbleData, unsigned char rumbleFlags );
extern void UpdateRumbleEffects( int userID );
extern void UpdateScreenShakeRumble( int userID, float shake, float balance = 0 );
extern void EnableRumbleOutput( int userID, bool bEnable );
extern void StopAllRumbleEffects( int userID );

struct FindClosestPassableSpace_TraceAdapter_t;

class CBaseEntity;
class CBasePlayer;

using namespace ::ks::reflect;


// Purpose: Shared util code between client and server.

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------

extern ConVar developer;	// developer mode

// Clear all recipients of a send proxy and set only the specified client (and its split screen
// players), or set all recipients save for it. Server only. Note: these are CLIENT indices.
void SendProxyRecipients_SetOnly( CSendProxyRecipients *pRecipients, int iClient );
void SendProxyRecipients_ExcludeOnly( CSendProxyRecipients *pRecipients, int iClient );

// Returns true if the trace hit the world entity.
// If this returns true, then you can't use GetHitBoxIndex().
bool TraceDidHitWorld( const CGameTrace &tr );

// Returns true if the trace hit something and it wasn't the world.
bool TraceDidHitNonWorldEntity( const CGameTrace &tr );

// Gets the entity's network index if the trace has hit an entity.
// If not, returns -1.
int TraceGetEntityIndex( const CGameTrace &tr );

//-----------------------------------------------------------------------------
// Language IDs.
//-----------------------------------------------------------------------------
inline constexpr auto LANGUAGE_ENGLISH = 0;
inline constexpr auto LANGUAGE_GERMAN = 1;
inline constexpr auto LANGUAGE_FRENCH = 2;
inline constexpr auto LANGUAGE_BRITISH = 3;

//-----------------------------------------------------------------------------
// Pitch + yaw
//-----------------------------------------------------------------------------
float		UTIL_VecToYaw			(const Vector &vec);
float		UTIL_VecToPitch			(const Vector &vec);
float		UTIL_VecToYaw			(const matrix3x4_t& matrix, const Vector &vec);
float		UTIL_VecToPitch			(const matrix3x4_t& matrix, const Vector &vec);
Vector		UTIL_YawToVector		( float yaw );

//-----------------------------------------------------------------------------
// Shared random number generators for shared/predicted code:
// whenever generating random numbers in shared/predicted code, these functions
// have to be used. Each call should specify a unique "sharedname" string that
// seeds the random number generator. In loops make sure the "additionalSeed"
// is increased with the loop counter, otherwise it will always return the
// same random number
//-----------------------------------------------------------------------------
float	SharedRandomFloat( const char *sharedname, float flMinVal, float flMaxVal, int additionalSeed = 0 );
int		SharedRandomInt( const char *sharedname, int iMinVal, int iMaxVal, int additionalSeed = 0 );
Vector	SharedRandomVector( const char *sharedname, float minVal, float maxVal, int additionalSeed = 0 );
QAngle	SharedRandomAngle( const char *sharedname, float minVal, float maxVal, int additionalSeed = 0 );

//-----------------------------------------------------------------------------
// Standard collision filters...
//-----------------------------------------------------------------------------
bool PassServerEntityFilter( const IHandleEntity *pTouch, const IHandleEntity *pPass );
bool StandardFilterRules( IHandleEntity *pHandleEntity, int fContentsMask );

// "weapon_"
inline constexpr auto WEAPON_CLASSNAME_PREFIX_LENGTH = 7;
bool IsWeaponClassname( const char *pszClassName );

//-----------------------------------------------------------------------------
// Converts an IHandleEntity to an CBaseEntity
//-----------------------------------------------------------------------------
const CBaseEntity *EntityFromEntityHandle( const IHandleEntity *pConstHandleEntity );
CBaseEntity *EntityFromEntityHandle( IHandleEntity *pHandleEntity );

typedef bool (*ShouldHitFunc_t)( IHandleEntity *pHandleEntity, int contentsMask );

//-----------------------------------------------------------------------------
// traceline methods
//-----------------------------------------------------------------------------
class CTraceFilterSimple : public CTraceFilter
{
public:
	// It does have a base, but we'll never network anything below here..
	DECLARE_CLASS_NOBASE( CTraceFilterSimple );
	
	CTraceFilterSimple( const IHandleEntity *passentity, int collisionGroup, ShouldHitFunc_t pExtraShouldHitCheckFn = nullptr );
	virtual bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask );
	virtual void SetPassEntity( const IHandleEntity *pPassEntity ) { m_pPassEnt = pPassEntity; }
	virtual void SetCollisionGroup( int iCollisionGroup ) { m_collisionGroup = iCollisionGroup; }

	const IHandleEntity *GetPassEntity( void ){ return m_pPassEnt;}
	int GetCollisionGroup( void ) const { return m_collisionGroup; }

private:
	const IHandleEntity *m_pPassEnt;
	int m_collisionGroup;
	ShouldHitFunc_t m_pExtraShouldHitCheckFunction;

};

class CTraceFilterSkipTwoEntities : public CTraceFilterSimple
{
public:
	// It does have a base, but we'll never network anything below here..
	DECLARE_CLASS( CTraceFilterSkipTwoEntities, CTraceFilterSimple );
	
	CTraceFilterSkipTwoEntities( const IHandleEntity *passentity = nullptr, const IHandleEntity *passentity2 = nullptr, int collisionGroup = COLLISION_GROUP_NONE );
	virtual bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask );
	virtual void SetPassEntity2( const IHandleEntity *pPassEntity2 ) { m_pPassEnt2 = pPassEntity2; }

private:
	const IHandleEntity *m_pPassEnt2;
};

class CTraceFilterSimpleList : public CTraceFilterSimple
{
public:
	CTraceFilterSimpleList( int collisionGroup );
	virtual bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask );

	void	AddEntityToIgnore( IHandleEntity *pEntity );
	void	AddEntitiesToIgnore( int nCount, IHandleEntity **ppEntities );

protected:
	std::vector< IHandleEntity* >	m_PassEntities;
};

class CTraceFilterNoNPCsOrPlayer : public CTraceFilterSimple
{
public:
	CTraceFilterNoNPCsOrPlayer( const IHandleEntity *passentity = nullptr, int collisionGroup = COLLISION_GROUP_NONE )
		: CTraceFilterSimple( passentity, collisionGroup )
	{
	}

	virtual bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask );
};

class CTraceFilterNoPlayers : public CTraceFilterSimple
{
public:
	CTraceFilterNoPlayers( const IHandleEntity *passentity = nullptr, int collisionGroup = COLLISION_GROUP_NONE )
		: CTraceFilterSimple( passentity, collisionGroup )
	{
	}

	virtual bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask );
};

// helper
void DebugDrawLine( const Vector& vecAbsStart, const Vector& vecAbsEnd, int r, int g, int b, bool test, float duration );

extern ConVar r_visualizetraces;

[[gnu::always_inline]] inline constexpr auto DidTraceSpike() { return false; }

inline void UTIL_TraceLine( const Vector& vecAbsStart, const Vector& vecAbsEnd, unsigned int mask, 
					 const IHandleEntity *ignore, int collisionGroup, trace_t *ptr )
{
	Ray_t ray;
	ray.Init( vecAbsStart, vecAbsEnd );
	CTraceFilterSimple traceFilter( ignore, collisionGroup );

	enginetrace->TraceRay( ray, mask, &traceFilter, ptr );

	if( r_visualizetraces.GetBool() || DidTraceSpike() )
	{
		DebugDrawLine( ptr->startpos, ptr->endpos, 255, 0, 0, true, ( r_visualizetraces.GetBool() ) ? -1.0f : .5 );
		if ( DidTraceSpike() ) // Opimizer will remove this block
		{
			Ray_t ray;
			ray.Init( vecAbsStart, vecAbsEnd );
			CTraceFilterSimple traceFilter( ignore, collisionGroup );

			enginetrace->TraceRay( ray, mask, &traceFilter, ptr );
			if ( DidTraceSpike() )
			{
			}
		}
	}
}

inline void UTIL_TraceLine( const Vector& vecAbsStart, const Vector& vecAbsEnd, unsigned int mask, 
					 ITraceFilter *pFilter, trace_t *ptr )
{
	Ray_t ray;
	ray.Init( vecAbsStart, vecAbsEnd );

	enginetrace->TraceRay( ray, mask, pFilter, ptr );

	if( r_visualizetraces.GetBool() || DidTraceSpike() )
	{
		DebugDrawLine( ptr->startpos, ptr->endpos, 255, 0, 0, true, ( r_visualizetraces.GetBool() ) ? -1.0f : .5 );
		if ( DidTraceSpike() ) // Opimizer will remove this block
		{
			Ray_t ray;
			ray.Init( vecAbsStart, vecAbsEnd );

			enginetrace->TraceRay( ray, mask, pFilter, ptr );

			if ( DidTraceSpike() )
			{
			}
		}
	}
}

inline void UTIL_TraceHull( const Vector &vecAbsStart, const Vector &vecAbsEnd, const Vector &hullMin, 
					 const Vector &hullMax,	unsigned int mask, const IHandleEntity *ignore, 
					 int collisionGroup, trace_t *ptr )
{
	Ray_t ray;
	ray.Init( vecAbsStart, vecAbsEnd, hullMin, hullMax );
	CTraceFilterSimple traceFilter( ignore, collisionGroup );

	enginetrace->TraceRay( ray, mask, &traceFilter, ptr );

	if( r_visualizetraces.GetBool() || DidTraceSpike() )
	{
		DebugDrawLine( ptr->startpos, ptr->endpos, 255, 255, 0, true, ( r_visualizetraces.GetBool() ) ? -1.0f : .5 );
		if ( DidTraceSpike() ) // Opimizer will remove this block
		{
			Ray_t ray;
			ray.Init( vecAbsStart, vecAbsEnd, hullMin, hullMax );
			CTraceFilterSimple traceFilter( ignore, collisionGroup );

			enginetrace->TraceRay( ray, mask, &traceFilter, ptr );

			if ( DidTraceSpike() )
			{
			}
		}
	}
}

inline void UTIL_TraceHull( const Vector &vecAbsStart, const Vector &vecAbsEnd, const Vector &hullMin, 
					 const Vector &hullMax,	unsigned int mask, ITraceFilter *pFilter, trace_t *ptr )
{
	Ray_t ray;
	ray.Init( vecAbsStart, vecAbsEnd, hullMin, hullMax );

	enginetrace->TraceRay( ray, mask, pFilter, ptr );

	if( r_visualizetraces.GetBool() || DidTraceSpike() )
	{
		DebugDrawLine( ptr->startpos, ptr->endpos, 255, 255, 0, true, ( r_visualizetraces.GetBool() ) ? -1.0f : .5 );
		if ( DidTraceSpike() ) // Opimizer will remove this block
		{
			Ray_t ray;
			ray.Init( vecAbsStart, vecAbsEnd, hullMin, hullMax );

			enginetrace->TraceRay( ray, mask, pFilter, ptr );

			if( DidTraceSpike() )
			{
			}
		}
	}
}

inline void UTIL_TraceRay( const Ray_t &ray, unsigned int mask, 
						  const IHandleEntity *ignore, int collisionGroup, trace_t *ptr )
{
	CTraceFilterSimple traceFilter( ignore, collisionGroup );

	enginetrace->TraceRay( ray, mask, &traceFilter, ptr );
	
	if( r_visualizetraces.GetBool() )
	{
		DebugDrawLine( ptr->startpos, ptr->endpos, 255, 0, 0, true, -1.0f );
	}
}

inline void UTIL_TraceRay( const Ray_t &ray, unsigned int mask, 
						  ITraceFilter *pFilter, trace_t *ptr )
{
	enginetrace->TraceRay( ray, mask, pFilter, ptr );

	if( r_visualizetraces.GetBool() )
	{
		DebugDrawLine( ptr->startpos, ptr->endpos, 255, 0, 0, true, -1.0f );
	}
}
// Sweeps a particular entity through the world
void UTIL_TraceEntity( CBaseEntity *pEntity, const Vector &vecAbsStart, const Vector &vecAbsEnd, unsigned int mask, trace_t *ptr );
void UTIL_TraceEntity( CBaseEntity *pEntity, const Vector &vecAbsStart, const Vector &vecAbsEnd, 
					  unsigned int mask, ITraceFilter *pFilter, trace_t *ptr );
void UTIL_TraceEntity( CBaseEntity *pEntity, const Vector &vecAbsStart, const Vector &vecAbsEnd, 
					  unsigned int mask, const IHandleEntity *ignore, int collisionGroup, trace_t *ptr );

bool UTIL_EntityHasMatchingRootParent( CBaseEntity *pRootParent, CBaseEntity *pEntity );

inline int UTIL_PointContents( const Vector &vec, int contentsMask )
{
	return enginetrace->GetPointContents( vec, contentsMask );
}

// Sweeps against a particular model, using collision rules 
void UTIL_TraceModel( const Vector &vecStart, const Vector &vecEnd, const Vector &hullMin, 
					  const Vector &hullMax, CBaseEntity *pentModel, int collisionGroup, trace_t *ptr );

void UTIL_ClipTraceToPlayers( const Vector& vecAbsStart, const Vector& vecAbsEnd, unsigned int mask, ITraceFilter *filter, trace_t *tr );

// Particle effect tracer
void		UTIL_ParticleTracer( const char *pszTracerEffectName, const Vector &vecStart, const Vector &vecEnd, int iEntIndex = 0, int iAttachment = 0, bool bWhiz = false );

// Old style, non-particle system, tracers
void		UTIL_Tracer( const Vector &vecStart, const Vector &vecEnd, int iEntIndex = 0, int iAttachment = TRACER_DONT_USE_ATTACHMENT, float flVelocity = 0, bool bWhiz = false, const char *pCustomTracerName = nullptr, int iParticleID = 0 );

bool		UTIL_IsLowViolence( void );
bool		UTIL_ShouldShowBlood( int bloodColor );
void		UTIL_BloodDrips( const Vector &origin, const Vector &direction, int color, int amount );

void		UTIL_BloodImpact( const Vector &pos, const Vector &dir, int color, int amount );
void		UTIL_BloodDecalTrace( trace_t *pTrace, int bloodColor );
void		UTIL_DecalTrace( trace_t *pTrace, char const *decalName );
bool		UTIL_IsSpaceEmpty( CBaseEntity *pMainEnt, const Vector &vMin, const Vector &vMax );
bool		UTIL_IsSpaceEmpty( CBaseEntity *pMainEnt, const Vector &vMin, const Vector &vMax, unsigned int mask, ITraceFilter *pFilter );

// Search for water transition along a vertical line
float UTIL_WaterLevel( const Vector &position, float minz, float maxz );

// Like UTIL_WaterLevel, but *way* less expensive.
// I didn't replace UTIL_WaterLevel everywhere to avoid breaking anything.

void		UTIL_StringToVector( float *pVector, const char *pString );
void		UTIL_StringToFloatArray( float *pVector, int count, const char *pString );

CBasePlayer *UTIL_PlayerByIndex( int entindex );

// decodes/encodes a buffer using a 64bit ICE key (inplace)
void		UTIL_DecodeICE( unsigned char * buffer, int size, const unsigned char *key );
void		UTIL_EncodeICE( unsigned char * buffer, unsigned int size, const unsigned char *key );
unsigned short UTIL_GetAchievementEventMask( void );	

//assumes the object is already in a mostly passable space
inline constexpr auto FL_AXIS_DIRECTION_NONE = ( 0 );
inline constexpr auto FL_AXIS_DIRECTION_X = ( 1 << 0 );
inline constexpr auto FL_AXIS_DIRECTION_NX = ( 1 << 1 );
inline constexpr auto FL_AXIS_DIRECTION_Y = ( 1 << 2 );
inline constexpr auto FL_AXIS_DIRECTION_NY = ( 1 << 3 );
inline constexpr auto FL_AXIS_DIRECTION_Z = ( 1 << 4 );
inline constexpr auto FL_AXIS_DIRECTION_NZ = ( 1 << 5 );

typedef void (*FN_RayTraceAdapterFunc)( const Ray_t &ray, trace_t *pResult, FindClosestPassableSpace_TraceAdapter_t *pTraceAdapter );
typedef bool (*FN_PointIsOutsideWorld)( const Vector &vTest, FindClosestPassableSpace_TraceAdapter_t *pTraceAdapter );

//derive from this to tack on additional data to your adapted functions
struct FindClosestPassableSpace_TraceAdapter_t
{
	FN_RayTraceAdapterFunc pTraceFunc;
	FN_PointIsOutsideWorld pPointOutsideWorldFunc;

	ITraceFilter *pTraceFilter;
	unsigned int fMask;
};

bool		UTIL_FindClosestPassableSpace( const Vector &vCenter, const Vector &vExtents, const Vector &vIndecisivePush, unsigned int iIterations, Vector &vCenterOut, int nAxisRestrictionFlags, FindClosestPassableSpace_TraceAdapter_t *pTraceAdapter );
bool		UTIL_FindClosestPassableSpace( const Vector &vCenter, const Vector &vExtents, const Vector &vIndecisivePush, ITraceFilter *pTraceFilter, unsigned int fMask, unsigned int iIterations, Vector &vCenterOut, int nAxisRestrictionFlags = FL_AXIS_DIRECTION_NONE );
bool		UTIL_FindClosestPassableSpace( CBaseEntity *pEntity, const Vector &vIndecisivePush, unsigned int fMask, unsigned int iIterations, Vector &vOriginOut, Vector *pStartingPosition = nullptr, int nAxisRestrictionFlags = FL_AXIS_DIRECTION_NONE );
bool		UTIL_FindClosestPassableSpace( CBaseEntity *pEntity, const Vector &vIndecisivePush, unsigned int fMask, Vector *pStartingPosition = nullptr, int nAxisRestrictionFlags = FL_AXIS_DIRECTION_NONE );

//--------------------------------------------------------------------------------------------------------------
/**
 * Given a position and a ray, return the shortest distance between the two.
 * If 'pos' is beyond either end of the ray, the returned distance is negated.
 */
inline float DistanceToRay( const Vector &pos, const Vector &rayStart, const Vector &rayEnd, float *along = nullptr, Vector *pointOnRay = nullptr )
{
	Vector to = pos - rayStart;
	Vector dir = rayEnd - rayStart;
	float length = dir.NormalizeInPlace();

	float rangeAlong = DotProduct( dir, to );
	if (along)
	{
		*along = rangeAlong;
	}

	float range;

	if (rangeAlong < 0.0f)
	{
		// off start point
		range = -(pos - rayStart).Length();

		if (pointOnRay)
		{
			*pointOnRay = rayStart;
		}
	}
	else if (rangeAlong > length)
	{
		// off end point
		range = -(pos - rayEnd).Length();

		if (pointOnRay)
		{
			*pointOnRay = rayEnd;
		}
	}
	else // within ray bounds
	{
		Vector onRay = rayStart + rangeAlong * dir;
		range = (pos - onRay).Length();

		if (pointOnRay)
		{
			*pointOnRay = onRay;
		}
	}

	return range;
}

//--------------------------------------------------------------------------------------------------------------
/**
* Macro for creating an interface that when inherited from automatically maintains a list of instances
* that inherit from that interface.
*/

// interface for entities that want to a auto maintained global list
#define DECLARE_AUTO_LIST( interfaceName ) \
	class interfaceName; \
	class interfaceName \
	{ \
	public: \
		interfaceName( bool bAutoAdd = true ); \
		virtual ~interfaceName(); \
		virtual CBaseEntity* GetEntity( void ) = 0; \
		static void Add( interfaceName *pElement ) { m_##interfaceName##AutoList.AddToTail( pElement ); } \
		static void Remove( interfaceName *pElement ) { m_##interfaceName##AutoList.FindAndFastRemove( pElement ); } \
		static const std::vector< interfaceName* >& AutoList( void ) { return m_##interfaceName##AutoList; } \
	private: \
	static std::vector< interfaceName* > m_##interfaceName##AutoList; \
	};

// Creates a simple function for accessing the higher level entity
#define IMPLEMENT_AUTO_LIST_GET() \
	virtual CBaseEntity* GetEntity( void ) { return this; }

// Creates the auto add/remove constructor/destructor...
// Pass false to the constructor to not auto add
#define IMPLEMENT_AUTO_LIST( interfaceName ) \
	std::vector< class interfaceName* > interfaceName::m_##interfaceName##AutoList; \
	interfaceName::interfaceName( bool bAutoAdd ) \
	{ \
		if ( bAutoAdd ) \
		{ \
			Add( this ); \
		} \
	} \
	interfaceName::~interfaceName() \
	{ \
		Remove( this ); \
	}

//--------------------------------------------------------------------------------------------------------------
/**
 * Simple class for tracking intervals of game time.
 * Upon creation, the timer is invalidated.  To measure time intervals, start the timer via Start().
 */
class IntervalTimer
{
	[[= ks::reflect::NetTable{ .name = "DT_IntervalTimer", .base = false } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_PREDICTABLE();
	DECLARE_CLASS_NOBASE( IntervalTimer );
	DECLARE_EMBEDDED_NETWORKVAR();

	IntervalTimer( void ) : m_timestamp( -1.0f )
	{
	}

	void Reset( void )
	{
		m_timestamp = Now();
	}		

	void Start( void )
	{
		m_timestamp = Now();
	}

	void StartFromTime( float startTime )
	{
		m_timestamp = startTime;
	}

	void Invalidate( void )
	{
		m_timestamp = -1.0f;
	}		

	bool HasStarted( void ) const
	{
		return (m_timestamp > 0.0f);
	}

	/// if not started, elapsed time is very large
	float GetElapsedTime( void ) const
	{
		return (HasStarted()) ? (Now() - m_timestamp) : 99999.9f;
	}

	bool IsLessThen( float duration ) const
	{
		return (Now() - m_timestamp < duration) ? true : false;
	}

	bool IsGreaterThen( float duration ) const
	{
		return (Now() - m_timestamp > duration) ? true : false;
	}

	float GetStartTime( void ) const
	{
		return m_timestamp;
	}

protected:
	CNetworkVar( float, m_timestamp, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );
	float Now( void ) const;		// work-around since client header doesn't like inlined gpGlobals->curtime
};

EXTERN_RECV_TABLE(DT_IntervalTimer);

//--------------------------------------------------------------------------------------------------------------
/**
 * Simple class for counting down a short interval of time.
 * Upon creation, the timer is invalidated.  Invalidated countdown timers are considered to have elapsed.
 */
class CountdownTimer
{
	[[= ks::reflect::NetTable{ .name = "DT_CountdownTimer", .base = false } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_PREDICTABLE();
	DECLARE_CLASS_NOBASE( CountdownTimer );
	DECLARE_EMBEDDED_NETWORKVAR();

	CountdownTimer( void ) : 
		m_timestamp( -1.0f), m_duration( 0.0f )
	{
	}

	void Reset( void )
	{
		m_timestamp = Now() + m_duration;
	}		

	void Start( float duration );

	void StartFromTime( float startTime, float duration );

	void Invalidate( void )
	{
		m_timestamp = -1.0f;
	}		

	bool HasStarted( void ) const
	{
		return (m_timestamp > 0.0f);
	}

	bool IsElapsed( void ) const
	{
		return (Now() > m_timestamp);
	}

	float GetElapsedTime( void ) const
	{
		return Now() - m_timestamp + m_duration;
	}

	float GetRemainingTime( void ) const
	{
		return (m_timestamp - Now());
	}

	float GetTargetTime() const
	{
		return m_timestamp;
	}

	/// return original countdown time
	float GetCountdownDuration( void ) const
	{
		return (m_timestamp > 0.0f) ? m_duration : 0.0f;
	}
	
	/// 1.0 for newly started, 0.0 for elapsed
	float GetRemainingRatio( void ) const
	{
		if ( HasStarted() )
		{
			float left = GetRemainingTime() / m_duration;
			if ( left < 0.0f )
				return 0.0f;
			if ( left > 1.0f )
				return 1.0f;
			return left;
		}
		
		return 0.0f;
	}

	float GetElapsedRatio() const;

	// Usage:
	//    Declaration: CountdownTimer mTimer;
	//    Think function:
	//        while(mTimer.RunEvery( timerInterval ))
	//        {
	//			  do fixed-rate stuff
	//        }
	//
	//        nextThinkTime = min(nextThinkTime, mTimer.GetTargetTime());
	//
	// This avoids 'losing' ticks on a repeating timer when
	// the think rate is not a multiple of the timer duration,
	// especially since SetNextThink rounds ticks up/down, causing
	// even a timer that is running exactly at the think rate of
	// the underlying class to not elapse correctly.
	// 
	// It also makes sure that ticks are never lost
	bool RunEvery( float amount = -1.0f )
	{
		// First call starts the timer
		if(!HasStarted())
		{
			if(amount > 0.0f)
				Start( amount );

			return false;
		}

		if( IsElapsed() )
		{
			if ( amount > 0.0f )
				m_duration = amount;

			m_timestamp += m_duration;
			return true;
		}

		return false;
	}

	// Same as RunEvery() but only returns true once per 'tick', then guarantees being non-elapsed.
	// Useful when "do fixed rate stuff" is idempotent, like updating something to match
	// the current time.
	bool Interval( float amount = -1.0f )
	{
		// First call starts the timer
		if ( !HasStarted() )
		{
			if ( amount > 0.0f )
				Start( amount );

			return false;
		}

		if ( IsElapsed() )
		{
			if ( amount > 0.0f )
				m_duration = amount;

			m_timestamp += m_duration;

			// If we are still expired, add a multiple of the interval 
			// until we become non-elapsed
			float remaining = GetRemainingTime();
			if ( remaining < 0.0f)
			{
				float numIntervalsRequired = -std::floorf( remaining / m_duration );
				m_timestamp += m_duration * numIntervalsRequired;
			}

			// We should no longer be elapsed
			contract_assert( !IsElapsed() );

			return true;
		}

		return false;
	}

private:
	CNetworkVar( float, m_duration, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );
	CNetworkVar( float, m_timestamp, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );
	float Now( void ) const;		// work-around since client header doesn't like inlined gpGlobals->curtime
};

EXTERN_RECV_TABLE(DT_CountdownTimer);

//--------------------------------------------------------------------------------------------------------------
/**
* Simple class for tracking change in values over time.
*/
inline constexpr auto TIMELINE_ARRAY_SIZE = 64;
inline constexpr auto TIMELINE_INTERVAL_START = 0.25f;

enum TimelineCompression_t
{
	TIMELINE_COMPRESSION_SUM,
	TIMELINE_COMPRESSION_COUNT_PER_INTERVAL,
	TIMELINE_COMPRESSION_AVERAGE,
	TIMELINE_COMPRESSION_AVERAGE_BLEND,

	TIMELINE_COMPRESSION_TOTAL
};

class CTimeline : public IntervalTimer
{
	[[= ks::reflect::NetTable{ .name = "DT_Timeline", .base = false } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTimeline, IntervalTimer );
	DECLARE_EMBEDDED_NETWORKVAR();

	CTimeline( void )
	{
		ClearValues();
	}

	void ClearValues( void );
	void ClearAndStart( void ) { ClearValues(); Start(); }
	void StopRecording( void ) { m_bStopped = true; }
	void RecordValue( float flValue );
	void RecordFinalValue( float flValue ) { RecordValue( flValue ); StopRecording(); }

	int Count( void ) const
	{
		return m_nBucketCount;
	}

	float GetValue( int i ) const;

	float GetValueAtInterp( float fInterp ) const;

	float GetValueTime( int i ) const;

	float GetInterval( void ) const
	{
		return m_flInterval;
	}

	void SetCompressionType( TimelineCompression_t nCompressionType )
	{
		m_nCompressionType = nCompressionType;
	}

	TimelineCompression_t GetCompressionType( void ) const
	{
		return m_nCompressionType;
	}

private:
	int GetCurrentBucket( void )
	{
		return static_cast<float>( Now() - m_timestamp ) / m_flInterval;
	}

	void Compress( void );

	CNetworkArray( float, m_flValues, TIMELINE_ARRAY_SIZE, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
	CNetworkArray( int, m_nValueCounts, TIMELINE_ARRAY_SIZE, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE, .enc = ks::reflect::WireEnc::Float } ]] );
	CNetworkVar( int, m_nBucketCount, [[= ks::reflect::Net{ .bits = NumBitsForCount( TIMELINE_ARRAY_SIZE ), .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( float, m_flInterval, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_flFinalValue, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( TimelineCompression_t, m_nCompressionType, [[= ks::reflect::Net{ .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( bool, m_bStopped, [[= ks::reflect::Net{} ]] );
};

EXTERN_RECV_TABLE(DT_Timeline);

const char* ReadAndAllocStringValue( KeyValues *pSub, const char *pName, const char *pFilename = nullptr );

int UTIL_StringFieldToInt( const char *szValue, const char **pValueStrings, int iNumStrings );

int UTIL_CountNumBitsSet( unsigned int nVar );
int UTIL_CountNumBitsSet( uint64 nVar );

bool UTIL_GetModDir( char *lpszTextOut, unsigned int nSize );

/*UTIL_CalcFrustumThroughPolygon - Given a frustum and a polygon, calculate how the current frustum would clip the polygon, then generate a new frustum that runs along the edge of the clipped polygon.
-returns number of planes in the output frustum, 0 if the polygon was completely clipped by the input frustum
-vFrustumOrigin can be thought of as the camera origin if your frustum is a view frustum
-planes should face inward
-iPreserveCount will preserve N planes at the end of your input frustum and ensure they're at the end of your output frustum. Assuming your input frustum is of type "Frustum", a value of 2 would preserve your near and far planes
-to ensure that your output frustum can hold the entire complex frustum we generate. Make it of size (iPolyVertCount + iCurrentFrustumPlanes + iPreserveCount). Otherwise the output frustum will be simplified to fit your maximum output by eliminating bounding planes with the clipped area.
-a lack of input frustum is considered valid input*/
int UTIL_CalcFrustumThroughConvexPolygon( const Vector *pPolyVertices, int iPolyVertCount, const Vector &vFrustumOrigin, const VPlane *pInputFrustumPlanes, int iInputFrustumPlanes, VPlane *pOutputFrustumPlanes, int iMaxOutputPlanes, int iPreserveCount );

//-----------------------------------------------------------------------------
// class CFlaggedEntitiesEnum
//-----------------------------------------------------------------------------
// enumerate entities that match a set of edict flags into a static array
class CFlaggedEntitiesEnum : public IPartitionEnumerator
{
public:
	CFlaggedEntitiesEnum( CBaseEntity **pList, int listMax, int flagMask );

	// This gets called	by the enumeration methods with each element
	// that passes the test.
	virtual IterationRetval_t EnumElement( IHandleEntity *pHandleEntity );

	int GetCount() { return m_count; }
	bool AddToList( CBaseEntity *pEntity );

private:
	CBaseEntity		**m_pList;
	int				m_listMax;
	int				m_flagMask;
	int				m_count;
};

int			UTIL_EntitiesAlongRay( const Ray_t &ray, CFlaggedEntitiesEnum *pEnum  );

inline int UTIL_EntitiesAlongRay( CBaseEntity **pList, int listMax, const Ray_t &ray, int flagMask )
{
	CFlaggedEntitiesEnum rayEnum( pList, listMax, flagMask );
	return UTIL_EntitiesAlongRay( ray, &rayEnum );
}

#define EASY_DIFFPRINT_CONDITIONAL( conditions, entity, szFormatString, ... )
#define EASY_DIFFPRINT( entity, szFormatString, ... )

void		UTIL_GetNextCommandLength( const char *pText, int nMaxLen, int *pCommandLength, int *pNextCommandOffset );

//--------------------------------------------------------------------------------------------------------------
/**
 *  remove double spaces and empty bold/italic HTML tags from a string
 */
void UTIL_TrimEmptyWhitespaceFromHTML( wchar_t* pszDescWriter, std::size_t descWriterByteSize, const wchar_t* pszDescReader );
void UTIL_TrimEmptyWhitespaceFromHTML( char* pszDescWriter, std::size_t descWriterByteSize, const char* pszDescReader );

#undef DECLARE_AUTO_LIST
#undef IMPLEMENT_AUTO_LIST_GET
#undef IMPLEMENT_AUTO_LIST
#undef EASY_DIFFPRINT_CONDITIONAL
#undef EASY_DIFFPRINT

// near and far Z it uses to render the world.
inline constexpr int VIEW_NEARZ = 7;
//#define VIEW_FARZ	28400

//-----------------------------------------------------------------------------
// There's a difference between the 'current view' and the 'main view'
// The 'main view' is where the player is sitting. Current view is just
// what's currently being rendered, which, owing to monitors or water,
// could be just about anywhere.
//-----------------------------------------------------------------------------
const Vector &MainViewOrigin( int nSlot );
const QAngle &MainViewAngles( int nSlot );
const Vector &PrevMainViewOrigin( int nSlot );
const QAngle &PrevMainViewAngles( int nSlot );
const VMatrix &MainWorldToViewMatrix( int nSlot );
const Vector &MainViewForward( int nSlot );
const Vector &MainViewRight( int nSlot );
const Vector &MainViewUp( int nSlot );

const Vector &CurrentViewOrigin();
const QAngle &CurrentViewAngles();
const VMatrix &CurrentWorldToViewMatrix();
const Vector &CurrentViewForward();
const Vector &CurrentViewRight();
const Vector &CurrentViewUp();

void AllowCurrentViewAccess( bool allow );
bool IsCurrentViewAccessAllowed();

// Returns true of the sphere is outside the frustum defined by pPlanes.
// (planes point inwards).
bool R_CullSphere( const VPlane *pPlanes, int nPlanes, const Vector *pCenter, float radius );
float ScaleFOVByWidthRatio( float fovDegrees, float ratio );

extern ConVar mat_wireframe;

extern const ConVar *sv_cheats;

inline int WireFrameMode( void )
{
	if ( !sv_cheats )
	{
		sv_cheats = cvar->FindVar( "sv_cheats" );
	}

	if ( sv_cheats && sv_cheats->GetBool() )
		return mat_wireframe.GetInt();
	else
		return 0;
}

inline bool ShouldDrawInWireFrameMode( void )
{
	if ( !sv_cheats )
	{
		sv_cheats = cvar->FindVar( "sv_cheats" );
	}

	if ( sv_cheats && sv_cheats->GetBool() )
		return ( mat_wireframe.GetInt() != 0 );
	else
		return false;
}

void ComputeCameraVariables( const Vector &vecOrigin, const QAngle &vecAngles, Vector *pVecForward, Vector *pVecRight, Vector *pVecUp, VMatrix *pMatCamInverse );

extern ConVar mat_wireframe;
extern ConVar building_cubemaps;

// Transform into view space (translate and rotate the camera into the origin).
void ViewTransform( const Vector &worldSpace, Vector &viewSpace );

// Transform a world point into normalized screen space (X and Y from -1 to 1).
// Returns 0 if the point is behind the viewer.
int ScreenTransform( const Vector& point, Vector& screen );

extern ConVar r_updaterefracttexture;
extern int g_viewscene_refractUpdateFrame;
extern int g_nCurrentPortalRender;
extern int g_nRefractUpdatePortalRender;
extern bool g_bAllowMultipleRefractUpdatesPerScenePerFrame;
bool DrawingShadowDepthView( void );

inline void UpdateRefractTexture( int x, int y, int w, int h, bool bForceUpdate = false )
{
	contract_assert( !DrawingShadowDepthView() );

	if ( !r_updaterefracttexture.GetBool() )
		return;

	CMatRenderContextPtr pRenderContext( materials );
	ITexture *pTexture = GetPowerOfTwoFrameBufferTexture();
	{
		// forced or only once per frame 
		Rect_t rect;
		rect.x = x;
		rect.y = y;
		rect.width = w;
		rect.height = h;
		pRenderContext->CopyRenderTargetToTextureEx( pTexture, 0, &rect, true ? nullptr : &rect );

		g_viewscene_refractUpdateFrame = gpGlobals->framecount;
	}
	pRenderContext->SetFrameBufferCopyTexture( pTexture );
}

inline void UpdateRefractTexture( bool bForceUpdate = false )
{
	contract_assert( !DrawingShadowDepthView() );

	CMatRenderContextPtr pRenderContext( materials );

	int x,y,w,h;
	pRenderContext->GetViewport( x, y, w, h );
	UpdateRefractTexture( x, y, w, h, bForceUpdate );
}

inline void UpdateScreenEffectTexture( int textureIndex, int x, int y, int w, int h, bool bDestFullScreen = false, Rect_t *pActualRect = nullptr )
{
	Rect_t srcRect;
	srcRect.x = x;
	srcRect.y = y;
	srcRect.width = w;
	srcRect.height = h;

	CMatRenderContextPtr pRenderContext( materials );
	ITexture *pTexture = GetFullFrameFrameBufferTexture( textureIndex );
	int nSrcWidth, nSrcHeight;
	pRenderContext->GetRenderTargetDimensions( nSrcWidth, nSrcHeight );
	int nDestWidth = pTexture->GetActualWidth();
	int nDestHeight = pTexture->GetActualHeight();

	Rect_t destRect = srcRect;
	if( !bDestFullScreen && ( nSrcWidth > nDestWidth || nSrcHeight > nDestHeight ) )
	{
		// the source and target sizes aren't necessarily the same (specifically in dx7 where 
		// nonpow2 rendertargets aren't supported), so lets figure it out here.
		float scaleX = ( float )nDestWidth / ( float )nSrcWidth;
		float scaleY = ( float )nDestHeight / ( float )nSrcHeight;
		destRect.x = srcRect.x * scaleX;
		destRect.y = srcRect.y * scaleY;
		destRect.width = srcRect.width * scaleX;
		destRect.height = srcRect.height * scaleY;
		destRect.x = clamp( destRect.x, 0, nDestWidth );
		destRect.y = clamp( destRect.y, 0, nDestHeight );
		destRect.width = clamp( destRect.width, 0, nDestWidth - destRect.x );
		destRect.height = clamp( destRect.height, 0, nDestHeight - destRect.y );
	}

	pRenderContext->CopyRenderTargetToTextureEx( pTexture, 0, &srcRect, bDestFullScreen ? nullptr : &destRect );
	pRenderContext->SetFrameBufferCopyTexture( pTexture, textureIndex );

	if ( pActualRect )
	{
		pActualRect->x = destRect.x;
		pActualRect->y = destRect.y;
		pActualRect->width = destRect.width;
		pActualRect->height = destRect.height;
	}
}

//-----------------------------------------------------------------------------
// Draws the screen effect
//-----------------------------------------------------------------------------
inline void DrawScreenEffectMaterial( IMaterial *pMaterial, int x, int y, int w, int h )
{
	Rect_t actualRect;
	UpdateScreenEffectTexture( 0, x, y, w, h, false, &actualRect );
	ITexture *pTexture = GetFullFrameFrameBufferTexture( 0 );

	CMatRenderContextPtr pRenderContext( materials );

	pRenderContext->DrawScreenSpaceRectangle( pMaterial, x, y, w, h,
		actualRect.x, actualRect.y, actualRect.x+actualRect.width-1, actualRect.y+actualRect.height-1, 
		pTexture->GetActualWidth(), pTexture->GetActualHeight() );
}

//intended for use by dynamic meshes to naively update front buffer textures needed by a material
inline void UpdateFrontBufferTexturesForMaterial( IMaterial *pMaterial, bool bForce = false )
{
	contract_assert( !DrawingShadowDepthView() );

	if( pMaterial->NeedsPowerOfTwoFrameBufferTexture() )
	{
		UpdateRefractTexture( bForce );
	}
	else if( pMaterial->NeedsFullFrameBufferTexture() )
	{
		const CViewSetup *pView = view->GetViewSetup();
		UpdateScreenEffectTexture( 0, pView->x, pView->y, pView->width, pView->height );
	}
}

inline void UpdateScreenEffectTexture( void )
{
	contract_assert( !DrawingShadowDepthView() );

	const CViewSetup *pViewSetup = view->GetViewSetup();
	UpdateScreenEffectTexture( 0, pViewSetup->x, pViewSetup->y, pViewSetup->width, pViewSetup->height);
}

// reset the tonem apping to a constant value, and clear the filter bank
void ResetToneMapping(float value);

void UpdateFullScreenDepthTexture( void );

class CBaseEntity;

using namespace ::ks::reflect;

inline constexpr auto VIEWANIM_RELATIVE = (1<<0); // angles in keyframe are relative, add anim to current angles
inline constexpr auto VIEWANIM_IGNORE_X = (1<<1); // ignore the x component of this animation
inline constexpr auto VIEWANIM_IGNORE_Y = (1<<2); // ditto for y
inline constexpr auto VIEWANIM_IGNORE_Z = (1<<3); // ditto for z

class CViewAngleKeyFrame
{	
public:
	CViewAngleKeyFrame( QAngle vecAngles, float flTime, int iFlags );

	// the target angles for this keyframe in the view angle animation
	QAngle m_vecAngles;

	// time position of this keyframe
	float m_flTime;	

	int m_iFlags;
};

typedef void (*ViewAnimCompleteCallback)( void );

class CViewAngleAnimation : public CBaseEntity
{
public:
	CViewAngleAnimation();
	~CViewAngleAnimation();

	virtual void Spawn();

	void DeleteKeyFrames();

	void LoadViewAnimFile( const char *pKeyFrameFileName );
	void SaveAsAnimFile( const char *pKeyFrameFileName );

	void AddKeyFrame( CViewAngleKeyFrame *pKeyFrame );
	bool IsFinished( void );
	void RunAnimation( QAngle angles );
	void ClientThink();

	void SetAnimCompleteCallback( ViewAnimCompleteCallback pFunc )
	{
		m_pAnimCompleteCallback = pFunc;
	}

private:
	void SetAngles( QAngle vecCalculatedAngles );

	float m_flAnimStartTime;	// time this animation started
	bool m_bFinished;

	std::vector< CViewAngleKeyFrame * > m_KeyFrames;

	QAngle m_vecBaseAngles;

	int m_iFlags;

	ViewAnimCompleteCallback m_pAnimCompleteCallback;
};

//-----------------------------------------------------------------------------
// Purpose: Implements the debugging elements of view rendering
//-----------------------------------------------------------------------------
class CDebugViewRender
{
	DECLARE_CLASS_NOBASE( CDebugViewRender );
public:
	// Draws all the debugging info
	static void	Draw3DDebuggingInfo( const CViewSetup &view );
	static void	Draw2DDebuggingInfo( const CViewSetup &view );
	static void GenerateOverdrawForTesting();
};

bool DoEnginePostProcessing( int x, int y, int w, int h, bool bFlashlightIsOn, bool bPostUI = false );
bool DoImageSpaceMotionBlur( const CViewSetup &view );
bool IsDepthOfFieldEnabled();
void DoDepthOfField( const CViewSetup &view );
void BlurEntity( IClientRenderable *pRenderable, bool bPreDraw, int drawFlags, const RenderableInstance_t &instance, const CViewSetup &view, int x, int y, int w, int h );

void UpdateMaterialSystemTonemapScalar();

float GetCurrentTonemapScale();

void SetOverrideTonemapScale( bool bEnableOverride, float flTonemapScale );

void SetOverridePostProcessingDisable( bool bForceOff );

void DoBlurFade( float flStrength, float flDesaturate, int x, int y, int w, int h );

void SetPostProcessParams( const PostProcessParameters_t *pPostProcessParameters );

void SetViewFadeParams( byte r, byte g, byte b, byte a, bool bModulate );

bool ApplyIronSightScopeEffect( int x, int y, int w, int h, CViewSetup *viewSetup, bool bPreparationStage );

class CViewRender;

class CClientRenderablesList;
class CBaseEntity;
class CBasePlayer;
class CPointCamera;


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CClientViewSetup;
struct ClientWorldListInfo_t;

//-----------------------------------------------------------------------------
// Data specific to intro mode to control rendering.
//-----------------------------------------------------------------------------
struct IntroDataBlendPass_t
{
	int m_BlendMode;
	float m_Alpha; // in [0.0f,1.0f]  This needs to add up to 1.0 for all passes, unless you are fading out.
};

struct IntroData_t
{
	bool	m_bDrawPrimary;
	Vector	m_vecCameraView;
	QAngle	m_vecCameraViewAngles;
	float	m_playerViewFOV;
	std::vector< IntroDataBlendPass_t > m_Passes;

	// Fade overriding for the intro
	float	m_flCurrentFadeColor[4];
};

// Robin, make this point at something to get intro mode.
extern IntroData_t *g_pIntroData;

// This identifies the view for certain systems that are unique per view (e.g. pixel visibility)
// NOTE: This is identifying which logical part of the scene an entity is being redered in
// This is not identifying a particular render target necessarily.  This is mostly needed for entities that
// can be rendered more than once per frame (pixel vis queries need to be identified per-render call)
enum view_id_t
{
	VIEW_ILLEGAL = -2,
	VIEW_NONE = -1,
	VIEW_MAIN = 0,
	VIEW_3DSKY = 1,
	VIEW_MONITOR = 2,
	VIEW_REFLECTION = 3,
	VIEW_REFRACTION = 4,
	VIEW_INTRO_PLAYER = 5,
	VIEW_INTRO_CAMERA = 6,
	VIEW_SHADOW_DEPTH_TEXTURE = 7,
	VIEW_SSAO = 8,
	VIEW_ID_COUNT
};
view_id_t CurrentViewID();

inline constexpr auto PASS_BUILDLISTS = (1<<0);
inline constexpr auto PASS_DRAWLISTS = (1<<1);

//-----------------------------------------------------------------------------
// Purpose: Stored pitch drifting variables
//-----------------------------------------------------------------------------
class CPitchDrift
{
public:
	float		pitchvel;
	bool		nodrift;
	float		driftmove;
	double		laststop;
};

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
struct ViewCustomVisibility_t
{
	ViewCustomVisibility_t();

	void AddVisOrigin( const Vector& origin );

	void ForceVisOverride( VisOverrideData_t& visData )
	{
		m_VisData = visData;
	}

	void ForceViewLeaf ( int iViewLeaf )
	{
		m_iForceViewLeaf = iViewLeaf;
	}

	// Set to true if you want to use multiple origins for doing client side map vis culling
	// NOTE:  In generaly, you won't want to do this, and by default the 3d origin of the camera, as above,
	//  will be used as the origin for vis, too.
	int				m_nNumVisOrigins;
	// Array of origins
	Vector			m_rgVisOrigins[ MAX_VIS_LEAVES ];

	// The view data overrides for visibility calculations with area portals
	VisOverrideData_t m_VisData;

	// The starting leaf to determing which area to start in when performing area portal culling on the engine
	// Default behavior is to use the leaf the camera position is in.
	int				m_iForceViewLeaf;
};

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
struct WaterRenderInfo_t
{
	bool m_bCheapWater : 1;
	bool m_bReflect : 1;
	bool m_bRefract : 1;
	bool m_bReflectEntities : 1;
	bool m_bReflectOnlyMarkedEntities : 1;
	bool m_bDrawWaterSurface : 1;
	bool m_bOpaqueWater : 1;
	bool m_bPseudoTranslucentWater : 1;
	bool m_bReflect2DSkybox : 1;
};

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
class CBase3dView : public CRefCounted<CBase3dView>,
					protected CViewSetup
{
	DECLARE_CLASS_NOBASE( CBase3dView );
public:
	explicit CBase3dView( CViewRender *pMainView );
	virtual ~CBase3dView() {}

	VPlane *		GetFrustum();
	virtual int		GetDrawFlags() { return 0; }

protected:
	// @MULTICORE (toml 8/11/2006): need to have per-view frustum. Change when move view stack to client
	VPlane			*m_Frustum;
	CViewRender *m_pMainView;
	int				m_nSlot;
};

// Frustum cache.
struct FrustumCache_t
{
	int			m_nFrameCount;
	Frustum_t	m_Frustums[MAX_SPLITSCREEN_PLAYERS];

	FrustumCache_t() { m_nFrameCount = 0; }

	bool IsValid( void ) { return ( m_nFrameCount == gpGlobals->framecount ); }
	void SetUpdated( void ) { m_nFrameCount = gpGlobals->framecount; }

	void Add( const CViewSetup *pView, int iSlot );
};

FrustumCache_t *FrustumCache( void );

//-----------------------------------------------------------------------------
// Base class for 3d views
//-----------------------------------------------------------------------------
class CRendering3dView : public CBase3dView
{
	DECLARE_CLASS( CRendering3dView, CBase3dView );
public:
	explicit CRendering3dView( CViewRender *pMainView );
	~CRendering3dView() { ReleaseLists(); }

	void Setup( const CViewSetup &setup );

	// What are we currently rendering? Returns a combination of DF_ flags.
	virtual int		GetDrawFlags();
	virtual void	Draw() {};

protected:

	// Fog setup
	void			EnableWorldFog( void );
	void			SetFogVolumeState( const VisibleFogVolumeInfo_t &fogInfo, bool bUseHeightFog );

	// Draw setup
	void			SetupRenderablesList( int viewID, bool bFastEntityRendering = false, bool bDrawDepthViewNonCachedObjectsOnly = false );

	// If iForceViewLeaf is not -1, then it uses the specified leaf as your starting area for setting up area portal culling.
	// This is used by water since your reflected view origin is often in solid space, but we still want to treat it as though
	// the first portal we're looking out of is a water portal, so our view effectively originates under the water.
	void			BuildWorldRenderLists( bool bDrawEntities, int iForceViewLeaf = -1, bool bUseCacheIfEnabled = true, bool bShadowDepth = false, float *pReflectionWaterHeight = nullptr );

	void			BuildWorldRenderLists_Epilogue( bool bShadowDepth );
	void			BuildRenderableRenderLists_Epilogue( int viewID );

	// Purpose: Builds render lists for renderables. Called once for refraction, once for over water
	void			BuildRenderableRenderLists( int viewID, bool bFastEntityRendering = false, bool bDrawDepthViewNonCachedObjectsOnly = false );

	// More concise version of the above BuildRenderableRenderLists().  Called for shadow depth map rendering
	void			BuildShadowDepthRenderableRenderLists();

	void			DrawWorld( IMatRenderContext *pRenderContext, float waterZAdjust );

	// Draws all opaque/translucent renderables in leaves that were rendered
	enum RenderablesRenderPath_t
	{
		RENDERABLES_RENDER_PATH_NORMAL							= 0,
		RENDERABLES_RENDER_PATH_SHADOWDEPTH_DEFAULT				= ( 1 << 0 ),
		RENDERABLES_RENDER_PATH_SHADOWDEPTH_BUILD_GEOCACHE		= ( 1 << 1 ),
		RENDERABLES_RENDER_PATH_SHADOWDEPTH_USE_GEOCACHE		= ( 1 << 2 ),
	};
	void			DrawOpaqueRenderables( IMatRenderContext *pRenderContext, RenderablesRenderPath_t eRenderPath, ERenderDepthMode_t DepthMode, std::vector< CClientRenderablesList::CEntry * > *pDeferClippedOpaqueRenderables_Out, RenderGroup_t nGroup = RENDER_GROUP_OPAQUE );
	void			DrawDeferredClippedOpaqueRenderables( IMatRenderContext *pRenderContext, RenderablesRenderPath_t eRenderPath, ERenderDepthMode_t DepthMode, std::vector< CClientRenderablesList::CEntry * > *pDeferClippedOpaqueRenderables );
	void			DrawTranslucentRenderables( bool bInSkybox, bool bShadowDepth );

	// Renders all translucent entities in the render list
	void			DrawTranslucentRenderablesNoWorld( bool bInSkybox );

	// Renders all translucent entities in the render list that ignore the z-buffer
	void			DrawNoZBufferTranslucentRenderables( void );

	// Renders all translucent world surfaces in a particular set of leaves
	void			DrawTranslucentWorldInLeaves( IMatRenderContext *pRenderContext, bool bShadowDepth );

	// Renders all translucent world + detail objects in a particular set of leaves
	void			DrawTranslucentWorldAndDetailPropsInLeaves( IMatRenderContext *pRenderContext, int iCurLeaf, int iFinalLeaf, int nEngineDrawFlags, int &nDetailLeafCount, LeafIndex_t* pDetailLeafList, bool bShadowDepth );

	// Purpose: Computes the actual world list info based on the render flags
	void			PruneWorldListInfo();

	// Sets up automatic z-prepass on the 360 and PS/3. No-op on PC.
	void			BeginConsoleZPass();
	void			EndConsoleZPass();

	void ReleaseLists();

	//-----------------------------------------------
	// Combination of DF_ flags.
	int m_DrawFlags;
	int m_ClearFlags;

	IWorldRenderList *m_pWorldRenderList;

	CClientRenderablesList *m_pRenderables;
	ClientWorldListInfo_t *m_pWorldListInfo;
	
	ViewCustomVisibility_t *m_pCustomVisibility;
};

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------

class CRenderExecutor
{
	DECLARE_CLASS_NOBASE( CRenderExecutor );
public:

	virtual void AddView( CRendering3dView *pView ) = 0;

	virtual void Execute() = 0;

protected:
	explicit CRenderExecutor( CViewRender *pMainView ) : m_pMainView( pMainView ) {}
	CViewRender *m_pMainView;
};

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------

class CSimpleRenderExecutor : public CRenderExecutor
{
	DECLARE_CLASS( CSimpleRenderExecutor, CRenderExecutor );
public:
	explicit CSimpleRenderExecutor( CViewRender *pMainView ) : CRenderExecutor( pMainView ) {}

	void AddView( CRendering3dView *pView );

	void Execute() {}
};

//-----------------------------------------------------------------------------
// Cache all the data needed for a single view so that "BuildWorldList" and 
// "BuildRenderablesList" jobs are thread safe
// Also holds word/renderables lists and world/renderables jobs
//-----------------------------------------------------------------------------
class CConcurrentViewData
{
public:

	void Init( void );
	void Purge( void );

	CVolumeCuller			m_volumeCuller;
	Frustum_t				m_Frustum;
	std::vector< Frustum_t > m_AreaFrustums;
	Frustum_t *				m_frustumList[MAX_MAP_AREAS];

	// Lists

	IWorldRenderList*		m_pWorldRenderList;
	ClientWorldListInfo_t*	m_pWorldListInfo;
	CClientRenderablesList*	m_pRenderablesList;

	// Jobs

	CJob*					m_pBuildWorldListJob;
	CJob*					m_pBuildRenderablesListJob;
	bool					m_bWaitForWorldList;
};

//-----------------------------------------------------------------------------
// For the support of parallel view building jobs (BuildWorldLists, BuildRenderables, etc)
//-----------------------------------------------------------------------------
class CConcurrentViewBuilder
{
public:

	CConcurrentViewBuilder();
	~CConcurrentViewBuilder();

	void					Init( void );
	void					Purge( void );

	void					SetBuildWRThreaded( bool bThreaded ) { m_bThreaded = bThreaded; }
	bool					GetBuildWRThreaded( void ) { return m_bThreaded; }

	// BuildViewID methods for supporting concurrent view building (BuildWorldLists, BuildRenderables, etc)

	void					ResetBuildViewID( void );

	// get current view index
	int						GetBuildViewID( void );

	// call at the start of each view
	void					PushBuildView( void );
	void					PopBuildView( void );

	int						GetPassFlags( void ) { return m_passFlags; };
	void					SetPassFlags( int flags ) { m_passFlags = flags; };

	// Volume cullers

	void					CacheBuildViewVolumeCuller( const CVolumeCuller *pVC );
	const CVolumeCuller*	GetBuildViewVolumeCuller( int buildViewID = -1 ) const ;

	// Frustum data

	void					CacheFrustumData( const Frustum_t& frustum, const std::vector< Frustum_t >& aeraFrustums );
	void					CacheFrustumData( void );
	const Frustum_t*		GetBuildViewFrustum( int buildViewID = -1 ) const;
	const std::vector< Frustum_t >* GetBuildViewAeraFrustums( int buildViewID = -1 ) const;
	Frustum_t**				GetBuildViewFrustumList( int buildViewID = -1 );

	// Lists
	// Note that the "Getter" will not increment the ref count of the list but the "Setter" will increment it

	IWorldRenderList *		GetWorldRenderListElement( void );
	void					SetWorldRenderListElement( IWorldRenderList *pRenderList );

	// Use the active view if buildViewID is set to -1
	ClientWorldListInfo_t*	GetClientWorldListInfoElement( int buildViewID = -1 );
	WorldListInfo_t*		GetWorldListInfoElement( int buildViewID );
	void					SetWorldListInfoElement( ClientWorldListInfo_t* pWorldListInfo, int buildViewID = -1 );

	CClientRenderablesList*	GetRenderablesListElement( int buildViewID = -1 );
	void					SetRenderablesListElement( CClientRenderablesList* pRenderables );

	// Queue jobs

	void					QueueBuildWorldListJob( CJob* pJob );
	void					WaitForBuildWorldListJob( void );
	void					FlushBuildWorldListJob( void );

	void					QueueBuildRenderablesListJob( CJob* pJob );
	void					WaitForBuildRenderablesListJob( void );
	void					FlushBuildRenderablesListJob( void );
	void					AddDependencyToWorldList( void );

private:

	void AddJobToThreadPool( CJob* pJob );
	void AddToSequentialJobs( CJob* pJob );
	void TryRunSequentialJobs( void );
	void WaitForCurrentSequentialJobAndRunPending();
	void InternalWaitForBuildWorldListJob( int buildViewID );

	// Jobs that will run sequentially. This is the work of a CJob that owns it (see Create).
	class SequentialJobs
	{
	public:

		// Makes a job (the caller owns a reference) that runs the jobs added to it, in order
		static CJob* Create( CJob* pJob = nullptr );
		// pSeqJob has to come from Create
		static void AddJob( CJob* pSeqJob, CJob* pJob );

		~SequentialJobs();

		JobStatus_t Run();

	private:

		std::inplace_vector< CJob *, 16 > m_jobs;
	};

	bool					m_bThreaded;

	// View stack & current view ID

	int						m_buildViewID;
	int						m_nextFreeBuildViewID;

	int*					m_pBuildViewStack;
	int						m_buildViewStack[ MAX_CONCURRENT_BUILDVIEWS ];

	// Flags

	int						m_passFlags;

	// Cached data for each view

	CConcurrentViewData		m_viewData[ MAX_CONCURRENT_BUILDVIEWS ];

	// Sequential jobs currently being executed
	CJob*					m_pCurrentSeqJobs;
	// Pending sequential job - need to wait for the current one to be done before executing
	CJob*					m_pPendingSeqJobs;

	// Keeps track of all jobs added to the global threadpool
	// We can then wait for them in Purge in case a job has been added in the build pass
	// but we didn't wait for it in the draw pass (because the list wasn't actually drawn)
	std::vector< CJob* >		mJobsAddedToThreadPool;
};

extern CConcurrentViewBuilder g_viewBuilder;

//-----------------------------------------------------------------------------
// Purpose: Implements the interface to view rendering for the client .dll
//-----------------------------------------------------------------------------

class CViewRender : public IViewRender
{
	DECLARE_CLASS_NOBASE( CViewRender );
public:
	virtual void	Init( void );
	virtual void	Shutdown( void );

	const CViewSetup *GetPlayerViewSetup( int nSlot = -1 ) const;

	virtual void	StartPitchDrift( void );
	virtual void	StopPitchDrift( void );

	virtual float	GetZNear();
	virtual float	GetZFar();

	virtual void	OnRenderStart();
	void			DriftPitch (void);

	static CViewRender *	GetMainView() { return static_cast<CViewRender *>( view ); }

	void			AddViewToScene( CRendering3dView *pView ) { m_SimpleExecutor.AddView( pView ); }

	CMaterialReference &GetWhite();

	virtual void	InitFadeData( void );

protected:
	// Sets up the view parameters
	void			SetUpView();

	// Sets up the view parameters of map overview mode (cl_leveloverview)
	void			SetUpOverView();
	// renders the third person overview spectator mode
	void			SetUpChaseOverview();

	// generates a low-res screenshot for save games
	virtual void	WriteSaveGameScreenshotOfSize( const char *pFilename, int width, int height );
	void			WriteSaveGameScreenshot( const char *filename );

	// This stores all of the view setup parameters that the engine needs to know about

	CViewSetup				&GetView( int nSlot = -1 );
	const CViewSetup		&GetView( int nSlot = -1 ) const;

	CViewSetup		m_UserView[ MAX_SPLITSCREEN_PLAYERS ];
	bool			m_bAllowViewAccess;
	// Pitch drifting data
	CPitchDrift		m_PitchDrift;

	virtual void	RenderPreScene( const CViewSetup &view ) { }
	virtual void	PreViewDrawScene( const CViewSetup &view ) {}
	virtual void	PostViewDrawScene( const CViewSetup &view ) {}

	float			m_flOldChaseOverviewScale;
	float			m_flIdealChaseOverviewScale;
	float			m_flNextIdealOverviewScaleUpdate;
public:
					CViewRender();
	virtual			~CViewRender( void ) {}

//	static bool		IsThirdPersonOverview( void );

// Implementation of IViewRender interface
public:

	void			SetupVis( const CViewSetup& view, unsigned int &visFlags, ViewCustomVisibility_t *pCustomVisibility = nullptr );

	// Render functions
	virtual	void	Render( vrect_t *rect );
	virtual void	RenderView( const CViewSetup &view, const CViewSetup &hudViewSetup, int nClearFlags, int whatToDraw );
	virtual void	RenderPlayerSprites();
	virtual void	Render2DEffectsPreHUD( const CViewSetup &view );
	virtual void	Render2DEffectsPostHUD( const CViewSetup &view );
	virtual void	RenderSmokeOverlay( bool bPreViewModel = true ) {};
	float			GetLastRenderSmokeOverlayAmount() const { return m_flSmokeOverlayAmount; }

	void			DisableFog( void );

	// Called once per level change
	void			LevelInit( void );
	void			LevelShutdown( void );

	// Add entity to transparent entity queue

	bool			ShouldDrawEntities( void );
	bool			ShouldDrawBrushModels( void );

	const CViewSetup *GetViewSetup( ) const;
	
	void			DisableVis( void );

	// Sets up the view model position relative to the local player
	void			MoveViewModels( );

	// Gets the abs origin + angles of the view models
	void			GetViewModelPosition( int nIndex, Vector *pPos, QAngle *pAngle );

	void			SetCheapWaterStartDistance( float flCheapWaterStartDistance );
	void			SetCheapWaterEndDistance( float flCheapWaterEndDistance );

	void			GetWaterLODParams( float &flCheapWaterStartDistance, float &flCheapWaterEndDistance );

	virtual void	QueueOverlayRenderView( const CViewSetup &view, int nClearFlags, int whatToDraw );

	virtual void	GetScreenFadeDistances( float *pMin, float *pMax, float *pScale );

	virtual CBaseEntity *GetCurrentlyDrawingEntity();
	virtual void		  SetCurrentlyDrawingEntity( CBaseEntity *pEnt );

	virtual bool		UpdateShadowDepthTexture( ITexture *pRenderTarget, ITexture *pDepthTexture, const CViewSetup &shadowView, bool bRenderWorldAndObjects = true, bool bRenderViewModels = false );

	int GetBaseDrawFlags() { return m_BaseDrawFlags; }
	virtual bool ShouldForceNoVis()  { return m_bForceNoVis; }
	int				BuildRenderablesListsNumber() const { return m_BuildRenderableListsNumber; }
	int				IncRenderablesListsNumber()  { return ++m_BuildRenderableListsNumber; }

	int				BuildWorldListsNumber() const;
	int				IncWorldListsNumber() { return ++m_BuildWorldListsNumber; }

	virtual VPlane*	GetFrustum() { return ( m_pActiveRenderer ) ? m_pActiveRenderer->GetFrustum() : m_Frustum; }

	// What are we currently rendering? Returns a combination of DF_ flags.
	virtual int		GetDrawFlags() { return ( m_pActiveRenderer ) ? m_pActiveRenderer->GetDrawFlags() : 0; }

	CBase3dView *	GetActiveRenderer() { return m_pActiveRenderer; }
	CBase3dView *	SetActiveRenderer( CBase3dView *pActiveRenderer ) { CBase3dView *pPrevious = m_pActiveRenderer; m_pActiveRenderer =  pActiveRenderer; return pPrevious; }

	void			FreezeFrame( float flFreezeTime );

	void SetWaterOverlayMaterial( IMaterial *pMaterial )
	{
		m_UnderWaterOverlayMaterial.Init( pMaterial );
	}

	void			DrawViewModelsShadowDepth( const CViewSetup &view );

protected:
	int				m_BuildWorldListsNumber;

	// General draw methods
	// baseDrawFlags is a combination of DF_ defines. DF_MONITOR is passed into here while drawing a monitor.
	void			ViewDrawScene( bool bDrew3dSkybox, SkyboxVisibility_t nSkyboxVisible, const CViewSetup &view, int nClearFlags, view_id_t viewID, bool bDrawViewModel = false, int baseDrawFlags = 0, ViewCustomVisibility_t *pCustomVisibility = nullptr );

	void			DrawMonitors( const CViewSetup &cameraView );

	bool			DrawOneMonitor( ITexture *pRenderTarget, int cameraNum, CPointCamera *pCameraEnt, const CViewSetup &cameraView, CBasePlayer *localPlayer, 
						int x, int y, int width, int height );

	// Drawing primitives
	bool			ShouldDrawViewModel( bool drawViewmodel );
	void			DrawViewModels( const CViewSetup &view, bool drawViewmodel, bool bDrawScopeLensMask = false );

	void			PerformScreenSpaceEffects( int x, int y, int w, int h );

	// Overlays
	void			SetScreenOverlayMaterial( IMaterial *pMaterial );
	IMaterial		*GetScreenOverlayMaterial( );
	void			PerformScreenOverlay( int x, int y, int w, int h );

	void DrawUnderwaterOverlay( void );

	// Water-related methods
	void			DrawWorldAndEntities( bool drawSkybox, const CViewSetup &view, int nClearFlags, ViewCustomVisibility_t *pCustomVisibility = nullptr );

	virtual void			ViewDrawScene_Intro( const CViewSetup &view, int nClearFlags, const IntroData_t &introData );

	// Determines what kind of water we're going to use
	void			DetermineWaterRenderInfo( const VisibleFogVolumeInfo_t &fogVolumeInfo, WaterRenderInfo_t &info );

	bool			UpdateRefractIfNeededByList( CViewModelRenderablesList::RenderGroups_t &list );
	void			DrawRenderablesInList( CViewModelRenderablesList::RenderGroups_t &list, int flags = 0 );

	// Sets up, cleans up the main 3D view
	void			SetupMain3DView( int nSlot, const CViewSetup &view, const CViewSetup &hudViewSetup, int &nClearFlags, ITexture *pRenderTarget );
	void			CleanupMain3DView( const CViewSetup &view );

	void			GetLetterBoxRectangles( int nSlot, const CViewSetup &view, std::vector< vrect_t >& vecLetterBoxRectangles );
	void			DrawLetterBoxRectangles( int nSlot, const std::vector< vrect_t >& vecLetterBoxRectangles );

	void			EnableWaterDepthFeathing( IMaterial *pWaterMaterial, bool bEnable );

	// This stores the current view
 	CViewSetup		m_CurrentView;

	// VIS Overrides
	// Set to true to turn off client side vis ( !!!! rendering will be slow since everything will draw )
	bool			m_bForceNoVis;	

	// Some cvars needed by this system
	const ConVar	*m_pDrawEntities;
	const ConVar	*m_pDrawBrushModels;

	// Some materials used...
	CMaterialReference	m_TranslucentSingleColor;
	CMaterialReference	m_ModulateSingleColor;
	CMaterialReference	m_ScreenOverlayMaterial;
	CMaterialReference m_UnderWaterOverlayMaterial;

	Vector				m_vecLastFacing;
	float				m_flCheapWaterStartDistance;
	float				m_flCheapWaterEndDistance;

	CViewSetup			m_OverlayViewSetup;
	int					m_OverlayClearFlags;
	int					m_OverlayDrawFlags;
	bool				m_bDrawOverlay;

	int					m_BaseDrawFlags;	// Set in ViewDrawScene and OR'd into m_DrawFlags as it goes.
	CBaseEntity		*m_pCurrentlyDrawingEntity;

	int				m_BuildRenderableListsNumber;

	friend class CBase3dView;

	Frustum m_Frustum;

	CBase3dView *m_pActiveRenderer;
	CSimpleRenderExecutor m_SimpleExecutor;

	struct FreezeParams_t
	{
		FreezeParams_t() : m_bTakeFreezeFrame( false ), m_flFreezeFrameUntil( 0.0f ) {}

		bool			m_bTakeFreezeFrame;
		float			m_flFreezeFrameUntil;
	};

	FreezeParams_t		m_FreezeParams[ MAX_SPLITSCREEN_PLAYERS ];

	FadeData_t			m_FadeData;

	CMaterialReference	m_WhiteMaterial;

	void OnScreenFadeMinSize( const CCommand &args );
	CConCommandMemberAccessor< CViewRender > m_OnScreenFadeMinSize_register{ this, "screenfademinsize", &CViewRender::OnScreenFadeMinSize, "Modify global screen fade min size in pixels", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY };
	void OnScreenFadeMaxSize( const CCommand &args );
	CConCommandMemberAccessor< CViewRender > m_OnScreenFadeMaxSize_register{ this, "screenfademaxsize", &CViewRender::OnScreenFadeMaxSize, "Modify global screen fade max size in pixels", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY };

	float				m_flSmokeOverlayAmount;
};

// This class manages the (persistent) list of squelched players.
class CVoiceBanMgr
{
public:

				CVoiceBanMgr();
				~CVoiceBanMgr();	

	// Init loads the list of squelched players from disk.
	bool		Init(const char *pGameDir);
	void		Term();

	// Saves the state into voice_squelch.dt.
	void		SaveState(const char *pGameDir);

	bool		GetPlayerBan(char const playerID[SIGNED_GUID_LEN]);
	void		SetPlayerBan(char const playerID[SIGNED_GUID_LEN], bool bSquelch);

protected:

	class BannedPlayer
	{
	public:
		char			m_PlayerID[SIGNED_GUID_LEN];
		BannedPlayer	*m_pPrev, *m_pNext;
	};

	void			Clear();
	BannedPlayer*	InternalFindPlayerSquelch(char const playerID[SIGNED_GUID_LEN]);
	BannedPlayer*	AddBannedPlayer(char const playerID[SIGNED_GUID_LEN]);

protected:

	BannedPlayer	m_PlayerHash[256];
};

inline constexpr auto VOICE_MAX_PLAYERS = MAX_PLAYERS;
inline constexpr auto VOICE_MAX_PLAYERS_DW = ((VOICE_MAX_PLAYERS / 32) + !!(VOICE_MAX_PLAYERS & 31));
inline constexpr auto VOICE_DEFAULT_PROXIMITY_RANGE = 1200; //100 feet

using namespace ::ks::reflect;

//#include "VGUI_Bitmap.h"


// This is provided by each mod to access data that may not be the same across mods.
class IVoiceStatusHelper
{
public:
	virtual					~IVoiceStatusHelper()	{}

	// Get RGB color for voice status text about this player.
	virtual void			GetPlayerTextColor(int entindex, int color[3]) = 0;

	// Force it to update the cursor state.
	virtual void			UpdateCursorState() = 0;

	// Return true if the voice manager is allowed to show speaker labels
	// (mods usually return false when the scoreboard is up).
	virtual bool			CanShowSpeakerLabels() = 0;
};

class CVoiceStatus
{
public:
				CVoiceStatus();
	virtual		~CVoiceStatus();

// CHudBase overrides.
public:
	
	// Initialize the cl_dll's voice manager.
	virtual int Init(
		IVoiceStatusHelper *m_pHelper,
		uintp pParentPanel);
	
	// ackPosition is the bottom position of where CVoiceStatus will draw the voice acknowledgement labels.
	virtual void VidInit();

	void LevelInit( void );
	void LevelShutdown( void );

public:
	
	// Call from HUD_Frame each frame.
	void	Frame(double frametime);

	// Called when a player starts or stops talking.
	// entindex is -1 to represent the local client talking (before the data comes back from the server).
	// When the server acknowledges that the local client is talking, then entindex will be gEngfuncs.GetLocalPlayer().
	// entindex is -2 to represent the local client's voice being acked by the server.
	// For local client iSsSlot will always be set >= 0 and designate a valid Splitscreen slot
	// for world-entities and context of local client as "in world" iSsSlot will always be negative
	void	UpdateSpeakerStatus(int entindex, int iSsSlot, bool bTalking);

	// Call from the HUD_CreateEntities function so it can add sprites above player heads.
	void	DrawHeadLabels();
	float	GetHeadLabelOffset( void ) const;
	void	SetHeadLabelsDisabled( bool bDisabled ) { m_bHeadLabelsDisabled = bDisabled; }

	// Called when the server registers a change to who this client can hear.
	bool HandleVoiceMaskMsg(const ks::net::CCSUsrMsg_VoiceMask &msg);

	// The server sends this message initially to tell the client to send their state.
	bool	HandleReqStateMsg(const ks::net::CCSUsrMsg_RequestState &msg);

// Squelch mode functions.
public:

	// When you enter squelch mode, pass in 
	void	StartSquelchMode();
	void	StopSquelchMode();
	bool	IsInSquelchMode();

	// returns true if the target client has been banned
	// playerIndex is of range 1..maxplayers
	bool	IsPlayerBlocked(int iPlayerIndex);

	// If individual game settings require us to prevent communication from this player.
	// Different concept from 'blocking', which persists from session to session or from 'audible' which
	// are gamerules that apply to all users and doesn't apply to all forms of communication (eg text chat to whole server).
	bool	ShouldHideCommunicationFromPlayer( int iPlayerIndex );

	// returns false if the player can't hear the other client due to game rules (eg. the other team)
	bool    IsPlayerAudible(int iPlayerIndex);

	// returns true if the player is currently speaking
	bool	IsPlayerSpeaking(int iPlayerIndex);

	// returns true if the local player is attempting to speak
	bool	IsLocalPlayerSpeaking( int iSsSlot );

	// returns true if the local player is attempting to speak, and is above the volume threshold
	bool	IsLocalPlayerSpeakingAboveThreshold( int iSsSlot );

	// blocks the target client from being heard
	void	SetPlayerBlockedState( int iPlayerIndex );

	void	SetHeadLabelMaterial( const char *pszMaterial );

	IMaterial *GetHeadLabelMaterial( void ) { return m_pHeadLabelMaterial; }

	CUserMessageBinder m_UMCMsgVoiceMask;
	CUserMessageBinder m_UMCMsgRequestState;

private:

	void			UpdateServerState(bool bForce);

	// Update the button artwork to reflect the client's current state.
	void			UpdateBanButton(int iClient);

private:
	float			m_LastUpdateServerState;		// Last time we called this function.
	int				m_bServerModEnable;				// What we've sent to the server about our "voice_modenable" cvar.

	uintp			m_pParentPanel;
	CPlayerBitVec	m_VoicePlayers;		// Who is currently talking. Indexed by client index.
	
	// This is the gamerules-defined list of players that you can hear. It is based on what teams people are on 
	// and is totally separate from the ban list. Indexed by client index.
	CPlayerBitVec	m_AudiblePlayers;

	// Players who have spoken at least once in the game so far
	CPlayerBitVec	m_VoiceEnabledPlayers;	

	// This is who the server THINKS we have banned (it can become incorrect when a new player arrives on the server).
	// It is checked periodically, and the server is told to squelch or unsquelch the appropriate players.
	CPlayerBitVec	m_ServerBannedPlayers;

	IVoiceStatusHelper	*m_pHelper;		// Each mod provides an implementation of this.

	// Squelch mode stuff.
	bool				m_bInSquelchMode;
	
	bool				m_bTalking[ MAX_SPLITSCREEN_CLIENTS ];				// Set to true when the client thinks it's talking.
	bool				m_bAboveThreshold[ MAX_SPLITSCREEN_CLIENTS ];		// Set to true when the client thinks it's voice is above the threshold to send to the server
	bool				m_bServerAcked[ MAX_SPLITSCREEN_CLIENTS ];			// Set to true when the server knows the client is talking.

public:
	
	CVoiceBanMgr		m_BanMgr;				// Tracks which users we have squelched and don't want to hear.

private:

	IMaterial			*m_pHeadLabelMaterial;	// For labels above players' heads.

	bool				m_bBanMgrInitialized;

	int					m_nControlSize;

	bool				m_bHeadLabelsDisabled;

	CountdownTimer		m_bAboveThresholdTimer[ MAX_SPLITSCREEN_CLIENTS ];
	float				m_flTalkTime[ VOICE_MAX_PLAYERS ];
	float				m_flTimeLastUpdate[ VOICE_MAX_PLAYERS ];
};

//-----------------------------------------------------------------------------
// returns true if the player is currently speaking
//-----------------------------------------------------------------------------
[[gnu::always_inline]] inline bool CVoiceStatus::IsPlayerSpeaking(int iPlayerIndex)
{
	return m_VoicePlayers[iPlayerIndex-1] != 0;
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the player can't hear the other client due to game rules (eg. the other team)
// Input  : playerID - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
[[gnu::always_inline]] inline bool CVoiceStatus::IsPlayerAudible(int iPlayer)
{
	return !!m_AudiblePlayers[iPlayer-1];
}

//-----------------------------------------------------------------------------
// returns true if the local player is attempting to speak
//-----------------------------------------------------------------------------
[[gnu::always_inline]] inline bool CVoiceStatus::IsLocalPlayerSpeaking( int iSsSlot )
{
	return m_bTalking[ iSsSlot ];
}

//-----------------------------------------------------------------------------
// returns true if the local player is attempting to speak, and is above the volume threshold
//-----------------------------------------------------------------------------
[[gnu::always_inline]] inline bool CVoiceStatus::IsLocalPlayerSpeakingAboveThreshold( int iSsSlot )
{
	return m_bTalking[ iSsSlot ] && !m_bAboveThresholdTimer[ iSsSlot ].IsElapsed();
}

// Get the (global) voice manager. 
CVoiceStatus* GetClientVoiceMgr();
void ClientVoiceMgr_Init();
void ClientVoiceMgr_Shutdown();
void ClientVoiceMgr_LevelInit();
void ClientVoiceMgr_LevelShutdown();

class CBaseEntity;

using namespace ::ks::reflect;

// Purpose: Client VoteController

class CVoteController : public CBaseEntity, public CGameEventListener
{
	[[= ks::reflect::NetTable{ .name = "DT_VoteController" } ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CVoteController, CBaseEntity );
public:
	DECLARE_CLIENTCLASS();

	CVoteController();
	virtual ~CVoteController();

	virtual void	Spawn( void );
	virtual void	ClientThink( void );

	static void		RecvProxy_VoteType( const CRecvProxyData *pData, void *pStruct, void *pOut );
	static void		RecvProxy_VoteOption( const CRecvProxyData *pData, void *pStruct, void *pOut );

	void			FireGameEvent( IGameEvent *event );
protected:
	void			ResetData();

	[[= ks::reflect::Net{} ]]
	[[= ks::reflect::Proxy<RecvProxy_VoteType, ks::reflect::WireSide::Recv>{} ]] int				m_iActiveIssueIndex;
	[[= ks::reflect::Net{} ]] int				m_iOnlyTeamToVote;
	[[= ks::reflect::Net{} ]]
	[[= ks::reflect::Proxy<RecvProxy_VoteOption, ks::reflect::WireSide::Recv>{} ]] int				m_nVoteOptionCount[MAX_VOTE_OPTIONS];
	int				m_iVoteChoiceIndex;
	[[= ks::reflect::Net{} ]] int				m_nPotentialVotes;
	bool			m_bVotesDirty;	// Received a vote, so remember to tell the Hud
	bool			m_bTypeDirty;	// Vote type changed, so show or hide the Hud
	[[= ks::reflect::Net{} ]] bool			m_bIsYesNoVote;
};

class CEconItemDefinition;
class CEconItemSchema;
class CHudTexture;
class CBaseEntity;


// Purpose: Weapon data file parsing, shared by game & client dlls.

//=============================================================================
// Econ item system stubs (econ system removed)
//=============================================================================

// CEconPersonaDataPublic stub
class CEconPersonaDataPublic
{
public:
	struct Commendation_t {
		int cmd_leader() const { return 0; }
		int cmd_teaching() const { return 0; }
		int cmd_friendly() const { return 0; }
	};
	struct Data_t {
		int player_level() const { return 0; }
		Commendation_t commendation() const { return Commendation_t(); }
	};
	virtual ~CEconPersonaDataPublic() {}
	const Data_t& Obj() const { static Data_t s; return s; }
};

// Item quality enums stub
enum
{
	AE_NORMAL = 0,
	AE_UNIQUE = 1,
};

// Operator enums stub
enum
{
	k_EOperator_String_EQ = 0,
};

// Item generation stub
class CItemSelectionCriteria
{
public:
	void SetQuality( int ) {}
	void BAddCondition( const char*, int, const char*, bool ) {}
};

class CItemGeneration
{
public:
	CBaseEntity* GenerateItemFromScriptData( void*, const Vector&, const QAngle&, const char* ) { return nullptr; }
	CBaseEntity* GenerateRandomItem( void*, const Vector&, const QAngle& ) { return nullptr; }
};
inline CItemGeneration* ItemGeneration() { static CItemGeneration s; return &s; }

// Combined item ID stub
inline uint64 CombinedItemIdMakeFromDefIndexAndPaint( uint16, int ) { return 0; }

// Unacknowledged item reasons stub
enum unacknowledged_item_inventory_positions_t
{
	UNACK_ITEM_DROPPED = 1,
	UNACK_ITEM_CRAFTED,
	UNACK_ITEM_TRADED,
	UNACK_ITEM_PURCHASED,
	UNACK_ITEM_FOUND_IN_CRATE,
	UNACK_ITEM_GIFTED,
	UNACK_ITEM_SUPPORT,
	UNACK_ITEM_PROMOTION,
	UNACK_ITEM_EARNED,
	UNACK_ITEM_REFUNDED,
};

// Sticker attribute IDs stub
enum EStickerAttributeType
{
	k_EStickerAttribute_ID = 0,
	k_EStickerAttribute_Wear,
	k_EStickerAttribute_Scale,
	k_EStickerAttribute_Rotation,
};

// Locchar type
typedef wchar_t locchar_t;

// Get unacknowledged reason stub
inline int GetUnacknowledgedReason( uint32 ) { return 0; }

// Schema attribute handle stub
class CSchemaAttributeDefHandle
{
public:
	CSchemaAttributeDefHandle( const char* ) {}
	operator bool() const { return false; }
	bool operator!() const { return true; }
};

// Attribute definition stub
class CEconItemAttributeDefinition
{
public:
	const char *GetDefinitionName() const { return ""; }
	uint16 GetDefinitionIndex() const { return 0; }
};

// Kill eater attribute pair stubs
inline const CEconItemAttributeDefinition* GetKillEaterAttrPair_Score( int ) { return nullptr; }
inline const CEconItemAttributeDefinition* GetKillEaterAttrPair_Type( int ) { return nullptr; }

// Music definition stub
class CEconMusicDefinition
{
public:
	const char *GetName() const { return ""; }
};

// Forward declaration

// Quest definition stub
class CEconQuestDefinition
{
public:
	uint32 GetQuestID() const { return 0; }
	const char* GetQuestExpression() const { return ""; }
	const char* GetQuestConVars() const { return ""; }
	const char* GetGameMode() const { return ""; }
	const char* GetMapGroup() const { return ""; }
	const char* GetMap() const { return ""; }
	int GetTargetTeam() const { return 0; }
	KeyValues* GetStringTokens() const { return nullptr; }
	const char* GetShortNameLocToken() const { return ""; }
	const std::vector< int >& GetQuestPoints() const { static std::vector< int > empty; return empty; }
};

// Taunt definition stub
class CEconTauntDefinition
{
public:
	const char* GetName() const { return ""; }
	const char* GetSequenceName() const { return ""; }
};

// Paint kit stub
class CPaintKit
{
public:
	const char* GetName() const { return ""; }
	int nID;
};

// Sticker kit stub
class CStickerKit
{
public:
	std::string sMaterialPath;
	std::string sMaterialPathNoDrips;
	int nRarity;
};

// Graffiti tint definition stub
class CEconGraffitiTintDefinition
{
public:
	uint32 GetHexColorRGB() const { return 0xFFFFFF; }
};

// Combined tint ID helper stub
inline uint8 CombinedTintIDGetHSVID( uint32 ) { return 0; }

// Viewmodel hand activity translation stub
inline int TranslateViewmodelHandActivity( int activity ) { return activity; }

// Pro player data stub
class CProPlayerData
{
public:
	const char* GetName() const { return ""; }
};

// GetItemSchema() is defined in cs_shareddefs.cpp
CEconItemSchema *GetItemSchema();
// GEconItemSchema returns the same as GetItemSchema
inline CEconItemSchema& GEconItemSchema() { return *GetItemSchema(); }

// CEconItemView - Wrapper for item data with pointer to real definition
class CEconItemView
{
public:
	CEconItemView() : m_pItemDef( nullptr ), m_nItemDefIndex( 0 ) {}

	// Initialize from a definition name (looks up in schema)
	void Init( const char *pszDefName )
	{
		m_pItemDef = GetItemSchema()->GetItemDefinitionByName( pszDefName );
		if ( m_pItemDef )
			m_nItemDefIndex = m_pItemDef->GetDefinitionIndex();
	}

	// Initialize from a definition index
	void Init( uint16 nDefIndex )
	{
		m_nItemDefIndex = nDefIndex;
		m_pItemDef = GetItemSchema()->GetItemDefinition( nDefIndex );
	}

	// Initialize from an existing definition pointer
	void Init( CEconItemDefinition *pDef )
	{
		m_pItemDef = pDef;
		if ( pDef )
			m_nItemDefIndex = pDef->GetDefinitionIndex();
	}

	uint16 GetItemIndex() const { return m_nItemDefIndex; }
	uint64 GetItemID() const { return m_nItemDefIndex; }
	// Always returns non-NULL (fallback to static default for backwards compatibility with stubs)
	CEconItemDefinition *GetItemDefinition() const;
	CEconItemDefinition *GetStaticData() const;
	const char *GetCustomDesc() const { return nullptr; }
	bool IsValid() const { return m_pItemDef != nullptr; }
	uint32 GetTauntID() const { return 0; }
	const CPaintKit *GetCustomPaintKit() const { return nullptr; }
	int GetCustomPaintKitIndex() const { return 0; }
	int GetRarity() const { return 0; }
	const wchar_t *GetItemName() const { return L""; }
	uint32 GetStickerAttributeBySlotIndexInt( int, int, uint32 defVal ) const { return defVal; }
	const char *GetMagazineModel() const { return nullptr; }
	const char *GetScopeLensMaskModel() const { return nullptr; }
	const char *GetUidModel() const { return nullptr; }
	const char *GetStatTrakModelByType( int ) const { return nullptr; }
	const char *GetCustomName() const { return nullptr; }
	void GetKillEaterTypes( std::flat_multiset<uint32>& ) const {}
	int GetKillEaterValueByType( int ) const { return -1; }
	uint32 GetAccountID() const { return 0; }
	int GetNumSupportedStickerSlots() const { return 0; }
	bool ItemHasAnyStickersApplied() const { return false; }
	IMaterial *GetStickerIMaterialBySlotIndex( int, bool ) const { return nullptr; }
	const char *GetStickerWorldModelBoneParentNameBySlotIndex( int ) const { return nullptr; }
	Vector GetStickerSlotWorldProjectionStartBySlotIndex( int ) const { return vec3_origin; }
	Vector GetStickerSlotWorldProjectionEndBySlotIndex( int ) const { return vec3_origin; }
	uint64 GetFauxItemIDFromDefinitionIndex() const { return m_nItemDefIndex; }
	template<typename T>
	bool FindAttribute( const CSchemaAttributeDefHandle&, T* ) const { return false; }
	template<typename T>
	bool FindAttribute( const CEconItemAttributeDefinition*, T* ) const { return false; }
	// Single-argument FindAttribute (for existence check)
	bool FindAttribute( const CSchemaAttributeDefHandle& ) const { return false; }
	bool FindAttribute( const CEconItemAttributeDefinition* ) const { return false; }
	// Update networked attributes stub
	void UpdateNetworkedDynamicAttributesForDemos( uint16, float ) {}

private:
	CEconItemDefinition *m_pItemDef;
	uint16 m_nItemDefIndex;
};

// Stub for FindAttribute_UnsafeBitwiseCast
template<typename T, typename U, typename V>
inline bool FindAttribute_UnsafeBitwiseCast( U*, const CSchemaAttributeDefHandle&, V* ) { return false; }
template<typename T, typename U, typename V>
inline bool FindAttribute_UnsafeBitwiseCast( U*, const CEconItemAttributeDefinition*, V* ) { return false; }

typedef CEconItemView CEconItemView;

typedef unsigned short WEAPON_FILE_INFO_HANDLE;

// -----------------------------------------------------------
// Weapon sound types
// Used to play sounds defined in the weapon's classname.txt file
// This needs to match pWeaponSoundCategories in weapon_parse.cpp
// ------------------------------------------------------------
enum WeaponSound_t {
	EMPTY,
	SINGLE,
	SINGLE_ACCURATE,
	SINGLE_NPC,
	WPN_DOUBLE, // Can't be "DOUBLE" because windows.h uses it.
	DOUBLE_NPC,
	BURST,
	RELOAD,
	RELOAD_NPC,
	MELEE_MISS,
	MELEE_HIT,
	MELEE_HIT_WORLD,
	SPECIAL1,
	SPECIAL2,
	SPECIAL3,
	TAUNT,
	NEARLYEMPTY,
	FAST_RELOAD,

	// Add new shoot sound types here

	NUM_SHOOT_SOUND_TYPES,
}; 

int GetWeaponSoundFromString( const char *pszString );

inline constexpr auto MAX_SHOOT_SOUNDS = 16; // Maximum number of shoot sounds per shoot type

inline constexpr auto MAX_WEAPON_STRING = 80;
inline constexpr auto MAX_WEAPON_PREFIX = 16;
inline constexpr auto MAX_WEAPON_AMMO_NAME = 32;

inline constexpr const char *WEAPON_PRINTNAME_MISSING = "!!! Missing printname on weapon";


struct WeaponInfoLookup
{
	std::size_t m_nWeaponParseDataOffset;
	_fieldtypes m_fieldType;
	CGameString m_iszAttribClassName;

	WeaponInfoLookup( void ) {}
	WeaponInfoLookup( size_t offset, _fieldtypes p_fieldType, const char* szAttribClassName );
	WeaponInfoLookup( const WeaponInfoLookup &WepInfoLookup );
};

struct CWeaponInfoLookupListLess
{
	bool operator()( const WeaponInfoLookup *src1, const WeaponInfoLookup *src2 ) const
	{
		return src1->m_iszAttribClassName.Get() < src2->m_iszAttribClassName.Get();
	}
};

//-----------------------------------------------------------------------------
// Purpose: Contains the data read from the weapon's script file. 
// It's cached so we only read each weapon's script file once.
// Each game provides a CreateWeaponInfo function so it can have game-specific
// data (like CS move speeds) in the weapon script.
//-----------------------------------------------------------------------------
class FileWeaponInfo_t
{
public:

	FileWeaponInfo_t();
	virtual ~FileWeaponInfo_t() {}
	
	// Each game can override this to get whatever values it wants from the script.
	virtual void Parse( KeyValues *pKeyValuesData, const char *szWeaponName );

	virtual void RefreshDynamicParameters() {};
	
public:	
	bool					bParsedScript;
	bool					bLoadedHudElements;

// SHARED
	char					szClassName[MAX_WEAPON_STRING];
	char					szPrintName[MAX_WEAPON_STRING];			// Name for showing in HUD, etc.

	int GetIndexofAttribute( string_t iszAttribClassName ) const;
	static std::vector< WeaponInfoLookup* > ms_vecWeaponInfoLookup;	// sorted by CWeaponInfoLookupListLess

protected:
	char					szViewModel[MAX_WEAPON_STRING];			// View model of this weapon
	char					szWorldModel[MAX_WEAPON_STRING];		// Model of this weapon seen carried by the player
	char					szAmmo1[MAX_WEAPON_AMMO_NAME];			// "primary" ammo type
	char					szWorldDroppedModel[MAX_WEAPON_STRING];

	static bool				ms_bWeaponInfoLookupInitialized;

public:
	char					szAnimationPrefix[MAX_WEAPON_PREFIX];	// Prefix of the animations that should be used by the player carrying this weapon
	int						iSlot;									// inventory slot.
	int						iPosition;								// position in the inventory slot.
	int						iMaxClip1;								// max primary clip size (-1 if no clip)
	int						iMaxClip2;								// max secondary clip size (-1 if no clip)
	int						iDefaultClip1;							// amount of primary ammo in the gun when it's created
	int						iDefaultClip2;							// amount of secondary ammo in the gun when it's created
	int						iWeight;								// this value used to determine this weapon's importance in autoselection.
	int						iRumbleEffect;							// Which rumble effect to use when fired? (xbox)
	bool					bAutoSwitchTo;							// whether this weapon should be considered for autoswitching to
	bool					bAutoSwitchFrom;						// whether this weapon can be autoswitched away from when picking up another weapon or ammo
	int						iFlags;									// miscellaneous weapon flags

	char					szAmmo2[MAX_WEAPON_AMMO_NAME];			// "secondary" ammo type
	char					szAIAddOn[MAX_WEAPON_STRING];			// addon that this weapon can become

	// Sound blocks
	char					aShootSounds[NUM_SHOOT_SOUND_TYPES][MAX_WEAPON_STRING];	

private:
	int						iAmmoType;
	int						iAmmo2Type;	

public:

	bool					m_bMeleeWeapon;		// Melee weapons can always "fire" regardless of ammo.

	// This tells if the weapon was built right-handed (defaults to true).
	// This helps cl_righthand make the decision about whether to flip the model or not.
	bool					m_bBuiltRightHanded;
	bool					m_bAllowFlipping;	// False to disallow flipping the model, regardless of whether
												// it is built left or right handed.

	virtual int		GetPrimaryClipSize( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const { return 0; }
	virtual int		GetSecondaryClipSize( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const { return 0; }
	virtual int		GetDefaultPrimaryClipSize( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const { return 0; }
	virtual int		GetDefaultSecondaryClipSize( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const{ return 0; }
	virtual int		GetPrimaryReserveAmmoMax( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const{ return 0; }
	virtual int		GetSecondaryReserveAmmoMax( const CEconItemView* pWepView = nullptr, int nAlt = 0, float flScale = 1.0f ) const{ return 0; }

	const char* GetWorldModel( const CEconItemView* pWepView = nullptr, int iTeam = 0 ) const;
	const char* GetViewModel( const CEconItemView* pWepView = nullptr, int iTeam = 0 ) const;
	const char* GetWorldDroppedModel( const CEconItemView* pWepView = nullptr, int iTeam = 0 ) const;
	const char* GetPrimaryAmmo( const CEconItemView* pWepView = nullptr ) const;
	
	int GetPrimaryAmmoType( const CEconItemView* pWepView = nullptr ) const;

// CLIENT DLL
	// Sprite data, read from the data file
	int						iSpriteCount;
	CHudTexture				*iconActive;
	CHudTexture	 			*iconInactive;
	CHudTexture 			*iconAmmo;
	CHudTexture 			*iconAmmo2;
	CHudTexture 			*iconCrosshair;
	CHudTexture 			*iconAutoaim;
	CHudTexture 			*iconZoomedCrosshair;
	CHudTexture 			*iconZoomedAutoaim;
	CHudTexture				*iconSmall;

// TF2 specific
	bool					bShowUsageHint;							// if true, then when you receive the weapon, show a hint about it

// SERVER DLL

};

WEAPON_FILE_INFO_HANDLE LookupWeaponInfoSlot( const char *name );
FileWeaponInfo_t *GetFileWeaponInfoFromHandle( WEAPON_FILE_INFO_HANDLE handle );
WEAPON_FILE_INFO_HANDLE GetInvalidWeaponInfoHandle( void );

void PrecacheFileWeaponInfoDatabase();

// 
// Read a possibly-encrypted KeyValues file in. 
// If pICEKey is NULL, then it appends .txt to the filename and loads it as an unencrypted file.
// If pICEKey is non-NULL, then it appends .ctx to the filename and loads it as an encrypted file.
//
// (This should be moved into a more appropriate place).
//
KeyValues* ReadEncryptedKVFile( IFileSystem *filesystem, const char *szFilenameWithoutExtension, const unsigned char *pICEKey, bool bForceReadEncryptedFile = false );

// Each game implements this. It can return a derived class and override Parse() if it wants.
extern FileWeaponInfo_t* CreateWeaponInfo();

extern void LoadEquipmentData();

class CWeaponDatabase : public CAutoGameSystem, public CGameEventListener
{
public:
	CWeaponDatabase();

	void Reset();
	bool LoadManifest();
	void PrecacheAllWeapons();
	void RefreshAllWeapons();

	WEAPON_FILE_INFO_HANDLE FindWeaponInfo( const char *name );
	FileWeaponInfo_t *GetFileWeaponInfoFromHandle( WEAPON_FILE_INFO_HANDLE handle );

protected:
	friend void LoadEquipmentData();

	virtual bool Init();

	WEAPON_FILE_INFO_HANDLE FindOrCreateWeaponInfo( const char *name );
	bool LoadWeaponDataFromFile( IFileSystem* filesystem, const char *szWeaponName, const unsigned char *pICEKey );

public:
	void FireGameEvent( IGameEvent *event );

private:
	// Handles are dense indices into m_WeaponInfos, held by weapons; the dict maps names to them.
	std::vector< std::unique_ptr< FileWeaponInfo_t > > m_WeaponInfos;
	ks::idict< WEAPON_FILE_INFO_HANDLE > m_WeaponInfoDatabase;
	bool m_bPreCached;
};

extern CWeaponDatabase g_WeaponDatabase;

struct WeaponProficiencyInfo_t
{
	float	spreadscale;
	float	bias;
};

enum WeaponProficiency_t
{
	WEAPON_PROFICIENCY_POOR = 0,
	WEAPON_PROFICIENCY_AVERAGE,
	WEAPON_PROFICIENCY_GOOD,
	WEAPON_PROFICIENCY_VERY_GOOD,
	WEAPON_PROFICIENCY_PERFECT,
};

const char *GetWeaponProficiencyName( WeaponProficiency_t proficiency );

class CHudElement;
class CBaseCombatWeapon;
class CBasePlayer;

// Purpose: Weapon selection handling


extern ConVar hud_fastswitch;

// weapon switch types for Convar hud_fastswitch
inline constexpr auto HUDTYPE_BUCKETS = 0; // PC buckets
inline constexpr auto HUDTYPE_FASTSWITCH = 1; // PC fastswitch
inline constexpr auto HUDTYPE_PLUS = 2; // console buckets
inline constexpr auto HUDTYPE_CAROUSEL = 3; // console carousel scroll

//-----------------------------------------------------------------------------
// Purpose: Base class for tf2 & hl2 weapon selection hud elements
//-----------------------------------------------------------------------------
class CBaseHudWeaponSelection : public CHudElement
{
	DECLARE_CLASS( CBaseHudWeaponSelection, CHudElement );

public:
	CBaseHudWeaponSelection( const char *pElementName );
	virtual void Init( void );
	virtual void VidInit( void );
	virtual void ProcessInput();
	virtual void Reset(void);
	virtual void OnThink(void);

	virtual void OpenSelection( void );
	virtual void HideSelection( void );

	virtual void				CancelWeaponSelection( void );

	// Game specific overrides
	virtual void CycleToNextWeapon( void ) = 0;
	virtual void CycleToPrevWeapon( void ) = 0;
	virtual void SwitchToLastWeapon( void );
	virtual void CycleToNextGrenadeOrBomb( void ) = 0;
	virtual void CycleToNextGrenadeBombOrMelee( void ) = 0;
	virtual void CycleToNextNonGrenadeOrBomb( void ) = 0;
	virtual CBaseCombatWeapon *GetWeaponInSlot( int iSlot, int iSlotPos ) = 0;
	virtual void SelectWeaponSlot( int iSlot ) = 0;
	virtual CBaseCombatWeapon	*GetFirstPos( int iSlot );
	virtual CBaseCombatWeapon	*GetNextActivePos( int iSlot, int iSlotPos );
	virtual void				SetWeaponSelected( void );
	virtual void				SelectWeapon( void );

	virtual CBaseCombatWeapon	*GetSelectedWeapon( void ) = 0;

	virtual void OnWeaponPickup( CBaseCombatWeapon *pWeapon );
	virtual void OnWeaponDrop( CBaseCombatWeapon *pWeapon ) {}
	virtual void OnWeaponSwitch( CBaseCombatWeapon *pWeapon ) {}
	virtual bool IsInSelectionMode();

	void UserCmd_Slot1( void );
	void UserCmd_Slot2( void );
	virtual void UserCmd_Slot3( void );
	virtual void UserCmd_Slot4( void );
	virtual void UserCmd_Slot5( void );
	virtual void UserCmd_Slot6( void );
	virtual void UserCmd_Slot7( void );
	virtual void UserCmd_Slot8( void );
	virtual void UserCmd_Slot9( void );
	void UserCmd_Slot0( void );
	virtual void UserCmd_Slot10( void );
	virtual void UserCmd_Slot11( void );
	void UserCmd_Close( void );
	void UserCmd_NextWeapon( void );
	void UserCmd_PrevWeapon( void );
	void UserCmd_LastWeapon( void );
	void UserCmd_DropPrimary( void );
	void UserCmd_NextGrenadeWeapon( void );
	void UserCmd_NextItemWeapon( void );
	void UserCmd_NextNonGrenadeWeapon( void );
	virtual void UserCmd_GamePadSlot1( void )=0;
	virtual void UserCmd_GamePadSlot2( void )=0;
	virtual void UserCmd_GamePadSlot3( void )=0;
	virtual void UserCmd_GamePadSlot4( void )=0;
	virtual void UserCmd_GamePadSlot5( void )=0;
	virtual void UserCmd_GamePadSlot6( void )=0;

	virtual void		SelectSlot( int iSlot );

	virtual bool IsHudMenuTakingInput();
	virtual bool IsHudMenuPreventingWeaponSelection();

	bool HandleHudMenuInput( int iSlot );

	static CBaseHudWeaponSelection *GetInstance();

	// these functions are exposed as virtual so that the tf_hints system can redraw the weapon selection
	virtual void DrawWList( CBasePlayer *pPlayer, CBaseCombatWeapon *pSelectedWeapon, bool drawOutline = false, int ora = 0, int og = 0, int ob = 0, int oa = 0 ) {}
	virtual bool ComputeRect( CBasePlayer *pPlayer, CBaseCombatWeapon *pSelectedWeapon, wrect_t *outrect ) { return false; }
	
	virtual int	KeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding );

protected:
	// returns true if there is a weapon currently visible to select
	virtual bool IsWeaponSelectable()	{ return IsInSelectionMode(); }

	bool	CanBeSelectedInHUD( CBaseCombatWeapon *pWeapon );

	void	UpdateSelectionTime( void );

	float	m_flSelectionTime;	// most recent time at which weapon selection had input

	static CBaseHudWeaponSelection *s_pInstance[MAX_SPLITSCREEN_PLAYERS];

	bool	m_bSelectionVisible;

	CHandle< CBaseCombatWeapon >	m_hSelectedWeapon;
};

// accessor
CBaseHudWeaponSelection *GetHudWeaponSelection();

class CHudTexture;
class FileWeaponInfo_t;



typedef unsigned short WEAPON_FILE_INFO_HANDLE;

//-----------------------------------------------------------------------------
// Purpose: Stores data about the Weapon Definitions passed to the client when
//			the client first connects to a server. 
//-----------------------------------------------------------------------------
class WeaponsResource
{
public:
	WeaponsResource( void );
	~WeaponsResource( void );

	void Init( void );
	void Reset( void );

	// Sprite handling
	void LoadWeaponSprites( WEAPON_FILE_INFO_HANDLE hWeaponFileInfo );
	void LoadAllWeaponSprites( void );

	// Ammo Handling
	CHudTexture					*GetAmmoIconFromWeapon( int iAmmoId );
	const FileWeaponInfo_t		*GetWeaponFromAmmo( int iAmmoId );
};

extern WeaponsResource gWR;

class CBaseEntity;

using namespace ::ks::reflect;

class CWorld : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_World" } ]]
	[[= ks::reflect::From<"m_flWaveHeight", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_iszDetailSpriteMaterial", ks::reflect::Net{}>{} ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CWorld, CBaseEntity );
	DECLARE_CLIENTCLASS();

	CWorld( void );
	~CWorld( void );
	
	// Override the factory create/delete functions since the world is a singleton.
	virtual bool Init( int entnum, int iSerialNum );
	virtual void Release();

	virtual void Precache();
	virtual void Spawn();

	// Don't worry about adding the world to the collision list; it's already there
	virtual CollideType_t	GetCollideType( void )	{ return ENTITY_SHOULD_NOT_COLLIDE; }

	virtual void OnDataChanged( DataUpdateType_t updateType );
	virtual void PreDataUpdate( DataUpdateType_t updateType );

	float GetWaveHeight() const;
	const char *GetDetailSpriteMaterial() const;

public:
	enum
	{
		MAX_DETAIL_SPRITE_MATERIAL_NAME_LENGTH = 256,
	};

	float	m_flWaveHeight;
	[[= ks::reflect::Net{} ]] Vector	m_WorldMins;
	[[= ks::reflect::Net{} ]] Vector	m_WorldMaxs;
	[[= ks::reflect::Net{} ]] bool	m_bStartDark;
	[[= ks::reflect::Net{} ]] float	m_flMaxOccludeeArea;
	[[= ks::reflect::Net{} ]] float	m_flMinOccluderArea;
	[[= ks::reflect::Net{} ]] float	m_flMinPropScreenSpaceWidth;
	[[= ks::reflect::Net{} ]] float	m_flMaxPropScreenSpaceWidth;
	[[= ks::reflect::Net{} ]] bool	m_bColdWorld;
	[[= ks::reflect::Net{} ]] int		m_iTimeOfDay;

private:
	char	m_iszDetailSpriteMaterial[MAX_DETAIL_SPRITE_MATERIAL_NAME_LENGTH];

};

void ClientWorldFactoryInit();
void ClientWorldFactoryShutdown();
CWorld* GetClientWorldEntity();
