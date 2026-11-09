// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "rocketui/rmlui.h"
#include "predictable_entity.h"
#include "util_shared.h"
#include "baseentity_shared.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "gametrace.h"

bool TraceDidHitWorld( const CGameTrace &tr )
{
	return tr.Ent<CBaseEntity>() == ClientEntityList().GetBaseEntity( 0 );
}


bool TraceDidHitNonWorldEntity( const CGameTrace &tr )
{
	return tr.m_pEnt != nullptr && !TraceDidHitWorld( tr );
}


int TraceGetEntityIndex( const CGameTrace &tr )
{
	if ( tr.m_pEnt )
		return tr.Ent<CBaseEntity>()->entindex();
	else
		return -1;
}
