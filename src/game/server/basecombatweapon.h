#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "entityoutput.h"
#include "basecombatweapon_shared.h"

//-----------------------------------------------------------------------------
// Bullet types
//-----------------------------------------------------------------------------

// -----------------------------------------
//	Sounds
// -----------------------------------------

struct animevent_t;

extern void	SpawnBlood(Vector vecSpot, const Vector &vecDir, int bloodColor, float flDamage);
