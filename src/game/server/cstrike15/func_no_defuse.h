#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 2011 Valve Corporation

// Purpose: No defuse area ent

#include "shareddefs.h"
#include "util.h"
#include "predictable_entity.h"
#include "variant_t.h"
#include "takedamageinfo.h"
#include "touchlink.h"
#include "groundlink.h"
#include "baseentity_shared.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "triggers.h"

class CNoDefuseArea : public CBaseTrigger
{
public:
	DECLARE_CLASS( CNoDefuseArea, CBaseTrigger );

	void Spawn_Impl();
	void NoDefuseAreaTouch( CBaseEntity* pOther );
};
