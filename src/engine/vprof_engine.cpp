// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: VProf engine integration

import std;
import ks.tier1;
#include "sys.h"
#include "vprof_engine.h"
#include "sv_main.h"
#include "iengine.h"
#include "cmd.h"
#include "con_nprint.h"
#include "materialsystem/imaterialsystem.h"
#ifndef DEDICATED
#include "engineui.h"
#endif
#include "sv_remoteaccess.h"
#include "ivprofexport.h"
#include "vprof_record.h"
#include "filesystem_engine.h"

void VProfExport_StartOrStop();

static ConVar vprof_dump_spikes( "vprof_dump_spikes","0", 0, "Framerate at which vprof will begin to dump spikes to the console. 0 = disabled, negative to reset after dump" );
static ConVar vprof_dump_spikes_terse( "vprof_dump_spikes_terse","0", 0, "Whether to use most terse output" );
static ConVar vprof_dump_spikes_hierarchy( "vprof_dump_spikes_hiearchy","0", 0, "Set to 1 to get a hierarchy report whith vprof_dump_spikes" );
static ConVar vprof_dump_spikes_node( "vprof_dump_spikes_node","", 0, "Node to start report from when doing a dump spikes" );
static ConVar vprof_dump_spikes_budget_group( "vprof_dump_spikes_budget_group","", 0, "Budget gtNode to start report from when doing a dump spikes" );
static ConVar vprof_dump_oninterval( "vprof_dump_oninterval", "0", 0, "Interval (in seconds) at which vprof will batch up data and dump it to the console." );

static void (*g_pfnDeferredOp)();

static void ExecuteDeferredOp() 
{
	if ( g_pfnDeferredOp )
	{
		(*g_pfnDeferredOp)();
		g_pfnDeferredOp = nullptr;
	}
}

unsigned g_VProfTargetThread = ThreadGetCurrentId();
	
const double MAX_SPIKE_REPORT = 1.0;
const int MAX_SPIKE_REPORT_FRAMES = 10;
static double LastSpikeTime = 0;
static int LastSpikeFrame = 0;
static ConVar vprof_counters( "vprof_counters", "0", 0 );
static ConVar vprof_counters_show_minmax( "vprof_counters_show_minmax", "0", 0 );

extern bool con_debuglog;
extern ConVar con_logfile;
static bool g_fVprofOnByUI;
static bool g_bVProfNoVSyncOff = false;

class ConsoleLogger
{
public:
	ConsoleLogger( void )
	{
#if !defined( DEDICATED )
		m_condebugEnabled = con_debuglog;
#else
		m_condebugEnabled = false;
#endif
		if ( !m_condebugEnabled )
		{
			g_pFileSystem->CreateDirHierarchy( "vprof" );
			while ( 1 )
			{
				++m_index;
				const char *fname = va( "vprof/vprof%d.txt", m_index );
				if ( g_pFileSystem->FileExists( fname ) )
				{
					continue;
				}

#if !defined( DEDICATED )
				con_logfile.SetValue( fname );
#endif
				break;
			}
		}
	}

	~ConsoleLogger()
	{
		if ( !m_condebugEnabled )
		{
#if !defined( DEDICATED )
			con_logfile.SetValue( "" );
#endif
		}
	}

private:
	static int m_index;
	bool m_condebugEnabled;
};

int ConsoleLogger::m_index = 0;

static float s_flIntervalStartTime = 0.0f;
static bool g_bDumpCounters = false;

static void vprof_dump_counters( const CCommand &args );
static ConCommand vprof_dump_counters_command( "vprof_dump_counters", vprof_dump_counters, "Dump vprof counters to the console" );
static void vprof_dump_counters( const CCommand &args )
{
	g_bDumpCounters = true;
}

