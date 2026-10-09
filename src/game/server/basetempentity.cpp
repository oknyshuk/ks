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
#include "entitylist.h"
#include "basetempentity.h"
#include "ndebugoverlay.h"
#include "recipientfilter.h"
#include "reflect_sendtable.h"
#include "reflect_annotations.h"
#include "basetempentity.h"

IMPLEMENT_REFLECT_SERVERCLASS( CBaseTempEntity, DT_BaseTempEntity )




// Global list of temp entity event classes
CBaseTempEntity *CBaseTempEntity::s_pTempEntities = nullptr;

//-----------------------------------------------------------------------------
// Purpose: Returns head of list
// Output : CBaseTempEntity * -- head of list
//-----------------------------------------------------------------------------
CBaseTempEntity *CBaseTempEntity::GetList( void )
{
	return s_pTempEntities;
}

//-----------------------------------------------------------------------------
// Purpose: Creates temp entity, sets name, adds to global list
// Input  : *name - 
//-----------------------------------------------------------------------------
CBaseTempEntity::CBaseTempEntity( const char *name )
{
	m_pszName = name;
	contract_assert( m_pszName );

	// Add to list
	m_pNext			= s_pTempEntities;
	s_pTempEntities = this;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseTempEntity::~CBaseTempEntity( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Get the name of this temp entity
// Output : const char *
//-----------------------------------------------------------------------------
const char *CBaseTempEntity::GetName( void )
{
	return m_pszName ? m_pszName : "Unnamed";
}

//-----------------------------------------------------------------------------
// Purpose: Get next temp ent in chain
// Output : CBaseTempEntity *
//-----------------------------------------------------------------------------
CBaseTempEntity *CBaseTempEntity::GetNext( void )
{
	return m_pNext;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTempEntity::Precache( void )
{
	// Nothing...
}

//-----------------------------------------------------------------------------
// Purpose: Default test implementation. Should only be called by derived classes
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CBaseTempEntity::Test( const Vector& current_origin, const QAngle& current_angles )
{
	Vector origin, forward;

	Msg( "%s\n", m_pszName );
	AngleVectors( current_angles, &forward );

	VectorMA( current_origin, 20, forward, origin );

	CBroadcastRecipientFilter filter;

	Create( filter, 0.0 );
}

//-----------------------------------------------------------------------------
// Purpose: Called at startup to allow temp entities to precache any models/sounds that they need
//-----------------------------------------------------------------------------
void CBaseTempEntity::PrecacheTempEnts( void )
{
	CBaseTempEntity *te = GetList();
	while ( te )
	{
		te->Precache();
		te = te->GetNext();
	}
}


void CBaseTempEntity::Create( IRecipientFilter& filter, float delay )
{
	// temp entities can't be part of the signon message, use real entities instead
	// temp entities *can* be reliable, necessarily, due to particles.
	contract_assert( !filter.IsInitMessage() );
	contract_assert( delay >= -1 && delay <= 1 ); // 1 second max delay

	engine->PlaybackTempEntity( filter, delay, 
		(void *)this, GetServerClass()->m_pTable, GetServerClass()->m_ClassID );
}
