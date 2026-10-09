// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.math;
#include "vphysics_interface.h"
#include "const.h"
#include "edict.h"
#include "eiface.h"
#include "iserverentity.h"
#include "shareddefs.h"
#include "datamap.h"
#include "util.h"
#include "predictable_entity.h"
#include "variant_t.h"
#include "takedamageinfo.h"
#include "touchlink.h"
#include "groundlink.h"
#include "networkvar.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "basetoggle.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "gamerules.h"
#include "basetempentity.h"
#include "player.h"
#include "reflect_datamap.h"
#include "mapinfo.h"
#include "cs_gamerules.h"

LINK_ENTITY_TO_CLASS( info_map_parameters, CMapInfo );

IMPLEMENT_REFLECT_DATAMAP( CMapInfo )

CMapInfo *g_pMapInfo = nullptr;


CMapInfo::CMapInfo()
{
	m_flBombRadius = 500.0f;
	m_iBuyingStatus = 0;
	m_flBotMaxVisionDistance = -1.0f;

	if ( g_pMapInfo )
	{
		// Should only be one of these.
		Warning( "Warning: Multiple info_map_parameters entities in map!\n" );
	}
	else
	{
		g_pMapInfo = this;
	}
}


CMapInfo::~CMapInfo()
{
	if ( g_pMapInfo == this )
		g_pMapInfo = nullptr;
}
 

bool CMapInfo::KeyValue( const char *szKeyName, const char *szValue )
{
	if (FStrEq(szKeyName, "buying"))
	{
		m_iBuyingStatus = std::atoi(szValue);
		return true;
	}
	else if (FStrEq(szKeyName, "bombradius"))
	{
		m_flBombRadius = (float)(std::atoi(szValue));
		if (m_flBombRadius > 2048)
			m_flBombRadius = 2048;
		
		return true;
	}
	
	return BaseClass::KeyValue( szKeyName, szValue );
}


void CMapInfo::Spawn( void )
{ 
	SetMoveType( MOVETYPE_NONE );
	SetSolid( SOLID_NONE );
	AddEffects( EF_NODRAW );
}

void CMapInfo::InputFireWinCondition(inputdata_t &inputdata )
{
	CSGameRules()->TerminateRound( 5, inputdata.value.Int() );
}
