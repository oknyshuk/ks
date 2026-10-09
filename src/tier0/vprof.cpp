// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Real-Time Hierarchical Profiling

import ks.core;

// First include standard libraries
import std;


// NOTE: Explicitly and intentionally using STL in here to not generate any
// cyclical dependencies between the low-level debug library and the higher
// level data structures (toml 01-27-03)
using namespace std;


//-----------------------------------------------------------------------------
bool g_VProfSignalSpike;

//-----------------------------------------------------------------------------

CVProfile g_VProfCurrentProfile [[gnu::init_priority(101)]];

int CVProfNode::s_iCurrentUniqueNodeID = 0;


CVProfNode::~CVProfNode()
{
}

CVProfNode *CVProfNode::GetSubNode( const tchar *pszName, int detailLevel, const tchar *pBudgetGroupName, int budgetFlags )
{
	// Try to find this sub node
	CVProfNode * child = m_pChild;
	while ( child ) 
	{
		if ( child->m_pszName == pszName ) 
		{
			return child;
		}
		child = child->m_pSibling;
	}

	// We didn't find it, so add it
	CVProfNode * node = new CVProfNode( pszName, detailLevel, this, pBudgetGroupName, budgetFlags );
	node->m_pSibling = m_pChild;
	m_pChild = node;
	return node;
}

CVProfNode *CVProfNode::GetSubNode( const tchar *pszName, int detailLevel, const tchar *pBudgetGroupName )
{
	return GetSubNode( pszName, detailLevel, pBudgetGroupName, BUDGETFLAG_OTHER );
}


//-------------------------------------

void CVProfNode::EnterScope()
{
	m_nCurFrameCalls++;
	if ( m_nRecursions++ == 0 )
	{
		m_Timer.Start();
		if ( g_VProfCurrentProfile.UsePME() )
		{
			m_L2Cache.Start();
		}

		g_VProfCurrentProfile.PushGroup( m_BudgetGroupID );
	}
}

//-------------------------------------

bool CVProfNode::ExitScope()
{
	if ( --m_nRecursions == 0 && m_nCurFrameCalls != 0 )
	{
		m_Timer.End();
		m_CurFrameTime += m_Timer.GetDuration();
		if ( g_VProfCurrentProfile.UsePME() )
		{
			m_L2Cache.End();
			m_iCurL2CacheMiss += m_L2Cache.GetL2CacheMisses();
		}

		g_VProfCurrentProfile.PopGroup();
	}
	return ( m_nRecursions == 0 );
}

//-------------------------------------

void CVProfNode::Pause()
{
	if ( m_nRecursions > 0 )
	{
		m_Timer.End();
		m_CurFrameTime += m_Timer.GetDuration();

		if ( g_VProfCurrentProfile.UsePME() )
		{
			m_L2Cache.End();
			m_iCurL2CacheMiss += m_L2Cache.GetL2CacheMisses();
		}
	}
	if ( m_pChild ) 
	{
		m_pChild->Pause();
	}
	if ( m_pSibling ) 
	{
		m_pSibling->Pause();
	}
}

//-------------------------------------

void CVProfNode::Resume()
{
	if ( m_nRecursions > 0 )
	{
		m_Timer.Start();

		if ( g_VProfCurrentProfile.UsePME() )
		{
			m_L2Cache.Start();
		}
	}
	if ( m_pChild ) 
	{
		m_pChild->Resume();
	}
	if ( m_pSibling ) 
	{
		m_pSibling->Resume();
	}
}

//-------------------------------------

void CVProfNode::Reset()
{
	m_nPrevFrameCalls = 0;
	m_PrevFrameTime.Init();

	m_nCurFrameCalls = 0;
	m_CurFrameTime.Init();
	
	m_nTotalCalls = 0;
	m_TotalTime.Init();
	
	m_PeakTime.Init();

	m_iPrevL2CacheMiss = 0;
	m_iCurL2CacheMiss = 0;
	m_iTotalL2CacheMiss = 0;

	if ( m_pChild ) 
	{
		m_pChild->Reset();
	}
	if ( m_pSibling ) 
	{
		m_pSibling->Reset();
	}
}