void PreUpdateProfile( float filteredtime )
{
	contract_assert( g_VProfCurrentProfile.AtRoot() );

	ExecuteDeferredOp();
	VProfExport_StartOrStop();
	VProfRecord_StartOrStop();

	if ( g_VProfCurrentProfile.GetTargetThreadId() != g_VProfTargetThread )
	{
		g_VProfCurrentProfile.SetTargetThreadId( g_VProfTargetThread );
	}

	// Check to see if it is time to dump the data and restart collection.
	if ( g_VProfCurrentProfile.IsEnabled() && ( vprof_dump_oninterval.GetFloat() != 0.0f ) )
	{
		float flCurrentTime = eng->GetCurTime();
		float flIntervalTime = vprof_dump_oninterval.GetFloat();

		g_VProfCurrentProfile.MarkFrame();

		if ( ( s_flIntervalStartTime + flIntervalTime ) < flCurrentTime )
		{
			// Dump the current profile.
			g_VProfCurrentProfile.OutputReport( VPRT_SUMMARY | VPRT_LIST_BY_TIME | VPRT_LIST_BY_AVG_TIME | VPRT_LIST_BY_TIME_LESS_CHILDREN | VPRT_LIST_TOP_ITEMS_ONLY );

			// Stop the current profile.
			g_VProfCurrentProfile.Stop();

			// Reset and restart the current profile.
			g_VProfCurrentProfile.Reset();
			g_VProfCurrentProfile.Start();
			s_flIntervalStartTime = flCurrentTime;
		}
	}

	if( g_VProfCurrentProfile.IsEnabled() && vprof_dump_spikes.GetFloat() )
	{
		float spikeThreash = std::fabsf( vprof_dump_spikes.GetFloat() );
		g_VProfCurrentProfile.MarkFrame();
		bool bSuppressRestart = false;
		if ( g_VProfSignalSpike || eng->GetFrameTime() > ( 1.f / spikeThreash ) )
		{
			if( g_VProfSignalSpike || ( Plat_FloatTime() - LastSpikeTime > MAX_SPIKE_REPORT && g_ServerGlobalVariables.framecount > LastSpikeFrame + MAX_SPIKE_REPORT_FRAMES ) )
			{
				ConsoleLogger consoleLog;
				Msg( "******** Spike on frame %d at time %.3f ", g_ServerGlobalVariables.framecount, Plat_FloatTime() );
				if ( vprof_dump_spikes_hierarchy.GetBool() )
				{
					g_VProfCurrentProfile.OutputReport( VPRT_HIERARCHY_TIME_PER_FRAME_AND_COUNT_ONLY,
						( vprof_dump_spikes_node.GetString()[0] ) ? vprof_dump_spikes_node.GetString() : nullptr,
						( vprof_dump_spikes_budget_group.GetString()[0] ) ? g_VProfCurrentProfile.BudgetGroupNameToBudgetGroupID( vprof_dump_spikes_budget_group.GetString() ) : -1 );
				}
				else
				{
					int flags;
					if ( !vprof_dump_spikes_terse.GetBool() )
					{
						flags = VPRT_SUMMARY | VPRT_LIST_BY_TIME | VPRT_LIST_BY_AVG_TIME | VPRT_LIST_BY_TIME_LESS_CHILDREN | VPRT_LIST_TOP_ITEMS_ONLY;
					}
					else
					{
						flags = VPRT_LIST_BY_TIME | VPRT_LIST_TOP_ITEMS_ONLY;
					}
					g_VProfCurrentProfile.OutputReport( flags,
						( vprof_dump_spikes_node.GetString()[0] ) ? vprof_dump_spikes_node.GetString() : nullptr,
						( vprof_dump_spikes_budget_group.GetString()[0] ) ? g_VProfCurrentProfile.BudgetGroupNameToBudgetGroupID( vprof_dump_spikes_budget_group.GetString() ) : -1 );
				}

				LastSpikeTime = Plat_FloatTime();
				LastSpikeFrame = g_ServerGlobalVariables.framecount;

				if ( vprof_dump_spikes.GetFloat() < 0.0 )
				{
					vprof_dump_spikes.SetValue( 0.0f );
					// g_VProfCurrentProfile.Stop();
					g_fVprofOnByUI = false;
					bSuppressRestart = true;
				}
			}
			g_VProfSignalSpike = false;
		}

		int iStartDepth = 0;
		do 
		{
			g_VProfCurrentProfile.Stop();
			iStartDepth++;
		} while( g_VProfCurrentProfile.IsEnabled() );

		if (!bSuppressRestart)
		{

			g_VProfCurrentProfile.Reset();

			while ( iStartDepth-- )
			{
				g_VProfCurrentProfile.Start();
			}
		}

		contract_assert( g_VProfCurrentProfile.AtRoot() );
		contract_assert( g_VProfCurrentProfile.IsEnabled() );
	}

	int nCounterType = vprof_counters.GetInt();
	if ( nCounterType || g_bDumpCounters )
	{
		int i;
		int n = g_VProfCurrentProfile.GetNumCounters();
		int nprintIndex = 0;
		int static nCycle = 0;
		for ( i = 0; i < n; i++ )
		{
			if ( g_VProfCurrentProfile.GetCounterGroup( i ) != ( nCounterType - 1 ) )
				continue;
			const char *pName;
			int val;
			pName = g_VProfCurrentProfile.GetCounterNameAndValue( i, val );
			if ( g_bDumpCounters )
			{
				Msg("VPROF: %s = %d\n", pName, val );
			}
			if ( !vprof_counters_show_minmax.GetBool() )
			{
				Con_NPrintf( nprintIndex, "%s = %d\n", pName, val );
			}
			else
			{
				static std::vector< int > history[30];

				ks::ensure_size( history[nCycle], n );
				history[nCycle][i] = val;

				int valMin = val;
				int valMax = val;
				for (int j = 0; j < 30; j++)
				{
					ks::ensure_size( history[j], n );
					valMin = std::min( valMin, history[j][i] );
					valMax = std::max( valMax, history[j][i] );
				}

				Con_NPrintf( nprintIndex, "%s = %6d (%6d:%6d)\n", pName, val, valMin, valMax );
			}
			nprintIndex++;
		}
		nCycle = (nCycle + 1) % 30;
	}
	g_VProfCurrentProfile.ResetCounters( COUNTER_GROUP_DEFAULT );
	g_VProfCurrentProfile.ResetCounters( COUNTER_GROUP_TEXTURE_PER_FRAME );
	g_VProfCurrentProfile.ResetCounters( COUNTER_GROUP_GRAPHICS_PER_FRAME );

	// This MUST come before GetVProfPanel()->UpdateProfile(), because UpdateProfile uses the data we snapshot here.
	VProfExport_SnapshotVProfHistory();
	VProfRecord_Snapshot();

	g_bDumpCounters = false;
}

