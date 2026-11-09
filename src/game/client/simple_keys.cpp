// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "rocketui/rmlui.h"
#include "predictable_entity.h"
#include "vphysics_interface.h"
#include "simple_keys.h"

//-----------------------------------------------------------------------------
// Simple key interpolations
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &out - 
//			t - 
//			&start - 
//			&end - 
//-----------------------------------------------------------------------------
void CSimpleKeyInterp::Interp( Vector &out, float t, const CSimpleKeyInterp &start, const CSimpleKeyInterp &end )
{
	float delta = end.GetTime() - start.GetTime();
	t = clamp( t-start.GetTime(), 0, delta );

	float unitT = (delta > 0) ? (t / delta) : 1;

	switch( end.m_interp )
	{
	case KEY_SPLINE:
		unitT = SimpleSpline( unitT );
		break;
	case KEY_ACCELERATE:
		unitT *= unitT;
		break;
	case KEY_DECELERATE:
		unitT = std::sqrt(unitT);
		break;
	default:
	case KEY_LINEAR:
		//unitT = unitT;
		break;
	}
	out = (1-unitT) * ((Vector)start) + unitT * ((Vector)end);
}

//-----------------------------------------------------------------------------
// Simple key list
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &key - 
// Output : int
//-----------------------------------------------------------------------------
int CSimpleKeyList::Insert( const CSimpleKeyInterp &key )
{
	for ( int i = 0; i < std::ssize( m_list ); i++ )
	{
		if ( key.GetTime() < m_list[i].GetTime() )
			m_list.insert( m_list.begin() + i, key ); return i;
	}

	m_list.push_back( key ); return (int)std::ssize( m_list ) - 1;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &out - 
//			t - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CSimpleKeyList::Interp( Vector &out, float t )
{
	int startIndex = -1;

	out.Init();
	for ( int i = 0; i < std::ssize( m_list ); i++ )
	{
		if ( t < m_list[i].GetTime() )
		{
			// before start
			if ( startIndex < 0 )
				return false;
			CSimpleKeyInterp::Interp( out, t, m_list[startIndex], m_list[i] );
			return true;
		}
		startIndex = i;
	}

	// past end
	return false;
}