//-------------------------------------

void CVProfNode::MarkFrame()
{
	m_nPrevFrameCalls = m_nCurFrameCalls;
	m_PrevFrameTime = m_CurFrameTime;
	m_iPrevL2CacheMiss = m_iCurL2CacheMiss;
	m_nTotalCalls += m_nCurFrameCalls;
	m_TotalTime += m_CurFrameTime;
	
	if ( m_PeakTime.IsLessThan( m_CurFrameTime ) )
	{
		m_PeakTime = m_CurFrameTime;
	}
	
	m_CurFrameTime.Init();
	m_nCurFrameCalls = 0;
	m_iTotalL2CacheMiss += m_iCurL2CacheMiss;
	m_iCurL2CacheMiss = 0;

	if ( m_pChild ) 
	{
		m_pChild->MarkFrame();
	}
	if ( m_pSibling ) 
	{
		m_pSibling->MarkFrame();
	}
}

//-------------------------------------

void CVProfNode::ResetPeak()
{
	m_PeakTime.Init();

	if ( m_pChild ) 
	{
		m_pChild->ResetPeak();
	}
	if ( m_pSibling ) 
	{
		m_pSibling->ResetPeak();
	}
}


void CVProfNode::SetCurFrameTime( unsigned long milliseconds )
{
	m_CurFrameTime.Init( (float)milliseconds );
}


//-----------------------------------------------------------------------------

struct TimeSums_t
{
	const tchar *pszProfileScope;
	unsigned	calls;
	double 		time;
	double 		timeLessChildren;
	double		peak;
};

static bool TimeCompare( const TimeSums_t &lhs, const TimeSums_t &rhs )
{
	return ( lhs.time > rhs.time );
}

static bool TimeLessChildrenCompare( const TimeSums_t &lhs, const TimeSums_t &rhs )
{
	return ( lhs.timeLessChildren > rhs.timeLessChildren );
}

static bool PeakCompare( const TimeSums_t &lhs, const TimeSums_t &rhs )
{
	return ( lhs.peak > rhs.peak );
}

static bool AverageTimeCompare( const TimeSums_t &lhs, const TimeSums_t &rhs )
{
	double avgLhs = ( lhs.calls ) ? lhs.time / (double)lhs.calls : 0.0;
	double avgRhs = ( rhs.calls ) ? rhs.time / (double)rhs.calls : 0.0;
	return ( avgLhs > avgRhs );
}

static bool AverageTimeLessChildrenCompare( const TimeSums_t &lhs, const TimeSums_t &rhs )
{
	double avgLhs = ( lhs.calls ) ? lhs.timeLessChildren / (double)lhs.calls : 0.0;
	double avgRhs = ( rhs.calls ) ? rhs.timeLessChildren / (double)rhs.calls : 0.0;
	return ( avgLhs > avgRhs );

}
static bool PeakOverAverageCompare( const TimeSums_t &lhs, const TimeSums_t &rhs )
{
	double avgLhs = ( lhs.calls ) ? lhs.time / (double)lhs.calls : 0.0;
	double avgRhs = ( rhs.calls ) ? rhs.time / (double)rhs.calls : 0.0;
	
	double lhsPoA = ( avgLhs != 0 ) ? lhs.peak / avgLhs : 0.0;
	double rhsPoA = ( avgRhs != 0 ) ? rhs.peak / avgRhs : 0.0;
	
	return ( lhsPoA > rhsPoA );
}

map<CVProfNode *, double> 	g_TimesLessChildren;
int							g_TotalFrames;
map<const tchar *, uintp>	g_TimeSumsMap;
vector<TimeSums_t> 			g_TimeSums;
CVProfNode *				g_pStartNode;
const tchar *				g_pszSumNode;

//-------------------------------------

