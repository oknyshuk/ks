// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "rocketui/rmlui.h"
import ks.core;
import std;
#include "predictable_entity.h"
#include "util_shared.h"
#include "baseentity_shared.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "reflect_recvtable.h"
#include "c_basedoor.h"

#ifdef CBaseDoor
#undef CBaseDoor
#endif

IMPLEMENT_REFLECT_CLIENTCLASS( C_BaseDoor, DT_BaseDoor, CBaseDoor )

C_BaseDoor::C_BaseDoor( void )
{
	m_flWaveHeight = 0.0f;
}

C_BaseDoor::~C_BaseDoor( void )
{
}
