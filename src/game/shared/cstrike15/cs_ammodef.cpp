// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#if defined(CLIENT_DLL)
#include "rocketui/rmlui.h"
#elif defined(GAME_DLL)
#endif
#if defined(CLIENT_DLL)
#include "predictable_entity.h"
#include "cdll_util.h"
#include "util_shared.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "precache_register.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "engine/ivmodelinfo.h"
#include "debugoverlay_shared.h"
#elif defined(GAME_DLL)
#include "string_t.h"
#include "vphysics_interface.h"
#include "const.h"
#include "edict.h"
#include "eiface.h"
#include "iserverentity.h"
#include "dt_send.h"
#include "shareddefs.h"
#include "ehandle.h"
#include "datamap.h"
#include "util.h"
#include "predictable_entity.h"
#include "predictableid.h"
#include "variant_t.h"
#include "takedamageinfo.h"
#include "touchlink.h"
#include "groundlink.h"
#include "soundflags.h"
#include "networkvar.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "basetoggle.h"
#include "igameevents.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "ndebugoverlay.h"
#endif
#include "cs_ammodef.h"

//-----------------------------------------------------------------------------
void CCSAmmoDef::AddAmmoCost( char const* name, int cost, int buySize )
{
	int index = Index( name );
	if ( index < 1 || index >= m_nAmmoIndex )
		return;

	m_csAmmo[index].buySize = buySize;
	m_csAmmo[index].cost = cost;
}


//-----------------------------------------------------------------------------
int CCSAmmoDef::GetBuySize( int index ) const
{
	if ( index < 1 || index >= m_nAmmoIndex )
		return 0;

	return m_csAmmo[index].buySize;
}


//-----------------------------------------------------------------------------
int CCSAmmoDef::GetCost( int index ) const
{
	if ( index < 1 || index >= m_nAmmoIndex )
		return 0;

	return m_csAmmo[index].cost;
}


//-----------------------------------------------------------------------------
CCSAmmoDef::CCSAmmoDef(void)
{
	std::memset( m_csAmmo, 0, sizeof( m_csAmmo ) );
}


//-----------------------------------------------------------------------------
CCSAmmoDef::~CCSAmmoDef( void )
{
}

//-----------------------------------------------------------------------------