void CVProfile::SumTimes( CVProfNode *pNode, int budgetGroupID )
{
	if ( !pNode )
		return; // this generally only happens on a failed FindNode()

	bool bSetStartNode;
	if ( !g_pStartNode && strcmp( pNode->GetName(), g_pszSumNode ) == 0 )
	{
		g_pStartNode = pNode;
		bSetStartNode = true;
	}
	else
		bSetStartNode = false;

	if ( GetRoot() != pNode )
	{
		if ( g_pStartNode && pNode->GetTotalCalls() > 0 && ( budgetGroupID == -1 || pNode->GetBudgetGroupID() == budgetGroupID ) )
		{
			double timeLessChildren = pNode->GetTotalTimeLessChildren();
			
			g_TimesLessChildren.insert( make_pair( pNode, timeLessChildren ) );
			
			map<const tchar *, uintp>::iterator iter;
			iter = g_TimeSumsMap.find( pNode->GetName() ); // intenionally using address of string rather than string compare (toml 01-27-03)
			if ( iter == g_TimeSumsMap.end() )
			{
				TimeSums_t timeSums = { pNode->GetName(), static_cast<unsigned int>(pNode->GetTotalCalls()), pNode->GetTotalTime(), timeLessChildren, pNode->GetPeakTime() };
				g_TimeSumsMap.insert( make_pair( pNode->GetName(), g_TimeSums.size() ) );
				g_TimeSums.push_back( timeSums );
			}
			else
			{
				TimeSums_t &timeSums = g_TimeSums[iter->second];
				timeSums.calls += pNode->GetTotalCalls();
				timeSums.time += pNode->GetTotalTime();
				timeSums.timeLessChildren += timeLessChildren;
				if ( pNode->GetPeakTime() > timeSums.peak )
					timeSums.peak = pNode->GetPeakTime();
			}
		}
			
		if( ( !g_pStartNode || pNode != g_pStartNode ) && pNode->GetSibling() )
		{
			SumTimes( pNode->GetSibling(), budgetGroupID );
		}
	}
	
	if( pNode->GetChild() )
	{
		SumTimes( pNode->GetChild(), budgetGroupID );
	}
		
	if ( bSetStartNode )
		g_pStartNode = nullptr;
}

//-------------------------------------

CVProfNode *CVProfile::FindNode( CVProfNode *pStartNode, const tchar *pszNode )
{
	if ( strcmp( pStartNode->GetName(), pszNode ) != 0 )
	{
		CVProfNode *pFoundNode = nullptr;
		if ( pStartNode->GetSibling() )
		{
			pFoundNode = FindNode( pStartNode->GetSibling(), pszNode );
		}

		if ( !pFoundNode && pStartNode->GetChild() )
		{
			pFoundNode = FindNode( pStartNode->GetChild(), pszNode );
		}

		return pFoundNode;
	}
	return pStartNode;
}

//-------------------------------------

void CVProfile::SumTimes( const tchar *pszStartNode, int budgetGroupID )
{
	if ( GetRoot()->GetChild() )
	{
		if ( pszStartNode == nullptr )
			g_pStartNode = GetRoot();
		else
			g_pStartNode = nullptr;

		g_pszSumNode = pszStartNode;
		SumTimes( GetRoot(), budgetGroupID );
		g_pStartNode = nullptr;
	}

}

//-------------------------------------

