// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2007 Valve Corporation

// Purpose: An entity that allows level designer control over the fog parameters.

#include "rocketui/rmlui.h"
#include "predictable_entity.h"
#include "util_shared.h"
#include "baseentity_shared.h"
#include "c_basecombatweapon.h"
#include "c_basecombatcharacter.h"
#include "c_baseplayer.h"
#include "c_env_fog_controller.h"
#include "reflect_recvtable.h"

IMPLEMENT_NETWORKCLASS_ALIASED( FogController, DT_FogController )

//-----------------------------------------------------------------------------
// Datatable
//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_TABLE( C_FogController, DT_FogController );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_FogController::C_FogController()
{
	// Make sure that old maps without fog fields don't get wacked out fog values.
	m_fog.enable = false;
	m_fog.maxdensity = 1.0f;
	m_fog.HDRColorScale = 1.0f;
}
