// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation
// Copyright (c) 1996-2008 Valve Corporation

module;
#include <time.h>
#include "edict.h"
#include "usermessages_schema.h"
#include "user_message_register.h"
#include "eiface.h"
#include "igameresources.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/materialsystem_config.h"
#include "utils/common/filesystem_tools.h"
#include "filesystem.h"
#include "igameevents.h"
#include "matchmaking/cstrike15/imatchext_cstrike15.h"
#include "matchmaking/imatchframework.h"
#include "matchmaking/mm_helpers.h"
#include "tier2/resourceprecacher.h"
#include "cdll_client_int.h"
#include "client_class.h"
#include "const.h"
#include "predictable_entity.h"
#include "reflect_recvtable.h"
#include "networkstringtabledefs.h"
#include "string_t.h"
#include "gametrace.h"
module ks.client;

import std;
import ks.math.mathlib;
import ks.platform;
import ks.simd;
import :particlemgr;
import :util;
import :achievements_and_stats_interface;
import :baseanimatedtextureproxy;
import :baseentity;
import :sharedvar;
import ks.core.basetypes;
import ks.core.cmdline;
import ks.core.cpu;
import ks.core.dbg;
import ks.core.perfstats;
import ks.core.platform;
import ks.core.types;
import ks.tier1.checksum_crc;
import ks.tier1.convar;
import ks.tier1.cvar;
import ks.tier1.fmtstr;
import ks.tier1.icvar;
import ks.tier1.interfaces;
import ks.tier1.keyvalues;
import ks.tier1.reflect_wirevar;
import ks.tier1.strtools;
import ks.tier1.text_writer;
import ks.tier1.timeutils;
import ks.tier1.uniqueid;
import :props;
import :iinput;
import :precache;
import ks.game.gamemovement;
import ks.core.color;
import ks.core.threadtools;
import ks.core.vprof;
import ks.tier1.iconvar;
import ks.tier1.createinterface;
import ks.tier1.registry;
import ks.tier1.vector_ops;
import :cstrike15.cs_player;
import ks.game.shareddefs;
import :cstrike15.cs_shareddefs;
import :noopanimcontroller;
import ks.math.compressed_vector;
import ks.math.vector;
import ks.game.mapentities_shared;
import ks.math.polyhedron;
import ks.tier1.map_ops;
import :gamevars_shared;

CGameTimescale g_GameTimescale;
CGameTimescale* GameTimescale() { return &g_GameTimescale; }

CGameTimescale::CGameTimescale( void ) : CAutoGameSystemPerFrame( "CGameTimescale" )
{
	m_flStartBlendRealtime = 0.0f;
}

CGameTimescale::~CGameTimescale()
{
}

bool CGameTimescale::Init()
{
	ResetTimescale();

	return true;
}

void CGameTimescale::Shutdown()
{
	ResetTimescale();
}

	void CGameTimescale::Update( float frametime )
	{
		UpdateTimescale();
	}

void CGameTimescale::LevelInitPostEntity()
{
	ResetTimescale();
}

void CGameTimescale::LevelShutdownPostEntity()
{
	ResetTimescale();
}

void CGameTimescale::SetCurrentTimescale( float flTimescale )
{
	if ( m_flCurrentTimescale == flTimescale && m_flCurrentTimescale == engine->GetTimescale() )
		return;

	// No ramp in/out, just set it!
	m_flDesiredTimescale = flTimescale;
	m_flCurrentTimescale = m_flDesiredTimescale;

	m_flDurationRealTimeSeconds = 0.0f;
	m_nInterpolatorType = INTERPOLATOR_LINEAR;

	m_flStartBlendTime = 0.0f;
	m_flStartBlendRealtime = 0.0f;

}

void CGameTimescale::SetDesiredTimescaleAtTime( float flDesiredTimescale, float flDurationRealTimeSeconds /*= 0.0f*/, Interpolators_e nInterpolatorType /*= INTERPOLATOR_LINEAR*/, float flStartBlendTime /*= 0.0f*/ )
{
	SetDesiredTimescale( flDesiredTimescale, flDurationRealTimeSeconds, nInterpolatorType, std::max( 0.0f, flStartBlendTime - gpGlobals->curtime ) );
}

void CGameTimescale::SetDesiredTimescale( float flDesiredTimescale, float flDurationRealTimeSeconds /*= 0.0f*/, Interpolators_e nInterpolatorType /*= INTERPOLATOR_LINEAR*/, float flDelayRealtime /*= 0.0f*/ )
{
	if ( m_flDesiredTimescale == flDesiredTimescale )
		return;

	m_flDesiredTimescale = flDesiredTimescale;
	m_flDurationRealTimeSeconds = flDurationRealTimeSeconds;
	m_nInterpolatorType = nInterpolatorType;

	m_flStartBlendTime = gpGlobals->curtime + flDelayRealtime * m_flCurrentTimescale;

	if ( gpGlobals->curtime >= m_flStartBlendTime )
	{
		m_flStartBlendRealtime = gpGlobals->realtime;
	}
	else
	{
		m_flStartBlendRealtime = 0.0f;
	}

	m_flStartTimescale = m_flCurrentTimescale;

}

void CGameTimescale::UpdateTimescale( void )
{
	if ( engine->IsPaused() )
		return;

	if ( m_flCurrentTimescale != m_flDesiredTimescale )
	{
		if ( gpGlobals->curtime >= m_flStartBlendTime )
		{
			if ( m_flStartBlendRealtime == 0.0f )
			{
				m_flStartBlendRealtime = gpGlobals->realtime;
			}

			float flInterp = 1.0f;

			if ( m_flDurationRealTimeSeconds > 0.0f )
			{
				flInterp = std::min( 1.0f, ( gpGlobals->realtime - m_flStartBlendRealtime ) / m_flDurationRealTimeSeconds );
			}

			switch ( m_nInterpolatorType )
			{
			case INTERPOLATOR_ACCEL:
				flInterp = Bias( flInterp, 0.25f );
				break;

			case INTERPOLATOR_DEACCEL:
				flInterp = Bias( flInterp, 0.75f );
				break;

			case INTERPOLATOR_EASE_IN_OUT:
				flInterp = Gain( flInterp, 0.75f );
				break;
			}

			m_flCurrentTimescale = m_flStartTimescale * ( 1.0f - flInterp ) + m_flDesiredTimescale * flInterp;
		}
	}

	if ( m_flCurrentTimescale != engine->GetTimescale() )
	{
		engine->SetTimescale( m_flCurrentTimescale );
	}
}

void CGameTimescale::ResetTimescale( void )
{
	m_flDesiredTimescale = 1.0f;
	m_flCurrentTimescale = 1.0f;

	m_flDurationRealTimeSeconds = 0.0f;
	m_nInterpolatorType = INTERPOLATOR_LINEAR;

	m_flStartBlendTime = 0.0f;
	m_flStartBlendRealtime = 0.0f;

	engine->SetTimescale( 1.0f );
}

bool __MsgFunc_CurrentTimescale( const ks::net::CCSUsrMsg_CurrentTimescale &msg )
{
	GameTimescale()->SetCurrentTimescale( msg.cur_timescale );

	return true;
}
USER_MESSAGE_REGISTER( CurrentTimescale );

bool __MsgFunc_DesiredTimescale( const ks::net::CCSUsrMsg_DesiredTimescale &msg )
{
	float flDesiredTimescale = msg.desired_timescale;
	float flDurationRealTimeSeconds = msg.duration_realtime_sec;
	CGameTimescale::Interpolators_e nInterpolatorType = static_cast< CGameTimescale::Interpolators_e >( msg.interpolator_type.get() );
	float flStartBlendTime = msg.start_blend_time;

	GameTimescale()->SetDesiredTimescaleAtTime( flDesiredTimescale, flDurationRealTimeSeconds, nInterpolatorType, flStartBlendTime );

	return true;
}
USER_MESSAGE_REGISTER( DesiredTimescale );

class CUserCmd;
class CBasePlayer;

using namespace ::ks::reflect;

#ifndef DEDICATED
#endif

extern const ConVar *sv_cheats;

inline constexpr const char *GAMESTATS_LOG_FILE = "gamestats.log";
inline constexpr const char *GAMESTATS_PATHID = "MOD";

/*
static constexpr auto ONE_DAY_IN_SECONDS = 86400;

// Lower threshold in debug for testing...
#if defined( _DEBUG )
#define WALKED_AWAY_FROM_KEYBOARD_SECONDS 15.0f   // 15 seconds of movement == might be paused
#else
#define WALKED_AWAY_FROM_KEYBOARD_SECONDS 300.0f   // 5 minutes of no movement == might be paused
#endif
*/

extern IUploadGameStats *gamestatsuploader;

static char s_szPseudoUniqueID[20] = "";

static inline char const *SafeString( char const *pStr )
{
	return ( pStr ) ? pStr : "?";
}

static CBaseGameStats s_GameStats_Singleton;
CBaseGameStats *gamestats = &s_GameStats_Singleton; //start out pointing at the basic version which does nothing by default
extern ConVar skill;
void OverWriteCharsWeHate( char *pStr );

bool StatsTrackingIsFullyEnabled( void );

class CGamestatsData
{
public:
	CGamestatsData()
	{
		m_pKVData = nullptr;
		m_bHaveData = false;
		AllocData();
	}

	~CGamestatsData()
	{
		FreeData();
	}

	void AllocData()
	{
		FreeData();
		char buffer[ 128 ];
		V_snprintf( buffer, sizeof( buffer ), "gamestats_" );
		UTIL_GetModDir( buffer + 10, sizeof( buffer ) - 10 );
		m_pKVData = new KeyValues( buffer );
	}
	void FreeData()
	{
		if ( m_pKVData != nullptr )
		{
			m_pKVData->deleteThis();
			m_pKVData = nullptr;
		}
	}

	KeyValues *m_pKVData;
	bool m_bHaveData;
};

//used to drive most of the game stat event handlers as well as track basic stats under the hood of CBaseGameStats
class CBaseGameStats_Driver : public CAutoGameSystemPerFrame
{
public:
	CBaseGameStats_Driver( void );
	
	typedef CAutoGameSystemPerFrame BaseClass;

	// IGameSystem overloads
	virtual bool Init();
	virtual void Shutdown();

	// Level init, shutdown
	virtual void LevelInitPreEntity();
	virtual void LevelShutdownPreEntity();
	// Called during game save
	virtual void OnSave();
	// Called during game restore, after the local player has connected and entities have been fully restored
	virtual void OnRestore();

	virtual void FrameUpdatePostEntityThink();

	void PossibleMapChange( void );

	void CollectData( StatSendType_t sendType );
	void SendData();
	void ResetData();
	bool AddBaseDataForSend( KeyValues *pKV, StatSendType_t sendType );

	StatsBufferRecord_t m_StatsBuffer[STATS_WINDOW_SIZE];
	bool m_bBufferFull;
	int m_nWriteIndex;
	double m_flLastRealTime;
	double m_flLastSampleTime;
	float m_flTotalTimeInLevels;
	int m_iNumLevels;
	bool m_bDidVoiceChat;	// Did the player use voice chat at ALL this map?

	// Track how often the CPU is reported as running at full speed (above kCPUMonitoringWarning1),
	// medium speed (above kCPUMonitoringWarning2) or low speed (other).
	int m_nCPUUnthrottledCount;
	int m_nCPUThrottle1Count;
	int m_nCPUThrottle2Count;
	// Track when we last received data.
	double m_lastCPUFrequencyTimestamp;

	// Generate a buffer that will keep trailing observations prior to adding them to the main m_StatsBuffer array
	StatsBufferRecord_t m_TrailingStatsBuffer[STATS_TRAILING_WINDOW_SIZE];
	int m_nTrailingWriteIndex;
	int m_nTrailingSamplesWritten;	
	
	template<class T> T AverageStat( T StatsBufferRecord_t::*field ) const
	{
		T sum = 0;
		int iMax = m_bBufferFull ? std::size( m_StatsBuffer ) : m_nWriteIndex;
		for( int i = 0; i < iMax; i++ )
			sum += m_StatsBuffer[i].*field;
		return sum / iMax;
	}

	template<class T> T MaxStat( T StatsBufferRecord_t::*field ) const
	{
		T maxsofar = -16000000;
		int iMax = m_bBufferFull ? std::size( m_StatsBuffer ) : m_nWriteIndex;
		for( int i = 0; i < iMax; i++ )
			maxsofar = std::max( maxsofar, m_StatsBuffer[i].*field );
		return maxsofar;
	}

	template<class T> T MinStat( T StatsBufferRecord_t::*field ) const
	{
		T minsofar = 16000000;
		int iMax = m_bBufferFull ? std::size( m_StatsBuffer ) : m_nWriteIndex;
		for( int i = 0; i < iMax; i++ )
			minsofar = std::min( minsofar, m_StatsBuffer[i].*field );
		return minsofar;
	}

	template<class T> T StdDevStat( T StatsBufferRecord_t::*field, T average ) const
	{
		T flDeviationsquared = 0;
		int iMax = m_bBufferFull ? std::size( m_StatsBuffer ) : m_nWriteIndex;

		// Compute std deviation
		for (int i = 0; i < iMax; i++)
		{
			T val = m_StatsBuffer[i].*field - average;
			flDeviationsquared += (val * val);
		}

		return ( ( iMax > 1 ) ? std::sqrt( (flDeviationsquared / T(iMax - 1) ) ) : 0 );
	}

	void SortFramerate()
	{
		int iMax = ( ( m_bBufferFull ) ? std::size( m_StatsBuffer ) : m_nWriteIndex ) - 1;

		// Sort members of m_StatsBuffer
		std::sort(&m_StatsBuffer[0],&m_StatsBuffer[iMax]);
	}

	float PercentileFramerate( int nPercent )  const
	{
	    int iMax = ( ( m_bBufferFull ) ? std::size( m_StatsBuffer ) : m_nWriteIndex ) - 1;

		// Return the first or last member if nPercent is out of bounds
		if ( nPercent >= 100 )
			return m_StatsBuffer[iMax].m_flFrameRate;
		if ( nPercent <= 0 )
			return m_StatsBuffer[0].m_flFrameRate;

		// Otherwise determine the desired index and return its field.	
		// NOTE: nPercent = 50 is close to (but is NEVER) the median - special code would be needed to handle odd-item arrays, or averaging two elements for even-item arrays.
		int nIdx = (iMax * nPercent) / 100;
		return m_StatsBuffer[nIdx].m_flFrameRate;
	}