void CVProfile::DumpNodes( CVProfNode *pNode, int indent, bool bAverageAndCountOnly )
{
	if ( !pNode )
		return; // this generally only happens on a failed FindNode()

	bool fIsRoot = ( pNode == GetRoot() );

	if ( fIsRoot || pNode == g_pStartNode )
	{
		if( bAverageAndCountOnly )
		{
			Msg( " Avg Time/Frame (ms)\n" );
			Msg( "[ func+child      func ]       Count\n" );
			Msg( "  ---------- ---------      --------\n" );
		}
		else
		{
			Msg( "       Sum (ms)            Avg Time/Frame (ms)     Avg Time/Call (ms)\n" );
			Msg( "[ func+child      func ]  [ func+child   func ]  [ func+child   func ]    Count   Peak\n" );
			Msg( "  ---------- ---------      ---------- ------      ---------- ------   -------- ------\n" );
		}
	}

	if ( !fIsRoot )
	{
		map<CVProfNode *, double>::iterator iterTimeLessChildren = g_TimesLessChildren.find( pNode );
		
		double dNodeTime = 0;
		if(iterTimeLessChildren != g_TimesLessChildren.end())
			dNodeTime = iterTimeLessChildren->second;

		if( bAverageAndCountOnly )
		{
			Msg( "  %10.3f %9.2f      %8d", 
						 ( pNode->GetTotalCalls() > 0 ) ? pNode->GetTotalTime() / (double)NumFramesSampled() : 0, 
						 ( pNode->GetTotalCalls() > 0 ) ? dNodeTime / (double)NumFramesSampled() : 0, 
						 pNode->GetTotalCalls()  );
		}
		else
		{
			Msg( "  %10.3f %9.2f      %10.3f %6.2f      %10.3f %6.2f   %8d %6.2f", 
						 pNode->GetTotalTime(), dNodeTime,
						 ( pNode->GetTotalCalls() > 0 ) ? pNode->GetTotalTime() / (double)NumFramesSampled() : 0, 
						 ( pNode->GetTotalCalls() > 0 ) ? dNodeTime / (double)NumFramesSampled() : 0, 
						 ( pNode->GetTotalCalls() > 0 ) ? pNode->GetTotalTime() / (double)pNode->GetTotalCalls() : 0, 
						 ( pNode->GetTotalCalls() > 0 ) ? dNodeTime / (double)pNode->GetTotalCalls() : 0, 
						 pNode->GetTotalCalls(), pNode->GetPeakTime()  );
		}
		
		Msg( "  " );
		for ( int i = 1; i < indent; i++ )
		{
			Msg( "|  " );
		}

		Msg( "%s\n", pNode->GetName() );
	}

	if( pNode->GetChild() )
	{
		DumpNodes( pNode->GetChild(), indent + 1, bAverageAndCountOnly );
	}
	
	if( !( fIsRoot || pNode == g_pStartNode ) && pNode->GetSibling() )
	{
		DumpNodes( pNode->GetSibling(), indent, bAverageAndCountOnly );
	}
}

//-------------------------------------

//-------------------------------------
static void DumpSorted( const tchar *pszHeading, double totalTime, bool (*pfnSort)( const TimeSums_t &, const TimeSums_t & ), int maxLen = 999999 )
{
	unsigned i;
	vector<TimeSums_t> sortedSums;
	sortedSums = g_TimeSums;
	sort( sortedSums.begin(), sortedSums.end(), pfnSort );
	
	Msg( "%s\n", pszHeading);
    Msg( "  Scope                                                      Calls Calls/Frame  Time+Child    Pct        Time    Pct   Avg/Frame    Avg/Call Avg-NoChild        Peak\n");
    Msg( "  ---------------------------------------------------- ----------- ----------- ----------- ------ ----------- ------ ----------- ----------- ----------- -----------\n");
    for ( i = 0; i < sortedSums.size() && i < (unsigned)maxLen; i++ )
    {
		double avg = ( sortedSums[i].calls ) ? sortedSums[i].time / (double)sortedSums[i].calls : 0.0;
		double avgLessChildren = ( sortedSums[i].calls ) ? sortedSums[i].timeLessChildren / (double)sortedSums[i].calls : 0.0;
		
        Msg( "  %52.52s%12d%12.3f%12.3f%7.2f%12.3f%7.2f%12.3f%12.3f%12.3f%12.3f\n", 
             sortedSums[i].pszProfileScope,
             sortedSums[i].calls,
			 (float)sortedSums[i].calls / (float)g_TotalFrames,
			 sortedSums[i].time,
			 min( ( sortedSums[i].time / totalTime ) * 100.0, 100.0 ),
			 sortedSums[i].timeLessChildren,
			 min( ( sortedSums[i].timeLessChildren / totalTime ) * 100.0, 100.0 ),
			 sortedSums[i].time / (float)g_TotalFrames,
			 avg,
			 avgLessChildren,
			 sortedSums[i].peak );
	}
}

