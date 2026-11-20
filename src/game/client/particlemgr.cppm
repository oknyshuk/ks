// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2013 Valve Corporation
// Copyright (c) Valve Corporation

module;
#include <stdio.h>
#include <wchar.h>
#include <stddef.h>
#include "basehandle.h"
#include "dt_recv.h"
#include "networkvar.h"
#include "reflect_annotations.h"
#include "string_t.h"
#include "igameevents.h"
#include "utils/common/filesystem_tools.h"
#include "datamap.h"
#include "declare_class.h"
#include "iachievementmgr.h"
#include "matchmaking/iplayer.h"
#include "baseachievement.h"
#include "proto_types.h"
#include "user_message_register.h"
#include "engine/ivmodelinfo.h"
#include "usermessages_schema.h"
#include "engine/ICollideable.h"
#include "icliententity.h"
#include "icliententitylist.h"
#include "cliententitylist.h"
#include "engine/IClientLeafSystem.h"
#include "iclientrenderable.h"
#include "ivrenderview.h"
#include "cmodel.h"
#include "dt_send.h"
#include "vphysics_interface.h"
#include "predictable_entity.h"
#include "irendertorthelperobject.h"
#include "materialsystem/itexture.h"
#include "materialsystem/icustommaterial.h"
#include "materialsystem/imaterialsystem.h"
#include "mathlib/camera.h"
#include "effect_color_tables.h"
#include "client_class.h"
#include "hud_element_helper.h"
#include "hud_macros.h"
#include "materialsystem/imaterial.h"
#include "view_shared.h"
#include "engine/IEngineTrace.h"
#include "studio.h"
#include "inputsystem/ButtonCode.h"
#include "datacache/imdlcache.h"
#include "irecipientfilter.h"
#include "particles/particles.h"
#include "particlemgr.h"
#include "globalvars_base.h"
#include "iprediction.h"
#include "prediction_private.h"
#include "predictioncopy.h"
#include "materialsystem/imesh.h"
module ks.client:particlemgr;

import ks.tier1.createinterface;
import ks.platform;
import ks.simd;
import ks.core.basetypes;
import ks.math.vector;
import std;
import ks.core.types;
import ks.tier1.keyvalues;
import ks.tier1.strtools;
import ks.math.ssequaternion;
import ks.game.shareddefs;
import ks.core.wchartypes;
import ks.math.compressed_vector;
import ks.tier1.bitbuf;
import ks.tier1.checksum_crc;
import ks.core.platform;
import ks.core.threadtools;
import ks.tier1.convar;
import ks.tier1.reflect_wirevar;
import ks.core.tslist;
import ks.tier1.slot_pool;
import ks.math.ssemath;
import ks.math.vmatrix;
import ks.math.volumeculler;
import ks.tier1.fixed_pool;
import ks.tier1.refcount;
import ks.tier1.small_vector;
import ks.math.mathlib;
import ks.core.mem;
import ks.math.vector4d;
import ks.math.vplane;
import ks.core.memalloc;
import ks.core.color;
import ks.tier1.exprevaluator;
import ks.tier1.map_ops;
import ks.game.tempentity;
import ks.tier1.interfaces;
import ks.tier1.intrusive_list;
import ks.vstdlib.jobthread;
import ks.tier1.intrusive_ptr;
import ks.tier1.tracked_ref;
import ks.game.igamemovement;

class CEntInfo
{
public:
	IHandleEntity	*m_pEntity;
	int				m_SerialNumber;
	CEntInfo		*m_pPrev;
	CEntInfo		*m_pNext;

	void			ClearLinks();
};

class CBaseEntityList
{
public:
	CBaseEntityList();
	~CBaseEntityList();
	
	// Add and remove entities. iForcedSerialNum should only be used on the client. The server
	// gets to dictate what the networkable serial numbers are on the client so it can send
	// ehandles over and they work.
	CBaseHandle AddNetworkableEntity( IHandleEntity *pEnt, int index, int iForcedSerialNum = -1 );
	CBaseHandle AddNonNetworkableEntity( IHandleEntity *pEnt );
	void RemoveEntity( CBaseHandle handle );

	// Get an ehandle from a networkable entity's index (note: if there is no entity in that slot,
	// then the ehandle will be invalid and produce NULL).
	CBaseHandle GetNetworkableHandle( int iEntity ) const;

	// ehandles use this in their Get() function to produce a pointer to the entity.
	IHandleEntity* LookupEntity( const CBaseHandle &handle ) const;
	IHandleEntity* LookupEntityByNetworkIndex( int edictIndex ) const;

	// Use these to iterate over all the entities.
	CBaseHandle FirstHandle() const;
	CBaseHandle NextHandle( CBaseHandle hEnt ) const;
	static CBaseHandle InvalidHandle();

	const CEntInfo *FirstEntInfo() const;
	const CEntInfo *NextEntInfo( const CEntInfo *pInfo ) const;
	const CEntInfo *GetEntInfoPtr( const CBaseHandle &hEnt ) const;
	const CEntInfo *GetEntInfoPtrByIndex( int index ) const;

	// Used by Foundry when an entity is respawned/edited.
	// We force the new entity's ehandle to be the same so anyone pointing at it still gets a valid CBaseEntity out of their ehandle.
	void ForceEntSerialNumber( int iEntIndex, int iSerialNumber );

// Overridables.
protected:

	// These are notifications to the derived class. It can cache info here if it wants.
	virtual void OnAddEntity( IHandleEntity *pEnt, CBaseHandle handle );
	
	// It is safe to delete the entity here. We won't be accessing the pointer after
	// calling OnRemoveEntity.
	virtual void OnRemoveEntity( IHandleEntity *pEnt, CBaseHandle handle );

private:

	CBaseHandle AddEntityAtSlot( IHandleEntity *pEnt, int iSlot, int iForcedSerialNum );
	void RemoveEntityAtSlot( int iSlot );

private:
	
	class CEntInfoList
	{
	public:
		CEntInfoList();

		const CEntInfo	*Head() const { return m_pHead; }
		const CEntInfo	*Tail() const { return m_pTail; }
		CEntInfo		*Head() { return m_pHead; }
		CEntInfo		*Tail() { return m_pTail; }
		void			AddToHead( CEntInfo *pElement ) { LinkAfter( nullptr, pElement ); }
		void			AddToTail( CEntInfo *pElement ) { LinkBefore( nullptr, pElement ); }

		void LinkBefore( CEntInfo *pBefore, CEntInfo *pElement );
		void LinkAfter( CEntInfo *pBefore, CEntInfo *pElement );
		void Unlink( CEntInfo *pElement );
		bool IsInList( CEntInfo *pElement );
	
	private:
		CEntInfo		*m_pHead;
		CEntInfo		*m_pTail;
	};

	int GetEntInfoIndex( const CEntInfo *pEntInfo ) const;

	// The first MAX_EDICTS entities are networkable. The rest are client-only or server-only.
	CEntInfo m_EntPtrArray[NUM_ENT_ENTRIES];
	CEntInfoList	m_activeList;
	CEntInfoList	m_freeNonNetworkableList;
};

// ------------------------------------------------------------------------------------ //
// Inlines.
// ------------------------------------------------------------------------------------ //

	

extern CBaseEntityList *g_pEntityList;

//-----------------------------------------------------------------------------
// For networking this bad boy
//-----------------------------------------------------------------------------
EXTERN_RECV_TABLE( DT_EntityParticleTrailInfo );

//-----------------------------------------------------------------------------
// Particle trail info
//-----------------------------------------------------------------------------
struct EntityParticleTrailInfo_t
{
	[[= ks::reflect::NetTable{ .name = "DT_EntityParticleTrailInfo", .base = false } ]]
	[[= ks::reflect::From<"m_flLifetime", ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE }>{} ]]
	[[= ks::reflect::From<"m_flStartSize", ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE }>{} ]]
	[[= ks::reflect::From<"m_flEndSize", ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE }>{} ]]
	static constexpr bool ks_class = true;

	EntityParticleTrailInfo_t();

	DECLARE_CLASS_NOBASE( EntityParticleTrailInfo_t );
	DECLARE_SIMPLE_DATADESC();
	DECLARE_EMBEDDED_NETWORKVAR();

	[[= ks::reflect::Key{ .name = "ParticleTrailMaterial" } ]] string_t m_strMaterialName;
	CNetworkVar( float, m_flLifetime, [[= ks::reflect::Key{ .name = "ParticleTrailLifetime" } ]] );
	CNetworkVar( float, m_flStartSize, [[= ks::reflect::Key{ .name = "ParticleTrailStartSize" } ]] );
	CNetworkVar( float, m_flEndSize, [[= ks::reflect::Key{ .name = "ParticleTrailEndSize" } ]] );
};

class CBaseEntity;

// -------------------------------------------------------------------------------------------------- //
// Game-code CBaseHandle implementation.
// -------------------------------------------------------------------------------------------------- //

extern CBaseEntityList *g_pEntityList;

inline IHandleEntity* EntityFromHandle( const CBaseHandle &handle )
{
	return g_pEntityList->LookupEntity( handle );
}

inline bool operator ==( const CBaseHandle &handle, const IHandleEntity *pEnt )
{
	return EntityFromHandle( handle ) == pEnt;
}

inline bool operator !=( const CBaseHandle &handle, const IHandleEntity *pEnt )
{
	return EntityFromHandle( handle ) != pEnt;
}

// -------------------------------------------------------------------------------------------------- //
// CHandle.
//
// Only safe to use in cases where you can statically verify that T* can safely be reinterpret-casted
// to IHandleEntity*; that is, that it's derived from IHandleEntity and IHandleEntity is the
// first base class.
//
// Unfortunately some classes are forward-declared and the compiler can't determine at compile time
// how to static_cast<> them to IHandleEntity.
// -------------------------------------------------------------------------------------------------- //
template< class T >
class CHandle : public CBaseHandle
{
public:

			CHandle();
			CHandle( int iEntry, int iSerialNumber );
	/*implicit*/ CHandle( T *pVal );
	/*implicit*/ CHandle( INVALID_EHANDLE_tag );
	// Declared, not implicit: GCC 16 crashes declaring the implicit members of a specialisation
	// that another partition instantiated.
	CHandle( const CHandle & ) = default;
	CHandle &operator=( const CHandle & ) = default;
	~CHandle() = default;

	// NOTE: The following two constructor functions are not type-safe, and can allow creating a
	// CHandle<T> that doesn't actually point to an object of type T.
	//
	// It is your responsibility to ensure that the target of the handle actually points to the
	// correct type of object before calling these functions.

	static CHandle<T> UnsafeFromBaseHandle( const CBaseHandle& handle );

	// The index should have come from a call to CBaseHandle::ToInt(). If it hasn't, you're in trouble.
	static CHandle<T> UnsafeFromIndex( int index );

	T*		Get() const;
	void	Set( const T* pVal )
	{
		CBaseHandle::Set( reinterpret_cast<const IHandleEntity*>( pVal ) );
	}

	/*implicit*/ operator T*();
	/*implicit*/ operator T*() const;

	bool	operator !() const;
	bool	operator==( T *val ) const;
	bool	operator!=( T *val ) const;
	CHandle& operator=( const T *val )
	{
		Set( val );
		return *this;
	}

	T*		operator->() const;
};

// ----------------------------------------------------------------------- //
// Inlines.
// ----------------------------------------------------------------------- //

template<class T>
inline CHandle<T>::CHandle()
{
}

template<class T>
inline CHandle<T>::CHandle( INVALID_EHANDLE_tag )
	: CBaseHandle( INVALID_EHANDLE )
{
}

template<class T>
inline CHandle<T>::CHandle( int iEntry, int iSerialNumber )
{
	Init( iEntry, iSerialNumber );
}

template<class T>
inline CHandle<T>::CHandle( T *pObj )
	: CBaseHandle( INVALID_EHANDLE )
{
	Set( pObj );
}

template<class T>
inline CHandle<T> CHandle<T>::UnsafeFromBaseHandle( const CBaseHandle &handle )
{
	return UnsafeFromIndex( handle.ToInt() );
}

template<class T>
inline CHandle<T> CHandle<T>::UnsafeFromIndex( int index )
{
	CHandle<T> ret;
	ret.m_Index = index;
	return ret;
}

template<class T>
inline T* CHandle<T>::Get() const
{
	return (T*)EntityFromHandle( *this );
}

template<class T>
inline CHandle<T>::operator T *() 
{ 
	return Get( ); 
}

template<class T>
inline CHandle<T>::operator T *() const
{ 
	return Get( ); 
}

template<class T>
inline bool CHandle<T>::operator !() const
{
	return !Get();
}

template<class T>
inline bool CHandle<T>::operator==( T *val ) const
{
	return Get() == val;
}

template<class T>
inline bool CHandle<T>::operator!=( T *val ) const
{
	return Get() != val;
}

template<class T>
T* CHandle<T>::operator -> () const
{
	return Get();
}

// specialization of EnsureValidValue for CHandle<T>
template<typename T>
[[gnu::always_inline]] inline void EnsureValidValue( CHandle<T> &x ) { x = INVALID_EHANDLE; }

// One instantiation for the module: imported partitions then share it instead of each merging its own.
template class CHandle<CBaseEntity>;

extern IGameEventManager2 *gameeventmanager;

// A safer method than registering with the event manager directly.
// Avoids requiring the user to remove themselves as listeners in 
// their deconstructor, and sets the serverside variable based on
// our dll location.
//
// Events are delivered to the FireGameEvent( IGameEvent * ) member of the type that
// ListenForGameEvent() is called on. A class that registers through a base class (so the
// call is made on the base type) must call SetGameEventHandler( this ) in its constructor
// before anything registers.
class CGameEventListener
{
public:
	CGameEventListener() : m_bRegisteredForEvents(false)
	{
	}

	~CGameEventListener()
	{
		StopListeningForAllEvents();
	}

	// Bind T::FireGameEvent as the function the event manager calls for this listener.
	template< class T >
	void SetGameEventHandler( T *self )
	{
		m_Handler.emplace( std::cw<&T::FireGameEvent>, self );
	}

	void ListenForGameEvent( this auto &self, const char *name )
	{
		CGameEventListener &listener = self;

		if ( !listener.m_Handler )
			listener.SetGameEventHandler( &self );

		listener.m_bRegisteredForEvents = true;

		bool bServerSide = false;

		gameeventmanager->AddListener( &listener, *listener.m_Handler, name, bServerSide );
	}

	void ListenForAllGameEvents( this auto &self )
	{
		CGameEventListener &listener = self;

		if ( !listener.m_Handler )
			listener.SetGameEventHandler( &self );

	bool bServerSide = false;

		gameeventmanager->AddListenerGlobal( &listener, *listener.m_Handler, bServerSide );
	}

	void StopListeningForAllEvents()
	{
		// remove me from list
		if ( m_bRegisteredForEvents )
		{
			if ( gameeventmanager )
				gameeventmanager->RemoveListener( this );

			m_bRegisteredForEvents = false;
		}
	}

private:

	// What the event manager calls
	std::optional< GameEventCallback_t > m_Handler;

	// Have we registered for any events?
	bool m_bRegisteredForEvents;
};

const int GAMESTATS_VERSION = 1;

enum StatSendType_t
{
	STATSEND_LEVELSHUTDOWN,
	STATSEND_APPSHUTDOWN
};

struct StatsBufferRecord_t
{
	float m_flFrameRate;									// fps
	float m_flServerPing;									// client ping to server
	float m_flMainThreadTimeMS;								// time in ms taken by the main thread
	float m_flMainThreadWaitTimeMS;							// time in ms the main thread is waiting for the render thread (cf EndFrame)
	float m_flRenderThreadTimeMS;							// time in ms taken by the render thread
	float m_flRenderThreadWaitTimeMS;						// time in ms waiting for the gpu (time spent in ForceHardwareSync)

	bool operator< ( const StatsBufferRecord_t &a) const
	{
		return m_flFrameRate < a.m_flFrameRate;
	}
};

class CFpsHistory
{
public:
	enum FpsEnum_t 
	{
		FPS_120,
		FPS_90,
		FPS_60,
		FPS_45,
		FPS_30,
		FPS_20,
		FPS_10,
		FPS_LOW,
		FPS_BIN_COUNT
	};

	enum HistoryBufSizeEnum_t { HISTORY_BUF_SIZE = 32 };
	struct Rec_t
	{
		float m_flFps;
		float m_flCpuWait;
	};

	CFpsHistory()
	{
		V_memset( this, 0, sizeof( *this ) );
	}

	uint8 Update( float flFps, float flCpuWait );

	uint8 operator []( int i ) const 
	{
		return m_History[ ( m_nNext + HISTORY_BUF_SIZE - 1 - i ) % HISTORY_BUF_SIZE ];
	}

	int m_nNext;
	uint8 m_History[ HISTORY_BUF_SIZE ];

};

class CFpsHistogram
{
public:
	enum FpsEnum_t
	{
		FPS_BIN_COUNT = CFpsHistory::FPS_BIN_COUNT
	};

	CFpsHistogram()
	{
		Reset();
	}
	void Reset()
	{
		V_memset( this, 0, sizeof( *this ) );
	}
	void Update( uint8 nBin );
	int64 Encode()const;
protected:
	uint32 m_nFps[ FPS_BIN_COUNT ];
	uint32 m_nCpuWaits[ FPS_BIN_COUNT ];
};

class CFpsSelectiveHistogram: public CFpsHistogram
{
public:
	CFpsSelectiveHistogram();

	void Reset()
	{
		V_memset( this, 0, sizeof( *this ) );
	}

	void Fire( int nLookAhead, int nLookBack, const CFpsHistory &history );

	void Update( uint8 nBin );
	int GetInactiveCount() const { return m_nLookBack; }
	void ResetInactiveCount() { m_nLookBack = 0; }

protected:
	int m_nLookAhead; // this is how many more frames we need to collect statistics (stay active)
	int m_nLookBack; // this is how many frames we skipped (did not collect statistics)
};

enum PerfStatsEventEnum_t
{
	PERF_STATS_SMOKE,
	PERF_STATS_BULLET,
	PERF_STATS_PLAYER,
	PERF_STATS_PLAYER_SPAWN,
	PERF_STATS_EVENT_COUNT
};

inline constexpr auto STATS_WINDOW_SIZE = ( 60 * 10 ); // # of records to hold
inline constexpr auto STATS_TRAILING_WINDOW_SIZE = ( 30 ); // # of records to hold in trailing window prior to stat recording
inline constexpr auto STATS_RECORD_INTERVAL = 1; // # of seconds between data grabs. 2 * 300 = every 10 minutes

class CGameStats;

void UpdatePerfStats( void );
void FirePerfStatsEvent( PerfStatsEventEnum_t nEvent, int nLookAhead = CFpsHistory::HISTORY_BUF_SIZE, int nLookBack = CFpsHistory::HISTORY_BUF_SIZE );
void SetGameStatsHandler( CGameStats *pGameStats );


class CBaseGameStats 
{
public:
	CBaseGameStats();

	// override this to declare what format you want to send.  New products should use new format.
	virtual bool UseOldFormat() 
	{ 
		return false;		// clients never used old format so no backward compat issues, they use new format by default
	}

	// Implement this if you support new format gamestats.
	// Return true if you added data to KeyValues, false if you have no data to report
	virtual bool AddDataForSend( KeyValues *pKV, StatSendType_t sendType ) { return false; }

	// These methods used for new format gamestats only and control when data gets sent.
	virtual bool ShouldSendDataOnLevelShutdown()
	{
		// by default, servers send data at every level change and clients don't
		return false;
	}
	virtual bool ShouldSendDataOnAppShutdown();

	virtual void Event_Init( void );
	virtual void Event_Shutdown( void );
	virtual void Event_MapChange( const char *szOldMapName, const char *szNewMapName );
	virtual void Event_LevelInit( void );
	virtual void Event_LevelShutdown( float flElapsed );
	virtual void Event_SaveGame( void );
	virtual void Event_LoadGame( void );

	void StatsLog( char const *fmt, ... );
	
	// This is the first call made, so that we can "subclass" the CBaseGameStats based on gamedir as needed (e.g., ep2 vs. episodic)
	virtual CBaseGameStats *OnInit( CBaseGameStats *pCurrentGameStats, char const *gamedir ) { return pCurrentGameStats; }

	// Frees up data from gamestats and resets it to a clean state.
	virtual void Clear( void );

	virtual bool StatTrackingEnabledForMod( void ) { return false; } //Override this to turn on the system. Stat tracking is disabled by default and will always be disabled at the user's request
	static bool StatTrackingAllowed( void ); //query whether stat tracking is possible and warranted by the user
	virtual bool HaveValidData( void ) { return true; } // whether we currently have an interesting enough data set to upload.  Called at upload time; if false, data is not uploaded.

	virtual bool ShouldTrackStandardStats( void ) { return true; } //exactly what was tracked for EP1 release
	
	//Get mod specific strings used for tracking, defaults should work fine for most cases
	virtual const char *GetStatSaveFileName( void );
	virtual const char *GetStatUploadRegistryKeyName( void );
	const char *GetUserPseudoUniqueID( void );

	virtual bool UserPlayedAllTheMaps( void ) { return false; } //be sure to override this to determine user completion time

public:
	bool			m_bLogging : 1;
	bool			m_bLoggingToFile : 1;
};

extern CBaseGameStats *gamestats; //starts out pointing at a singleton of the class above, overriding this in any constructor should work for replacing it

class CAchievementMgr;
class CBaseEntity;


inline constexpr const char *AWARD_ID_NONE = "";


//
// Base class for achievements
//

class CBaseAchievement : public CGameEventListener, public IAchievement
{
	DECLARE_CLASS_NOBASE( CBaseAchievement );
public:
	CBaseAchievement();	
	virtual void Init() {}
	virtual void ListenForEvents() {};
	virtual void Event_EntityKilled( CBaseEntity *pVictim, CBaseEntity *pAttacker, CBaseEntity *pInflictor, IGameEvent *event );

	int GetAchievementID() { return m_iAchievementID; }
	void SetAchievementID( int iAchievementID ) { m_iAchievementID = iAchievementID; }
	void SetName( const char *pszName ) { m_pszName = pszName; }
	const char *GetName() { return m_pszName; }
	void SetFlags( int iFlags );
	int GetFlags() { return m_iFlags; }
	void SetGoal( int iGoal ) { m_iGoal = iGoal; }
	int GetGoal() { return m_iGoal; }
	void SetGameDirFilter( const char *pGameDir );
	bool HasComponents() { return ( m_iFlags & ACH_HAS_COMPONENTS ) > 0; }	
	void SetPointValue( int iPointValue ) { m_iPointValue = iPointValue; }
	int	GetPointValue() { return m_iPointValue; }
	bool ShouldHideUntilAchieved() { return m_bHideUntilAchieved; }

	void SetHideUntilAchieved( bool bHide ) { m_bHideUntilAchieved = bHide; }
	virtual bool ShouldShowProgressNotification() { return true; }
	virtual void OnPlayerStatsUpdate( int nUserSlot ) {}

	virtual bool ShouldSaveWithGame();
	bool ShouldSaveGlobal();
	virtual void PreRestoreSavedGame();
	virtual void PostRestoreSavedGame();
	void SetCount( int iCount ) { m_iCount = iCount; }
	int GetCount() { return m_iCount; }
	void SetProgressShown( int iProgressShown ) { m_iProgressShown = iProgressShown; }
	int GetProgressShown() { return m_iProgressShown; }
	virtual bool IsAchieved() { return m_bAchieved; }
	virtual bool IsActive();
	virtual bool IsAvailable(); // Is this achievement available?  Might need DLC, etc
	virtual bool LocalPlayerCanEarn( void ) { return true; }
	void SetAchieved( bool bAchieved ) { m_bAchieved = bAchieved; }
	virtual void CheckAssetAwards( int nSlotId ) {}
	virtual bool IsMetaAchievement() { return false; }

	virtual void OnAchieved( void ) {}
	uint32 GetUnlockTime( void ) const { return m_uUnlockTime; }
	void SetUnlockTime( uint32 unlockTime ) { m_uUnlockTime = unlockTime; }

	uint64 GetComponentBits() { return m_iComponentBits; }
	virtual int GetNumComponents() { return m_iNumComponents; }
	virtual const char *GetComponentDisplayString( int iComponent );
	void SetComponentBits( uint64 iComponentBits );
	int GetNumComponentBitsSet( void );
	void OnComponentEvent( const char *pchComponentName );
	void EnsureComponentBitSetAndEvaluate( int iBitNumber );
	void EvaluateIsAlreadyAchieved();
	virtual void OnMapEvent( const char *pEventName );
	virtual void PrintAdditionalStatus() {}		// for debugging, achievements may report additional status in achievement_status concmd
	virtual void OnAchievementAwarded() {}
	virtual void UpdateAchievement( int nData ) {}
	virtual bool ShouldShowOnHUD() { return m_bShowOnHUD; }
	virtual void SetShowOnHUD( bool bShow );
	virtual void SetUserSlot( int nUserSlot ) { m_nUserSlot = nUserSlot; }
	virtual void ClearAchievementData();
	virtual const char *GetIconPath() { return nullptr; }
	void SetDisplayOrder( int iDisplayOrder ) { m_iDisplayOrder = iDisplayOrder; }
	int GetDisplayOrder( ) { return m_iDisplayOrder; }

	virtual void ReadProgress( IPlayerLocal *pPlayer ) {}
	virtual bool WriteProgress( IPlayerLocal *pPlayer ) { return false; }

	virtual void GetSettings( KeyValues* pNodeOut );				// serialize
	virtual void ApplySettings( /* const */ KeyValues* pNodeIn );	// unserialize

	virtual void Think( void ) { return; }
	
	// XBox Asset Awards
	void SetAssetAward( const char* assetAwardName );
	void SetAssetAwardID( int iAssetAwardID ) { m_iAssetAwardID = iAssetAwardID; }
	int GetAssetAwardID( void ) { return m_iAssetAwardID; }
	bool IsAssetAward( void ) { return m_iAssetAwardID > 0; }

	virtual bool CheckAchievementsEnabled( void );

protected:
public:
	void FireGameEvent( IGameEvent *event );
protected:
	virtual void FireGameEvent_Internal( IGameEvent *event ) {};
	void SetVictimFilter( const char *pClassName );
	void SetAttackerFilter( const char *pClassName );
	void SetInflictorFilter( const char *pClassName );
	void SetInflictorEntityNameFilter( const char *pEntityName );
	void SetMapNameFilter( const char *pMapName );
	void SetComponentPrefix( const char *pPrefix );
	void IncrementCount( int iOptIncrement = 0 );
	void EvaluateNewAchievement();
	void AwardAchievement();
	void ShowProgressNotification();
	void HandleProgressUpdate();
	virtual void CalcProgressMsgIncrement();
	void SetNextThink( float flThinkTime );
	void ClearThink( void );

	const char *m_pszName;								// name of this achievement
	int m_iAchievementID;								// ID of this achievement
	int	m_iFlags;										// ACH_* flags for this achievement
	int	m_iGoal;										// goal # of steps to award this achievement
	int m_iProgressMsgIncrement;						// after how many steps show we show a progress notification
	int m_iProgressMsgMinimum;							// the minimum progress needed before showing progress notification
	int m_iPointValue;									// # of points this achievement is worth (currently only used for XBox Live)
	bool m_bHideUntilAchieved;							// should this achievement be hidden until achieved?
	const char *m_pInflictorClassNameFilter;			// if non-NULL, inflictor class name to filter with
	const char *m_pInflictorEntityNameFilter;			// if non-NULL, inflictor entity name to filter with
	const char *m_pVictimClassNameFilter;				// if non-NULL, victim class name to filter with
	const char *m_pAttackerClassNameFilter;				// if non-NULL, attacker class name to filter with
	const char *m_pMapNameFilter;						// if non-NULL, map name to filter with
	const char *m_pGameDirFilter;						// if non-NULL, game dir name to filter with

	const char **m_pszComponentNames;			
	const char **m_pszComponentDisplayNames;			// localizable strings for each component
	int			m_iNumComponents;
	const char *m_pszComponentPrefix;
	int			m_iComponentPrefixLen;
	bool		m_bAchieved;							// is this achievement achieved
	uint32		m_uUnlockTime;							// time_t that this achievement was unlocked (0 if unknown)
	int			m_iCount;								// # of steps satisfied toward this achievement (only valid if not achieved)
	int			m_iProgressShown;						// # of progress msgs we've shown
	uint64		m_iComponentBits;						// bitfield of components achieved
	CAchievementMgr *m_pAchievementMgr;					// our achievement manager
	int			m_nUserSlot;
	int			m_iDisplayOrder;						// Order in which the achievement is displayed in the UI
	bool		m_bShowOnHUD;							// if set, the player wants this achievement pinned to the HUD

	int			m_iAssetAwardID;						// ID of the avatar award asset associated with this achievement. Alliteration!

	friend class CAchievementMgr;
public:
	DECLARE_DATADESC();
};

class CMapAchievement : public CBaseAchievement
{
	virtual void Init()
	{
		SetFlags( ACH_LISTEN_MAP_EVENTS | ACH_SAVE_GLOBAL );
		SetGoal( 1 );
	}
};

//
// Helper class for achievement creation
//
 
typedef CBaseAchievement* (*achievementCreateFunc) (void);
class CBaseAchievementHelper
{
public:
	CBaseAchievementHelper( achievementCreateFunc createFunc );
	achievementCreateFunc m_pfnCreate;
	CBaseAchievementHelper *m_pNext;
	static CBaseAchievementHelper *s_pFirst;
};