	inline void AdvanceIndex( void )
	{
		m_nWriteIndex++;
		if ( m_nWriteIndex == STATS_WINDOW_SIZE )
		{
			m_nWriteIndex = 0;
			m_bBufferFull = true;
		}
	}

	void UpdatePerfStats( void )
	{
		double flCurTime = Plat_FloatTime();

		if ( ( m_flLastRealTime > 0.0f ) &&
				( flCurTime > m_flLastRealTime ) )
		{				
			float flFps = 1.0 / ( flCurTime - m_flLastRealTime );
			float flCpuWait = g_PerfStats.m_Slots[ PERF_STATS_SLOT_END_FRAME ].m_PrevFrameTime.GetMillisecondsF();
			if ( g_pGameRules ) // are we in the game? if not, it's not worth collecting histogram info
			{
				uint8 nBin = m_FpsHistory.Update( flFps, flCpuWait );
				m_FpsHistogram.Update( nBin );
				for ( int nEvent = 0; nEvent < PERF_STATS_EVENT_COUNT; ++nEvent )
				{
					m_FpsHistogramGame[ nEvent ].Update( nBin );
				}
			}

			m_TrailingStatsBuffer[m_nTrailingWriteIndex].m_flFrameRate = flFps;

			m_nTrailingWriteIndex++;
			if ( m_nTrailingWriteIndex == STATS_TRAILING_WINDOW_SIZE )
				m_nTrailingWriteIndex = 0;
				
			if ( m_nTrailingSamplesWritten < STATS_TRAILING_WINDOW_SIZE )
				m_nTrailingSamplesWritten++;
		}

		if ( ( m_flLastSampleTime >= 0.0f ) && 
			( flCurTime - m_flLastSampleTime >= STATS_RECORD_INTERVAL ) )
		{
			if ( ( m_flLastRealTime > 0 ) && ( flCurTime > m_flLastRealTime ) )
			{
			
				StatsBufferRecord_t &stat = m_StatsBuffer[m_nWriteIndex];

				float flSumFrameRate = 0;
				for ( int i = 0; i < m_nTrailingSamplesWritten; i++ )
				{
					flSumFrameRate += m_TrailingStatsBuffer[i].m_flFrameRate;
				}
				float flFrameRate = flSumFrameRate / m_nTrailingSamplesWritten; 
				stat.m_flFrameRate = flFrameRate;

				m_nTrailingSamplesWritten = 0;
				m_nTrailingWriteIndex = 0;

				// The stat system isn't designed to handle split screen players. Until it get's 
				// redesigned, let's take the first player's split screen ping, since all other stats
				// will be based on the first player
				IGameResources *gr = GameResources();
				int ping = 0;
				CBasePlayer *pPlayer =	CBasePlayer::GetLocalPlayer( FirstValidSplitScreenSlot() );
				if ( pPlayer && gr )
				{
					ping = gr->GetPing( pPlayer->entindex() );
				}
				stat.m_flServerPing = ping;

				stat.m_flMainThreadTimeMS		= g_PerfStats.m_Slots[PERF_STATS_SLOT_MAINTHREAD].m_PrevFrameTime.GetMillisecondsF();
				stat.m_flMainThreadWaitTimeMS	= g_PerfStats.m_Slots[PERF_STATS_SLOT_END_FRAME].m_PrevFrameTime.GetMillisecondsF();
				stat.m_flRenderThreadTimeMS		= g_PerfStats.m_Slots[PERF_STATS_SLOT_RENDERTHREAD].m_PrevFrameTime.GetMillisecondsF();
				stat.m_flRenderThreadWaitTimeMS	= g_PerfStats.m_Slots[PERF_STATS_SLOT_FORCE_HARDWARE_SYNC].m_PrevFrameTime.GetMillisecondsF();

				AdvanceIndex();
				m_flLastSampleTime = flCurTime;
			}
		}
		if ( m_flLastSampleTime == -1 )
		{
			m_flLastSampleTime = flCurTime;
		}
		m_flLastRealTime = flCurTime;

		if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
		{
			m_bDidVoiceChat |= GetClientVoiceMgr()->IsLocalPlayerSpeaking( FirstValidSplitScreenSlot() );
		}

		const CPUFrequencyResults results = GetCPUFrequencyResults( true );
		if ( results.m_timeStamp > m_lastCPUFrequencyTimestamp )
		{
			m_lastCPUFrequencyTimestamp = results.m_timeStamp;
			// Increment the appropriate counter for how much the CPU is throttled (if at all)
			if ( results.m_percentage > kCPUMonitoringWarning1 )
				++m_nCPUUnthrottledCount;
			else if ( results.m_percentage > kCPUMonitoringWarning2 )
				++m_nCPUThrottle1Count;
			else
				++m_nCPUThrottle2Count;
		}
	}

	void FirePerfStatsEvent( PerfStatsEventEnum_t nEvent, int nLookAhead = CFpsHistory::HISTORY_BUF_SIZE, int nLookBack = CFpsHistory::HISTORY_BUF_SIZE )
	{
		m_FpsHistogramGame[ nEvent ].Fire( nLookAhead, nLookBack, m_FpsHistory );
	}

	std::string		m_PrevMapName; //used to track "OnMapChange" events
	int				m_iLoadedVersion;
	char			m_szLoadedUserID[ 17 ];	// GUID

	bool			m_bEnabled; //false if incapable of uploading or the user doesn't want to enable stat tracking
	bool			m_bShuttingDown;
	bool			m_bInLevel;
	bool			m_bFirstLevel;
	std::time_t			m_tLastUpload;

	float			m_flLevelStartTime;

	bool			m_bStationary;
	float			m_flLastMovementTime;
	CUserCmd		m_LastUserCmd;
	bool			m_bGamePaused;
	float			m_flPauseStartTime;

	CGamestatsData	*m_pGamestatsData;

	CFpsHistory m_FpsHistory;
	CFpsHistogram m_FpsHistogram;
	CFpsSelectiveHistogram m_FpsHistogramGame[ PERF_STATS_EVENT_COUNT ];
};
static CBaseGameStats_Driver CBGSDriver;

void UpdatePerfStats( void )
{
	CBGSDriver.UpdatePerfStats();
}

void FirePerfStatsEvent( PerfStatsEventEnum_t nEvent, int nLookAhead, int nLookBack )
{
	CBGSDriver.FirePerfStatsEvent( nEvent, nLookAhead, nLookBack );
}

uint8 CFpsHistory::Update( float flFps, float flCpuWait )
{
	uint8 nBin = 0;
	if ( flFps >= 45 )
	{
		if ( flFps >= 90 )
		{
			if ( flFps >= 120 )
			{
				nBin = FPS_120;
			}
			else
			{
				nBin = FPS_90;
			}
		}
		else
		{
			if ( flFps >= 60 )
			{
				nBin = FPS_60;
			}
			else
			{
				nBin = FPS_45;
			}
		}
	}
	else
	{
		if ( flFps >= 20 )
		{
			if ( flFps >= 30 )
			{
				nBin = FPS_30;
			}
			else
			{
				nBin = FPS_20;
			}
		}
		else
		{
			if ( flFps >= 10 )
			{
				nBin = FPS_10;
			}
			else
			{
				nBin = FPS_LOW;
			}
		}
	}

	if ( flCpuWait > 0.5f )
	{
		nBin |= 0x80;
	}

	return nBin;
}

int64 CFpsHistogram::Encode()const
{
	uint nMaxBin = 0;
	for ( int i = 0; i < FPS_BIN_COUNT; ++i )
	{
		nMaxBin = Max( nMaxBin, m_nFps[ i ] );
	}

	if ( nMaxBin == 0 )
		return 0;

	int64 nEncode = 0;
	for ( int i = 0; i < FPS_BIN_COUNT / 2; ++i )
	{
		nEncode <<= 4;
		if ( uint nDenominator = m_nFps[ i * 2 ] + m_nFps[ i * 2 + 1 ] )
		{
			uint nNormalized = ( 15ll * ( m_nCpuWaits[ i * 2 ] + m_nCpuWaits[ i * 2 + 1 ] ) + nDenominator / 2 ) / nDenominator;
			nEncode |= nNormalized;
		}
	}
	for ( int i = 0; i < FPS_BIN_COUNT; ++i )
	{
		nEncode <<= 6;
		uint nNormalized = ( ( 0x3Fll * m_nFps[ i ] + nMaxBin / 2 ) / nMaxBin ); // round to nearest
		nEncode |= nNormalized;
	}
	return nEncode;
}

CBaseGameStats_Driver::CBaseGameStats_Driver( void ) :
	BaseClass( "CGameStats" ),
	m_iLoadedVersion( -1 ),
	m_bEnabled( false ),
	m_bShuttingDown( false ),
	m_bInLevel( false ),
	m_bFirstLevel( true ),
	m_flLevelStartTime( 0.0f ),
	m_bStationary( false ),
	m_flLastMovementTime( 0.0f ),
	m_bGamePaused( false ),
	m_pGamestatsData( nullptr ),
	m_bBufferFull( false ),
	m_nWriteIndex( 0 ),
	m_flLastRealTime( -1 ),
	m_flLastSampleTime( -1 ),
	m_flTotalTimeInLevels( 0 ),
	m_iNumLevels( 0 ),
	m_bDidVoiceChat( false ),
	m_nTrailingWriteIndex( 0 ),
	m_nTrailingSamplesWritten( 0 )
{
	m_szLoadedUserID[0] = 0;
	m_tLastUpload = 0;
	m_LastUserCmd.Reset();
}

static FileHandle_t g_LogFileHandle = FILESYSTEM_INVALID_HANDLE;

CBaseGameStats::CBaseGameStats() :
	m_bLogging( false ),
	m_bLoggingToFile( false )
{
}

bool CBaseGameStats::StatTrackingAllowed( void )
{
	return CBGSDriver.m_bEnabled;
}

// Don't care about vcr hooks here...
#undef localtime
#undef asctime

void CBaseGameStats::StatsLog( char const *fmt, ... )
{
	if ( !m_bLogging && !m_bLoggingToFile )
		return;

	char buf[ 2048 ];
	std::va_list argptr;
	__builtin_va_start( argptr, fmt );
	V_vsnprintf( buf, sizeof( buf ), fmt, argptr );
	__builtin_va_end( argptr );

	// Prepend timestamp and spew it

	// Prepend the time.
	std::time_t aclock;
	std::time( &aclock );
	struct tm *newtime = std::localtime( &aclock );

	char timeString[ 128 ];
	V_strncpy( timeString, std::asctime( newtime ), sizeof( timeString ) );
	// Get rid of the \n.
	char *pEnd = std::strstr( timeString, "\n" );
	if ( pEnd )
	{
		*pEnd = 0;
	}

	if ( m_bLogging )
	{
		DevMsg( "[GS %s - %7.2f] %s", timeString, gpGlobals->realtime, buf );
	}

	if ( m_bLoggingToFile )
	{
		if ( FILESYSTEM_INVALID_HANDLE == g_LogFileHandle )
		{
			g_LogFileHandle = filesystem->Open( GAMESTATS_LOG_FILE, "a", GAMESTATS_PATHID );
		}

		if ( FILESYSTEM_INVALID_HANDLE != g_LogFileHandle )
		{
			filesystem->FPrintf( g_LogFileHandle, "[GS %s - %7.2f] %s", timeString, gpGlobals->realtime, buf );
			filesystem->Flush( g_LogFileHandle );
		}
	}
}

static char s_szSaveFileName[256] = "";
static char s_szStatUploadRegistryKeyName[256] = "";

const char *CBaseGameStats::GetStatSaveFileName( void )
{
	contract_assert( ( s_szSaveFileName[0] != '\0' ) && "Don't know what file to save stats to." );
	return s_szSaveFileName;
}

const char *CBaseGameStats::GetStatUploadRegistryKeyName( void )
{
	contract_assert( ( s_szStatUploadRegistryKeyName[0] != '\0' ) && "Don't know the registry key to use to mark stats uploads." );
	return s_szStatUploadRegistryKeyName;
}

const char *CBaseGameStats::GetUserPseudoUniqueID( void )
{
	contract_assert( ( s_szPseudoUniqueID[0] != '\0' ) && "Don't have a pseudo unique ID." );
	return s_szPseudoUniqueID;
}

void CBaseGameStats::Event_Init( void )
{
}

void CBaseGameStats::Event_Shutdown( void )
{
}

void CBaseGameStats::Event_MapChange( const char *szOldMapName, const char *szNewMapName )
{
	StatsLog( "CBaseGameStats::Event_MapChange to [%s]\n", szNewMapName );
}

void CBaseGameStats::Event_LevelInit( void )
{
}

void CBaseGameStats::Event_LevelShutdown( float flElapsed )
{
}

void CBaseGameStats::Event_SaveGame( void )
{
	StatsLog( "CBaseGameStats::Event_SaveGame [%s]\n", CBGSDriver.m_PrevMapName.c_str() );
}

void CBaseGameStats::Event_LoadGame( void )
{
}

bool CBaseGameStats_Driver::Init()
{
	const char *pGameDir = CommandLine()->ParmValue( "-game", "hl2" );

	//standardizing is a good thing
	char szLoweredGameDir[256];
	V_strncpy( szLoweredGameDir, pGameDir, sizeof( szLoweredGameDir ) );
	V_strlower( szLoweredGameDir );

	gamestats = gamestats->OnInit( gamestats, szLoweredGameDir );

	//determine constant strings needed for saving and uploading
	V_strncpy( s_szSaveFileName, szLoweredGameDir, sizeof( s_szSaveFileName ) );
	V_strncat( s_szSaveFileName, "_gamestats.dat", sizeof( s_szSaveFileName ) );

	V_strncpy( s_szStatUploadRegistryKeyName, "GameStatsUpload_", sizeof( s_szStatUploadRegistryKeyName ) );
	V_strncat( s_szStatUploadRegistryKeyName, szLoweredGameDir, sizeof( s_szStatUploadRegistryKeyName ) );

	gamestats->m_bLoggingToFile = CommandLine()->FindParm( "-gamestatsloggingtofile" ) ? true : false;
	gamestats->m_bLogging = CommandLine()->FindParm( "-gamestatslogging" ) ? true : false;

	if ( gamestatsuploader )
	{
		m_bEnabled = gamestatsuploader->IsGameStatsLoggingEnabled();
		if ( m_bEnabled )
		{
			gamestatsuploader->GetPseudoUniqueId( s_szPseudoUniqueID, sizeof( s_szPseudoUniqueID ) );
		}
	}

	ResetData();

	if ( s_szPseudoUniqueID[ 0 ] != 0 )
	{			
		gamestats->Event_Init();
	}
	else
	{
		m_bEnabled = false; //unable to generate a pseudo-unique ID, disable tracking
	}

	return true;
}