//-------------------------------------

void CVProfile::OutputReport( int type, const tchar *pszStartNode, int budgetGroupID )
{
	Msg( "******** BEGIN VPROF REPORT ********\n");

	g_TotalFrames = max( NumFramesSampled() - 1, 1 );
	
	if ( NumFramesSampled() == 0 || GetTotalTimeSampled() == 0)
		Msg( "No samples\n" );
	else
	{
		if ( type & VPRT_SUMMARY )
		{
			Msg( "-- Summary --\n" );
			Msg( "%d frames sampled for %.2f seconds\n", g_TotalFrames, GetTotalTimeSampled() / 1000.0 );
			Msg( "Average %.2f fps, %.2f ms per frame\n", 1000.0 / ( GetTotalTimeSampled() / g_TotalFrames ), GetTotalTimeSampled() / g_TotalFrames );
			Msg( "Peak %.2f ms frame\n", GetPeakFrameTime() );
			
			double timeAccountedFor = 100.0 - ( m_Root.GetTotalTimeLessChildren() / m_Root.GetTotalTime() );
			Msg( "%.0f pct of time accounted for\n", min( 100.0, timeAccountedFor ) );
			Msg( "\n" );
		}

		if ( pszStartNode == nullptr )
		{
			pszStartNode = GetRoot()->GetName();
		}

		SumTimes( pszStartNode, budgetGroupID );
		
		// Dump the hierarchy
		if ( type & VPRT_HIERARCHY )
		{
			Msg( "-- Hierarchical Call Graph --\n");
			if ( pszStartNode == nullptr )
				g_pStartNode = nullptr;
			else
				g_pStartNode = FindNode( GetRoot(), pszStartNode );

			DumpNodes( (!g_pStartNode) ? GetRoot() : g_pStartNode, 0, false );
			Msg( "\n" );
		}
		
		if ( type & VPRT_HIERARCHY_TIME_PER_FRAME_AND_COUNT_ONLY )
		{
			Msg( "-- Hierarchical Call Graph --\n");
			if ( pszStartNode == nullptr )
				g_pStartNode = nullptr;
			else
				g_pStartNode = FindNode( GetRoot(), pszStartNode );

			DumpNodes( (!g_pStartNode) ? GetRoot() : g_pStartNode, 0, true );
			Msg( "\n" );
		}

		int maxLen = ( type & VPRT_LIST_TOP_ITEMS_ONLY ) ? 25 : 999999;

		if ( type & VPRT_LIST_BY_TIME )
		{
			DumpSorted( "-- Profile scopes sorted by time (including children) --", GetTotalTimeSampled(), TimeCompare, maxLen );
			Msg( "\n" );
		}
		if ( type & VPRT_LIST_BY_TIME_LESS_CHILDREN )
		{
			DumpSorted( "-- Profile scopes sorted by time (without children) --", GetTotalTimeSampled(), TimeLessChildrenCompare, maxLen );
			Msg( "\n" );
		}
		if ( type & VPRT_LIST_BY_AVG_TIME )
		{
			DumpSorted( "-- Profile scopes sorted by average time (including children) --", GetTotalTimeSampled(), AverageTimeCompare, maxLen );
			Msg( "\n" );
		}
		if ( type & VPRT_LIST_BY_AVG_TIME_LESS_CHILDREN )
		{
			DumpSorted( "-- Profile scopes sorted by average time (without children) --", GetTotalTimeSampled(), AverageTimeLessChildrenCompare, maxLen );
			Msg( "\n" );
		}
		if ( type & VPRT_LIST_BY_PEAK_TIME )
		{
			DumpSorted( "-- Profile scopes sorted by peak --", GetTotalTimeSampled(), PeakCompare, maxLen);
			Msg( "\n" );
		}
		if ( type & VPRT_LIST_BY_PEAK_OVER_AVERAGE )
		{
			DumpSorted( "-- Profile scopes sorted by peak over average (including children) --", GetTotalTimeSampled(), PeakOverAverageCompare, maxLen );
			Msg( "\n" );
		}
		
		// TODO: Functions by time less children
		// TODO: Functions by time averages
		// TODO: Functions by peak
		// TODO: Peak deviation from average
		g_TimesLessChildren.clear();
		g_TimeSumsMap.clear();
		g_TimeSums.clear();
	}
	Msg( "******** END VPROF REPORT ********\n");

}

