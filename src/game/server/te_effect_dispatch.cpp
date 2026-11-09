// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

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
#include "gamerules.h"
#include "entitylist.h"
#include "te.h"
#include "ndebugoverlay.h"
#include "recipientfilter.h"
#include "reflect_sendtable.h"
#include "reflect_annotations.h"
#include "basetempentity.h"
#include "te_effect_dispatch.h"
#include "networkstringtable_gamedll.h"

//-----------------------------------------------------------------------------
// Purpose: This TE provides a simple interface to dispatch effects by name using DispatchEffect().
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_TEEffectDispatch" } ]]
      [[= ks::reflect::FromTable<"m_EffectData", &REFERENCE_SEND_TABLE( DT_EffectData )>{} ]]
      CTEEffectDispatch : public CBaseTempEntity
{
public:
	DECLARE_CLASS( CTEEffectDispatch, CBaseTempEntity );

					CTEEffectDispatch( const char *name );
	virtual			~CTEEffectDispatch( void );

	DECLARE_SERVERCLASS();

public:
	CEffectData m_EffectData;
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEEffectDispatch::CTEEffectDispatch( const char *name ) :
	CBaseTempEntity( name )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEEffectDispatch::~CTEEffectDispatch( void )
{
}

IMPLEMENT_REFLECT_SERVERCLASS( CTEEffectDispatch, DT_TEEffectDispatch )


// Singleton to fire TEEffectDispatch objects
static CTEEffectDispatch g_TEEffectDispatch( "EffectDispatch" );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void DispatchEffect( const char *pName, const CEffectData &data )
{
	//Broadcast effects to all players. The potential for cheating
	//seems minimal on the effects that are sent through this,
	//and the overhead is minimal enough to make it worth sending to
	//all players.
	CReliableBroadcastRecipientFilter filter;
	filter.AddAllPlayers();

	if ( data.m_fFlags & EFFECTDATA_SERVER_IGNOREPREDICTIONCULL )
	{
		// remove prediction cull so it works on listen servers for the host
		filter.SetIgnorePredictionCull( true );
	}

	if ( !te->SuppressTE( filter ) )
	{
		// Copy the supplied effect data.
		g_TEEffectDispatch.m_EffectData = data;

		// Get the entry index in the string table.
		g_TEEffectDispatch.m_EffectData.m_iEffectName = GetEffectIndex( pName );

		// Send it to anyone who can see the effect's origin.
		g_TEEffectDispatch.Create( filter, 0 );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void DispatchEffect( IRecipientFilter& filter, float flDelay, const char *pName, const CEffectData &data )
{
	if ( !te->SuppressTE( filter ) )
	{
		// Copy the supplied effect data.
		g_TEEffectDispatch.m_EffectData = data;

		// Get the entry index in the string table.
		g_TEEffectDispatch.m_EffectData.m_iEffectName = GetEffectIndex( pName );

		// Send it to anyone who can see the effect's origin.
		g_TEEffectDispatch.Create( filter, flDelay );
	}
}
