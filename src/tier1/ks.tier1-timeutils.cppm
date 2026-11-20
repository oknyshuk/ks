// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2004 Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.tier1.timeutils;

import std;
import ks.core.basetypes;
import ks.core.types;
import ks.platform;
import ks.simd;

export extern "C++" {

enum RoundStyle_t
{
	ROUND_NEAREST,
	ROUND_DOWN,
	ROUND_UP,
};

namespace ks
{
	class byte_reader;
	class byte_writer;
	class text_reader;
	class text_writer;
}

class DmeTime_t;

class DmeFramerate_t
{
public:
	DmeFramerate_t( float fps );
	DmeFramerate_t( int fps = 0 );
	DmeFramerate_t( const DmeFramerate_t& src ) : m_num( src.m_num ), m_den( src.m_den ) {}

	void SetFramerate( float flFrameRate );
	void SetFramerate( int fps );
	// other (uncommon) options besides 30(29.97 - ntsc video) are 24 (23.976 - ntsc film) and 60 (59.94 - ntsc progressive)
	void SetFramerateNTSC( int multiplier = 30 );

	float GetFramesPerSecond() const;
	DmeTime_t GetTimePerFrame() const;

	bool operator==( DmeFramerate_t f ) const { return m_num == f.m_num && m_den == f.m_den; }
	bool operator!=( DmeFramerate_t f ) const { return m_num != f.m_num && m_den != f.m_den; }
	bool operator< ( DmeFramerate_t f ) const { return m_num * ( int )f.m_den <  f.m_num * ( int )m_den; }
	bool operator> ( DmeFramerate_t f ) const { return m_num * ( int )f.m_den >  f.m_num * ( int )m_den; }
	bool operator<=( DmeFramerate_t f ) const { return m_num * ( int )f.m_den <= f.m_num * ( int )m_den; }
	bool operator>=( DmeFramerate_t f ) const { return m_num * ( int )f.m_den >= f.m_num * ( int )m_den; }

	DmeFramerate_t operator*( int i ) const { return DmeFramerate_t( m_num * i, m_den ); }
	DmeFramerate_t operator/( int i ) const { return DmeFramerate_t( m_num, m_den * i ); }

	DmeFramerate_t operator*=( int i ) { contract_assert( abs( m_num * i ) <= std::numeric_limits<unsigned short>::max() ); m_num *= ( unsigned short )i; return *this; }
	DmeFramerate_t operator/=( int i ) { contract_assert( abs( m_den * i ) <= std::numeric_limits<unsigned short>::max() ); m_den *= ( unsigned short )i; return *this; }

private:
	DmeFramerate_t( int nNumerator, int nDenominator );

	unsigned short m_num;
	unsigned short m_den;

	friend class DmeTime_t;
};

class DmeTime_t
{
public:
	constexpr DmeTime_t() : m_tms( std::numeric_limits<int>::min() ) {} // invalid time
	constexpr explicit DmeTime_t( int tms ) : m_tms( tms ) {}
	explicit DmeTime_t( float  sec ) : m_tms( RoundSecondsToTMS( sec ) ) {}
	explicit DmeTime_t( double sec ) : m_tms( RoundSecondsToTMS( sec ) ) {}
	DmeTime_t( int frame, DmeFramerate_t framerate );

	// time operators

	friend bool operator==( DmeTime_t a, DmeTime_t b ) { return a.m_tms == b.m_tms; }
	friend bool operator!=( DmeTime_t a, DmeTime_t b ) { return a.m_tms != b.m_tms; }
	friend bool operator< ( DmeTime_t a, DmeTime_t b ) { return a.m_tms <  b.m_tms; }
	friend bool operator> ( DmeTime_t a, DmeTime_t b ) { return a.m_tms >  b.m_tms; }
	friend bool operator<=( DmeTime_t a, DmeTime_t b ) { return a.m_tms <= b.m_tms; }
	friend bool operator>=( DmeTime_t a, DmeTime_t b ) { return a.m_tms >= b.m_tms; }

	friend DmeTime_t operator%( DmeTime_t a, DmeTime_t b ) { return DmeTime_t( a.m_tms % b.m_tms ); }
	friend DmeTime_t operator+( DmeTime_t a, DmeTime_t b ) { return DmeTime_t( a.m_tms + b.m_tms ); }
	friend DmeTime_t operator-( DmeTime_t a, DmeTime_t b ) { return DmeTime_t( a.m_tms - b.m_tms ); }

	DmeTime_t operator-() const { return DmeTime_t( -m_tms ); }

	DmeTime_t operator+=( DmeTime_t t ) { m_tms += t.m_tms; return *this; }
	DmeTime_t operator-=( DmeTime_t t ) { m_tms -= t.m_tms; return *this; }

	// float operators - comment these out to find potentially incorrect uses of DmeTime_t

	friend DmeTime_t operator*( DmeTime_t t, float f ) { t *= f; return t; }
	friend DmeTime_t operator*( float f, DmeTime_t t ) { t *= f; return t; }
	friend DmeTime_t operator/( DmeTime_t t, float f ) { t /= f; return t; }
	friend float     operator/( DmeTime_t n, DmeTime_t d ) { return float( n.m_tms / double( d.m_tms ) ); }

	DmeTime_t operator*=( float f );
	DmeTime_t operator/=( float f );

	DmeTime_t operator++() { ++m_tms; return *this; }
	DmeTime_t operator--() { --m_tms; return *this; }
	DmeTime_t operator++( int ) { DmeTime_t t = *this; ++m_tms; return t; } // postfix version..
	DmeTime_t operator--( int ) { DmeTime_t t = *this; --m_tms; return t; } // postfix version..

	// limits, special values and validity

	bool IsValid() const { return m_tms != std::numeric_limits<int>::min(); }

	static DmeTime_t InvalidTime() { return DmeTime_t( std::numeric_limits<int>::min() ); }
	static DmeTime_t MinTime() { return DmeTime_t( std::numeric_limits<int>::min() + 1 ); }
	static DmeTime_t MaxTime() { return DmeTime_t( std::numeric_limits<int>::max() ); }
	static DmeTime_t MinTimeDelta() { return DmeTime_t( 1 ); }

	// conversions between other time representations

	int GetTenthsOfMS() const { return m_tms; }
	float GetSeconds() const { return m_tms * 0.0001f; }
	void SetTenthsOfMS( int tms ) { m_tms = tms; }
	void SetSeconds( float sec ) { m_tms = RoundSecondsToTMS( sec ); }

	// helper methods

	void Clamp( DmeTime_t lo, DmeTime_t hi );
	bool IsInRange( DmeTime_t lo, DmeTime_t hi ) const;

	// helper functions

	friend float GetFractionOfTimeBetween( DmeTime_t t, DmeTime_t start, DmeTime_t end, bool bClamp );
	friend float GetFractionOfTime( DmeTime_t t, DmeTime_t duration, bool bClamp );
	friend int FrameForTime( DmeTime_t t, DmeFramerate_t framerate );

	// standard arithmetic methods

	friend DmeTime_t abs( DmeTime_t t ) { return t.m_tms >= 0 ? t : -t; }

	// framerate-dependent conversions to/from frames

	int CurrentFrame( DmeFramerate_t framerate, RoundStyle_t roundStyle = ROUND_DOWN ) const;
	DmeTime_t TimeAtCurrentFrame( DmeFramerate_t framerate, RoundStyle_t roundStyle = ROUND_DOWN ) const;
	DmeTime_t TimeAtNextFrame( DmeFramerate_t framerate ) const;
	DmeTime_t TimeAtPrevFrame( DmeFramerate_t framerate ) const;

private:
	explicit DmeTime_t( int64 tms ) : m_tms( int( tms ) ) {}

	// conversion helper methods - implementation
	static int RoundSecondsToTMS( float sec );
	static int RoundSecondsToTMS( double sec );

	int m_tms;
};
inline constexpr DmeTime_t DMETIME_ZERO = DmeTime_t( 0 );

float GetFractionOfTimeBetween( DmeTime_t t, DmeTime_t start, DmeTime_t end, bool bClamp = false );
float GetFractionOfTime( DmeTime_t t, DmeTime_t duration, bool bClamp = false );

bool Serialize( ks::byte_writer &buf, const DmeTime_t &src );
bool Unserialize( ks::byte_reader &buf, DmeTime_t &dest );
bool Serialize( ks::text_writer &buf, const DmeTime_t &src );
bool Unserialize( ks::text_reader &buf, DmeTime_t &dest );

}