void PostUpdateProfile()
{
	if ( g_VProfCurrentProfile.IsEnabled() && !vprof_dump_spikes.GetFloat() && !vprof_dump_oninterval.GetFloat() )
	{
		g_VProfCurrentProfile.MarkFrame();
	}
}

static bool g_fVprofCacheMissOnByUI = false;
static char g_szDefferedArg1[128];
static char g_szDefferedArg2[128];

static void DeferCommand( void (*pfnImpl)(), const CCommand &args )
{
	g_pfnDeferredOp = pfnImpl;
	V_strncpy( g_szDefferedArg1, args[1], sizeof(g_szDefferedArg1) );
	V_strncpy( g_szDefferedArg2, args[2], sizeof(g_szDefferedArg2) );
}

static void spike( const CCommand &args );
static ConCommand spike_command( "spike", spike, "generates a fake spike", FCVAR_CHEAT );
static void spike( const CCommand &args )
{
	Sys_Sleep(1000);
}

static void vprof_vtune_group( const CCommand &args );
static ConCommand vprof_vtune_group_command( "vprof_vtune_group", vprof_vtune_group, "enable vtune for a particular vprof group (\"disable\" to disable)" );
static void vprof_vtune_group( const CCommand &args )
{
	if( args.ArgC() != 2 )
	{
		Warning( "vprof_vtune_group groupName (disable to turn off)\n" );
		return;
	}
	const char *pArg = args[ 1 ];
	if( V_stricmp( pArg, "disable" ) == 0 )
	{
		g_VProfCurrentProfile.DisableVTuneGroup();
	}
	else
	{
		g_VProfCurrentProfile.EnableVTuneGroup( args[ 1 ] );
	}
}

static void vprof_dump_groupnames( const CCommand &args );
static ConCommand vprof_dump_groupnames_command( "vprof_dump_groupnames", vprof_dump_groupnames, "Write the names of all of the vprof groups to the console." );
static void vprof_dump_groupnames( const CCommand &args )
{
	int n = g_VProfCurrentProfile.GetNumBudgetGroups();
	int i;
	for( i = 0; i < n; i++ )
	{
		Msg( "group %d: \"%s\"\n", i, g_VProfCurrentProfile.GetBudgetGroupName( i ) );
	}
}

static void vprof_cachemiss_Impl();
static void vprof_cachemiss( const CCommand &args ) { DeferCommand( vprof_cachemiss_Impl, args ); }
static ConCommand vprof_cachemiss_command( "vprof_cachemiss", vprof_cachemiss, "Toggle VProf cache miss checking" );
static void vprof_cachemiss_Impl()
{
	if ( !g_fVprofCacheMissOnByUI )
	{
		Msg("VProf cache miss enabled.\n");
		g_VProfCurrentProfile.PMEEnable( true );
		g_fVprofCacheMissOnByUI = true;
	}
	else
	{
		Msg("VProf cache miss disabled.\n");
		g_VProfCurrentProfile.PMEEnable( false );
		g_fVprofCacheMissOnByUI = false;
	}
}

static void vprof_cachemiss_on_Impl();
static void vprof_cachemiss_on( const CCommand &args ) { DeferCommand( vprof_cachemiss_on_Impl, args ); }
static ConCommand vprof_cachemiss_on_command( "vprof_cachemiss_on", vprof_cachemiss_on, "Turn on VProf cache miss checking" );
static void vprof_cachemiss_on_Impl()
{
	if ( !g_fVprofCacheMissOnByUI )
	{
		Msg("VProf cache miss enabled.\n");
		g_VProfCurrentProfile.PMEEnable( true );
		g_fVprofCacheMissOnByUI = true;
	}
}