void CBaseGameStats_Driver::Shutdown()
{
	m_bShuttingDown = true;

	gamestats->Event_Shutdown();

	if ( gamestats->UseOldFormat() )
	{
	}
	else
	{
		// code path for new format game stats
		if ( gamestats->ShouldSendDataOnAppShutdown() )
		{
			CollectData( STATSEND_APPSHUTDOWN );
			SendData();
		}
	}
	if ( FILESYSTEM_INVALID_HANDLE != g_LogFileHandle )
	{
		filesystem->Close( g_LogFileHandle );
		g_LogFileHandle = FILESYSTEM_INVALID_HANDLE;
	}

	if ( m_pGamestatsData != nullptr )
	{
		engine->SetGamestatsData( nullptr );
		delete m_pGamestatsData;
		m_pGamestatsData = nullptr;
	}
}

void CBaseGameStats_Driver::PossibleMapChange( void )
{
}

void CBaseGameStats_Driver::LevelInitPreEntity()
{
	m_bInLevel = true;
	m_bFirstLevel = false;

	if ( V_stricmp( s_szPseudoUniqueID, "unknown" ) == 0 )
	{
		// "unknown" means this is a dedicated server and we weren't able to generate a unique ID (e.g. Linux server).
		// Change the unique ID to be a hash of IP & port.  We couldn't do this earlier because IP is not known until level
		// init time.
		ConVar *hostip = cvar->FindVar( "hostip" );
		ConVar *hostport = cvar->FindVar( "hostport" );
		if ( hostip && hostport )
		{
			int crcInput[2];
			crcInput[0] = hostip->GetInt();
			crcInput[1] = hostport->GetInt();
			if ( crcInput[0] && crcInput[1] )
			{
				CRC32_t crc = CRC32_ProcessSingleBuffer( crcInput, sizeof( crcInput ) );
				V_snprintf( s_szPseudoUniqueID, std::size( s_szPseudoUniqueID ), "H:%x", ( int ) crc );
			}
		}
	}

	PossibleMapChange();

	m_flPauseStartTime = 0.0f;
	m_flLevelStartTime = gpGlobals->realtime;

	gamestats->Event_LevelInit();

}

void CBaseGameStats_Driver::LevelShutdownPreEntity()
{
	float flElapsed = gpGlobals->realtime - m_flLevelStartTime;

	if ( flElapsed < 0.0f )
	{
		contract_assert( 0 );
		Warning( "EVENT_LEVELSHUTDOWN:  with negative elapsed time (rt %f starttime %f)\n", gpGlobals->realtime, m_flLevelStartTime );
		flElapsed = 0.0f;
	}

	//Assert( m_bInLevel ); //so, apparently shutdowns can happen before inits

	if ( m_bInLevel )
	{
		m_flTotalTimeInLevels += flElapsed;
		m_iNumLevels ++;

		gamestats->Event_LevelShutdown( flElapsed );

		if ( gamestats->UseOldFormat() )
		{
		}
		else
		{
			// code path for new format game stats
			CollectData( STATSEND_LEVELSHUTDOWN );
			if ( gamestats->ShouldSendDataOnLevelShutdown() )
			{
				SendData();
			}	
		}
		m_bInLevel = false;	
	}
}

void CBaseGameStats_Driver::OnSave()
{
	gamestats->Event_SaveGame();
}

void CBaseGameStats_Driver::CollectData( StatSendType_t sendType )
{

	// The base stat system holds memory until "APPSHUTDOWN" which on the Xbox isn't good.
	// When on the Xbox, neither server nor clients collect stats, but the base system still does.

	CGamestatsData *pGamestatsData = nullptr;
	pGamestatsData = m_pGamestatsData;
	contract_assert( pGamestatsData || //valid stats
			(!gpGlobals->IsClient() && (sendType == STATSEND_APPSHUTDOWN) && gamestats->ShouldSendDataOnLevelShutdown()) ); //or already sent off stats during level shutdown (now in app shutdown)
    
    // If a server is told to send stats on STATSEND_APPSHUTDOWN, we need to check to make sure we still have a valid object.
	if ( !pGamestatsData )
		return;

	KeyValues *pKV = pGamestatsData->m_pKVData;

	int iAppID = engine->GetAppID();
	pKV->SetInt( "appid", iAppID );

	switch ( sendType )
	{
	case STATSEND_LEVELSHUTDOWN:
		{
			// make a map node in the KeyValues to use for this level
			char szMap[MAX_PATH+1]="";
			V_FileBase( engine->GetLevelName(), szMap, std::size( szMap ) );
			if ( !szMap[0] )
			{
				V_FileBase( MapName(), szMap, std::size( szMap ) );
			}

			KeyValues *pKVMap = new KeyValues( "map" );
			pKV->AddSubKey( pKVMap );
			pKVMap->SetString( "mapname", szMap );
			pKV = pKVMap;

		}
		break;
	case STATSEND_APPSHUTDOWN:
		break;
	default:
		contract_assert( false );
		break;
	}

	// add common data
	pGamestatsData->m_bHaveData |= AddBaseDataForSend( pKV, sendType );
    
	// add game-specific data
	pGamestatsData->m_bHaveData |= gamestats->AddDataForSend( pKV, sendType );
}
void CBaseGameStats_Driver::SendData()
{
	// if we don't own the data container or there's no valid data, nothing to do
	if ( !m_pGamestatsData || !m_pGamestatsData->m_bHaveData )
		return;

	// save the data to a buffer
	ks::text_writer buf;
	m_pGamestatsData->m_pKVData->RecursiveSaveToFile( buf, 0 );

	const bool bOutDirOverriden = false;
	const char *szOverrideDirectory = nullptr;
	if ( CommandLine()->FindParm( "-gamestatsfileoutputonly" ) || bOutDirOverriden )
	{
		// write file for debugging
		if ( !bOutDirOverriden ) // if there isn't an override path, put it in the game dir.
		{
			const char szFileName[] = "gamestats.dat";
			filesystem->WriteFile( szFileName, GAMESTATS_PATHID, buf.bytes() );
		}
		else
		{
			char szPathName[MAX_PATH] = {0};
			V_strncat( szPathName, szOverrideDirectory, MAX_PATH );
			V_AppendSlash( szPathName, MAX_PATH );
			int pathLen = V_strlen( szPathName );
			
			char szFileName[MAX_PATH] = {0};
			struct tm timeinfo;
			Plat_GetLocalTime( &timeinfo );
			int fileLen = V_snprintf( szFileName, MAX_PATH, "%s__%2d_%3d_%2d_%2d_%2d.txt", s_szPseudoUniqueID, 
				timeinfo.tm_year % 100, timeinfo.tm_yday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec );

			contract_assert( pathLen + fileLen < MAX_PATH );	fileLen;

			V_strncat( szPathName + pathLen, szFileName, MAX_PATH - pathLen );

			filesystem->WriteFile( szPathName, nullptr, buf.bytes() );
		}
	}
	else
	{
		if ( gamestatsuploader )
			gamestatsuploader->UploadGameStats( "", 1, buf.size(), buf.c_str() );
	}	

	ResetData();
}

extern ConVar closecaption;
bool CBaseGameStats_Driver::AddBaseDataForSend( KeyValues *pKV, StatSendType_t sendType )
{
	switch ( sendType )
	{
	case STATSEND_APPSHUTDOWN:
		if ( m_iNumLevels > 0 )
		{
			// add playtime data
			KeyValues *pKVData = new KeyValues( "playtime" );
			pKVData->SetInt( "TotalLevelTime", m_flTotalTimeInLevels );
			pKVData->SetInt( "NumLevels", m_iNumLevels );
			pKV->AddSubKey( pKVData );
			return true;
		}
		break;
	case STATSEND_LEVELSHUTDOWN:
		const int MIN_PERF_STAT_COLLECT_TIME = 60 * 5;	// send perf data if we have collected at least this many seconds
		bool bSend = m_bBufferFull || m_nWriteIndex >= MIN_PERF_STAT_COLLECT_TIME;
		if ( bSend )
		{
			// add perf data
			KeyValues *pKVPerf = new KeyValues( "perfdata" );

			SortFramerate();

			float flAverageFrameRate = AverageStat( &StatsBufferRecord_t::m_flFrameRate );
			float flMinFrameRate =	PercentileFramerate( 20 ); 
			float flMaxFrameRate =	PercentileFramerate( 80 ); 
			float flStandardDeviationFrameRate = StdDevStat( &StatsBufferRecord_t::m_flFrameRate, flAverageFrameRate );

			pKVPerf->SetFloat( "AvgFPS", flAverageFrameRate );
			pKVPerf->SetFloat( "MinFPS", flMinFrameRate );
			pKVPerf->SetFloat( "MaxFPS", flMaxFrameRate );
			pKVPerf->SetFloat( "StdDevFPS", flStandardDeviationFrameRate );
			// if encoded histogram is 0, it just means we have no samples, so we don't really need to report it (save on bandwidth)
			if ( uint64 nHist = m_FpsHistogram.Encode() )
			{
				pKVPerf->SetUint64( "FrameHistAll", nHist );
			}
			if ( uint64 nHistSmoke = m_FpsHistogramGame[ PERF_STATS_SMOKE ].Encode() )
			{
				pKVPerf->SetUint64( "FrameHistGame1", nHistSmoke );
			}
			if ( uint64 nHistBullet = m_FpsHistogramGame[ PERF_STATS_BULLET ].Encode() )
			{
				pKVPerf->SetUint64( "FrameHistGame2", nHistBullet );
			}
			if ( uint64 nHistPlayer = m_FpsHistogramGame[ PERF_STATS_PLAYER ].Encode() )
			{
				pKVPerf->SetUint64( "FrameHistGame3", nHistPlayer );
			}
			if ( uint64 nHistPlayerSpawn = m_FpsHistogramGame[ PERF_STATS_PLAYER_SPAWN ].Encode() )
			{
				pKVPerf->SetUint64( "FrameHistGame4", nHistPlayerSpawn );
			}
			m_FpsHistogram.Reset();
			m_FpsHistogramGame[ PERF_STATS_SMOKE ].Reset();
			m_FpsHistogramGame[ PERF_STATS_BULLET ].Reset();
			m_FpsHistogramGame[ PERF_STATS_PLAYER ].Reset();
			m_FpsHistogramGame[ PERF_STATS_PLAYER_SPAWN ].Reset();

			// Determine the min/avg/max Server Ping and store the results 
			float flAverageServerPing = AverageStat( &StatsBufferRecord_t::m_flServerPing );
			pKVPerf->SetFloat( "AvgServerPing", flAverageServerPing );

			// Avg/StdDev main thread time
			float flAvgMainThreadTime = AverageStat( &StatsBufferRecord_t::m_flMainThreadTimeMS );
			float flStdDevMainThreadTime = StdDevStat( &StatsBufferRecord_t::m_flMainThreadTimeMS, flAvgMainThreadTime );
			pKVPerf->SetFloat( "AvgMainThreadTime", flAvgMainThreadTime );
			pKVPerf->SetFloat( "StdDevMainThreadTime", flStdDevMainThreadTime );

			// Avg/StdDev main thread wait time (time waiting for the render thread)
			float flAvgMainThreadWaitTime = AverageStat( &StatsBufferRecord_t::m_flMainThreadWaitTimeMS );
			float flStdDevMainThreadWaitTime = StdDevStat( &StatsBufferRecord_t::m_flMainThreadWaitTimeMS, flAvgMainThreadWaitTime );
			pKVPerf->SetFloat( "AvgMainThreadWaitTime", flAvgMainThreadWaitTime );
			pKVPerf->SetFloat( "StdDevMainThreadWaitTime", flStdDevMainThreadWaitTime );

			// Avg/StdDev render thread time
			float flAvgRenderThreadTime = AverageStat( &StatsBufferRecord_t::m_flRenderThreadTimeMS );
			float flStdDevRenderThreadTime = StdDevStat( &StatsBufferRecord_t::m_flRenderThreadTimeMS, flAvgRenderThreadTime );
			pKVPerf->SetFloat( "AvgRenderThreadTime", flAvgRenderThreadTime );
			pKVPerf->SetFloat( "StdDevRenderThreadTime", flStdDevRenderThreadTime );

			// Avg/StdDev render thread wait time (time waiting for the gpu, ForceHardwareSync)
			float flAvgRenderThreadWaitTime = AverageStat( &StatsBufferRecord_t::m_flRenderThreadWaitTimeMS );
			float flStdDevRenderThreadWaitTime = StdDevStat( &StatsBufferRecord_t::m_flRenderThreadWaitTimeMS, flAvgRenderThreadWaitTime );
			pKVPerf->SetFloat( "AvgRenderThreadWaitTime", flAvgRenderThreadWaitTime );
			pKVPerf->SetFloat( "StdDevRenderThreadWaitTime", flStdDevRenderThreadWaitTime );

			int nCPUThrottlingSamples = m_nCPUUnthrottledCount + m_nCPUThrottle1Count + m_nCPUThrottle2Count;
			if ( nCPUThrottlingSamples )
			{
				float fThrottledToLevel1 = m_nCPUThrottle1Count * 100.0f / nCPUThrottlingSamples;
				float fThrottledToLevel2 = m_nCPUThrottle2Count * 100.0f / nCPUThrottlingSamples;
				pKVPerf->SetFloat( "PercentageCPUThrottledToLevel1", fThrottledToLevel1 );
				pKVPerf->SetFloat( "PercentageCPUThrottledToLevel2", fThrottledToLevel2 );
			}

			pKV->AddSubKey( pKVPerf );
		}

		// Only keeping track of using voice in a multiplayer game
		if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
		{
			pKV->SetInt( "UsedVoice", m_bDidVoiceChat );
		}

		pKV->SetInt( "Caption", closecaption.GetInt() );

		// Set language to "unknown", then check to see if we can obtain the language.
		char language[64] = "unknown";
		if ( engine )
		{
			engine->GetUILanguage( language, sizeof( language ) );
			pKV->SetInt( "BugSub", engine->GetBugSubmissionCount() );
			engine->ClearBugSubmissionCount();
		}
		pKV->SetString( "Language", language );

		if ( materials )
		{
			const MaterialSystem_Config_t &config = materials->GetCurrentConfigForVideoCard();
			pKV->SetBool( "Windowed", config.Windowed() );
			pKV->SetBool( "WindowedNoBorder", config.NoWindowBorder() );
		}

		// We want to know if a user is running with cheats on so that we can filter out their data
		// depending on what we're looking for.
		if ( nullptr != sv_cheats  )
		{
			pKV->SetInt( "Cheats", sv_cheats->GetInt() );			
		}

		int mapTime = gpGlobals->realtime - m_flLevelStartTime;
		pKV->SetInt( "MapTime", mapTime );

		// reset buffer for next level. This is also done by ResetData().
		m_bBufferFull = false;
		m_nWriteIndex = 0;
		// Reset last realtime timestamp so that we don't compute the fps 
		// the first time we re-enter UpdatePerfStats
		m_flLastRealTime	= -1.0f;
		m_flLastSampleTime	= -1.0f;

		return bSend;
		break;
	}

	return false;
}

