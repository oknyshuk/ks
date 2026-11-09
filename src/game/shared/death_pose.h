#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "ai_activity.h"

class CBaseAnimating;
class C_BaseAnimating;
class CTakeDamageInfo;

enum
{
	DEATH_FRAME_HEAD = 1,
	DEATH_FRAME_STOMACH,
	DEATH_FRAME_LEFTARM,
	DEATH_FRAME_RIGHTARM,
	DEATH_FRAME_LEFTLEG,
	DEATH_FRAME_RIGHTLEG,
	MAX_DEATHPOSE_FRAMES  = DEATH_FRAME_RIGHTLEG
};

#ifdef CLIENT_DLL

void GetRagdollCurSequenceWithDeathPose( C_BaseAnimating *entity, matrix3x4a_t *curBones, float flTime, int activity, int frame );

#else // !CLIENT_DLL

/// Calculates death pose activity and frame
void SelectDeathPoseActivityAndFrame( CBaseAnimating *entity, const CTakeDamageInfo &info, int hitgroup, Activity& activity, int& frame );

#endif // !CLIENT_DLL
