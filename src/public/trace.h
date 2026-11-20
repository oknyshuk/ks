#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.vector;
import ks.platform;
import ks.simd;

// Note: These flags need to match the bspfile.h DISPTRI_TAG_* flags.
inline constexpr auto DISPSURF_FLAG_SURFACE = (1<<0);
inline constexpr auto DISPSURF_FLAG_WALKABLE = (1<<1);
inline constexpr auto DISPSURF_FLAG_BUILDABLE = (1<<2);
inline constexpr auto DISPSURF_FLAG_SURFPROP1 = (1<<3);
inline constexpr auto DISPSURF_FLAG_SURFPROP2 = (1<<4);
inline constexpr auto DISPSURF_FLAG_SURFPROP3 = (1<<5);
inline constexpr auto DISPSURF_FLAG_SURFPROP4 = (1<<6);

//=============================================================================
// Base Trace Structure
// - shared between engine/game dlls and tools (vrad)
//=============================================================================

class CBaseTrace
{
public:

	// Displacement flags tests.
	bool IsDispSurface( void )				{ return ( ( dispFlags & DISPSURF_FLAG_SURFACE ) != 0 ); }
	bool IsDispSurfaceWalkable( void )		{ return ( ( dispFlags & DISPSURF_FLAG_WALKABLE ) != 0 ); }
	bool IsDispSurfaceBuildable( void )		{ return ( ( dispFlags & DISPSURF_FLAG_BUILDABLE ) != 0 ); }
	bool IsDispSurfaceProp1( void )			{ return ( ( dispFlags & DISPSURF_FLAG_SURFPROP1 ) != 0 ); }
	bool IsDispSurfaceProp2( void )			{ return ( ( dispFlags & DISPSURF_FLAG_SURFPROP2 ) != 0 ); }
	bool IsDispSurfaceProp3( void )			{ return ( ( dispFlags & DISPSURF_FLAG_SURFPROP3 ) != 0 ); }
	bool IsDispSurfaceProp4( void )			{ return ( ( dispFlags & DISPSURF_FLAG_SURFPROP4 ) != 0 ); }

public:

	// these members are aligned!!
	Vector			startpos;				// start position
	Vector			endpos;					// final position
	cplane_t		plane;					// surface normal at impact

	float			fraction;				// time completed, 1.0 = didn't hit anything

	int				contents;				// contents on other side of surface hit
	unsigned short	dispFlags;				// displacement flags for marking surfaces with data

	bool			allsolid;				// if true, plane is not valid
	bool			startsolid;				// if true, the initial point was in a solid area

	CBaseTrace() {}
	CBaseTrace( CBaseTrace && ) = default;	// std::vector relocation; copies stay private
	CBaseTrace &operator=( CBaseTrace && ) = default;
	CBaseTrace &operator=( const CBaseTrace & ) = default;

private:
	// No copy constructors allowed
	CBaseTrace(const CBaseTrace& vOther);
};
