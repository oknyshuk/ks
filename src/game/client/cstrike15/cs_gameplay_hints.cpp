// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

#include "rocketui/rmlui.h"
#include "predictable_entity.h"
#include "util_shared.h"
#include "baseentity_shared.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "cs_gameplay_hints.h"
#include "filesystem.h"
#include "localize/ilocalize.h"
#include "cs_gamerules.h"

struct CSGameplayHint_t
{
	CSGameplayHint_t( const char* pszToken, uint32 flags ): 
	m_pszLocToken( pszToken ), m_nRequiredContextFlags( flags ), m_nDisplayCount( 0 ) {}
	const char* m_pszLocToken;
	uint32 m_nRequiredContextFlags;
	uint32 m_nDisplayCount;
};

CCSGameplayHints::CCSGameplayHints()
{
	m_pHintKV = nullptr;
}

CCSGameplayHints::~CCSGameplayHints()
{
	Cleanup();
}

CCSGameplayHints g_CSGameplayHints;


uint32 CCSGameplayHints::GetCurrentContextFlags( void )
{
	uint32 flags = 0;

	if ( CSGameRules() && CSGameRules()->IsBombDefuseMap() )
		flags |= HINT_CONTEXT_BOMB_MAP;
	if ( CSGameRules() && CSGameRules()->IsHostageRescueMap() )
		flags |= HINT_CONTEXT_HOSTAGE_MAP;
	if ( CSGameRules() && CSGameRules()->IsPlayingGunGame() )
		flags |= HINT_CONTEXT_GUNGAME;

	return flags;
}

uint32 ContextEntryToBitFlag( const char* szName )
{
	if ( V_stricmp( szName, "bomb_map_only" ) == 0 )
	{
		return CCSGameplayHints::HINT_CONTEXT_BOMB_MAP;
	}
	else if ( V_stricmp( szName, "hostage_map_only" ) == 0 )
	{
		return CCSGameplayHints::HINT_CONTEXT_HOSTAGE_MAP;
	}
	else if ( V_stricmp( szName, "gungame_map_only" ) == 0 )
	{
		return CCSGameplayHints::HINT_CONTEXT_GUNGAME;
	}
	else
	{
		Warning( "HintConfig.txt contains invalid context setting: %s\n", szName );
		contract_assert( 0 );
	}

	// Show it as a failsafe.
	return CCSGameplayHints::HINT_CONTEXT_ALWAYS_SHOW; 
}

uint32 BuildContextFlags( KeyValues *pContextKeys )
{
	if ( pContextKeys == nullptr || pContextKeys->GetFirstSubKey() == nullptr )
		return CCSGameplayHints::HINT_CONTEXT_ALWAYS_SHOW; 

	uint32 nFlags = 0;
	for ( KeyValues *entry = pContextKeys->GetFirstSubKey(); entry != nullptr; entry = entry->GetNextKey() )
	{
		if ( entry->GetInt() > 0 )
		{
			nFlags |= ContextEntryToBitFlag( entry->GetName() );
		}
	}
	return nFlags;
}

void CCSGameplayHints::PostInit()
{
	KeyValues *m_pHintKV = new KeyValues( "HintConfig.txt" );
	if ( m_pHintKV->LoadFromFile( g_pFullFileSystem, "resource/HintConfig.txt" ) )
	{
		KeyValues *hints = m_pHintKV->FindKey( "hints" );
		if ( hints )
		{
			for ( KeyValues *entry = hints->GetFirstSubKey(); entry != nullptr; entry = entry->GetNextKey() )
			{
				const char* szLocToken = entry->GetString( "locToken", nullptr );
				if ( szLocToken )
				{
					const wchar_t *wszText = g_pLocalize->Find( szLocToken );
					contract_assert( wszText );
					// Sanity check length here
					if ( wszText )
					{
						uint32 nContextFlags = BuildContextFlags( entry->FindKey( "context", false ) );
						m_HintList.push_back( std::make_unique< CSGameplayHint_t >( szLocToken, nContextFlags ) );
					}
					else
					{
						Warning( "Bad localization token in resource/HintConfig.txt: '%s'\n", szLocToken );
					}
				}
			}
		}
	}
	
	contract_assert( std::ssize( m_HintList ) > 0 );
 }

void CCSGameplayHints::Cleanup( void )
{
	m_HintList.clear();
	if ( m_pHintKV )
	{
		m_pHintKV->deleteThis();
		m_pHintKV = nullptr;
	}
}
void CCSGameplayHints::Shutdown()
{
	Cleanup();
}

const char* CCSGameplayHints::GetRandomLeastPlayedHint( void )
{
	if ( m_HintList.empty() )
		return nullptr;

	// Sort by times 'used' and pick among the lowest counts. Counts aren't stored and will 
	// re-zero every time this the CCSGameplayHints class is created (so, every app launch).
	std::sort( m_HintList.begin(), m_HintList.end(), []( const auto &a, const auto &b ) { return a->m_nDisplayCount < b->m_nDisplayCount; } );

	uint32 contextFlags = GetCurrentContextFlags();

	std::vector< CSGameplayHint_t* > validHints;
	// Filter out based on current context flags
	for ( const auto &pHint : m_HintList )
	{
		CSGameplayHint_t* hint = pHint.get();

		// If bits are set for required context, make sure all of them are currently met. 
		if ( hint->m_nRequiredContextFlags == HINT_CONTEXT_ALWAYS_SHOW ||
			 ( contextFlags & hint->m_nRequiredContextFlags ) == hint->m_nRequiredContextFlags )
		{
			// Early out if we found at least one valid hint and we've moved 
			// on to checking hints that have been displayed more often.
			// After the sort, the first found valid hint should have the lowest display count.
			if ( !validHints.empty() && validHints[0]->m_nDisplayCount < hint->m_nDisplayCount )
				break;

			validHints.push_back( hint );
		}
	}

	int pick = RandomInt( 0, std::ssize( validHints )-1 );
	CSGameplayHint_t *hint = validHints[pick];
	hint->m_nDisplayCount++;
	return hint->m_pszLocToken;
}
/*
static void pick_hint( const CCommand &args );
static ConCommand pick_hint_command( "pick_hint", pick_hint, "" );
static void pick_hint( const CCommand &args )
{
	Msg( "Hint: '%s'\n", g_CSGameplayHints.GetRandomLeastPlayedHint() );
}
*/