#define DECLARE_MAP_EVENT_ACHIEVEMENT_( achievementID, achievementName, gameDirFilter, iPointValue, bHidden ) \
class CAchievement##achievementID : public CMapAchievement {};		\
DECLARE_ACHIEVEMENT_( CAchievement##achievementID, achievementID, achievementName, gameDirFilter, iPointValue, bHidden )	\

#define DECLARE_MAP_EVENT_ACHIEVEMENT( achievementID, achievementName, iPointValue )	\
	DECLARE_MAP_EVENT_ACHIEVEMENT_( achievementID, achievementName, nullptr, iPointValue, false )

#define DECLARE_MAP_EVENT_ACHIEVEMENT_HIDDEN( achievementID, achievementName, iPointValue )	\
	DECLARE_MAP_EVENT_ACHIEVEMENT_( achievementID, achievementName, nullptr, iPointValue, true )

#undef DECLARE_MAP_EVENT_ACHIEVEMENT_
#undef DECLARE_MAP_EVENT_ACHIEVEMENT
#undef DECLARE_MAP_EVENT_ACHIEVEMENT_HIDDEN

//-----------------------------------------------------------------------------
// Game systems are singleton objects in the client + server codebase responsible for
// various tasks
// The order in which the server systems appear in this list are the
// order in which they are initialized and updated. They are shut down in
// reverse order from which they are initialized.
//-----------------------------------------------------------------------------

// UNDONE: Do these need GameInit/GameShutdown as well?
// UNDONE: Remove the Pre/Post entity semantics and rely on system ordering?
// FIXME: Remove all ifdef CLIENT_DLL if we can...
class IGameSystem
{
public:
	// GameSystems are expected to implement these methods.
	virtual char const *Name() = 0;

	// Init, shutdown
	// return true on success. false to abort DLL init!
	virtual bool Init() = 0;
	virtual void PostInit() = 0;
	virtual void Shutdown() = 0;

	// Level init, shutdown
	virtual void LevelInitPreEntity() = 0;
	// entities are created / spawned / precached here
	virtual void LevelInitPostEntity() = 0;

	virtual void LevelShutdownPreEntity() = 0;
	// Entities are deleted / released here...
	virtual void LevelShutdownPostEntity() = 0;
	// end of level shutdown
	
	// Called during game save
	virtual void OnSave() = 0;

	// Called during game restore, after the local player has connected and entities have been fully restored
	virtual void OnRestore() = 0;

	// Called every frame. It's safe to remove an igamesystem from within this callback.
	virtual void SafeRemoveIfDesired() = 0;

	virtual bool	IsPerFrame() = 0;

	// destructor, cleans up automagically....
	virtual ~IGameSystem();

	// Client systems can use this to get at the map name
	static char const*	MapName();

	// These methods are used to add and remove server systems from the
	// main server loop. The systems are invoked in the order in which
	// they are added.
	static void Add ( IGameSystem* pSys );
	static void Remove ( IGameSystem* pSys );
	static void RemoveAll (  );

	// These methods are used to initialize, shutdown, etc all systems
	static bool InitAllSystems();
	static void PostInitAllSystems();
	static void ShutdownAllSystems();
	static void LevelInitPreEntityAllSystems( char const* pMapName );
	static void LevelInitPostEntityAllSystems();
	static void LevelShutdownPreEntityAllSystems();
	static void LevelShutdownPostEntityAllSystems();

	static void OnSaveAllSystems();
	static void OnRestoreAllSystems();

	static void SafeRemoveIfDesiredAllSystems();

	static void PreRenderAllSystems();
	static void UpdateAllSystems( float frametime );
	static void PostRenderAllSystems();
};

class IGameSystemPerFrame : public IGameSystem
{
public:
	// destructor, cleans up automagically....
	virtual ~IGameSystemPerFrame();

	// Called before rendering
	virtual void PreRender() = 0;

	// Gets called each frame
	virtual void Update( float frametime ) = 0;

	// Called after rendering
	virtual void PostRender() = 0;
};

// Quick and dirty server system for users who don't care about precise ordering
// and usually only want to implement a few of the callbacks
class CBaseGameSystem : public IGameSystem
{
public:

	virtual char const *Name() { return "unnamed"; }

	// Init, shutdown
	// return true on success. false to abort DLL init!
	virtual bool Init() { return true; }
	virtual void PostInit() {}
	virtual void Shutdown() {}

	// Level init, shutdown
	virtual void LevelInitPreEntity() {}
	virtual void LevelInitPostEntity() {}
	virtual void LevelShutdownPreEntity() {}
	virtual void LevelShutdownPostEntity() {}

	virtual void OnSave() {}
	virtual void OnRestore() {}
	virtual void SafeRemoveIfDesired() {}

	virtual bool	IsPerFrame() { return false; }
private:

	// Prevent anyone derived from CBaseGameSystem from implementing these, they need
	//  to derive from CBaseGameSystemPerFrame below!!!
	// Called before rendering
	virtual void PreRender() {}

	// Gets called each frame
	virtual void Update( float frametime ) {}

	// Called after rendering
	virtual void PostRender() {}
};

// Quick and dirty server system for users who don't care about precise ordering
// and usually only want to implement a few of the callbacks
class CBaseGameSystemPerFrame : public IGameSystemPerFrame
{
public:
	virtual char const *Name() { return "unnamed"; }

	// Init, shutdown
	// return true on success. false to abort DLL init!
	virtual bool Init() { return true; }
	virtual void PostInit() {}
	virtual void Shutdown() {}

	// Level init, shutdown
	virtual void LevelInitPreEntity() {}
	virtual void LevelInitPostEntity() {}
	virtual void LevelShutdownPreEntity() {}
	virtual void LevelShutdownPostEntity() {}

	virtual void OnSave() {}
	virtual void OnRestore() {}
	virtual void SafeRemoveIfDesired() {}

	virtual bool	IsPerFrame() { return true; }

	// Called before rendering
	virtual void PreRender () { }

	// Gets called each frame
	virtual void Update( float frametime ) { }

	// Called after rendering
	virtual void PostRender () { }
};

// Quick and dirty server system for users who don't care about precise ordering
// and usually only want to implement a few of the callbacks
class CAutoGameSystem : public CBaseGameSystem
{
public:
	CAutoGameSystem( char const *name = nullptr );	// hooks in at startup, no need to explicitly add
	CAutoGameSystem		*m_pNext;

	virtual char const *Name() { return m_pszName ? m_pszName : "unnamed"; }

private:
	char const *m_pszName;
};

//-----------------------------------------------------------------------------
// Purpose: This is a CAutoGameSystem which also cares about the "per frame" hooks
//-----------------------------------------------------------------------------
class CAutoGameSystemPerFrame : public CBaseGameSystemPerFrame
{
public:
	CAutoGameSystemPerFrame( char const *name = nullptr );
	CAutoGameSystemPerFrame *m_pNext;

	virtual char const *Name() { return m_pszName ? m_pszName : "unnamed"; }
	
private:
	char const *m_pszName;
};

class CBaseEntity;



//-----------------------------------------------------------------------------
// Purpose: Allows an entity to access its owner regardless of entity type 
//-----------------------------------------------------------------------------
class IHasOwner
{
public:
	virtual CBaseEntity			*GetOwnerViaInterface( void ) = 0;
};

class IUserMessageBinder;

// eliminates a conflict with TYPE_BOOL in OSX
#ifdef TYPE_BOOL
#undef TYPE_BOOL
#endif

//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose: Interface for registering and dispatching usermessages
// Shred code creates same ordered list on client/server
//-----------------------------------------------------------------------------
class CUserMessages
{
public:

	CUserMessages();
	~CUserMessages();

	bool DispatchUserMessage( int msg_type, int32 nPassthroughFlags, int size, const void *msg );
	void BindMessage( IUserMessageBinder *pMessageBinder );
	bool UnbindMessage( IUserMessageBinder *pMessageBinder );

private:
	typedef std::vector< IUserMessageBinder * > UserMessageBinderVec_t;
	typedef std::map< int, std::inplace_vector< UserMessageBinderVec_t, MAX_SPLITSCREEN_PLAYERS > > UserMessageHandlerMap_t;
	UserMessageHandlerMap_t m_UserMessageBinderMap;
};

extern CUserMessages *UserMessages();

//-----------------------------------------------------------------------------
// One bound handler of a user message type. The typed parts are supplied by
// CUserMessageBinder as callables. A parsed message is passed around type-erased so
// that every handler bound to the same message id shares one parse.
class IUserMessageBinder
{
public:
	IUserMessageBinder( int type, int32 nExpectedPassthroughInReplay, const char *typeName,
		std::move_only_function< std::shared_ptr< void >( const void *msg, int size ) > parse,
		std::move_only_function< bool( const void *msg ) > invoke,
		std::move_only_function< std::size_t( const void *msg ) > byteSize,
		std::move_only_function< std::string( const void *msg ) > describe )
		: m_nType( type )
		, m_nExpectedPassthroughInReplay( nExpectedPassthroughInReplay )
		, m_pszTypeName( typeName )
		, m_Parse( std::move( parse ) )
		, m_Invoke( std::move( invoke ) )
		, m_ByteSize( std::move( byteSize ) )
		, m_Describe( std::move( describe ) )
	{
		UserMessages()->BindMessage( this );
	}

	~IUserMessageBinder()
	{
		UserMessages()->UnbindMessage( this );
	}

	IUserMessageBinder( const IUserMessageBinder & ) = delete;
	IUserMessageBinder &operator=( const IUserMessageBinder & ) = delete;

	int GetType() const { return m_nType; }

	std::shared_ptr< void > Parse( int32 nPassthroughFlags, const void *msg, int size, bool &bSilentIgnore );
	bool Invoke( const void *msg ) { return msg && m_Invoke( msg ); }
	const char *TypeName() const { return m_pszTypeName; }
	std::size_t ByteSize( const void *msg ) { return m_ByteSize( msg ); }
	std::string Describe( const void *msg ) { return m_Describe( msg ); }

private:
	int m_nType;
	int32 m_nExpectedPassthroughInReplay;
	const char *m_pszTypeName;
	std::move_only_function< std::shared_ptr< void >( const void *msg, int size ) > m_Parse;
	std::move_only_function< bool( const void *msg ) > m_Invoke;
	std::move_only_function< std::size_t( const void *msg ) > m_ByteSize;
	std::move_only_function< std::string( const void *msg ) > m_Describe;
};

//-----------------------------------------------------------------------------
class CUserMessageBinder
{
public:
	CUserMessageBinder() = default;

	template< int msgType, typename PB_OBJECT_TYPE >
	void Bind( std::move_only_function< bool ( const PB_OBJECT_TYPE & obj ) > handler )
	{
		m_pBind.reset();
		m_pBind = Create< msgType, PB_OBJECT_TYPE, 0 >( std::move( handler ) );
	}

	template< int msgType, typename PB_OBJECT_TYPE >
	void BindRealtimePassthrough( std::move_only_function< bool ( const PB_OBJECT_TYPE & obj ) > handler )
	{
		m_pBind.reset();
		m_pBind = Create< msgType, PB_OBJECT_TYPE, 1 >( std::move( handler ) );
	}

	void Unbind()
	{
		m_pBind.reset();
	}
	bool IsBound() const
	{
		return m_pBind != nullptr;
	}

private:
	template< int msgType, typename PB_OBJECT_TYPE, int32 nExpectedPassthroughInReplay >
	static std::unique_ptr< IUserMessageBinder > Create( std::move_only_function< bool ( PB_OBJECT_TYPE const &msg ) > handler )
	{
		auto parse = []( const void *msg, int size ) -> std::shared_ptr< void >
		{
			auto pMsg = std::make_shared< PB_OBJECT_TYPE >();
			if ( !ks::proto::read_bytes( *pMsg, msg, size ) )
			{
				return nullptr;
			}
			return pMsg;
		};

		auto invoke = [handler = std::move( handler )]( const void *msg ) mutable -> bool
		{
			return handler( *static_cast< const PB_OBJECT_TYPE * >( msg ) );
		};

		auto byteSize = []( const void *msg ) -> std::size_t
		{
			return ks::proto::byte_size( *static_cast< const PB_OBJECT_TYPE * >( msg ) );
		};

		auto describe = []( const void *msg ) -> std::string
		{
			return ks::proto::to_text( *static_cast< const PB_OBJECT_TYPE * >( msg ) );
		};

		return std::make_unique< IUserMessageBinder >( msgType, nExpectedPassthroughInReplay,
			ks::proto::type_name< PB_OBJECT_TYPE >(),
			std::move( parse ), std::move( invoke ), std::move( byteSize ), std::move( describe ) );
	}

	std::unique_ptr< IUserMessageBinder > m_pBind;
};

// This provides an alternative to HOOK_MESSAGE, where you can declare it globally
// instead of finding a place to run it.
// It registers a function called __MsgFunc_<msgName>

class CUserMessageRegisterBase
{
public:
	CUserMessageRegisterBase( );

	// This is called at startup to register all the user messages.
	static void RegisterAll();

	virtual void Register() = 0;

private:

	// Linked list of all the CUserMessageRegisters.
	static CUserMessageRegisterBase *s_pHead;
	CUserMessageRegisterBase *m_pNext;
};

template< int msgType, typename PB_OBJECT_TYPE >
class CUserMessageRegister : public CUserMessageRegisterBase 
{
public:
	typedef bool (*pfnHandler )( const PB_OBJECT_TYPE& );

	explicit CUserMessageRegister( pfnHandler msgFunc )
	{
		m_pMsgFunc = msgFunc;
	}

	// This is called at startup to register all the user messages.
	void Register()
	{
		m_msgBinder.Bind< msgType, PB_OBJECT_TYPE >( m_pMsgFunc);
	}
	
	CUserMessageBinder m_msgBinder;
	pfnHandler m_pMsgFunc;
};

class CEntityGroundContact
{
public:
	int					entindex;
	float				minheight;
	float				maxheight;
};

//#define KEEP_COMMAND_REPREDICTION_COUNT //define to keep a count of how many times we've predicted each command (for debugging)

class CUserCmd
{
public:
	CUserCmd()
	{
		Reset();
	}

	~CUserCmd() { };

	void Reset()
	{
		command_number = 0;
		tick_count = 0;
		viewangles.Init();
		aimdirection.Init();
		forwardmove = 0.0f;
		sidemove = 0.0f;
		upmove = 0.0f;
		buttons = 0;
		impulse = 0;
		weaponselect = 0;
		weaponsubtype = 0;
		random_seed = 0;
		mousedx = 0;
		mousedy = 0;

		hasbeenpredicted = false;

		// TrackIR
		headangles.Init();
		headoffset.Init();
		// TrackIR

	}

	CUserCmd& operator =( const CUserCmd& src )
	{
		if ( this == &src )
			return *this;

		command_number		= src.command_number;
		tick_count			= src.tick_count;
		viewangles			= src.viewangles;
		aimdirection		= src.aimdirection;
		forwardmove			= src.forwardmove;
		sidemove			= src.sidemove;
		upmove				= src.upmove;
		buttons				= src.buttons;
		impulse				= src.impulse;
		weaponselect		= src.weaponselect;
		weaponsubtype		= src.weaponsubtype;
		random_seed			= src.random_seed;
		mousedx				= src.mousedx;
		mousedy				= src.mousedy;

		hasbeenpredicted	= src.hasbeenpredicted;

		// TrackIR
		headangles			= src.headangles;
		headoffset			= src.headoffset;
		// TrackIR

		return *this;
	}

	CUserCmd( const CUserCmd& src )
	{
		*this = src;
	}

	CRC32_t GetChecksum( void ) const;

	// For matching server and client commands for debugging
	int		command_number;
	
	// the tick the client created this command
	int		tick_count;
	
	// Player instantaneous view angles.
	QAngle	viewangles;
	Vector	aimdirection;	// For pointing devices. 
	// Intended velocities
	//	forward velocity.
	float	forwardmove;   
	//  sideways velocity.
	float	sidemove;      
	//  upward velocity.
	float	upmove;         
	// Attack button states
	int		buttons;		
	// Impulse command issued.
	byte    impulse;        
	// Current weapon id
	int		weaponselect;	
	int		weaponsubtype;

	int		random_seed;	// For shared random functions

	short	mousedx;		// mouse accum in x from create move
	short	mousedy;		// mouse accum in y from create move

	// Client only, tracks whether we've predicted this command at least once
	bool	hasbeenpredicted;

	// Back channel to communicate IK state

	// TrackIR
	QAngle headangles;
	Vector headoffset;
	// TrackIR

};

void ReadUsercmd( bf_read *buf, CUserCmd *move, CUserCmd *from );
void WriteUsercmd( bf_write *buf, const CUserCmd *to, const CUserCmd *from );

class CBaseAchievement;
class CBaseEntity;


inline constexpr auto THINK_CLEAR = -1;

enum SyncAchievementValueDirection_t
{
	ACHIEVEMENT_WRITE_ACHIEVEMENT,
	ACHIEVEMENT_READ_ACHIEVEMENT
};

class CAchievementMgr : public CAutoGameSystemPerFrame, public CGameEventListener
{
public:
	CAchievementMgr();
	static CAchievementMgr * GetInstance();
	static IAchievementMgr * GetInstanceInterface();

	virtual bool Init();
	virtual void PostInit();
	virtual void Shutdown();
	virtual void LevelInitPreEntity();
	virtual void LevelShutdownPreEntity();
	virtual void InitializeAchievements( );
	virtual void Update( float frametime );
	virtual bool IsPerFrame( void ) { return true; }

	void OnMapEvent( const char *pchEventName, int nUserSlot );

	// Interfaces exported to other dlls for achievement list queries
	IAchievement* GetAchievementByIndex( int index, int nUserSlot );
	IAchievement* GetAchievementByDisplayOrder( int orderIndex, int nUserSlot );
	IAchievement* GetAwardByDisplayOrder( int orderIndex, int nUserSlot );
	int GetAchievementCount( bool bAssets=false );

	CBaseAchievement *GetAchievementByID( int iAchievementID, int nUserSlot );
	typedef std::map< int, std::unique_ptr< CBaseAchievement > > AchievementMap_t;
	AchievementMap_t &GetAchievements( int nUserSlot ) { return m_mapAchievement[nUserSlot]; }

	CBaseAchievement *GetAchievementByName( const char *pchName, int nUserSlot );

	void UserConnected( int nUserSlot );
	void UserDisconnected( int nUserSlot );
	bool IsUserConnected( int nUserSlot ) { return m_bUserSlotActive[nUserSlot]; }
	void ReadAchievementsFromTitleData( int iController, int iSlot );
	void SaveGlobalState();
	void SaveGlobalStateIfDirty();
	bool IsAchievementAllowedInGame( int iAchievementID );
	bool HasAchieved( const char *pchName, int nUserSlot );
	void AwardAchievement( int iAchievementID, int nUserSlot );
	void UpdateAchievement( int iAchievementID, int nData, int nUserSlot );
	void PreRestoreSavedGame();
	void PostRestoreSavedGame();
	void ResetAchievements();
	void ClearAchievements( int nUserSlot );
	void ResetAchievement( int iAchievementID );
	void PrintAchievementStatus();
	float GetLastClassChangeTime( int nUserSlot ) { return m_flLastClassChangeTime[nUserSlot]; }
	float GetTeamplayStartTime( int nUserSlot ) { return m_flTeamplayStartTime[nUserSlot]; }
	int	  GetMiniroundsCompleted( int nUserSlot ) { return m_iMiniroundsCompleted[nUserSlot]; }
	const char *GetMapName() { return m_szMap; }
	bool IsCurrentMap( const char *pszMapName );
	void OnAchievementEvent( int iAchievementID, int nUserSlot );
	void SetDirty( bool bDirty, int nUserSlot ) { m_bDirty[nUserSlot] = bDirty; }
	bool CheckAchievementsEnabled();

	void CheckSignInState( bool bCheck ) { m_bCheckSigninState = bCheck; }

	// Match events
	virtual void OnEvent( KeyValues *pEvent );

	bool WereCheatsEverOn( void ) { return m_bCheatsEverOn; }

	const std::vector< int >& GetAchievedDuringCurrentGame( int nPlayerSlot );
	void ResetAchievedDuringCurrentGame( int nPlayerSlot );

	// [sbodenbender] brought over from orange box
	void SetAchievementThink( CBaseAchievement *pAchievement, float flThinkTime );

	bool SyncAchievementsToTitleData( int iController, SyncAchievementValueDirection_t eOp );

	void SendWriteProfileEvent();
	void SendResetProfileEvent();
	UIProfileInfo* GetUIProfileInfo() { return &m_UIProfileInfo; };
	void ResetProfileInfo();

	bool GetWriteProfileResult() { return m_writeProfileResult; }
	void SetWriteProfileResult(bool result) { m_writeProfileResult = result; }

	CUserMessageBinder m_UMCMsgAchievementEvent;

public:
	void FireGameEvent( IGameEvent *event );

private:

	// [sbodenbender] gobetween for UI and profile data
	UIProfileInfo				m_UIProfileInfo;
	bool						m_writeProfileResult;

	void OnKillEvent( CBaseEntity *pVictim, CBaseEntity *pAttacker, CBaseEntity *pInflictor, IGameEvent *event );
	void ResetAchievement_Internal( CBaseAchievement *pAchievement );

	AchievementMap_t m_mapAchievement[MAX_SPLITSCREEN_PLAYERS];			// map of all achievements (owning)
	std::vector< CBaseAchievement * >	 m_vecAchievement[MAX_SPLITSCREEN_PLAYERS];			// vector of all achievements for accessing by index
	std::vector< CBaseAchievement * >	 m_vecAward[MAX_SPLITSCREEN_PLAYERS];				// vector of all asset award achievements for accessing by index
	std::vector< CBaseAchievement * > m_vecKillEventListeners[MAX_SPLITSCREEN_PLAYERS];	// vector of achievements that are listening for kill events
	std::vector< CBaseAchievement * > m_vecMapEventListeners[MAX_SPLITSCREEN_PLAYERS];		// vector of achievements that are listening for map events
	std::vector< CBaseAchievement * > m_vecComponentListeners[MAX_SPLITSCREEN_PLAYERS];	// vector of achievements that are listening for components that make up an achievement
	std::vector< CBaseAchievement * >	 m_vecAchievementInOrder[MAX_SPLITSCREEN_PLAYERS];	// vector of all achievements for accessing by display order
	std::vector< CBaseAchievement * >	 m_vecAwardInOrder[MAX_SPLITSCREEN_PLAYERS];		// vector of all asset award achievements for accessing by display order

	// [sbodenbender] brought over from orange box
	struct achievementthink_t
	{
		float			 m_flThinkTime;
		CBaseAchievement *pAchievement;
	};
	std::vector< achievementthink_t > m_vecThinkListeners;					// vector of achievements that are actively thinking

	float m_flLevelInitTime[MAX_SPLITSCREEN_PLAYERS];
	float m_flLastClassChangeTime[MAX_SPLITSCREEN_PLAYERS];		// Time when player last changed class
	float m_flTeamplayStartTime[MAX_SPLITSCREEN_PLAYERS];		// Time when player joined a non-spectating team.  Not updated if she switches game teams; cleared if she joins spectator
	float m_iMiniroundsCompleted[MAX_SPLITSCREEN_PLAYERS];		// # of minirounds played since game start (for maps that have minirounds)
	char  m_szMap[MAX_PATH];										// file base of map name, cached since we access it frequently in this form
	bool  m_bDirty[MAX_SPLITSCREEN_PLAYERS];					// do we have interesting state that needs to be saved
	bool  m_bUserSlotActive[MAX_SPLITSCREEN_PLAYERS];
	bool  m_bCheatsEverOn;				// have cheats ever been turned on in this level

	bool m_bCheckSigninState;
	bool m_bReadingFromTitleData;

	std::vector< int > m_AchievementsAwardedDuringCurrentGame[MAX_SPLITSCREEN_PLAYERS];
	void ClearAchievementData( int nUserSlot );
};

class CAchievementMgrDelegateIAchievementMgr : public IAchievementMgr
{
public:
	virtual IAchievement* GetAchievementByIndex( int index, int nPlayerSlot ) { return m_pDelegate->CAchievementMgr::GetAchievementByIndex( index, nPlayerSlot ); }
	virtual IAchievement* GetAchievementByDisplayOrder( int orderIndex, int nPlayerSlot ) { return m_pDelegate->CAchievementMgr::GetAchievementByDisplayOrder( orderIndex, nPlayerSlot ); }
	virtual IAchievement* GetAwardByDisplayOrder( int orderIndex, int nPlayerSlot )  { return m_pDelegate->CAchievementMgr::GetAwardByDisplayOrder( orderIndex, nPlayerSlot ); }
	virtual CBaseAchievement* GetAchievementByID ( int id, int nPlayerSlot )  { return m_pDelegate->CAchievementMgr::GetAchievementByID( id, nPlayerSlot ); }
	virtual int GetAchievementCount( bool bAssets = false ) { return m_pDelegate->CAchievementMgr::GetAchievementCount( bAssets ); }
	virtual void InitializeAchievements( ) { m_pDelegate->CAchievementMgr::InitializeAchievements(); }
	virtual void AwardAchievement( int nAchievementID, int nPlayerSlot ) { m_pDelegate->CAchievementMgr::AwardAchievement( nAchievementID, nPlayerSlot ); }
	virtual void OnMapEvent( const char *pchEventName, int nPlayerSlot ) { m_pDelegate->CAchievementMgr::OnMapEvent( pchEventName, nPlayerSlot ); }
	virtual void SaveGlobalStateIfDirty( ) { m_pDelegate->CAchievementMgr::SaveGlobalState(); }
	virtual bool HasAchieved( const char *pchName, int nPlayerSlot ) { return m_pDelegate->CAchievementMgr::HasAchieved( pchName, nPlayerSlot ); }
	virtual const std::vector< int >& GetAchievedDuringCurrentGame( int nPlayerSlot ) { return m_pDelegate->CAchievementMgr::GetAchievedDuringCurrentGame( nPlayerSlot ); }
	virtual bool WereCheatsEverOn() { return m_pDelegate->CAchievementMgr::WereCheatsEverOn(); }

	virtual UIProfileInfo* GetUIProfileInfo() { return m_pDelegate->CAchievementMgr::GetUIProfileInfo(); }
	virtual void ResetProfileInfo() { m_pDelegate->CAchievementMgr::ResetProfileInfo(); }
	virtual void SendWriteProfileEvent() { m_pDelegate->CAchievementMgr::SendWriteProfileEvent(); }
	virtual void SendResetProfileEvent() { m_pDelegate->CAchievementMgr::SendResetProfileEvent(); }
	virtual bool GetWriteProfileResult() { return m_pDelegate->CAchievementMgr::GetWriteProfileResult(); }

	explicit CAchievementMgrDelegateIAchievementMgr( CAchievementMgr *pDelegate ) : m_pDelegate( pDelegate ) {}

protected:
	CAchievementMgr *m_pDelegate;
};

// helper functions
const char *GetModelName( CBaseEntity *pBaseEntity );

int	CalcPlayerCount();
int	CalcTeammateCount();

extern ConVar	cc_achievement_debug;

bool MsgFunc_AchievementEvent( const ks::net::CCSUsrMsg_AchievementEvent &msg );

class CBaseEntity;

using namespace ::ks::reflect;

// PlayerUse defines

typedef CHandle<CBaseEntity> EHANDLE;
inline constexpr auto PLAYER_USE_RADIUS = 80.f;
inline constexpr auto PLAYER_USE_BOT_RADIUS = 140.f;

inline constexpr auto CONE_45_DEGREES = 0.707f;
inline constexpr auto CONE_15_DEGREES = 0.9659258f;
inline constexpr auto CONE_90_DEGREES = 0;

inline constexpr auto TRAIN_ACTIVE = 0x80;
inline constexpr auto TRAIN_NEW = 0xc0;
inline constexpr auto TRAIN_OFF = 0x00;
inline constexpr auto TRAIN_NEUTRAL = 0x01;
inline constexpr auto TRAIN_SLOW = 0x02;
inline constexpr auto TRAIN_MEDIUM = 0x03;
inline constexpr auto TRAIN_FAST = 0x04;
inline constexpr auto TRAIN_BACK = 0x05;

// entity messages
inline constexpr auto UPDATE_PLAYER_RADAR = 2;

inline constexpr auto DEATH_ANIMATION_TIME = 3.0f;

// multiplayer only
inline constexpr auto NOINTERP_PARITY_MAX = 4;
inline constexpr auto NOINTERP_PARITY_MAX_BITS = 2;

struct autoaim_params_t
{
	autoaim_params_t();

	Vector		m_vecAutoAimDir;		// Output: The direction autoaim wishes to point.
	Vector		m_vecAutoAimPoint;		// Output: The point (world space) that autoaim is aiming at.
	EHANDLE		m_hAutoAimEntity;		// Output: The entity that autoaim is aiming at.
	float		m_fScale;				// Input:
	float		m_fMaxDist;				// Input:
	float		m_fMaxDeflection;		// Input:
	bool		m_bOnTargetQueryOnly;	// Input: Don't do expensive assistance, just resolve m_bOnTargetNatural
	bool		m_bAutoAimAssisting;	// Output: If this is true, autoaim is aiming at the target.
	bool		m_bOnTargetNatural;		// Output: If true, the player is on target without assistance.
};

enum stepsoundtimes_t
{
	STEPSOUNDTIME_NORMAL = 0,
	STEPSOUNDTIME_ON_LADDER,
	STEPSOUNDTIME_WATER_KNEE,
	STEPSOUNDTIME_WATER_FOOT,
};

//
// Player PHYSICS FLAGS bits
//
enum PlayerPhysFlag_e
{
	PFLAG_DIROVERRIDE	= ( 1<<0 ),		// override the player's directional control (trains, physics gun, etc.)
	PFLAG_DUCKING		= ( 1<<1 ),		// In the process of ducking, but totally squatted yet
	PFLAG_USING			= ( 1<<2 ),		// Using a continuous entity
	PFLAG_OBSERVER		= ( 1<<3 ),		// player is locked in stationary cam mode. Spectators can move, observers can't.
	PFLAG_VPHYSICS_MOTIONCONTROLLER = ( 1<<4 ),	// player is physically attached to a motion controller
	PFLAG_GAMEPHYSICS_ROTPUSH = (1<<5), // game physics did a rotating push that we may want to override with vphysics

	// If you add another flag here check that you aren't 
	// overwriting phys flags in the HL2 of TF2 player classes
};

enum
{
	VPHYS_WALK = 0,
	VPHYS_CROUCH,
	VPHYS_NOCLIP,
};

// useful cosines
inline constexpr auto DOT_1DEGREE = 0.9998476951564;
inline constexpr auto DOT_2DEGREE = 0.9993908270191;
inline constexpr auto DOT_3DEGREE = 0.9986295347546;
inline constexpr auto DOT_4DEGREE = 0.9975640502598;
inline constexpr auto DOT_5DEGREE = 0.9961946980917;
inline constexpr auto DOT_6DEGREE = 0.9945218953683;
inline constexpr auto DOT_7DEGREE = 0.9925461516413;
inline constexpr auto DOT_8DEGREE = 0.9902680687416;
inline constexpr auto DOT_9DEGREE = 0.9876883405951;
inline constexpr auto DOT_10DEGREE = 0.9848077530122;
inline constexpr auto DOT_15DEGREE = 0.9659258262891;
inline constexpr auto DOT_20DEGREE = 0.9396926207859;
inline constexpr auto DOT_25DEGREE = 0.9063077870367;
inline constexpr auto DOT_30DEGREE = 0.866025403784;
inline constexpr auto DOT_45DEGREE = 0.707106781187;

//#define DEBUG_MOTION_CONTROLLERS  //uncomment to spew debug data while in a motion controller

enum HltvUiType_t
{
	HLTV_UI_XRAY_ON = 0,
	HLTV_UI_XRAY_OFF,
	HLTV_UI_SCOREBOARD_ON,
	HLTV_UI_SCOREBOARD_OFF,
	HLTV_UI_OVERVIEW_ON,
	HLTV_UI_OVERVIEW_OFF,
	HLTV_UI_GRAPHS_ON,
	HLTV_UI_GRAPHS_OFF
};

// Shared header file for players

class CBaseEntity;

typedef CBaseEntity* (*DISPATCHFUNCTION)( void );

class IClassMap
{
public:
	virtual					~IClassMap() {}

	virtual void			Add( const char *mapname, const char *classname, int size, DISPATCHFUNCTION factory = 0 ) = 0;
	virtual char const		*Lookup( const char *classname ) = 0;
	virtual CBaseEntity	*CreateEntity( const char *mapname ) = 0;
	virtual int				GetClassSize( const char *classname ) = 0;
};

extern IClassMap& GetClassMap();

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// represents a handle used only by the client DLL
//-----------------------------------------------------------------------------

typedef CBaseHandle ClientEntityHandle_t;

class IClientEntityListener;

class CBaseEntity;

inline constexpr auto INPVS_YES = 0x0001; // The entity thinks it's in the PVS.
inline constexpr auto INPVS_THISFRAME = 0x0002; // Accumulated as different views are rendered during the frame and used to notify the entity if
										// it is not in the PVS anymore (at the end of the frame).
inline constexpr auto INPVS_NEEDSNOTIFY = 0x0004; // The entity thinks it's in the PVS.
							   

class CBaseEntityClassList
{
public:
	CBaseEntityClassList();
	~CBaseEntityClassList();
	virtual void LevelShutdown() = 0;

	CBaseEntityClassList *m_pNextClassList;
};

template< class T >
class CEntityClassList : public CBaseEntityClassList
{
public:
	virtual void LevelShutdown()  { m_pClassList = nullptr; }

	void Insert( T *pEntity )
	{
		pEntity->m_pNext = m_pClassList;
		m_pClassList = pEntity;
	}

	void Remove( T *pEntity )
	{
		T **pPrev = &m_pClassList;
		T *pCur = *pPrev;
		while ( pCur )
		{
			if ( pCur == pEntity )
			{
				*pPrev = pCur->m_pNext;
				return;
			}
			pPrev = &pCur->m_pNext;
			pCur = *pPrev;
		}
	}

	static T *m_pClassList;
};

// Maximum size of entity list

//
// This is the IClientEntityList implemenation. It serves two functions:
//
// 1. It converts server entity indices into IClientNetworkables for the engine.
//
// 2. It provides a place to store IClientUnknowns and gives out ClientEntityHandle_t's
//    so they can be indexed and retreived. For example, this is how static props are referenced
//    by the spatial partition manager - it doesn't know what is being inserted, so it's 
//	  given ClientEntityHandle_t's, and the handlers for spatial partition callbacks can
//    use the client entity list to look them up and check for supported interfaces.
//
class CClientEntityList : public CBaseEntityList, public IClientEntityList
{
friend class CBaseEntityIterator;
friend class CAllBaseEntityIterator;

public:
	// Constructor, destructor
								CClientEntityList( void );
	virtual 					~CClientEntityList( void );

	void						Release();		// clears everything and releases entities

// Implement IClientEntityList
public:

	virtual IClientNetworkable*	GetClientNetworkable( int entnum );
	virtual EntityCacheInfo_t	*GetClientNetworkableArray();
	virtual IClientEntity*		GetClientEntity( int entnum );

	virtual int					NumberOfEntities( bool bIncludeNonNetworkable = false );

	virtual IClientUnknown*		GetClientUnknownFromHandle( ClientEntityHandle_t hEnt );
	virtual IClientNetworkable*	GetClientNetworkableFromHandle( ClientEntityHandle_t hEnt );
	virtual IClientEntity*		GetClientEntityFromHandle( ClientEntityHandle_t hEnt );

	virtual int					GetHighestEntityIndex( void );

	virtual void				SetMaxEntities( int maxents );
	virtual int					GetMaxEntities( );

// CBaseEntityList overrides.
protected:

	virtual void OnAddEntity( IHandleEntity *pEnt, CBaseHandle handle );
	virtual void OnRemoveEntity( IHandleEntity *pEnt, CBaseHandle handle );

// Internal to client DLL.
public:

	// All methods of accessing specialized IClientUnknown's go through here.
	IClientUnknown*			GetListedEntity( int entnum );
	
	// Simple wrappers for convenience..
	CBaseEntity*			GetBaseEntity( int entnum );
	ICollideable*			GetCollideable( int entnum );

	IClientRenderable*		GetClientRenderableFromHandle( ClientEntityHandle_t hEnt );
	CBaseEntity*			GetBaseEntityFromHandle( ClientEntityHandle_t hEnt );
	ICollideable*			GetCollideableFromHandle( ClientEntityHandle_t hEnt );
	IClientThinkable*		GetClientThinkableFromHandle( ClientEntityHandle_t hEnt );

	// Convenience methods to convert between entindex + ClientEntityHandle_t
	ClientEntityHandle_t	EntIndexToHandle( int entnum );
	int						HandleToEntIndex( ClientEntityHandle_t handle );

	// Is a handle valid?
	bool					IsHandleValid( ClientEntityHandle_t handle ) const;

	// For backwards compatibility...
	CBaseEntity*			GetEnt( int entnum ) { return GetBaseEntity( entnum ); }

	void					RecomputeHighestEntityUsed( void );

	// Use this to iterate over all the C_BaseEntities.
	CBaseEntity* FirstBaseEntity() const;
	CBaseEntity* NextBaseEntity( CBaseEntity *pEnt ) const;

	class CPVSNotifyInfo
	{
	public:
		IPVSNotify *m_pNotify;
		IClientRenderable *m_pRenderable;
		unsigned char m_InPVSStatus;				// Combination of the INPVS_ flags.
	};

	// Get the list of all PVS notifiers.
	std::list<CPVSNotifyInfo>& GetPVSNotifiers();

	std::vector< IClientEntityListener * >	m_entityListeners;

	// add a class that gets notified of entity events
	void AddListenerEntity( IClientEntityListener *pListener );
	void RemoveListenerEntity( IClientEntityListener *pListener );

	void NotifyCreateEntity( CBaseEntity *pEnt );
	void NotifyRemoveEntity( CBaseEntity *pEnt );
	void SetDormant( int entityIndex, bool bDormant );
private:

	// Current count
	int					m_iNumServerEnts;
	// Max allowed
	int					m_iMaxServerEnts;

	int					m_iNumClientNonNetworkable;

	// Current last used slot
	int					m_iMaxUsedServerIndex;

	// This holds fast lookups for special edicts.
	EntityCacheInfo_t	m_EntityCacheInfo[NUM_ENT_ENTRIES];

	// For fast iteration. Slot handles live in EntityCacheInfo_t::m_BaseEntitiesIndex.
	ks::slot_pool<CBaseEntity*, unsigned short> m_BaseEntities;

private:

	void AddPVSNotifier( IClientUnknown *pUnknown );
	void RemovePVSNotifier( IClientUnknown *pUnknown );
	
	// These entities want to know when they enter and leave the PVS (server entities
	// already can get the equivalent notification with NotifyShouldTransmit, but client
	// entities have to get it this way).
	std::list<CPVSNotifyInfo> m_PVSNotifyInfos;
	std::map<IClientUnknown*,std::list<CPVSNotifyInfo>::iterator> m_PVSNotifierMap;	// Maps IClientUnknowns to their entries in m_PVSNotifyInfos.
};

// Use this to iterate over *all* (even dormant) the C_BaseEntities in the client entity list.
class CAllBaseEntityIterator
{
public:
	CAllBaseEntityIterator();

	void Restart();
	CBaseEntity* Next();	// keep calling this until it returns null.

private:
	unsigned short m_CurBaseEntity;
};

class CBaseEntityIterator
{
public:
	CBaseEntityIterator();

	void Restart();
	CBaseEntity* Next();	// keep calling this until it returns null.

private:
	unsigned short m_CurBaseEntity;
};

//-----------------------------------------------------------------------------
// Returns the client entity list
//-----------------------------------------------------------------------------
extern CClientEntityList *cl_entitylist;

inline CClientEntityList& ClientEntityList()
{
	return *cl_entitylist;
}

// Implement this class and register with entlist to receive entity create/delete notification
class IClientEntityListener
{
public:
	virtual void OnEntityCreated( CBaseEntity *pEntity ) {};
	//virtual void OnEntitySpawned( CBaseEntity *pEntity ) {};
	virtual void OnEntityDeleted( CBaseEntity *pEntity ) {};
};

class CBaseEntity;

// Purpose: This file contains code to allow us to associate client data with bsp leaves.

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CStaticProp;

//-----------------------------------------------------------------------------
// Render groups
//-----------------------------------------------------------------------------
enum RenderGroup_t
{
	RENDER_GROUP_OPAQUE = 0,
	RENDER_GROUP_TRANSLUCENT,
	RENDER_GROUP_TRANSLUCENT_IGNOREZ,
	RENDER_GROUP_COUNT, // Indicates the groups above are real and used for bucketing a scene
};

//-----------------------------------------------------------------------------
// Handle to an renderables in the client leaf system
//-----------------------------------------------------------------------------
enum
{
	DETAIL_PROP_RENDER_HANDLE = (ClientRenderHandle_t)0xfffe
};

//-----------------------------------------------------------------------------
// Distance fade information
//-----------------------------------------------------------------------------
struct DistanceFadeInfo_t
{
	float m_flMaxDistSqr;		// distance at which everything is faded out
	float m_flMinDistSqr;		// distance at which everything is unfaded
	float m_flFalloffFactor;	// 1.0f / ( maxDistSqr - MinDistSqr )
								// opacity = ( maxDist - distSqr ) * falloffFactor 
};

class CClientRenderablesList : public CRefCounted<CClientRenderablesList>, public ks::pooled< CClientRenderablesList >
{
	private:
		static ks::fixed_pool s_Allocator;
		friend class ks::pooled< CClientRenderablesList >;

public:
	CClientRenderablesList();

	enum
	{
		MAX_GROUP_ENTITIES = 4096,
		MAX_BONE_SETUP_DEPENDENCY = 64,
	};

	struct CEntry
	{
		IClientRenderable	*m_pRenderable;
		unsigned short		m_iWorldListInfoLeaf; // NOTE: this indexes WorldListInfo_t's leaf list.
		RenderableInstance_t m_InstanceData;
		uint8				m_nModelType : 6;		// See RenderableModelType_t
		uint8				m_bShadowDepthNoCache : 1;	// the renderable cannot be cached in shadow depth cache
		uint8				m_TwoPass : 1;
		bool				m_bIsCombinedModel;
	};

	// The leaves for the entries are in the order of the leaves you call CollateRenderablesInLeaf in.
	DistanceFadeInfo_t	m_DetailFade;
	CEntry				m_RenderGroups[RENDER_GROUP_COUNT][MAX_GROUP_ENTITIES];
	int					m_RenderGroupCounts[RENDER_GROUP_COUNT];
	int					m_nBoneSetupDependencyCount;
	IClientRenderable	*m_pBoneSetupDependency[MAX_BONE_SETUP_DEPENDENCY];
};

struct ViewmodelRenderableInstance_t : public RenderableInstance_t
{
	uint8 m_bTwoPass;
};
//-----------------------------------------------------------------------------
// Render list for viewmodels
//-----------------------------------------------------------------------------
class CViewModelRenderablesList
{
public:
	enum
	{
		VM_GROUP_OPAQUE = 0,
		VM_GROUP_TRANSLUCENT,
		VM_GROUP_COUNT,
	};

	struct CEntry
	{
		IClientRenderable	*m_pRenderable;
		ViewmodelRenderableInstance_t m_InstanceData;
	};

	typedef ks::small_vector< CEntry, 32 > RenderGroups_t;

	// The leaves for the entries are in the order of the leaves you call CollateRenderablesInLeaf in.
	RenderGroups_t	m_RenderGroups[VM_GROUP_COUNT];
};

//-----------------------------------------------------------------------------
// Used to do batched screen size computations
//-----------------------------------------------------------------------------
struct ScreenSizeComputeInfo_t
{
	VMatrix m_matViewProj;
	Vector m_vecViewUp;
	int m_nViewportHeight;
};

void ComputeScreenSizeInfo( ScreenSizeComputeInfo_t *pInfo );
float ComputeScreenSize( const Vector &vecOrigin, float flRadius, const ScreenSizeComputeInfo_t& info );

//-----------------------------------------------------------------------------
// Used by CollateRenderablesInLeaf
//-----------------------------------------------------------------------------
struct SetupRenderInfo_t
{
	mutable WorldListInfo_t *		m_pWorldListInfo;
	mutable CClientRenderablesList *m_pRenderList;
	Vector m_vecRenderOrigin;
	Vector m_vecRenderForward;
	int m_nRenderFrame;
	int m_nDetailBuildFrame;	// The "render frame" for detail objects

	float m_flRenderDistSq;
	int m_nViewID;
	int m_nBuildViewID;
	int m_nOcclustionViewID;
	mutable const CVolumeCuller *	m_pCSMVolumeCuller;
	mutable const Frustum_t*		m_pFrustum;
	mutable Frustum_t**				m_ppFrustumList;
	ScreenSizeComputeInfo_t m_screenSizeInfo;

	bool m_bDrawDetailObjects : 1;
	bool m_bDrawTranslucentObjects : 1;
	bool m_bFastEntityRendering: 1;
	bool m_bDrawDepthViewNonCachedObjectsOnly : 1;
	bool m_bCSMView : 1;

	SetupRenderInfo_t();
};

//-----------------------------------------------------------------------------
// A handle associated with shadows managed by the client leaf system
//-----------------------------------------------------------------------------
typedef unsigned short ClientLeafShadowHandle_t;
enum
{
	CLIENT_LEAF_SHADOW_INVALID_HANDLE = (ClientLeafShadowHandle_t)~0 
};

//-----------------------------------------------------------------------------
// The client leaf system
//-----------------------------------------------------------------------------
class IClientLeafShadowEnum
{
public:
	// The user ID is the id passed into CreateShadow
	virtual void EnumShadow( ClientShadowHandle_t userId ) = 0;
};

// subclassed by things which wish to add per-leaf data managed by the client leafsystem
class CClientLeafSubSystemData
{
public:
	virtual ~CClientLeafSubSystemData( void )
	{
	}
};

// defines for subsystem ids. each subsystem id uses up one pointer in each leaf
inline constexpr int CLSUBSYSTEM_DETAILOBJECTS = 0;
inline constexpr int N_CLSUBSYSTEMS = 1;

//-----------------------------------------------------------------------------
// The client leaf system
//-----------------------------------------------------------------------------
class IClientLeafSystem : public IClientLeafSystemEngine, public IGameSystemPerFrame
{
public:
	// Adds and removes renderables from the leaf lists
	virtual void AddRenderable( IClientRenderable* pRenderable, bool bRenderWithViewModels, RenderableTranslucencyType_t nType, RenderableModelType_t nModelType, uint32 nSplitscreenEnabled = 0xFFFFFFFF ) = 0;

	// This tells if the renderable is in the current PVS. It assumes you've updated the renderable
	// with RenderableChanged() calls
	virtual bool IsRenderableInPVS( IClientRenderable *pRenderable ) = 0;

	virtual void SetSubSystemDataInLeaf( int leaf, int nSubSystemIdx, CClientLeafSubSystemData *pData ) =0;
	virtual CClientLeafSubSystemData *GetSubSystemDataInLeaf( int leaf, int nSubSystemIdx ) =0;

	virtual void SetDetailObjectsInLeaf( int leaf, int firstDetailObject, int detailObjectCount ) = 0;
	virtual void GetDetailObjectsInLeaf( int leaf, int& firstDetailObject, int& detailObjectCount ) = 0;

	// Indicates which leaves detail objects should be rendered from, returns the detais objects in the leaf
	virtual void DrawDetailObjectsInLeaf( int leaf, int frameNumber, int& firstDetailObject, int& detailObjectCount ) = 0;

	// Should we draw detail objects (sprites or models) in this leaf (because it's close enough to the view)
	// *and* are there any objects in the leaf?
	virtual bool ShouldDrawDetailObjectsInLeaf( int leaf, int frameNumber ) = 0;

	// Call this when a renderable origin/angles/bbox parameters has changed
	virtual void RenderableChanged( ClientRenderHandle_t handle ) = 0;

	// Put renderables into their appropriate lists.
	virtual void BuildRenderablesList( const SetupRenderInfo_t &info ) = 0;

	// Put renderables in the leaf into their appropriate lists.
	virtual void CollateViewModelRenderables( CViewModelRenderablesList *pList ) = 0;

	// Call this to deactivate static prop rendering..
	virtual void DrawStaticProps( bool enable ) = 0;

	// Call this to deactivate small object rendering
	virtual void DrawSmallEntities( bool enable ) = 0;

	// The following methods are related to shadows...
	virtual ClientLeafShadowHandle_t AddShadow( ClientShadowHandle_t userId, unsigned short flags ) = 0;
	virtual void RemoveShadow( ClientLeafShadowHandle_t h ) = 0;

	// Project a shadow
	virtual void ProjectShadow( ClientLeafShadowHandle_t handle, int nLeafCount, const int *pLeafList ) = 0;

	// Project a projected texture spotlight
	virtual void ProjectFlashlight( ClientLeafShadowHandle_t handle, int nLeafCount, const int *pLeafList ) = 0;

	// Find all shadow casters in a set of leaves
	virtual void EnumerateShadowsInLeaves( int leafCount, WorldListLeafData_t* pLeaves, IClientLeafShadowEnum* pEnum ) = 0;

	// Fill in a list of the leaves this renderable is in.
	// Returns -1 if the handle is invalid.
	virtual int GetRenderableLeaves( ClientRenderHandle_t handle, int leaves[128] ) = 0;

	// Get leaves this renderable is in
	virtual bool GetRenderableLeaf ( ClientRenderHandle_t handle, int* pOutLeaf, const int* pInIterator = 0, int* pOutIterator = 0 ) = 0;

	// Draw translucent objects in the opaque renderables pass
	virtual void EnableForceOpaquePass( ClientRenderHandle_t handle, bool bEnable ) = 0;
	virtual bool IsEnableForceOpaquePass( ClientRenderHandle_t handle ) const = 0;

	// Use alternate translucent sorting algorithm (draw translucent objects in the furthest leaf they lie in)
	virtual void EnableAlternateSorting( ClientRenderHandle_t handle, bool bEnable ) = 0;

	// Mark this as rendering with viewmodels
	virtual void RenderWithViewModels( ClientRenderHandle_t handle, bool bEnable ) = 0;
	virtual bool IsRenderingWithViewModels( ClientRenderHandle_t handle ) const = 0;

	// Call this if the model changes
	virtual void SetTranslucencyType( ClientRenderHandle_t handle, RenderableTranslucencyType_t nType ) = 0;
	virtual RenderableTranslucencyType_t GetTranslucencyType( ClientRenderHandle_t handle ) const = 0;
	virtual void SetModelType( ClientRenderHandle_t handle, RenderableModelType_t nType = RENDERABLE_MODEL_UNKNOWN_TYPE ) = 0;
	virtual void EnableSplitscreenRendering( ClientRenderHandle_t handle, uint32 nFlags ) = 0;

	// Suppress rendering of this renderable
	virtual void EnableRendering( ClientRenderHandle_t handle, bool bEnable ) = 0;

	// Indicates this renderable should bloat its client leaf bounds over time
	// used for renderables with oscillating bounds to reduce the cost of
	// them reinserting themselves into the tree over and over.
	virtual void EnableBloatedBounds( ClientRenderHandle_t handle, bool bEnable ) = 0;

	// Indicates this renderable should always recompute its bounds accurately
	virtual void DisableCachedRenderBounds( ClientRenderHandle_t handle, bool bDisable ) = 0;

	// Recomputes which leaves renderables are in
	virtual void RecomputeRenderableLeaves() = 0;

	// Warns about leaf reinsertion
	virtual void DisableLeafReinsertion( bool bDisable ) = 0;

	//Assuming the renderable would be in a properly built render list, generate a render list entry
	virtual RenderGroup_t GenerateRenderListEntry( IClientRenderable *pRenderable, CClientRenderablesList::CEntry &entryOut ) = 0; 

	// Get renderable that render bound intersect with the query box
	virtual int GetEntitiesInBox( CBaseEntity **pEntityList, int listMax, const Vector& vWorldSpaceMins, const Vector& vWorldSpaceMaxs ) = 0;

	// Mark as rendering in the reflection
	virtual bool IsRenderingInFastReflections( ClientRenderHandle_t handle ) const = 0;

	// Enable/disable rendering into the shadow depth buffer
	virtual void DisableShadowDepthRendering( ClientRenderHandle_t handle, bool bDisable ) = 0;

	// Enable/disable rendering into the CSM buffer and rendering usig CSM's
	virtual void DisableCSMRendering( ClientRenderHandle_t handle, bool bDisable ) = 0;

	// Enable/disable caching in the shadow depth buffer
	virtual void DisableShadowDepthCaching( ClientRenderHandle_t handle, bool bDisable ) = 0;

	virtual void ComputeAllBounds( void ) = 0;

};

//-----------------------------------------------------------------------------
// Singleton accessor
//-----------------------------------------------------------------------------
extern IClientLeafSystem *g_pClientLeafSystem;
inline IClientLeafSystem* ClientLeafSystem()
{
	return g_pClientLeafSystem;
}

typedef struct
{
	char *name;
	bool draw;
} ModeElements;

class CBaseEntity;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
void SendProxy_Solid( const SendProp *pProp, const void *pStruct,
    const void *pData, DVariant *pOut, int iElement, int objectID );

void SendProxy_SolidFlags( const SendProp *pProp, const void *pStruct,
    const void *pData, DVariant *pOut, int iElement, int objectID );

void RecvProxy_IntDirtySurround( const CRecvProxyData *pData, void *pStruct, void *pOut );

void RecvProxy_OBBMaxs( const CRecvProxyData *pData, void *pStruct, void *pOut );

void RecvProxy_OBBMins( const CRecvProxyData *pData, void *pStruct, void *pOut );

void RecvProxy_VectorDirtySurround( const CRecvProxyData *pData, void *pStruct, void *pOut );

void RecvProxy_Solid( const CRecvProxyData *pData, void *pStruct, void *pOut );

void RecvProxy_SolidFlags( const CRecvProxyData *pData, void *pStruct, void *pOut );

//-----------------------------------------------------------------------------
// Force spatial partition updates (to avoid threading problems caused by lazy update)
//-----------------------------------------------------------------------------
void UpdateDirtySpatialPartitionEntities();

//-----------------------------------------------------------------------------
// Specifies how to compute the surrounding box
//-----------------------------------------------------------------------------
enum SurroundingBoundsType_t
{
	USE_OBB_COLLISION_BOUNDS = 0,
	USE_BEST_COLLISION_BOUNDS,		// Always use the best bounds (most expensive)
	USE_HITBOXES,
	USE_SPECIFIED_BOUNDS,
	USE_GAME_CODE,
	USE_ROTATION_EXPANDED_BOUNDS,
	USE_COLLISION_BOUNDS_NEVER_VPHYSICS,
	USE_ROTATION_EXPANDED_SEQUENCE_BOUNDS,

	SURROUNDING_TYPE_BIT_COUNT = 3
};

//-----------------------------------------------------------------------------
// Encapsulates collision representation for an entity
//-----------------------------------------------------------------------------
class CCollisionProperty : public ICollideable
{
	[[= ks::reflect::NetTable{ .name = "DT_CollisionProperty", .base = false } ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS_NOBASE( CCollisionProperty );
	DECLARE_EMBEDDED_NETWORKVAR();
	DECLARE_PREDICTABLE();

public:
	CCollisionProperty();
	~CCollisionProperty();

	void Init( CBaseEntity *pEntity );

	// Methods of ICollideable
	virtual IHandleEntity	*GetEntityHandle();
 	virtual const Vector&	OBBMins( ) const;
	virtual const Vector&	OBBMaxs( ) const;
	virtual void			WorldSpaceTriggerBounds( Vector *pVecWorldMins, Vector *pVecWorldMaxs ) const;
	virtual bool			TestCollision( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr );
	virtual bool			TestHitboxes( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr );
	virtual int				GetCollisionModelIndex();
	virtual const model_t*	GetCollisionModel();
	virtual const Vector&	GetCollisionOrigin() const;
	virtual const QAngle&	GetCollisionAngles() const;
	virtual const matrix3x4_t&	CollisionToWorldTransform() const;
	virtual SolidType_t		GetSolid() const;
	virtual int				GetSolidFlags() const;
	virtual IClientUnknown*	GetIClientUnknown();
	virtual int				GetCollisionGroup() const;
	virtual void			WorldSpaceSurroundingBounds( Vector *pVecMins, Vector *pVecMaxs );
	virtual uint			GetRequiredTriggerFlags() const;
	virtual const matrix3x4_t *GetRootParentToWorldTransform() const;
	virtual IPhysicsObject	*GetVPhysicsObject() const;

public:
	// Spatial partition management
	void						CreatePartitionHandle();
	void						DestroyPartitionHandle();
	SpatialPartitionHandle_t	GetPartitionHandle() const;

	// Marks the spatial partition dirty
	void			MarkPartitionHandleDirty();

	// Sets the collision bounds + the size (OBB)
	void			SetCollisionBounds( const Vector& mins, const Vector &maxs );

	// Sets special trigger bounds. The bloat amount indicates how much bigger the 
	// trigger bounds should be beyond the bounds set in SetCollisionBounds
	// This method will also set the FSOLID flag FSOLID_USE_TRIGGER_BOUNDS
	void			UseTriggerBounds( bool bEnable, float flBloat = 0.0f );

	// Sets the method by which the surrounding collision bounds is set
	// You must pass in values for mins + maxs if you select the USE_SPECIFIED_BOUNDS type. 
	void			SetSurroundingBoundsType( SurroundingBoundsType_t type, const Vector *pMins = nullptr, const Vector *pMaxs = nullptr );
	SurroundingBoundsType_t GetSurroundingBoundsType() const;

	// Sets the solid type (which type of collision representation)
	void			SetSolid( SolidType_t val );

	// Methods related to size. The OBB here is measured in CollisionSpace
	// (specified by GetCollisionToWorld)
	const Vector&	OBBSize( ) const;

	// Returns a radius (or the square of the radius) of a sphere 
	// *centered at the world space center* bounding the collision representation 
	// of the entity. NOTE: The world space center *may* move when the entity rotates.
	float			BoundingRadius() const;
	float			BoundingRadius2D() const;

	// Returns the center of the OBB in collision space
	const Vector &	OBBCenter( ) const;

	// center point of entity measured in world space
	// NOTE: This point *may* move when the entity moves depending on
	// which solid type is being used.
	const Vector &	WorldSpaceCenter( ) const;

	// Methods related to solid flags
	void			ClearSolidFlags( void );	
	void			RemoveSolidFlags( int flags );
	void			AddSolidFlags( int flags );
	bool			IsSolidFlagSet( int flagMask ) const;
	void		 	SetSolidFlags( int flags );
	bool			IsSolid() const;

	// Updates the spatial partition
	void			UpdatePartition( );

	// Are the bounds defined in entity space?
	bool			IsBoundsDefinedInEntitySpace() const;

	// Transforms a point in OBB space to world space
	const Vector &	CollisionToWorldSpace( const Vector &in, Vector *pResult ) const;

	// Transforms a point in world space to OBB space
	const Vector &	WorldToCollisionSpace( const Vector &in, Vector *pResult ) const;

	// Transforms a direction in world space to OBB space
	const Vector &	WorldDirectionToCollisionSpace( const Vector &in, Vector *pResult ) const;

	// Selects a random point in the bounds given the normalized 0-1 bounds 
	void			RandomPointInBounds( const Vector &vecNormalizedMins, const Vector &vecNormalizedMaxs, Vector *pPoint) const;

	// Is a worldspace point within the bounds of the OBB?
	bool			IsPointInBounds( const Vector &vecWorldPt ) const;

	// Computes a bounding box in world space surrounding the collision bounds
	void			WorldSpaceAABB( Vector *pWorldMins, Vector *pWorldMaxs ) const;

	// Computes a "normalized" point (range 0,0,0 - 1,1,1) in collision space
	// Useful for things like getting a point 75% of the way along z on the OBB, for example
	const Vector &	NormalizedToCollisionSpace( const Vector &in, Vector *pResult ) const;

	// Computes a "normalized" point (range 0,0,0 - 1,1,1) in world space
	const Vector &	NormalizedToWorldSpace( const Vector &in, Vector *pResult ) const;

	// Transforms a point in world space to normalized space
	const Vector &	WorldToNormalizedSpace( const Vector &in, Vector *pResult ) const;

	// Transforms a point in collision space to normalized space
	const Vector &	CollisionToNormalizedSpace( const Vector &in, Vector *pResult ) const;

	// Computes the nearest point in the OBB to a point specified in world space
	void			CalcNearestPoint( const Vector &vecWorldPt, Vector *pVecNearestWorldPt ) const;

	// Computes the distance from a point in world space to the OBB
	float			CalcDistanceFromPoint( const Vector &vecWorldPt ) const;
	float			CalcSqrDistanceFromPoint( const Vector &vecWorldPt ) const;

	// Does a rotation make us need to recompute the surrounding box?
	bool			DoesRotationInvalidateSurroundingBox( ) const;

	// Does VPhysicsUpdate make us need to recompute the surrounding box?
	bool			DoesVPhysicsInvalidateSurroundingBox( ) const;

	// Does a sequence change make us need to recompute the surrounding box?
	bool			DoesSequenceChangeInvalidateSurroundingBox( ) const;

	// Marks the entity has having a dirty surrounding box
	void			MarkSurroundingBoundsDirty();

	// Compute the largest dot product of the OBB and the specified direction vector
	float			ComputeSupportMap( const Vector &vecDirection ) const;

private:
	// Transforms an AABB measured in collision space to a box that surrounds it in world space
	void CollisionAABBToWorldAABB( const Vector &entityMins, const Vector &entityMaxs, Vector *pWorldMins, Vector *pWorldMaxs ) const;

	// Expand trigger bounds..
	void ComputeVPhysicsSurroundingBox( Vector *pVecWorldMins, Vector *pVecWorldMaxs );

	// Expand trigger bounds..
	bool ComputeHitboxSurroundingBox( Vector *pVecWorldMins, Vector *pVecWorldMaxs );
	bool ComputeEntitySpaceHitboxSurroundingBox( Vector *pVecWorldMins, Vector *pVecWorldMaxs );

	// Computes the surrounding collision bounds based on the current sequence box
	void ComputeOBBBounds( Vector *pVecWorldMins, Vector *pVecWorldMaxs );

	// Computes the surrounding collision bounds from the current sequence box
	void ComputeRotationExpandedSequenceBounds( Vector *pVecWorldMins, Vector *pVecWorldMaxs );

	// Computes the surrounding collision bounds based on whatever algorithm we want...
	void ComputeCollisionSurroundingBox( bool bUseVPhysics, Vector *pVecWorldMins, Vector *pVecWorldMaxs );

	// Computes the surrounding collision bounds from the OBB (not vphysics)
	void ComputeRotationExpandedBounds( Vector *pVecWorldMins, Vector *pVecWorldMaxs );

	// Computes the surrounding collision bounds based on whatever algorithm we want...
	void ComputeSurroundingBox( Vector *pVecWorldMins, Vector *pVecWorldMaxs );

	// Check for untouch
	void CheckForUntouch();

	// Updates the spatial partition
	void UpdateServerPartitionMask( );
	uint ComputeServerPartitionMask( );
	inline const Vector&	GetCollisionOrigin_Inline() const;

	// Outer
	CBaseEntity *GetOuter();
	const CBaseEntity *GetOuter() const;

private:
	CBaseEntity *m_pOuter;

// BEGIN PREDICTION DATA COMPACTION (these fields are together to allow for faster copying in prediction system)
	CNetworkVector( m_vecMins, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE, .enc = ks::reflect::WireEnc::Vector } ]] [[= ks::reflect::Proxy<RecvProxy_OBBMins, ks::reflect::WireSide::Recv>{} ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]]);
	CNetworkVector( m_vecMaxs, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE, .enc = ks::reflect::WireEnc::Vector } ]] [[= ks::reflect::Proxy<RecvProxy_OBBMaxs, ks::reflect::WireSide::Recv>{} ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]]);
	CNetworkVar( unsigned short, m_usSolidFlags, [[= ks::reflect::Net{ .bits = FSOLID_MAX_BITS, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Proxy<SendProxy_SolidFlags, ks::reflect::WireSide::Send>{} ]] [[= ks::reflect::Proxy<RecvProxy_SolidFlags, ks::reflect::WireSide::Recv>{} ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]]);
	// One of the SOLID_ defines. Use GetSolid/SetSolid.
	CNetworkVar( unsigned char, m_nSolidType, [[= ks::reflect::Net{ .bits = 3, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "solid" } ]] [[= ks::reflect::Proxy<SendProxy_Solid, ks::reflect::WireSide::Send>{} ]] [[= ks::reflect::Proxy<RecvProxy_Solid, ks::reflect::WireSide::Recv>{} ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]]);			
	CNetworkVar( unsigned char , m_triggerBloat, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );
// END PREDICTION DATA COMPACTION

	float m_flRadius;

	// Spatial partition
	SpatialPartitionHandle_t m_Partition;
	CNetworkVar( unsigned char, m_nSurroundType, [[= ks::reflect::Net{ .bits = SURROUNDING_TYPE_BIT_COUNT, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Proxy<RecvProxy_IntDirtySurround, ks::reflect::WireSide::Recv>{} ]]);

	// SUCKY: We didn't use to have to store this previously
	// but storing it here means that we can network it + avoid a ton of
	// client-side mismatch problems
	CNetworkVector( m_vecSpecifiedSurroundingMins, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVector( m_vecSpecifiedSurroundingMaxs, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE, .enc = ks::reflect::WireEnc::Vector } ]] [[= ks::reflect::Proxy<RecvProxy_VectorDirtySurround, ks::reflect::WireSide::Recv>{} ]]);

	// Cached off world-aligned surrounding bounds
	Vector	m_vecSurroundingMins;
	Vector	m_vecSurroundingMaxs;

	// pointer to the entity's physics object (vphysics.dll)
	//IPhysicsObject	*m_pPhysicsObject;
	
	friend class CBaseEntity;
};

