// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.tier1:rangecheckedvar;

import std;
import ks.math;
export import :reflect_wirevar;

export extern "C++" {

// Use this to disable range checks within a scope.
class CDisableRangeChecks
{
public:
	CDisableRangeChecks();
	~CDisableRangeChecks();
};

template< class T >
inline void RangeCheck( const T &value, int minValue, int maxValue )
{
}

inline void RangeCheck( const Vector &value, int minValue, int maxValue )
{
}

// The wire payload is the single data member this class declares; see reflect_wirevar.h.
template< class T, int minValue, int maxValue, int startValue >
class [[= ks::reflect::WireVar{} ]] CRangeCheckedVar
{
public:

	inline CRangeCheckedVar()
	{
		m_Val = startValue;
	}

	inline CRangeCheckedVar( const T &value )
	{
		*this = value;
	}

	// Clamp the value to its limits. Interpolation code uses this after interpolating.
	inline void Clamp()
	{
		if ( m_Val < minValue )
			m_Val = minValue;
		else if ( m_Val > maxValue )
			m_Val = maxValue;
	}

	inline operator const T&() const
	{
		return m_Val;
	}

	inline CRangeCheckedVar<T, minValue, maxValue, startValue>& operator=( const T &value )
	{
		RangeCheck( value, minValue, maxValue );
		m_Val = value;
		return *this;
	}

	inline CRangeCheckedVar<T, minValue, maxValue, startValue>& operator+=( const T &value )
	{
		return (*this = m_Val + value);
	}

	inline CRangeCheckedVar<T, minValue, maxValue, startValue>& operator-=( const T &value )
	{
		return (*this = m_Val - value);
	}

	inline CRangeCheckedVar<T, minValue, maxValue, startValue>& operator*=( const T &value )
	{
		return (*this = m_Val * value);
	}

	inline CRangeCheckedVar<T, minValue, maxValue, startValue>& operator/=( const T &value )
	{
		return (*this = m_Val / value);
	}

private:

	T m_Val;
};

}