//=============================================================================

CVProfile::CVProfile() 
 :	m_Root( "Root", 0, nullptr, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, 0 ),
	m_pCurNode( &m_Root ), 
 	m_nFrames( 0 ),
 	m_enabled( 0 ),  // don't change this. if m_enabled is anything but zero coming out of this constructor, vprof will break.
 	m_pausedEnabledDepth( 0 ),
	m_fAtRoot( true )
{

	m_GroupIDStackDepth = 1;
	m_GroupIDStack[0] = 0; // VPROF_BUDGETGROUP_OTHER_UNACCOUNTED

	m_TargetThreadId = ThreadGetCurrentId();
	
	// Go ahead and allocate 32 slots for budget group names
	m_pBudgetGroups = new CVProfile::CBudgetGroup[32];
	m_nBudgetGroupNames = 0;
	m_nBudgetGroupNamesAllocated = 32;

	// Add these here so that they will always be in the same order.
	// VPROF_BUDGETGROUP_OTHER_UNACCOUNTED has to be FIRST!!!!
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_OTHER_UNACCOUNTED,		BUDGETFLAG_OTHER | BUDGETFLAG_SERVER );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_WORLD_RENDERING,			BUDGETFLAG_CLIENT );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_DISPLACEMENT_RENDERING,	BUDGETFLAG_CLIENT );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_GAME,						BUDGETFLAG_OTHER | BUDGETFLAG_SERVER );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_PLAYER,					BUDGETFLAG_OTHER | BUDGETFLAG_SERVER );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_NPCS,						BUDGETFLAG_OTHER | BUDGETFLAG_SERVER );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_SERVER_ANIM,				BUDGETFLAG_OTHER | BUDGETFLAG_SERVER );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_CLIENT_ANIMATION,			BUDGETFLAG_CLIENT );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_PHYSICS,					BUDGETFLAG_OTHER | BUDGETFLAG_SERVER );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_STATICPROP_RENDERING,		BUDGETFLAG_CLIENT );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_MODEL_RENDERING,			BUDGETFLAG_CLIENT );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_MODEL_FAST_PATH_RENDERING,BUDGETFLAG_CLIENT );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_LIGHTCACHE,				BUDGETFLAG_CLIENT );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_BRUSHMODEL_RENDERING,		BUDGETFLAG_CLIENT );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_SHADOW_RENDERING,			BUDGETFLAG_CLIENT );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_DETAILPROP_RENDERING,		BUDGETFLAG_CLIENT );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_PARTICLE_RENDERING,		BUDGETFLAG_CLIENT );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_ROPES,					BUDGETFLAG_CLIENT );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_DLIGHT_RENDERING,			BUDGETFLAG_CLIENT );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_OTHER_NETWORKING,			BUDGETFLAG_OTHER | BUDGETFLAG_SERVER );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_OTHER_SOUND,				BUDGETFLAG_OTHER | BUDGETFLAG_SERVER );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_OTHER_VGUI,				BUDGETFLAG_OTHER | BUDGETFLAG_SERVER );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_OTHER_FILESYSTEM,			BUDGETFLAG_OTHER | BUDGETFLAG_SERVER );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_PREDICTION,				BUDGETFLAG_CLIENT );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_INTERPOLATION,			BUDGETFLAG_CLIENT );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_SWAP_BUFFERS,				BUDGETFLAG_CLIENT );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_OCCLUSION,				BUDGETFLAG_CLIENT );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_OVERLAYS,					BUDGETFLAG_CLIENT );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_TOOLS,					BUDGETFLAG_OTHER | BUDGETFLAG_CLIENT );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_TEXTURE_CACHE,			BUDGETFLAG_CLIENT );
	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_REPLAY,					BUDGETFLAG_SERVER );
