#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation


import std;
import ks.math;

enum simplekeyinterp_t
{
	KEY_LINEAR = 0,
	KEY_SPLINE,
	KEY_ACCELERATE,
	KEY_DECELERATE,
};

class CSimpleKeyInterp : public Vector
{
public:
	CSimpleKeyInterp( float t, simplekeyinterp_t interp, float x, float y = 0, float z = 0 ) : Vector( x, y, z ) 
	{
		m_interp = interp;
		m_keyTime = t;
	}
	
	float GetTime() const { return m_keyTime; }
	
	// out = t*start + (1-t) * end (may be splinear or linear)
	static void Interp( Vector &out, float t, const CSimpleKeyInterp &start, const CSimpleKeyInterp &end );

	float				m_keyTime;
	simplekeyinterp_t	m_interp;
};


class CSimpleKeyList
{
public:
	int		Insert( const CSimpleKeyInterp &key );
	bool	Interp( Vector &out, float t );

	std::vector< CSimpleKeyInterp >	m_list;
};
