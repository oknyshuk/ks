#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.math;

inline float CalcDistance( float x, float y )
{
	return x - y;
}

inline float CalcDistance( const Vector &a, const Vector &b )
{
	return a.DistTo( b );
}


template<class T>
class CDefaultCalcDistance
{
public:
	static inline float CalcDistance( const T &a, const T &b )
	{
		return ::CalcDistance( a, b );
	}
};

class CCalcDistance2D
{
public:
	static inline float CalcDistance( const Vector &a, const Vector &b )
	{
		return (a-b).Length2D();
	}
};


template< class T, class Functor=CDefaultCalcDistance<T> >
class CApparentVelocity
{
public:
	CApparentVelocity()
	{
		m_LastTime = -1;
	}

	float AddSample( float time, T value )
	{
		float flRet = 0;
		if ( m_LastTime != -1 )
		{
			flRet = Functor::CalcDistance(value, m_LastValue) / (time - m_LastTime);
		}

		m_LastTime = time;
		m_LastValue = value;

		return flRet;
	}

private:
	T m_LastValue;
	float m_LastTime;
};