//	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_DISP_HULLTRACES );

	m_bPMEInit = false;
	m_bPMEEnabled = false;

}


CVProfile::~CVProfile()
{
	Term();
}


void CVProfile::FreeNodes_R( CVProfNode *pNode )
{
	CVProfNode *pNext;
	for ( CVProfNode *pChild = pNode->GetChild(); pChild; pChild = pNext )
	{
		pNext = pChild->GetSibling();
		FreeNodes_R( pChild );
	}
	
	if ( pNode == GetRoot() )
	{
		pNode->m_pChild = nullptr;
	}
	else
	{
		delete pNode;
	}
}


void CVProfile::Term()
{
	int i;
	for( i = 0; i < m_nBudgetGroupNames; i++ )
	{
		delete [] m_pBudgetGroups[i].m_pName;
	}
	delete[] m_pBudgetGroups; //lwss: fix delete => delete[]
	m_nBudgetGroupNames = m_nBudgetGroupNamesAllocated = 0;
	m_pBudgetGroups = nullptr;

	int n;
	for( n = 0; n < m_NumCounters; n++ )
	{
		delete [] m_CounterNames[n];
		m_CounterNames[n] = nullptr;
	}
	m_NumCounters = 0;

	// Free the nodes.
	if ( GetRoot() )
	{
		FreeNodes_R( GetRoot() );
	}
}


inline constexpr int COLORMIN = 160;
inline constexpr int COLORMAX = 255;

static int g_ColorLookup[4] = 
{
	COLORMIN, 
	COLORMAX,
	COLORMIN+(COLORMAX-COLORMIN)/3,
	COLORMIN+((COLORMAX-COLORMIN)*2)/3, 
};

template <typename T1, typename T2> [[gnu::always_inline]] inline auto GET_BIT( T1 val, T2 bitnum ) { return ( ( val >> bitnum ) & 0x1 ); }

void CVProfile::GetBudgetGroupColor( int budgetGroupID, int &r, int &g, int &b, int &a )
{
	budgetGroupID = budgetGroupID % ( 1 << 6 );

	int index;
	index = GET_BIT( budgetGroupID, 0 ) | ( GET_BIT( budgetGroupID, 5 ) << 1 );
	r = g_ColorLookup[index];
	index = GET_BIT( budgetGroupID, 1 ) | ( GET_BIT( budgetGroupID, 4 ) << 1 );
	g = g_ColorLookup[index];
	index = GET_BIT( budgetGroupID, 2 ) | ( GET_BIT( budgetGroupID, 3 ) << 1 );
	b = g_ColorLookup[index];
	a = 255;
}

// return -1 if it doesn't exist.
int CVProfile::FindBudgetGroupName( const tchar *pBudgetGroupName )
{
	int i;
	for( i = 0; i < m_nBudgetGroupNames; i++ )
	{
		if( V_tier0_stricmp( pBudgetGroupName, m_pBudgetGroups[i].m_pName ) == 0 )
		{
			return i;
		}
	}
	return -1;
}

int CVProfile::AddBudgetGroupName( const tchar *pBudgetGroupName, int budgetFlags )
{
	tchar *pNewString = new tchar[ _tcslen( pBudgetGroupName ) + 1 ];
	_tcscpy( pNewString, pBudgetGroupName );
	if( m_nBudgetGroupNames + 1 > m_nBudgetGroupNamesAllocated )
	{
		m_nBudgetGroupNamesAllocated *= 2;
		m_nBudgetGroupNamesAllocated = max( m_nBudgetGroupNames + 6, m_nBudgetGroupNamesAllocated );
		
		CBudgetGroup *pNew = new CBudgetGroup[ m_nBudgetGroupNamesAllocated ];
		for ( int i=0; i < m_nBudgetGroupNames; i++ )
			pNew[i] = m_pBudgetGroups[i];
		
		delete [] m_pBudgetGroups;
		m_pBudgetGroups = pNew;
	}

	m_pBudgetGroups[m_nBudgetGroupNames].m_pName = pNewString;
	m_pBudgetGroups[m_nBudgetGroupNames].m_BudgetFlags = budgetFlags;
	m_nBudgetGroupNames++;
	if( m_pNumBudgetGroupsChangedCallBack )
	{
		(*m_pNumBudgetGroupsChangedCallBack)();
	}

	return m_nBudgetGroupNames - 1;
}

