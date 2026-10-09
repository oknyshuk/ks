// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.tier1;
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
#include "base_transmit_proxy.h"
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
#include "gamerules.h"
#include "basetempentity.h"
#include "ndebugoverlay.h"
#include "recipientfilter.h"
#include "basetempentity.h"
#include "event_tempentity_tester.h"

constexpr auto TEMPENT_TEST_GAP = 1.0f;

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &vecOrigin - 
//			&vecAngles - 
//			*single_te - 
// Output : CBaseEntity
//-----------------------------------------------------------------------------
CBaseEntity *CTempEntTester::Create( const Vector &vecOrigin, const QAngle &vecAngles, const char *lifetime, const char *single_te )
{
	float life;
	char classname[ 128 ];
	if ( lifetime && lifetime[0] )
	{
		life = std::atoi( lifetime );
		life = std::max<std::common_type_t<double, float>>( 1.0, life );
		life = std::min<std::common_type_t<double, float>>( 1000.0, life );

		life += gpGlobals->curtime;
	}
	else
	{
		Msg( "Usage:  te <lifetime> <entname>\n" );
		return nullptr;
	}

	if ( single_te && single_te[0] )
	{
		V_strncpy( classname, single_te ,sizeof(classname));
		strlwr( classname );
	}
	else
	{
		Msg( "Usage:  te <lifetime> <entname>\n" );
		return nullptr;
	}

	CTempEntTester *p = ( CTempEntTester * )CBaseEntity::CreateNoSpawn( "te_tester", vecOrigin, vecAngles );
	if ( !p )
	{
		return nullptr;
	}

	V_strncpy( p->m_szClass, classname ,sizeof(p->m_szClass));
	p->m_fLifeTime = life;

	p->Spawn();

	return p;
}

LINK_ENTITY_TO_CLASS( te_tester, CTempEntTester );

//-----------------------------------------------------------------------------
// Purpose: Called when object is being created
//-----------------------------------------------------------------------------
void CTempEntTester::Spawn( void )
{
	// Not a physical thing...
	AddEffects( EF_NODRAW );

	m_pCurrent = CBaseTempEntity::GetList();
	while ( m_pCurrent )
	{
		char name[ 128 ];
		V_strncpy( name, m_pCurrent->GetName() ,sizeof(name));
		strlwr( name );
		if ( std::strstr( name, m_szClass ) )
		{
			break;
		}

		m_pCurrent = m_pCurrent->GetNext();
	}

	if ( !m_pCurrent )
	{
		DevMsg("Couldn't find temp entity '%s'\n", m_szClass );
		UTIL_Remove( this );
		return;
	}

	// Think right away
	SetNextThink( gpGlobals->curtime );
}

//-----------------------------------------------------------------------------
// Purpose: Called when object should fire itself and move on
//-----------------------------------------------------------------------------
void CTempEntTester::Think( void )
{
	// Should never happen
	if ( !m_pCurrent )
	{
		UTIL_Remove( this );
		return;
	}

	m_pCurrent->Test( GetLocalOrigin(), GetLocalAngles() );
	SetNextThink( gpGlobals->curtime + TEMPENT_TEST_GAP );

	// Time to destroy?
	if ( gpGlobals->curtime >= m_fLifeTime )
	{
		UTIL_Remove( this );
		return;
	}
}