//-----------------------------------------------------------------------------
// For networking this bad boy
//-----------------------------------------------------------------------------
EXTERN_RECV_TABLE( DT_CollisionProperty );

// Purpose: implements various common send proxies

// This converts the int stored in pData to an EHANDLE in pOut.
void RecvProxy_IntToEHandle( const CRecvProxyData *pData, void *pStruct, void *pOut );

void RecvProxy_IntToMoveParent( const CRecvProxyData *pData, void *pStruct, void *pOut );
void RecvProxy_IntToColor32( const CRecvProxyData *pData, void *pStruct, void *pOut );
void RecvProxy_IntSubOne( const CRecvProxyData *pData, void *pStruct, void *pOut );
void RecvProxy_ShortSubOne( const CRecvProxyData *pData, void *pStruct, void *pOut );
void RecvProxy_InterpolationAmountChanged( const CRecvProxyData *pData, void *pStruct, void *pOut );

RecvProp RecvPropTime(
	const char *pVarName,
	int offset, 
	int sizeofVar=kSizeofIgnore );

RecvProp RecvPropEHandle(
	const char *pVarName,
	int offset, 
	int sizeofVar=kSizeofIgnore,
	RecvVarProxyFn proxyFn=RecvProxy_IntToEHandle );

RecvProp RecvPropBool(
	const char *pVarName,
	int offset, 
	int sizeofVar );

RecvProp RecvPropIntWithMinusOneFlag(
	const char *pVarName,
	int offset, 
	int sizeofVar=kSizeofIgnore,
	RecvVarProxyFn proxyFn=RecvProxy_IntSubOne );

// Purpose: Class to help with rendering CMDLs & CMergedMDLs to textures.

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------

class CRenderToRTHelper	: public IRenderToRTHelper
{
public:
	CRenderToRTHelper();
	virtual ~CRenderToRTHelper();

	// Methods of IRenderToRTHelper
	virtual bool Init();
	virtual void Shutdown();
	virtual RenderToRTData_t *CreateRenderToRTData( IRenderToRTHelperObject *pObject, IVTFTexture *pResultVTF );
	virtual void StartRenderToRT( RenderToRTData_t *pRendertoRTData );
	virtual void DestroyRenderToRTData( RenderToRTData_t *pRendertoRTData );
	virtual bool Process();

	void LookAt( Camera_t &camera, const Vector &vecCenter, float flRadius, QAngle cameraAngles, Vector cameraOffset );

private:
	std::vector< RenderToRTData_t * > m_objectsToRender;
	RenderToRTData_t *m_pCurrentObjectToRender;
	MaterialLightingState_t m_LightingState;	

	CThreadEvent *m_pPixelsReadEvent;
	ITexture *m_pRenderTarget;
};

class CBasePlayer;
class CColorCorrection;

// Purpose: Singleton manager for color correction on the client

class CColorCorrectionVolume;

//------------------------------------------------------------------------------
// Purpose : Singleton manager for color correction on the client
//------------------------------------------------------------------------------
struct ClientCCHandle_t__ { int unused; };
using ClientCCHandle_t = ClientCCHandle_t__ *;
inline constexpr auto INVALID_CLIENT_CCHANDLE = ( (ClientCCHandle_t)0 );

class CColorCorrectionMgr : public CBaseGameSystem
{
	// Inherited from IGameSystemPerFrame
public:
	virtual char const *Name() { return "Color Correction Mgr"; }

	// Other public methods
public:
	CColorCorrectionMgr();

	// Create, destroy color correction
	ClientCCHandle_t AddColorCorrection( const char *pName, const char *pFileName = nullptr );
	void RemoveColorCorrection( ClientCCHandle_t );

	ClientCCHandle_t AddColorCorrectionEntity( CColorCorrection *pEntity, const char *pName, const char *pFileName = nullptr );
	void RemoveColorCorrectionEntity( CColorCorrection *pEntity, ClientCCHandle_t );

	ClientCCHandle_t AddColorCorrectionVolume( CColorCorrectionVolume *pVolume, const char *pName, const char *pFileName = nullptr );
	void RemoveColorCorrectionVolume( CColorCorrectionVolume *pVolume, ClientCCHandle_t );

	// Modify color correction weights
	void SetColorCorrectionWeight( ClientCCHandle_t h, float flWeight, bool bExclusive = false );
	void UpdateColorCorrection();
	void ResetColorCorrectionWeights();
	void SetResetable( ClientCCHandle_t h, bool bResetable );

	// Is color correction active?
	bool HasNonZeroColorCorrectionWeights() const;

	ClientCCHandle_t FindColorCorrection( const char *pName );

private:
	int m_nActiveWeightCount;
	bool m_bHaveExclusiveWeight;
	float m_flExclusiveWeight;

	struct SetWeightParams_t
	{
		ClientCCHandle_t handle;
		float flWeight;
		bool bExclusive;
	};

	std::vector< SetWeightParams_t > m_colorCorrectionWeights;

	void CommitColorCorrectionWeights();
};

void UpdateColorCorrectionEntities( CBasePlayer *pPlayer, float ccScale, CColorCorrection **pList, int listCount );
void UpdateColorCorrectionVolumes( CBasePlayer *pPlayer, float ccScale, CColorCorrectionVolume **pList, int listCount );

//------------------------------------------------------------------------------
// Singleton access
//------------------------------------------------------------------------------
extern CColorCorrectionMgr *g_pColorCorrectionMgr;

class CBaseEntity;

// Purpose: Provide custom material swapping for weapons (when switch from world to view or vice versa)

typedef CHandle<CBaseEntity> EHANDLE;

struct CCSPendingCustomMaterialSwap_t
{
	ICustomMaterial *m_pNewCustomMaterial;
	ICustomMaterial *m_pOldCustomMaterial;
	int m_nCustomMaterialIndex;
	EHANDLE m_hOwner;
};

class CCSCustomMaterialSwapManager : public CAutoGameSystem
{
public:
	CCSCustomMaterialSwapManager();
	virtual ~CCSCustomMaterialSwapManager();

	virtual bool Init();
	virtual void Shutdown();

	bool Process();

