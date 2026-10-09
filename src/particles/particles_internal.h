#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2006 Valve Corporation

// Purpose: sheet code for particles and other sprite functions

import ks.math;
#include "tier2/fileutils.h"
import std;

class CParticleCollection;

inline constexpr auto MAX_WORLD_PLANAR_CONSTRAINTS = ( 26 + 5 + 10 );

inline constexpr auto COLLISION_MODE_PER_PARTICLE_TRACE = 0;
inline constexpr auto COLLISION_MODE_PER_FRAME_PLANESET = 1;
inline constexpr auto COLLISION_MODE_INITIAL_TRACE_DOWN = 2;
inline constexpr auto COLLISION_MODE_USE_NEAREST_TRACE = 3;

inline constexpr auto PARTICLEBLEND_DEFAULT = 0;
inline constexpr auto PARTICLEBLEND_OVERLAY = 1;
inline constexpr auto PARTICLEBLEND_DARKEN = 2;
inline constexpr auto PARTICLEBLEND_LIGHTEN = 3;
inline constexpr auto PARTICLEBLEND_MULTIPLY = 4;

struct CWorldCollideContextData
{
	FourVectors m_TraceStartPnt[MAX_WORLD_PLANAR_CONSTRAINTS];
	FourVectors m_TraceEndPnt[MAX_WORLD_PLANAR_CONSTRAINTS];
	FourVectors m_PointOnPlane[MAX_WORLD_PLANAR_CONSTRAINTS];
	FourVectors m_PlaneNormal[MAX_WORLD_PLANAR_CONSTRAINTS];

	int m_nActivePlanes;
	int m_nNumFixedPlanes;
	float m_flLastUpdateTime;
	Vector m_vecLastUpdateOrigin;
	bool m_bPlaneActive[MAX_WORLD_PLANAR_CONSTRAINTS];

	void *operator new( std::size_t nSize );
	void *operator new( std::size_t nSize, int nBlockUse, const char *pFileName, int nLine );
	void operator delete(void *pData);
	void operator delete( void* p, int nBlockUse, const char *pFileName, int nLine );

	void SetBaseTrace(  int nIndex, Vector const &rayStart, Vector const &traceDir, int nCollisionGroup, bool bKeepMisses );

	void CalculatePlanes( CParticleCollection *pParticles, int nCollisionMode, int nCollisionGroupNumber,
						  Vector const *pCpOffset = nullptr, float flMovementTolerance = 0.  );
};

// This is defined in the owner DLL
extern bool UTIL_IsDedicatedServer( void );
