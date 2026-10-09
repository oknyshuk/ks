// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "rocketui/rmlui.h"
import ks.core;
import std;
#include "predictable_entity.h"
#include "util_shared.h"
#include "baseentity_shared.h"
#include "precache_register.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "reflect_recvtable.h"
#include "c_physbox.h"

IMPLEMENT_REFLECT_CLIENTCLASS( C_PhysBox, DT_PhysBox, CPhysBox )


C_PhysBox::C_PhysBox()
{
}

//-----------------------------------------------------------------------------
// Should this object cast shadows?
//-----------------------------------------------------------------------------
ShadowType_t C_PhysBox::ShadowCastType()
{
	if (IsEffectActive(EF_NODRAW | EF_NOSHADOW))
		return SHADOWS_NONE;
	return SHADOWS_RENDER_TO_TEXTURE;
}

C_PhysBox::~C_PhysBox()
{
}