	void RequestMaterialSwap( EHANDLE hOwner, int nCustomMaterialIndex, ICustomMaterial *pNewCustomMaterialInterface );
	void ClearPendingSwaps( EHANDLE hOwner, int nCustomMaterialIndex );

private:
	void ClearAllPendingSwaps();

	std::vector< CCSPendingCustomMaterialSwap_t > m_pPendingSwaps;
};

extern CCSCustomMaterialSwapManager g_CSCustomMaterialSwapManager;

namespace CCSMFrustumDefinition
{
	enum { NUM_FRUSTUM_VERTS = 8 };
	enum { NUM_FRUSTUM_QUADS = 6 };
	enum { NUM_FRUSTUM_PLANES = NUM_FRUSTUM_QUADS };
	enum { NUM_FRUSTUM_EDGES = 12 };
	enum { NUM_FRUSTUM_LINES = 12 };

	extern Vector4D g_vProjFrustumVerts[NUM_FRUSTUM_VERTS];
	extern const uint8 g_nFrustumLineVertIndices[24];

	// The vertices of each frustum quad
	extern const uint8 g_nQuadVertIndices[NUM_FRUSTUM_QUADS][4];

	// The vertices of each frustum edge
	extern const uint8 g_nEdgeVertIndices[NUM_FRUSTUM_EDGES][2];

	// The quads sharing each frustum edge
	extern const uint8 g_nEdgeQuadIndices[NUM_FRUSTUM_EDGES][2];
} // namespace CCSMFrustumDefinition

enum 
{ 
	// Use caution changing this constant. There are a number of assumptions made about the max # of cascades being 4 (such as the manually laid out shader constants).
	MAX_SUN_LIGHT_SHADOW_CASCADE_SIZE = 4
};

struct ShadowFrustaDebugInfo_t
{
	uint m_nNumWorldFocusVerts;

	enum { MAX_WORLD_FOCUS_POINTS = 9 };
	Vector m_WorldFocusVerts[MAX_WORLD_FOCUS_POINTS];

	float m_flSplitPlaneDistance;

	float m_flLeft, m_flRight;
	float m_flTop, m_flBottom;
	float m_flZNear, m_flZFar;
};

struct SunLightState_t
{
	uint m_nShadowCascadeSize;
       		
	CFrustum m_CascadeFrustums[ MAX_SUN_LIGHT_SHADOW_CASCADE_SIZE ];	
	CVolumeCuller m_CascadeVolumeCullers[ MAX_SUN_LIGHT_SHADOW_CASCADE_SIZE ];
	VMatrix m_CascadeProjToTexMatrices[ MAX_SUN_LIGHT_SHADOW_CASCADE_SIZE ];
	Rect_t m_CascadeViewports[ MAX_SUN_LIGHT_SHADOW_CASCADE_SIZE ];
	VPlane m_CascadeFrustumPlanes[ MAX_SUN_LIGHT_SHADOW_CASCADE_SIZE ][6];
	
	CascadedShadowMappingState_t m_SunLightShaderParams;

	ShadowFrustaDebugInfo_t m_DebugInfo[ MAX_SUN_LIGHT_SHADOW_CASCADE_SIZE ];

	float m_flActualMaxShadowDist;
	float m_flZLerpStartDist;
	float m_flZLerpEndDist;
};

struct SunLightViewState_t
{
	Vector m_Direction;
	color32 m_LightColor;
	int m_LightColorScale;
	CFrustum m_frustum;
	int m_nMaxCascadeSize;
	int m_nAtlasFirstCascadeIndex;
	bool m_bSetAllCascadesToFirst;
	float m_flMaxVisibleDist;
	float m_flMaxShadowDist;
	int m_nCSMQualityLevel;
};

class CCSMParallelSplit : public CAlignedNewDelete<>
{
public:
	CCSMParallelSplit();
	~CCSMParallelSplit();
	
	CCSMParallelSplit( const CCSMParallelSplit &other );
	const CCSMParallelSplit &operator==( const CCSMParallelSplit &rhs );
	
	void Init( uint nMaxShadowBufferSize, uint nMaxCascadeSize );
	void Clear();
	
	bool Update( const SunLightViewState_t &viewState );

	bool IsValid() const { return m_bValid; }
	inline void Reset() { m_bValid = false; }
			
	const SunLightViewState_t& GetViewState() const { return m_viewState; }
	SunLightViewState_t& GetViewState() { return m_viewState; }

	const SunLightState_t &GetLightState() const { return m_lightState; }
	SunLightState_t &GetLightState() { return m_lightState; }
                    		
private:
	
	void CalculateShadowFrustaParallelSplits( const CFrustum &sceneFrustum, uint &nOutCascadeSize, CFrustum *pOutFrustums, CVolumeCuller *pOutVolumeCullers, float &flOutActualMaxShadowDist, int nMaxCascadeSize, Vector vLightDir, float flMaxShadowDistance, float flMaxVisibleDistance, int nCSMQualityLevel, ShadowFrustaDebugInfo_t* pDebugInfo );
	uint ComputeCullingVolumePlanes( VPlane *pOutPlanes, Vector vDirToLight, const CFrustum &sceneFrustum, float flMaxDistBetweenAnyCasterAndReceiver );

	void ComputeCascadeProjToTexMatrices( SunLightState_t &lightState );
		
	static float CalculateSplitPlaneDistance( int nSplit, int nShadowSplits, float flMaxShadowDistance, float flZNear, float flZFar );
	
	float ComputeFarPlaneCameraRelativePoints( Vector *pPoints, float flZNear, float flZFar, const CFrustum &frustum );

	bool m_bValid;		
	
	uint m_nShadowBufferSize;			// resolution of a single shadow map
	uint m_nShadowCascadeMaxSize;		// cascade size (number of shadow maps)

	uint m_nShadowAtlasWidth;			// actual shadow texture resolution
	uint m_nShadowAtlasHeight;
	
	SunLightViewState_t m_viewState;
	
	SunLightState_t m_lightState;
};

class CBaseAnimating;


enum
{
	DEATH_FRAME_HEAD = 1,
	DEATH_FRAME_STOMACH,
	DEATH_FRAME_LEFTARM,
	DEATH_FRAME_RIGHTARM,
	DEATH_FRAME_LEFTLEG,
	DEATH_FRAME_RIGHTLEG,
	MAX_DEATHPOSE_FRAMES  = DEATH_FRAME_RIGHTLEG
};

void GetRagdollCurSequenceWithDeathPose( CBaseAnimating *entity, matrix3x4a_t *curBones, float flTime, int activity, int frame );

class CBaseEntity;


//=============================================================================
// NDebugOverlay
//=============================================================================
namespace NDebugOverlay
{
	void	Box(const Vector &origin, const Vector &mins, const Vector &maxs, int r, int g, int b, int a, float flDuration);
	void	BoxDirection(const Vector &origin, const Vector &mins, const Vector &maxs, const Vector &forward, int r, int g, int b, int a, float flDuration);
	void	BoxAngles(const Vector &origin, const Vector &mins, const Vector &maxs, const QAngle &angles, int r, int g, int b, int a, float flDuration);
	void	SweptBox(const Vector& start, const Vector& end, const Vector& mins, const Vector& maxs, const QAngle & angles, int r, int g, int b, int a, float flDuration);
	void	EntityBounds( const CBaseEntity *pEntity, int r, int g, int b, int a, float flDuration );
	void	Line( const Vector &origin, const Vector &target, int r, int g, int b, bool noDepthTest, float flDuration );
	void	Triangle( const Vector &p1, const Vector &p2, const Vector &p3, int r, int g, int b, int a, bool noDepthTest, float duration );
	void	EntityText( int entityID, int text_offset, const char *text, float flDuration, int r = 255, int g = 255, int b = 255, int a = 255);
	void	EntityTextAtPosition( const Vector &origin, int text_offset, const char *text, float flDuration, int r = 255, int g = 255, int b = 255, int a = 255);
	void	Grid( const Vector &vPosition );
	void	Text( const Vector &origin, const char *text, bool bViewCheck, float flDuration );
	void	ScreenText( float fXpos, float fYpos, const char *text, int r, int g, int b, int a, float flDuration);
	void	Cross3D(const Vector &position, const Vector &mins, const Vector &maxs, int r, int g, int b, bool noDepthTest, float flDuration );
	void	Cross3D(const Vector &position, float size, int r, int g, int b, bool noDepthTest, float flDuration );
	void	Cross3DOriented( const Vector &position, const QAngle &angles, float size, int r, int g, int b, bool noDepthTest, float flDuration );
	void	Cross3DOriented( const matrix3x4_t &m, float size, int c, bool noDepthTest, float flDuration );
	void	DrawOverlayLines(void);
	void	DrawTickMarkedLine(const Vector &startPos, const Vector &endPos, float tickDist, int tickTextDist, int r, int g, int b, bool noDepthTest, float flDuration );
	void	DrawGroundCrossHairOverlay();
	void	HorzArrow( const Vector &startPos, const Vector &endPos, float width, int r, int g, int b, int a, bool noDepthTest, float flDuration);
	void	YawArrow( const Vector &startPos, float yaw, float length, float width, int r, int g, int b, int a, bool noDepthTest, float flDuration);
	void	VertArrow( const Vector &startPos, const Vector &endPos, float width, int r, int g, int b, int a, bool noDepthTest, float flDuration);
	void	Axis( const Vector &position, const QAngle &angles, float size, bool noDepthTest, float flDuration );
	void	Sphere( const Vector &center, float radius, int r, int g, int b, bool noDepthTest, float flDuration );
	void	Circle( const Vector &position, float radius, int r, int g, int b, int a, bool bNoDepthTest, float flDuration );
	void	Circle( const Vector &position, const QAngle &angles, float radius, int r, int g, int b, int a, bool bNoDepthTest, float flDuration );
	void	Circle( const Vector &position, const Vector &xAxis, const Vector &yAxis, float radius, int r, int g, int b, int a, bool bNoDepthTest, float flDuration );
	void	Sphere( const Vector &position, const QAngle &angles, float radius, int r, int g, int b, int a, bool bNoDepthTest, float flDuration );
	void	Cone( const Vector &position, const Vector &axis, float angleRadians, float length, int r, int g, int b, int a, bool bNoDepthTest, float flDuration );
	void	Cross( const Vector &position, float radius, int r, int g, int b, bool bNoDepthTest, float flDuration );
	void	Capsule(const Vector &vStart, const Vector &vEnd, const float &flRadius, int r, int g, int b, int a, float flDuration);
	void	PurgeTextOverlays();
};

// NOTE: If you add a tex type, be sure to modify the s_pImpactEffect
// array in fx_impact.cpp to get an effect when that surface is shot.
inline constexpr auto CHAR_TEX_ANTLION = 'A';
inline constexpr auto CHAR_TEX_BLOODYFLESH = 'B';
inline constexpr auto CHAR_TEX_CONCRETE = 'C';
inline constexpr auto CHAR_TEX_DIRT = 'D';
inline constexpr auto CHAR_TEX_EGGSHELL = 'E'; ///< the egg sacs in the tunnels in ep2.
inline constexpr auto CHAR_TEX_FLESH = 'F';
inline constexpr auto CHAR_TEX_GRATE = 'G';
inline constexpr auto CHAR_TEX_ALIENFLESH = 'H';
inline constexpr auto CHAR_TEX_CLIP = 'I';
inline constexpr auto CHAR_TEX_GRASS = 'J'; // L4D addition
inline constexpr auto CHAR_TEX_SNOW = 'K';
inline constexpr auto CHAR_TEX_PLASTIC = 'L';
inline constexpr auto CHAR_TEX_METAL = 'M';
inline constexpr auto CHAR_TEX_SAND = 'N';
inline constexpr auto CHAR_TEX_FOLIAGE = 'O';
inline constexpr auto CHAR_TEX_COMPUTER = 'P';
inline constexpr auto CHAR_TEX_ASPHALT = 'Q'; // L4D addition
inline constexpr auto CHAR_TEX_BRICK = 'R'; // L4D addition
inline constexpr auto CHAR_TEX_SLOSH = 'S';
inline constexpr auto CHAR_TEX_TILE = 'T';
inline constexpr auto CHAR_TEX_CARDBOARD = 'U'; // L4D addition
inline constexpr auto CHAR_TEX_VENT = 'V';
inline constexpr auto CHAR_TEX_WOOD = 'W';
//#define CHAR_TEX_UNUSED		'X' ///< do not use - "fake" materials use this (ladders, wading, clips, etc)
inline constexpr auto CHAR_TEX_GLASS = 'Y';
inline constexpr auto CHAR_TEX_WARPSHIELD = 'Z'; ///< wierd-looking jello effect for advisor shield.

inline constexpr auto FIRST_CHAR_TEX = CHAR_TEX_ANTLION;
inline constexpr auto LAST_CHAR_TEX = CHAR_TEX_WARPSHIELD;

inline constexpr auto CHAR_TEX_CLAY = 1; // L4D addition
inline constexpr auto CHAR_TEX_PLASTER = 2; // L4D addition
inline constexpr auto CHAR_TEX_ROCK = 3; // L4D addition
inline constexpr auto CHAR_TEX_RUBBER = 4; // L4D addition
inline constexpr auto CHAR_TEX_SHEETROCK = 5; // L4D addition
inline constexpr auto CHAR_TEX_CLOTH = 6; // L4D addition
inline constexpr auto CHAR_TEX_CARPET = 7; // L4D addition
inline constexpr auto CHAR_TEX_PAPER = 8; // L4D addition
inline constexpr auto CHAR_TEX_UPHOLSTERY = 9; // L4D addition
inline constexpr auto CHAR_TEX_PUDDLE = 10; // L4D addition
inline constexpr auto CHAR_TEX_MUD = 11; // L4D addition

inline constexpr auto CHAR_TEX_SANDBARREL = 12; // CSGO addition (no penetration) - sand filled barrel

inline constexpr auto FIRST_L4D_CHAR_TEX = CHAR_TEX_CLAY;
inline constexpr auto LAST_L4D_CHAR_TEX = CHAR_TEX_SANDBARREL;

class IDecalEmitterSystem
{
public:
	virtual int	GetDecalIndexForName( char const *decalname ) = 0;
	virtual char const *TranslateDecalForGameMaterial( char const *decalName, unsigned char gamematerial ) = 0;
};

extern IDecalEmitterSystem *decalsystem;

struct DistanceFadeInfo_t;
class IGameSystem;

// Purpose: Deals with singleton


//-----------------------------------------------------------------------------
// Info used when building lists of detail objects to render
//-----------------------------------------------------------------------------
struct DetailRenderableInfo_t
{
	IClientRenderable *m_pRenderable;
	int m_nLeafIndex;
	RenderGroup_t m_nRenderGroup;
	RenderableInstance_t m_InstanceData;
};

typedef ks::small_vector< DetailRenderableInfo_t, 2048 > DetailRenderableList_t;

//-----------------------------------------------------------------------------
// Responsible for managing detail objects
//-----------------------------------------------------------------------------
class IDetailObjectSystem : public IGameSystem
{
public:
	// How many detail models (as opposed to sprites) are there in the level?
	virtual int GetDetailModelCount() const = 0;

	// Gets a particular detail object
	virtual IClientRenderable* GetDetailModel( int idx ) = 0;

	// Computes the detail prop fade info
	virtual float ComputeDetailFadeInfo( DistanceFadeInfo_t *pInfo ) = 0;

	// Builds a list of renderable info for all detail objects to render
	virtual void BuildRenderingData( DetailRenderableList_t &list, const SetupRenderInfo_t &info, float flDetailDist, const DistanceFadeInfo_t &fadeInfo ) = 0;

	// Call this before rendering translucent detail objects
	virtual void BeginTranslucentDetailRendering( ) = 0;

	// Renders all translucent detail objects in a particular set of leaves
	virtual void RenderTranslucentDetailObjects( const DistanceFadeInfo_t &info, const Vector &viewOrigin, const Vector &viewForward, const Vector &viewRight, const Vector &viewUp, int nLeafCount, LeafIndex_t *pLeafList ) =0;

	// Renders all translucent detail objects in a particular leaf up to a particular point
	virtual void RenderTranslucentDetailObjectsInLeaf( const DistanceFadeInfo_t &info, const Vector &viewOrigin, const Vector &viewForward, const Vector &viewRight, const Vector &viewUp, int nLeaf, const Vector *pVecClosestPoint ) = 0;

};

//-----------------------------------------------------------------------------
// System for dealing with detail objects
//-----------------------------------------------------------------------------
extern IDetailObjectSystem *g_pDetailObjectSystem;
inline IDetailObjectSystem* DetailObjectSystem()
{
	return g_pDetailObjectSystem;
}

struct colorentry_t
{
	unsigned char	index;
	
	unsigned char	r;
	unsigned char	g;
	unsigned char	b;
};

// Commander mode indicators (HL2)
enum
{
	COMMAND_POINT_RED = 0,
	COMMAND_POINT_BLUE,
	COMMAND_POINT_GREEN,
	COMMAND_POINT_YELLOW,
};

// Commander mode table
inline colorentry_t commandercolors[] =
{
	{ COMMAND_POINT_RED,	1,	0,	0	},
	{ COMMAND_POINT_BLUE,	0,	0,	1	},
	{ COMMAND_POINT_GREEN,	0,	1,	0	},
	{ COMMAND_POINT_YELLOW,	1,	1,	0	},
};

inline colorentry_t bloodcolors[] =
{
	{ BLOOD_COLOR_RED,		72,		0,		0	},
	{ BLOOD_COLOR_YELLOW,	195,	195,	0	},
	{ BLOOD_COLOR_MECH,		20,		20,		20	},
	{ BLOOD_COLOR_BRIGHTGREEN,	0,		128,	0	},
};

class CBaseEntity;


	EXTERN_RECV_TABLE( DT_EffectData );

	// Serves the "entindex" prop, which is named by a string rather than by a member and writes
	// m_hEntity itself. An annotation on the class needs it visible here, so it is not static.
	void RecvProxy_EntIndex( const CRecvProxyData *pData, void *pStruct, void *pOut );

inline constexpr auto EFFECTDATA_SERVER_IGNOREPREDICTIONCULL = 0x4;
// NOTE: These flags are specifically *not* networked; so it's placed above the max effect flag bits
inline constexpr auto EFFECTDATA_NO_RECORD = 0x80000000;

inline constexpr auto MAX_EFFECT_FLAG_BITS = 8;

// Note, must match common/qlimits.h!!!
// Must have this value in sync(-1) with qlimits.h and const.h
#define MAX_MODEL_INDEX_BITS	12

inline constexpr auto MAX_EFFECT_DISPATCH_STRING_BITS = 10;
inline constexpr auto MAX_EFFECT_DISPATCH_STRINGS = ( 1 << MAX_EFFECT_DISPATCH_STRING_BITS );

// This is the class that holds whatever data we're sending down to the client to make the effect.
class CEffectData
{
	[[= ks::reflect::NetTable{ .name = "DT_EffectData", .base = false } ]]
	// Six flat props reached through a dotted path: SENDINFO_NOCHECK( m_vOrigin.x ) and
	// RECVINFO( m_vOrigin.x ) both name the prop "m_vOrigin.x" in this table, not a sub-table.
	[[= ks::reflect::From<"m_vOrigin.x", ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD_MP_INTEGRAL }>{} ]]
	[[= ks::reflect::From<"m_vOrigin.y", ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD_MP_INTEGRAL }>{} ]]
	[[= ks::reflect::From<"m_vOrigin.z", ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD_MP_INTEGRAL }>{} ]]
	[[= ks::reflect::From<"m_vStart.x", ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD_MP_INTEGRAL }>{} ]]
	[[= ks::reflect::From<"m_vStart.y", ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD_MP_INTEGRAL }>{} ]]
	[[= ks::reflect::From<"m_vStart.z", ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD_MP_INTEGRAL }>{} ]]
	// "entindex" is not a member: the legacy prop is RecvPropInt( "entindex", 0, ... ) whose
	// proxy writes m_hEntity itself, so it is anchored on m_vOrigin, the member at offset 0.
	[[= ks::reflect::From<"m_vOrigin",
	ks::reflect::Net{ .enc = ks::reflect::WireEnc::Int, .wire = "entindex" },
	RecvProxy_EntIndex>{} ]]
	static constexpr bool ks_class = true;

public:
	Vector m_vOrigin;
	Vector m_vStart;
	[[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NORMAL } ]] Vector m_vNormal;
	// Sent with SendPropQAngles, received with RecvPropQAngles, which is RecvPropVector.
	[[= ks::reflect::Net{ .bits = 7 } ]] QAngle m_vAngles;
	[[= ks::reflect::Net{ .bits = MAX_EFFECT_FLAG_BITS, .flags = SPROP_UNSIGNED } ]] int		m_fFlags;
	ClientEntityHandle_t m_hEntity;
	[[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] float	m_flScale;
	[[= ks::reflect::Net{ .bits = 12, .low = 0.0f, .high = 1023.0f, .flags = SPROP_ROUNDDOWN } ]] float	m_flMagnitude;
	[[= ks::reflect::Net{ .bits = 10, .low = 0.0f, .high = 1023.0f, .flags = SPROP_ROUNDDOWN } ]] float	m_flRadius;
	[[= ks::reflect::Net{ .bits = 5 } ]] int		m_nAttachmentIndex;
	// Send/RecvPropIntWithMinusOneFlag are Send/RecvPropInt wrappers whose whole effect is the
	// default proxy, and this call site overrides it -- with a different function on each side.
	[[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]]
	[[= ks::reflect::Proxy<RecvProxy_ShortSubOne, ks::reflect::WireSide::Recv>{} ]]
	short	m_nSurfaceProp;

	// Some TF2 specific things
	[[= ks::reflect::Net{ .bits = MAX_MODEL_INDEX_BITS, .flags = SPROP_UNSIGNED } ]] int		m_nMaterial;
	[[= ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED } ]] int		m_nDamageType;
	[[= ks::reflect::Net{ .bits = 11, .flags = SPROP_UNSIGNED } ]] int		m_nHitBox;

	[[= ks::reflect::Net{ .bits = MAX_EDICT_BITS, .flags = SPROP_UNSIGNED } ]] int		m_nOtherEntIndex;
	
	[[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] unsigned char	m_nColor;

	[[= ks::reflect::Net{} ]] bool	m_bPositionsAreRelativeToEntity;

// Don't mess with stuff below here. DispatchEffect handles all of this.
public:
	CEffectData();

	int GetEffectNameIndex() { return m_iEffectName; }

	IClientRenderable *GetRenderable() const;
	CBaseEntity *GetEntity() const;
	int entindex() const;

private:

		DECLARE_CLIENTCLASS_NOBASE()

	[[= ks::reflect::Net{ .bits = MAX_EFFECT_DISPATCH_STRING_BITS, .flags = SPROP_UNSIGNED } ]] int m_iEffectName;	// Entry in the EffectDispatch network string table. The is automatically handled by DispatchEffect().
};

bool SuppressingParticleEffects();
void SuppressParticleEffects( bool bSuppress );

#undef MAX_MODEL_INDEX_BITS

class CBaseEntity;

typedef CHandle<CBaseEntity> EHANDLE;

//-----------------------------------------------------------------------------
// Purpose: Used for tracking many to one ground entity chains ( many ents can share a single ground entity )
//-----------------------------------------------------------------------------
struct groundlink_t
{
	EHANDLE					entity;
	groundlink_t			*nextLink;
	groundlink_t			*prevLink;
};

class CHud;

#undef new

//-----------------------------------------------------------------------------
// Purpose: Base class for all hud elements
//-----------------------------------------------------------------------------
class CHudElement : public CGameEventListener
{
public:
	DECLARE_CLASS_NOBASE( CHudElement );
	
	// constructor - registers object in global list
								CHudElement() {}
	explicit 					CHudElement( const char *pElementName );
	// destructor - removes object from the global list
	virtual						~CHudElement();

	virtual void				SetHud( CHud *pHud );

	// called when the Hud is initialised (whenever the DLL is loaded)
	virtual void				Init( void ) { return; }

	// called whenever the video mode changes, and whenever Init() would be called, so the hud can vid init itself
	virtual void				VidInit( void ) { return; }

	// LevelInit's called whenever a new level's starting
	virtual void				LevelInit( void ) { return; };
	// LevelShutdown's called whenever a level's finishing
	virtual void				LevelShutdown( void ) { return; };

	// called whenever the hud receives "reset" message, which is (usually) every time the client respawns after getting killed
	virtual void				Reset( void ) { return; }

	// Called once per frame for visible elements before general key processing
	virtual void				ProcessInput( void ) { return; }

	// Called once per frame whether the element is visible or not
	virtual void				Think( void ) {return;}

	// Called when time warping occurs, i.e. when instant replay rewinds or forwards client's time
	virtual void				OnTimeJump( void ){ return; }

	// 
	virtual const char			*GetName( void ) const { return m_pElementName; };

	// Return true if this hud element should be visible in the current hud state
	virtual bool				ShouldDraw( void );

	virtual bool				IsActive( void ) { return m_bActive; };
	virtual void				SetActive( bool bActive );

	// Hidden bits. 
	// HIDEHUD_ flags that note when this element should be hidden in the HUD
	virtual void				SetHiddenBits( int iBits );

	virtual void				SetIgnoreGlobalHudDisable( bool hide );
	virtual bool				GetIgnoreGlobalHudDisable( void );

	bool						IsParentedToClientDLLRootPanel() const;
	void						SetParentedToClientDLLRootPanel( bool parented );

	// Return true if this HUD element expects an entry in  HudLayout.res
	virtual bool				WantsHudLayoutEntry( void ) const { return true; }

	// memory handling, uses calloc so members are zero'd out on instantiation
    void *operator new( std::size_t stAllocateBlock )	
	{												
		contract_assert( stAllocateBlock != 0 );				
		void *pMem = MemAlloc_Alloc( stAllocateBlock );
		std::memset( pMem, 0, stAllocateBlock );
		return pMem;												
	}
	
	void* operator new( std::size_t stAllocateBlock, int nBlockUse, const char *pFileName, int nLine )  
	{ 
		contract_assert( stAllocateBlock != 0 );
		void *pMem = MemAlloc_Alloc( stAllocateBlock, pFileName, nLine );
		std::memset( pMem, 0, stAllocateBlock );
		return pMem;												
	}

	void operator delete( void *pMem )				
	{												
		g_pMemAlloc->Free( pMem );								
	}

	void operator delete( void *pMem, int nBlockUse, const char *pFileName, int nLine )				
	{
		operator delete( pMem );
	}

	void SetNeedsRemove( bool needsremove );

	void RegisterForRenderGroup( const char *pszName );
	void UnregisterForRenderGroup( const char *pszGroupName );
	void HideLowerPriorityHudElementsInGroup( const char *pszGroupName );
	void UnhideLowerPriorityHudElementsInGroup( const char *pszGroupName );

	// For now, CHUdElements declare a single priority value. They will only be hidden 
	// by panels with a lower priority and will only lock out panels with a lower priority
	virtual int	GetRenderGroupPriority();

	void SetSplitScreenPlayerSlot( int nSlot );
	int GetSplitScreenPlayerSlot() const;

	virtual void OnSplitScreenStateChanged() {}

public: // Game event handler
	
	void FireGameEvent( IGameEvent * event ) {}

protected:
	void InitCHudElementAfterConstruction( const char* pElementName );

public:

	// True if this element is visible, and should think
	bool						m_bActive;
	
	// m_bWantLateUpdate defaults to false. Set this to true if you need to position something based on the 
	// position of a player or other moving game actor. 
	// When true, your Think and ProcessInput functions will be called after the simulation updates this means 
	// that the Flash VM will not have a chance to run until after the frame has been rendered. This is 
	//probably not a big deal, but is good to know.
    bool                        m_bWantLateUpdate;

protected:
	int							m_iHiddenBits;
	int							m_nSplitScreenPlayerSlot;
	bool						m_ignoreGlobalHudDisable;

private:
	const char					*m_pElementName;
	bool						m_bNeedsRemove;
	bool						m_bIsParentedToClientDLLRootPanel;

	std::vector< int >			m_HudRenderGroups;
	CHud						*m_pHud;
};

inline bool RenderGroupLessFunc( CHudElement * const &lhs, CHudElement * const &rhs )
{
	return ( lhs->GetRenderGroupPriority() < rhs->GetRenderGroupPriority() );
}

// hud elements declare themselves to be part of a hud render group, by name
// we register with each hudelement a list of indeces of groups they are in
// then they can query by index the state of their render group
class CHudRenderGroup
{
public:
	CHudRenderGroup()
	{
		bHidden = false;
	}

	bool bHidden;
	std::vector< CHudElement * >	m_pLockingElements;	// heap by RenderGroupLessFunc, highest priority first
};

class CHud;

class CHudElement;
class CHudRenderGroup;

// Purpose: CHud handles the message, calculation, and drawing the HUD

// basic rectangle struct used for drawing
typedef struct wrect_s
{
	int	left;
	int right;
	int top;
	int bottom;
} wrect_t;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CHudTexture
{
public:
	CHudTexture();

	CHudTexture& operator =( const CHudTexture& src )
	{
		if ( this == &src )
			return *this;

		V_strncpy( szShortName, src.szShortName, sizeof( szShortName ) );
		V_strncpy( szTextureFile, src.szTextureFile, sizeof( szTextureFile ) );
		V_memcpy( texCoords, src.texCoords, sizeof( texCoords ) );
		textureId = src.textureId;
		rc = src.rc;
		bRenderUsingFont = src.bRenderUsingFont;
		cCharacterInFont = src.cCharacterInFont;

		return *this;
	}

	int Width() const
	{
		return rc.right - rc.left;
	}

	int Height() const
	{
		return rc.bottom - rc.top;
	}

	// causes the font manager to generate the glyph, prevents run time hitches on platforms that have slow font managers
	void Precache( void );

	// returns width & height of icon with scale applied (scale is ignored if font is used to render)
	int EffectiveWidth( float flScale ) const;
	int EffectiveHeight( float flScale ) const;

	void DrawSelf( int x, int y, const Color& clr, float flApparentZ = 1.0f ) const;
	void DrawSelf( int x, int y, int w, int h, const Color& clr, float flApparentZ = 1.0f ) const;
	void DrawSelfCropped( int x, int y, int cropx, int cropy, int cropw, int croph, Color clr, float flApparentZ = 1.0f ) const;
	// new version to scale the texture over a finalWidth and finalHeight passed in
	void DrawSelfCropped( int x, int y, int cropx, int cropy, int cropw, int croph, int finalWidth, int finalHeight, Color clr, float flApparentZ = 1.0f ) const;
	void DrawSelfScalableCorners( int x, int y, int w, int h, int iSrcCornerW, int iSrcCornerH, int iDrawCornerW, int iDrawCornerH, Color clr, float flApparentZ = 1.0f ) const;

	char		szShortName[ 64 ];
	char		szTextureFile[ 64 ];

	bool		bRenderUsingFont;
	bool		bPrecached;
	char		cCharacterInFont;

	// vgui texture Id assigned to this item
	int			textureId;
	// s0, t0, s1, t1
	float		texCoords[ 4 ];

	// Original bounds
	wrect_t		rc;
};


//-----------------------------------------------------------------------------
// Purpose: Main hud manager
//-----------------------------------------------------------------------------
class CHud 
{
public:
	//For progress bar orientations
	static const int			HUDPB_HORIZONTAL;
	static const int			HUDPB_VERTICAL;
	static const int			HUDPB_HORIZONTAL_INV;

public:
								CHud();
								~CHud();

	// Init's called when the HUD's created at DLL load
	void						Init( void );
	// VidInit's called when the video mode's changed
	void						VidInit( void );
	// Shutdown's called when the engine's shutting down
	void						Shutdown( void );
	// LevelInit's called whenever a new level's starting
	void						LevelInit( void );
	// LevelShutdown's called whenever a level's finishing
	void						LevelShutdown( void );
	
	void						ResetHUD( void );

	// A saved game has just been loaded
	void						OnRestore();

	//							called during simulation, Players and other moving actors
	// 							may not be in their final position when this is called
	void						Think( void );
	
	//							called just before rendering, Players will be in the right
	//							position. Also, late think is only called when in the game.
    void                        LateThink( void );

	void						ProcessInput( bool bActive );
	void						OnTimeJump();
	void						UpdateHud( bool bActive );

	// Hud element registration
	void						AddHudElement( CHudElement *pHudElement );
	void						RemoveHudElement( CHudElement *pHudElement );
	// Search list for "name" and return the hud element if it exists
	CHudElement					*FindElement( const char *pName );
	
	bool						IsHidden( int iHudFlags );

	float						GetSensitivity();
	float						GetFOVSensitivityAdjust();

	void						DrawProgressBar( int x, int y, int width, int height, float percentage, Color& clr, unsigned char type );
	void						DrawIconProgressBar( int x, int y, CHudTexture *icon, CHudTexture *icon2, float percentage, Color& clr, int type );

	// User messages
	bool						MsgFunc_ResetHUD( const ks::net::CCSUsrMsg_ResetHud& msg );
	bool 						MsgFunc_SendAudio( const ks::net::CCSUsrMsg_SendAudio& msg );

	// Hud Render group
	int							LookupRenderGroupIndexByName( const char *pszGroupName );
	bool						LockRenderGroup( int iGroupIndex, CHudElement *pLocker = nullptr );
	bool						UnlockRenderGroup( int iGroupIndex, CHudElement *pLocker = nullptr );
	bool						IsRenderGroupLockedFor( CHudElement *pHudElement, int iGroupIndex );
	int							RegisterForRenderGroup( const char *pszGroupName );
	int							AddHudRenderGroup( const char *pszGroupName );
	bool						DoesRenderGroupExist( int iGroupIndex );

	void						SetScreenShotTime( float flTime ){ m_flScreenShotTime = flTime; }

	std::vector< CHudElement * > &GetHudList();
	const std::vector< CHudElement * > &GetHudList() const;
	void						OnSplitScreenStateChanged();

