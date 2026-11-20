#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.platform;
import ks.simd;
#include "cmodel.h"
import std;
#include "ihandleentity.h"
#include "ispatialpartition.h"

//-----------------------------------------------------------------------------
// Purpose: A trace is returned when a box is swept through the world
// NOTE: eventually more of this class should be moved up into the base class!!
//-----------------------------------------------------------------------------
class CGameTrace : public CBaseTrace
{
public:

	// What the trace hit depends on the game's entity list: TraceDidHitWorld() and friends
	// (util_shared.h) answer it.

	// Returns true if there was any kind of impact at all
	bool DidHit() const;

	template < class E > E *Ent() const { return reinterpret_cast< E * >( m_pEnt ); }


public:

	float			fractionleftsolid;	// time we left a solid, only valid if we started in solid
	csurface_t		surface;			// surface hit (impact surface)

	int				hitgroup;			// 0 == generic, non-zero is specific body part

	short			physicsbone;		// physics bone hit by trace in studio
	unsigned short	worldSurfaceIndex;	// Index of the msurface2_t, if applicable

	void			*m_pEnt;

	// NOTE: this member is overloaded.
	// If hEnt points at the world entity, then this is the static prop index.
	// Otherwise, this is the hitbox index.
	int			hitbox;					// box hit by trace in studio

	CGameTrace() : m_pEnt(nullptr) {}
	CGameTrace( CGameTrace && ) = default;	// std::vector relocation; copies stay private
	CGameTrace &operator=( CGameTrace && ) = default;
	CGameTrace &operator=( const CGameTrace & ) = default;

private:
	// No copy constructors allowed
	CGameTrace(const CGameTrace& vOther);
};


//-----------------------------------------------------------------------------
// Returns true if there was any kind of impact at all
//-----------------------------------------------------------------------------
inline bool CGameTrace::DidHit() const 
{ 
	return fraction < 1 || allsolid || startsolid; 
}


typedef CGameTrace trace_t;

//=============================================================================

class ITraceListData
{
public:
	virtual ~ITraceListData() {}

	virtual void Reset() = 0;
	virtual bool IsEmpty() = 0;
	// CanTraceRay will return true if the current volume encloses the ray
	// NOTE: The leaflist trace will NOT check this.  Traces are intersected
	// against the culled volume exclusively.
	virtual bool CanTraceRay( const Ray_t &ray ) = 0;
};
