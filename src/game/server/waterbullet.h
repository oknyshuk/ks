#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Entity that simulates bullets that are underwater.

#include "reflect_annotations.h"
#include "baseanimating.h"

inline constexpr auto WATER_BULLET_BUBBLES_PER_INCH = 0.05f;

//=========================================================
//=========================================================
class [[= ks::reflect::NetTable{ .name = "DT_WaterBullet" } ]]
      CWaterBullet : public CBaseAnimating
{
	DECLARE_CLASS( CWaterBullet, CBaseAnimating );

public:
	void Precache();
	void Spawn( const Vector &vecOrigin, const Vector &vecDir );
	void Touch( CBaseEntity *pOther );
	void BulletThink();

	DECLARE_SERVERCLASS();
};