	void						DisableHud( void );
	void						EnableHud( void );
	bool						HudDisabled( void );

public:

	int							m_iKeyBits;

	float						m_flMouseSensitivity;
	float						m_flMouseSensitivityFactor;

	float						m_flFOVSensitivityAdjust;

	Color						m_clrNormal;
	Color						m_clrCaution;
	Color						m_clrYellowish;

	std::vector< CHudElement * >	m_HudList;

private:
	void						DoElementThink( CHudElement* pElement );

	std::vector< const char * >				m_RenderGroupNames;
	std::map< int, std::unique_ptr< CHudRenderGroup > >	m_RenderGroups;

	float						m_flScreenShotTime; // used to take end-game screenshots
	int							m_nSplitScreenSlot;
	bool						m_bEngineIsInGame;
	int							m_iDisabledCount;
};

CHud &GetHud( int nSlot = -1 );

class CHudIcons
{
public:
	CHudIcons();
	~CHudIcons();

	void						Init();
	void						Shutdown();

	CHudTexture					*GetIcon( const char *szIcon );

	// loads a new icon into the list, without duplicates
	CHudTexture					*AddUnsearchableHudIconToList( CHudTexture& texture );
	CHudTexture					*AddSearchableHudIconToList( CHudTexture& texture );

	void						RefreshHudTextures();

private:

	void						SetupNewHudTexture( CHudTexture *t );
	bool						m_bHudTexturesLoaded;
	// Global list of known icons; allocated from and freed to the hud texture pool
	ks::idict< CHudTexture * >		m_Icons;

};

CHudIcons &HudIcons();

// Texture scripts may repeat a name (across files); equal names keep load order.
typedef std::multimap< std::string, std::unique_ptr< CHudTexture >, ks::iless > CHudTextureList;
void LoadHudTextures( CHudTextureList& list, const char *szFilenameWithoutExtension, const unsigned char *pICEKey );

void GetHudSize( int& w, int &h );

class CHudElement;

inline constexpr auto CHATLINE_NUM_FLASHES = 8.0f;
inline constexpr auto CHATLINE_FLASH_TIME = 5.0f;
inline constexpr auto CHATLINE_FADE_TIME = 1.0f;

#define CHAT_HISTORY_ONE_OVER_FADE_TIME 4.0f;
inline constexpr auto CHAT_HISTORY_FADE_TIME = 0.25f;
inline constexpr auto CHAT_HISTORY_IDLE_TIME = 15.0f;
inline constexpr auto CHAT_HISTORY_IDLE_FADE_TIME = 2.5f;
inline constexpr auto CHAT_HISTORY_ALPHA = 127;

extern Color g_ColorBlue;
extern Color g_ColorRed;
extern Color g_ColorGreen;
extern Color g_ColorDarkGreen;
extern Color g_ColorYellow;
extern Color g_ColorGrey;

extern ConVar cl_showtextmsg;

enum ChatFilters
{
	CHAT_FILTER_NONE		= 0,
	CHAT_FILTER_JOINLEAVE	= 0x000001,
	CHAT_FILTER_NAMECHANGE	= 0x000002,
	CHAT_FILTER_PUBLICCHAT	= 0x000004,
	CHAT_FILTER_SERVERMSG	= 0x000008,
	CHAT_FILTER_TEAMCHANGE	= 0x000010,
	CHAT_FILTER_ACHIEVEMENT	= 0x000020,
};

//-----------------------------------------------------------------------------
enum TextColor
{
	COLOR_NORMAL =			1,
	COLOR_USEOLDCOLORS =	2,
	COLOR_PLAYERNAME =		3,
	COLOR_LOCATION =		4,
	COLOR_ACHIEVEMENT =		5,
	COLOR_AWARD =			6,
	COLOR_PENALTY =			7,
	COLOR_SILVER =			8,
	COLOR_GOLD =			9,

	COLOR_RARITY_FIRST =	10,
	COLOR_COMMON =			COLOR_RARITY_FIRST,
	COLOR_UNCOMMON =		11,
	COLOR_RARE =			12,
	COLOR_MYTHICAL =		13,
	COLOR_LEGENDARY =		14,
	COLOR_ANCIENT =			15,
	COLOR_IMMORTAL =		16,

	COLOR_RARITY_LAST =		COLOR_IMMORTAL,

	COLOR_MAX
};

//--------------------------------------------------------------------------------------------------------------
struct TextRange
{
	int start;
	int end;
	Color color;
};

void StripEndNewlineFromString( char *str );
void StripEndNewlineFromString( wchar_t *str );

char* ConvertCRtoNL( char *str );
wchar_t* ConvertCRtoNL( wchar_t *str );
wchar_t* ReadLocalizedString( const char *szString, wchar_t *pOut, int outSizeInBytes, bool bStripNewline, char *originalString = nullptr, int originalSize = 0 );
wchar_t* ReadChatTextString( const char *szString, wchar_t *pOut, int outSizeInBytes, bool stripBugData = false );
char* RemoveColorMarkup( char *str );

//--------------------------------------------------------------------------------------------------------
/**
 * Simple utility function to allocate memory and duplicate a wide string
 */
inline wchar_t *CloneWString( const wchar_t *str )
{
	wchar_t *cloneStr = new wchar_t [ wcslen(str)+1 ];
	wcscpy( cloneStr, str );
	return cloneStr;
}

class CBaseHudChatLine
{
};

class CHudChatHistory
{
};

class CBaseHudChatEntry
{
};

class CBaseHudChatInputLine
{
};

class CHudChatFilterButton
{
};

class CHudChatFilterPanel
{
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CBaseHudChat : public CHudElement
{
	typedef CHudElement BaseClass;
public:
	DECLARE_MULTIPLY_INHERITED();

	enum
	{
		CHAT_INTERFACE_LINES = 6,
		MAX_CHARS_PER_LINE = 128
	};

	explicit CBaseHudChat( const char *pElementName );
	~CBaseHudChat();

	static CBaseHudChat *GetHudChat( void );

	virtual void	CreateChatInputLine( void );
	virtual void	CreateChatLines( void );
	
	virtual void	Init( void );

	void			LevelInit( const char *newmap );
	void			LevelShutdown( void );

	void			MsgFunc_TextMsg(const char *pszName, int iSize, void *pbuf);
	
	virtual void	Printf( int iFilter, const char *fmt, ... );
	[[gnu::format(printf, 4, 5)]] virtual void	ChatPrintf( int iPlayerIndex, int iFilter, const char *fmt, ... );
	virtual void	ChatPrintfW( int iPlayerIndex, int iFilter, const wchar_t *wszNotice );
	
	virtual void	StartMessageMode( int iMessageModeType );
	virtual void	StopMessageMode( bool bFade = true );
	void			Send( void );

	virtual void	Reset();

	static int		m_nLineCounter;

	virtual int		GetChatInputOffset( void );

	// Game event handler
	void FireGameEvent( IGameEvent *event);

	CHudChatHistory			*GetChatHistory();

	void					FadeChatHistory();
	float					m_flHistoryFadeTime;
	float					m_flHistoryIdleTime;

	virtual bool			MsgFunc_SayText( const ks::net::CCSUsrMsg_SayText &msg );
	virtual bool			MsgFunc_SayText2( const ks::net::CCSUsrMsg_SayText2 &msg );
	virtual bool			MsgFunc_TextMsg( const ks::net::CCSUsrMsg_TextMsg &msg );
	virtual void			MsgFunc_VoiceSubtitle( bf_read &msg );

	CBaseHudChatInputLine	*GetChatInput( void ) { return m_pChatInput; }
	CHudChatFilterPanel		*GetChatFilterPanel( void );

	virtual int				GetFilterFlags( void ) { return m_iFilterFlags; }
	void					SetFilterFlag( int iFilter );

	virtual void		SetChatPrompt( int iMessageModeType );

	//-----------------------------------------------------------------------------
	virtual Color	GetDefaultTextColor( void );
	virtual Color	GetTextColorForClient( TextColor colorNum, int clientIndex );
	virtual Color	GetClientColor( int clientIndex );

	virtual int		GetFilterForString( const char *pString );

	virtual const char *GetDisplayedSubtitlePlayerName( int clientIndex );

	bool			IsVoiceSubtitle( void ) { return m_bEnteringVoice; }
	void			SetVoiceSubtitleState( bool bState ) { m_bEnteringVoice = bState; }

protected:
	CBaseHudChatLine		*FindUnusedChatLine( void );

	CBaseHudChatInputLine	*m_pChatInput;
	CBaseHudChatLine		*m_ChatLine;
	int						m_iFontHeight;

	CHudChatHistory			*m_pChatHistory;

	CHudChatFilterButton	*m_pFiltersButton;
	CHudChatFilterPanel		*m_pFilterPanel;

private:	
	void			Clear( void );

	int				m_nMessageMode;

	int				m_iFilterFlags;
	bool			m_bEnteringVoice;
};

#undef CHAT_HISTORY_ONE_OVER_FADE_TIME


class CHudElement;

// Purpose: Helper for the CHudElement class to add themselves to the list of hud elements


inline constexpr auto HUDELEMENT_DEFAULT = 0; // default, hud elements present in all split contexts
inline constexpr auto HUDELEMENT_SS_FULLSCREEN_ONLY = 1; // hud element only created in fullscreen context

//-----------------------------------------------------------------------------
// Purpose: Used by DECLARE_HUDELEMENT macro to create a linked list of
//  instancing functions
//-----------------------------------------------------------------------------
class CHudElementHelper
{
public:
	// Static list of helpers
	static CHudElementHelper *m_sHelpers;
	// Create all the hud elements
	static void CreateAllElements( void );

public:
	// Construction
	CHudElementHelper( CHudElement *( *pfnCreate )( void ), int depth, int flags = HUDELEMENT_DEFAULT );

	// Accessors
	CHudElementHelper *GetNext( void );

private:
	// Next factory in list
	CHudElementHelper	*m_pNext;
	// Creation function to use for this technology
	CHudElement			*( *m_pfnCreate )( void );

	//Depth used to determine hud panel ordering
	int					m_iDepth;

