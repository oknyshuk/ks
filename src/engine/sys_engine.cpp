// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.core;
#include "quakedef.h"
#include "engine_launcher_api.h"
#include "iengine.h"
#include "ivideomode.h"
#include "igame.h"
#include "host.h"
#include "keys.h"
#include "cdll_int.h"
#include "host_state.h"
#include "cdll_engine_int.h"
#include "sys_dll.h"
import std;
#include "gl_matsysiface.h"
#include "vprof_engine.h"
#include "server.h"
#include "cl_demo.h"
#include "toolframework/itoolframework.h"
#include "toolframework/itoolsystem.h"
#include "inputsystem/iinputsystem.h"
#include "gl_cvars.h"
#include "filesystem_engine.h"
#ifndef DEDICATED
#include "engineui.h"
#endif



//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
void Sys_ShutdownGame( void );
int Sys_InitGame( CreateInterfaceFn appSystemFactory,
			char const* pBaseDir, void *pwnd, int bIsDedicated );

// sleep time when not focus
ConVar engine_no_focus_sleep( "engine_no_focus_sleep", "50", FCVAR_ARCHIVE );


// Dedicated server fps locking to tickrate values
extern float host_nexttick;

//-----------------------------------------------------------------------------
// ConVars and ConCommands
//-----------------------------------------------------------------------------
// The client-side frame rate limiter is gone (fps_max, fps_max_menu,
// fps_max_splitscreen, mat_powersavingsmode). Frame pacing is owned by vsync plus
// dxvk.latencySleep: dxvk limits to the display refresh rate and starts the CPU
// frame just late enough that work lands before vblank. An engine-side sleep is
// counted as CPU work by dxvk's estimator and makes it pace worse, and upstream
// dxvk documents that latency sleep has no effect at all in games that limit
// their own frame rate.
//
// sleep_when_meeting_framerate still applies to dedicated servers, which pace
// themselves against host_nexttick in FilterTime().
ConVar sleep_when_meeting_framerate( "sleep_when_meeting_framerate", "1", FCVAR_NONE, "Sleep instead of spinning when meeting the dedicated server tick rate." );

static ConVar async_serialize( "async_serialize", "0", 0, "Force async reads to serialize for profiling" );
[[gnu::always_inline]] inline auto ShouldSerializeAsync() { return async_serialize.GetBool(); }

static ConVar vx_do_not_throttle_events( "vx_do_not_throttle_events", "0", 0, "Force VXConsole updates every frame; smoother vprof data on PS3 but at a slight (~0.2ms) perf cost." );