int CVProfile::BudgetGroupNameToBudgetGroupID( const tchar *pBudgetGroupName, int budgetFlagsToORIn )
{
	int budgetGroupID = FindBudgetGroupName( pBudgetGroupName );
	if( budgetGroupID == -1 )
	{
		budgetGroupID = AddBudgetGroupName( pBudgetGroupName, budgetFlagsToORIn );
	}
	else
	{
		m_pBudgetGroups[budgetGroupID].m_BudgetFlags |= budgetFlagsToORIn;
	}

	return budgetGroupID;
}

int CVProfile::BudgetGroupNameToBudgetGroupID( const tchar *pBudgetGroupName )
{
	return BudgetGroupNameToBudgetGroupID( pBudgetGroupName, BUDGETFLAG_OTHER );
}

int CVProfile::GetNumBudgetGroups( void )
{
	return m_nBudgetGroupNames;
}

void CVProfile::RegisterNumBudgetGroupsChangedCallBack( void (*pCallBack)(void) )
{
	m_pNumBudgetGroupsChangedCallBack = pCallBack;
}

void CVProfile::HideBudgetGroup( int budgetGroupID, bool bHide )
{
	if( budgetGroupID != -1 )
	{
		if ( bHide )
			m_pBudgetGroups[budgetGroupID].m_BudgetFlags |= BUDGETFLAG_HIDDEN;
		else
			m_pBudgetGroups[budgetGroupID].m_BudgetFlags &= ~BUDGETFLAG_HIDDEN;
	}
}

int *CVProfile::FindOrCreateCounter( const tchar *pName, CounterGroup_t eCounterGroup )
{	
	contract_assert( m_NumCounters+1 < MAXCOUNTERS );
	if ( m_NumCounters + 1 >= MAXCOUNTERS || !InTargetThread() )
	{
		static int dummy;
		return &dummy;
	}
	int i;
	for( i = 0; i < m_NumCounters; i++ )
	{
		if( V_tier0_stricmp( m_CounterNames[i], pName ) == 0 )
		{
			// found it!
			return &m_Counters[i];
		}
	}

	// NOTE: These get freed in ~CVProfile.
	tchar *pNewName = new tchar[_tcslen( pName ) + 1];
	_tcscpy( pNewName, pName );
	m_Counters[m_NumCounters] = 0;
	m_CounterGroups[m_NumCounters] = (char)eCounterGroup;
	m_CounterNames[m_NumCounters++] = pNewName;
	return &m_Counters[m_NumCounters-1];
}

void CVProfile::ResetCounters( CounterGroup_t eCounterGroup )
{
	int i;
	for( i = 0; i < m_NumCounters; i++ )
	{
		if ( m_CounterGroups[i] == eCounterGroup )
			m_Counters[i] = 0;
	}
}

int CVProfile::GetNumCounters() const
{
	return m_NumCounters;
}

const tchar *CVProfile::GetCounterName( int index ) const
{
	contract_assert( index >= 0 && index < m_NumCounters );
	return m_CounterNames[index];
}

int CVProfile::GetCounterValue( int index ) const
{
	contract_assert( index >= 0 && index < m_NumCounters );
	return m_Counters[index];
}

const tchar *CVProfile::GetCounterNameAndValue( int index, int &val ) const
{
	contract_assert( index >= 0 && index < m_NumCounters );
	val = m_Counters[index];
	return m_CounterNames[index];
}

CounterGroup_t CVProfile::GetCounterGroup( int index ) const
{
	contract_assert( index >= 0 && index < m_NumCounters );
	return (CounterGroup_t)m_CounterGroups[index];
}
