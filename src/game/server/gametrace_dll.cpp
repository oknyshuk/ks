//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "gametrace.h"
#include "world.h"

bool CGameTrace::DidHitWorld() const
{
	return Ent<CBaseEntity>() == GetWorldEntity();
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

