// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.core.microprofiler;

import std;
import ks.platform;
import ks.core.platform;
import ks.core.types;
import ks.core.color;
import ks.core.logging;
import ks.core.dbg;
import ks.core.memalloc;
import ks.core.threadtools;
import ks.core.tslist;
import ks.core.cache_hints;
import ks.core.scratcharena;
import ks.core.perfstats;

export extern "C++" {

// define this to 1 to enable microprofiler; 2 to enable miniprofiler

class CMicroProfiler;

void MicroProfilerAddTS( CMicroProfiler *pProfiler, uint64 numTimeBaseTicks );
int64 GetHardwareClockReliably();

inline unsigned long long GetTimebaseRegister( void )
{
    unsigned long long Low, High;
    __asm__ __volatile__ ( "rdtsc" : "=a" (Low), "=d" (High) );
    return ( High << 32 ) | ( Low & 0xffffffff );
}

class CMicroProfilerSample
{
	int64 m_nTimeBaseBegin;       // time base is kept here instead of using -= and += for better reliability and to avoid a cache miss at the beginning of the profiled section

public:
	CMicroProfilerSample()
	{
		m_nTimeBaseBegin = GetTimebaseRegister();
	}

	int64 GetElapsed()const
	{
		return GetTimebaseRegister() - m_nTimeBaseBegin;
	}
};

typedef CMicroProfilerSample CMicroProfilerQpcSample;

class CMicroProfiler
{
public:
	uint64 m_numTimeBaseTicks;	 // this will be totally screwed between Begin() and End()
	uint64 m_numCalls;
public:
	CMicroProfiler()
	{
		Reset();
	}

	CMicroProfiler( const CMicroProfiler &other )
	{
		m_numTimeBaseTicks = other.m_numTimeBaseTicks;
		m_numCalls = other.m_numCalls;
	}

	CMicroProfiler &operator=( const CMicroProfiler &other )
	{
		m_numTimeBaseTicks = other.m_numTimeBaseTicks;
		m_numCalls = other.m_numCalls;
		return *this;
	}

	void Begin()
	{
		m_numTimeBaseTicks -= GetTimebaseRegister();
	}

	void End()
	{
		m_numTimeBaseTicks += GetTimebaseRegister();
		m_numCalls ++;
	}
	void End( uint64 nCalls )
	{
		m_numTimeBaseTicks += GetTimebaseRegister();
		m_numCalls += nCalls;
	}

	void Add( uint64 numTimeBaseTicks, uint64 numCalls = 1)
	{
		m_numTimeBaseTicks += numTimeBaseTicks;
		m_numCalls += numCalls;
	}

	void AddTS( uint64 numTimeBaseTicks )
	{
		MicroProfilerAddTS( this, numTimeBaseTicks );
	}

	void Add(const CMicroProfilerSample &sample)
	{
		Add( sample.GetElapsed() );
	}

	void Add( const CMicroProfiler& profiler)
	{
		Add( profiler.m_numTimeBaseTicks, profiler.m_numCalls );
	}

	void Reset()
	{
		m_numTimeBaseTicks = 0;
		m_numCalls = 0;
	}

	void Damp( int shift = 1 )
	{
		m_numTimeBaseTicks >>= shift;
		m_numCalls >>= shift;
	}

	uint64 GetNumCalls() const
	{
		return m_numCalls;
	}

	uint64 GetNumTimeBaseTicks() const
	{
		return m_numTimeBaseTicks;
	}

	float GetAverageMilliseconds() const
	{
		return m_numCalls ? GetTotalMilliseconds() / m_numCalls : 0;
	}

	float GetTotalMilliseconds() const
	{
		return TimeBaseTicksToMilliseconds( m_numTimeBaseTicks );
	}

	static float TimeBaseTicksToMilliseconds( uint64 numTimeBaseTicks )
	{
		return numTimeBaseTicks / ( GetCPUInformation().m_Speed * 0.001f );
	}
	float GetAverageTicks() const
	{
		return float( m_numTimeBaseTicks / m_numCalls );
	}
	friend const CMicroProfiler operator + ( const CMicroProfiler &left, const CMicroProfiler &right );
	
	void Accumulate( const CMicroProfiler &other )
	{
		m_numCalls += other.m_numCalls;
		m_numTimeBaseTicks += other.m_numTimeBaseTicks;
	}
};

inline const CMicroProfiler operator + ( const CMicroProfiler &left, const CMicroProfiler &right )
{
	CMicroProfiler result;
	result.m_numCalls = left.m_numCalls + right.m_numCalls;
	result.m_numTimeBaseTicks = left.m_numTimeBaseTicks + right.m_numTimeBaseTicks;
	return result;
}

class CMicroProfilerGuard: public CMicroProfilerSample
{
	CMicroProfiler *m_pProfiler;
public:
	CMicroProfilerGuard( CMicroProfiler *pProfiler )
	{
		m_pProfiler = pProfiler;
	}
	~CMicroProfilerGuard()
	{
		m_pProfiler->Add( GetElapsed() );
	}
};

class CMicroProfilerGuardWithCount: public CMicroProfilerSample
{
	CMicroProfiler *m_pProfiler;
	uint m_nCount;
public:
	CMicroProfilerGuardWithCount( CMicroProfiler *pProfiler, uint nCount )
	{
		m_nCount = nCount;
		m_pProfiler = pProfiler;
	}
	void OverrideCount( uint nCount ) { m_nCount = nCount; }
	~CMicroProfilerGuardWithCount( )
	{
		m_pProfiler->Add( GetElapsed( ), m_nCount );
	}
};

// thread-safe variant of the same profiler
class CMicroProfilerGuardTS: public CMicroProfilerSample
{
	CMicroProfiler *m_pProfiler;
public:
	CMicroProfilerGuardTS( CMicroProfiler *pProfiler )
	{
		m_pProfiler = pProfiler;
	}
	~CMicroProfilerGuardTS()
	{
		m_pProfiler->AddTS( GetElapsed() );
	}
};

}