float		host_filtered_time_history[128] = { 0 };
unsigned int host_filtered_time_history_pos = 0;
static void host_filtered_time_report( const CCommand &args );
static ConCommand host_filtered_time_report_command( "host_filtered_time_report", host_filtered_time_report, "Dumps time spent idle in previous frames in ms(dedicated only)." );
static void host_filtered_time_report( const CCommand &args )
{
	if ( sv.IsDedicated() )
	{
		for (int i = 1; i <= std::size( host_filtered_time_history ); ++i)
		{
			unsigned int slot = ( i + host_filtered_time_history_pos) % std::size( host_filtered_time_history );
			Msg( "%.4f\n", ( host_filtered_time_history[ slot ] * 1000 ) );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class CEngine : public IEngine
{
public:
					CEngine( void );
	virtual			~CEngine( void );

	bool			Load( bool dedicated, const char *basedir );

	virtual void	Unload( void );
	virtual EngineState_t GetState( void );
	virtual void	SetNextState( EngineState_t iNextState );

	void			Frame( void );

	float			GetFrameTime( void );
	float			GetCurTime( void );

	bool			TrapKey_Event( ButtonCode_t key, bool down );
	void			TrapMouse_Event( int buttons, bool down );

	void			StartTrapMode( void );
	bool			IsTrapping( void );
	bool			CheckDoneTrapping( ButtonCode_t& key );

	int				GetQuitting( void );
	void			SetQuitting( int quittype );

private:
	bool			FilterTime( float t );

	int				m_nQuitting;

	EngineState_t	m_nDLLState;
	EngineState_t	m_nNextDLLState;

	double			m_flCurrentTime;
	float			m_flFrameTime;
	double			m_flPreviousTime;
	float			m_flFilteredTime;
	float			m_flMinFrameTime; // Expected duration of a frame, or zero if it is unlimited.

};

static CEngine g_Engine;

IEngine *eng = ( IEngine * )&g_Engine;
//IEngineAPI *engine = NULL;


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CEngine::CEngine( void )
{
	m_nDLLState			= DLL_INACTIVE;
	m_nNextDLLState		= DLL_INACTIVE;

	m_flCurrentTime		= 0.0;
	m_flFrameTime		= 0.0f;
	m_flPreviousTime	= 0.0;
	m_flFilteredTime	= 0.0f;
	m_flMinFrameTime	= 0.0f;

	m_nQuitting			= QUIT_NOTQUITTING;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CEngine::~CEngine( void )
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CEngine::Unload( void )
{
	Sys_ShutdownGame();

	m_nDLLState			= DLL_INACTIVE;
	m_nNextDLLState		= DLL_INACTIVE;
}

//-----------------------------------------------------------------------------
// Purpose:
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CEngine::Load( bool dedicated, const char *rootdir )
{
	bool success = false;

	// Activate engine
	// NOTE: We must bypass the 'next state' block here for initialization to work properly.
	m_nDLLState = m_nNextDLLState = DLL_ACTIVE;

	if ( Sys_InitGame(
		g_AppSystemFactory,
		rootdir,
		game->GetMainWindowAddress(),
		dedicated ) )
	{
		success = true;

		UpdateMaterialSystemConfig();
	}

	return success;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : dt -
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CEngine::FilterTime( float dt )
{
	// Dedicated servers will lock fps max to tick rate essentially
	if ( sv.IsDedicated() && !g_bDedicatedServerBenchmarkMode )
	{
		m_flMinFrameTime = host_nexttick;
		return ( dt >= host_nexttick );
	}

	// The client does not pace itself: dxvk owns frame pacing via dxvk.latencySleep
	// (see CShaderDeviceMgrDx8::Connect). It delays the start of the CPU frame so that
	// work lands just before vblank, and under vsync it limits to the display refresh
	// rate. An engine-side sleep here defeats that outright, because dxvk measures
	// frameStart->queueSubmit as CPU time: our idle sleep would be counted as work and
	// it would compensate by sleeping less. Upstream dxvk documents that latencySleep
	// "will not have any effect in games with built-in frame rate limiters", so the
	// client limiter is removed rather than merely bypassed.
	m_flMinFrameTime = 0.0f;
	return true;
}

extern void PS3_PollSaveSystem();
//-----------------------------------------------------------------------------
// Purpose:
// Output : int
//-----------------------------------------------------------------------------
void CEngine::Frame( void )
{
	// yield the CPU for a little while when paused, minimized, or not the focus
	// FIXME:  Move this to main windows message pump?
	// lwss - This convar is completely missing, commenting this out
	//static ConVarRef cl_embedded_stream_video_playing( "cl_embedded_stream_video_playing" );
	//if ( IsPC() && !game->IsActiveApp() && !sv.IsDedicated()
	//	&& !( cl_embedded_stream_video_playing.IsValid() && cl_embedded_stream_video_playing.GetBool() )
	//	&& engine_no_focus_sleep.GetInt() > 0 )
	//{
	//	g_pInputSystem->SleepUntilInput( engine_no_focus_sleep.GetInt() );
	//}
	// lwss end

	// Get current time
	m_flCurrentTime	= Plat_FloatTime();

	// Watch for data from the CPU frequency monitoring system and print it to the console.
	const CPUFrequencyResults frequency = GetCPUFrequencyResults();
	static double s_lastFrequencyTimestamp;
	if ( frequency.m_timeStamp > s_lastFrequencyTimestamp )
	{
		s_lastFrequencyTimestamp = frequency.m_timeStamp;
		Msg( "~CPU Freq: %1.3f GHz    Percent of requested: %3.1f%%    Minimum percent seen: %3.1f%%\n",
					frequency.m_GHz, frequency.m_percentage, frequency.m_lowestPercentage );
	}

	// Determine dt since we last checked
	float dt = m_flCurrentTime - m_flPreviousTime;
	if ( sv.IsDedicated() && ( dt < 0 ) )
	{
		// ... but if the clock ever went backwards due to a bug,
		// we'd have no idea how much time has elapsed, so just
		// catch up to the next scheduled server tick.
		dt = host_nexttick;
	}


	// Remember old time
	m_flPreviousTime = m_flCurrentTime;

	// Accumulate current time delta into the true "frametime"
	m_flFrameTime += dt;

	// If the time is < 0, that means we've restarted.
	// Set the new time high enough so the engine will run a frame
	if ( m_flFrameTime < 0.0f )
		return;

	// If the frametime is still too short, don't pass through.
	// Only dedicated servers gate here now; the client is paced by dxvk.
	if ( !FilterTime( m_flFrameTime ) )
	{
		double fSleepNS = ( m_flMinFrameTime - m_flFrameTime ) * 1000000000.0;
		unsigned nSleepNS = (unsigned)std::floor( fSleepNS );
		if ( nSleepNS && sleep_when_meeting_framerate.GetInt() )
		{
			ThreadNanoSleep( nSleepNS );
		}
		m_flFilteredTime += dt;
		return;
	}


	if ( ShouldSerializeAsync() )
	{
		static ConVar *pSyncReportConVar = g_pCVar->FindVar( "fs_report_sync_opens" );
		bool bReportingSyncOpens = ( pSyncReportConVar && pSyncReportConVar->GetInt() );
		int reportLevel = 0;
		if ( bReportingSyncOpens )
		{
			reportLevel = pSyncReportConVar->GetInt();
			pSyncReportConVar->SetValue( 0 );
		}
		g_pFileSystem->AsyncFinishAll();
		if ( bReportingSyncOpens )
		{
			pSyncReportConVar->SetValue( reportLevel );
		}
	}

	PreUpdateProfile( m_flFilteredTime );

	// Record previous swallowed time counts.
	host_filtered_time_history[ host_filtered_time_history_pos ] = m_flFilteredTime;
	host_filtered_time_history_pos = ( host_filtered_time_history_pos + 1 ) % std::size( host_filtered_time_history );
	// Reset swallowed time...
	m_flFilteredTime = 0.0f;

#ifndef DEDICATED
	if ( !sv.IsDedicated() )
	{
		ClientDLL_FrameStageNotify( FRAME_START );
	}
#endif

	PostUpdateProfile();

	ETWRenderFrameMark( sv.IsDedicated() );

	{ // profile scope

	CVProfScope _( "CEngine::Frame", 0, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, BUDGETFLAG_OTHER );

	switch( m_nDLLState )
	{
	case DLL_PAUSED:			// paused, in hammer
	case DLL_INACTIVE:			// no dll
		break;

	case DLL_ACTIVE:			// engine is focused
	case DLL_CLOSE:				// closing down dll
	case DLL_RESTART:			// engine is shutting down but will restart right away
		// Run the engine frame
		HostState_Frame( m_flFrameTime );
		break;
	}

	// Has the state changed?
	if ( m_nNextDLLState != m_nDLLState )
	{
		m_nDLLState = m_nNextDLLState;

		// Do special things if we change to particular states
		switch( m_nDLLState )
		{
		case DLL_CLOSE:
			SetQuitting( QUIT_TODESKTOP );
			break;
		case DLL_RESTART:
			SetQuitting( QUIT_RESTART );
			break;
		}
	}

	} // profile scope

	// Reset for next frame
	m_flFrameTime = 0.0f;

	// reload dlls that are marked for reload; currently for debug purposes only
}



//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CEngine::EngineState_t CEngine::GetState( void )
{
	return m_nDLLState;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CEngine::SetNextState( EngineState_t iNextState )
{
	m_nNextDLLState = iNextState;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
float CEngine::GetFrameTime( void )
{
	return m_flFrameTime;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
float CEngine::GetCurTime( void )
{
	return m_flCurrentTime;
}


//-----------------------------------------------------------------------------
// Purpose: Flag that we are in the process of quiting
//-----------------------------------------------------------------------------
void CEngine::SetQuitting( int quittype )
{
	m_nQuitting = quittype;
}


//-----------------------------------------------------------------------------
// Purpose: Check whether we are ready to exit
//-----------------------------------------------------------------------------
int CEngine::GetQuitting( void )
{
	return m_nQuitting;
}

