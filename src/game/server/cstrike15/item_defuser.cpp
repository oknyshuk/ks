// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Defuser kit that drops from counter-strike CTS

#include "string_t.h"
#include "edict.h"
#include "eiface.h"
#include "iserverentity.h"
#include "shareddefs.h"
#include "ehandle.h"
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
#include "igameevents.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "entitylist.h"
#include "ndebugoverlay.h"
#include "recipientfilter.h"
#include "npcevent.h"
#include "items.h"
#include "cs_player.h"
#include "cs_gamerules.h"
#include "cs_entity_spotting.h"

class CItemDefuser : public CItem
{
public:
	DECLARE_CLASS( CItemDefuser, CItem );
	
	CItemDefuser();
	~CItemDefuser();

	void	Spawn_Impl( void );
	void	Precache_Impl( void );
	void	DefuserTouch( CBaseEntity *pOther );
	void	ActivateThink( void );
	
};

LINK_ENTITY_TO_CLASS( item_defuser, CItemDefuser );
LINK_ENTITY_TO_CLASS_ALIAS( item_cutters, CItemDefuser );
PRECACHE_REGISTER(item_defuser);



Vector g_vecDefuserPosition = vec3_origin;
CBaseEntity* g_pDefuserEntity = nullptr;


CItemDefuser::CItemDefuser()
{
	g_pDefuserEntity = this;
	SetSpotRules( CCSEntitySpotting::SPOT_RULE_T | CCSEntitySpotting::SPOT_RULE_ALWAYS_SEEN_BY_CT );
}

CItemDefuser::~CItemDefuser()
{
	if ( g_pDefuserEntity == this )
	{
		g_pDefuserEntity = nullptr;
		g_vecDefuserPosition = vec3_origin;
	}
}

void CItemDefuser::Spawn_Impl( void )
{ 
	Precache( );
	SetModel( "models/weapons/w_defuser.mdl" );
	BaseClass::Spawn_Impl();
	
#if !defined( CLIENT_DLL )

	if ( mp_defuser_allocation.GetInt() == DefuserAllocation::Random )
	{
		IGameEvent * event = gameeventmanager->CreateEvent( "defuser_dropped" );
		if ( event )
		{
			event->SetInt( "entityid", entindex() );
			event->SetInt( "priority", 0 ); //defuser_dropped

			gameeventmanager->FireEvent( event );
		}
	}

	g_vecDefuserPosition = GetAbsOrigin();

#endif

	SetNextThink( gpGlobals->curtime + 0.5f );
	SetThink( &CItemDefuser::ActivateThink );

	SetTouch( nullptr );
}
	
void CItemDefuser::Precache_Impl( void )
{
	PrecacheModel( "models/weapons/w_defuser.mdl" );

	PrecacheScriptSound( "BaseCombatCharacter.ItemPickup2" );
}

void CItemDefuser::ActivateThink( void )
{
	//since we can't stop the item from being touched while its in the air,
	//activate 1 second after being dropped

	SetTouch( &CItemDefuser::DefuserTouch );
	SetThink( nullptr );
}
	
void CItemDefuser::DefuserTouch( CBaseEntity *pOther )
{
	if ( !pOther->IsPlayer() )
	{
		return;
	}

	//if( GetFlags() & FL_ONGROUND )
	{
		CCSPlayer *pPlayer = (CCSPlayer *)pOther;

		if ( !pPlayer )
		{
			contract_assert( false );
			return;
		}

		if( pPlayer->GetTeamNumber() == TEAM_CT && !pPlayer->HasDefuser() )
		{
			// [dwenger] Added for fun-fact support
			pPlayer->GiveDefuser( true );

#if !defined( CLIENT_DLL )

			if ( mp_defuser_allocation.GetInt() == DefuserAllocation::Random )
			{
				IGameEvent * event = gameeventmanager->CreateEvent( "defuser_pickup" );
				if ( event )
				{
					event->SetInt( "entityid", entindex() );
					event->SetInt( "userid", pPlayer->GetUserID() );
					event->SetInt( "priority", 0 ); //defuser_pickup
					gameeventmanager->FireEvent( event );
				}
			}

#endif

// 			if ( pPlayer->IsDead() == false )
// 			{
//				CBroadcastRecipientFilter filter;
// 				EmitSound( filter, entindex(), "BaseCombatCharacter.ItemPickup2" );
// 			}

			UTIL_Remove( this );
			return;
		}	
	}
}