	// Flags for visibility of hud elements in splitscreen
	int					m_iFlags;
};

// This is the macro which implements creation of each hud element
// It creates a function which instances an object of the specified type
// It them hooks that function up to the helper list so that the CHud objects can create
//  the elements by name, with no header file dependency, etc.

#define DECLARE_NAMED_HUDELEMENT( className, panelName )						\
	static CHudElement *Create_##panelName( void )							\
		{																		\
			return new className( #panelName );									\
		};																		\
	static CHudElementHelper g_##panelName##_Helper( Create_##panelName, 50 );

// Versions with flags
#define DECLARE_HUDELEMENT_FLAGS( className, flags )							\
	static CHudElement *Create_##className( void )							\
		{																		\
			return new className( #className );									\
		};																		\
	static CHudElementHelper g_##className##_Helper( Create_##className, 50, flags );

#define DECLARE_HUDELEMENT_DEPTH_FLAGS( className, depth, flags )				\
	static CHudElement *Create_##className( void )							\
{																		\
	return new className( #className );									\
};																		\
	static CHudElementHelper g_##className##_Helper( Create_##className, depth );

#define DECLARE_NAMED_HUDELEMENT_FLAGS( className, panelName, flags )			\
	static CHudElement *Create_##panelName( void )							\
{																		\
	return new className( #panelName );									\
};																		\
	static CHudElementHelper g_##panelName##_Helper( Create_##panelName, 50 );

// This macro can be used to get a pointer to a specific hud element

#define GET_NAMED_HUDELEMENT( className, panelName )							\
	(className*)GetHud().FindElement( #panelName )

#define GET_FULLSCREEN_HUDELEMENT( className )										\
	((className*)GetHud( 0 ).FindElement( #className ))

// Things that inherit from a panel, too, will have ambiguous new operators
//  so this should disambiguate them

// Alias for base hud element

#undef DECLARE_NAMED_HUDELEMENT
#undef DECLARE_HUDELEMENT_FLAGS
#undef DECLARE_HUDELEMENT_DEPTH_FLAGS
#undef DECLARE_NAMED_HUDELEMENT_FLAGS
#undef GET_NAMED_HUDELEMENT
#undef GET_FULLSCREEN_HUDELEMENT

class CHudTexture;

// Purpose: Add entities to this system, and the Locator will maintain an arrow
//			on the HUD that points to the entities when they are offscreen.


inline constexpr auto MAX_LOCATOR_BINDINGS_SHOWN = 8;
inline constexpr auto MAX_LOCATOR_TARGETS = 10;
inline constexpr auto LOCATOR_FLAGS_NONE = 0x00000000;

inline constexpr auto LOCATOR_ICON_FX_NONE = 0x00000000;
inline constexpr auto LOCATOR_ICON_FX_PULSE_SLOW = 0x00000001;
inline constexpr auto LOCATOR_ICON_FX_PULSE_FAST = 0x00000002;
inline constexpr auto LOCATOR_ICON_FX_PULSE_URGENT = 0x00000004;
inline constexpr auto LOCATOR_ICON_FX_ALPHA_SLOW = 0x00000008;
inline constexpr auto LOCATOR_ICON_FX_ALPHA_FAST = 0x00000010;
inline constexpr auto LOCATOR_ICON_FX_ALPHA_URGENT = 0x00000020;
inline constexpr auto LOCATOR_ICON_FX_SHAKE_NARROW = 0x00000040;
inline constexpr auto LOCATOR_ICON_FX_SHAKE_WIDE = 0x00000080;
inline constexpr auto LOCATOR_ICON_FX_STATIC = 0x00000100; // This icon draws at a fixed location on the HUD.
inline constexpr auto LOCATOR_ICON_FX_NO_OFFSCREEN = 0x00000200;
inline constexpr auto LOCATOR_ICON_FX_FORCE_CAPTION = 0x00000400; // Always draw the caption, even when the icon is occluded.
inline constexpr auto LOCATOR_ICON_FX_FADE_OUT = 0x00000800; // Set when deactivated so it can smoothly vanish
inline constexpr auto LOCATOR_ICON_FX_FADE_IN = 0x00001000; // Set when activated so it can smoothly appear
inline constexpr auto LOCATOR_ICON_FX_SCALE_BY_DIST = 0x00002000; // Scales the icon in the world by how far away it is
inline constexpr auto LOCATOR_ICON_FX_SCALE_LARGE = 0x00004000; // Scales the icon triple the normal size

// Interned game instructor string; equal strings share storage, so == compares pointers.
class CGameInstructorSymbol
{
public:
	CGameInstructorSymbol() = default;
	CGameInstructorSymbol( const char *pStr );
	bool operator==( const CGameInstructorSymbol &src ) const	{ return m_pString == src.m_pString; }
	const char *String() const									{ return m_pString ? m_pString : ""; }

private:
	const char *m_pString = nullptr;
};

//-----------------------------------------------------------------------------
// This class represents a single target to be tracked by the locator
//-----------------------------------------------------------------------------
class CLocatorTarget
{
public:
	bool		m_bOriginInScreenspace;
	Vector		m_vecOrigin;			// The location in the world to draw on the locator

	// ONLY the locator panel should fiddle with these fields.
	bool		m_isActive;		
	int			m_serialNumber;
	int			m_frameLastUpdated;
	bool		m_bOnscreen;
	bool		m_bOccluded;
	bool		m_bVisible;
	bool		m_bIsDrawing;
	bool		m_bIconNoTarget;
	float		m_distFromPlayer;
	CHudTexture	*m_pIcon_onscreen;
	CHudTexture	*m_pIcon_offscreen;
	int			m_iBindingTick;
	float		m_flNextBindingTick;
	float		m_flNextOcclusionTest;
	int			m_iBindingChoicesCount;
	const char	*(m_pchBindingChoices[ MAX_LOCATOR_BINDINGS_SHOWN ]);
	int			m_iBindChoicesOriginalToken[ MAX_LOCATOR_BINDINGS_SHOWN ];

	// Fields for drawing
	int			m_targetX;				// screen X position of the actual target
	int			m_targetY;				// screen Y position of the actual target
	int			m_iconX;				// screen X position (top)
	int			m_iconY;				// screen Y position (left)
	int			m_centerX;				// screen X position (center)
	int			m_centerY;				// screen Y position (center)
	int			m_wide;					// draw width of icon (may be different from frame to frame as the icon's size animates, for instance)
	int			m_tall;					// draw height of icon  ''			''
	float		m_widthScale_onscreen;	// for icons that are wider than standard
	int			m_alpha;				// 
	float		m_fadeStart;			// time stamp when fade out started
	float		m_lerpStart;			// time stamp when lerping started
	float		m_pulseStart;			// time stamp when pulsing started
	int			m_declutterIndex;		// sort order from the declutterer
	int			m_lastDeclutterIndex;	// last sort order from the declutterer
	bool		m_bDrawArrow;			// Whether to draw an arrow indicating this target is off-screen
	float		m_fDrawArrowAngle;		// Direction of the offscreen arrow (up, left, etc.)
	int			m_captionWide;			// How wide (pixels) my caption is.
	bool		m_bDrawControllerButton;
	bool		m_bDrawControllerButtonOffscreen;
	int			m_offsetX;				// User-specified X offset which is applied in screenspace
	int			m_offsetY;				// User-specified Y offset which is applied in screenspace

	// Fields for interpolating icon position
	float		m_flTimeLerpDone;		// How much time left before this icon arrives where it is supposed to be.
	int			m_lastXPos;				// screen X position last frame
	int			m_lastYPos;				// ''     Y

	CLocatorTarget( void );
	void Activate( int serialNumber );
	void Deactivate( bool bNoFade = false );
	void Update();

	int GetIconX( void );
	int GetIconY( void );
	int GetIconCenterX( void );
	int GetIconCenterY( void );
	int GetIconWidth( void );
	int GetIconHeight( void );

	void AddIconEffects( int add )			{ m_iEffectsFlags |= add; }
	void RemoveIconEffects( int remove )	{ m_iEffectsFlags &= ~remove; }
	int GetIconEffectsFlags()				{ return m_iEffectsFlags; }
	bool IsEffectActive( int f )			{ return ( m_iEffectsFlags & f ) != 0; }
	void SetIconColor( Color color )		{ m_rgbaIconColor = color; }
	Color GetIconColor( void ) const		{ return m_rgbaIconColor; }
	void SetCaptionColor( Color col )		{ m_captionColor = col; }
	void SetCaptionColor( const char *pszCaptionColor );
	bool IsStatic();
	bool IsPresenting();
	void StartTimedLerp();
	void StartPresent();
	void EndPresent();

	void UpdateVguiTarget( void );
	void *GetVguiTarget( void );
	void SetVguiTargetName( const char *pchUITargetName );
	const char *GetVguiTargetName( void ) { return m_szUITargetName.String(); }
	void SetVguiTargetLookup( const char *pchUITargetLookup );
	const char *GetVguiTargetLookup( void ) { return m_szUITargetLookup.String(); }
	void SetVguiTargetEdge( int nVguiEdge );
	int GetVguiTargetEdge( void ) const { return m_nUITargetEdge; }

	void SetOnscreenIconTextureName( const char *pszTexture );
	void SetOffscreenIconTextureName( const char *pszTexture );
	void SetBinding( const char *pszBinding );
	const char *UseBindingImage( char *pchIconTextureName, size_t bufSize );
	void SetIconNoTarget( bool bIconNoTarget ) { m_bIconNoTarget = bIconNoTarget; }

	const char *GetOnscreenIconTextureName()	{ return m_szOnscreenTexture.String(); }
	const char *GetOffscreenIconTextureName()	{ return m_szOffscreenTexture.String(); }
	const char *GetBinding()			{ return m_szBinding.String(); }

	void SetVisible( bool bVisible );
	bool IsVisible( void );

	void SetCaptionText( const char *pszText, const char *pszParam );
	const wchar_t *GetCaptionText( void )	{ return (const wchar_t *)m_wszCaption.data(); }
	bool HasCaptionText( void )			{ return std::ssize( m_wszCaption ) > 1; }

	void DrawBindingName( const char *pchDrawName )		{ m_pchDrawBindingName = pchDrawName; }
	void DrawBindingNameOffscreen( const char *pchDrawName )	{ m_pchDrawBindingNameOffscreen = pchDrawName; }

	const char *DrawBindingName( void )				{ return m_pchDrawBindingName; }
	const char *DrawBindingNameOffscreen( void )	{ return m_pchDrawBindingNameOffscreen; }

	bool IsOnScreen()	{ return m_bOnscreen; }
	bool IsOccluded()	{ return m_bOccluded; }

private:
	CGameInstructorSymbol		m_szUITargetName;
	CGameInstructorSymbol		m_szUITargetLookup;
	int							m_nUITargetEdge;

	CGameInstructorSymbol	m_szOnscreenTexture;
	CGameInstructorSymbol	m_szOffscreenTexture;
	CGameInstructorSymbol	m_szBinding;

	bool		m_bWasControllerLast;
	const char	*m_pchDrawBindingName;
	const char	*m_pchDrawBindingNameOffscreen;
	int			m_iEffectsFlags;
	Color		m_rgbaIconColor;
	std::vector< wchar_t > m_wszCaption;

public:
	Color		m_captionColor;
};

extern int Locator_AddTarget();
extern void Locator_RemoveTarget( int hTarget );
CLocatorTarget *Locator_GetTargetFromHandle( int hTarget );
void Locator_ComputeTargetIconPositionFromHandle( int hTarget );

class CHudElement;

// Macros to hook function calls into the HUD object

#define HOOK_HUD_MESSAGE_REALTIME_PASSTHROUGH(y, x) \
	m_UMCMsg##x.BindRealtimePassthrough< ks::net::CS_UM_##x, ks::net::CCSUsrMsg_##x >( __MsgFunc_##y##_##x )

// Message declaration for non-CHudElement classes
#define DECLARE_MESSAGE(y, x) bool __MsgFunc_##y##_##x(const ks::net::CCSUsrMsg_##x &msg) \
	{							\
		return y.MsgFunc_##x( msg );	\
	}

// Message declaration for CHudElement classes that use the hud element factory for creation

// Commands
// Command declaration for non CHudElement classes
#define DECLARE_COMMAND(y, x) void __CmdFunc_##x( void ) \
	{							\
		y.UserCmd_##x( );		\
	}
// Command declaration for CHudElement classes that use the hud element factory for creation
#define DECLARE_HUD_COMMAND(y, x) void __CmdFunc_##x( void )									\
	{																\
		CHudElement *pElement = GetHud().FindElement( #y );				\
		if ( pElement )												\
		{															\
			((y *)pElement)->UserCmd_##x( );						\
		}															\
	}

#undef HOOK_HUD_MESSAGE_REALTIME_PASSTHROUGH
#undef DECLARE_MESSAGE
#undef DECLARE_COMMAND
#undef DECLARE_HUD_COMMAND

class CHudTexture;


class CHudTextureHandle
{
public:
	CHudTextureHandle()
	{
		m_pValue = nullptr;
	}

	// Assign a value to the handle.
	const CHudTextureHandle& operator=( const CHudTexture *t )
	{
		m_pValue = const_cast<CHudTexture *>(t);
		return *this;
	}

	void Set( CHudTexture *t )
	{
		m_pValue = t;
	}

	CHudTexture *Get()
	{
		return m_pValue;
	}

	operator CHudTexture*()
	{
		return m_pValue;
	}

	operator CHudTexture*() const
	{
		return m_pValue;
	}

	CHudTexture	*operator->() const
	{
		return m_pValue;
	}

private:
	CHudTexture *m_pValue;
};

class CBaseEntity;

inline constexpr int MAX_DEPTH_TEXTURE_SHADOWS = 1;
inline constexpr int MAX_DEPTH_TEXTURE_HIGHRES_SHADOWS = 0;

inline constexpr int MAX_DEPTH_TEXTURE_SHADOWS_TOOLS = 8;
inline constexpr int MAX_DEPTH_TEXTURE_HIGHRES_SHADOWS_TOOLS = 0;

// These are set as it draws reflections, refractions, etc, so certain effects can avoid 
// drawing themselves in reflections.
enum DrawFlags_t
{
	DF_RENDER_REFRACTION	= 0x1,
	DF_RENDER_REFLECTION	= 0x2,

	DF_CLIP_Z				= 0x4,
	DF_CLIP_BELOW			= 0x8,

	DF_RENDER_UNDERWATER	= 0x10,
	DF_RENDER_ABOVEWATER	= 0x20,
	DF_RENDER_WATER			= 0x40,

	DF_SSAO_DEPTH_PASS		= 0x80,

	DF_RENDER_PSEUDO_TRANSLUCENT_WATER = 0x100, // Pseudo-translucent water is water that renders after all opaques but before all transparents, writes to depth, and uses alpha blending
	DF_WATERHEIGHT			= 0x200,
	DF_DRAW_SSAO			= 0x400,
	DF_DRAWSKYBOX			= 0x800,

	DF_FUDGE_UP				= 0x1000,

	DF_DRAW_ENTITITES		= 0x2000,

	DF_SKIP_WORLD			= 0x4000,
	DF_SKIP_WORLD_DECALS_AND_OVERLAYS	= 0x8000,

	DF_UNUSED5				= 0x10000,
	DF_SAVEGAMESCREENSHOT	= 0x20000,
	DF_CLIP_SKYBOX			= 0x40000,

	DF_DRAW_SIMPLE_WORLD_MODEL = 0x80000,	// Draw a singe studio model for the world to save CPU.

	DF_SHADOW_DEPTH_MAP		= 0x100000,	// Currently rendering a shadow depth map
	
	DF_FAST_ENTITY_RENDERING = 0x200000, // Used with DF_DRAW_ENTITIES to only render marked entities into the water reflection buffer for "fast reflections"
	DF_DRAW_SIMPLE_WORLD_MODEL_WATER = 0x400000,	// Draw a singe studio model for the world to save CPU.
};

//-----------------------------------------------------------------------------
// Purpose: View setup and rendering
//-----------------------------------------------------------------------------

class IViewRender
{
public:
	// SETUP
	// Initialize view renderer
	virtual void		Init( void ) = 0;

	// Clear any systems between levels
	virtual void		LevelInit( void ) = 0;
	virtual void		LevelShutdown( void ) = 0;

	// Shutdown
	virtual void		Shutdown( void ) = 0;

	// RENDERING
	// Called right before simulation. It must setup the view model origins and angles here so 
	// the correct attachment points can be used during simulation.	
	virtual void		OnRenderStart() = 0;

	// Called to render the entire scene
	virtual	void		Render( vrect_t *rect ) = 0;

	// Called to render just a particular setup ( for timerefresh and envmap creation )
	// First argument is 3d view setup, second is for the HUD (in most cases these are ==, but in split screen the client .dll handles this differently)
	virtual void		RenderView( const CViewSetup &view, const CViewSetup &hudViewSetup, int nClearFlags, int whatToDraw ) = 0;

	// What are we currently rendering? Returns a combination of DF_ flags.
	virtual int GetDrawFlags() = 0;

	// MISC
	// Start and stop pitch drifting logic
	virtual void		StartPitchDrift( void ) = 0;
	virtual void		StopPitchDrift( void ) = 0;

	// This can only be called during rendering (while within RenderView).
	virtual VPlane*		GetFrustum() = 0;

	virtual bool		ShouldDrawBrushModels( void ) = 0;

	virtual const CViewSetup *GetPlayerViewSetup( int nSlot = -1 ) const = 0;
	virtual const CViewSetup *GetViewSetup( void ) const = 0;

	virtual void		DisableVis( void ) = 0;

	virtual int			BuildWorldListsNumber() const = 0;

	virtual void		SetCheapWaterStartDistance( float flCheapWaterStartDistance ) = 0;
	virtual void		SetCheapWaterEndDistance( float flCheapWaterEndDistance ) = 0;

	virtual void		GetWaterLODParams( float &flCheapWaterStartDistance, float &flCheapWaterEndDistance ) = 0;

	virtual void		DriftPitch (void) = 0;

	virtual void		SetScreenOverlayMaterial( IMaterial *pMaterial ) = 0;
	virtual IMaterial	*GetScreenOverlayMaterial( ) = 0;

	virtual void		WriteSaveGameScreenshot( const char *pFilename ) = 0;
	virtual void		WriteSaveGameScreenshotOfSize( const char *pFilename, int width, int height ) = 0;

	// Draws another rendering over the top of the screen
	virtual void		QueueOverlayRenderView( const CViewSetup &view, int nClearFlags, int whatToDraw ) = 0;

	// Returns znear and zfar
	virtual float		GetZNear() = 0;
	virtual float		GetZFar() = 0;

	// Returns the min/max fade distances, and distance scale
	virtual void		GetScreenFadeDistances( float *pMin, float *pMax, float *pScale ) = 0;

	virtual CBaseEntity *GetCurrentlyDrawingEntity() = 0;
	virtual void		SetCurrentlyDrawingEntity( CBaseEntity *pEnt ) = 0;

	virtual bool		UpdateShadowDepthTexture( ITexture *pRenderTarget, ITexture *pDepthTexture, const CViewSetup &shadowView, bool bRenderWorldAndObjects = true, bool bRenderViewModels = false ) = 0;

	virtual void		FreezeFrame( float flFreezeTime ) = 0;

	virtual void		InitFadeData( void ) = 0;
};

extern IViewRender *view;

class Beam_t;
class CBaseEntity;
class CBeam;

// common to server, too


extern void SetBeamCreationAllowed( bool state );
extern bool BeamCreationAllowed( void );

//-----------------------------------------------------------------------------
// beam flags
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Purpose: Popcorn trail for Beam Follow rendering...
//-----------------------------------------------------------------------------

struct BeamTrail_t
{
	// NOTE:  Don't add user defined fields except after these four fields.
	BeamTrail_t*	next;
	float			die;
	Vector			org;
	Vector			vel;
};

//-----------------------------------------------------------------------------
// Data type for beams.
//-----------------------------------------------------------------------------
struct BeamInfo_t
{
	int			m_nType;

	// Entities
	CBaseEntity* m_pStartEnt;
	int			m_nStartAttachment;
	CBaseEntity* m_pEndEnt;
	int			m_nEndAttachment;

	// Points
	Vector		m_vecStart;
	Vector		m_vecEnd;

	int			m_nModelIndex;
	const char	*m_pszModelName;

	int			m_nHaloIndex;
	const char	*m_pszHaloName;
	float		m_flHaloScale;

	float		m_flLife;
	float		m_flWidth;
	float		m_flEndWidth;
	float		m_flFadeLength;
	float		m_flAmplitude;

	float		m_flBrightness;
	float		m_flSpeed;
	
	int			m_nStartFrame;
	float		m_flFrameRate;

	float		m_flRed;
	float		m_flGreen;
	float		m_flBlue;

	bool		m_bRenderable;

	int			m_nSegments;

	int			m_nFlags;

	// Rings
	Vector		m_vecCenter;
	float		m_flStartRadius;
	float		m_flEndRadius;

	BeamInfo_t() 
	{ 
		m_nType = TE_BEAMPOINTS;
		m_nSegments = -1;
		m_pszModelName = nullptr;
		m_pszHaloName = nullptr;
		m_nModelIndex = -1;
		m_nHaloIndex = -1;
		m_bRenderable = true;
		m_nFlags = 0;
	}
};

//-----------------------------------------------------------------------------
// Purpose: Declare client .dll beam entity interface
//-----------------------------------------------------------------------------

class IViewRenderBeams
{
public:
	virtual void	InitBeams( void ) = 0;
	virtual void	ShutdownBeams( void ) = 0;
	virtual void	ClearBeams( void ) = 0;

	// Updates the state of the temp ent beams
	virtual void	UpdateTempEntBeams() = 0;

	virtual void	DrawBeam( CBeam* pbeam, const RenderableInstance_t &instance, ITraceFilter *pEntityBeamTraceFilter = nullptr ) = 0;
	virtual void	DrawBeam( Beam_t *pbeam ) = 0;

	virtual void	KillDeadBeams( CBaseEntity *pEnt ) = 0;

	// New interfaces!
	virtual Beam_t	*CreateBeamEnts( BeamInfo_t &beamInfo ) = 0;
	virtual Beam_t	*CreateBeamEntPoint( BeamInfo_t &beamInfo ) = 0;
	virtual	Beam_t	*CreateBeamPoints( BeamInfo_t &beamInfo ) = 0;
	virtual Beam_t	*CreateBeamRing( BeamInfo_t &beamInfo ) = 0;
	virtual Beam_t	*CreateBeamRingPoint( BeamInfo_t &beamInfo ) = 0;
	virtual Beam_t	*CreateBeamCirclePoints( BeamInfo_t &beamInfo ) = 0;
	virtual Beam_t	*CreateBeamFollow( BeamInfo_t &beamInfo ) = 0;

	virtual void	FreeBeam( Beam_t *pBeam ) = 0;
	virtual void	UpdateBeamInfo( Beam_t *pBeam, BeamInfo_t &beamInfo ) = 0;

	// These will go away!
	virtual void	CreateBeamEnts( int startEnt, int endEnt, int modelIndex, int haloIndex, float haloScale,  
							float life, float width, float m_nEndWidth, float m_nFadeLength, float amplitude, 
							float brightness, float speed, int startFrame, 
							float framerate, float r, float g, float b, int type = -1 ) = 0;
	virtual void	CreateBeamEntPoint( int	nStartEntity, const Vector *pStart, int nEndEntity, const Vector* pEnd,
							int modelIndex, int haloIndex, float haloScale,   
							float life, float width, float m_nEndWidth, float m_nFadeLength, float amplitude, 
							float brightness, float speed, int startFrame, 
							float framerate, float r, float g, float b ) = 0;
	virtual void	CreateBeamPoints( Vector& start, Vector& end, int modelIndex, int haloIndex, float haloScale,   
							float life, float width, float m_nEndWidth, float m_nFadeLength, float amplitude, 
							float brightness, float speed, int startFrame, 
							float framerate, float r, float g, float b ) = 0;
	virtual void	CreateBeamRing( int startEnt, int endEnt, int modelIndex, int haloIndex, float haloScale,   
							float life, float width, float m_nEndWidth, float m_nFadeLength, float amplitude, 
							float brightness, float speed, int startFrame, 
							float framerate, float r, float g, float b, int flags = 0 ) = 0;
	virtual void	CreateBeamRingPoint( const Vector& center, float start_radius, float end_radius, int modelIndex, int haloIndex, float haloScale,   
							float life, float width, float m_nEndWidth, float m_nFadeLength, float amplitude, 
							float brightness, float speed, int startFrame, 
							float framerate, float r, float g, float b, int flags = 0 ) = 0;
	virtual void	CreateBeamCirclePoints( int type, Vector& start, Vector& end, 
							int modelIndex,  int haloIndex,  float haloScale, float life, float width, 
							float m_nEndWidth, float m_nFadeLength, float amplitude, float brightness, float speed, 
							int startFrame, float framerate, float r, float g, float b ) = 0;
	virtual void	CreateBeamFollow( int startEnt, int modelIndex, int haloIndex, float haloScale,  
							float life, float width, float m_nEndWidth, float m_nFadeLength, float r, float g, float b, 
							float brightness ) = 0;
};

extern IViewRenderBeams *beams;

class IVModeManager
{
public:
	virtual void	Init( void ) = 0;
	// HL2 will ignore, TF2 will change modes.
	virtual void	SwitchMode( bool commander, bool force ) = 0;
	virtual void	LevelInit( const char *newmap ) = 0;
	virtual void	LevelShutdown( void ) = 0;
};

extern IVModeManager *modemanager;

//-----------------------------------------------------------------------------
/**
 * JiggleData is the instance-specific data for a jiggle bone
 */
struct JiggleData
{
	void Init( int initBone, float currenttime, const Vector &initBasePos, const Vector &initTipPos )
	{
		bone = initBone;

		lastUpdate = currenttime;

		basePos = initBasePos;
		baseLastPos = basePos;
		baseVel.Init();
		baseAccel.Init();

		tipPos = initTipPos;
		tipVel.Init();
		tipAccel.Init();
	}

	int bone;
	int id;

	float lastUpdate;	// based on gpGlobals->realtime

	Vector basePos;		// position of the base of the jiggle bone
	Vector baseLastPos;
	Vector baseVel;
	Vector baseAccel;

	Vector tipPos;		// position of the tip of the jiggle bone
	Vector tipVel;
	Vector tipAccel;
};

class CJiggleBones
{
public:
	JiggleData * GetJiggleData( int bone, float currenttime, const Vector &initBasePos, const Vector &initTipPos );
	void BuildJiggleTransformations( int boneIndex, float currentime, const mstudiojigglebone_t *jiggleParams, const matrix3x4_t &goalMX, matrix3x4_t &boneMX, bool coordSystemIsFlipped );

	std::vector< JiggleData >	m_jiggleBoneState;
};

struct kbutton_t
{
	struct Split_t
	{
		// key nums holding it down
		int		down[ 2 ];		
		// low bit is down state
		int		state;			
	};

	Split_t		&GetPerUser( int nSlot = -1 );

	Split_t		m_PerUser[ MAX_SPLITSCREEN_PLAYERS ];
};

class IKeyBindingListener
{
public:
	virtual void OnKeyBindingChanged( int nSplitScreenSlot, ButtonCode_t buttonCode, char const *pchKeyName, char const *pchNewBinding ) = 0;
};

class IKeyBindingListenerMgr
{
public:
	// Callback when button is bound
	virtual void AddListenerForCode( IKeyBindingListener *pListener, ButtonCode_t buttonCode ) = 0;
	// Callback whenver binding is set to a button
	virtual void AddListenerForBinding( IKeyBindingListener *pListener, char const *pchBindingString ) = 0;

	virtual void RemoveListener( IKeyBindingListener *pListener ) = 0;
};

extern IKeyBindingListenerMgr *g_pKeyBindingListenerMgr;

// Purpose: Client handler for instruction players how to play

typedef void (*KeyValueBuilder)( KeyValues* );

struct KeyValueSaverData 
{
	char szFileName[ MAX_PATH ];
	bool bDirtySaveData;
	KeyValues *pKeyValues;
	KeyValueBuilder funcKeyValueBuilder;
};

class CKeyValueSaver : public CAutoGameSystemPerFrame, public CGameEventListener
{
public:
	CKeyValueSaver() : CAutoGameSystemPerFrame( "CKeyValueSaver" )
	{
		m_nSplitScreenSlot = -1;
	}

	void SetSlot( int nSlot ) { m_nSplitScreenSlot = nSlot; }

	// Methods of IGameSystem
	virtual bool Init( void );
	virtual void Shutdown( void );
	virtual void Update( float frametime );

	// Game event handler
	void FireGameEvent( IGameEvent *event );

	bool InitKeyValues( const char *pchFileName, KeyValueBuilder funcKeyValueBuilder );
	bool WriteDirtyKeyValues( const char *pchFileName, bool bForceWrite = false );

	KeyValues * GetKeyValues( const char *pchFileName, bool bForceReread = false );
	void MarkKeyValuesDirty( const char *pchFileName );

private:

	bool ReadKeyValues( KeyValueSaverData *pKeyValueData );
	bool WriteDirtyKeyValues( KeyValueSaverData *pKeyValueData, bool bForceWrite = false );
	void WriteAllDirtyKeyValues( void );

	KeyValueSaverData * FindKeyValueData( const char *pchFileName );

private:
	
	std::vector< KeyValueSaverData > m_KeyValueData;

	int		m_nSplitScreenSlot;
};

CKeyValueSaver &KeyValueSaver();

// Merged from L4D but waiting on other code to be merged before this can compile 
void GameInstructor_Init();
void GameInstructor_Shutdown();

class CHudElement;

inline constexpr auto MENU_SELECTION_TIMEOUT = 5.0f;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CHudMenu : public CHudElement
{
public:
	explicit CHudMenu( const char *pElementName );
	void Init( void );
	void VidInit( void );
	void Reset( void );
	virtual bool ShouldDraw( void );
	bool MsgFunc_ShowMenu( const ks::net::CCSUsrMsg_ShowMenu &msg );
	void HideMenu( void );
	void ShowMenu( const char * menuName, int keySlot );
	void ShowMenu_KeyValueItems( KeyValues *pKV );

	bool IsMenuOpen( void );
	void SelectMenuItem( int menu_item );

	CUserMessageBinder m_UMCMsgShowMenu;

private:
	void OnThink();
	void		ProcessText( void );

	struct ProcessedLine
	{
		int	menuitem; // -1 for just text
		int startchar;
		int length;
		int pixels;
		int height;
	};

	std::vector< ProcessedLine >	m_Processed;

	int				m_nMaxPixels;
	int				m_nHeight;

	bool			m_bMenuDisplayed;
	int				m_bitsValidSlots;
	float			m_flShutoffTime;
	int				m_nSelectedItem;
	bool			m_bMenuTakesInput;

	float			m_flSelectionTime;

	float			m_flOpenCloseTime;
};

// Anyone requesting model combines needs to derive from this to get completion callbacks.

class IModelCombinerRequesterInterface
{
public:
	// Called when a requested model combine has finished.
	virtual void	ModelCombineFinished( bool bSucceeded, int nCombinedModelIndex ) = 0;

	// The CModelCombiner will never call this. It's here to remind you that 
	// you need to call CModelCombiner::AbortCombineModelFor() when you're deleted, so it
	// removes you from any pending callbacks.
	virtual void	ModelCombineAbort( void ) = 0;
};

// Request a combined model to be created for the base model and the attached additional models.
// The requester it must derive from IModelCombinerRequesterInterface. The ModelCombineFinished
// function will be called with the result of the combine request.
bool	ModelCombiner_CombineModel( IModelCombinerRequesterInterface *pRequester, const char *pszBaseModel, std::vector< const char * > *vecAdditionalModels );
bool	ModelCombiner_CombineModel( IModelCombinerRequesterInterface *pRequester, const std::vector< SCombinerModelInput_t > &vecModelsToCombine );

// Call this when you're being destroyed. It'll remove you from any pending callback lists.
void	ModelCombiner_AbortCombineModelFor( IModelCombinerRequesterInterface *pRequester );

// Purpose: Fast path model rendering

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
struct ModelRenderSystemData_t
{
	IClientRenderable *m_pRenderable;
	IClientModelRenderable *m_pModelRenderable;
	RenderableInstance_t m_InstanceData;
};
	
struct TranslucentInstanceRenderData_t
{
	StudioModelArrayInfo_t *m_pModelInfo;
	StudioArrayInstanceData_t *m_pInstanceData;
};

struct TranslucentTempData_t
{
	int m_nColorMeshHandleCount;
	DataCacheHandle_t *m_pColorMeshHandles;
	bool m_bReleaseRenderData;
};

enum ModelRenderMode_t
{
	MODEL_RENDER_MODE_NORMAL,
	MODEL_RENDER_MODE_SHADOW_DEPTH,
	MODEL_RENDER_MODE_RTT_SHADOWS,
	MODEL_RENDER_MODE_SSAO
};

//-----------------------------------------------------------------------------
// The client leaf system
//-----------------------------------------------------------------------------
class IModelRenderSystem
{
public:
	virtual void DrawModels( ModelRenderSystemData_t *pModels, int nCount, ModelRenderMode_t renderMode, bool bShadowDepthIncludeTranslucentMaterials = false ) = 0;
	virtual void DrawBrushModels( ModelRenderSystemData_t *pModels, int nCount, ModelRenderMode_t renderMode ) = 0;

	virtual void ComputeTranslucentRenderData( ModelRenderSystemData_t *pModels, int nCount, TranslucentInstanceRenderData_t *pRenderData, TranslucentTempData_t *pTempData ) = 0;
	virtual void CleanupTranslucentTempData( TranslucentTempData_t *pTempData ) = 0;

	virtual IMaterial *GetFastPathColorMaterial() = 0;
};

//-----------------------------------------------------------------------------
// Singleton accessor
//-----------------------------------------------------------------------------
extern IModelRenderSystem *g_pModelRenderSystem;

class CBaseEntity;


//-----------------------------------------------------------------------------
// Particle attachment methods
//-----------------------------------------------------------------------------
enum ParticleAttachment_t
{
	PATTACH_ABSORIGIN = 0,			// Create at absorigin, but don't follow
	PATTACH_ABSORIGIN_FOLLOW,		// Create at absorigin, and update to follow the entity
	PATTACH_CUSTOMORIGIN,			// Create at a custom origin, but don't follow
	PATTACH_CUSTOMORIGIN_FOLLOW,	// Create at a custom origin, follow relative position to specified entity
	PATTACH_POINT,					// Create on attachment point, but don't follow
	PATTACH_POINT_FOLLOW,			// Create on attachment point, and update to follow the entity
	PATTACH_EYES_FOLLOW,			// Create on eyes of the attached entity, and update to follow the entity
	PATTACH_OVERHEAD_FOLLOW,		// Create at the top of the entity's bbox
	PATTACH_WORLDORIGIN,			// Used for control points that don't attach to an entity
	PATTACH_ROOTBONE_FOLLOW,		// Create at the root bone of the entity, and update to follow

	MAX_PATTACH_TYPES,
};

extern int GetAttachTypeFromString( const char *pszString );

inline constexpr auto PARTICLE_DISPATCH_FROM_ENTITY = (1<<0);
inline constexpr auto PARTICLE_DISPATCH_RESET_PARTICLES = (1<<1);
inline constexpr auto PARTICLE_DISPATCH_ALLOW_DORMANT = (1<<2);

//-----------------------------------------------------------------------------
// Particle parsing methods
//-----------------------------------------------------------------------------
// Parse the particle manifest file & register the effects within it
// Only needs to be called once per game, unless tools change particle definitions
void ParseParticleEffects( bool bLoadSheets );

// Precaches standard particle systems (only necessary on server)
// Should be called once per level
void PrecacheStandardParticleSystems( );

//-----------------------------------------------------------------------------
// Particle spawning methods
//-----------------------------------------------------------------------------
void DispatchParticleEffect( const char *pszParticleName, ParticleAttachment_t iAttachType, CBaseEntity *pEntity, const char *pszAttachmentName, bool bResetAllParticlesOnEntity = false, int nSplitScreenPlayerSlot = -1, IRecipientFilter *filter = nullptr );
void DispatchParticleEffect( const char *pszParticleName, ParticleAttachment_t iAttachType, CBaseEntity *pEntity = nullptr, int iAttachmentPoint = -1, bool bResetAllParticlesOnEntity = false, int nSplitScreenPlayerSlot = -1, IRecipientFilter *filter = nullptr, bool bAllowDormantSpawn = false );
void DispatchParticleEffect( const char *pszParticleName, Vector vecOrigin, QAngle vecAngles, CBaseEntity *pEntity = nullptr, int nSplitScreenPlayerSlot = -1, IRecipientFilter *filter = nullptr );
void DispatchParticleEffect( const char *pszParticleName, const Vector &vecOrigin, const QAngle &vecAngles, ParticleAttachment_t iAttachType, CBaseEntity *pEntity = nullptr, int nSplitScreenPlayerSlot = -1 );
void DispatchParticleEffect( const char *pszParticleName, Vector vecOrigin, Vector vecStart, QAngle vecAngles, CBaseEntity *pEntity = nullptr, int nSplitScreenPlayerSlot = -1, IRecipientFilter *filter = nullptr );
void DispatchParticleEffect( int iEffectIndex, Vector vecOrigin, Vector vecStart, QAngle vecAngles, CBaseEntity *pEntity = nullptr, int nSplitScreenPlayerSlot = -1, IRecipientFilter *filter = nullptr );
void DispatchParticleEffectLink( const char *pszParticleName, ParticleAttachment_t iAttachType, CBaseEntity *pEntity = nullptr, CBaseEntity *pOtherEntity = nullptr, int iAttachmentPoint = -1, bool bResetAllParticlesOnEntity = false, int nSplitScreenPlayerSlot = -1 );
void DispatchParticleEffect( int iEffectIndex, const Vector &vecOrigin, const QAngle &vecAngles, ParticleAttachment_t iAttachType, CBaseEntity *pEntity = nullptr, int nSplitScreenPlayerSlot = -1 );
void StopParticleEffects( CBaseEntity *pEntity );
void StopParticleEffect( CBaseEntity *pEntity, const char *pszParticleName );

class CParticleEffectBinding;
class CParticleMgr;
class CParticleSubTexture;
class CParticleSubTextureGroup;
class IClientParticleListener;
class IParticleEffect;
struct Particle;

class CNewParticleEffect;
class CParticleRenderIterator;
class CParticleSimulateIterator;
class ParticleDraw;

//
// This module implements the particle manager for the client DLL.
// In a nutshell, to create your own effect, implement the ParticleEffect 
// interface and call CParticleMgr::AddEffect to add your effect. Then you can 
// add particles and simulate and render them.

/*

Particle manager documentation
-----------------------------------------------------------------------------

All particle effects are managed by a class called CParticleMgr. It tracks 
the list of particles, manages their materials, sorts the particles, and
has callbacks to render them.

Conceptually, CParticleMgr is NOT part of VEngine's entity system. It does
not care about entities, only particle effects. Usually, the two are implemented
together, but you should be aware the CParticleMgr talks to you through its
own interfaces and does not talk to entities. Thus, it is possible to have
particle effects that are not entities.

To make a particle effect, you need two things: 

1. An implementation of the IParticleEffect interface. This is how CParticleMgr 
   talks to you for things like rendering and updating your effect.

2. A (member) variable of type CParticleEffectBinding. This allows CParticleMgr to 
   store its internal data associated with your effect.

Once you have those two things, you call CParticleMgr::AddEffect and pass them
both in. You will then get updates through IParticleEffect::Update, and you will
be asked to render your particles with IParticleEffect::SimulateAndRender.

When you want to remove the effect, call CParticleEffectBinding::SetRemoveFlag(), which
tells CParticleMgr to remove the effect next chance it gets.

Example class:

	class CMyEffect : public IParticleEffect
	{
	public:
		// Call this to start the effect by adding it to the particle manager.
		void			Start()
		{
			ParticleMgr()->AddEffect( &m_ParticleEffect, this );
		}

		// implementation of IParticleEffect functions go here...

	public:
		CParticleEffectBinding	m_ParticleEffect;
	};

How the particle effects are integrated with the entity system
-----------------------------------------------------------------------------

There are two helper classes that you can use to create particles for your
entities. Each one is useful under different conditions.

1. CSimpleEmitter is a class that does some of the dirty work of using particles.
   If you want, you can just instantiate one of these with CSimpleEmitter::Create
   and call its AddParticle functions to add particles. When you are done and 
   want to 'free' it, call its Release function rather than deleting it, and it
   will wait until all of its particles have gone away before removing itself
   (so you don't have to write code to wait for all of the particles to go away).

   In most cases, it is the easiest and most clear to use CSimpleEmitter or
   derive a class from it, then use that class from inside an entity that wants
   to make particles.

   CSimpleEmitter and derived classes handle adding themselves to the particle
   manager, tracking how many particles in the effect are active, and 
   rendering the particles.

   CSimpleEmitter has code to simulate and render particles in a generic fashion,
   but if you derive a class from it, you can override some of its behavior
   with virtuals like UpdateAlpha, UpdateScale, UpdateColor, etc..

   Example code:
		CSimpleEmitter *pEmitter = CSimpleEmitter::Create();
		
		CEffectMaterialHandle hMaterial = pEmitter->GetCEffectMaterial( "mymaterial" );
		
		for( int i=0; i < 100; i++ )
			pEmitter->AddParticle( hMaterial, RandomVector(0,10), 4 );

		pEmitter->Release();

2. Some older effects derive from C_BaseParticleEffect and implement an entity 
   and a particle system at the same time. This gets nasty and is not encouraged anymore.

*/

//-----------------------------------------------------------------------------
// forward declarations
//-----------------------------------------------------------------------------

inline constexpr auto INVALID_MATERIAL_HANDLE = nullptr;

// Various stats, disabled
// extern int			g_nParticlesDrawn;
// extern CCycleCount	g_ParticleTimer;

//-----------------------------------------------------------------------------
// The basic particle description; all particles need to inherit from this.
//-----------------------------------------------------------------------------

struct Particle
{
	Particle *m_pPrev, *m_pNext;

	// Which sub texture this particle uses (so we can get at the tcoord mins and maxs).
	CParticleSubTexture *m_pSubTexture;

	// If m_Pos isn't used to store the world position, then implement IParticleEffect::GetParticlePosition()
	Vector m_Pos;			// Position of the particle in world space
};

//-----------------------------------------------------------------------------
// This is the CParticleMgr's reference to a material in the material system.
// Particles are sorted by material.
//-----------------------------------------------------------------------------

// This indexes CParticleMgr::m_SubTextures.
typedef CParticleSubTexture* PMaterialHandle;

// Each effect stores a list of particles associated with each material. The list is 
// hashed on the IMaterial pointer.
class CEffectMaterial
{
public:
	CEffectMaterial();

public:
	// This provides the material that gets bound for this material in this effect.
	// There can be multiple subtextures all within the same CEffectMaterial.
	CParticleSubTextureGroup *m_pGroup;
	
	Particle m_Particles;
	CEffectMaterial *m_pHashedNext;
};

class CParticleSubTextureGroup
{
public:
				CParticleSubTextureGroup();
				~CParticleSubTextureGroup();

	// Even though each of the subtextures has its own material, they should all basically be 
	// the same exact material and just use different texture coordinates, so this is the 
	// material of the first subtexture that is bound.
	//
	// This is gotten from GetMaterialPage().
	IMaterial	*m_pPageMaterial;
};

// Precalculated data for each material used for particles.
// This allows us to put multiple subtextures into one VTF and sort them against each other.
class CParticleSubTexture
{
public:
	CParticleSubTexture();

	float m_tCoordMins[2];	// bbox in texel space that this particle material uses.
	float m_tCoordMaxs[2];	// Specified in the SubTextureMins/SubTextureMaxs parameter in the materials.

	// Which group does this subtexture belong to?
	CParticleSubTextureGroup *m_pGroup;
	CParticleSubTextureGroup m_DefaultGroup;	// This is used as the group if a particle's material
												// isn't using a group.

	IMaterial *m_pMaterial;
};

//-----------------------------------------------------------------------------
// interface IParticleEffect:
//
// This is the interface that particles effects must implement. The effect is 
// responsible for starting itself and calling CParticleMgr::AddEffect, then it 
// will get the callbacks it needs to simulate and render the particles.
//-----------------------------------------------------------------------------

class IParticleEffect
{
// Overridables.
public:
	
	virtual			~IParticleEffect() {}
	
	// Called at the beginning of a frame to precalculate data for rendering 
	// the particles. If you manage your own list of particles and want to 
	// simulate them all at once, you can do that here and just render them in 
	// the SimulateAndRender call.
	virtual void	Update( float fTimeDelta ) {}
	
	// Called once for the entire effect before the batch of SimulateAndRender() calls.
	// For particle systems using FLAGS_CAMERASPACE (the default), effectMatrix transforms the particles from
	// world space into camera space. You can change this matrix if you want your particles relative to something
	// else like an attachment's space.
	virtual void	StartRender( VMatrix &effectMatrix ) {}

	// Simulate the particles.
	virtual bool	ShouldSimulate() const = 0;
	virtual void	SetShouldSimulate( bool bSim ) = 0;
	virtual void	SimulateParticles( CParticleSimulateIterator *pIterator ) = 0;

	// Render the particles.
	virtual void	RenderParticles( CParticleRenderIterator *pIterator ) = 0;

	// Implementing this is optional. It is called when an effect is removed. It is useful if
	// you hold onto pointers to the particles you created (so when this is called, you should
	// clean up your data so you don't reference the particles again).
	// NOTE: after calling this, the particle manager won't touch the IParticleEffect
	// or its associated CParticleEffectBinding anymore.
	virtual void	NotifyRemove() {}

	// This method notifies the effect a particle is about to be deallocated.
	// Implementations should *not* actually deallocate it.
	// NOTE: The particle effect's GetNumActiveParticles is updated BEFORE this is called
	//       so if GetNumActiveParticles returns 0, then you know this is the last particle
	//       in the system being removed.
	virtual void	NotifyDestroyParticle( Particle* pParticle ) {}

	// Fill in the origin used to sort this entity.
	// This is a world space position.
	virtual const Vector &GetSortOrigin() = 0;

	// Fill in the origin used to sort this entity.
// TODO: REMOVE THIS. ALL PARTICLE SYSTEMS SHOULD EITHER SET m_Pos IN CONJUNCTION WITH THE
// PARTICLE_LOCALSPACE FLAG, OR DO SETBBOX THEMSELVES.
	virtual const Vector *GetParticlePosition( Particle *pParticle ) { return &pParticle->m_Pos; }

	virtual const char *GetEffectName() { return "???"; } 
};

// In order to create a particle effect, you must have one of these around and
// implement IParticleEffect. Pass them both into CParticleMgr::AddEffect and you
// are good to go.
class CParticleEffectBinding : public CDefaultClientRenderable
{
	friend class CParticleMgr;
	friend class CParticleSimulateIterator;
	friend class CNewParticleEffect;

public:
	CParticleEffectBinding();
	~CParticleEffectBinding();
	
// Helper functions to setup, add particles, etc..
public:

	// Simulate all the particles.
	void			SimulateParticles( float flTimeDelta );

	// Use this to specify materials when adding particles. 
	// Returns the index of the material it found or added.
	// Returns INVALID_MATERIAL_HANDLE if it couldn't find or add a material.
	PMaterialHandle	FindOrAddMaterial( const char *pMaterialName );

	// Allocate particles. The Particle manager will automagically
	// deallocate them when the IParticleEffect SimulateAndRender() method 
	// returns false. The first argument is the size of the particle
	// structure in bytes
	Particle*		AddParticle( int sizeInBytes, PMaterialHandle pMaterial );

	// This is an optional call you can make if you want to manually manage the effect's
	// bounding box. Normally, the bounding box is managed automatically, but in certain
	// cases it is more efficient to set it manually.
	//
	// Note: this is a WORLD SPACE bounding box, even if you've used SetLocalSpaceTransform.
	//
	// After you make this call, the particle manager will no longer update the bounding
	// box automatically if bDisableAutoUpdate is true.
	void			SetBBox( const Vector &bbMin, const Vector &bbMax, bool bDisableAutoUpdate = true );
	// gets a copy of the current bbox mins/maxs in worldspace
	void			GetWorldspaceBounds( Vector *pMins, Vector *pMaxs );

	// This tells the particle manager that your particles are transformed by the specified matrix.
	// That way, it can transform the bbox defined by Particle::m_Pos into world space correctly.
	//
	// It also sets up the matrix returned by CParticleMgr::GetModelView() to include this matrix, so you
	// can do TransformParticle with it like any other particle system.
	const matrix3x4_t&	GetLocalSpaceTransform() const;
	void			SetLocalSpaceTransform( const matrix3x4_t &transform );

	// This expands the bbox to contain the specified point. Returns true if bbox changed
	bool			EnlargeBBoxToContain( const Vector &pt );

	// The EZ particle singletons use this - they don't want to be added to all the leaves and drawn through the
	// leaf system - they are specifically told to draw each frame at a certain point.
	void			SetDrawThruLeafSystem( int bDraw );

	// Some view model particle effects want to be drawn right before the view model (after everything else is 
	// drawn).
	void			SetDrawBeforeViewModel( int bDraw );

	// Call this to have the effect removed whenever it safe to do so.
	// This is a lot safer than calling CParticleMgr::RemoveEffect.
	int				GetRemoveFlag()									{ return GetFlag( FLAGS_REMOVE ); }
	void			SetRemoveFlag()									{ SetFlag( FLAGS_REMOVE, 1 ); }

	// Set this flag to tell the particle manager to simulate your particles even
	// if the particle system isn't visible. Tempents and fast effects can always use
	// this if they want since they want to simulate their particles until they go away.
	// This flag is ON by default.
	int				GetAlwaysSimulate()								{ return GetFlag( FLAGS_ALWAYSSIMULATE ); }
	void			SetAlwaysSimulate( int bAlwaysSimulate )		{ SetFlag( FLAGS_ALWAYSSIMULATE, bAlwaysSimulate ); }

	void			SetIsNewParticleSystem( void )		{ SetFlag( FLAGS_NEW_PARTICLE_SYSTEM, 1 ); }
	// Set if the effect was drawn the previous frame.
	// This can be used by particle effect classes
	// to decide whether or not they want to spawn
	// new particles - if they weren't drawn, then
	// they can 'freeze' the particle system to avoid
	// overhead.
	int				WasDrawnPrevFrame()								{ return GetFlag( FLAGS_DRAWN_PREVFRAME ); }
	void			SetWasDrawnPrevFrame( int bWasDrawnPrevFrame )	{ SetFlag( FLAGS_DRAWN_PREVFRAME, bWasDrawnPrevFrame ); }

	// When the effect is in camera space mode, then the transforms are setup such that
	// the particle vertices are specified in camera space (in CParticleDraw) rather than world space. 
	//
	// This makes it faster to specify the particles - you only have to transform the center 
	// by CParticleMgr::GetModelView then add to X and Y to build the quad.
	//
	// Effects that want to specify verts (in CParticleDraw) in world space should set this to false and
	// ignore CParticleMgr::GetModelView.
	//
	// Camera space mode is ON by default.
	int				IsEffectCameraSpace()							{ return GetFlag( FLAGS_CAMERASPACE ); }
	void			SetEffectCameraSpace( int bCameraSpace )		{ SetFlag( FLAGS_CAMERASPACE, bCameraSpace ); }

	// This tells it whether or not to apply the local transform to the matrix returned by CParticleMgr::GetModelView().
	// Usually, you'll want this, so you can just say TransformParticle( pMgr->GetModelView(), vPos ), but you may want
	// to manually apply your local transform before saying TransformParticle.
	//
	// This is ON by default.
	int				GetAutoApplyLocalTransform() const				{ return GetFlag( FLAGS_AUTOAPPLYLOCALTRANSFORM ); }
	void			SetAutoApplyLocalTransform( int b )				{ SetFlag( FLAGS_AUTOAPPLYLOCALTRANSFORM, b ); }

	// If this is true, then the bbox is calculated from particle positions. This works
	// fine if you always simulate (SetAlwaysSimulateFlag) so the system can become visible
	// if it moves into the PVS. If you don't use this, then you should call SetBBox at 
	// least once to tell the particle manager where your entity is.
	int				GetAutoUpdateBBox()							{ return GetFlag( FLAGS_AUTOUPDATEBBOX ); }
	void			SetAutoUpdateBBox( int bAutoUpdate )		{ SetFlag( FLAGS_AUTOUPDATEBBOX, bAutoUpdate ); }

	// Get the current number of particles in the effect.
	int				GetNumActiveParticles();

	// The is the max size of the particles for use in bounding	computation
	void			SetParticleCullRadius( float flMaxParticleRadius );

	// Build a list of all active particles, returns actual count filled in
	int				GetActiveParticleList( int nCount, Particle **ppParticleList );

	// detect origin/bbox changes and update leaf system if necessary
	void			DetectChanges();

private:
	// Change flags..
	void			SetFlag( int flag, int bOn )	{ if( bOn ) m_Flags |= flag; else m_Flags &= ~flag; }
	int				GetFlag( int flag ) const		{ return m_Flags & flag; }

	void			Init( CParticleMgr *pMgr, IParticleEffect *pSim );
	void			Term();

	// Get rid of the specified particle.
	void			RemoveParticle( Particle *pParticle );

	void			StartDrawMaterialParticles(
						CEffectMaterial *pMaterial,
						float flTimeDelta,
						IMesh* &pMesh,
						CMeshBuilder &builder,
						ParticleDraw &particleDraw,
						bool bWireframe );

	int				DrawMaterialParticles( 
						bool bBucketSort,
						CEffectMaterial *pMaterial, 
						float flTimeDelta,
						bool bWireframe
						 );

	void			GrowBBoxFromParticlePositions( CEffectMaterial *pMaterial, bool &bboxSet, Vector &bbMin, Vector &bbMax );

	void			RenderStart( VMatrix &mTempModel, VMatrix &mTempView );
	void			RenderEnd( VMatrix &mModel, VMatrix &mView );

	void			BBoxCalcStart( Vector &bbMin, Vector &bbMax );
	void			BBoxCalcEnd( bool bboxSet, Vector &bbMin, Vector &bbMax );
	
	void			DoBucketSort( 
						CEffectMaterial *pMaterial, 
						float *zCoords, 
						int nZCoords,
						float minZ,
						float maxZ );

	int				GetRemovalInProgressFlag()					{ return GetFlag( FLAGS_REMOVALINPROGRESS ); }
	void			SetRemovalInProgressFlag()					{ SetFlag( FLAGS_REMOVALINPROGRESS, 1 ); }

	// BBox is recalculated before it's put into the tree for the first time.
	int				GetNeedsBBoxUpdate()						{ return GetFlag( FLAGS_NEEDS_BBOX_UPDATE ); }
	void			SetNeedsBBoxUpdate( int bFirstUpdate )		{ SetFlag( FLAGS_NEEDS_BBOX_UPDATE, bFirstUpdate ); }

	// Set on creation and cleared after the first PostRender (whether or not the system was rendered).
	int				GetFirstFrameFlag()							{ return GetFlag( FLAGS_FIRST_FRAME ); }
	void			SetFirstFrameFlag( int bFirstUpdate )		{ SetFlag( FLAGS_FIRST_FRAME, bFirstUpdate ); }

	int				WasDrawn()									{ return GetFlag( FLAGS_DRAWN ); }
	void			SetDrawn( int bDrawn )						{ SetFlag( FLAGS_DRAWN, bDrawn ); }

	// Update m_Min/m_Max. Returns false and sets the bbox to the sort origin if there are no particles.
	bool			RecalculateBoundingBox();

	CEffectMaterial* GetEffectMaterial( CParticleSubTexture *pSubTexture );

// IClientRenderable overrides.
public:		

	virtual const Vector&			GetRenderOrigin( void );
	virtual const QAngle&			GetRenderAngles( void );
	virtual const matrix3x4_t &		RenderableToWorldTransform();
	virtual void					GetRenderBounds( Vector& mins, Vector& maxs );
	virtual bool					ShouldDraw( void );
	virtual int						DrawModel( int flags, const RenderableInstance_t &instance );

private:
	enum
	{
		FLAGS_REMOVE =				(1<<0),	// Set in SetRemoveFlag
		FLAGS_REMOVALINPROGRESS =	(1<<1), // Set while the effect is being removed to prevent
											// infinite recursion.
		FLAGS_NEEDS_BBOX_UPDATE =	(1<<2),	// This is set until the effect's bbox has been updated once.
		FLAGS_AUTOUPDATEBBOX =		(1<<3),	// Update bbox automatically? Cleared in SetBBox.
		FLAGS_ALWAYSSIMULATE =		(1<<4), // See SetAlwaysSimulate.
		FLAGS_DRAWN =				(1<<5),	// Set if the effect is drawn through the leaf system.
		FLAGS_DRAWN_PREVFRAME =		(1<<6),	// Set if the effect was drawn the previous frame.
											// This can be used by particle effect classes
											// to decide whether or not they want to spawn
											// new particles - if they weren't drawn, then
											// they can 'freeze' the particle system to avoid
											// overhead.
		FLAGS_CAMERASPACE =			(1<<7),	// See SetEffectCameraSpace.
		FLAGS_DRAW_THRU_LEAF_SYSTEM=(1<<8),	// This is the default - do the effect's visibility through the leaf system.
		FLAGS_DRAW_BEFORE_VIEW_MODEL=(1<<9),// Draw before the view model? If this is set, it assumes FLAGS_DRAW_THRU_LEAF_SYSTEM goes off.
		FLAGS_AUTOAPPLYLOCALTRANSFORM=(1<<10), // Automatically apply the local transform to CParticleMgr::GetModelView()'s matrix.
		FLAGS_FIRST_FRAME =         (1<<11),	// Cleared after the first frame that this system exists (so it can simulate after rendering once).
		FLAGS_NEW_PARTICLE_SYSTEM=  (1<<12) // uses new particle system
	};

	VMatrix m_LocalSpaceTransform;
	bool m_bLocalSpaceTransformIdentity;	// If this is true, then m_LocalSpaceTransform is assumed to be identity.
	
	// Bounding box. Stored in WORLD space.
	Vector							m_Min;
	Vector							m_Max;

	// paramter copies to detect changes
	Vector							m_LastMin;
	Vector							m_LastMax;
	
	// The particle cull size
	float							m_flParticleCullRadius;

	// Number of active particles.
	unsigned short					m_nActiveParticles;

	// See CParticleMgr::m_FrameCode.
	unsigned short					m_FrameCode;

	// Position in CParticleMgr::m_Effects, if added.
	std::optional< std::list< CParticleEffectBinding * >::iterator > m_ListIndex;

	IParticleEffect					*m_pSim;
	CParticleMgr					*m_pParticleMgr;
	
	// Combination of the CParticleEffectBinding::FLAGS_ flags.
	int								m_Flags;

	// Materials this effect is using.
	enum { EFFECT_MATERIAL_HASH_SIZE = 8 };
	CEffectMaterial *m_EffectMaterialHash[EFFECT_MATERIAL_HASH_SIZE];

	// For faster iteration.
	std::vector< std::unique_ptr< CEffectMaterial > > m_Materials;

	// auto updates the bbox after N frames
	unsigned short					m_UpdateBBoxCounter;
};

class CParticleLightInfo
{
public:
	Vector	m_vPos;
	Vector	m_vColor;	// 0-1
	float	m_flIntensity;
};

typedef IParticleEffect* (*CreateParticleEffectFN)();

enum
{
	TOOLPARTICLESYSTEMID_INVALID = -1,
};

class CNonDrawingParticleSystem
{
public:
	CNonDrawingParticleSystem *m_pNext;
	CNonDrawingParticleSystem *m_pPrev;
	CParticleCollection *m_pSystem;

	[[gnu::always_inline]] inline CParticleCollection *operator()( void ) const
	{
		return m_pSystem;
	}

	[[gnu::always_inline]] inline CParticleCollection *Get( void ) const
	{
		return m_pSystem;
	}

	~CNonDrawingParticleSystem( void );
};

class CClientTools;

class CParticleMgr
{
	friend class CParticleEffectBinding;
	friend class CParticleCollection;
	friend class CNonDrawingParticleSystem;
	friend class CClientTools;

public:

	CParticleMgr();
	virtual			~CParticleMgr();

	// Call at init time to preallocate the bucket of particles.
	bool			Init(unsigned long nPreallocatedParticles, IMaterialSystem *pMaterial);

	// Shutdown - free everything.
	void			Term(bool bCanReferenceOtherStaticObjects = true);

	void			LevelInit();

	void			RegisterEffect( const char *pEffectType, CreateParticleEffectFN func );
	IParticleEffect	*CreateEffect( const char *pEffectType );

	// Add and remove effects from the active list.
	// Note: once you call AddEffect, CParticleEffectBinding will automatically call
	//       RemoveEffect in its destructor.
	// Note: it's much safer to call CParticleEffectBinding::SetRemoveFlag instead of
	//       CParticleMgr::RemoveEffect.
	bool			AddEffect( CParticleEffectBinding *pEffect, IParticleEffect *pSim );
	void			RemoveEffect( CParticleEffectBinding *pEffect );

	void			AddEffect( CNewParticleEffect *pEffect );
	void			RemoveEffect( CNewParticleEffect *pEffect );

	// Called at level shutdown to free all the lingering particle effects (usually
	// CParticleEffect-derived effects that can linger with noone holding onto them).
	void			RemoveAllEffects();

	// This should be called at the start of the frame.
	void			IncrementFrameCode();
	
	// This updates all the particle effects and inserts them into the leaves.
	void			Simulate( float fTimeDelta );

	// This just marks effects that were drawn so during their next simulation they can know
	// if they were drawn in the previous frame.
	void			PostRender();

	// Draw the effects marked with SetDrawBeforeViewModel.
	void			DrawBeforeViewModelEffects();

	// Returns the modelview matrix
	VMatrix&		GetModelView();

	Particle		*AllocParticle( int size );
	void			FreeParticle( Particle * );

	PMaterialHandle	GetPMaterial( const char *pMaterialName );
	IMaterial*		PMaterialToIMaterial( PMaterialHandle hMaterial );

	//HACKHACK: quick fix that compensates for the fact that this system was designed to never release materials EVER.
	void RepairPMaterial( PMaterialHandle hMaterial );

	// Particles drawn with the ParticleSphere material will use this info.
	// This should be set in IParticleEffect.
	void GetDirectionalLightInfo( CParticleLightInfo &info ) const;
	void SetDirectionalLightInfo( const CParticleLightInfo &info );

	void SpewInfo( bool bDetail );

	// add a class that gets notified of entity events
	void AddEffectListener( IClientParticleListener *pListener );
	void RemoveEffectListener( IClientParticleListener *pListener );

	// Tool effect ids
	int AllocateToolParticleEffectId();

	// Remove all new effects
	void RemoveAllNewEffects();

	CNewParticleEffect *FirstNewEffect();
	CNewParticleEffect *NextNewEffect( CNewParticleEffect *pEffect );

	// Should particle effects be rendered?
	void RenderParticleSystems( bool bEnable );
	bool ShouldRenderParticleSystems() const;

	void RemoveOldParticleEffects( float flTime );   // Removes all particles created more than flTime in the past immediately
	void SetRemoveAllParticleEffects( void );	// Flags all the particle effects for removal.
	int GetNumParticles() const { return m_nCurrentParticlesAllocated; }

	CNonDrawingParticleSystem *CreateNonDrawingEffect( const char *pEffectName );

private:
	struct RetireInfo_t
	{
		CParticleCollection *m_pCollection;
		float m_flScreenArea;
		bool m_bFirstFrame;
	};

	// Call Update() on all the effects.
	void UpdateAllEffects( float flTimeDelta );

	void UpdateNewEffects( float flTimeDelta );				// update new particle effects

	void SpewActiveParticleSystems( );

	CParticleSubTextureGroup* FindOrAddSubTextureGroup( IMaterial *pPageMaterial );

	int ComputeParticleDefScreenArea( int nInfoCount, RetireInfo_t *pInfo, float *pTotalArea, CParticleSystemDefinition* pDef, 
		const CViewSetup& view, const VMatrix &worldToPixels, float flFocalDist );

	bool RetireParticleCollections( CParticleSystemDefinition* pDef, int nCount, RetireInfo_t *pInfo, float flScreenArea, float flMaxTotalArea );
	void BuildParticleSimList( std::vector< CNewParticleEffect* > &list );
	bool EarlyRetireParticleSystems( int nCount, CNewParticleEffect **ppEffects );
	static int RetireSort( const void *p1, const void *p2 ); 

private:

	int m_nCurrentParticlesAllocated;

	// Directional lighting info.
	CParticleLightInfo m_DirectionalLight;

	// Frame code, used to prevent CParticleEffects from simulating multiple times per frame.
	// Their DrawModel can be called multiple times per frame because of water reflections,
	// but we only want to simulate the particles once.
	unsigned short					m_FrameCode;

	bool							m_bUpdatingEffects;
	bool							m_bRenderParticleEffects;

	// All the active effects.
	std::list<CParticleEffectBinding*>		m_Effects;

	// all the active effects using the new particle interface
	ks::intrusive_dlist< CNewParticleEffect > m_NewEffects;
	ks::intrusive_dlist< CNonDrawingParticleSystem > m_NonDrawingParticleSystems;

	std::vector< IClientParticleListener * > m_effectListeners;

	IMaterialSystem					*m_pMaterialSystem;

	// Store the concatenated modelview matrix
	VMatrix							m_mModelView;
	
	std::vector< std::unique_ptr< CParticleSubTextureGroup > >	m_SubTextureGroups;	// lookup by group name
	ks::idict< std::unique_ptr< CParticleSubTexture > >	m_SubTextures;		// lookup by material name
	CParticleSubTexture m_DefaultInvalidSubTexture; // Used when they specify an invalid material name.

	std::map< const char*, CreateParticleEffectFN, ks::cstr_less > m_effectFactories;	// keys are typeid names

	int m_nToolParticleEffectId;

	IThreadPool *m_pThreadPool[2];

};

// Implement this class and register with CParticleMgr to receive particle effect add/remove notification
class IClientParticleListener
{
public:
	virtual void OnParticleEffectAdded( IParticleEffect *pEffect ) = 0;
	virtual void OnParticleEffectRemoved( IParticleEffect *pEffect ) = 0;
};

// Helper functions to abstract out the particle testbed app.
float	Helper_GetTime();
float	Helper_GetFrameTime();
float	Helper_RandomFloat( float minVal, float maxVal );
int		Helper_RandomInt( int minVal, int maxVal );

// ------------------------------------------------------------------------ //
// CParticleMgr inlines
// ------------------------------------------------------------------------ //

// ------------------------------------------------------------------------ //
// CParticleEffectBinding inlines.
// ------------------------------------------------------------------------ //

// ------------------------------------------------------------------------ //
// GLOBALS
// ------------------------------------------------------------------------ //

CParticleMgr *ParticleMgr();

//-----------------------------------------------------------------------------
// StandardParticle_t; this is just one type of particle
// effects may implement their own particle data structures
//-----------------------------------------------------------------------------

struct StandardParticle_t : public Particle
{
	// Color and alpha values are 0 - 1
	void			SetColor(float r, float g, float b);
	void			SetAlpha(float a);

	Vector			m_Velocity;
	
	// How this is used is up to the effect's discretion. Some use it for how long it has been alive
	// and others use it to count down until the particle disappears.
	float			m_Lifetime;

	unsigned char	m_EffectData;	// Data specific to the IParticleEffect. This can be used to distinguish between
									// different types of particles the effect is simulating.
	unsigned short	m_EffectDataWord;

	unsigned char	m_Color[4];		// RGBA - not all effects need to use this.
};

// ------------------------------------------------------------------------ //
// Transform a particle.
// ------------------------------------------------------------------------ //

inline void TransformParticle(const VMatrix &vMat, const Vector &vIn, Vector &vOut)
{
	//vOut = vMat.VMul4x3(vIn);
	vOut.x = vMat.m[0][0]*vIn.x + vMat.m[0][1]*vIn.y + vMat.m[0][2]*vIn.z + vMat.m[0][3];
	vOut.y = vMat.m[1][0]*vIn.x + vMat.m[1][1]*vIn.y + vMat.m[1][2]*vIn.z + vMat.m[1][3];
	vOut.z = vMat.m[2][0]*vIn.x + vMat.m[2][1]*vIn.y + vMat.m[2][2]*vIn.z + vMat.m[2][3];
}

// ------------------------------------------------------------------------ //
// CEffectMaterial inlines
// ------------------------------------------------------------------------ //

//-----------------------------------------------------------------------------
// List functions.
//-----------------------------------------------------------------------------

inline void UnlinkParticle( Particle *pParticle )
{
	pParticle->m_pPrev->m_pNext = pParticle->m_pNext;
	pParticle->m_pNext->m_pPrev = pParticle->m_pPrev;
}

inline void InsertParticleBefore( Particle *pInsert, Particle *pNext )
{
	// link pCur before pPrev
	pInsert->m_pNext = pNext;
	pInsert->m_pPrev = pNext->m_pPrev;
	pInsert->m_pNext->m_pPrev = pInsert->m_pPrev->m_pNext = pInsert;
}

inline void InsertParticleAfter( Particle *pInsert, Particle *pPrev )
{
	pInsert->m_pPrev = pPrev;
	pInsert->m_pNext = pPrev->m_pNext;

	pInsert->m_pNext->m_pPrev = pInsert->m_pPrev->m_pNext = pInsert;
}

inline void SwapParticles( Particle *pPrev, Particle *pCur )
{
	// unlink pCur
	UnlinkParticle( pCur );
	InsertParticleBefore( pCur, pPrev );
}

class CParticleRenderIterator;
class CParticleSimulateIterator;
class CBaseEntity;
class IParticleEffect;
struct Particle;

// Purpose: provide client-side access to the new particle system, with similar
// usage to CSimpleEmitter

typedef CHandle<CBaseEntity> EHANDLE;

//-----------------------------------------------------------------------------
// Particle effect
//-----------------------------------------------------------------------------
class CNewParticleEffect : public IParticleEffect, public CParticleCollection, public CDefaultClientRenderable
{
public:
	DECLARE_CLASS_NOBASE( CNewParticleEffect );
	private:
		ks::tracked_ref_list< CNewParticleEffect > m_References;
		template < class T > friend class ks::tracked_ref;;

public:
	template < class > friend class ks::intrusive_ptr;
	friend class CParticleSystemQuery;

	// list management
	CNewParticleEffect *m_pNext;
	CNewParticleEffect *m_pPrev;

	// Call this before adding a bunch of particles to give it a rough estimate of where
	// your particles are for sorting amongst other translucent entities.
	void SetSortOrigin( const Vector &vSortOrigin );
	bool ShouldDraw( void );
	virtual int GetRenderFlags( void );
	const QAngle& GetRenderAngles( void );
	const matrix3x4_t& RenderableToWorldTransform();
	void GetRenderBounds( Vector& mins, Vector& maxs );
	virtual bool	ShouldDrawForSplitScreenUser( int nSlot );

	// check if the new bounds of the particle system needs its client-leaf info needs to be updated
	void DetectChanges( void );
	const Vector &GetRenderOrigin( void );
	PMaterialHandle GetPMaterial( const char *name );
	bool RecalculateBoundingBox();
	Particle* AddParticle( unsigned int particleSize, PMaterialHandle material, const Vector &origin );
	const char *GetEffectName();
	void SetDontRemove( bool bSet );
	void SetDrawn( bool bDrawn );
	void SetFirstFrameFlag( bool bFirst );
	void SetNeedsBBoxUpdate( bool bNeedsUpdate );
	void SetAutoUpdateBBox( bool bNeedsUpdate );
	void SetRemoveFlag( void );
	bool GetRemoveFlag( void );
	bool GetFirstFrameFlag( void );
	bool GetNeedsBBoxUpdate( void );
	bool GetAutoUpdateBBox( void );
	bool ShouldPerformCullCheck() const;
	void MarkShouldPerformCullCheck( bool bEnable );
	CBaseEntity *GetOwner( void ) { return m_hOwner; }
	void SetOwner( CBaseEntity *pOwner );
	CNewParticleEffect* ReplaceWith( const char *pParticleSystemName );

	static CNewParticleEffect *Create( CBaseEntity *pOwner, const char *pParticleSystemName,
												 const char *pDebugName = nullptr );
	static CNewParticleEffect *Create( CBaseEntity *pOwner, CParticleSystemDefinition *pDef,
												 const char *pDebugName = nullptr );
	static CNewParticleEffect *CreatePrecached( CBaseEntity *pOwner, int nPrecacheIndex,
												 const char *pDebugName = nullptr );	
	static CNewParticleEffect *CreateOrAggregate( CBaseEntity *pOwner, const char *pParticleSystemName,
												  Vector const &vecAggregatePosition, const char *pDebugName = nullptr, 
												  int nSplitScreenUser = -1 );
	static CNewParticleEffect *CreateOrAggregate( CBaseEntity *pOwner, CParticleSystemDefinition *pDef,
												  Vector const &vecAggregatePosition, const char *pDebugName = nullptr,
												  int nSplitScreenUser = -1 );
	static CNewParticleEffect *CreateOrAggregatePrecached( CBaseEntity *pOwner, int nPrecacheIndex,
														   Vector const &vecAggregatePosition,
														   const char *pDebugName = nullptr,
														   int nSplitScreenUser = -1 );

	static void RemoveParticleEffect( int nPrecacheIndex );

	virtual int DrawModel( int flags, const RenderableInstance_t &instance );

	bool SetupBones( matrix3x4a_t *pBoneToWorldOut, int nMaxBones, int boneMask, float currentTime );

	void SetDrawOnlyForSplitScreenUser( int nSlot );

	// CParticleCollection overrides
public:
	void StopEmission( bool bInfiniteOnly = false, bool bRemoveAllParticles = false, bool bWakeOnStop = false, bool bPlayEndCap = false );
	void SetDormant( bool bDormant );
	void SetControlPoint( int nWhichPoint, const Vector &v );
	void SetControlPointEntity( int nWhichPoint, CBaseEntity *pEntity );
	void SetControlPointOrientation( int nWhichPoint, const Quaternion &q );
	void SetControlPointOrientation( int nWhichPoint, const Vector &forward, const Vector &right, const Vector &up );
	void SetControlPointForwardVector( int nWhichPoint, const Vector &v );
	void SetControlPointUpVector( int nWhichPoint, const Vector &v );
	void SetControlPointRightVector( int nWhichPoint, const Vector &v );

	[[gnu::always_inline]] inline EHANDLE const &GetControlPointEntity( int nWhichPoint )
	{
		return m_hControlPointOwners[ nWhichPoint ];
	}

// IParticleEffect overrides
public:

	virtual void	SimulateParticles( CParticleSimulateIterator *pIterator )
	{
	}
	virtual void	RenderParticles( CParticleRenderIterator *pIterator )
	{
	}		

	virtual void				SetParticleCullRadius( float radius );
	virtual void				NotifyRemove( void );
	virtual const Vector &		GetSortOrigin( void );

//	virtual void				NotifyDestroyParticle( Particle* pParticle );
	virtual void				Update( float flTimeDelta );

	// All Create() functions should call this so the effect deletes itself
	// when it is removed from the particle manager.
	void SetDynamicallyAllocated( bool bDynamic = true );

	virtual bool				ShouldSimulate() const { return m_bSimulate; }
	virtual void				SetShouldSimulate( bool bSim ) { m_bSimulate = bSim; }

	void SetToolRecording( bool bRecord );

	int AllocateToolParticleEffectId();
	int GetToolParticleEffectId() const;

	virtual ~CNewParticleEffect();

protected:
	CNewParticleEffect( CBaseEntity *pOwner, const char *pEffectName );
	CNewParticleEffect( CBaseEntity *pOwner, CParticleSystemDefinition *pEffect );
	CNewParticleEffect( CBaseEntity *pOwner, int nPrecacheIndex );

	// Returns nonzero if Release() has been called.
	int		IsReleased();
	
	// Used to track down bugs.
	const char	*m_pDebugName;

	bool		m_bDontRemove : 1;
	bool		m_bRemove : 1;
	bool		m_bDrawn : 1;
	bool		m_bNeedsBBoxUpdate : 1;
	bool		m_bIsFirstFrame : 1;
	bool		m_bAutoUpdateBBox : 1;
	bool		m_bAllocated : 1;
	bool		m_bSimulate : 1;
	bool		m_bRecord : 1;
	bool		m_bShouldPerformCullCheck : 1;

	// if a particle system is created through the non-aggregation entry point, we can't aggregate into it
	bool        m_bDisableAggregation : 1;

	int			m_nToolParticleEffectId;
	Vector		m_vSortOrigin;
	EHANDLE		m_hOwner;
	EHANDLE     m_hControlPointOwners[MAX_PARTICLE_CONTROL_POINTS];

	// holds the min/max bounds used to manage this thing in the client leaf system
	Vector		m_LastMin;
	Vector		m_LastMax;

	int			m_nSplitScreenUser; // -1 means don't care
	Vector m_vecAggregationCenter;							// origin for aggregation if aggregation enabled

private:
	// Update the reference count.
	void		AddRef();
	void		Release();
	void		RecordControlPointOrientation( int nWhichPoint );
	void		Construct();
	void		RecordCreation();
	
	int			m_RefCount;		// When this goes to zero and the effect has no more active
								// particles, (and it's dynamically allocated), it will delete itself.

	matrix3x4a_t m_DrawModelMatrix;
	
	CNewParticleEffect( const CNewParticleEffect & ); // not defined, not accessible
};

//-----------------------------------------------------------------------------
// Inline methods
//-----------------------------------------------------------------------------

//--------------------------------------------------------------------------------
// If you use an HPARTICLEFFECT instead of a cnewparticleeffect *, you get a pointer
// which will go to null when the effect is deleted
//--------------------------------------------------------------------------------
typedef ks::tracked_ref<CNewParticleEffect> HPARTICLEFFECT;

class CUserCmd;
class CBaseEntity;
class CBasePlayer;

using namespace ::ks::reflect;


//-----------------------------------------------------------------------------
// Purpose: Implements prediction in the client .dll
//-----------------------------------------------------------------------------
class CPrediction : public IPrediction
{
// Construction
public:
	DECLARE_CLASS_GAMEROOT( CPrediction, IPrediction );

					CPrediction( void );
	virtual			~CPrediction( void );

	virtual void	Init( void );
	virtual void	Shutdown( void );

// Implement IPrediction
public:

	virtual void	Update
					( 
						int startframe,		// World update ( un-modded ) most recently received
						bool validframe,		// Is frame data valid
						int incoming_acknowledged, // Last command acknowledged to have been run by server (un-modded)
						int outgoing_command	// Last command (most recent) sent to server (un-modded)
					);

	virtual void	OnReceivedUncompressedPacket( void );

	virtual void	PreEntityPacketReceived( int commands_acknowledged, int current_world_update_packet, int server_ticks_elapsed );
	virtual void	PostEntityPacketReceived( void );
	virtual void	PostNetworkDataReceived( int commands_acknowledged );

	virtual bool	InPrediction( void ) const;
	virtual bool	IsFirstTimePredicted( void ) const;
	virtual int		GetLastAcknowledgedCommandNumber( void ) const;

	virtual int		GetIncomingPacketNumber( void ) const;

	float			GetIdealPitch( int nSlot ) const;

	// The engine needs to be able to access a few predicted values
	virtual void	GetViewOrigin( Vector& org );
	virtual void	SetViewOrigin( Vector& org );
	virtual void	GetViewAngles( QAngle& ang );
	virtual void	SetViewAngles( QAngle& ang );

	virtual void	GetLocalViewAngles( QAngle& ang );
	virtual void	SetLocalViewAngles( QAngle& ang );

	virtual void	CheckMovingGround( CBasePlayer *player, double frametime );
	virtual void	RunCommand( CBasePlayer *player, CUserCmd *ucmd, IMoveHelper *moveHelper );

	float			GetSavedTime() const;

// Internal
protected:
	virtual void	SetupMove( CBasePlayer *player, CUserCmd *ucmd, IMoveHelper *pHelper, CMoveData *move );
	virtual void	FinishMove( CBasePlayer *player, CUserCmd *ucmd, CMoveData *move );
	virtual void	SetIdealPitch ( int nSlot, CBasePlayer *player, const Vector& origin, const QAngle& angles, const Vector& viewheight );

	virtual void	CheckError( int nSlot, CBasePlayer *player, int commands_acknowledged );

	// Called before and after any movement processing
	void			StartCommand( CBasePlayer *player, CUserCmd *cmd );
	void			FinishCommand( CBasePlayer *player );

	// Helpers to call pre and post think for player, and to call think if a think function is set
	void			RunPreThink( CBasePlayer *player );
	void			RunThink (CBasePlayer *ent, double frametime );
	void			RunPostThink( CBasePlayer *player );

private:
	virtual void	_Update
					( 
						int nSlot,
						bool received_new_world_update,
						bool validframe,		// Is frame data valid
						int incoming_acknowledged, // Last command acknowledged to have been run by server (un-modded)
						int outgoing_command	// Last command (most recent) sent to server (un-modded)
					);

	// Actually does the prediction work, returns false if an error occurred
	bool			PerformPrediction( int nSlot, CBasePlayer *localPlayer, bool received_new_world_update, int incoming_acknowledged, int outgoing_command );

	void			ShiftIntermediateDataForward( int nSlot, int slots_to_remove, int previous_last_slot );
	void			ShiftFirstPredictedIntermediateDataForward( int nSlot, int slots_to_remove );
	void			RestoreEntityToPredictedFrame( int nSlot, int predicted_frame );
	int				ComputeFirstCommandToExecute( int nSlot, bool received_new_world_update, int incoming_acknowledged, int outgoing_command );

	void			DumpEntity( CBaseEntity *ent, int commands_acknowledged );

	void			ShutdownPredictables( void );
	void			ReinitPredictables( void );

	void			RemoveStalePredictedEntities( int nSlot, int last_command_packet );
	void			RestoreOriginalEntityState( int nSlot );
	void			RunSimulation( int current_command, float curtime, CUserCmd *cmd, CBasePlayer *localPlayer );
	void			Untouch( int nSlot );
	void			StorePredictionResults( int nSlot, int predicted_frame );
	bool			ShouldDumpEntity( CBaseEntity *ent );

	void			SmoothViewOnMovingPlatform( CBasePlayer *pPlayer, Vector& offset );
	void			ResetSimulationTick();
	void			ShowPredictionListEntry( int listRow, int showlist, CBaseEntity *ent, int &totalsize, int &totalsize_intermediate );
	void			FinishPredictionList( int listRow, int showlist, int totalsize, int totalsize_intermediate );
	void			CheckPredictConvar();

// Data
protected:
	// Last object the player was standing on
	CHandle< CBaseEntity > m_hLastGround;
private:
	bool			m_bInPrediction;
	bool			m_bOldCLPredictValue;
	bool			m_bEnginePaused;

	int				m_nPreviousStartFrame;
	int				m_nIncomingPacketNumber;

	float			m_flLastServerWorldTimeStamp;

	// Last network origin for local player
	struct Split_t
	{
		Split_t()
		{
			m_bFirstTimePredicted = false;
			m_nCommandsPredicted = 0;
			m_nServerCommandsAcknowledged = 0;
			m_bPreviousAckHadErrors = false;
			m_flIdealPitch = 0.0f;
			m_nLastCommandAcknowledged = 0;
			m_bPreviousAckErrorTriggersFullLatchReset = false;
		}

		bool			m_bFirstTimePredicted;
		int				m_nCommandsPredicted;
		int				m_nServerCommandsAcknowledged;
		int				m_bPreviousAckHadErrors;
		float			m_flIdealPitch;
		int				m_nLastCommandAcknowledged;
		bool			m_bPreviousAckErrorTriggersFullLatchReset;
		std::vector< CHandle< CBaseEntity > > m_EntsWithPredictionErrorsInLastAck;
		bool			m_bPerformedTickShift;

	};

	Split_t				m_Split[ MAX_SPLITSCREEN_PLAYERS ];

	CGlobalVarsBase	m_SavedVars;

	bool			m_bPlayerOriginTypedescriptionSearched;
	std::vector< const typedescription_t * >	m_PlayerOriginTypeDescription; // A vector in cases where the .x, .y, and .z are separately listed

};
 
extern CPrediction *prediction;

// point on plane side epsilon

inline constexpr int MAX_FORWARD = 6;

// Use smoothing if error is > this
inline constexpr auto MIN_CORRECTION_DISTANCE = 0.25f;

// Complain if error is > this and we have cl_showerror set
inline constexpr auto MIN_PREDICTION_EPSILON = 0.5f;

// Above this is assumed to be a teleport, don't smooth, etc.
inline constexpr auto MAX_PREDICTION_ERROR = 64.0f;

class CBaseEntity;

typedef CHandle<CBaseEntity> EHANDLE;

// #define COPY_CHECK_STRESSTEST

typedef void ( *FN_FIELD_COMPARE )( const char *classname, const char *fieldname, const char *fieldtype,
	bool networked, bool noterrorchecked, bool differs, bool withintolerance, const char *value );

// Each datamap_t is broken down into two flattened arrays of fields, 
//  one for PC_NETWORKED_DATA and one for PC_NON_NETWORKED_ONLY (optimized_datamap_t::datamapinfo_t::flattenedoffsets_t)
// Each flattened array is sorted by offset for better cache performance
// Finally, contiguous "runs" off offsets are precomputed (optimized_datamap_t::datamapinfo_t::datacopyruns_t) for fast copy operations

// A data run is a set of DEFINE_PRED_FIELD fields in a c++ object which are contiguous and can be processing
//  using a single memcpy operation
struct datarun_t
{
	datarun_t();

	// Indices of start/end fields in the flattened typedescription_t list
	int m_nStartFlatField;
	int m_nEndFlatField;

	// Offsets for run in the packed/unpacked data (I think the run starts need to be properly aligned)
	int m_nStartOffset[ TD_OFFSET_COUNT ];
	int m_nLength;
};

struct datacopyruns_t 
{
public:
	std::vector< datarun_t > m_vecRuns;
};

struct flattenedoffsets_t
{
	std::vector< typedescription_t >	m_Flattened;
	int								m_nPackedSize; // Contiguous memory to pack all of these together for TD_OFFSET_PACKED
	int								m_nPackedStartOffset;
};

struct datamapinfo_t
{
	// Flattened list, with FIELD_EMBEDDED, FTYPEDESC_PRIVATE, 
	//  and FTYPEDESC_OVERRIDE (overridden) fields removed
	flattenedoffsets_t	m_Flat;
	datacopyruns_t		m_CopyRuns;
};

struct optimized_datamap_t
{
	// Optimized info for PC_NON_NETWORKED and PC_NETWORKED data
	datamapinfo_t	m_Info[ PC_COPYTYPE_COUNT ];
};

inline optimized_datamap_t *OptimizedDataMap( const datamap_t *pMap )
{
	return static_cast<optimized_datamap_t *>( pMap->m_pOptimizedDataMap );
}

class CPredictionCopy
{
public:
	typedef enum
	{
		DIFFERS = 0,
		IDENTICAL,
		WITHINTOLERANCE,
	} difftype_t;

	typedef enum
	{
		TRANSFERDATA_COPYONLY = 0,  // Data copying only (uses runs)
		TRANSFERDATA_ERRORCHECK_NOSPEW, // Checks for errors, returns after first error found
		TRANSFERDATA_ERRORCHECK_SPEW,   // checks for errors, reports all errors to console
		TRANSFERDATA_ERRORCHECK_DESCRIBE, // used by hud_pdump, dumps values, etc, for all fields
	} optype_t;

	CPredictionCopy( int type, byte *dest, bool dest_packed, const byte *src, bool src_packed,
		optype_t opType, FN_FIELD_COMPARE func = nullptr );
	
	int		TransferData( const char *operation, int entindex, datamap_t *dmap );

	static bool PrepareDataMap( datamap_t *dmap );
	static const typedescription_t *FindFlatFieldByName( const char *fieldname, const datamap_t *dmap );
private:

	// Operations:
	void	TransferDataCopyOnly( const datamap_t *dmap );
	void	TransferDataErrorCheckNoSpew( char const *pchOperation, const datamap_t *dmap );
	void	TransferDataErrorCheckSpew( char const *pchOperation, const datamap_t *dmap );
	void	TransferDataDescribe( char const *pchOperation, const datamap_t *dmap );

	// Report function
	void	ReportFieldsDiffer( const datamap_t *pCurrentMap, const typedescription_t *pField, const char *fmt, ... );
	void	OutputFieldDescription( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t dt, const char *fmt, ... );
	
	// Helper for TransferDataCopyOnly
	void	CopyFlatFieldsUsingRuns( const datamap_t *pCurrentMap, int nPredictionCopyType );
	void	CopyFlatFields( const datamap_t *pCurrentMap, int nPredictionCopyType );
	template< class T >
	[[gnu::always_inline]] inline void CopyField( difftype_t difftype, T *outvalue, const T *invalue, int count );

	// Helper for TransferDataErrorCheckNoSpew
	void	ErrorCheckFlatFields_NoSpew( const datamap_t *pCurrentMap, int nPredictionCopyType );
	template< class T >
	[[gnu::always_inline]] inline void ProcessField_Compare_NoSpew( const datamap_t *pCurrentMap, const typedescription_t *pField, const T *pOutputData, const T *pInputData, int fieldSize );

	// Helper for TransferDataErrorCheckSpew
	void	ErrorCheckFlatFields_Spew( const datamap_t *pCurrentMap, int nPredictionCopyType );
	template< class T >
	[[gnu::always_inline]] inline void ProcessField_Compare_Spew( const datamap_t *pCurrentMap, const typedescription_t *pField, const T *pOutputData, const T *pInputData, int fieldSize );

	// Helper for TransferDataDescribe
	void	DescribeFields( const std::vector< const datamap_t * > &vecGroups, const datamap_t *pCurrentMap, int nPredictionCopyType );
	// Main entry point
	template< class T >
	[[gnu::always_inline]] inline void ProcessField_Describe( const datamap_t *pCurrentMap, const typedescription_t *pField, const T *pOutputData, const T *pInputData, int fieldSize );

	// Helpers for entity field watcher
	void	DetermineWatchField( const char *operation, int entindex, const datamap_t *dmap );
	void	WatchMsg( const typedescription_t *pField, const char *fmt, ... );
	void	DumpWatchField( const typedescription_t *pField, const byte *outvalue, int count );
	template< class T >
	[[gnu::always_inline]] inline void WatchField( const typedescription_t *pField, const T *outvalue, int count );

	// Helper for ErrorCheck ops
	template< class T >
	[[gnu::always_inline]] inline difftype_t CompareField( const typedescription_t *pField, const T *outvalue, const T *invalue, int count );
	// Used by TRANSFERDATA_ERRORCHECK_SPEW and by TRANSFERDATA_ERRORCHECK_DESCRIBE
	template< class T >
	[[gnu::always_inline]] inline void DescribeField( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t difftype, const T *outvalue, const T *invalue, int count );

private:

	optype_t		m_OpType;
	int				m_nType;
	byte			*m_pDest;
	const byte		*m_pSrc;
	int				m_nDestOffsetIndex;
	int				m_nSrcOffsetIndex;
	int				m_nErrorCount;
	int				m_nEntIndex;

	FN_FIELD_COMPARE	m_FieldCompareFunc;

	const typedescription_t	 *m_pWatchField;
	char const			*m_pOperation;

};

typedef void (*FN_FIELD_DESCRIPTION)( const char *classname, const char *fieldname, const char *fieldtype,
	bool networked, const char *value );

// 
// Compare methods
// 
// Specializations
template<> [[gnu::always_inline]] inline CPredictionCopy::difftype_t CPredictionCopy::CompareField( const typedescription_t *pField, const float *outvalue, const float *invalue, int count );
template<> [[gnu::always_inline]] inline CPredictionCopy::difftype_t CPredictionCopy::CompareField( const typedescription_t *pField, const Vector *outvalue, const Vector *invalue, int count );
template<> [[gnu::always_inline]] inline CPredictionCopy::difftype_t CPredictionCopy::CompareField( const typedescription_t *pField, const Quaternion *outvalue, const Quaternion *invalue, int count );
template<> [[gnu::always_inline]] inline CPredictionCopy::difftype_t CPredictionCopy::CompareField( const typedescription_t *pField, const char *outvalue, const char *invalue, int count );
template<> [[gnu::always_inline]] inline CPredictionCopy::difftype_t CPredictionCopy::CompareField( const typedescription_t *pField, const EHANDLE *outvalue, const EHANDLE *invalue, int count );
template<> [[gnu::always_inline]] inline CPredictionCopy::difftype_t CPredictionCopy::CompareField( const typedescription_t *pField, const color32 *outvalue, const color32 *invalue, int count );

//
// Describe Methods
//

// Specializations
template<> [[gnu::always_inline]] inline void CPredictionCopy::DescribeField( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t difftype, const short *outvalue, const short *invalue, int count );
template<> [[gnu::always_inline]] inline void CPredictionCopy::DescribeField( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t difftype, const int *outvalue, const int *invalue, int count );
template<> [[gnu::always_inline]] inline void CPredictionCopy::DescribeField( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t difftype, const bool *outvalue, const bool *invalue, int count );
template<> [[gnu::always_inline]] inline void CPredictionCopy::DescribeField( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t difftype, const float *outvalue, const float *invalue, int count );
template<> [[gnu::always_inline]] inline void CPredictionCopy::DescribeField( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t difftype, const char *outvalue, const char *invalue, int count );
template<> [[gnu::always_inline]] inline void CPredictionCopy::DescribeField( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t difftype, const Vector* outValue, const Vector *inValue, int count );
template<> [[gnu::always_inline]] inline void CPredictionCopy::DescribeField( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t difftype, const Quaternion* outValue, const Quaternion *inValue, int count );
template<> [[gnu::always_inline]] inline void CPredictionCopy::DescribeField( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t difftype, const EHANDLE *outvalue, const EHANDLE *invalue, int count );
template<> [[gnu::always_inline]] inline void CPredictionCopy::DescribeField( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t difftype, const color32 *outvalue, const color32 *invalue, int count );
template<> [[gnu::always_inline]] inline void CPredictionCopy::DescribeField( const datamap_t *pCurrentMap, const typedescription_t *pField, difftype_t difftype, const uint8 *outvalue, const uint8 *invalue, int count );

//
// Watch Methods
//
// Specializations
template<> [[gnu::always_inline]] inline void CPredictionCopy::WatchField( const typedescription_t *pField, const short *outvalue,int count );
template<> [[gnu::always_inline]] inline void CPredictionCopy::WatchField( const typedescription_t *pField, const int *outvalue, int count );
template<> [[gnu::always_inline]] inline void CPredictionCopy::WatchField( const typedescription_t *pField, const bool *outvalue, int count );
template<> [[gnu::always_inline]] inline void CPredictionCopy::WatchField( const typedescription_t *pField, const float *outvalue, int count );
template<> [[gnu::always_inline]] inline void CPredictionCopy::WatchField( const typedescription_t *pField, const Vector *outvalue, int count );
template<> [[gnu::always_inline]] inline void CPredictionCopy::WatchField( const typedescription_t *pField, const Quaternion *outvalue, int count );
template<> [[gnu::always_inline]] inline void CPredictionCopy::WatchField( const typedescription_t *pField, const EHANDLE *outvalue, int count );
template<> [[gnu::always_inline]] inline void CPredictionCopy::WatchField( const typedescription_t *pField, const char *outvalue, int count );
template<> [[gnu::always_inline]] inline void CPredictionCopy::WatchField( const typedescription_t *pField, const color32 *outvalue, int count );
//
// Copy Methods
//
// specializations
template<> [[gnu::always_inline]] inline void CPredictionCopy::CopyField( difftype_t difftype, char *outvalue, const char *invalue, int count );

template< class T >
[[gnu::always_inline]] inline void CPredictionCopy::ProcessField_Compare_NoSpew( const datamap_t *pCurrentMap, const typedescription_t *pField, const T *pOutputData, const T *pInputData, int fieldSize )
{
	difftype_t difftype = CompareField( pField, pOutputData, pInputData, fieldSize );
	if ( difftype == DIFFERS )
	{
		++m_nErrorCount;
	}
}

template< class T >
[[gnu::always_inline]] inline void CPredictionCopy::ProcessField_Compare_Spew( const datamap_t *pCurrentMap, const typedescription_t *pField, const T *pOutputData, const T *pInputData, int fieldSize )
{
	difftype_t difftype = CompareField( pField, pOutputData, pInputData, fieldSize );
	DescribeField( pCurrentMap, pField, difftype, pOutputData, pInputData, fieldSize );
}

template< class T >
[[gnu::always_inline]] inline void CPredictionCopy::ProcessField_Describe( const datamap_t *pCurrentMap, const typedescription_t *pField, const T *pOutputData, const T *pInputData, int fieldSize )
{
	difftype_t difftype = CompareField( pField, pOutputData, pInputData, fieldSize );
	DescribeField( pCurrentMap, pField, difftype, pOutputData, pInputData, fieldSize );
}

class CValueChangeTracker
{
public:
	CValueChangeTracker();

	void Reset();

	void StartTrack( char const *pchContext );
	void EndTrack();

	bool IsActive() const;

	void SetupTracking( CBaseEntity *ent, char const *pchFieldName );
	void ClearTracking();

	void Spew();

	CBaseEntity *GetEntity();

private:

	enum
	{
		eChangeTrackerBufSize = 128,
	};

	// Returns field size
	void				GetValue( char *buf, std::size_t bufsize );

	bool				m_bActive : 1;
	bool				m_bTracking : 1;
	EHANDLE				m_hEntityToTrack;
	const typedescription_t	*m_pTrackField;
	std::string			m_strFieldName;
	std::string			m_strContext;
	// First 128 bytes of data is all we will consider
	char				m_OrigValueBuf[ eChangeTrackerBufSize ];
	std::vector< std::string >	 m_History;
};

extern CValueChangeTracker *g_pChangeTracker;

class CValueChangeTrackerScope
{
public:
	CValueChangeTrackerScope( char const *pchContext );

	// Only calls Start/End if passed in entity matches entity to track
	CValueChangeTrackerScope( CBaseEntity *pEntity, char const *pchContext )
	{
		m_bCallEndTrack = g_pChangeTracker->GetEntity() == pEntity;
		if ( m_bCallEndTrack )
		{
			g_pChangeTracker->StartTrack( pchContext );
		}
	}

	~CValueChangeTrackerScope();
private:

	bool		m_bCallEndTrack;
};

class CNewParticleEffect;
class CBaseEntity;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------

// Argh: Server considers -1 to be an invalid attachment, whereas the client uses 0
inline constexpr auto INVALID_PARTICLE_ATTACHMENT = 0;

struct ParticleControlPoint_t
{
	ParticleControlPoint_t();

	int								iControlPoint;
	ParticleAttachment_t			iAttachType;
	int								iAttachmentPoint;
	Vector							vecOriginOffset;
	matrix3x4_t						matOffset;
	EHANDLE							hEntity;
};

struct ParticleEffectList_t
{
	ParticleEffectList_t()
	{
		pParticleEffect = nullptr;
	}

	std::vector< ParticleControlPoint_t >	pControlPoints;
	ks::intrusive_ptr<CNewParticleEffect>		pParticleEffect;
};

extern int GetAttachTypeFromString( const char *pszString );

//-----------------------------------------------------------------------------
// Encapsulates particle handling for an entity
//-----------------------------------------------------------------------------
class CParticleProperty
{
	[[= ks::reflect::NetTable{ .name = "DT_ParticleProperty", .base = false } ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS_NOBASE( CParticleProperty );
	DECLARE_EMBEDDED_NETWORKVAR();
	DECLARE_PREDICTABLE();

public:
	CParticleProperty();
	~CParticleProperty();

	void				Init( CBaseEntity *pEntity );
	CBaseEntity			*GetOuter( void ) { return m_pOuter; }
	int					GetAllParticleEffectRenderables( IClientRenderable **pOutput, int iMaxOutput ); //gets a list of all renderables used by this particle property

	// Effect Creation
	CNewParticleEffect *Create( const char *pszParticleName, ParticleAttachment_t iAttachType, const char *pszAttachmentName );
	CNewParticleEffect *Create( const char *pszParticleName, ParticleAttachment_t iAttachType, int iAttachmentPoint = -1, Vector vecOriginOffset = vec3_origin, matrix3x4_t *vecOffsetMatrix = nullptr );
	CNewParticleEffect *CreatePrecached( int nPrecacheIndex, ParticleAttachment_t iAttachType, int iAttachmentPoint = -1, Vector vecOriginOffset = vec3_origin, matrix3x4_t *vecOffsetMatrix = nullptr );
	void				AddControlPoint( CNewParticleEffect *pEffect, int iPoint, CBaseEntity *pEntity, ParticleAttachment_t iAttachType, const char *pszAttachmentName = nullptr, Vector vecOriginOffset = vec3_origin, matrix3x4_t *vecOffsetMatrix = nullptr );
	void				AddControlPoint( int iEffectIndex, int iPoint, CBaseEntity *pEntity, ParticleAttachment_t iAttachType, int iAttachmentPoint = -1, Vector vecOriginOffset = vec3_origin, matrix3x4_t *vecOffsetMatrix = nullptr );

	inline void			SetControlPointParent( CNewParticleEffect *pEffect, int whichControlPoint, int parentIdx );
	void				SetControlPointParent( int iEffectIndex, int whichControlPoint, int parentIdx );

	// Commands
	void				StopEmission( CNewParticleEffect *pEffect = nullptr, bool bWakeOnStop = false, bool bDestroyAsleepSystems = false, bool bForceRemoveInstantly = false, bool bPlayEndCap = false );
	void				StopEmissionAndDestroyImmediately( CNewParticleEffect *pEffect = nullptr );

	// kill all particle systems involving a given entity for their control points
	void				StopParticlesInvolving( CBaseEntity *pEntity, bool bForceRemoveInstantly = false );
	void				StopParticlesNamed( const char *pszEffectName, bool bForceRemoveInstantly = false, int nSplitScreenPlayerSlot = -1 ); ///< kills all particles using the given definition name

	// Particle System hooks
	void				OnParticleSystemUpdated( CNewParticleEffect *pEffect, float flTimeDelta );
	void				OnParticleSystemDeleted( CNewParticleEffect *pEffect );

	void				OwnerSetDormantTo( bool bDormant );

	// Used to replace a particle effect with a different one; attaches the control point updating to the new one
	void				ReplaceParticleEffect( CNewParticleEffect *pOldEffect, CNewParticleEffect *pNewEffect );

	// Debugging
	void				DebugPrintEffects( void );

	bool				IsValidEffect( const CNewParticleEffect *pEffect ); //is this effect still alive?

	int					FindEffect( CNewParticleEffect *pEffect );
	int					FindEffect( const char *pEffectName );

private:
	CNewParticleEffect *Create( CParticleSystemDefinition *pDef, ParticleAttachment_t iAttachType, int iAttachmentPoint, Vector vecOriginOffset, matrix3x4_t *matOffset = nullptr );
	int					GetParticleAttachment( CBaseEntity *pEntity, const char *pszAttachmentName, const char *pszParticleName );
	void				UpdateParticleEffect( ParticleEffectList_t *pEffect, bool bInitializing = false, int iOnlyThisControlPoint = -1 );
	void				UpdateControlPoint( ParticleEffectList_t *pEffect, int iPoint, bool bInitializing );
	inline CNewParticleEffect *GetParticleEffectFromIdx( int idx );

private:
	CBaseEntity *m_pOuter;
	std::vector< ParticleEffectList_t >	m_ParticleEffects;
	int			m_iDormancyChangedAtFrame;

	friend class CBaseEntity;
};

class ParticleDraw;

class CParticleEffectBinding;
class CParticleSubTexture;



// FIXME: Should we just pass the Particle draw members directly as
// arguments to IParticleEffect::SimulateAndRender?

// This file defines and implements the ParticleDraw class, which is used
// by ParticleEffects to render particles. It simply stores render + simulation
// state
//


class ParticleDraw
{
friend class CParticleEffectBinding;

public:

					ParticleDraw();

	void			Init( CMeshBuilder *pMeshBuilder, IMaterial *pMaterial, float fTimeDelta );

	// Time delta..
	float			GetTimeDelta() const;

	// Get the material being used (mostly useful for getting the tcoord padding).
	//IMaterial*		GetPMaterial();

	// This can return NULL if the particle system is only being simulated.
	CMeshBuilder*	GetMeshBuilder();

	CParticleSubTexture	*m_pSubTexture;

private:
	CMeshBuilder	*m_pMeshBuilder;
	IMaterial		*m_pMaterial;
	float			m_fTimeDelta;
};

// ------------------------------------------------------------------------- //
// Inlines
// ------------------------------------------------------------------------- //

ParticleDraw::ParticleDraw()
{
	m_pMaterial = 0;
}

void ParticleDraw::Init( CMeshBuilder *pMeshBuilder, IMaterial *pMaterial, float fTimeDelta )
{
	m_pMeshBuilder = pMeshBuilder;
	m_pMaterial = pMaterial;
	m_fTimeDelta = fTimeDelta;
}

float ParticleDraw::GetTimeDelta() const
{
	return m_fTimeDelta;
}

CMeshBuilder* ParticleDraw::GetMeshBuilder()
{
	return m_pMeshBuilder;
}

inline constexpr int MAX_COLLISION_PLANES = 6;

//
// CBaseSimpleCollision
//

class CBaseSimpleCollision
{
public:

	CBaseSimpleCollision( void );

	static	CBaseSimpleCollision *Create( void )	{	return	new CBaseSimpleCollision;	}

	virtual void	Setup( const Vector &origin, float speed, float gravity );
	virtual void	TraceLine( const Vector &start, const Vector &end, trace_t *pTrace, bool coarse = true );

	void	ClearActivePlanes( void	);

protected:

	virtual	void	TestForPlane( const Vector &start, const Vector &dir, float speed, float gravity );
	virtual	void	ConsiderPlane( cplane_t *plane );

	VPlane	m_collisionPlanes[MAX_COLLISION_PLANES];
	int		m_nActivePlanes;
};

//
// CParticleCollision
//

class CParticleCollision : public CBaseSimpleCollision
{
public:

	CParticleCollision( void );

	static	CParticleCollision *Create( void )	{	return	new CParticleCollision;	}

	virtual void	Setup( const Vector &origin, const Vector *dir, float angularSpread, float minSpeed, float maxSpeed, float gravity, float dampen );
	virtual bool	MoveParticle( Vector &origin, Vector &velocity, float *rollDelta, float timeDelta, trace_t *pTrace );

	void	SetGravity( float gravity )					{	m_flGravity = gravity;			}
	void	SetCollisionDampen( float dampen )			{	m_flCollisionDampen = dampen;	}
	void	SetAngularCollisionDampen( float dampen )	{	m_flAngularCollisionDampen = dampen;}

protected:

	float	m_flGravity;
	float	m_flCollisionDampen;
	float	m_flAngularCollisionDampen;
};

class CEffectMaterial;
class CParticleEffectBinding;
class CParticleMgr;
struct Particle;
class ParticleDraw;

// Purpose: Client side CTFTeam class

inline constexpr auto NUM_PARTICLES_PER_BATCH = 200;
inline constexpr auto MAX_TOTAL_PARTICLES = 2048; // Max particles in the world

//
// Iterate the particles like this:
//
// Particle *pCur = pIterator->GetFirst();
// while ( pCur )
// {
//     ... render the particle here and figure out the sort key and position
//     pCur = pIterator->GetNext( sortKey, pCur->m_Pos );
// }
//
class CParticleRenderIterator
{
friend class CParticleMgr;
friend class CParticleEffectBinding;
public:
	CParticleRenderIterator();

	// The sort key is used to sort the particles incrementally as they're rendered.
	// They only get sorted in the main rendered view (ie: not in reflections or monitors).
	// These return const because you should only modify particles during their simulation.
	const Particle* GetFirst();
	const Particle* GetNext( float sortKey );

	// Use this to render. This can return NULL, in which case you shouldn't render.
	// This being NULL is a carryover from when particles rendered and simulated together and
	// it should GO AWAY SOON!
	ParticleDraw* GetParticleDraw() const;

private:

	void TestFlushBatch();

private:
	// Set by CParticleMgr.
	CParticleEffectBinding *m_pEffectBinding;
	CEffectMaterial *m_pMaterial;
	ParticleDraw *m_pParticleDraw;
	CMeshBuilder *m_pMeshBuilder;
	IMesh *m_pMesh;
	bool m_bBucketSort;
	IMaterialSystem *m_pMaterialSystem; // Needed when we have to break particles into multiple batches
	
	// Output after rendering.
	float m_MinZ;
	float m_MaxZ;
	float m_zCoords[MAX_TOTAL_PARTICLES];
	int m_nZCoords;
	
	Particle *m_pCur;
	bool m_bGotFirst;
	float m_flPrevZ;
	int m_nParticlesInCurrentBatch;
};

//
// Iterate the particles like this:
//
// Particle *pCur = pIterator->GetFirst();
// while ( pCur )
// {
//     ... simulate here.. call pIterator->RemoveParticle if you want the particle to go away
//     pCur = pIterator->GetNext();
// }
//
class CParticleSimulateIterator
{
friend class CParticleMgr;
friend class CParticleEffectBinding;
public:
	CParticleSimulateIterator();
	
	// Iterate through the particles, simulate them, and remove them if necessary.
	Particle* GetFirst();
	Particle* GetNext();
	float GetTimeDelta() const;

	void RemoveParticle( Particle *pParticle );
	void RemoveAllParticles();

private:
	CParticleEffectBinding *m_pEffectBinding;
	CEffectMaterial *m_pMaterial;
	float m_flTimeDelta;

	bool m_bGotFirst;
	Particle *m_pNextParticle;
};

// -------------------------------------------------------------------------------------------------------- //
// CParticleRenderIterator inlines
// -------------------------------------------------------------------------------------------------------- //

CParticleRenderIterator::CParticleRenderIterator()
{
	m_pCur = nullptr;
	m_bGotFirst = false;
	m_flPrevZ = 0;
	m_nParticlesInCurrentBatch = 0;
	m_MinZ = 1e24;
	m_MaxZ = -1e24;
	m_nZCoords = 0;
}

const Particle* CParticleRenderIterator::GetFirst()
{
	contract_assert( !m_bGotFirst );
	m_bGotFirst = true;

	m_pCur = m_pMaterial->m_Particles.m_pNext;
	if ( m_pCur == &m_pMaterial->m_Particles )
		return nullptr;

	m_pParticleDraw->m_pSubTexture = m_pCur->m_pSubTexture;
	return m_pCur;
}

void CParticleRenderIterator::TestFlushBatch()
{
	++m_nParticlesInCurrentBatch;
	if( m_nParticlesInCurrentBatch >= NUM_PARTICLES_PER_BATCH )
	{
		m_pMeshBuilder->End( false, true );

		if(m_pMaterialSystem)
		{
			CMatRenderContextPtr pRenderContext( m_pMaterialSystem );
			m_pMesh = pRenderContext->GetDynamicMesh( true );
		}

		m_pMeshBuilder->Begin( m_pMesh, MATERIAL_QUADS, NUM_PARTICLES_PER_BATCH * 4 );

		m_nParticlesInCurrentBatch = 0;
	}
}

const Particle* CParticleRenderIterator::GetNext( float sortKey )
{
	contract_assert( m_bGotFirst );
	contract_assert( m_pCur );

	TestFlushBatch();

	Particle *pNext = m_pCur->m_pNext;

	// Update the incremental sort.
	if( m_bBucketSort )
	{
		m_MinZ = std::min( sortKey, m_MinZ );
		m_MaxZ = std::max( sortKey, m_MaxZ );
		
		m_zCoords[m_nZCoords] = sortKey;
		++m_nZCoords;
	}
	else
	{
		// Swap with the previous particle (incremental sort)?
		if( m_pCur != m_pMaterial->m_Particles.m_pNext && m_flPrevZ > sortKey )
		{
			SwapParticles( m_pCur->m_pPrev, m_pCur );
		}
		else
		{
			m_flPrevZ = sortKey;
		}
	}

	m_pCur = pNext;
	if ( m_pCur == &m_pMaterial->m_Particles )
		return nullptr;

	m_pParticleDraw->m_pSubTexture = m_pCur->m_pSubTexture;
	return m_pCur;
}

ParticleDraw* CParticleRenderIterator::GetParticleDraw() const
{
	return m_pParticleDraw;
}

// -------------------------------------------------------------------------------------------------------- //
// CParticleSimulateIterator inlines
// -------------------------------------------------------------------------------------------------------- //

CParticleSimulateIterator::CParticleSimulateIterator()
{
	m_pNextParticle = nullptr;
}

Particle* CParticleSimulateIterator::GetFirst()
{

	Particle *pRet = m_pMaterial->m_Particles.m_pNext;
	if ( pRet == &m_pMaterial->m_Particles )
		return nullptr;

	m_pNextParticle = pRet->m_pNext;
	return pRet;
}

Particle* CParticleSimulateIterator::GetNext()
{
	Particle *pRet = m_pNextParticle;

	if ( pRet == &m_pMaterial->m_Particles )
		return nullptr;
	
	m_pNextParticle = pRet->m_pNext;
	return pRet;
}

void CParticleSimulateIterator::RemoveParticle( Particle *pParticle )
{
	m_pEffectBinding->RemoveParticle( pParticle );
}

void CParticleSimulateIterator::RemoveAllParticles()
{
	Particle *pParticle = GetFirst();
	while ( pParticle )
	{
		RemoveParticle( pParticle );
		pParticle = GetNext();
	}
}

float CParticleSimulateIterator::GetTimeDelta() const
{
	return m_flTimeDelta;
}

class CNewParticleEffect;

using namespace ::ks::reflect;

// Purpose: Define inline functions for the CParticleProperty class. The
//			definitions	exist in this include file to avoid cluttering up
//			particle_property.h, but this file should not be included from any-
//			where else.

/// Set the parent of a given control point on a given effect to the index of another control point. This is
/// in fact entirely redundant given the function is actually on the CNewParticleEffect, but is included here
/// for uniformity of interface.
void CParticleProperty::SetControlPointParent( CNewParticleEffect *pEffect, int whichControlPoint, int parentIdx )
{
	pEffect->SetControlPointParent(whichControlPoint, parentIdx);
}

/// Given an index, return a pointer to the relevant particle effect structure. For convenience.
CNewParticleEffect *CParticleProperty::GetParticleEffectFromIdx( int idx )
{
	return m_ParticleEffects[idx].pParticleEffect.get();
}

class CParticleMgr;

// This defines things that allow particle effects to hook into the particle app.


// Command-line args can be passed in to set state or options in the effects.
class IPrototypeArgAccess
{
public:
	virtual const char*	FindArg(const char *pName, const char *pDefault=0)=0;
};

// You must implement this interface for the prototype app to be able to run your effect.
class IPrototypeAppEffect
{
public:
	virtual				~IPrototypeAppEffect()	{}
	
	// Start the effect. You can get command-line args with pArgs.
	virtual void		Start(CParticleMgr *pParticleMgr, IPrototypeArgAccess *pArgs)=0;
	
	// Return false if you don't allow properties to be edited in the prototype app.
	virtual bool		GetPropEditInfo(RecvTable **ppTable, void **ppObj) {return false;}
};

// Used internally.
typedef IPrototypeAppEffect* (*PrototypeEffectCreateFn)();
class PrototypeEffectLink
{
public:
							PrototypeEffectLink(PrototypeEffectCreateFn fn, const char *pName);								

	PrototypeEffectCreateFn	m_CreateFn;
	const char				*m_pEffectName;
	PrototypeEffectLink		*m_pNext;
};

class SimpleParticle;


// Use these to fire and forget particles.
// pParticle should be ON THE STACK - ie: don't allocate it from a CSimpleEmitter or from the particle manager.
// Just make one on the stack, fill in its parameters, and pass it in here.
void AddSimpleParticle( const SimpleParticle *pParticle, PMaterialHandle hMaterial, bool bInSkybox=false );
void AddEmberParticle( const SimpleParticle *pParticle, PMaterialHandle hMaterial, bool bInSkybox=false );
void AddFireSmokeParticle( const SimpleParticle *pParticle, PMaterialHandle hMaterial, bool bInSkybox=false );
void AddFireParticle( const SimpleParticle *pParticle, PMaterialHandle hMaterial, bool bInSkybox=false );

// Called by the renderer to draw all the particles.
void DrawParticleSingletons( bool bInSkybox );

class CParticleEffectBinding;
class CParticleRenderIterator;
class CParticleSimulateIterator;
class IParticleEffect;
struct Particle;

// ------------------------------------------------------------------------------------------------ //
// CParticleEffect is the base class that you can derive from to make a particle effect.
// These can be used two ways:
//
// 1. Allocate a CParticleEffect-based object using the class's static Create() function. This gives
//    you back a smart pointer that handles the reference counting for you.
//
// 2. Contain a CParticleEffect object in your class.
// ------------------------------------------------------------------------------------------------ //

class CParticleEffect : public IParticleEffect
{
public:
	DECLARE_CLASS_NOBASE( CParticleEffect );

	template < class > friend class ks::intrusive_ptr;

	// Call this before adding a bunch of particles to give it a rough estimate of where
	// your particles are for sorting amongst other translucent entities.
	void				SetSortOrigin( const Vector &vSortOrigin );

	PMaterialHandle		GetPMaterial(const char *name);
	
	Particle*			AddParticle( unsigned int particleSize, PMaterialHandle material, const Vector &origin );

	CParticleEffectBinding&	GetBinding()	{ return m_ParticleEffect; }

	const char *GetEffectName();

	void AddFlags( int iFlags ) { m_Flags |= iFlags; }
	void RemoveFlags( int iFlags ) { m_Flags &= ~iFlags; }

	void SetDontRemove( bool bSet );

// IParticleEffect overrides
public:

	virtual void				SetParticleCullRadius( float radius );
	virtual void				NotifyRemove( void );
	virtual const Vector &		GetSortOrigin();
	virtual void				NotifyDestroyParticle( Particle* pParticle );
	virtual void				Update( float flTimeDelta );

	// All Create() functions should call this so the effect deletes itself
	// when it is removed from the particle manager.
	void						SetDynamicallyAllocated( bool bDynamic=true );

	virtual bool				ShouldSimulate() const { return m_bSimulate; }
	virtual void				SetShouldSimulate( bool bSim ) { m_bSimulate = bSim; }

	int							AllocateToolParticleEffectId();
	int							GetToolParticleEffectId() const;
protected:
	explicit					CParticleEffect( const char *pDebugName );
	virtual						~CParticleEffect();

	// Returns nonzero if Release() has been called.
	int							IsReleased();
	
	enum
	{
		FLAG_ALLOCATED = (1<<1),	// Most of the CParticleEffects are dynamically allocated but
								// some are member variables of a class. If they're member variables.
		FLAG_DONT_REMOVE = (1<<2),
	};

	// Used to track down bugs.
	char const					*m_pDebugName;

	CParticleEffectBinding		m_ParticleEffect;
	Vector						m_vSortOrigin;
	
	int							m_Flags;		// Combination of CParticleEffect::FLAG_

	bool						m_bSimulate;
	int							m_nToolParticleEffectId;

private:
	// Update the reference count.
	void						AddRef();
	void						Release();
	
	int							m_RefCount;		// When this goes to zero and the effect has no more active
												// particles, (and it's dynamically allocated), it will delete itself.

	CParticleEffect( const CParticleEffect & ); // not defined, not accessible
};

//-----------------------------------------------------------------------------
// Particle flags
//-----------------------------------------------------------------------------
enum SimpleParticleFlag_t
{
	SIMPLE_PARTICLE_FLAG_WINDBLOWN = 0x1,
	SIMPLE_PARTICLE_FLAG_NO_VEL_DECAY = 0x2	// Used by the blood spray emitter. By default, it decays the
											// particle velocity.
};

class SimpleParticle : public Particle
{
public:
	SimpleParticle() : m_iFlags(0) {}

	// AddSimpleParticle automatically initializes these fields.
	Vector		m_vecVelocity;
	float		m_flRoll;
	float		m_flDieTime;	// How long it lives for.
	float		m_flLifetime;	// How long it has been alive for so far.
	unsigned char	m_uchColor[3];
	unsigned char	m_uchStartAlpha;
	unsigned char	m_uchEndAlpha;
	unsigned char	m_uchStartSize;
	unsigned char	m_uchEndSize;
	unsigned char 	m_iFlags;	// See SimpleParticleFlag_t above
	float		m_flRollDelta;
};

// CSimpleEmitter implements a common way to simulate and render particles.
//
// Effects can add particles to the particle manager and point at CSimpleEmitter
// for the effect so they don't have to implement the simulation code. It simulates
// velocity, and fades their alpha from invisible to solid and back to invisible over their lifetime.
//
// Particles you add using this effect must use the class CParticleSimple::Particle.
class CSimpleEmitter : public CParticleEffect
{
// IParticleEffect overrides.
public:

	DECLARE_CLASS( CSimpleEmitter, CParticleEffect );

	static ks::intrusive_ptr<CSimpleEmitter>	Create( const char *pDebugName );

	virtual void	SimulateParticles( CParticleSimulateIterator *pIterator );
	virtual void	RenderParticles( CParticleRenderIterator *pIterator );

	void			SetNearClip( float nearClipMin, float nearClipMax );

	void			SetDrawBeforeViewModel( bool state = true );

	SimpleParticle*	AddSimpleParticle( PMaterialHandle hMaterial, const Vector &vOrigin, float flDieTime=3, unsigned char uchSize=10 );
	
	void			SetShouldDrawForSplitScreenUser( int nSlot );

// Overridables for variants like CEmberEffect.
protected:
	explicit		CSimpleEmitter( const char *pDebugName = nullptr );
	virtual			~CSimpleEmitter();

	virtual	float	UpdateAlpha( const SimpleParticle *pParticle );
	virtual float	UpdateScale( const SimpleParticle *pParticle );
	virtual	float	UpdateRoll( SimpleParticle *pParticle, float timeDelta );
	virtual	void	UpdateVelocity( SimpleParticle *pParticle, float timeDelta );
	virtual Vector	UpdateColor( const SimpleParticle *pParticle );

	float			m_flNearClipMin;
	float			m_flNearClipMax;

	int				m_nSplitScreenPlayerSlot;

private:
	CSimpleEmitter( const CSimpleEmitter & ); // not defined, not accessible
};

//==================================================
// EmberEffect
//==================================================

class CEmberEffect : public CSimpleEmitter
{
public:
	explicit						CEmberEffect( const char *pDebugName );
	static ks::intrusive_ptr<CEmberEffect>	Create( const char *pDebugName );

	virtual void UpdateVelocity( SimpleParticle *pParticle, float timeDelta );
	virtual Vector UpdateColor( const SimpleParticle *pParticle );

private:
	CEmberEffect( const CEmberEffect & ); // not defined, not accessible
};

//==================================================
// FireSmokeEffect
//==================================================

class CFireSmokeEffect : public CSimpleEmitter
{
public:
	explicit							CFireSmokeEffect( const char *pDebugName );
	static ks::intrusive_ptr<CFireSmokeEffect>	Create( const char *pDebugName );

	virtual void UpdateVelocity( SimpleParticle *pParticle, float timeDelta );
	virtual float UpdateAlpha( const SimpleParticle *pParticle );

protected:
	VPlane	m_planeClip;

private:
	CFireSmokeEffect( const CFireSmokeEffect & ); // not defined, not accessible
};

//==================================================
// CFireParticle
//==================================================

class CFireParticle : public CSimpleEmitter
{
public:
	explicit						CFireParticle( const char *pDebugName );
	static ks::intrusive_ptr<CFireParticle>	Create( const char *pDebugName );
	
	virtual Vector UpdateColor( const SimpleParticle *pParticle );

private:
	CFireParticle( const CFireParticle & ); // not defined, not accessible
};

class CParticleRenderIterator;
class CParticleSimulateIterator;


inline constexpr auto FLE_VIEWMODEL = 0x00000001;

class CLocalSpaceEmitter : public CSimpleEmitter
{
public:

	DECLARE_CLASS( CLocalSpaceEmitter, CParticleEffect );
	
	static ks::intrusive_ptr<CLocalSpaceEmitter> Create( const char *pDebugName, ClientEntityHandle_t hEntity, int nAttachment, int flags = 0 );

	virtual void RenderParticles( CParticleRenderIterator *pIterator );
	virtual void SimulateParticles( CParticleSimulateIterator *pIterator );

	virtual void SetupTransformMatrix( void );
	virtual void Update( float flTimeDelta );

	const matrix3x4_t& GetTransformMatrix() const;	

protected:

	explicit CLocalSpaceEmitter( const char *pDebugName );

	ClientEntityHandle_t m_hEntity;
	int	m_nAttachment;
	int m_fFlags;

private:

	CLocalSpaceEmitter( const CLocalSpaceEmitter & ); // not defined, not accessible

	// This is stored in the ParticleEffectBinding now.
	//matrix3x4_t	m_matTransform;

	//FIXME: Bones here as well...
};
