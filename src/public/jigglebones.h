#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.tier1;
#include "studio.h"
import std;

//-----------------------------------------------------------------------------
/**
 * JiggleData is the instance-specific data for a jiggle bone
 */
struct JiggleData
{
	void Init( int initBone, float currenttime, const Vector &initBasePos, const Vector &initTipPos )
	{
		bone = initBone;

		lastUpdate = currenttime;

		basePos = initBasePos;
		baseLastPos = basePos;
		baseVel.Init();
		baseAccel.Init();

		tipPos = initTipPos;
		tipVel.Init();
		tipAccel.Init();
	}

	int bone;
	int id;

	float lastUpdate;	// based on gpGlobals->realtime

	Vector basePos;		// position of the base of the jiggle bone
	Vector baseLastPos;
	Vector baseVel;
	Vector baseAccel;

	Vector tipPos;		// position of the tip of the jiggle bone
	Vector tipVel;
	Vector tipAccel;
};

class CJiggleBones
{
public:
	JiggleData * GetJiggleData( int bone, float currenttime, const Vector &initBasePos, const Vector &initTipPos );
	void BuildJiggleTransformations( int boneIndex, float currentime, const mstudiojigglebone_t *jiggleParams, const matrix3x4_t &goalMX, matrix3x4_t &boneMX, bool coordSystemIsFlipped );

	std::vector< JiggleData >	m_jiggleBoneState;
};


extern void DevMsgRT( char const* pMsg, ... );
