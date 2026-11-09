// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

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

class CItemKevlar : public CItem
{
public:

	DECLARE_CLASS( CItemKevlar, CItem );

	void Spawn_Impl( void )
	{ 
		Precache( );
		BaseClass::Spawn_Impl( );
	}
	
	void Precache_Impl( void )
	{
		PrecacheScriptSound( "BaseCombatCharacter.ItemPickup2" );
	}
	
	bool MyTouch( CBasePlayer *pBasePlayer )
	{
		CCSPlayer *pPlayer = dynamic_cast< CCSPlayer* >( pBasePlayer );
		if ( !pPlayer )
		{
			contract_assert( false );
			return false;
		}
		
		pPlayer->SetArmorValue( 100 );
		pPlayer->RecalculateCurrentEquipmentValue();

		if ( pPlayer->IsAlive() )
		{
			CBroadcastRecipientFilter filter;
			if (pPlayer->GetTeamNumber() == TEAM_CT)
			{
				// Play the CT Suit sound
				EmitSound(filter, entindex(), "Player.EquipArmor_CT");

			}
			else
			{
				// Play the T Suit sound
				EmitSound(filter, entindex(), "Player.EquipArmor_T");
			}

			//EmitSound( filter, entindex(), "BaseCombatCharacter.ItemPickup2" );
		}

		return true;
	}
};

LINK_ENTITY_TO_CLASS( item_kevlar, CItemKevlar );
