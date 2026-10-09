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
#include "gametrace.h"

bool CGameTrace::DidHitWorld() const
{
	return Ent<CBaseEntity>() == ClientEntityList().GetBaseEntity( 0 );
}


bool CGameTrace::DidHitNonWorldEntity() const
{
	return m_pEnt != nullptr && !DidHitWorld();
}


int CGameTrace::GetEntityIndex() const
{
	if ( m_pEnt )
		return Ent<CBaseEntity>()->entindex();
	else
		return -1;
}