static void vprof_cachemiss_off_Impl();
static void vprof_cachemiss_off( const CCommand &args ) { DeferCommand( vprof_cachemiss_off_Impl, args ); }
static ConCommand vprof_cachemiss_off_command( "vprof_cachemiss_off", vprof_cachemiss_off, "Turn off VProf cache miss checking" );
static void vprof_cachemiss_off_Impl()
{
	if ( g_fVprofCacheMissOnByUI )
	{
		Msg("VProf cache miss disabled.\n");
		g_VProfCurrentProfile.PMEEnable( false );
		g_fVprofCacheMissOnByUI = false;
	}
}

static void vprof_Impl();
static void vprof( const CCommand &args ) { DeferCommand( vprof_Impl, args ); }
static ConCommand vprof_command( "vprof", vprof, "Toggle VProf profiler" );
static void vprof_Impl()
{
	if ( !g_fVprofOnByUI )
	{
		Msg("VProf enabled.\n");
		g_VProfCurrentProfile.Start();
		g_fVprofOnByUI = true;
	}
	else
	{
		Msg("VProf disabled.\n");
		g_VProfCurrentProfile.Stop();
		g_fVprofOnByUI = false;
	}
}

static void vprof_on_Impl();
static void vprof_on( const CCommand &args ) { DeferCommand( vprof_on_Impl, args ); }
static ConCommand vprof_on_command( "vprof_on", vprof_on, "Turn on VProf profiler" );
static void vprof_on_Impl()
{
	if ( !g_fVprofOnByUI )
	{
		Msg("VProf enabled.\n");

		g_VProfCurrentProfile.Start();
		g_fVprofOnByUI = true;
	}
}

static void budget_toggle_group( const CCommand &args );
static ConCommand budget_toggle_group_command( "budget_toggle_group", budget_toggle_group, "Turn a budget group on/off" );
static void budget_toggle_group( const CCommand &args )
{
	if( args.ArgC() != 2 )
	{
		return;
	}

	int budgetGroup = g_VProfCurrentProfile.BudgetGroupNameToBudgetGroupIDNoCreate( args[1] );

	if ( budgetGroup == -1 )
	{
		return;
	}

	g_VProfCurrentProfile.HideBudgetGroup( budgetGroup, !(g_VProfCurrentProfile.GetBudgetGroupFlags( budgetGroup ) & BUDGETFLAG_HIDDEN) );
}

static void vprof_off_Impl();
static void vprof_off( const CCommand &args ) { DeferCommand( vprof_off_Impl, args ); }
static ConCommand vprof_off_command( "vprof_off", vprof_off, "Turn off VProf profiler" );
static void vprof_off_Impl()
{
	if ( g_fVprofOnByUI )
	{
		Msg("VProf disabled.\n");
		g_VProfCurrentProfile.Stop();
		g_fVprofOnByUI = false;

		// alien swarm has special behavior for certain testing scenarios:
		// generate a report after turning off vprof
		if ( g_szDefferedArg1[0] && stricmp("infested",g_szDefferedArg1) == 0 )
		{
			ConsoleLogger consoleLog;
			g_VProfCurrentProfile.OutputReport( VPRT_FULL & ~VPRT_HIERARCHY, nullptr );
		}
	}
}

static void vprof_reset_Impl();
static void vprof_reset( const CCommand &args ) { DeferCommand( vprof_reset_Impl, args ); }
static ConCommand vprof_reset_command( "vprof_reset", vprof_reset, "Reset the stats in VProf profiler" );
static void vprof_reset_Impl()
{
	Msg("VProf reset.\n");
	g_VProfCurrentProfile.Reset();
}

static void vprof_reset_peaks_Impl();
static void vprof_reset_peaks( const CCommand &args ) { DeferCommand( vprof_reset_peaks_Impl, args ); }
static ConCommand vprof_reset_peaks_command( "vprof_reset_peaks", vprof_reset_peaks, "Reset just the peak time in VProf profiler" );
static void vprof_reset_peaks_Impl()
{
	Msg("VProf peaks reset.\n");
	g_VProfCurrentProfile.ResetPeaks();
}

static void vprof_generate_report_Impl();
static void vprof_generate_report( const CCommand &args ) { DeferCommand( vprof_generate_report_Impl, args ); }
static ConCommand vprof_generate_report_command( "vprof_generate_report", vprof_generate_report, "Generate a report to the console." );
static void vprof_generate_report_Impl()
{
	g_VProfCurrentProfile.Pause();
	ConsoleLogger consoleLog;
	g_VProfCurrentProfile.OutputReport( VPRT_FULL & ~VPRT_HIERARCHY, (g_szDefferedArg1[0]) ? g_szDefferedArg1 : nullptr );
	g_VProfCurrentProfile.Resume();
}