void CBaseGameStats_Driver::ResetData()
{

	if ( m_pGamestatsData != nullptr )
	{
		delete m_pGamestatsData;
		m_pGamestatsData = nullptr;
	}

	m_bBufferFull = false;
	m_nWriteIndex = 0;
	m_bDidVoiceChat = false;

	// Reset the CPU throttling statistics.
	m_nCPUUnthrottledCount = 0;
	m_nCPUThrottle1Count = 0;
	m_nCPUThrottle2Count = 0;
	// Track when we last received data.
	m_lastCPUFrequencyTimestamp = 0.0;

	m_pGamestatsData = new CGamestatsData();
	KeyValues *pKV = m_pGamestatsData->m_pKVData;
	pKV->SetInt( "IsPc", true );
	pKV->SetInt( "version", GAMESTATS_VERSION );
	pKV->SetString( "srcid", s_szPseudoUniqueID );

	const CPUInformation &cpu = GetCPUInformation();
	OverWriteCharsWeHate( cpu.m_szProcessorID );
	pKV->SetString( "CPUID", cpu.m_szProcessorID );
	pKV->SetFloat( "CPUGhz", cpu.m_Speed * ( 1.0 / 1.0e9 ) );
	pKV->SetUint64( "CPUModel", cpu.m_nModel );
	pKV->SetUint64( "CPUFeatures0", cpu.m_nFeatures[ 0 ] );
	pKV->SetUint64( "CPUFeatures1", cpu.m_nFeatures[ 1 ] );
	pKV->SetUint64( "CPUFeatures2", cpu.m_nFeatures[ 2 ] );
	pKV->SetInt( "NumCores", cpu.m_nPhysicalProcessors );

	MaterialAdapterInfo_t gpu;
	materials->GetDisplayAdapterInfo( materials->GetCurrentAdapter(), gpu );

	CMatRenderContextPtr pRenderContext( materials );
	int dest_width,dest_height;
	pRenderContext->GetRenderTargetDimensions( dest_width, dest_height );

	if ( gpu.m_pDriverName )
	{
		OverWriteCharsWeHate( gpu.m_pDriverName );
	}
	pKV->SetString( "GPUDrv", SafeString( gpu.m_pDriverName ) );
	pKV->SetInt( "GPUVendor", gpu.m_VendorID );
	pKV->SetInt( "GPUDeviceID", gpu.m_DeviceID );
	pKV->SetString( "GPUDriverVersion", CFmtStr( "%d.%d", gpu.m_nDriverVersionHigh, gpu.m_nDriverVersionLow ) );
	pKV->SetInt( "DxLvl", g_pMaterialSystemHardwareConfig->GetDXSupportLevel() );
	pKV->SetInt( "Width", dest_width );
	pKV->SetInt( "Height", dest_height );
	pKV->SetInt( "IsSplitScreen", UI_IsSplitScreen() );
	
	engine->SetGamestatsData( m_pGamestatsData );

}

void CBaseGameStats_Driver::OnRestore()
{
	PossibleMapChange();

	gamestats->Event_LoadGame();
}

void CBaseGameStats_Driver::FrameUpdatePostEntityThink()
{
	bool bGamePaused = ( gpGlobals->frametime == 0.0f );

	if ( !m_bInLevel )
	{
		m_flPauseStartTime = 0.0f;
	}
	else if ( m_bGamePaused != bGamePaused )
	{
		if ( bGamePaused )
		{
			m_flPauseStartTime = gpGlobals->realtime;
		}
		else if ( m_flPauseStartTime != 0.0f )
		{
			float flPausedTime = gpGlobals->realtime - m_flPauseStartTime;
			if ( flPausedTime < 0.0f )
			{
				contract_assert( 0 );
				Warning( "Game paused time showing up negative (rt %f pausestart %f)\n", gpGlobals->realtime, m_flPauseStartTime );
				flPausedTime = 0.0f;
			}

			// Remove this from global time counters

			//			Msg( "Pause:  adding %f to level starttime\n", flPausedTime );

			m_flLevelStartTime += flPausedTime;
			m_flPauseStartTime = 0.0f;

			//			Msg( "Paused for %.2f seconds\n", flPausedTime );
		}
		m_bGamePaused = bGamePaused;
	}
}

bool StatsTrackingIsFullyEnabled( void )
{
	return CBGSDriver.m_bEnabled && gamestats->StatTrackingEnabledForMod();
}

void CBaseGameStats::Clear( void )
{
}

//-----------------------------------------------------------------------------
// Nukes any dangerous characters and replaces w/space char
//-----------------------------------------------------------------------------
void OverWriteCharsWeHate( char *pStr )
{
	while( *pStr )
	{
		switch( *pStr )
		{
		case '\n':
		case '\r':
		case '\\':
		case '\"':
		case '\'':
		case '\032':
		case ';':
			*pStr = ' ';
		}
		pStr++;
	}
}

void CFpsHistogram::Update( uint8 nBin)
{
	m_nFps[ nBin & ( FPS_BIN_COUNT - 1 ) ]++;
	m_nCpuWaits[ nBin & ( FPS_BIN_COUNT - 1 ) ] += ( nBin >> 7 );
}

CFpsSelectiveHistogram::CFpsSelectiveHistogram()
{
	m_nLookAhead = 0;
	m_nLookBack = 0;
}

void CFpsSelectiveHistogram::Fire( int nLookAhead, int nLookBack, const CFpsHistory &history)
{
	// get the prerecorded stats
	if ( m_nLookAhead < nLookAhead )
	{
		m_nLookAhead = nLookAhead;
	}
	int nCatchUp = std::min( nLookBack, m_nLookBack );
	// exhaust back-stats buffer
	for ( int i = 0; i < nCatchUp; ++i )
	{
		CFpsHistogram::Update( history[ i ] );
	}

	m_nLookBack = 0;
}

void CFpsSelectiveHistogram::Update( uint8 nBin)
{
	if ( m_nLookAhead > 0 )
	{
		m_nLookAhead--;
		CFpsHistogram::Update( nBin );
	}
	else
	{
		++m_nLookBack;
	}
}

bool CBaseGameStats::ShouldSendDataOnAppShutdown()
{
	// by default, clients send data at app shutdown and servers don't
	return true;
}

#undef WALKED_AWAY_FROM_KEYBOARD_SECONDS

class CBaseLesson;
class CBasePlayer;

using namespace ::ks::reflect;

// Purpose: Client handler implementations for instruction players how to play

PRECACHE_REGISTER_BEGIN( GLOBAL, GameinstructorIcons )
	PRECACHE( KV_DEP_FILE, "scripts/instructor_texturemanifest.txt" )
PRECACHE_REGISTER_END()

#define MOD_DIR "MOD"

inline constexpr const char *GAMEINSTRUCTOR_SCRIPT_FILE = "scripts/instructor_lessons.txt";
inline constexpr const char *GAMEINSTRUCTOR_MOD_SCRIPT_FILE = "scripts/mod_lessons.txt";

inline constexpr const char *GAMEINSTRUCTOR_SAVE_FILE = "game_instructor_counts.txt";

// Game instructor auto game system instantiation
CGameInstructor g_GameInstructor[ MAX_SPLITSCREEN_PLAYERS ];
CGameInstructor &GetGameInstructor()
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	return g_GameInstructor[ GET_ACTIVE_SPLITSCREEN_SLOT() ];
}

ConVar gameinstructor_save_restore_lessons( side_registry(), "gameinstructor_save_restore_lessons", "1", FCVAR_CHEAT, "Set to 0 to disable save/load of open lesson opportunities in single player." );
ConVar gameinstructor_verbose( side_registry(), "gameinstructor_verbose", "0", FCVAR_CHEAT, "Set to 1 for standard debugging or 2 (in combo with gameinstructor_verbose_lesson) to show update actions." );
ConVar gameinstructor_verbose_lesson( side_registry(), "gameinstructor_verbose_lesson", "", FCVAR_CHEAT, "Display more verbose information for lessons have this name." );
ConVar gameinstructor_find_errors( side_registry(), "gameinstructor_find_errors", "0", FCVAR_CHEAT, "Set to 1 and the game instructor will run EVERY scripted command to uncover errors." );

void GameInstructorEnable_ChangeCallback( ConVar *var, const char *pOldValue, float flOldValue );
ConVar gameinstructor_enable( side_registry(), "gameinstructor_enable", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Display in game lessons that teach new players.", GameInstructorEnable_ChangeCallback );
extern ConVar sv_gameinstructor_disable;

ConVar gameinstructor_start_sound_cooldown( side_registry(), "gameinstructor_start_sound_cooldown", "4.0", FCVAR_NONE, "Number of seconds forced between similar lesson start sounds." );

static void FixGameInstructorLessonNameForTitleData( char *chBuffer )
{
	for ( ; chBuffer && *chBuffer; ++ chBuffer )
	{
		char const ch = *chBuffer;
		if ( ch >= 'a' && ch <= 'z' )
			continue;
		if ( ch >= 'A' && ch <= 'Z' )
			continue;
		if ( ch >= '0' && ch <= '9' )
			continue;
		if ( ch == '.' || ch == '_' )
			continue;
		*chBuffer = '_';
	}
}

// Enable or Disable the game instructor based on the client setting
void EnableDisableInstructor( void )
{
	bool bEnabled = (!sv_gameinstructor_disable.GetBool() && gameinstructor_enable.GetBool());

	if ( CSGameRules() && CSGameRules()->IsPlayingTraining() )
	{
		bEnabled = true;
	}

	if ( bEnabled )
	{
		// Game instructor has been enabled, so init it!
		GetGameInstructor().Init();
	}
	else 
	{
		// Game instructor has been disabled, so shut it down!
		GetGameInstructor().Shutdown();
	}
}

void GameInstructorEnable_ChangeCallback( ConVar *var, const char *pOldValue, float flOldValue )
{
	for ( int i = 0 ; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( i );
		if ( ( flOldValue != 0.0f ) != gameinstructor_enable.GetBool() )
		{
			EnableDisableInstructor();
		}
	}
}

void SVGameInstructorDisable_ChangeCallback( ConVar *var, const char *pOldValue, float flOldValue );
ConVar sv_gameinstructor_disable( side_registry(), "sv_gameinstructor_disable", "0", FCVAR_REPLICATED | FCVAR_RELEASE, "Force all clients to disable their game instructors.", SVGameInstructorDisable_ChangeCallback );
void SVGameInstructorDisable_ChangeCallback( ConVar *var, const char *pOldValue, float flOldValue )
{
	if ( !engine )
		return;

	for ( int i = 0 ; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( i );
		EnableDisableInstructor();
	}
}

void GameInstructor_KeyValueBuilder( KeyValues *pKeyValues )
{
	GetGameInstructor().KeyValueBuilder( pKeyValues );
}

// Merged from L4D but waiting on other code to be merged before this can compile 
class CGameInstructorUserNotificationsListener
{
public:
	CGameInstructorUserNotificationsListener() : m_nRefCount( 0 ) {}

public:
	void RefCount( int nDelta );

public:
	void OnEvent( KeyValues *pEvent );

protected:
	void OnGameUsersChanged();
	void OnStorageDeviceAvailable( int iCtrlr );

protected:
	int m_nRefCount;
};

void CGameInstructorUserNotificationsListener::RefCount( int nDelta )
{
	if ( !g_pMatchFramework )
		return;

	if ( m_nRefCount <= 0 && nDelta > 0 )
	{
		g_pMatchFramework->GetEventsSubscription()->Subscribe( this, [this]( KeyValues *pEvent ) { OnEvent( pEvent ); } );
	}

	if ( m_nRefCount > 0 && m_nRefCount - nDelta <= 0 )
	{
		g_pMatchFramework->GetEventsSubscription()->Unsubscribe( this );
	}

	m_nRefCount = std::max( 0, m_nRefCount + nDelta );
}

void CGameInstructorUserNotificationsListener::OnEvent( KeyValues *pEvent )
{
	char const *szEvent = pEvent->GetName();

	if ( !V_stricmp( szEvent, "OnProfileDataLoaded" ) )
	{
		OnStorageDeviceAvailable( pEvent->GetInt( "iController" ) );
	}
	else if ( !V_stricmp( szEvent, "OnProfilesChanged" ) )
	{
		OnGameUsersChanged();
	}
}

void CGameInstructorUserNotificationsListener::OnGameUsersChanged()
{
	for ( int i = 0 ; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( i );
		GetGameInstructor().ResetDisplaysAndSuccesses();
	}
}

void CGameInstructorUserNotificationsListener::OnStorageDeviceAvailable( int iCtrlr )
{

	ACTIVE_SPLITSCREEN_PLAYER_GUARD( iSlot );
	GetGameInstructor().RefreshDisplaysAndSuccesses();
}

CGameInstructorUserNotificationsListener s_GameInstructorUserNotificationsListener;

void GameInstructor_Init()
{
	// Subscribe for match events
	s_GameInstructorUserNotificationsListener.RefCount( +1 );
}

void GameInstructor_Shutdown()
{
	// Unsubscribe for match events
	s_GameInstructorUserNotificationsListener.RefCount( -1 );
}

//
// CGameInstructor
//

bool CGameInstructor::Init( void )
{
	// Make sure split slot is up to date
	for ( int i = 0 ; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( i );
		if ( &GetGameInstructor() == this )
		{
			SetSlot( i );
			break;
		}
	}

	ACTIVE_SPLITSCREEN_PLAYER_GUARD( m_nSplitScreenSlot );

	if ( (!gameinstructor_enable.GetBool() || sv_gameinstructor_disable.GetBool()) && !(CSGameRules() && CSGameRules()->IsPlayingTraining()) )
	{
		// Don't init if it's disabled
		return true;
	}

	if ( gameinstructor_verbose.GetInt() > 0 )
	{
		ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
		ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Initializing...\n" );
	}

	if ( m_bEnsureThatInitIsNotCalledMultipleTimes )
	{
		if ( gameinstructor_verbose.GetInt() > 0 )
		{
			ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
			ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Initializing is prevented by reentry guard, high level code probably has a problem...\n" );
		}
		return true;
	}
	m_bEnsureThatInitIsNotCalledMultipleTimes = true;

	m_bNoDraw = false;
	m_bHiddenDueToOtherElements = false;

	m_iCurrentPriority = 0;
	m_hLastSpectatedPlayer = nullptr;
	m_bSpectatedPlayerChanged = false;

	m_szPreviousStartSound[ 0 ] = '\0';
	m_fNextStartSoundTime = 0;

	ReadLessonsFromFile( GAMEINSTRUCTOR_MOD_SCRIPT_FILE );
	ReadLessonsFromFile( GAMEINSTRUCTOR_SCRIPT_FILE );

	InitLessonPrerequisites();

	KeyValueSaver().InitKeyValues( GAMEINSTRUCTOR_SAVE_FILE, GameInstructor_KeyValueBuilder );

	ListenForGameEvent( "gameinstructor_draw" );
	ListenForGameEvent( "gameinstructor_nodraw" );

	ListenForGameEvent( "round_end" );
	ListenForGameEvent( "round_start" );
	ListenForGameEvent( "player_death" );
	ListenForGameEvent( "player_team" );
	ListenForGameEvent( "player_disconnect" );
	ListenForGameEvent( "map_transition" );
	ListenForGameEvent( "game_newmap" );

	ListenForGameEvent( "set_instructor_group_enabled" );

	EvaluateLessonsForGameRules();

	return true;
}

void CGameInstructor::Shutdown( void )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( m_nSplitScreenSlot );

	if ( gameinstructor_verbose.GetInt() > 0 )
	{
		ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
		ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Shutting down...\n" );
	}

	CloseAllOpenOpportunities();

	WriteSaveData();

	// Clear out all the lessons
	for ( int i = 0; i < std::ssize( m_Lessons ); ++i )
	{
		if ( m_Lessons[ i ] )
		{
			m_Lessons[ i ]->StopListeningForAllEvents();
			delete m_Lessons[ i ];
			m_Lessons[ i ] = nullptr;
		}
	}
	m_Lessons.clear();

	m_LessonGroupConVarToggles.clear();

	// Stop listening for events
	StopListeningForAllEvents();

	m_bEnsureThatInitIsNotCalledMultipleTimes = false;
}

