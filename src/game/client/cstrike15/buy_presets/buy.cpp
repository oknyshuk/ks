// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

module;
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <math.h>
#include <stdlib.h>
#include "localize/ilocalize.h"
#include "utils/common/scriplib.h"
module ks.client;

import std;
import ks.core.platform;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.interfaces;
import ks.tier1.keyvalues;
import ks.tier1.reflect_wirevar;
import ks.tier1.strtools;
import :props;
import :baseanimatedtextureproxy;
import :iinput;
import :util;
import :cstrike15.bot.bot;
import :cstrike15.buy_presets.buy;
import :cstrike15.cs_player;
import :cstrike15.cs_shareddefs;
import :precache;
import ks.game.shareddefs;
import :achievements_and_stats_interface;
import :baseentity;
import :sharedvar;
import :cstrike15.cs_player_shared;
import :econ.econ_item_schema_minimal;
import :particlemgr;
import ks.tier1.createinterface;
import ks.tier1.registry;
import :noopanimcontroller;
import ks.core.dbg;
import ks.tier1.iconvar;
import ks.game.gamemovement;

class BuyPreset;
class CCSWeaponInfo;
class CCSPlayer;
class CWeaponCSBase;

using namespace ::ks::reflect;

extern ConVar cl_rebuy;

extern CSWeaponID GetClientWeaponID( bool primary );

//--------------------------------------------------------------------------------------------------------------
/**
 *  Returns true if the local player is a CT and the map is a defuse map.
 */
static bool CanBuyDefuser()
{
	CCSPlayer *pPlayer = CCSPlayer::GetLocalCSPlayer();
	return ( pPlayer && pPlayer->GetTeamNumber() == TEAM_CT && CSGameRules()->IsBombDefuseMap() );
}

void BuyPresetManager::GetCurrentLoadout( WeaponSet *weaponSet )
{
	if ( !weaponSet )
		return;

	CCSPlayer *player = CCSPlayer::GetLocalCSPlayer();
	if ( !player )
		return;

	CWeaponCSBase *pWeapon;
	const CCSWeaponInfo *pInfo;

	int ammo[MAX_AMMO_TYPES];
	memset( ammo, 0, sizeof(ammo) );
	FillClientAmmo( ammo );

	weaponSet->Reset();

	// Grab current armor values
	weaponSet->m_armor = ( player->ArmorValue() > 0 ) ? 100 : 0;
	weaponSet->m_helmet = player->HasHelmet();

	// Grab current smoke grenade
	pWeapon = dynamic_cast< CWeaponCSBase * >(player->GetCSWeapon( WEAPON_SMOKEGRENADE ));
	pInfo = GetWeaponInfo( WEAPON_SMOKEGRENADE );
	int ammoType = (pInfo)?pInfo->GetPrimaryAmmoType():0;
	weaponSet->m_smokeGrenade = (pWeapon && player->GetAmmoCount( ammoType ));

	// Grab current HE grenade
	pWeapon = dynamic_cast< CWeaponCSBase * >(player->GetCSWeapon( WEAPON_HEGRENADE ));
	pInfo = GetWeaponInfo( WEAPON_HEGRENADE );
	ammoType = (pInfo)?pInfo->GetPrimaryAmmoType():0;
	weaponSet->m_HEGrenade = (pWeapon && player->GetAmmoCount( ammoType ));

	// Grab current flashbangs
	pWeapon = dynamic_cast< CWeaponCSBase * >(player->GetCSWeapon( WEAPON_FLASHBANG ));
	pInfo = GetWeaponInfo( WEAPON_FLASHBANG );
	ammoType = (pInfo)?pInfo->GetPrimaryAmmoType():0;
	weaponSet->m_flashbangs = (!pWeapon) ? 0 : player->GetAmmoCount( ammoType );

	// Grab current equipment
	weaponSet->m_defuser = player->HasDefuser();
	weaponSet->m_nightvision = player->HasNightVision();

	// Grab current primary weapon
	CSWeaponID primaryID = GetClientWeaponID( true );		///< The local player's current primary weapon
	pInfo = GetWeaponInfo( primaryID );
	if ( pInfo )
	{
		int roundsNeeded = ammo[pInfo->GetPrimaryAmmoType()];
		int buySize = GetCSAmmoDef()->GetBuySize( pInfo->GetPrimaryAmmoType() );
		int numClips = std::ceil(roundsNeeded/(float)buySize);

		weaponSet->m_primaryWeapon.SetWeaponID( primaryID );
		weaponSet->m_primaryWeapon.SetAmmoType( AMMO_CLIPS );
		weaponSet->m_primaryWeapon.SetAmmoAmount( numClips );
		weaponSet->m_primaryWeapon.SetFillAmmo( false );
	}

	// Grab current pistol
	CSWeaponID secondaryID = GetClientWeaponID( false );	///< The local player's current pistol
	pInfo = GetWeaponInfo( secondaryID );
	if ( pInfo )
	{
		int roundsNeeded = ammo[pInfo->GetPrimaryAmmoType()];
		int buySize = GetCSAmmoDef()->GetBuySize( pInfo->GetPrimaryAmmoType() );
		int numClips = std::ceil(roundsNeeded/(float)buySize);

		weaponSet->m_secondaryWeapon.SetWeaponID( secondaryID );
		weaponSet->m_secondaryWeapon.SetAmmoType( AMMO_CLIPS );
		weaponSet->m_secondaryWeapon.SetAmmoAmount( numClips );
		weaponSet->m_secondaryWeapon.SetFillAmmo( false );
	}
}