static void vprof_generate_report_budget_Impl();
static void vprof_generate_report_budget( const CCommand &args ) { DeferCommand( vprof_generate_report_budget_Impl, args ); }
static ConCommand vprof_generate_report_budget_command( "vprof_generate_report_budget", vprof_generate_report_budget, "Generate a report to the console based on budget group." );
static void vprof_generate_report_budget_Impl()
{
	if ( !g_szDefferedArg1[0] )
	{
		return;
	}
	g_VProfCurrentProfile.Pause();
	ConsoleLogger consoleLog;
	g_VProfCurrentProfile.OutputReport( VPRT_FULL & ~VPRT_HIERARCHY, nullptr, g_VProfCurrentProfile.BudgetGroupNameToBudgetGroupID( g_szDefferedArg1 ) );
	g_VProfCurrentProfile.Resume();
}

static void vprof_generate_report_hierarchy_Impl();
static void vprof_generate_report_hierarchy( const CCommand &args ) { DeferCommand( vprof_generate_report_hierarchy_Impl, args ); }
static ConCommand vprof_generate_report_hierarchy_command( "vprof_generate_report_hierarchy", vprof_generate_report_hierarchy, "Generate a report to the console." );
static void vprof_generate_report_hierarchy_Impl()
{
	g_VProfCurrentProfile.Pause();
	ConsoleLogger consoleLog;
	g_VProfCurrentProfile.OutputReport( VPRT_HIERARCHY, (g_szDefferedArg1[0]) ? g_szDefferedArg1 : nullptr );
	g_VProfCurrentProfile.Resume();
}

static void vprof_generate_report_hierarchy_per_frame_and_count_only_Impl();
static void vprof_generate_report_hierarchy_per_frame_and_count_only( const CCommand &args ) { DeferCommand( vprof_generate_report_hierarchy_per_frame_and_count_only_Impl, args ); }
static ConCommand vprof_generate_report_hierarchy_per_frame_and_count_only_command( "vprof_generate_report_hierarchy_per_frame_and_count_only", vprof_generate_report_hierarchy_per_frame_and_count_only, "Generate a minimal hiearchical report to the console." );
static void vprof_generate_report_hierarchy_per_frame_and_count_only_Impl()
{
	g_VProfCurrentProfile.Pause();
	ConsoleLogger consoleLog;
	g_VProfCurrentProfile.OutputReport( VPRT_HIERARCHY_TIME_PER_FRAME_AND_COUNT_ONLY );
	g_VProfCurrentProfile.Resume();
}

static void vprof_generate_report_AI_Impl();
static void vprof_generate_report_AI( const CCommand &args ) { DeferCommand( vprof_generate_report_AI_Impl, args ); }
static ConCommand vprof_generate_report_AI_command( "vprof_generate_report_AI", vprof_generate_report_AI, "Generate a report to the console." );
static void vprof_generate_report_AI_Impl()
{
	// This is an unfortunate artifact of deferred commands not supporting arguments
	g_VProfCurrentProfile.Pause();
	ConsoleLogger consoleLog;
	g_VProfCurrentProfile.OutputReport( (VPRT_FULL & ~VPRT_HIERARCHY), "NPCs" );
	g_VProfCurrentProfile.Resume();
}

static void vprof_generate_report_AI_only_Impl();
static void vprof_generate_report_AI_only( const CCommand &args ) { DeferCommand( vprof_generate_report_AI_only_Impl, args ); }
static ConCommand vprof_generate_report_AI_only_command( "vprof_generate_report_AI_only", vprof_generate_report_AI_only, "Generate a report to the console." );
static void vprof_generate_report_AI_only_Impl()
{
	// This is an unfortunate artifact of deferred commands not supporting arguments
	g_VProfCurrentProfile.Pause();
	ConsoleLogger consoleLog;
	g_VProfCurrentProfile.OutputReport( (VPRT_FULL & ~VPRT_HIERARCHY), "NPCs", g_VProfCurrentProfile.BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_NPCS ) );
	g_VProfCurrentProfile.Resume();
}

static void vprof_generate_report_map_load_Impl();
static void vprof_generate_report_map_load( const CCommand &args ) { DeferCommand( vprof_generate_report_map_load_Impl, args ); }
static ConCommand vprof_generate_report_map_load_command( "vprof_generate_report_map_load", vprof_generate_report_map_load, "Generate a report to the console." );
static void vprof_generate_report_map_load_Impl()
{
	// This is an unfortunate artifact of deferred commands not supporting arguments
	g_VProfCurrentProfile.Pause();
	ConsoleLogger consoleLog;
	g_VProfCurrentProfile.OutputReport( VPRT_FULL, "Host_NewGame" );
	g_VProfCurrentProfile.Resume();
}