void CGameInstructor::UpdateHiddenByOtherElements( void )
{
	bool bHidden = Mod_HiddenByOtherElements();

	if ( bHidden && !m_bHiddenDueToOtherElements )
	{
		StopAllLessons();
	}

	m_bHiddenDueToOtherElements = bHidden;
}

void CGameInstructor::Update( float frametime )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( m_nSplitScreenSlot );

	CVProfScope _( "CGameInstructor::Update", 0, "GameInstructor", false, BUDGETFLAG_OTHER );

	UpdateHiddenByOtherElements();

	if ( (!gameinstructor_enable.GetBool() || m_bNoDraw || m_bHiddenDueToOtherElements) && !(CSGameRules() && CSGameRules()->IsPlayingTraining()) )
	{
		// Don't update if disabled or hidden
		return;
	}

	if ( gameinstructor_find_errors.GetBool() )
	{
		FindErrors();

		gameinstructor_find_errors.SetValue( 0 );
	}

	if ( m_bSpectatedPlayerChanged )
	{
		// Safe spot to clean out stale lessons if spectator changed
		if ( gameinstructor_verbose.GetInt() > 0 )
		{
			ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
			ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Spectated player changed...\n" );
		}

		CloseAllOpenOpportunities();

		m_bSpectatedPlayerChanged = false;
	}

	// Loop through all the lesson roots and reset their active status
	for ( int i = std::ssize( m_OpenOpportunities ) - 1; i >= 0; --i )
	{
		CBaseLesson *pLesson = m_OpenOpportunities[ i ];
		CBaseLesson *pRootLesson = pLesson->GetRoot();

		if ( pRootLesson->InstanceType() == LESSON_INSTANCE_SINGLE_ACTIVE )
		{
			pRootLesson->SetInstanceActive( false );
		}
	}

	int iCurrentPriority = 0;

	// Loop through all the open lessons
	for ( int i = std::ssize( m_OpenOpportunities ) - 1; i >= 0; --i )
	{
		CBaseLesson *pLesson = m_OpenOpportunities[ i ];

		if ( !pLesson->IsOpenOpportunity() || pLesson->IsTimedOut() )
		{
			// This opportunity has closed
			CloseOpportunity( pLesson );
			continue;
		}

		// Lesson should be displayed, so it can affect priority
		CBaseLesson *pRootLesson = pLesson->GetRoot();

		bool bShouldDisplay = pLesson->ShouldDisplay();
		bool bIsLocked = pLesson->IsLocked();

		if ( ( bShouldDisplay || bIsLocked ) && 
			 ( pLesson->GetPriority() >= m_iCurrentPriority || pLesson->NoPriority() || bIsLocked ) && 
			 ( pRootLesson && ( pRootLesson->InstanceType() != LESSON_INSTANCE_SINGLE_ACTIVE || !pRootLesson->IsInstanceActive() ) ) )
		{
			// Lesson is at the highest priority level, isn't violating instance rules, and has met all the prerequisites
			if ( UpdateActiveLesson( pLesson, pRootLesson ) || pRootLesson->IsLearned() )
			{
				// Lesson is active
				if ( pLesson->IsVisible() || pRootLesson->IsLearned() )
				{
					pRootLesson->SetInstanceActive( true );

					if ( iCurrentPriority < pLesson->GetPriority() && !pLesson->NoPriority() )
					{
						// This active or learned lesson has the highest priority so far
						iCurrentPriority = pLesson->GetPriority();
					}
				}
			}
			else
			{
				// On second thought, this shouldn't have been displayed
				bShouldDisplay = false;
			}
		}
		else
		{
			// Lesson shouldn't be displayed right now
			UpdateInactiveLesson( pLesson );
		}
	}

	// Set the priority for next frame
	if ( gameinstructor_verbose.GetInt() > 1 && m_iCurrentPriority != iCurrentPriority )
	{
		ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
		ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Priority changed from " );
		ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "%i ", m_iCurrentPriority );
		ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "to " );
		ConColorMsg( CBaseLesson::m_rgbaVerboseOpen, "%i", iCurrentPriority );
		ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ".\n" );
	}

	m_iCurrentPriority = iCurrentPriority;
}

void CGameInstructor::FireGameEvent( IGameEvent *event )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( m_nSplitScreenSlot );

	CVProfScope _( "CGameInstructor::FireGameEvent", 0, "GameInstructor", false, BUDGETFLAG_OTHER );

	const char *name = event->GetName();

	if ( V_strcmp( name, "gameinstructor_draw" ) == 0 )
	{
		if ( m_bNoDraw )
		{
			if ( gameinstructor_verbose.GetInt() > 0 )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Set to draw...\n" );
			}

			m_bNoDraw = false;
		}
	}
	else if ( V_strcmp( name, "gameinstructor_nodraw" ) == 0 )
	{
		if ( !m_bNoDraw )
		{
			if ( gameinstructor_verbose.GetInt() > 0 )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Set to not draw...\n" );
			}

			m_bNoDraw = true;
			StopAllLessons();
		}
	}
	else if ( V_strcmp( name, "round_end" ) == 0 )
	{
		if ( gameinstructor_verbose.GetInt() > 0 )
		{
			ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
			ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Round ended...\n" );
		}

		CloseAllOpenOpportunities();
	}
	else if ( V_strcmp( name, "round_start" ) == 0 )
	{
		if ( gameinstructor_verbose.GetInt() > 0 )
		{
			ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
			ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Round started...\n" );
		}

		CloseAllOpenOpportunities();

		EvaluateLessonsForGameRules();
	}
	else if ( V_strcmp( name, "player_death" ) == 0 )
	{
		CBasePlayer *pLocalPlayer = GetLocalPlayer();
		if ( pLocalPlayer && pLocalPlayer == UTIL_PlayerByUserId( event->GetInt( "userid" ) ) )
		{
			if ( gameinstructor_verbose.GetInt() > 0 )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Local player died...\n" );
			}

			for ( int i = std::ssize( m_OpenOpportunities ) - 1; i >= 0; --i )
			{
				CBaseLesson *pLesson = m_OpenOpportunities[ i ];
				CBaseLesson *pRootLesson = pLesson->GetRoot();
				if ( !pRootLesson->CanOpenWhenDead() )
				{
					CloseOpportunity( pLesson );
				}
			}
		}
	}
	else if ( V_strcmp( name, "player_team" ) == 0 )
	{
		CBasePlayer *pLocalPlayer = GetLocalPlayer();
		if ( pLocalPlayer && pLocalPlayer == UTIL_PlayerByUserId( event->GetInt( "userid" ) ) && 
			 ( event->GetInt( "team" ) != event->GetInt( "oldteam" ) || event->GetBool( "disconnect" ) ) )
		{
			if ( gameinstructor_verbose.GetInt() > 0 )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Local player changed team (or disconnected)...\n" );
			}

			CloseAllOpenOpportunities();
		}

		EvaluateLessonsForGameRules();
	}
	else if ( V_strcmp( name, "player_disconnect" ) == 0 )
	{
		CBasePlayer *pLocalPlayer = GetLocalPlayer();
		if ( pLocalPlayer && pLocalPlayer == UTIL_PlayerByUserId( event->GetInt( "userid" ) ) )
		{
			if ( gameinstructor_verbose.GetInt() > 0 )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Local player disconnected...\n" );
			}

			CloseAllOpenOpportunities();
		}
	}
	else if ( V_strcmp( name, "map_transition" ) == 0 )
	{
		if ( gameinstructor_verbose.GetInt() > 0 )
		{
			ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
			ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Map transition...\n" );
		}

		CloseAllOpenOpportunities();

		if ( m_bNoDraw )
		{
			if ( gameinstructor_verbose.GetInt() > 0 )
			{
				ConColorMsg( Color( 255, 128, 64, 255 ), "GAME INSTRUCTOR: " );
				ConColorMsg( Color( 64, 128, 255, 255 ), "Set to draw...\n" );
			}

			m_bNoDraw = false;
		}
	}
	else if ( V_strcmp( name, "game_newmap" ) == 0 )
	{
		if ( gameinstructor_verbose.GetInt() > 0 )
		{
			ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
			ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "New map...\n" );
		}

		CloseAllOpenOpportunities();

		if ( m_bNoDraw )
		{
			if ( gameinstructor_verbose.GetInt() > 0 )
			{
				ConColorMsg( Color( 255, 128, 64, 255 ), "GAME INSTRUCTOR: " );
				ConColorMsg( Color( 64, 128, 255, 255 ), "Set to draw...\n" );
			}

			m_bNoDraw = false;
		}
	}
	else if ( V_strcmp( name, "set_instructor_group_enabled" ) == 0 )
	{
		const char *pszGroup = event->GetString( "group" );
		bool bEnabled = event->GetInt( "enabled" ) != 0;

		if ( pszGroup && pszGroup[0] )
		{
			SetLessonGroupEnabled( pszGroup, bEnabled );
		}
	}
}

void CGameInstructor::DefineLesson( CBaseLesson *pLesson )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( m_nSplitScreenSlot );

	if ( gameinstructor_verbose.GetInt() > 0 )
	{
		ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
		ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Lesson " );
		ConColorMsg( CBaseLesson::m_rgbaVerboseName, "\"%s\" ", pLesson->GetName() );
		ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "defined.\n" );
	}

	m_Lessons.push_back( pLesson );
}

const CBaseLesson * CGameInstructor::GetLesson( const char *pchLessonName )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( m_nSplitScreenSlot );
	return GetLesson_Internal( pchLessonName );
}

bool CGameInstructor::IsLessonOfSameTypeOpen( const CBaseLesson *pLesson ) const
{
	for ( int i = 0; i < std::ssize( m_OpenOpportunities ); ++i )
	{
		CBaseLesson *pOpenOpportunity = m_OpenOpportunities[ i ];

		if ( pOpenOpportunity->GetNameSymbol() == pLesson->GetNameSymbol() )
		{	
			return true;
		}
	}

	return false;
}

