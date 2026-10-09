// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

// The tier0 timers.
//
// Everything defined here is inline on purpose: in a module interface a member function defined
// inside its class is not implicitly inline the way it is in a header, and these are called on
// the hottest paths in the engine.

export module ks.core:time;

import :platform;

// Monotonic nanoseconds, defined in tier0/fasttimer.cpp. Out of line so that <chrono> stays out
// of every importer.
export uint64 Plat_SteadyNanos();

// A count is a duration in nanoseconds, not a hardware cycle count: the CPU's advertised clock is not the
// rate a counter runs at on parts with turbo or boost.

export class CCycleCount
{
	friend class CFastTimer;

public:
					CCycleCount();
					CCycleCount( uint64 cycles );

	void			Sample();	// Sample the clock. This takes about 34 clocks to execute (or 26,000 calls per millisecond on a P900).

	void			Init();		// Set to zero.
	void			Init( float initTimeMsec );
	inline void		Init( double initTimeMsec )		{ Init( (float)initTimeMsec ); }
	void			Init( uint64 cycles );
	bool			IsLessThan( CCycleCount const &other ) const;					// Compare two counts.

	// Convert to other time representations. These functions are slow, so it's preferable to call them
	// during display rather than inside a timing block.
	unsigned long	GetCycles()  const;
	uint64			GetLongCycles() const;

	unsigned long	GetMicroseconds() const;
	uint64			GetUlMicroseconds() const;
	double			GetMicrosecondsF() const; 	
	void			SetMicroseconds( unsigned long nMicroseconds );

	unsigned long	GetMilliseconds() const;
	double			GetMillisecondsF() const;

	double			GetSeconds() const;

	CCycleCount&	operator+=( CCycleCount const &other );

	// dest = rSrc1 + rSrc2
	static void		Add( CCycleCount const &rSrc1, CCycleCount const &rSrc2, CCycleCount &dest );	// Add two samples together.
	
	// dest = rSrc1 - rSrc2
	static void		Sub( CCycleCount const &rSrc1, CCycleCount const &rSrc2, CCycleCount &dest );	// Add two samples together.

	static uint64	GetTimestamp();

	uint64			m_Int64;
};

export class CFastTimer
{
public:
	// These functions are fast to call and should be called from your sampling code.
	void				Start();
	void				End();

	const CCycleCount &	GetDuration() const;	// Get the elapsed time between Start and End calls.
	CCycleCount 		GetDurationInProgress() const; // Call without ending. Not that cheap.

private:
	CCycleCount	m_Duration;
};

// This is a helper class that times whatever block of code it's in
export class CTimeScope
{
public:
				CTimeScope( CFastTimer *pTimer );
				~CTimeScope();

private:	
	CFastTimer	*m_pTimer;
};

inline CTimeScope::CTimeScope( CFastTimer *pTotal )
{
	m_pTimer = pTotal;
	m_pTimer->Start();
}

inline CTimeScope::~CTimeScope()
{
	m_pTimer->End();
}

// -------------------------------------------------------------------------- // 

// -------------------------------------------------------------------------- // 

// -------------------------------------------------------------------------- // 
// CCycleCount inlines.
// -------------------------------------------------------------------------- // 

inline CCycleCount::CCycleCount()
{
	Init( (uint64)0 );
}

inline CCycleCount::CCycleCount( uint64 cycles )
{
	Init( cycles );
}

inline void CCycleCount::Init()
{
	Init( (uint64)0 );
}

inline void CCycleCount::Init( float initTimeMsec )
{
	Init( (uint64)( (double)initTimeMsec * 1000000.0 ) );
}

inline void CCycleCount::Init( uint64 cycles )
{
	m_Int64 = cycles;
}

inline void CCycleCount::Sample()
{
	m_Int64 = Plat_SteadyNanos();
}

inline CCycleCount& CCycleCount::operator+=( CCycleCount const &other )
{
	m_Int64 += other.m_Int64;
	return *this;
}

inline void CCycleCount::Add( CCycleCount const &rSrc1, CCycleCount const &rSrc2, CCycleCount &dest )
{
	dest.m_Int64 = rSrc1.m_Int64 + rSrc2.m_Int64;
}