// ------------------------------------------------------------------------------------------------------------------------------------ //
// Exports for the dedicated server UI.
// ------------------------------------------------------------------------------------------------------------------------------------ //
class CVProfExport : public IVProfExport
{
public:

	CVProfExport()
	{
		m_nListeners = 0;
		m_bStart = m_bStop = false;
		m_BudgetFlagsFilter = 0;
	}

	inline CVProfile* GetActiveVProfile()
	{
		return g_pVProfileForDisplay;
	}
	
	inline bool CanShowBudgetGroup( int iGroup )
	{
		return ( GetActiveVProfile()->GetBudgetGroupFlags( iGroup ) & m_BudgetFlagsFilter ) != 0;
	}

	virtual void AddListener()
	{
		++m_nListeners;
		if ( m_nListeners == 1 )
			m_bStart = true;		// Defer the command till vprof is ready.
	}

	virtual void RemoveListener()
	{
		--m_nListeners;
		if ( m_nListeners == 0 )
			m_bStop = true;			// Defer the command till vprof is ready.
	}

	virtual void SetBudgetFlagsFilter( int filter )
	{
		m_BudgetFlagsFilter = filter;
	}

	virtual int GetNumBudgetGroups()
	{
		int nTotalGroups = std::min<int>( std::ssize( m_Times ), GetActiveVProfile()->GetNumBudgetGroups() );
		int nRet = 0;
		for ( int i=0; i < nTotalGroups; i++ )
		{
			if ( CanShowBudgetGroup( i ) )
				++nRet;
		}
		return nRet;
	}

	virtual void GetBudgetGroupInfos( CExportedBudgetGroupInfo *pInfos )
	{
		int iOut = 0;
		int nTotalGroups = std::min<int>( std::ssize( m_Times ), GetActiveVProfile()->GetNumBudgetGroups() );
		for ( int i=0; i < nTotalGroups; i++ )
		{
			if ( CanShowBudgetGroup( i ) )
			{
				pInfos[iOut].m_pName = GetActiveVProfile()->GetBudgetGroupName( i );
				
				int red, green, blue, alpha;
				GetActiveVProfile()->GetBudgetGroupColor( i, red, green, blue, alpha );
				pInfos[iOut].m_Color = Color( red, green, blue, alpha );

				pInfos[iOut].m_BudgetFlags = GetActiveVProfile()->GetBudgetGroupFlags( i );
				++iOut;
			}
		}
	}

	virtual void GetBudgetGroupTimes( float times[IVProfExport::MAX_BUDGETGROUP_TIMES] )
	{
		int nTotalGroups = std::min<int>( std::ssize( m_Times ), GetActiveVProfile()->GetNumBudgetGroups() );
		int nGroups = std::min<int>( nTotalGroups, IVProfExport::MAX_BUDGETGROUP_TIMES );
		std::memset( times, 0, sizeof( times[0] ) * nGroups );

		int iOut = 0;
		for ( int i=0; i < nTotalGroups; i++ )
		{
			if ( CanShowBudgetGroup( i ) )
			{
				times[iOut] = m_Times[i];
				++iOut;
			}
		}
	}

	void GetAllBudgetGroupTimes( float *pTimes )
	{
		int nTotalGroups = GetActiveVProfile()->GetNumBudgetGroups();
		for ( int i=0; i < nTotalGroups; i++ )
		{
			pTimes[i] = CanShowBudgetGroup( i ) ? m_Times[i] : 0.0f;
		}
	}

	virtual void PauseProfile()
	{
		if ( materials )
			materials->Flush();

		g_VProfCurrentProfile.Pause();
	}

	virtual void ResumeProfile()
	{
		if ( materials )
			materials->Flush();
		
		g_VProfCurrentProfile.Resume();
	}		


public:

	void StartOrStop()
	{
		if ( m_bStart )
		{
			g_VProfCurrentProfile.Start();
			m_bStart = false;
		}

		if ( m_bStop )
		{
			g_VProfCurrentProfile.Stop();
			m_bStop = false;
		}
	}

