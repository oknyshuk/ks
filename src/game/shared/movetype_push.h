#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.math;
#include "ehandle.h"
#ifdef CLIENT_DLL
class C_BaseEntity;
typedef CHandle<C_BaseEntity> EHANDLE;
#else
class CBaseEntity;
typedef CHandle<CBaseEntity> EHANDLE;
#endif

const int MAX_PUSHED_ENTITIES = 32;
struct physicspushlist_t
{
	float	localMoveTime;
	Vector	localOrigin;
	QAngle	localAngles;
	int		pushedCount;
	EHANDLE	pushedEnts[MAX_PUSHED_ENTITIES];
	Vector	pushVec[MAX_PUSHED_ENTITIES];
};