inline void CCycleCount::Sub( CCycleCount const &rSrc1, CCycleCount const &rSrc2, CCycleCount &dest )
{
	dest.m_Int64 = rSrc1.m_Int64 - rSrc2.m_Int64;
}

inline uint64 CCycleCount::GetTimestamp()
{
	CCycleCount c;
	c.Sample();
	return c.GetLongCycles();
}

inline bool CCycleCount::IsLessThan(CCycleCount const &other) const
{
	return m_Int64 < other.m_Int64;
}

inline unsigned long CCycleCount::GetCycles() const
{
	return (unsigned long)m_Int64;
}

inline uint64 CCycleCount::GetLongCycles() const
{
	return m_Int64;
}

// m_Int64 is nanoseconds, so each conversion is a fixed divisor.
inline unsigned long CCycleCount::GetMicroseconds() const
{
	return (unsigned long)( m_Int64 / 1000 );
}

inline uint64 CCycleCount::GetUlMicroseconds() const
{
	return m_Int64 / 1000;
}

inline double CCycleCount::GetMicrosecondsF() const
{
	return (double)m_Int64 / 1.0e3;
}

inline void	CCycleCount::SetMicroseconds( unsigned long nMicroseconds )
{
	m_Int64 = (uint64)nMicroseconds * 1000;
}

inline unsigned long CCycleCount::GetMilliseconds() const
{
	return (unsigned long)( m_Int64 / 1000000 );
}

inline double CCycleCount::GetMillisecondsF() const
{
	return (double)m_Int64 / 1.0e6;
}

inline double CCycleCount::GetSeconds() const
{
	return (double)m_Int64 / 1.0e9;
}

// -------------------------------------------------------------------------- // 
// CFastTimer inlines.
// -------------------------------------------------------------------------- // 
inline void CFastTimer::Start()
{
	m_Duration.Sample();
}

inline void CFastTimer::End()
{
	CCycleCount cnt;
	cnt.Sample();

	m_Duration.m_Int64 = cnt.m_Int64 - m_Duration.m_Int64;

}

inline CCycleCount CFastTimer::GetDurationInProgress() const
{
	CCycleCount cnt;
	cnt.Sample();

	CCycleCount result;
	result.m_Int64 = cnt.m_Int64 - m_Duration.m_Int64;
	
	return result;
}

inline CCycleCount const& CFastTimer::GetDuration() const
{
	return m_Duration;
}

// -------------------------------------------------------------------------- // 
// CAverageCycleCounter inlines

// -------------------------------------------------------------------------- // 

// CLimitTimer
// Use this to time whether a desired interval of time has passed.  It's extremely fast
// to check while running.  NOTE: CMicroSecOverage() and CMicroSecLeft() are not as fast to check.
export class CLimitTimer
{
public:
	CLimitTimer() {}
	CLimitTimer( uint64 cMicroSecDuration ) { SetLimit( cMicroSecDuration ); }
	void SetLimit( uint64 m_cMicroSecDuration );
	bool BLimitReached() const;

	int CMicroSecOverage() const;
	uint64 CMicroSecLeft() const; 

private:
	uint64 m_lLimit;      // absolute, in nanoseconds, like the counts it is compared against
};

//-----------------------------------------------------------------------------
// Purpose: Initializes the limit timer with a period of time to measure.
// Input  : cMicroSecDuration -		How long a time period to measure
//-----------------------------------------------------------------------------
inline void CLimitTimer::SetLimit( uint64 cMicroSecDuration )
{
	CCycleCount now;
	now.Sample();
	m_lLimit = now.GetLongCycles() + cMicroSecDuration * 1000;   // microseconds -> nanoseconds
}

//-----------------------------------------------------------------------------
// Purpose: Determines whether our specified time period has passed
// Output:	true if at least the specified time period has passed
//-----------------------------------------------------------------------------
inline bool CLimitTimer::BLimitReached() const
{
	CCycleCount now;
	now.Sample();
	return ( now.GetLongCycles() >= m_lLimit );
}