bool CGameInstructor::ReadSaveData( void )
{
	// for external playtests, don't ever read in persisted instructor state, always start fresh
	if( CommandLine()->FindParm( "-playtest" ) )
		return true;

	if ( m_bHasLoadedSaveData )
		return true;
	
	// Always reset state first in case storage device
	// was declined or ends up in faulty state
	ResetDisplaysAndSuccesses();

	if ( m_nSplitScreenSlot < 0 || m_nSplitScreenSlot >= (int) XBX_GetNumGameUsers() )
		return true;

	IPlayerLocal *pPlayer = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( XBX_GetUserId( m_nSplitScreenSlot ) );
	if ( !pPlayer )
		return true;

	TitleDataFieldsDescription_t const *fields = g_pMatchFramework->GetMatchTitle()->DescribeTitleDataStorage();

	m_bHasLoadedSaveData = true;

	int nVersion = 0;

	for ( int i = 0; i < std::ssize( m_Lessons );++i )
	{
		CBaseLesson *pLesson = m_Lessons[i];
		if ( !pLesson || ( pLesson->GetDisplayLimit() == 0 && pLesson->GetSuccessLimit() == 0 ) )
			continue;

		CFmtStr tdKey( "GI.lesson.%s", pLesson->GetName() );
		FixGameInstructorLessonNameForTitleData( tdKey.Access() );
		TitleDataFieldsDescription_t const *fdKey = TitleDataFieldsDescriptionFindByString( fields, tdKey );
		if ( !fdKey )
		{
			Warning( "CGameInstructor::ReadSaveData failed to read %s\n", tdKey.Access() );
			continue;
		}

		TitleData3::GameInstructorData_t::LessonInfo_t li;
		li.u8dummy = TitleDataFieldsDescriptionGetValue<uint8>( fdKey, pPlayer );
		
		pLesson->SetDisplayCount( li.display );
		pLesson->SetSuccessCount( li.success );

		if ( V_strcmp( pLesson->GetName(), "version number" ) == 0 )
		{
			nVersion = pLesson->GetSuccessCount();
		}
	}

	CBaseLesson *pLessonVersionNumber = GetLesson_Internal( "version number" );
	if ( pLessonVersionNumber && !pLessonVersionNumber->IsLearned() )
	{
		ResetDisplaysAndSuccesses();
		pLessonVersionNumber->SetSuccessCount( pLessonVersionNumber->GetSuccessLimit() );
		KeyValueSaver().MarkKeyValuesDirty( GAMEINSTRUCTOR_SAVE_FILE );
	}

	return true;
}

bool CGameInstructor::WriteSaveData( void )
{
	if ( engine->IsPlayingDemo() )
		return false;

	return KeyValueSaver().WriteDirtyKeyValues( GAMEINSTRUCTOR_SAVE_FILE );
}

void CGameInstructor::KeyValueBuilder( KeyValues *pKeyValues )
{
	if ( m_nSplitScreenSlot < 0 || m_nSplitScreenSlot >= (int) XBX_GetNumGameUsers() )
		return;
	
	IPlayerLocal *pPlayer = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( XBX_GetUserId( m_nSplitScreenSlot ) );
	if ( !pPlayer )
		return;

	TitleDataFieldsDescription_t const *fields = g_pMatchFramework->GetMatchTitle()->DescribeTitleDataStorage();

	// Build key value data to save
	for ( int i = 0; i < std::ssize( m_Lessons );++i )
	{
		CBaseLesson *pLesson = m_Lessons[ i ];
		TitleData3::GameInstructorData_t::LessonInfo_t li;
		li.u8dummy = 0;
		li.display = pLesson->GetDisplayCount() & 0xF;
		li.success = pLesson->GetSuccessCount() & 0xF;

		CFmtStr tdKey( "GI.lesson.%s", pLesson->GetName() );
		FixGameInstructorLessonNameForTitleData( tdKey.Access() );
		TitleDataFieldsDescription_t const *fdKey = TitleDataFieldsDescriptionFindByString( fields, tdKey );
		if ( fdKey )
		{
			TitleDataFieldsDescriptionSetValue<uint8>( fdKey, pPlayer, li.u8dummy );
		}
		else
		{
			DevWarning( "CGameInstructor::KeyValueBuilder did not save %s; is it missing from inc_gameinstructor_lessons.inc\n", tdKey.Access() );
		}
	}
}

void CGameInstructor::RefreshDisplaysAndSuccesses( void )
{
	m_bHasLoadedSaveData = false;
	ReadSaveData();
}

void CGameInstructor::ResetDisplaysAndSuccesses( void )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( m_nSplitScreenSlot );
	if ( gameinstructor_verbose.GetInt() > 0 )
	{
		ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
		ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Reset all lesson display and success counts.\n" );
	}

	for ( int i = 0; i < std::ssize( m_Lessons ); ++i )
	{
		m_Lessons[ i ]->ResetDisplaysAndSuccesses();
	}
}

void CGameInstructor::MarkDisplayed( const char *pchLessonName )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( m_nSplitScreenSlot );
	CBaseLesson *pLesson = GetLesson_Internal( pchLessonName );
	if ( !pLesson )
		return;

	if ( gameinstructor_verbose.GetInt() > 0 )
	{
		ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
		ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Lesson " );
		ConColorMsg( CBaseLesson::m_rgbaVerboseOpen, "\"%s\" ", pLesson->GetName() );
		ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "marked as displayed.\n" );
	}

	if ( pLesson->IncDisplayCount() )
	{
		KeyValueSaver().MarkKeyValuesDirty( GAMEINSTRUCTOR_SAVE_FILE );
	}
}

void CGameInstructor::MarkSucceeded( const char *pchLessonName )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( m_nSplitScreenSlot );
	CBaseLesson *pLesson = GetLesson_Internal( pchLessonName );
	if ( !pLesson )
		return;

	if ( gameinstructor_verbose.GetInt() > 0 )
	{
		ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
		ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Lesson " );
		ConColorMsg( CBaseLesson::m_rgbaVerboseSuccess, "\"%s\" ", pLesson->GetName() );
		ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "marked as succeeded.\n" );
	}

	if ( pLesson->IncSuccessCount() )
	{
		KeyValueSaver().MarkKeyValuesDirty( GAMEINSTRUCTOR_SAVE_FILE );
	}
}

void CGameInstructor::PlaySound( const char *pchSoundName )
{
	// emit alert sound
	CBasePlayer *pLocalPlayer = CBasePlayer::GetLocalPlayer();
	if ( pLocalPlayer )
	{
		// Local player exists
		if ( pchSoundName[ 0 ] != '\0' && V_strcmp( m_szPreviousStartSound, pchSoundName ) != 0 )
		{
			V_strcpy_safe( m_szPreviousStartSound, pchSoundName );
			m_fNextStartSoundTime = 0.0f;
		}

		if ( gpGlobals->curtime >= m_fNextStartSoundTime && pchSoundName[ 0 ] != '\0' )
		{
			// A sound was specified, so play it!
			pLocalPlayer->EmitSound( pchSoundName );
			m_fNextStartSoundTime = gpGlobals->curtime + gameinstructor_start_sound_cooldown.GetFloat();
		}
	}
}

bool CGameInstructor::OpenOpportunity( CBaseLesson *pLesson )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( m_nSplitScreenSlot );
	// Get the root lesson
	CBaseLesson *pRootLesson = pLesson->GetRoot();
	if ( !pRootLesson )
	{
		if ( gameinstructor_verbose.GetInt() > 0 )
		{
			ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
			ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Opportunity " );
			ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\"%s\" ", pLesson->GetName() );
			ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "NOT opened (because root lesson could not be found).\n" );
		}

		delete pLesson;
		return false;
	}

	CBasePlayer *pLocalPlayer = GetLocalPlayer();
	if ( !pRootLesson->CanOpenWhenDead() && ( !pLocalPlayer || !pLocalPlayer->IsAlive() ) )
	{
		// If the player is dead don't allow lessons that can't be opened when dead
		if ( gameinstructor_verbose.GetInt() > 0 )
		{
			ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
			ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Opportunity " );
			ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\"%s\" ", pLesson->GetName() );
			ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "NOT opened (because player is dead and can_open_when_dead not set).\n" );
		}

		delete pLesson;
		return false;
	}

	if ( !pRootLesson->CanOpenOnceLearned() && pRootLesson->IsLearned() )
	{
		// If the player has learned this and we don't want it to open onced learned
		if ( gameinstructor_verbose.GetInt() > 0 )
		{
			ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
			ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Opportunity " );
			ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\"%s\" ", pLesson->GetName() );
			ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "NOT opened (because this is learned and ONCE_LEARNED_NEVER_OPEN is set).\n" );
		}

		delete pLesson;
		return false;
	}

	if ( !pRootLesson->PrerequisitesHaveBeenMet() )
	{
		// If the prereqs haven't been met, don't open it
		if ( gameinstructor_verbose.GetInt() > 0 )
		{
			ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
			ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Opportunity " );
			ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\"%s\" ", pLesson->GetName() );
			ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "NOT opened (because prereqs haven't been met).\n" );
		}

		delete pLesson;
		return false;
	}

	if ( pRootLesson->InstanceType() == LESSON_INSTANCE_FIXED_REPLACE )
	{
		CBaseLesson *pLessonToReplace = nullptr;
		CBaseLesson *pLastReplacableLesson = nullptr;

		int iInstanceCount = 0;

		// Check how many are already open
		for ( int i = std::ssize( m_OpenOpportunities ) - 1; i >= 0; --i )
		{
			CBaseLesson *pOpenOpportunity = m_OpenOpportunities[ i ];

			if ( pOpenOpportunity->GetNameSymbol() == pLesson->GetNameSymbol() && 
				 pOpenOpportunity->GetReplaceKeySymbol() == pLesson->GetReplaceKeySymbol() )
			{
				iInstanceCount++;

				if ( pRootLesson->ShouldReplaceOnlyWhenStopped() )
				{
					if ( !pOpenOpportunity->IsInstructing() )
					{
						pLastReplacableLesson = pOpenOpportunity;
					}
				}
				else
				{
					pLastReplacableLesson = pOpenOpportunity;
				}

				if ( iInstanceCount >= pRootLesson->GetFixedInstancesMax() )
				{
					pLessonToReplace = pLastReplacableLesson;
					break;
				}
			}
		}

		if ( pLessonToReplace )
		{
			// Take the place of the previous instance
			if ( gameinstructor_verbose.GetInt() > 0 )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Opportunity " );
				ConColorMsg( CBaseLesson::m_rgbaVerboseOpen, "\"%s\" ", pLesson->GetName() );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "replacing open lesson of same type.\n" );
			}

			pLesson->TakePlaceOf( pLessonToReplace );
			CloseOpportunity( pLessonToReplace );
		}
		else if ( iInstanceCount >= pRootLesson->GetFixedInstancesMax() )
		{
			// Don't add another lesson of this type
			if ( gameinstructor_verbose.GetInt() > 0 )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Opportunity " );
				ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\"%s\" ", pLesson->GetName() );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "NOT opened (there is too many started lessons of this type).\n" );
			}

			delete pLesson;
			return false;
		}
	}

	if ( gameinstructor_verbose.GetInt() > 0 )
	{
		ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
		ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Opportunity " );
		ConColorMsg( CBaseLesson::m_rgbaVerboseOpen, "\"%s\" ", pLesson->GetName() );
		ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "opened.\n" );
	}

	m_OpenOpportunities.push_back( pLesson );

	return true;
}

void CGameInstructor::DumpOpenOpportunities( void )
{
	ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
	ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Open lessons...\n" );

	for ( int i = std::ssize( m_OpenOpportunities ) - 1; i >= 0; --i )
	{
		CBaseLesson *pLesson = m_OpenOpportunities[ i ];
		CBaseLesson *pRootLesson = pLesson->GetRoot();

		Color color;

		if ( pLesson->IsInstructing() )
		{
			// Green
			color = CBaseLesson::m_rgbaVerboseOpen;
		}
		else if ( pRootLesson->IsLearned() && pLesson->GetPriority() >= m_iCurrentPriority )
		{
			// Yellow
			color = CBaseLesson::m_rgbaVerboseSuccess;
		}
		else
		{
			// Red
			color = CBaseLesson::m_rgbaVerboseClose;
		}

		ConColorMsg( color, "\t%s\n", pLesson->GetName() );
	}
}

KeyValues * CGameInstructor::GetScriptKeys( void )
{
	return m_pScriptKeys;
}

CBasePlayer * CGameInstructor::GetLocalPlayer( void )
{
	CBasePlayer *pLocalPlayer = CBasePlayer::GetLocalPlayer();

	// If we're not a developer, don't do the special spectator hook ups
	if ( !developer.GetBool() )
		return pLocalPlayer;

	// If there is no local player and we're not spectating, just return that
	if ( !pLocalPlayer || pLocalPlayer->GetTeamNumber() != TEAM_SPECTATOR )
		return pLocalPlayer;

	// We're purely a spectator let's get lessons of the person we're spectating
	CBasePlayer *pSpectatedPlayer = nullptr;

	if ( pLocalPlayer->GetObserverMode() == OBS_MODE_IN_EYE || pLocalPlayer->GetObserverMode() == OBS_MODE_CHASE )
	{
		pSpectatedPlayer = ToBasePlayer( pLocalPlayer->GetObserverTarget() );
	}

	if ( m_hLastSpectatedPlayer != pSpectatedPlayer )
	{
		// We're spectating someone new! Close all the stale lessons!
		m_bSpectatedPlayerChanged = true;
		m_hLastSpectatedPlayer = pSpectatedPlayer;
	}

	return pSpectatedPlayer;
}

void CGameInstructor::EvaluateLessonsForGameRules( void )
{
	// Enable everything by default
	for ( int i = 0; i < std::ssize( m_Lessons ); ++i )
	{
		m_Lessons[ i ]->SetEnabled( true );
	}

	// Then see if we should disable anything
	for ( int nConVar = 0; nConVar < std::ssize( m_LessonGroupConVarToggles ); ++nConVar )
	{
		LessonGroupConVarToggle_t *pLessonGroupConVarToggle = &(m_LessonGroupConVarToggles[ nConVar ]);
		if ( pLessonGroupConVarToggle->var.IsValid() )
		{
			if ( pLessonGroupConVarToggle->var.GetBool() )
			{
				SetLessonGroupEnabled( pLessonGroupConVarToggle->szLessonGroupName, false );
			}
		}
	}
}

void CGameInstructor::SetLessonGroupEnabled( const char *pszGroup, bool bEnabled )
{
	for ( int i = 0; i < std::ssize( m_Lessons ); ++i )
	{
		if ( !V_stricmp(pszGroup, m_Lessons[i]->GetGroup()) )
		{
			m_Lessons[ i ]->SetEnabled( bEnabled );
		}
	}
}