//--------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------
WeaponSet::WeaponSet()
{
	Reset();
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Sets reasonable defaults for an empty weapon set: no primary/secondary weapons, no equipment, and
 *  everything optional.
 */
void WeaponSet::Reset()
{
	BuyPresetWeapon blank;
	m_primaryWeapon = blank;
	m_secondaryWeapon = blank;

	m_armor = 0;
	m_helmet = false;
	m_smokeGrenade = false;
	m_HEGrenade = false;
	m_flashbangs = 0;
	m_defuser = false;
	m_nightvision = false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Constructs a WeaponSet containing everything that could be purchased right now.  The cost is also
 *  returned, and is -1 if the set is not purchasable (as opposed to completely purchased already, in
 *  which case cost is 0.)
 */
void WeaponSet::GetCurrent( int& cost, WeaponSet& ws ) const
{
	cost = -1;
	ws.Reset();

	if ( !engine->IsConnected() )
		return;

	CCSPlayer *player = CCSPlayer::GetLocalCSPlayer();
	if ( !player )
		return;

	if ( player->GetTeamNumber() != TEAM_CT && player->GetTeamNumber() != TEAM_TERRORIST )
		return;

	// we're connected, and a valid team, so we can purchase
	cost = 0;

	if ( player->IsVIP() )
		return; // can't buy anything as the VIP

	int iHelmetPrice = ITEM_PRICE_HELMET;
	int iKevlarPrice = ITEM_PRICE_KEVLAR;
	int iNVGPrice = ITEM_PRICE_NVG;

	//-------------------------------------------------------------------------
	//-------------------------------------------------------------------------
	// Buy any required items

	//-------------------------------------------------------------------------
	// Armor
	if ( m_armor > player->ArmorValue() )
	{
		cost += iKevlarPrice;
		ws.m_armor = 100;
	}

	if ( (m_helmet && m_armor > 0) && !player->HasHelmet() )
	{
		cost += iHelmetPrice;
		ws.m_armor = 100;
		ws.m_helmet = true;
	}

	CWeaponCSBase *pWeapon;
	const CCSWeaponInfo *pInfo;

	//-------------------------------------------------------------------------
	// Smoke grenade
	pWeapon = dynamic_cast< CWeaponCSBase * >(player->GetCSWeapon( WEAPON_SMOKEGRENADE ));
	pInfo = GetWeaponInfo( WEAPON_SMOKEGRENADE );
	int ammoType = (pInfo)?pInfo->GetPrimaryAmmoType():0;

	bool hasSmokeGrenade = (pWeapon && player->GetAmmoCount( ammoType ));
	if ( m_smokeGrenade && !hasSmokeGrenade )
	{
		cost += pWeapon->GetWeaponPrice();
		ws.m_smokeGrenade = true;
		hasSmokeGrenade = true;
	}

	//-------------------------------------------------------------------------
	// HE grenade
	pWeapon = dynamic_cast< CWeaponCSBase * >(player->GetCSWeapon( WEAPON_HEGRENADE ));
	pInfo = GetWeaponInfo( WEAPON_HEGRENADE );
	ammoType = (pInfo)?pInfo->GetPrimaryAmmoType():0;

	bool hasHEGrenade = (pWeapon && player->GetAmmoCount( ammoType ));
	if ( m_HEGrenade && !hasHEGrenade )
	{
		cost += pWeapon->GetWeaponPrice();
		ws.m_HEGrenade = true;
		hasHEGrenade = true;
	}

	//-------------------------------------------------------------------------
	// Flashbang grenades
	pWeapon = dynamic_cast< CWeaponCSBase * >(player->GetCSWeapon( WEAPON_FLASHBANG ));
	pInfo = GetWeaponInfo( WEAPON_FLASHBANG );
	ammoType = (pInfo)?pInfo->GetPrimaryAmmoType():0;

	int numFlashbangs = (!pWeapon) ? 0 : player->GetAmmoCount( ammoType );
	if ( m_flashbangs && numFlashbangs < m_flashbangs )
	{
		int count = m_flashbangs - numFlashbangs;
		cost += pWeapon->GetWeaponPrice() * count;
		ws.m_flashbangs = count;
		numFlashbangs += count;
	}

	//-------------------------------------------------------------------------
	// defuser
	if ( m_defuser && player->GetTeamNumber() == TEAM_CT && !player->HasDefuser() && CanBuyDefuser() )
	{
		cost += ITEM_PRICE_DEFUSEKIT;
		ws.m_defuser = true;
	}

	//-------------------------------------------------------------------------
	// nightvision goggles
	if ( m_nightvision && !player->HasNightVision() )
	{
		cost += iNVGPrice;
		ws.m_nightvision = true;
	}

	//-------------------------------------------------------------------------
	// Construct a list of weapons to buy from.
	CSWeaponID primaryID = GetClientWeaponID( true );		///< The local player's current primary weapon
	CSWeaponID secondaryID = GetClientWeaponID( false );	///< The local player's current pistol

	BuyPresetWeaponList primaryWeapons;
	primaryWeapons.push_back( m_primaryWeapon );

	BuyPresetWeaponList secondaryWeapons;
	secondaryWeapons.push_back( m_secondaryWeapon );

	/**
	 *  Determine best weapons & ammo to purchase
	 *
	 *  For each primary weapon in the list, do the following until one satisfies the current money situation:
	 *    1. buy primary weapon and its ammo (NEED TO WATCH FOR WEAPONS PLAYER CANNOT BUY!!!)
	 *    2. for each non-optional secondary weapon (if any),
	 *         try to buy it and its ammo
	 *    3. when a set can be purchased, add it to the total cost and stop processing
	 *  If we can't purchase the primary weapon, or a required secondary weapon, the WeaponSet can't be bought,
	 *  so we return -1.
	 *
	 *  To find if a weapon is owned, we can compare it's weaponID to either primaryID or secondaryID from above.
	 */

	int currentCost = cost;
	int ammo[MAX_AMMO_TYPES];
	memset( ammo, 0, sizeof(ammo) );
	FillClientAmmo( ammo );
	const BuyPresetWeapon *primaryWeaponToBuy = nullptr;
	const BuyPresetWeapon *secondaryWeaponToBuy = nullptr;
	int primaryClipsToBuy = 0;
	int secondaryClipsToBuy = 0;
	int currentNonWeaponCost = currentCost;
	bool doneBuyingWeapons = false;

	int currentCash = player->GetAccount();

	//-------------------------------------------------------------------------
	// Try to buy the primary weapon
	int i;
	for ( i=0; i<std::ssize( primaryWeapons ) && !doneBuyingWeapons; ++i )
	{
		const BuyPresetWeapon *primaryWeapon = &primaryWeapons[i];
		primaryWeaponToBuy = nullptr;
		int primaryClips = 0;
		primaryClipsToBuy = 0;
		currentCost = currentNonWeaponCost;
		FillClientAmmo( ammo );

		CSWeaponID weaponID = primaryWeapon->GetCSWeaponID();
		if ( weaponID == WEAPON_NONE )
			weaponID = primaryID;

		const CCSWeaponInfo *primaryInfo = GetWeaponInfo( weaponID );
		int primaryAmmoCost = 0;
		int primaryAmmoBuySize = 0;
		if ( primaryInfo )
		{
			primaryClips = CalcClipsNeeded( primaryWeapon, primaryInfo, ammo );
			int primaryWeaponCost = ( weaponID != primaryID ) ? primaryInfo->GetWeaponPrice() : 0;
			primaryAmmoCost = GetCSAmmoDef()->GetCost( primaryInfo->GetPrimaryAmmoType() );
			primaryAmmoBuySize = GetCSAmmoDef()->GetBuySize( primaryInfo->GetPrimaryAmmoType() );
			currentCost += primaryWeaponCost;
			currentCost += primaryAmmoCost * primaryClips;
			ammo[primaryInfo->GetPrimaryAmmoType()] += primaryClips * primaryAmmoBuySize;

			CURRENTCOST_DEBUG("\t%5.5d (%s)\n", primaryWeaponCost, WeaponIDToAlias( weaponID ));
			CURRENTCOST_DEBUG("\t%5.5d (ammo x %d)\n", primaryAmmoCost * primaryClips, primaryClips);
			if ( currentCash < currentCost )
			{
				currentCost = currentNonWeaponCost;	// can't afford it, so try the next weapon
				continue;
			}

			// check for as_* maps, and CTs buying AK47s, etc
			if ( !CanBuyWeapon(primaryID, secondaryID, weaponID) && weaponID != primaryID )
			{
				currentCost = currentNonWeaponCost;	// can't buy it, so try the next weapon
				continue;
			}

			primaryWeaponToBuy = primaryWeapon;
			primaryClipsToBuy = primaryClips;
		}

		if ( secondaryWeapons.empty() )
		{
			CURRENTCOST_DEBUG("\t\t\tDone buying weapons (no secondary)\n");
			break;
		}

		//-------------------------------------------------------------------------
		// Try to buy a (required) secondary weapon to go with primaryWeaponToBuy.
		// If not, reset cost to not reflect either weapon, so we can look at buying
		// the next primary weapon in the list.
		int j;
		for ( j=0; j<std::ssize( secondaryWeapons ); ++j )
		{
			const BuyPresetWeapon *secondaryWeapon = &secondaryWeapons[j];

			// reset ammo counts and cost
			secondaryWeaponToBuy = nullptr;
			secondaryClipsToBuy = 0;
			currentCost = currentNonWeaponCost;
			FillClientAmmo( ammo );

			// add in ammo we're already buying for the primary weapon to the client's actual ammo
			if ( primaryInfo )
			{
				currentCost += ( weaponID != primaryID ) ? primaryInfo->GetWeaponPrice() : 0;
				currentCost += primaryAmmoCost * primaryClips;
				ammo[primaryInfo->GetPrimaryAmmoType() ] += primaryClips * primaryAmmoBuySize;
			}

			int currentCostWithoutSecondary = currentCost;

			CSWeaponID weaponID = secondaryWeapon->GetCSWeaponID();
			if ( weaponID == WEAPON_NONE )
				weaponID = secondaryID;

			const CCSWeaponInfo *secondaryInfo = GetWeaponInfo( weaponID );
			if ( !secondaryInfo )
			{
				currentCost = currentCostWithoutSecondary;
				continue;
			}

			int secondaryClips = CalcClipsNeeded( secondaryWeapon, secondaryInfo, ammo );
			int secondaryWeaponCost = ( weaponID != secondaryID ) ? secondaryInfo->GetWeaponPrice() : 0;
			int secondaryAmmoCost = GetCSAmmoDef()->GetCost( secondaryInfo->GetPrimaryAmmoType() );
			int secondaryAmmoBuySize = GetCSAmmoDef()->GetBuySize( secondaryInfo->GetPrimaryAmmoType() );
			currentCost += secondaryWeaponCost;
			currentCost += secondaryAmmoCost * secondaryClips;
			ammo[secondaryInfo->GetPrimaryAmmoType() ] += secondaryClips * secondaryAmmoBuySize;

			CURRENTCOST_DEBUG("\t%5.5d (%s)\n", secondaryWeaponCost, WeaponIDToAlias( weaponID ));
			CURRENTCOST_DEBUG("\t%5.5d (ammo x %d)\n", secondaryAmmoCost * secondaryClips, secondaryClips);
			if ( currentCash < currentCost )
			{
				currentCost = currentCostWithoutSecondary;	// can't afford it, so skip
				continue;
			}

			// check for as_* maps, and CTs buying elites, etc
			if ( !CanBuyWeapon(primaryID, secondaryID, weaponID) && weaponID != secondaryID )
			{
				currentCost = currentCostWithoutSecondary;	// can't buy it, so skip
				continue;
			}

			// We found both a primary and secondary weapon to buy.  Stop looking for more.
			secondaryWeaponToBuy = secondaryWeapon;
			secondaryClipsToBuy = secondaryClips;
			doneBuyingWeapons = true;
			CURRENTCOST_DEBUG("\t\t\tDone buying weapons (primary && secondary)\n");
			break;
		}
	}

	// Update our cost to reflect the weapons and ammo purchased
	cost = currentCost;

	if ( primaryWeaponToBuy )
	{
		BuyPresetWeapon weapon = *primaryWeaponToBuy;
		weapon.SetAmmoAmount( primaryClipsToBuy );
		weapon.SetAmmoType( AMMO_CLIPS );
		ws.m_primaryWeapon = weapon;
	}
	else
	{
		// If we failed to buy a primary weapon, and one was in the list, bail - the player can't afford this WeaponSet.
		for ( int i=0; i<std::ssize( primaryWeapons ); ++i )
		{
			int weaponID = primaryWeapons[i].GetCSWeaponID();
			if ( weaponID != WEAPON_NONE )
			{
				cost = -1;
				ws.Reset();
				return;
			}
		}
	}

	if ( secondaryWeaponToBuy )
	{
		BuyPresetWeapon weapon = *secondaryWeaponToBuy;
		weapon.SetAmmoAmount( secondaryClipsToBuy );
		weapon.SetAmmoType( AMMO_CLIPS );
		ws.m_secondaryWeapon = weapon;
	}
	else
	{
		// If we failed to buy a required secondary weapon, and one was in the list, bail - the player can't afford this WeaponSet.
		for ( int i=0; i<std::ssize( secondaryWeapons ); ++i )
		{
			int weaponID = secondaryWeapons[i].GetCSWeaponID();
			if ( weaponID != WEAPON_NONE )
			{
				cost = -1;
				ws.Reset();
				return;
			}
		}
	}

	//-------------------------------------------------------------------------
	// now any extra ammo, in rebuy order
	const char *remainder = SharedParse( cl_rebuy.GetString() );
	const char *token;

	while ( remainder )
	{
		token = SharedGetToken();
		if ( !token || !*token )
			return;

		if ( !stricmp( token, "PrimaryAmmo" ) )
		{
			if ( primaryWeaponToBuy )
			{
				CSWeaponID id = primaryWeaponToBuy->GetCSWeaponID();
				if ( id == WEAPON_NONE )
					id = primaryID;

				if ( primaryWeaponToBuy->GetFillAmmo() )
				{
					const CCSWeaponInfo *info = GetWeaponInfo( id );
					if ( info )
					{
						int maxRounds = GetCSAmmoDef()->MaxCarry( info->GetPrimaryAmmoType(), CCSPlayer::GetLocalCSPlayer() );
						int ammoCost = GetCSAmmoDef()->GetCost( info->GetPrimaryAmmoType() );
						int ammoBuySize = GetCSAmmoDef()->GetBuySize( info->GetPrimaryAmmoType() );
						int roundsLeft = maxRounds - ammo[info->GetPrimaryAmmoType()];
						int clipsLeft = (ammoBuySize > 0) ? std::ceil(roundsLeft / (float)ammoBuySize) : 0;
						while ( clipsLeft > 0 && cost + ammoCost <= currentCash )
						{
							ws.m_primaryWeapon.SetAmmoAmount( ws.m_primaryWeapon.GetAmmoAmount() + 1 );
							--clipsLeft;
							ammo[info->GetPrimaryAmmoType()] += std::min(maxRounds, ammoBuySize);
							cost += ammoCost;
						}
					}
				}
			}
		}
		else if ( !stricmp( token, "SecondaryAmmo" ) )
		{
			if ( secondaryWeaponToBuy )
			{
				if ( secondaryWeaponToBuy->GetFillAmmo() )
				{
					CSWeaponID weaponID = secondaryWeaponToBuy->GetCSWeaponID();
					if ( weaponID == WEAPON_NONE )
						weaponID = secondaryID;

					const CCSWeaponInfo *info = GetWeaponInfo( weaponID );
					if ( info )
					{
						int maxRounds = GetCSAmmoDef()->MaxCarry( info->GetPrimaryAmmoType(), CCSPlayer::GetLocalCSPlayer() );
						int ammoCost = GetCSAmmoDef()->GetCost( info->GetPrimaryAmmoType() );
						int ammoBuySize = GetCSAmmoDef()->GetBuySize( info->GetPrimaryAmmoType() );
						int roundsLeft = maxRounds - ammo[info->GetPrimaryAmmoType() ];
						int clipsLeft = (ammoBuySize > 0) ? std::ceil(roundsLeft / (float)ammoBuySize) : 0;
						while ( clipsLeft > 0 && cost + ammoCost <= currentCash )
						{
							ws.m_secondaryWeapon.SetAmmoAmount( ws.m_secondaryWeapon.GetAmmoAmount() + 1 );
							--clipsLeft;
							ammo[info->GetPrimaryAmmoType() ] += std::min(maxRounds, ammoBuySize);
							cost += ammoCost;
						}
					}
				}
			}
		}

		remainder = SharedParse( remainder );
	}

	if ( cost > currentCash )
	{
		cost = -1; // can't buy it
		ws.Reset();
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Constructs a WeaponSet containing everything that can be purchased.
 *  The cost (including extra cash) is also calculated and returned.
 */
void WeaponSet::GetFromScratch( int& cost, WeaponSet& ws ) const
{
	cost = 0;
	ws.Reset();

	int ammo[MAX_AMMO_TYPES];
	memset( ammo, 0, sizeof(ammo) );

	int iHelmetPrice = ITEM_PRICE_HELMET;
	int iKevlarPrice = ITEM_PRICE_KEVLAR;
	int iNVGPrice = ITEM_PRICE_NVG;

	//-------------------------------------------------------------------------
	// Primary weapon
	const CCSWeaponInfo *primaryInfo = GetWeaponInfo( m_primaryWeapon.GetCSWeaponID() );
	if ( primaryInfo )
	{
		int primaryClips = CalcClipsNeeded( &m_primaryWeapon, primaryInfo, ammo );
		int primaryWeaponCost = primaryInfo->GetWeaponPrice();
		int primaryAmmoCost = GetCSAmmoDef()->GetCost( primaryInfo->GetPrimaryAmmoType() );
		int primaryAmmoBuySize = GetCSAmmoDef()->GetBuySize( primaryInfo->GetPrimaryAmmoType() );
		cost += primaryWeaponCost;
		cost += primaryAmmoCost * primaryClips;
		ammo[primaryInfo->GetPrimaryAmmoType()] += primaryClips * primaryAmmoBuySize;

		ws.m_primaryWeapon = m_primaryWeapon;
		ws.m_primaryWeapon.SetAmmoAmount( primaryClips );
		ws.m_primaryWeapon.SetAmmoType( AMMO_CLIPS );
		ws.m_primaryWeapon.SetFillAmmo( false );
	}

	//-------------------------------------------------------------------------
	// secondary weapon
	const CCSWeaponInfo *secondaryInfo = GetWeaponInfo( m_secondaryWeapon.GetCSWeaponID() );
	if ( secondaryInfo )
	{
		int secondaryClips = CalcClipsNeeded( &m_secondaryWeapon, secondaryInfo, ammo );
		int secondaryWeaponCost = secondaryInfo->GetWeaponPrice();
		int secondaryAmmoCost = GetCSAmmoDef()->GetCost( secondaryInfo->GetPrimaryAmmoType() );
		int secondaryAmmoBuySize = GetCSAmmoDef()->GetBuySize( secondaryInfo->GetPrimaryAmmoType() );
		cost += secondaryWeaponCost;
		cost += secondaryAmmoCost * secondaryClips;
		ammo[ secondaryInfo->GetPrimaryAmmoType() ] += secondaryClips * secondaryAmmoBuySize;

		ws.m_secondaryWeapon = m_secondaryWeapon;
		ws.m_secondaryWeapon.SetAmmoAmount( secondaryClips );
		ws.m_secondaryWeapon.SetAmmoType( AMMO_CLIPS );
		ws.m_secondaryWeapon.SetFillAmmo( false );
	}

	//-------------------------------------------------------------------------
	// equipment
	if ( m_armor )
	{
		cost += (m_helmet) ? (iKevlarPrice + iHelmetPrice) : iKevlarPrice;
		ws.m_armor = m_armor;
		ws.m_helmet = m_helmet;
	}

	if ( m_smokeGrenade )
	{
		const CCSWeaponInfo *pInfo = GetWeaponInfo( WEAPON_SMOKEGRENADE );
		cost += ( pInfo ) ? pInfo->GetWeaponPrice() : 0;
		ws.m_smokeGrenade = m_smokeGrenade;
	}

	if ( m_HEGrenade )
	{
		const CCSWeaponInfo *pInfo = GetWeaponInfo( WEAPON_HEGRENADE );
		cost += ( pInfo ) ? pInfo->GetWeaponPrice() : 0;
		ws.m_HEGrenade = m_HEGrenade;
	}

	const CCSWeaponInfo *pInfo = GetWeaponInfo( WEAPON_FLASHBANG );
	cost += ( pInfo ) ? pInfo->GetWeaponPrice() * m_flashbangs : 0;
	ws.m_flashbangs = m_flashbangs;

	if ( m_defuser )
	{
		cost += ITEM_PRICE_DEFUSEKIT;
		ws.m_defuser = m_defuser;
	}

	if ( m_nightvision )
	{
		cost += iNVGPrice;
		ws.m_nightvision = m_nightvision;
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Convenience function that wraps GetFromScratch() and discards the generated WeaponSet.  Returns the cost
 *  to buy the full WeaponSet from scratch.
 */
int WeaponSet::FullCost() const
{
	WeaponSet fullSet;
	int cost = 0;
	GetFromScratch( cost, fullSet );
	return cost;
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Generates a list of buy commands that will buy the current WeaponSet.
 */
void WeaponSet::GenerateBuyCommands( char command[BUY_PRESET_COMMAND_LEN] ) const
{
	command[0] = 0;
	char *tmp = command;
	int remainder = BUY_PRESET_COMMAND_LEN;
	int i;

	CSWeaponID primaryID = GetClientWeaponID( true );		///< The local player's current primary weapon
	CSWeaponID secondaryID = GetClientWeaponID( false );	///< The local player's current pistol

	//-------------------------------------------------------------------------
	// Primary weapon
	CSWeaponID weaponID = m_primaryWeapon.GetCSWeaponID();
	if ( weaponID == WEAPON_NONE )
		weaponID = primaryID;
	const CCSWeaponInfo *primaryInfo = GetWeaponInfo( weaponID );
	if ( primaryInfo )
	{
		if ( weaponID != primaryID )
		{
			tmp = BufPrintf( tmp, remainder, "buy %s\n", WeaponIDToAlias(weaponID) );
		}
		for ( i=0; i<m_primaryWeapon.GetAmmoAmount(); ++i )
		{
			tmp = BufPrintf( tmp, remainder, "buyammo1\n" );
		}
	}

	//-------------------------------------------------------------------------
	// secondary weapon
	weaponID = m_secondaryWeapon.GetCSWeaponID();
	if ( weaponID == WEAPON_NONE )
		weaponID = secondaryID;
	const CCSWeaponInfo *secondaryInfo = GetWeaponInfo( weaponID );
	if ( secondaryInfo )
	{
		if ( weaponID != secondaryID )
		{
			tmp = BufPrintf( tmp, remainder, "buy %s\n", WeaponIDToAlias(weaponID) );
		}
		for ( i=0; i<m_secondaryWeapon.GetAmmoAmount(); ++i )
		{
			tmp = BufPrintf( tmp, remainder, "buyammo2\n" );
		}
	}

	//-------------------------------------------------------------------------
	// equipment
	if ( m_armor )
	{
		if ( m_helmet )
		{
			tmp = BufPrintf( tmp, remainder, "buy vesthelm\n" );
		}
		else
		{
			tmp = BufPrintf( tmp, remainder, "buy vest\n" );
		}
	}

	if ( m_smokeGrenade )
	{
		tmp = BufPrintf( tmp, remainder, "buy smokegrenade\n" );
	}

	if ( m_HEGrenade )
	{
		tmp = BufPrintf( tmp, remainder, "buy hegrenade\n" );
	}

	for ( i=0; i<m_flashbangs; ++i )
	{
		tmp = BufPrintf( tmp, remainder, "buy flashbang\n" );
	}

	if ( m_defuser )
	{
		tmp = BufPrintf( tmp, remainder, "buy defuser\n" );
	}

	if ( m_nightvision )
	{
		tmp = BufPrintf( tmp, remainder, "buy nvgs\n" );
	}
}

//--------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------
BuyPreset::BuyPreset()
{
	SetName( L"" );
}

//--------------------------------------------------------------------------------------------------------------
BuyPreset::BuyPreset( const BuyPreset& other )
{
	*this = other;
}

//--------------------------------------------------------------------------------------------------------------
BuyPreset::~BuyPreset()
{
}

//--------------------------------------------------------------------------------------------------------------
void BuyPreset::SetName( const wchar_t *name )
{
	wcsncpy( m_name, name, MaxBuyPresetName );
	if ( m_name[0] == 0 )
	{
		const wchar_t * defaultName = g_pLocalize->Find( "#Cstrike_BuyPresetBlank" );
		if ( defaultName )
		{
			wcsncpy( m_name, defaultName, MaxBuyPresetName );
		}
	}
	m_name[MaxBuyPresetName-1] = 0;
}

//--------------------------------------------------------------------------------------------------------------
// Parse KeyValues string into a BuyPresetWeapon vector
static void ParseWeaponString( const char *str, BuyPresetWeaponList& weapons, bool isPrimary )
{
	weapons.clear();

	if ( !str )
		return;

	const char *remainder = SharedParse( str );
	const char *token;

	const int BufLen = 32;
	char tmpBuf[BufLen];
	char weaponBuf[BufLen];
	char clipModifier;
	int numClips;

	while ( remainder )
	{
		token = SharedGetToken();
		if ( !token || strlen(token) >= BufLen )
			return;

		V_strncpy( tmpBuf, token, BufLen );
		tmpBuf[BufLen - 1] = 0;
		char *tmp = tmpBuf;
		while ( *tmp )
		{
			if ( *tmp == '/' )
			{
				*tmp = ' ';
			}
			++tmp;
		}
		// sscanf is safe, since the size of the string being scanned is at least as small as any individual destination buffer
		if ( sscanf( tmpBuf, "%s %d%c", weaponBuf, &numClips, &clipModifier ) != 3 )
			return;

		CSWeaponID weaponID = AliasToWeaponID( weaponBuf );
		if ( weaponID != WEAPON_NONE )
		{
			const CCSWeaponInfo *info = GetWeaponInfo( weaponID );
			if ( info )
			{
				int maxRounds = GetCSAmmoDef()->MaxCarry( info->GetPrimaryAmmoType(), CCSPlayer::GetLocalCSPlayer() );
				int buySize = GetCSAmmoDef()->GetBuySize( info->GetPrimaryAmmoType() );
				numClips = std::min<std::common_type_t<float, int>>(std::ceil(maxRounds/(float)buySize), std::max(0, numClips));
				if ( ((!isPrimary) ^ IsPrimaryWeapon( weaponID )) == 0 )
					return;
			}
			else
			{
				numClips = std::min<int>( NUM_CLIPS_FOR_CURRENT, std::max<int>( 0, numClips ) );
			}
		}
		else
		{
			numClips = std::min<int>( NUM_CLIPS_FOR_CURRENT, std::max<int>( 0, numClips ) );
		}

		BuyPresetWeapon weapon( weaponID );
		weapon.SetAmmoType( AMMO_CLIPS );
		weapon.SetAmmoAmount( numClips );
		weapon.SetFillAmmo( (clipModifier == '+') );
		weapons.push_back( weapon );

		remainder = SharedParse( remainder );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Populates data from a KeyValues structure, for loading
 */
void BuyPreset::Parse( KeyValues *data )
{
	m_name[0] = 0;
	m_weaponList.clear();

	if (!data)
		return;

	//-------------------------------------------------------------------------
	// Read name
	wcsncpy( m_name, data->GetWString( "PresetName", L""), MaxBuyPresetName );
	m_name[ MaxBuyPresetName - 1 ] = 0;
	PRESET_DEBUG( "Parsing Buy Preset %ls\n", m_name );

	int version = data->GetInt( "Version", 0 );
	if ( version < 4 || version > 4 )
	{
		PRESET_DEBUG( "Invalid preset version %d\n", version );
		return;
	}

	const char *primaryString = data->GetString( "Primary", nullptr );
	const char *secondaryString = data->GetString( "Secondary", nullptr );
	const char *equipmentString = data->GetString( "Equipment", nullptr );

	std::vector< BuyPresetWeapon > weapons;
	ParseWeaponString( primaryString, weapons, true );

	WeaponSet ws;
	if ( !weapons.empty() )
		ws.m_primaryWeapon = weapons[0];

	weapons.clear();
	ParseWeaponString( secondaryString, weapons, false );
	if ( !weapons.empty() )
		ws.m_secondaryWeapon = weapons[0];

	// parse equipment
	if ( equipmentString )
	{
		const char *remainder = SharedParse( equipmentString );
		const char *token;

		const int BufLen = 32;
		char tmpBuf[BufLen];
		char itemBuf[BufLen];
		int intVal;

		while ( remainder )
		{
			token = SharedGetToken();
			if ( !token || V_strlen(token) >= BufLen )
				break;

			V_strncpy( tmpBuf, token, BufLen );
			tmpBuf[BufLen - 1] = 0;
			char *tmp = tmpBuf;
			while ( *tmp )
			{
				if ( *tmp == '/' )
				{
					*tmp = ' ';
				}
				++tmp;
			}
			// sscanf is safe, since the size of the string being scanned is at least as small as any individual destination buffer
			if ( sscanf( tmpBuf, "%s %d", itemBuf, &intVal ) != 2 )
				break;

			if ( !strcmp( itemBuf, "vest" ) )
			{
				ws.m_armor = (intVal > 0) ? 100 : 0;
				ws.m_helmet = false;
			}
			else if ( !strcmp( itemBuf, "vesthelm" ) )
			{
				ws.m_armor = (intVal > 0) ? 100 : 0;
				ws.m_helmet = true;
			}
			else if ( !strcmp( itemBuf, "defuser" ) )
			{
				ws.m_defuser = (intVal > 0);
			}
			else if ( !strcmp( itemBuf, "nvgs" ) )
			{
				ws.m_nightvision = (intVal > 0);
			}
			else if ( !strcmp( itemBuf, "sgren" ) )
			{
				ws.m_smokeGrenade = (intVal > 0);
			}
			else if ( !strcmp( itemBuf, "hegren" ) )
			{
				ws.m_HEGrenade = (intVal > 0);
			}
			else if ( !strcmp( itemBuf, "flash" ) )
			{
				ws.m_flashbangs = std::min( 2, std::max( 0, intVal ) );
			}

			remainder = SharedParse( remainder );
		}

		m_weaponList.push_back( ws );
	}
}

//--------------------------------------------------------------------------------------------------------------
// Build a string of weapons+ammo for KeyValues saving/loading
static const char* ConstructWeaponString( const BuyPresetWeapon& weapon )
{
	const int WeaponLen = 1024;
	static char weaponString[WeaponLen];
	weaponString[0] = 0;
	int remainder = WeaponLen;
	char *tmp = weaponString;

	tmp = BufPrintf( tmp, remainder, "%s/%d%c",
		WeaponIDToAlias( weapon.GetCSWeaponID() ),
		weapon.GetAmmoAmount(),
		(weapon.GetFillAmmo()) ? '+' : '=' );
	return weaponString;
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Fills in a KeyValues structure with data, for saving
 */
void BuyPreset::Save( KeyValues *data )
{
	//-------------------------------------------------------------------------
	// Save name and version
	KeyValues *presetKey = data->CreateNewKey();
	presetKey->SetWString( "PresetName", m_name );

	/**
	 *  Version History
	 *  ---------------
	 *
	 *  PRE-RELEASE
	 *  1.  2/2004
	 *      First-pass implementation.  Allowed for multiple weapons per fallback.
	 *      Individual items could be marked optional.  Cash reserve could be specified.
	 *  2.  3/2004
	 *      Second-pass implementation.  Removed multiple weapons per fallback.  The
	 *      pistol could be marked optional.  A preset could be marked as 'use for
	 *      Autobuy'.
	 *
	 *  CZ RELEASE
	 *  3.  4/22/2004
	 *      The first released version.  Removed optional/required status.  Also
	 *      removed the cash reserve and autobuy status.  Corresponds to streamlined
	 *      editing interface mocked up by Greg Coomer.
	 *
	 *  CS:S RELEASE
	 *  4.  3/10/2004
	 *      Removed fallbacks to correspond to new UI.
	 */
	presetKey->SetInt( "Version", 4 );
	if ( !m_weaponList.empty() )
	{
		//-------------------------------------------------------------------------
		// Save a fallback WeaponSet
		const WeaponSet& ws = m_weaponList[0];

		presetKey->SetString( "Primary", ConstructWeaponString( ws.m_primaryWeapon ) );
		presetKey->SetString( "Secondary", ConstructWeaponString( ws.m_secondaryWeapon ) );

		presetKey->SetString( "Equipment",
			SharedVarArgs("vest%s/%d flash/%d sgren/%d hegren/%d defuser/%d nvgs/%d",
			(ws.m_helmet)?"helm":"", ws.m_armor,
			ws.m_flashbangs,
			ws.m_smokeGrenade,
			ws.m_HEGrenade,
			ws.m_defuser,
			ws.m_nightvision
			) );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Calculates the full cost of the preset, which is exactly the full cost of the first fallback WeaponSet
 */
int BuyPreset::FullCost() const
{
	if ( m_weaponList.empty() )
		return 0;

	return m_weaponList[0].FullCost();
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Returns the specified fallback WeaponSet, or NULL if it doesn't exist
 */
const WeaponSet * BuyPreset::GetSet( int index ) const
{
	if ( index < 0 || index >= std::ssize( m_weaponList ) )
		return nullptr;

	return &(m_weaponList[index]);
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Deletes a fallback
 */
void BuyPreset::DeleteSet( int index )
{
	if ( index < 0 || index >= std::ssize( m_weaponList ) )
		return;

	m_weaponList.erase( m_weaponList.begin() + index );
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Switches the order of two fallbacks
 */
void BuyPreset::SwapSet( int firstIndex, int secondIndex )
{
	if ( firstIndex < 0 || firstIndex >= std::ssize( m_weaponList ) )
		return;

	if ( secondIndex < 0 || secondIndex >= std::ssize( m_weaponList ) )
		return;

	WeaponSet tmpSet = m_weaponList[firstIndex];
	m_weaponList[firstIndex] = m_weaponList[secondIndex];
	m_weaponList[secondIndex] = tmpSet;
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Replaces a fallback with the supplied WeaponSet
 */
void BuyPreset::ReplaceSet( int index, const WeaponSet& weaponSet )
{
	if ( index < 0 || index > std::ssize( m_weaponList ) )
		return;

	if ( index == std::ssize( m_weaponList ) )
	{
		m_weaponList.push_back( weaponSet );
	}
	else
	{
		m_weaponList[index] = weaponSet;
	}
}

//--------------------------------------------------------------------------------------------------------------

// Author: Matthew D. Campbell (matt@turtlerockstudios.com), 2004

#define BUY_PRESET_DEBUGGING 0

#if BUY_PRESET_DEBUGGING

static ConVar cl_preset_debug( side_registry(), "cl_preset_debug", "0", 0, "Controls debug information about buy presets" );

//--------------------------------------------------------------------------------------------------------------
bool IsPresetDebuggingEnabled()
{
	return cl_preset_debug.GetInt() >= 1.0f;
}

//--------------------------------------------------------------------------------------------------------------
bool IsPresetFullCostDebuggingEnabled()
{
	return cl_preset_debug.GetInt() == 2.0f;
}

//--------------------------------------------------------------------------------------------------------------
bool IsPresetCurrentCostDebuggingEnabled()
{
	return cl_preset_debug.GetInt() == 3.0f;
}

#else // ! BUY_PRESET_DEBUGGING

//--------------------------------------------------------------------------------------------------------------
bool IsPresetDebuggingEnabled()
{
	return false;
}

//--------------------------------------------------------------------------------------------------------------
bool IsPresetFullCostDebuggingEnabled()
{
	return false;
}

//--------------------------------------------------------------------------------------------------------------
bool IsPresetCurrentCostDebuggingEnabled()
{
	return false;
}

#endif // ! BUY_PRESET_DEBUGGING

//--------------------------------------------------------------------------------------------------------------

#undef BUY_PRESET_DEBUGGING

class CCSWeaponInfo;
class CCSPlayer;
class CWeaponCSBase;

using namespace ::ks::reflect;

// Purpose: BuyPresetWeapon implementation, and misc knowledge relating to weapons

//--------------------------------------------------------------------------------------------------------------
struct WeaponDisplayNameInfo
{
	CSWeaponID id;
	const char *displayName;
};

//--------------------------------------------------------------------------------------------------------------
// NOTE: Array must be NULL-terminated
static WeaponDisplayNameInfo weaponDisplayNameInfo[] = 
{
	{ WEAPON_DEAGLE,	"#Cstrike_TitlesTXT_DesertEagle" },
	{ WEAPON_ELITE,		"#Cstrike_TitlesTXT_Beretta96G" },
	{ WEAPON_FIVESEVEN,	"#Cstrike_TitlesTXT_ESFiveSeven" },
	{ WEAPON_GLOCK,		"#Cstrike_TitlesTXT_Glock18" },
	{ WEAPON_P228,		"#Cstrike_TitlesTXT_P228" },
	{ WEAPON_USP,		"#Cstrike_TitlesTXT_USP45" },
	
	{ WEAPON_AK47,		"#Cstrike_TitlesTXT_AK47" },
	{ WEAPON_AUG,		"#Cstrike_TitlesTXT_Aug" },
	{ WEAPON_AWP,		"#Cstrike_TitlesTXT_ArcticWarfareMagnum" },
	{ WEAPON_FAMAS,		"#Cstrike_TitlesTXT_Famas" },
	{ WEAPON_G3SG1,		"#Cstrike_TitlesTXT_G3SG1" },
	{ WEAPON_GALIL,		"#Cstrike_TitlesTXT_Galil" },
	{ WEAPON_GALILAR,	"#Cstrike_TitlesTXT_GalilAR" },
	{ WEAPON_M249,		"#Cstrike_TitlesTXT_ESM249" },
	{ WEAPON_M3,		"#Cstrike_TitlesTXT_Leone12" },
	{ WEAPON_M4A1,		"#Cstrike_TitlesTXT_M4A1_Short" },
	{ WEAPON_MAC10,		"#Cstrike_TitlesTXT_Mac10_Short" },
	{ WEAPON_MP5NAVY,	"#Cstrike_TitlesTXT_mp5navy" },
	{ WEAPON_P90,		"#Cstrike_TitlesTXT_ESC90" },
	{ WEAPON_SCOUT,		"#Cstrike_TitlesTXT_Scout" },
	{ WEAPON_SG550,		"#Cstrike_TitlesTXT_SG550" },
	{ WEAPON_SG552,		"#Cstrike_TitlesTXT_SG552" },
	{ WEAPON_TMP,		"#Cstrike_TitlesTXT_tmp" },
	{ WEAPON_UMP45,		"#Cstrike_TitlesTXT_KMUMP45" },
	{ WEAPON_XM1014,	"#Cstrike_TitlesTXT_AutoShotgun" },
	
	{ WEAPON_BIZON,		"#Cstrike_TitlesTXT_Bizon" },
	{ WEAPON_MAG7,		"#Cstrike_TitlesTXT_Mag7" },
	{ WEAPON_NEGEV,		"#Cstrike_TitlesTXT_Negev" },
	{ WEAPON_SAWEDOFF,	"#Cstrike_TitlesTXT_Sawedoff" },
	{ WEAPON_TEC9,		"#Cstrike_TitlesTXT_Tec9" },
	{ WEAPON_TASER,		"#Cstrike_TitlesTXT_Taser" },

	{ WEAPON_HKP2000,	"#Cstrike_TitlesTXT_HKP2000" },
	{ WEAPON_MP7,		"#Cstrike_TitlesTXT_MP7" },
	{ WEAPON_MP9,		"#Cstrike_TitlesTXT_MP9" },
	{ WEAPON_NOVA,		"#Cstrike_TitlesTXT_Nova" },
	{ WEAPON_P250,		"#Cstrike_TitlesTXT_P250" },
	{ WEAPON_SCAR17,	"#Cstrike_TitlesTXT_Scar17" },
	{ WEAPON_SCAR20,	"#Cstrike_TitlesTXT_Scar20" },
	{ WEAPON_SG556,		"#Cstrike_TitlesTXT_SG556" },
	{ WEAPON_SSG08,		"#Cstrike_TitlesTXT_SSG08" },

	{ WEAPON_NONE,		"#Cstrike_CurrentWeapon" }
};

//--------------------------------------------------------------------------------------------------------------
/**
 *  Returns the display name for a weapon, based on it's weaponID
 */
const wchar_t* WeaponIDToDisplayName( CSWeaponID weaponID )
{
	for( int i=0; weaponDisplayNameInfo[i].displayName; ++i )
		if ( weaponDisplayNameInfo[i].id == weaponID )
			return g_pLocalize->Find( weaponDisplayNameInfo[i].displayName );

	return nullptr;
}

//--------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------
BuyPresetWeapon::BuyPresetWeapon()
{
	m_name = nullptr;
	m_weaponID = WEAPON_NONE;
	m_ammoType = AMMO_CLIPS;
	m_ammoAmount = 0;
	m_fillAmmo = true;
}

//--------------------------------------------------------------------------------------------------------------
BuyPresetWeapon::BuyPresetWeapon( CSWeaponID weaponID )
{
	m_name = WeaponIDToDisplayName( weaponID );
	m_weaponID = weaponID;
	m_ammoType = AMMO_CLIPS;
	m_ammoAmount = (weaponID == WEAPON_NONE) ? 0 : 1;
	m_fillAmmo = true;
}

//--------------------------------------------------------------------------------------------------------------
/*
BuyPresetWeapon& BuyPresetWeapon::operator= (const BuyPresetWeapon& other)
{
	m_name = other.m_name;
	m_weaponID = other.m_weaponID;
	m_ammoType = other.m_ammoType;
	m_ammoAmount = other.m_ammoAmount;
	m_fillAmmo = other.m_fillAmmo;

	return *this;
}
*/

//--------------------------------------------------------------------------------------------------------------
/**
 *  Sets the WEAPON_* weapon ID (and resets ammo, etc)
 */
void BuyPresetWeapon::SetWeaponID( CSWeaponID weaponID )
{
	m_name = WeaponIDToDisplayName( weaponID );
	m_weaponID = weaponID;
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Returns the number of clips that will have to be bought for the specified BuyPresetWeapon
 */
int CalcClipsNeeded( const BuyPresetWeapon *pWeapon, const CCSWeaponInfo *pInfo, const int ammo[MAX_AMMO_TYPES] )
{
	if ( !pWeapon || !pInfo )
		return 0;

	int maxRounds = GetCSAmmoDef()->MaxCarry( pInfo->GetPrimaryAmmoType(), CCSPlayer::GetLocalCSPlayer() );
	int buySize = GetCSAmmoDef()->GetBuySize( pInfo->GetPrimaryAmmoType() );

	int numClips = 0;
	if ( buySize && pInfo->GetPrimaryAmmoType() >= 0 )
	{
		switch ( pWeapon->GetAmmoType() )
		{
		case AMMO_CLIPS:
			numClips = pWeapon->GetAmmoAmount();
			if ( pWeapon->GetCSWeaponID() == WEAPON_NONE && numClips >= 4 )
			{
				numClips = std::ceil(maxRounds/(float)buySize);
			}
			numClips = std::min<std::common_type_t<float, int>>( std::ceil(maxRounds/(float)buySize), numClips );

			numClips -= ammo[pInfo->GetPrimaryAmmoType() ]/buySize;
			if ( numClips < 0 || ammo[pInfo->GetPrimaryAmmoType() ] == maxRounds )
			{
				numClips = 0;
			}
			break;
		case AMMO_ROUNDS:
			{
				int roundsNeeded = pWeapon->GetAmmoAmount() - ammo[pInfo->GetPrimaryAmmoType() ];
				if ( roundsNeeded > 0 )
				{
					numClips = ceil(roundsNeeded/(float)buySize);
				}
				else
				{
					numClips = 0;
				}
			}
			break;
		case AMMO_PERCENT:
			{
				int roundsNeeded = maxRounds*pWeapon->GetAmmoAmount() - ammo[pInfo->GetPrimaryAmmoType() ];
				roundsNeeded *= 0.01f;
				if ( roundsNeeded > 0 )
				{
					numClips = ceil(roundsNeeded/(float)buySize);
				}
				else
				{
					numClips = 0;
				}
			}
			break;
		}
	}
	return numClips;
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Returns true if the client can currently buy the weapon according to class/gamemode restrictions.  Returns
 *  true also if the player owns the weapon already.
 */
bool CanBuyWeapon( CSWeaponID currentPrimaryID, CSWeaponID currentSecondaryID, CSWeaponID weaponID )
{
	CCSPlayer *pPlayer = CCSPlayer::GetLocalCSPlayer();
	if ( !pPlayer )
		return false;

	const CCSWeaponInfo *info = GetWeaponInfo( weaponID );
	if ( !info )
		return false;

	/// @TODO: assasination maps have a specific set of weapons that can be used in them.
	if ( info->GetUsedByTeam() != TEAM_UNASSIGNED && pPlayer->GetTeamNumber() != info->GetUsedByTeam() )
		return false;

	return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Returns true if the CSWeaponType is a primary class
 */
static bool IsPrimaryWeaponClassID( CSWeaponType classId )
{
	switch (classId)
	{
	case WEAPONTYPE_SUBMACHINEGUN:
	case WEAPONTYPE_SHOTGUN:
	case WEAPONTYPE_MACHINEGUN:
	case WEAPONTYPE_RIFLE:
	case WEAPONTYPE_SNIPER_RIFLE:
		return true;
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Returns true if the weapon ID is for a primary weapon
 */
static bool IsPrimaryWeaponID( CSWeaponID id )
{
	const CCSWeaponInfo *info = GetWeaponInfo( id );
	if ( !info )
		return false;

	return IsPrimaryWeaponClassID( info->GetWeaponType() );
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Returns true if the CSWeaponType is a secondary class
 */
static bool IsSecondaryWeaponClassID( CSWeaponType classId )
{
	return (classId == WEAPONTYPE_PISTOL);
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Returns true if the weapon ID is for a secondary weapon
 */
static bool IsSecondaryWeaponID( CSWeaponID id )
{
	const CCSWeaponInfo *info = GetWeaponInfo( id );
	if ( !info )
		return false;

	return IsSecondaryWeaponClassID( info->GetWeaponType() );
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Fills the ammo array with the ammo currently owned by the local player
 */
void FillClientAmmo( int ammo[MAX_AMMO_TYPES] )
{
	int i;
	for ( i=0; i<MAX_AMMO_TYPES; ++i )
	{
		ammo[i] = 0;
	}

	CCSPlayer *localPlayer = CCSPlayer::GetLocalCSPlayer();
	if ( !localPlayer )
		return;

	for ( i=0; i<WEAPON_MAX; ++i )
	{
		CSWeaponID gameWeaponID = (CSWeaponID)i;
		if ( gameWeaponID == WEAPON_NONE || gameWeaponID >= EQUIPMENT_FIRST )
			continue;

		const CCSWeaponInfo *info = GetWeaponInfo( gameWeaponID );
		if ( !info )
			continue;

		int clientAmmoType = info->GetPrimaryAmmoType();
		int clientAmmoCount = localPlayer->GetAmmoCount( clientAmmoType );
		if ( clientAmmoCount > 0 )
		{
			ammo[ clientAmmoType ] = std::max( ammo[ clientAmmoType ], clientAmmoCount );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: returns the weapon in the specified slot
//-----------------------------------------------------------------------------
CWeaponCSBase *GetWeaponInSlot( int iSlot, int iSlotPos )
{
	CCSPlayer *player = CCSPlayer::GetLocalCSPlayer();
	if ( !player )
		return nullptr;

	for ( int i = 0; i < MAX_WEAPONS; i++ )
	{
		CWeaponCSBase *pWeapon = static_cast< CWeaponCSBase * >(player->GetWeapon(i));
		
		if ( pWeapon == nullptr )
			continue;

		if ( pWeapon->GetSlot() == iSlot && pWeapon->GetPosition() == iSlotPos )
			return pWeapon;
	}

	return nullptr;
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Returns the client's WEAPON_* value for the currently owned weapon, or WEAPON_NONE if no weapon is owned
 */
CSWeaponID GetClientWeaponID( bool primary )
{
	CCSPlayer *localPlayer = CCSPlayer::GetLocalCSPlayer();
	if ( !localPlayer )
		return WEAPON_NONE;

	int slot = (primary)?0:1;
	CWeaponCSBase *pWeapon = GetWeaponInSlot( slot, slot );
	if ( !pWeapon )
		return WEAPON_NONE;

	return pWeapon->GetCSWeaponID();
}

//--------------------------------------------------------------------------------------------------------------

class BuyPreset;
class CCSPlayer;

using namespace ::ks::reflect;

BuyPresetManager *TheBuyPresets = nullptr;

//--------------------------------------------------------------------------------------------------------------
ConVar cl_buy_favorite_quiet( side_registry(), "cl_buy_favorite_quiet", "0", FCVAR_ARCHIVE | FCVAR_CLIENTCMD_CAN_EXECUTE, "Skips the prompt when saving a buy favorite in the buy menu" );
ConVar cl_buy_favorite_nowarn( side_registry(), "cl_buy_favorite_nowarn", "0", FCVAR_ARCHIVE | FCVAR_CLIENTCMD_CAN_EXECUTE, "Skips the error prompt when saving an invalid buy favorite" );

//--------------------------------------------------------------------------------------------------------------
static void PrintBuyPresetUsage( void )
{
	if ( TheBuyPresets->GetNumPresets() )
	{
		Msg( "usage:  cl_buy_favorite <1...%d>\n", TheBuyPresets->GetNumPresets() );
		for ( int i=0; i<TheBuyPresets->GetNumPresets(); ++i )
		{
			const BuyPreset *preset = TheBuyPresets->GetPreset( i );
			if ( preset && preset->GetName() && preset->GetName()[0] )
			{
				char buffer[64];
				g_pLocalize->ConvertUnicodeToANSI( preset->GetName(), buffer, sizeof( buffer ) );
				Msg( " %d. %s\n", i+1, buffer );
			}
		}
	}
	else
	{
		Msg( "cl_buy_favorite: no favorites are defined\n" );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Callback function for handling the "cl_buy_favorite" command
 */
static void cl_buy_favorite( const CCommand &args );
static ConCommand cl_buy_favorite_command( side_registry(), "cl_buy_favorite", cl_buy_favorite, "Purchase a favorite weapon/equipment loadout", FCVAR_CLIENTCMD_CAN_EXECUTE );
static void cl_buy_favorite( const CCommand &args )
{
	if ( !engine->IsConnected() )
		return;

	if ( !TheBuyPresets )
		TheBuyPresets = new BuyPresetManager();

	if ( args.ArgC() != 2 )
	{
		PRESET_DEBUG( "cl_buy_favorite: no favorite specified\n" );
		PrintBuyPresetUsage();
		return;
	}

	int presetIndex = atoi( args[1] ) - 1;
	if ( presetIndex < 0 || presetIndex >= TheBuyPresets->GetNumPresets() )
	{
		PRESET_DEBUG( "cl_buy_favorite: favorite %d doesn't exist\n", presetIndex );
		PrintBuyPresetUsage();
		return;
	}

	TheBuyPresets->PurchasePreset( presetIndex );
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Callback function for handling the "cl_buy_favorite_set" command
 */
static void cl_buy_favorite_set( const CCommand &args );
static ConCommand cl_buy_favorite_set_command( side_registry(), "cl_buy_favorite_set", cl_buy_favorite_set, "Saves the current loadout as a favorite", FCVAR_CLIENTCMD_CAN_EXECUTE );
static void cl_buy_favorite_set( const CCommand &args )
{
	if ( !engine->IsConnected() )
		return;

	if ( !TheBuyPresets )
		TheBuyPresets = new BuyPresetManager();

	if ( args.ArgC() != 2 )
	{
		PRESET_DEBUG( "cl_buy_favorite_set: no favorite specified\n" );
		PrintBuyPresetUsage();
		return;
	}

	int presetIndex = atoi( args[ 1 ] ) - 1;
	if ( presetIndex < 0 || presetIndex >= TheBuyPresets->GetNumPresets() )
	{
		PRESET_DEBUG( "cl_buy_favorite_set: favorite %d doesn't exist\n", presetIndex );
		PrintBuyPresetUsage();
		return;
	}

	const BuyPreset *preset = TheBuyPresets->GetPreset( presetIndex );
	if ( !preset )
	{
		return;
	}

	WeaponSet ws;
	TheBuyPresets->GetCurrentLoadout( &ws );
	BuyPreset newPreset( *preset );
	newPreset.ReplaceSet( 0, ws );

	TheBuyPresets->SetPreset( presetIndex, &newPreset );
	TheBuyPresets->Save();

	CCSPlayer *pPlayer = CCSPlayer::GetLocalCSPlayer();
	if ( pPlayer )
	{
		pPlayer->EmitSound( "BuyPreset.Updated" );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Callback function for handling the "cl_buy_favorite_reset" command
 */
void __CmdFunc_BuyPresetsReset(void)
{
	if ( !engine->IsConnected() )
		return;

	if ( !TheBuyPresets )
		TheBuyPresets = new BuyPresetManager();

	CCSPlayer *pPlayer = CCSPlayer::GetLocalCSPlayer();
	if ( !pPlayer || ( pPlayer->GetTeamNumber() != TEAM_CT && pPlayer->GetTeamNumber() != TEAM_TERRORIST ) )
	{
		return;
	}

	TheBuyPresets->ResetEditToDefaults();
	TheBuyPresets->SetPresets( TheBuyPresets->GetEditPresets() );
	TheBuyPresets->Save();
}
ConCommand cl_buy_favorite_reset( side_registry(), "cl_buy_favorite_reset", __CmdFunc_BuyPresetsReset, "Reset favorite loadouts to the default", FCVAR_CLIENTCMD_CAN_EXECUTE );

//--------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------
/**
 *  Creates the BuyPresetManager singleton
 */
BuyPresetManager::BuyPresetManager()
{
	m_loadedTeam = TEAM_UNASSIGNED;
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Resets the BuyPresetManager, loading BuyPresets from disk.  If no presets are defined, it loads the
 *  default presets instead.
 */
void BuyPresetManager::VerifyLoadedTeam( void )
{
	CCSPlayer *pPlayer = CCSPlayer::GetLocalCSPlayer();
	if ( !pPlayer )
		return;

	int playerTeam = pPlayer->GetTeamNumber();

	if ( playerTeam == m_loadedTeam )
		return;

	if ( playerTeam != TEAM_CT && playerTeam != TEAM_TERRORIST )
		return;

	m_presets.clear();

	const char *filename = "cfg/BuyPresets_TER.vdf";
	if ( playerTeam == TEAM_CT )
	{
		filename = "cfg/BuyPresets_CT.vdf";
	}

	KeyValues *data;
	KeyValues *presetKey;
	data = new KeyValues( "Presets" );
	bool fileExists = data->LoadFromFile( filesystem, filename, nullptr );

	presetKey = data->GetFirstSubKey();
	while ( presetKey )
	{
		BuyPreset preset;
		preset.Parse( presetKey );
		m_presets.push_back( preset );

		presetKey = presetKey->GetNextKey();
	}

	if ( m_presets.empty() )
	{
		const char *filename = "cfg/BuyPresetsDefault_TER.vdf";
		if ( playerTeam == TEAM_CT )
		{
			filename = "cfg/BuyPresetsDefault_CT.vdf";
		}

		KeyValues *data;
		KeyValues *presetKey;
		data = new KeyValues( "Presets" );
		data->LoadFromFile( filesystem, filename, nullptr );

		presetKey = data->GetFirstSubKey();
		while ( presetKey )
		{
			BuyPreset preset;
			preset.Parse( presetKey );
			m_presets.push_back( preset );

			presetKey = presetKey->GetNextKey();
		}

		data->deleteThis();
	}

	// Guarantee we have at least this many presets, even if they are blank
	while ( std::ssize( m_presets ) < NUM_PRESETS )
	{
		BuyPreset preset;
		m_presets.push_back( preset );
	}

	data->deleteThis();
	m_editPresets = m_presets;

	if ( !fileExists )
		Save();
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Loads the default presets into the editing presets (e.g. when hitting the "Use Defaults" button)
 */
void BuyPresetManager::ResetEditToDefaults( void )
{
	VerifyLoadedTeam();

	CCSPlayer *pPlayer = CCSPlayer::GetLocalCSPlayer();
	if ( !pPlayer )
		return;

	int playerTeam = pPlayer->GetTeamNumber();

	if ( playerTeam != TEAM_CT && playerTeam != TEAM_TERRORIST )
		return;

	m_editPresets.clear();

	const char *filename = "cfg/BuyPresetsDefault_TER.vdf";
	if ( playerTeam == TEAM_CT )
	{
		filename = "cfg/BuyPresetsDefault_CT.vdf";
	}

	KeyValues *data;
	KeyValues *presetKey;
	data = new KeyValues( "Presets" );
	data->LoadFromFile( filesystem, filename, nullptr );

	presetKey = data->GetFirstSubKey();
	while ( presetKey )
	{
		BuyPreset preset;
		preset.Parse( presetKey );
		m_editPresets.push_back( preset );

		presetKey = presetKey->GetNextKey();
	}

	data->deleteThis();
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Saves the current BuyPresets to disk
 */
void BuyPresetManager::Save()
{
	CCSPlayer *pPlayer = CCSPlayer::GetLocalCSPlayer();
	if ( !pPlayer )
		return;

	const char *filename = "cfg/BuyPresets_TER.vdf";
	switch ( pPlayer->GetTeamNumber() )
	{
	case TEAM_CT:
		filename = "cfg/BuyPresets_CT.vdf";
		break;
	case TEAM_TERRORIST:
		filename = "cfg/BuyPresets_TER.vdf";
		break;
	default:
		return; // don't bother saving presets unless we're on a team
	}

	KeyValues *data = new KeyValues( "Presets" );
	for( int i=0; i<std::ssize( m_presets ); ++i )
	{
		m_presets[i].Save( data );
	}
	data->SaveToFile( filesystem, filename, nullptr );
	data->deleteThis();
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Returns the specified "live" preset, or NULL if it doesn't exist
 */
const BuyPreset * BuyPresetManager::GetPreset( int index ) const
{
	if ( index < 0 || index >= std::ssize( m_presets ) )
	{
		return nullptr;
	}

	return &(m_presets[index]);
}

//--------------------------------------------------------------------------------------------------------------
void BuyPresetManager::SetPreset( int index, const BuyPreset *preset )
{
	if ( index < 0 || index >= std::ssize( m_presets ) || !preset )
	{
		return;
	}

	m_presets[index] = *preset;
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Returns the specified editing preset, or NULL if it doesn't exist
 */
BuyPreset * BuyPresetManager::GetEditPreset( int index )
{
	if ( index < 0 || index >= std::ssize( m_editPresets ) )
	{
		return nullptr;
	}

	return &(m_editPresets[index]);
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Generates and sends buy commands to buy a specific preset
 */
void BuyPresetManager::PurchasePreset( int presetIndex )
{
	if ( presetIndex >= 0 && presetIndex < std::ssize( m_presets ) )
	{
		const BuyPreset *preset = &(m_presets[presetIndex]);

		int setIndex;
		for ( setIndex = 0; setIndex < preset->GetNumSets(); ++setIndex )
		{
			// Try to buy this weapon set.
			const WeaponSet *itemSet = preset->GetSet( setIndex );
			if ( itemSet )
			{
				int currentCost;
				WeaponSet currentSet;
				itemSet->GetCurrent( currentCost, currentSet );
				if ( currentCost > 0 )
				{
					PRESET_DEBUG( "cl_buy_favorite: buying %ls for a total of $%d.\n",
						preset->GetName(),
						currentCost );
					char buf[BUY_PRESET_COMMAND_LEN];
					currentSet.GenerateBuyCommands( buf );

					// Send completed string
					PRESET_DEBUG( "%s\n", buf );
					engine->ClientCmd( buf );
					return;
				}
				else if ( currentCost == 0 )
				{
					CCSPlayer *pPlayer = CCSPlayer::GetLocalCSPlayer();
					if ( pPlayer )
					{
						pPlayer->EmitSound( "BuyPreset.AlreadyBought" );
					}

					// We have everything already.  Let the player know.
					if ( setIndex == 0 )
					{
						PRESET_DEBUG( "cl_buy_favorite: already have a complete %ls set.\n", preset->GetName() );
					}
					else
					{
						PRESET_DEBUG( "cl_buy_favorite: can't afford anything better from %ls.\n", preset->GetName() );
					}
					return;
				}
			}
		}

		CCSPlayer *pPlayer = CCSPlayer::GetLocalCSPlayer();
		if ( pPlayer )
		{
			pPlayer->EmitSound( "BuyPreset.CantBuy" );
		}

		PRESET_DEBUG( "cl_buy_favorite: can't afford anything better from %ls.\n", preset->GetName() );
		return;
	}

	// We failed to buy anything.  Let the player know.
	CCSPlayer *pPlayer = CCSPlayer::GetLocalCSPlayer();
	if ( pPlayer )
	{
		pPlayer->EmitSound( "BuyPreset.CantBuy" );
	}

	PRESET_DEBUG( "cl_buy_favorite: preset %d doesn't exist.\n", presetIndex );
}

//--------------------------------------------------------------------------------------------------------------