	void CalculateBudgetGroupTimes_Recursive( CVProfNode *pNode )
	{
		// If this node's info is filtered out, then put it in its parent's budget group.
		CVProfNode *pTestNode = pNode;
		while ( pTestNode != GetActiveVProfile()->GetRoot() && 
				( !CanShowBudgetGroup( pTestNode->GetBudgetGroupID() ) || 
				    ( GetActiveVProfile()->GetBudgetGroupFlags( pTestNode->GetBudgetGroupID() ) & BUDGETFLAG_HIDDEN ) != 0 ) )
		{
			pTestNode = pTestNode->GetParent();
		}

		int groupID = pTestNode->GetBudgetGroupID();
		double nodeTime = pNode->GetPrevTimeLessChildren();
		if ( groupID >= 0 && groupID < std::min<int>( std::ssize( m_Times ), IVProfExport::MAX_BUDGETGROUP_TIMES ) )
		{
			m_Times[groupID] += nodeTime;
		}
		else
		{
			contract_assert( false );
		}

		if( pNode->GetSibling() )
		{
			CalculateBudgetGroupTimes_Recursive( pNode->GetSibling() );
		}
		if( pNode->GetChild() )
		{
			CalculateBudgetGroupTimes_Recursive( pNode->GetChild() );
		}

		if ( !VProfRecord_IsPlayingBack() )
		{
			pNode->ClearPrevTime();
		}
	}

	void SnapshotVProfHistory()
	{
		// Don't do the work if there are no listeners.
		if ( !GetActiveVProfile()->IsEnabled() )
			return;
	
		if ( std::ssize( m_Times ) < GetActiveVProfile()->GetNumBudgetGroups() )
		{
			m_Times.assign( GetActiveVProfile()->GetNumBudgetGroups(), {} );
		}

		std::memset( m_Times.data(), 0, sizeof( m_Times[0] ) * GetActiveVProfile()->GetNumBudgetGroups() );
		CVProfNode *pNode = GetActiveVProfile()->GetRoot();
		if( pNode && pNode->GetChild() )
		{
			CalculateBudgetGroupTimes_Recursive( pNode->GetChild() );
		}
	}

private:
	std::vector< float > m_Times;	// Times from the most recent snapshot.
	int m_nListeners;
	int m_BudgetFlagsFilter;	// We can only capture one type of filtered data at a time.
	bool m_bStart;
	bool m_bStop;
};

CVProfExport g_VProfExport;
IVProfExport *g_pVProfExport = &g_VProfExport;


void VProfExport_SnapshotVProfHistory()
{
	g_VProfExport.SnapshotVProfHistory();
}

void VProfExport_StartOrStop()
{
	g_VProfExport.StartOrStop();
}

// Used by rpt
void VProfExport_Pause()
{
	g_VProfExport.PauseProfile();
}

void VProfExport_Resume()
{
	g_VProfExport.ResumeProfile();
}


//-----------------------------------------------------------------------------
// Used to point the budget panel at remote data
//-----------------------------------------------------------------------------
void OverrideVProfExport( IVProfExport *pExport )
{
	if ( g_pVProfExport == &g_VProfExport )
	{
		g_pVProfExport = pExport;
	}
}

void ResetVProfExport( IVProfExport *pExport )
{
	if ( g_pVProfExport == pExport )
	{
		g_pVProfExport = &g_VProfExport;
	}
}


//-----------------------------------------------------------------------------
// Listener to vprof data
//-----------------------------------------------------------------------------
struct VProfListenInfo_t
{
	ra_listener_id m_nListenerId;
	float m_flLastSentVProfDataTime;
	std::vector< std::string > m_SentGroups;

	VProfListenInfo_t() : m_flLastSentVProfDataTime( 0.0f ) {}
	VProfListenInfo_t( ra_listener_id nListenerId ) : m_nListenerId( nListenerId ), m_flLastSentVProfDataTime( 0.0f ) {}
	bool operator==( const VProfListenInfo_t& src ) const { return src.m_nListenerId == m_nListenerId; }

	VProfListenInfo_t( VProfListenInfo_t && ) = default;	// std::vector relocation; copies stay private
	VProfListenInfo_t &operator=( VProfListenInfo_t && ) = default;

private:
	VProfListenInfo_t( const VProfListenInfo_t& src );
};

static std::vector< VProfListenInfo_t > s_VProfListeners;


//-----------------------------------------------------------------------------
// Purpose: serialize and send data to remote listeners
//-----------------------------------------------------------------------------
static int FindSentGroupIndex( VProfListenInfo_t &info, const char *pGroupName )
{
	int nCount = std::ssize( info.m_SentGroups );
	for ( int i = 0; i < nCount; ++i )
	{
		if ( !V_strcmp( pGroupName, info.m_SentGroups[i].data() ) )
			return i;
	}
	return -1;
}