void CGameInstructor::FindErrors( void )
{
	// Loop through all the lesson and run all their scripted actions
	for ( int i = 0; i < std::ssize( m_Lessons ); ++i )
	{
		CScriptedIconLesson *pLesson = dynamic_cast<CScriptedIconLesson *>( m_Lessons[ i ] );
		if ( pLesson )
		{
			// Process all open events
			for ( int iLessonEvent = 0; iLessonEvent < std::ssize( pLesson->GetOpenEvents() ); ++iLessonEvent )
			{
				const LessonEvent_t *pLessonEvent = &(pLesson->GetOpenEvents()[ iLessonEvent ]);
				pLesson->ProcessElements( nullptr, &(pLessonEvent->elements) );
			}

			// Process all close events
			for ( int iLessonEvent = 0; iLessonEvent < std::ssize( pLesson->GetCloseEvents() ); ++iLessonEvent )
			{
				const LessonEvent_t *pLessonEvent = &(pLesson->GetCloseEvents()[ iLessonEvent ]);
				pLesson->ProcessElements( nullptr, &(pLessonEvent->elements) );
			}

			// Process all success events
			for ( int iLessonEvent = 0; iLessonEvent < std::ssize( pLesson->GetSuccessEvents() ); ++iLessonEvent )
			{
				const LessonEvent_t *pLessonEvent = &(pLesson->GetSuccessEvents()[ iLessonEvent ]);
				pLesson->ProcessElements( nullptr, &(pLessonEvent->elements) );
			}

			// Process all on open events
			for ( int iLessonEvent = 0; iLessonEvent < std::ssize( pLesson->GetOnOpenEvents() ); ++iLessonEvent )
			{
				const LessonEvent_t *pLessonEvent = &(pLesson->GetOnOpenEvents()[ iLessonEvent ]);
				pLesson->ProcessElements( nullptr, &(pLessonEvent->elements) );
			}

			// Process all update events
			for ( int iLessonEvent = 0; iLessonEvent < std::ssize( pLesson->GetUpdateEvents() ); ++iLessonEvent )
			{
				const LessonEvent_t *pLessonEvent = &(pLesson->GetUpdateEvents()[ iLessonEvent ]);
				pLesson->ProcessElements( nullptr, &(pLessonEvent->elements) );
			}
		}
	}
}

bool CGameInstructor::UpdateActiveLesson( CBaseLesson *pLesson, const CBaseLesson *pRootLesson )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( m_nSplitScreenSlot );

	CVProfScope _( "CGameInstructor::UpdateActiveLesson", 0, "GameInstructor", false, BUDGETFLAG_OTHER );

	bool bIsOpen = pLesson->IsInstructing();

	if ( !bIsOpen && !pRootLesson->IsLearned() )
	{
		pLesson->SetStartTime();
		pLesson->Start();

		// Check to see if it successfully started
		bIsOpen = ( pLesson->IsOpenOpportunity() && pLesson->ShouldDisplay() );

		if ( bIsOpen )
		{
			// Lesson hasn't been started and hasn't been learned
			if ( gameinstructor_verbose.GetInt() > 0 )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Started lesson " );
				ConColorMsg( CBaseLesson::m_rgbaVerboseOpen, "\"%s\"", pLesson->GetName() );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ".\n" );
			}
		}
		else
		{
			pLesson->Stop();
			pLesson->ResetStartTime();
		}
	}

	if ( bIsOpen )
	{
		// Update the running lesson
		pLesson->Update();
		return true;
	}
	else
	{
		pLesson->UpdateInactive();
		return false;
	}
}

void CGameInstructor::UpdateInactiveLesson( CBaseLesson *pLesson )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( m_nSplitScreenSlot );

	CVProfScope _( "CGameInstructor::UpdateInactiveLesson", 0, "GameInstructor", false, BUDGETFLAG_OTHER );

	if ( pLesson->IsInstructing() )
	{
		// Lesson hasn't been stopped
		if ( gameinstructor_verbose.GetInt() > 0 )
		{
			ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
			ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Stopped lesson " );
			ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\"%s\"", pLesson->GetName() );
			ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ".\n" );
		}

		pLesson->Stop();
		pLesson->ResetStartTime();
	}

	pLesson->UpdateInactive();
}

CBaseLesson * CGameInstructor::GetLesson_Internal( const char *pchLessonName )
{
	for ( int i = 0; i < std::ssize( m_Lessons ); ++i )
	{
		CBaseLesson *pLesson = m_Lessons[ i ];

		if ( V_strcmp( pLesson->GetName(), pchLessonName ) == 0 )
		{
			return pLesson;
		}
	}

	return nullptr;
}

void CGameInstructor::StopAllLessons( void )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( m_nSplitScreenSlot );
	// Stop all the current lessons
	for ( int i = std::ssize( m_OpenOpportunities ) - 1; i >= 0; --i )
	{
		CBaseLesson *pLesson = m_OpenOpportunities[ i ];
		UpdateInactiveLesson( pLesson );
	}
}

void CGameInstructor::CloseAllOpenOpportunities( void )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( m_nSplitScreenSlot );
	// Clear out all the open opportunities
	for ( int i = std::ssize( m_OpenOpportunities ) - 1; i >= 0; --i )
	{
		CBaseLesson *pLesson = m_OpenOpportunities[ i ];
		CloseOpportunity( pLesson );
	}

	contract_assert( std::ssize( m_OpenOpportunities ) == 0 );
}

void CGameInstructor::CloseOpportunity( CBaseLesson *pLesson )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( m_nSplitScreenSlot );
	UpdateInactiveLesson( pLesson );

	if ( pLesson->WasDisplayed() )
	{
		MarkDisplayed( pLesson->GetName() );
	}

	if ( gameinstructor_verbose.GetInt() > 0 )
	{
		ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
		ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Opportunity " );
		ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\"%s\" ", pLesson->GetName() );
		ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "closed for reason: " );
		ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "%s\n", pLesson->GetCloseReason() );
	}

	pLesson->StopListeningForAllEvents();

	ks::erase_first( m_OpenOpportunities, pLesson );
	delete pLesson;
}

void CGameInstructor::ReadLessonsFromFile( const char *pchFileName )
{
	// Static init function
	CScriptedIconLesson::PreReadLessonsFromFile();

	ACTIVE_SPLITSCREEN_PLAYER_GUARD( m_nSplitScreenSlot );
	KeyValues *pLessonKeys = new KeyValues( "instructor_lessons" );
	KeyValues::AutoDelete autoDelete(pLessonKeys);
	pLessonKeys->LoadFromFile( g_pFullFileSystem, pchFileName, nullptr );

	for ( m_pScriptKeys = pLessonKeys->GetFirstTrueSubKey(); m_pScriptKeys; m_pScriptKeys = m_pScriptKeys->GetNextTrueSubKey() )
	{
		if ( V_stricmp( m_pScriptKeys->GetName(), "GroupConVarToggle" ) == 0 )
		{
			// Add convar group toggler to the list
			int nLessonGroupConVarToggle = std::ssize( m_LessonGroupConVarToggles ); m_LessonGroupConVarToggles.push_back( LessonGroupConVarToggle_t( m_pScriptKeys->GetString( "convar" ) ) );
			LessonGroupConVarToggle_t *pLessonGroupConVarToggle = &(m_LessonGroupConVarToggles[ nLessonGroupConVarToggle ]);
			V_strcpy_safe( pLessonGroupConVarToggle->szLessonGroupName, m_pScriptKeys->GetString( "group" ) );

			continue;
		}

		// Ensure that lessons aren't added twice
		if ( GetLesson_Internal( m_pScriptKeys->GetName() ) )
		{
			DevWarning( "Lesson \"%s\" defined twice!\n", m_pScriptKeys->GetName() );
			continue;
		}

		CScriptedIconLesson *pNewLesson = new CScriptedIconLesson( m_pScriptKeys->GetName(), false, false, m_nSplitScreenSlot );
		GetGameInstructor().DefineLesson( pNewLesson );
	}

	m_pScriptKeys = nullptr;
}

void CGameInstructor::InitLessonPrerequisites( void )
{
	for ( int i = 0; i < std::ssize( m_Lessons ); ++i )
	{
		m_Lessons[ i ]->InitPrerequisites();
	}
}

//====================================================================================================
// CLIENTSIDE GAME INSTRUCTOR SAVE/RESTORE 
//====================================================================================================
static short GAMEINSTRUCTOR_SAVE_RESTORE_VERSION = 1;

static void gameinstructor_reload_lessons( const CCommand &args );
static ConCommand gameinstructor_reload_lessons_command( side_registry(), "gameinstructor_reload_lessons", gameinstructor_reload_lessons, "Shuts down all open lessons and reloads them from the script file.", FCVAR_CHEAT );
static void gameinstructor_reload_lessons( const CCommand &args )
{
	for ( int i = 0 ; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( i );
		GetGameInstructor().Shutdown();
		GetGameInstructor().Init();
	}
}

static void gameinstructor_reset_counts( const CCommand &args );
static ConCommand gameinstructor_reset_counts_command( side_registry(), "gameinstructor_reset_counts", gameinstructor_reset_counts, "Resets all display and success counts to zero.", FCVAR_NONE );
static void gameinstructor_reset_counts( const CCommand &args )
{
	for ( int i = 0 ; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( i );
		GetGameInstructor().ResetDisplaysAndSuccesses();
	}
}

static void gameinstructor_dump_open_lessons( const CCommand &args );
static ConCommand gameinstructor_dump_open_lessons_command( side_registry(), "gameinstructor_dump_open_lessons", gameinstructor_dump_open_lessons, "Gives a list of all currently open lessons.", FCVAR_CHEAT );
static void gameinstructor_dump_open_lessons( const CCommand &args )
{
	for ( int i = 0 ; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( i );
		GetGameInstructor().DumpOpenOpportunities();
	}
}

CGameInstructor::CGameInstructor() : CAutoGameSystemPerFrame( "               " )
{
	m_nSplitScreenSlot = -1;

	m_bHasLoadedSaveData = false;
	m_bEnsureThatInitIsNotCalledMultipleTimes = false;
}

#undef MOD_DIR

class CAmmoDef;
class CBaseCombatCharacter;
class CBaseCombatWeapon;
class CBaseEntity;

using namespace ::ks::reflect;

ConVar g_Language( side_registry(), "g_Language", "0", FCVAR_REPLICATED );
ConVar sk_autoaim_mode( side_registry(), "sk_autoaim_mode", "1", FCVAR_ARCHIVE | FCVAR_REPLICATED );

static CViewVectors g_DefaultViewVectors(
	Vector( 0, 0, 64 ),			//VEC_VIEW (m_vView)
								
	Vector(-16, -16, 0 ),		//VEC_HULL_MIN (m_vHullMin)
	Vector( 16,  16,  72 ),		//VEC_HULL_MAX (m_vHullMax)
													
	Vector(-16, -16, 0 ),		//VEC_DUCK_HULL_MIN (m_vDuckHullMin)
	Vector( 16,  16,  36 ),		//VEC_DUCK_HULL_MAX	(m_vDuckHullMax)
	Vector( 0, 0, 28 ),			//VEC_DUCK_VIEW		(m_vDuckView)
													
	Vector(-10, -10, -10 ),		//VEC_OBS_HULL_MIN	(m_vObsHullMin)
	Vector( 10,  10,  10 ),		//VEC_OBS_HULL_MAX	(m_vObsHullMax)
													
	Vector( 0, 0, 14 )			//VEC_DEAD_VIEWHEIGHT (m_vDeadViewHeight)
);													
	
// ------------------------------------------------------------------------------------ //
// CGameRulesProxy implementation.
// ------------------------------------------------------------------------------------ //

CGameRulesProxy *CGameRulesProxy::s_pGameRulesProxy = nullptr;

IMPLEMENT_NETWORKCLASS_ALIASED( GameRulesProxy, DT_GameRulesProxy )

// Don't send any of the CBaseEntity stuff..
IMPLEMENT_REFLECT_TABLE( CGameRulesProxy, DT_GameRulesProxy );

CGameRulesProxy::CGameRulesProxy()
{
	// allow map placed proxy entities to overwrite the static one
	if ( s_pGameRulesProxy )
	{
		s_pGameRulesProxy = nullptr;
	}
	s_pGameRulesProxy = this;
}

CGameRulesProxy::~CGameRulesProxy()
{
	if ( s_pGameRulesProxy == this )
	{
		s_pGameRulesProxy = nullptr;
	}
}

int CGameRulesProxy::UpdateTransmitState()
{
	return 0;

}

void CGameRulesProxy::NotifyNetworkStateChanged()
{
	if ( s_pGameRulesProxy )
		s_pGameRulesProxy->NetworkStateChanged();
}

ConVar	old_radius_damage( side_registry(), "old_radiusdamage", "0.0", FCVAR_REPLICATED );

bool CGameRules::IsBonusChallengeTimeBased( void )
{
	return true;
}

CGameRules::CGameRules() : CAutoGameSystemPerFrame( "CGameRules" )
{
	contract_assert( !g_pGameRules );
	g_pGameRules = this;
}	

// ----------------------------------------------------------------------------- //
// Shared CGameRules implementation.
// ----------------------------------------------------------------------------- //

CGameRules::~CGameRules()
{
	contract_assert( g_pGameRules == this );
	g_pGameRules = nullptr;
}

bool CGameRules::Init()
{

	return true;
}

bool CGameRules::SwitchToNextBestWeapon( CBaseCombatCharacter *pPlayer, CBaseCombatWeapon *pCurrentWeapon )
{
	return false;
}

CBaseCombatWeapon *CGameRules::GetNextBestWeapon( CBaseCombatCharacter *pPlayer, CBaseCombatWeapon *pCurrentWeapon )
{
	return nullptr;
}

