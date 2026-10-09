// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "rocketui/rmlui.h"
import ks.core;
import std;
#include "predictable_entity.h"
#include "cdll_util.h"
#include "util_shared.h"
#include "baseentity_shared.h"
#include "c_basecombatweapon.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "c_baseplayer.h"
#include "vphysics_interface.h"
#include "cdll_client_int.h"

#include "buy_preset_debug.h"
#include "buy_presets.h"
#include "weapon_csbase.h"
#include "filesystem.h"
#include "localize/ilocalize.h"
#include "c_cs_player.h"

BuyPresetManager *TheBuyPresets = nullptr;

//--------------------------------------------------------------------------------------------------------------
ConVar cl_buy_favorite_quiet( "cl_buy_favorite_quiet", "0", FCVAR_ARCHIVE | FCVAR_CLIENTCMD_CAN_EXECUTE, "Skips the prompt when saving a buy favorite in the buy menu" );
ConVar cl_buy_favorite_nowarn( "cl_buy_favorite_nowarn", "0", FCVAR_ARCHIVE | FCVAR_CLIENTCMD_CAN_EXECUTE, "Skips the error prompt when saving an invalid buy favorite" );

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
static ConCommand cl_buy_favorite_command( "cl_buy_favorite", cl_buy_favorite, "Purchase a favorite weapon/equipment loadout", FCVAR_CLIENTCMD_CAN_EXECUTE );
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
static ConCommand cl_buy_favorite_set_command( "cl_buy_favorite_set", cl_buy_favorite_set, "Saves the current loadout as a favorite", FCVAR_CLIENTCMD_CAN_EXECUTE );
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

	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
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

	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pPlayer || ( pPlayer->GetTeamNumber() != TEAM_CT && pPlayer->GetTeamNumber() != TEAM_TERRORIST ) )
	{
		return;
	}

	TheBuyPresets->ResetEditToDefaults();
	TheBuyPresets->SetPresets( TheBuyPresets->GetEditPresets() );
	TheBuyPresets->Save();
}
ConCommand cl_buy_favorite_reset( "cl_buy_favorite_reset", __CmdFunc_BuyPresetsReset, "Reset favorite loadouts to the default", FCVAR_CLIENTCMD_CAN_EXECUTE );


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
	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
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

	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
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
	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
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
					C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
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

		C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
		if ( pPlayer )
		{
			pPlayer->EmitSound( "BuyPreset.CantBuy" );
		}

		PRESET_DEBUG( "cl_buy_favorite: can't afford anything better from %ls.\n", preset->GetName() );
		return;
	}

	// We failed to buy anything.  Let the player know.
	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( pPlayer )
	{
		pPlayer->EmitSound( "BuyPreset.CantBuy" );
	}

	PRESET_DEBUG( "cl_buy_favorite: preset %d doesn't exist.\n", presetIndex );
}


//--------------------------------------------------------------------------------------------------------------