//-----------------------------------------------------------------------------
// Purpose: serialize and send data to remote listeners
//-----------------------------------------------------------------------------
void WriteRemoteVProfGroupData( VProfListenInfo_t &info )
{
	ks::ScratchArena<> scratch;

	int nGroupCount = g_pVProfileForDisplay->GetNumBudgetGroups();
	int nInitialCount = std::ssize( info.m_SentGroups );

	// Build list of unsent groups to send
	int nSendCount = 0;
	int *pIndex = scratch.Alloc<int>( nGroupCount ); 
	for ( int i = 0; i < nGroupCount; ++i )
	{
		const char *pName = g_pVProfileForDisplay->GetBudgetGroupName( i );
		if ( FindSentGroupIndex( info, pName ) >= 0 )
			continue;
		int j = std::ssize( info.m_SentGroups ); info.m_SentGroups.emplace_back();
		info.m_SentGroups[j] = ks::nz( pName );
		pIndex[nSendCount++] = i;
	}

	if ( nSendCount == 0 )
		return;

	ks::byte_writer buf( 1024 );
	buf.write< int >( nInitialCount );
	buf.write< int >( nSendCount );

	for ( int i=0; i < nSendCount; i++ )
	{
		int nIndex = pIndex[i];
		int red, green, blue, alpha;
		g_pVProfileForDisplay->GetBudgetGroupColor( nIndex, red, green, blue, alpha );
		buf.write< unsigned char >( red );
		buf.write< unsigned char >( green );
		buf.write< unsigned char >( blue );
		buf.write< unsigned char >( alpha );

		const char *pName = g_pVProfileForDisplay->GetBudgetGroupName( nIndex );
		buf.write_cstring( ks::nz( pName ) );
	}

	g_ServerRemoteAccess.SendVProfData( info.m_nListenerId, true, buf.data(), int( buf.size() ) );
}
		 
static ConVar rpt_vprof_time( "rpt_vprof_time","0.25", FCVAR_HIDDEN | FCVAR_DONTRECORD, "" );
void WriteRemoteVProfData()
{
	ks::ScratchArena<> scratch;

	// Throttle sending too much data
	float flMaxDelta = rpt_vprof_time.GetFloat();
	float flTime = Plat_FloatTime();
	bool bShouldSend = false;
	int nListenerCount = std::ssize( s_VProfListeners );
	for( int i = 0; i < nListenerCount; i++ )
	{
		if ( flTime - s_VProfListeners[i].m_flLastSentVProfDataTime >= flMaxDelta )
		{
			bShouldSend = true;
			break;
		}
	}

	if ( !bShouldSend )
		return;

	int nGroupCount = g_pVProfileForDisplay->GetNumBudgetGroups();
	int nBufSize = nGroupCount * sizeof(float);
	float *pTimes = (float*)scratch.AllocBytes( nBufSize );
	g_VProfExport.GetAllBudgetGroupTimes( pTimes );

	for( int i = 0; i < nListenerCount; i++ )
	{
		if ( flTime - s_VProfListeners[i].m_flLastSentVProfDataTime < flMaxDelta )
			continue;

		WriteRemoteVProfGroupData( s_VProfListeners[i] );
		s_VProfListeners[i].m_flLastSentVProfDataTime = flTime;

		// Re-order send times to match send group order
		int nSentSize = std::ssize( s_VProfListeners[i].m_SentGroups ) * sizeof(float);
		float *pSentTimes = (float*)scratch.AllocBytes( nSentSize );
		std::memset( pSentTimes, 0, nSentSize );
		for ( int j = 0; j < nGroupCount; ++j )
		{
			int nIndex = FindSentGroupIndex( s_VProfListeners[i], g_pVProfileForDisplay->GetBudgetGroupName( j ) );
			contract_assert( nIndex >= 0 );
			pSentTimes[ nIndex ] = pTimes[j];
		}
		g_ServerRemoteAccess.SendVProfData( s_VProfListeners[i].m_nListenerId, false, pSentTimes, nSentSize );
	}
}


//-----------------------------------------------------------------------------
// Purpose: add a new endpoint to send data to 
//-----------------------------------------------------------------------------
void RegisterVProfDataListener( ra_listener_id listenerID )
{
	RemoveVProfDataListener( listenerID );
	int nIndex = std::ssize( s_VProfListeners ); s_VProfListeners.emplace_back();
	s_VProfListeners[nIndex].m_nListenerId = listenerID;
	g_VProfExport.AddListener();
	WriteRemoteVProfGroupData( s_VProfListeners[nIndex] );
}


//-----------------------------------------------------------------------------
// Purpose: remove an endpoint we are sending data to
//-----------------------------------------------------------------------------
void RemoveVProfDataListener( ra_listener_id listenerID )
{
	VProfListenInfo_t findInfo( listenerID );
	if ( ks::erase_first( s_VProfListeners, findInfo ) )
	{
		g_VProfExport.RemoveListener();
	}
}