bool CGameRules::ShouldCollide( int collisionGroup0, int collisionGroup1 )
{
	if ( collisionGroup0 > collisionGroup1 )
	{
		// swap so that lowest is always first
		V_swap(collisionGroup0,collisionGroup1);
	}

	if ( (collisionGroup0 == COLLISION_GROUP_PLAYER || collisionGroup0 == COLLISION_GROUP_PLAYER_MOVEMENT) &&
		collisionGroup1 == COLLISION_GROUP_PUSHAWAY )
	{
		return false;
	}

	if ( collisionGroup0 == COLLISION_GROUP_DEBRIS && collisionGroup1 == COLLISION_GROUP_PUSHAWAY )
	{
		// let debris and multiplayer objects collide
		return true;
	}

	// Only let projectile blocking debris collide with projectiles
	if ( collisionGroup0 == COLLISION_GROUP_PROJECTILE && collisionGroup1 == COLLISION_GROUP_DEBRIS_BLOCK_PROJECTILE )
		return true;

	if ( collisionGroup0 == COLLISION_GROUP_DEBRIS_BLOCK_PROJECTILE || collisionGroup1 == COLLISION_GROUP_DEBRIS_BLOCK_PROJECTILE )
		return false;
	
	// --------------------------------------------------------------------------
	// NOTE: All of this code assumes the collision groups have been sorted!!!!
	// NOTE: Don't change their order without rewriting this code !!!
	// --------------------------------------------------------------------------

	// Don't bother if either is in a vehicle...
	if (( collisionGroup0 == COLLISION_GROUP_IN_VEHICLE ) || ( collisionGroup1 == COLLISION_GROUP_IN_VEHICLE ))
		return false;

	if ( ( collisionGroup1 == COLLISION_GROUP_DOOR_BLOCKER ) && ( collisionGroup0 != COLLISION_GROUP_NPC ) )
		return false;

	if ( ( collisionGroup0 == COLLISION_GROUP_PLAYER ) && ( collisionGroup1 == COLLISION_GROUP_PASSABLE_DOOR ) )
		return false;

	if ( collisionGroup0 == COLLISION_GROUP_DEBRIS || collisionGroup0 == COLLISION_GROUP_DEBRIS_TRIGGER )
	{
		// put exceptions here, right now this will only collide with COLLISION_GROUP_NONE
		return false;
	}

	// Dissolving guys only collide with COLLISION_GROUP_NONE
	if ( (collisionGroup0 == COLLISION_GROUP_DISSOLVING) || (collisionGroup1 == COLLISION_GROUP_DISSOLVING) )
	{
		if ( collisionGroup0 != COLLISION_GROUP_NONE )
			return false;
	}

	// doesn't collide with other members of this group
	// or debris, but that's handled above
	if ( collisionGroup0 == COLLISION_GROUP_INTERACTIVE_DEBRIS && collisionGroup1 == COLLISION_GROUP_INTERACTIVE_DEBRIS )
		return false;

	// This change was breaking HL2DM
	// Adrian: TEST! Interactive Debris doesn't collide with the player.
	if ( collisionGroup0 == COLLISION_GROUP_INTERACTIVE_DEBRIS && ( collisionGroup1 == COLLISION_GROUP_PLAYER_MOVEMENT || collisionGroup1 == COLLISION_GROUP_PLAYER ) )
		 return false;

	if ( collisionGroup0 == COLLISION_GROUP_BREAKABLE_GLASS && collisionGroup1 == COLLISION_GROUP_BREAKABLE_GLASS )
		return false;

	// interactive objects collide with everything except debris & interactive debris
	if ( collisionGroup1 == COLLISION_GROUP_INTERACTIVE && collisionGroup0 != COLLISION_GROUP_NONE )
		return false;

	// Projectiles hit everything but debris, weapons, + other projectiles
	if ( collisionGroup1 == COLLISION_GROUP_PROJECTILE )
	{
		if ( collisionGroup0 == COLLISION_GROUP_DEBRIS || 
			collisionGroup0 == COLLISION_GROUP_WEAPON ||
			collisionGroup0 == COLLISION_GROUP_PROJECTILE )
		{
			return false;
		}
	}

	// Don't let vehicles collide with weapons
	// Don't let players collide with weapons...
	// Don't let NPCs collide with weapons
	// Weapons are triggers, too, so they should still touch because of that
	if ( collisionGroup1 == COLLISION_GROUP_WEAPON )
	{
		if ( collisionGroup0 == COLLISION_GROUP_VEHICLE || 
			collisionGroup0 == COLLISION_GROUP_PLAYER ||
			collisionGroup0 == COLLISION_GROUP_NPC || 
			collisionGroup0 == COLLISION_GROUP_WEAPON ) //don't let weapons collide with weapons, they can pile up and choke the server
		{
			return false;
		}
	}

	// collision with vehicle clip entity??
	if ( collisionGroup0 == COLLISION_GROUP_VEHICLE_CLIP || collisionGroup1 == COLLISION_GROUP_VEHICLE_CLIP )
	{
		// yes then if it's a vehicle, collide, otherwise no collision
		// vehicle sorts lower than vehicle clip, so must be in 0
		if ( collisionGroup0 == COLLISION_GROUP_VEHICLE )
			return true;
		// vehicle clip against non-vehicle, no collision
		return false;
	}

	return true;
}

const CViewVectors* CGameRules::GetViewVectors() const
{
	return &g_DefaultViewVectors;
}

//-----------------------------------------------------------------------------
// Purpose: Returns how much damage the given ammo type should do to the victim
//			when fired by the attacker.
// Input  : pAttacker - Dude what shot the gun.
//			pVictim - Dude what done got shot.
//			nAmmoType - What been shot out.
// Output : How much hurt to put on dude what done got shot (pVictim).
//-----------------------------------------------------------------------------
float CGameRules::GetAmmoDamage( CBaseEntity *pAttacker, CBaseEntity *pVictim, int nAmmoType )
{
	float flDamage = 0;
	CAmmoDef *pAmmoDef = GetAmmoDef();

	if ( pAttacker->IsPlayer() )
	{
		flDamage = pAmmoDef->PlrDamage( nAmmoType );
	}
	else
	{
		flDamage = pAmmoDef->NPCDamage( nAmmoType );
	}

	return flDamage;
}

using namespace ::ks::reflect;

// ------------------------------------------------------------------------------------------ //
// CGameRulesRegister implementation.
// ------------------------------------------------------------------------------------------ //

CGameRulesRegister* CGameRulesRegister::s_pHead = nullptr;

CGameRulesRegister::CGameRulesRegister( const char *pClassName, CreateGameRulesFn fn )
{
	m_pClassName = pClassName;
	m_pFn = fn;
	
	m_pNext = s_pHead;	// Add us to the global list.
	s_pHead = this;
}

void CGameRulesRegister::CreateGameRules()
{
	m_pFn();
}

CGameRulesRegister* CGameRulesRegister::FindByName( const char *pName )
{
	for ( CGameRulesRegister *pCur=s_pHead; pCur; pCur=pCur->m_pNext )
	{
		if ( V_stricmp( pName, pCur->m_pClassName ) == 0 )
			return pCur;
	}
	return nullptr;
}

// ------------------------------------------------------------------------------------------ //
// Functions to dispatch the messages to create the game rules object on the client.
// ------------------------------------------------------------------------------------------ //

inline constexpr const char *GAMERULES_STRINGTABLE_NAME = "GameRulesCreation";

	INetworkStringTable *g_StringTableGameRules = nullptr;

	void OnGameRulesCreationStringChanged( 
		void *object, 
		INetworkStringTable *stringTable, 
		int stringNumber, 
		const char *newString, 
		void const *newData )
	{
		// The server has created a new CGameRules object.
		delete g_pGameRules;
		g_pGameRules = nullptr;

		const char *pClassName = (const char*)newData;
		CGameRulesRegister *pReg = CGameRulesRegister::FindByName( pClassName );
		if ( !pReg )
			Error( "OnGameRulesCreationStringChanged: missing gamerules class '%s' on the client", pClassName );

		// Create the new game rules object.
		pReg->CreateGameRules();

		if ( !g_pGameRules )
		{
			Error( "OnGameRulesCreationStringChanged: game rules entity (%s) not created", pClassName );
		}
	}

	// On the client, we respond to string table changes on the server.
	void InstallStringTableCallback_GameRules( const char *tableName )
	{
		if ( 0 == V_strcasecmp( tableName, GAMERULES_STRINGTABLE_NAME ) )
		{
			g_StringTableGameRules = networkstringtable->FindTable( tableName );
			if ( g_StringTableGameRules )
				g_StringTableGameRules->SetStringChangedCallback( nullptr, OnGameRulesCreationStringChanged );
		}
	}

class IGameSystem;

//-----------------------------------------------------------------------------
// Purpose: The actual storage for pooled per-level strings
//-----------------------------------------------------------------------------
class CGameStringPool : public CBaseGameSystem
{
	virtual char const *Name() { return "CGameStringPool"; }

	virtual void LevelShutdownPostEntity() 
	{
		FreeAll();
		PurgeDeferredDeleteList();
		CGameString::IncrementSerialNumber();
	}

public:
	// Case-insensitive; keeps the first spelling. string_t holds the returned pointer, which a node set keeps stable.
	const char *Allocate( const char *pszValue )
	{
		auto it = m_Strings.find( std::string_view( pszValue ) );
		if ( it == m_Strings.end() )
			it = m_Strings.emplace( pszValue ).first;
		return it->c_str();
	}

	const char *Find( const char *pszValue )
	{
		if ( !pszValue )
			return nullptr;
		auto it = m_Strings.find( std::string_view( pszValue ) );
		return it == m_Strings.end() ? nullptr : it->c_str();
	}

	void FreeAll()
	{
		m_Strings.clear();
	}

	void PurgeDeferredDeleteList()
	{
		m_DeferredDeleteList.clear(); m_DeferredDeleteList.shrink_to_fit();
	}

	void Dump( void )
	{
		std::vector< const char * > sorted;
		for ( const std::string &s : m_Strings )
			sorted.push_back( s.c_str() );
		std::sort( sorted.begin(), sorted.end(), ks::cstr_iless() );
		for ( int i = 0; i < std::ssize( sorted ); i++ )
		{
			DevMsg( "  %d (0x%p) : %s\n", i, sorted[i], sorted[i] );
		}
		DevMsg( "\n" );
		DevMsg( "Size:  %d items\n", (int)m_Strings.size() );
	}

	// Existing string_t's may still point at the string, so it stays allocated until level shutdown.
	void Remove( const char *pszValue )
	{
		if ( !pszValue )
			return;
		auto it = m_Strings.find( std::string_view( pszValue ) );
		if ( it != m_Strings.end() )
		{
			m_DeferredDeleteList.push_back( m_Strings.extract( it ) );
		}
	}

private:
	using StringSet_t = std::unordered_set< std::string, ks::ihash, ks::iequal >;
	StringSet_t m_Strings;
	std::vector< StringSet_t::node_type > m_DeferredDeleteList;
};

static CGameStringPool g_GameStringPool;

//-----------------------------------------------------------------------------
// String system accessor
//-----------------------------------------------------------------------------
IGameSystem *GameStringSystem()
{
	return &g_GameStringPool;
}

//-----------------------------------------------------------------------------
// Purpose: The public accessor for the level-global pooled strings
//-----------------------------------------------------------------------------
string_t AllocPooledString( const char * pszValue )
{
	if (pszValue && *pszValue)
		return MAKE_STRING( g_GameStringPool.Allocate( pszValue ) );
	return NULL_STRING;
}

string_t FindPooledString( const char *pszValue )
{
	return MAKE_STRING( g_GameStringPool.Find( pszValue ) );
}

void RemovePooledString( const char *pszValue )
{
	g_GameStringPool.Remove( pszValue );
}

int CGameString::gm_iSerialNumber = 1;

CGameString::~CGameString()
{
	if ( m_bCopy ) 
		std::free( (void *)m_pszString ); 
}

void CGameString::Set( const char *pszString, bool bCopy)
{
	if ( m_bCopy && m_pszString )
		std::free( (void *)m_pszString );
	m_iszString = NULL_STRING;
	m_pszString = ( !bCopy ) ? pszString : __builtin_strdup( pszString );
}

class CBaseEntity;

using namespace ::ks::reflect;

bool TraceDidHitWorld( const CGameTrace &tr )
{
	return tr.Ent<CBaseEntity>() == ClientEntityList().GetBaseEntity( 0 );
}

bool TraceDidHitNonWorldEntity( const CGameTrace &tr )
{
	return tr.m_pEnt != nullptr && !TraceDidHitWorld( tr );
}

int TraceGetEntityIndex( const CGameTrace &tr )
{
	if ( tr.m_pEnt )
		return tr.Ent<CBaseEntity>()->entindex();
	else
		return -1;
}

// some shared cvars used by game rules
ConVar mp_forcecamera( 
	side_registry(), "mp_forcecamera", 
	"1", 
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Restricts spectator modes for dead players. 0 = Any team. 1 = Only own team. 2 = No one; fade to black on death (previously mp_fadetoblack)."
	);

ConVar mp_radar_showall(
	side_registry(), "mp_radar_showall",
	"0",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Determines who should see all. 0 = default. 1 = both teams. 2 = Terrorists. 3 = Counter-Terrorists.",
	true, 0, //min
	true, 3	 //max
	);
	
ConVar mp_allowspectators(
	side_registry(), "mp_allowspectators", 
	"1.0", 
	FCVAR_REPLICATED,
	"toggles whether the server allows spectator mode or not" );

ConVar mp_friendlyfire(
	side_registry(), "mp_friendlyfire",
	"0",
	FCVAR_REPLICATED | FCVAR_NOTIFY | FCVAR_RELEASE,
	"Allows team members to injure other members of their team"
	);

ConVar mp_teammates_are_enemies(
	side_registry(), "mp_teammates_are_enemies",
	"0",
	FCVAR_REPLICATED | FCVAR_NOTIFY | FCVAR_RELEASE,
	"When set, your teammates act as enemies and all players are valid targets."
	);

ConVar mp_buy_anywhere(
	side_registry(), "mp_buy_anywhere",
	"0",
	FCVAR_REPLICATED | FCVAR_NOTIFY | FCVAR_RELEASE,
	"When set, players can buy anywhere, not only in buyzones. 0 = default. 1 = both teams. 2 = Terrorists. 3 = Counter-Terrorists."
	);

ConVar mp_buy_during_immunity(
	side_registry(), "mp_buy_during_immunity",
	"0",
	FCVAR_REPLICATED | FCVAR_NOTIFY | FCVAR_RELEASE,
	"When set, players can buy when immune, ignoring buytime. 0 = default. 1 = both teams. 2 = Terrorists. 3 = Counter-Terrorists."
	);
