#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "reflect_annotations.h"

#include "entityoutput.h"
#include "fire_smoke.h"
#include "plasma.h"

//Spawnflags
inline constexpr auto SF_FIRE_INFINITE = 0x00000001;
inline constexpr auto SF_FIRE_SMOKELESS = 0x00000002;
inline constexpr auto SF_FIRE_START_ON = 0x00000004;
inline constexpr auto SF_FIRE_START_FULL = 0x00000008;
inline constexpr auto SF_FIRE_DONT_DROP = 0x00000010;
inline constexpr auto SF_FIRE_NO_GLOW = 0x00000020;
inline constexpr auto SF_FIRE_DIE_PERMANENT = 0x00000080;
inline constexpr auto SF_FIRE_VISIBLE_FROM_ABOVE = 0x00000100;

//==================================================
// CFire
//==================================================

enum fireType_e
{
	FIRE_NATURAL = 0,
	FIRE_PLASMA,
};

//==================================================

// NPCs and grates do not prevent fire from travelling
inline constexpr auto MASK_FIRE_SOLID = ( MASK_SOLID & (~(CONTENTS_MONSTER|CONTENTS_GRATE)) );

//==================================================
// FireSystem
//==================================================
bool FireSystem_StartFire( const Vector &position, float fireHeight, float attack, float fuel, int flags, CBaseEntity *owner, fireType_e type = FIRE_NATURAL);
void FireSystem_ExtinguishInRadius( const Vector &origin, float radius, float rate );
void FireSystem_AddHeatInRadius( const Vector &origin, float radius, float heat );

bool FireSystem_GetFireDamageDimensions( CBaseEntity *pFire, Vector *pFireMins, Vector *pFireMaxs );
