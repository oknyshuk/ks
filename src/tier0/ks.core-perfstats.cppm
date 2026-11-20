// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2007 Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.core.perfstats;

import std;
import ks.platform;
import ks.core.time;
import ks.core.types;
import ks.core.color;
import ks.core.logging;
import ks.core.dbg;
import ks.core.memalloc;
import ks.core.threadtools;
import ks.core.tslist;
import ks.core.cache_hints;
import ks.core.scratcharena;

export extern "C++" {

//-----------------------------------------------------------------------------
//
// Slot ID for which we will be collecting performance stats
//
enum PerfStatsSlot_t
{
	PERF_STATS_SLOT_MAINTHREAD,			// stats collected in CPerfStatsData::Tick()
	PERF_STATS_SLOT_MAINTHREAD_NOWAIT,	// time spent in the main thread - time spent in EndFrame
	PERF_STATS_SLOT_RENDERTHREAD,
	PERF_STATS_SLOT_END_FRAME,
	PERF_STATS_SLOT_FORCE_HARDWARE_SYNC,
	
	PERF_STATS_SLOT_MAX
};

//-----------------------------------------------------------------------------
//
// PerfStatsSlotData struct
//	Contains performance stats for a single slot
//
class CPerfStatsSlotData
{
public:

	void Reset()
	{
		m_pszName = nullptr;
		m_CurrFrameTime.Init();
		m_PrevFrameTime.Init();
		m_AccTotalTime.Init();
	}

	void ResetFrameStats()
	{
		m_CurrFrameTime.Init();
	}
	
	void StartTimer( const char* pszName )
	{
		m_pszName = pszName;
		m_Timer.Start();
	}

	void EndTimer( void )
	{
		m_Timer.End();
		const CCycleCount& duration = m_Timer.GetDuration();
		m_CurrFrameTime += duration;
		m_AccTotalTime += duration;
	}

	const char*	m_pszName;
	CFastTimer	m_Timer;
	CCycleCount	m_CurrFrameTime;	// Accumulated time over a single frame
	CCycleCount m_PrevFrameTime;	// This is used to display cl_showfps
	CCycleCount m_AccTotalTime;		// Total accumulated time
};

//-----------------------------------------------------------------------------
//
// PerfStatsData struct
//
class CPerfStatsData
{
public:

	CPerfStatsData();

	void Tick();
	void Reset();

	uint64				m_nFrames;
	CPerfStatsSlotData	m_Slots[PERF_STATS_SLOT_MAX];
};
extern CPerfStatsData g_PerfStats;

//-----------------------------------------------------------------------------
//
// Helper class that times whatever block of code it's in
// and collect data for the given slot
//
class CPerfStatsScope
{
public:

	CPerfStatsScope( const char* pszName, PerfStatsSlot_t slotId ) : m_SlotData( nullptr )
	{
		if ( slotId < PERF_STATS_SLOT_MAX )
		{
			m_SlotData = &g_PerfStats.m_Slots[slotId];
			m_SlotData->StartTimer( pszName );
		}
	}

	~CPerfStatsScope()
	{
		if ( m_SlotData )
		{
			m_SlotData->EndTimer();

		}
	}

private:

	CPerfStatsSlotData* m_SlotData;
};

}
