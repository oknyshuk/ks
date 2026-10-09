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
#include "itempents.h"
#include "vphysics_interface.h"
#include "c_recipientfilter.h"
#include "debugoverlay_shared.h"
#include "prediction.h"

static IPredictionSystem g_RecipientFilterPredictionSystem;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_RecipientFilter::C_RecipientFilter()
{
	Reset();
}

C_RecipientFilter::~C_RecipientFilter()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : src - 
//-----------------------------------------------------------------------------
void C_RecipientFilter::CopyFrom( const C_RecipientFilter& src )
{
	m_bReliable = src.IsReliable();
	m_bInitMessage = src.IsInitMessage();

	m_bUsingPredictionRules = src.IsUsingPredictionRules();
	m_bIgnorePredictionCull = src.IgnorePredictionCull();

	int c = src.GetRecipientCount();
	for ( int i = 0; i < c; ++i )
	{
		m_Recipients.push_back( src.GetRecipientIndex( i ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_RecipientFilter::Reset( void )
{
	m_bReliable			= false;
	m_Recipients.clear();
	m_bUsingPredictionRules = false;
	m_bIgnorePredictionCull = false;
}

void C_RecipientFilter::MakeReliable( void )
{
	m_bReliable = true;
}

bool C_RecipientFilter::IsReliable( void ) const
{
	return m_bReliable;
}

int C_RecipientFilter::GetRecipientCount( void ) const
{
	return std::ssize( m_Recipients );
}

int	C_RecipientFilter::GetRecipientIndex( int slot ) const
{
	if ( slot < 0 || slot >= GetRecipientCount() )
		return -1;

	return m_Recipients[ slot ];
}

void C_RecipientFilter::AddAllPlayers( void )
{
	if ( !C_BasePlayer::HasAnyLocalPlayer() )
		return;

	m_Recipients.clear();
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
	{
		AddRecipient( C_BasePlayer::GetLocalPlayer( hh ) );
	}
}

void C_RecipientFilter::AddRecipient( C_BasePlayer *player )
{
	if ( !player )
		return;

	int index = player->index;

	// If we're predicting and this is not the first time we've predicted this sound
	//  then don't send it to the local player again.
	if ( m_bUsingPredictionRules )
	{
		contract_assert( C_BasePlayer::IsLocalPlayer( player ) );
		contract_assert( prediction->InPrediction() );

		// Only add local player if this is the first time doing prediction
		if ( !g_RecipientFilterPredictionSystem.CanPredict() )
		{
			return;
		}
	}

	// Already in list
	if ( ks::find_index( m_Recipients, index ) != -1 )
		return;

	// this is a client side filter, only add the local player
	if ( !C_BasePlayer::IsLocalPlayer( player ) )
		return;

	m_Recipients.push_back( index );
}

void C_RecipientFilter::RemoveRecipient( C_BasePlayer *player )
{
	if ( !player )
		return;

	int index = player->index;

	// Remove it if it's in the list
	ks::erase_first( m_Recipients, index );
}

void C_RecipientFilter::AddRecipientsByTeam( C_Team *team )
{
	AddAllPlayers();
}

void C_RecipientFilter::RemoveRecipientsByTeam( C_Team *team )
{
	contract_assert( 0 );
}


void C_RecipientFilter::AddPlayersFromBitMask( CPlayerBitVec& playerbits )
{
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
	{
		C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer( hh );
		if ( !pPlayer )
			continue;

		// only add the local player on client side
		if ( !playerbits[ pPlayer->index ] )
			continue;

		AddRecipient( pPlayer );
	}
}

void C_RecipientFilter::AddRecipientsByPVS( const Vector& origin )
{
	AddAllPlayers();
}

void C_RecipientFilter::AddRecipientsByPAS( const Vector& origin )
{
	AddAllPlayers();
}

void C_RecipientFilter::UsePredictionRules( void )
{
	if ( m_bUsingPredictionRules )
		return;

	if ( !prediction->InPrediction() )
	{
		contract_assert( 0 );
		return;
	}

	if ( !C_BasePlayer::HasAnyLocalPlayer() )
	{
		contract_assert( 0 );
		return;
	}

	m_bUsingPredictionRules = true;

	// Cull list now, if needed
	int c = GetRecipientCount();
	if ( c == 0 )
		return;

	if ( !g_RecipientFilterPredictionSystem.CanPredict() )
	{
		FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
		{
			RemoveRecipient( C_BasePlayer::GetLocalPlayer( hh ) );
		}
	}
}

void C_RecipientFilter::RemoveSplitScreenPlayers()
{
	for ( int i = GetRecipientCount() - 1; i >= 0; --i )
	{
		int idx = m_Recipients[ i ];
		C_BasePlayer *pPlayer = UTIL_PlayerByIndex( idx );
		if ( !pPlayer || !pPlayer->IsSplitScreenPlayer() )
			continue;

		m_Recipients.erase( m_Recipients.begin() + i );
	}
}

// THIS FUNCTION IS SUFFICIENT FOR PORTAL2 SPECIFIC CIRCUMSTANCES
// AND MAY OR MAY NOT FUNCTION AS EXPECTED WHEN USED WITH MULTIPLE
// SPLITSCREEN CLIENTS NETWORKED TOGETHER, ETC.
void C_RecipientFilter::ReplaceSplitScreenPlayersWithOwners()
{
	// coop
	if( gpGlobals->maxClients >= 2 && C_BasePlayer::HasAnyLocalPlayer() ) 
	{
		bool addedHostPlayer = false;
		const int count = std::ssize( m_Recipients );
		for( int i = 0; i < count; ++i )
		{
			// If this is a split screen player
			CBasePlayer* pPlayer = UTIL_PlayerByIndex( m_Recipients[i] );
			if( pPlayer && pPlayer->IsSplitScreenPlayer() )
			{
				// Add the host player, if it exists, to the filter
				C_BasePlayer* pHostPlayer = C_BasePlayer::GetLocalPlayer( 0 );
				if( pHostPlayer )
				{
					m_Recipients.push_back( pHostPlayer->entindex() );
					addedHostPlayer = true;
				}
				break;
			}
		}

		if( addedHostPlayer )
		{
			// Remove all split screen players
			RemoveSplitScreenPlayers();

			// Remove duplicates
			RemoveDuplicateRecipients();
		}
	}
}
bool C_RecipientFilter::IsUsingPredictionRules( void ) const
{
	return m_bUsingPredictionRules;
}

bool C_RecipientFilter::IgnorePredictionCull( void ) const
{
	return m_bIgnorePredictionCull;
}

void C_RecipientFilter::SetIgnorePredictionCull( bool ignore )
{
	m_bIgnorePredictionCull = ignore;
}

void C_RecipientFilter::RemoveDuplicateRecipients()
{
	for( int i = 0; i < std::ssize( m_Recipients ); ++i )
	{
		int currentElem = m_Recipients[i];
		for( int j = std::ssize( m_Recipients ) - 1; j > i ; --j )
		{
			if( m_Recipients[j] == currentElem )
				ks::swap_remove( m_Recipients, j );
		}
	}
}

CLocalPlayerFilter::CLocalPlayerFilter()
{
	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		if ( C_BasePlayer::GetLocalPlayer() )
		{
			AddRecipient( C_BasePlayer::GetLocalPlayer() );
		}
	}
}
