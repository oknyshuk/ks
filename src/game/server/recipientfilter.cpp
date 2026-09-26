//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "cbase.h"
#include "recipientfilter.h"
#include "team.h"
#include "tier1/bit_ops.h"
#include "ipredictionsystem.h"

static IPredictionSystem g_RecipientFilterPredictionSystem;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CRecipientFilter::CRecipientFilter()
{
	Reset();
}

CRecipientFilter::~CRecipientFilter()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : src - 
//-----------------------------------------------------------------------------
void CRecipientFilter::CopyFrom( const CRecipientFilter& src )
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
void CRecipientFilter::Reset( void )
{
	m_bReliable			= false;
	m_bInitMessage		= false;
	m_Recipients.clear();
	m_bUsingPredictionRules = false;
	m_bIgnorePredictionCull = false;
}

void CRecipientFilter::MakeReliable( void )
{
	m_bReliable = true;
}

bool CRecipientFilter::IsReliable( void ) const
{
	return m_bReliable;
}

int CRecipientFilter::GetRecipientCount( void ) const
{
	return std::ssize( m_Recipients );
}

int	CRecipientFilter::GetRecipientIndex( int slot ) const
{
	if ( slot < 0 || slot >= GetRecipientCount() )
		return -1;

	return m_Recipients[ slot ];
}

void CRecipientFilter::AddAllPlayers( void )
{
	m_Recipients.clear();

	int i;
	for ( i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if ( !pPlayer )
		{
			continue;
		}

		AddRecipient( pPlayer );
	}
}

void CRecipientFilter::AddRecipient( CBasePlayer *player )
{
	Assert( player );

	if ( !player )
		return;

	int index = player->entindex();

	// If we're predicting and this is not the first time we've predicted this sound
	//  then don't send it to the local player again.
	if ( m_bUsingPredictionRules )
	{
		// Only add local player if this is the first time doing prediction
		if ( g_RecipientFilterPredictionSystem.GetSuppressHost() == player )
		{
			return;
		}
	}

	// Already in list
	if ( ks::find_index( m_Recipients, index ) != -1 )
		return;

	m_Recipients.push_back( index );
}

void CRecipientFilter::RemoveAllRecipients( void )
{
	m_Recipients.clear();
}

void CRecipientFilter::RemoveRecipient( CBasePlayer *player )
{
	Assert( player );
	if ( player )
	{
		int index = player->entindex();

		// Remove it if it's in the list
		ks::erase_first( m_Recipients, index );
	}
}

void CRecipientFilter::RemoveRecipientByPlayerIndex( int playerindex )
{
	Assert( playerindex >= 1 && playerindex <= MAX_PLAYERS );

	ks::erase_first( m_Recipients, playerindex );
}

void CRecipientFilter::AddRecipientsByTeam( CTeam *team )
{
	Assert( team );

	int i;
	int c = team->GetNumPlayers();
	for ( i = 0 ; i < c ; i++ )
	{
		CBasePlayer *player = team->GetPlayer( i );
		if ( !player )
			continue;

		AddRecipient( player );
	}
}

void CRecipientFilter::RemoveRecipientsByTeam( CTeam *team )
{
	Assert( team );

	int i;
	int c = team->GetNumPlayers();
	for ( i = 0 ; i < c ; i++ )
	{
		CBasePlayer *player = team->GetPlayer( i );
		if ( !player )
			continue;

		RemoveRecipient( player );
	}
}

void CRecipientFilter::RemoveRecipientsNotOnTeam( CTeam *team )
{
	Assert( team );

	int i;
	for ( i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *player = UTIL_PlayerByIndex( i );
		if ( !player )
			continue;

		if ( player->GetTeam() != team )
		{
			RemoveRecipient( player );
		}
	}
}

void CRecipientFilter::AddPlayersFromBitMask( CPlayerBitVec& playerbits )
{
	int index = ks::next_set_bit( playerbits, 0 );

	while ( index > -1 )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( index + 1 );
		if ( pPlayer )
		{
			AddRecipient( pPlayer );
		}

		index = ks::next_set_bit( playerbits, index + 1 );
	}
}

void CRecipientFilter::RemovePlayersFromBitMask( CPlayerBitVec& playerbits )
{
	int index = ks::next_set_bit( playerbits, 0 );

	while ( index > -1 )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( index + 1 );
		if ( pPlayer )
		{
			RemoveRecipient( pPlayer );
		}

		index = ks::next_set_bit( playerbits, index + 1 );
	}
}

void CRecipientFilter::AddRecipientsByPVS( const Vector& origin )
{
	if ( gpGlobals->maxClients == 1 )
	{
		AddAllPlayers();
	}
	else
	{
		CPlayerBitVec playerbits;
		engine->Message_DetermineMulticastRecipients( false, origin, playerbits );
		AddPlayersFromBitMask( playerbits );
	}
}

void CRecipientFilter::RemoveRecipientsByPVS( const Vector& origin )
{
	if ( gpGlobals->maxClients == 1 )
	{
		m_Recipients.clear();
	}
	else
	{
		CPlayerBitVec playerbits;
		engine->Message_DetermineMulticastRecipients( false, origin, playerbits );
		RemovePlayersFromBitMask( playerbits );
	}
}



void CRecipientFilter::AddRecipientsByPAS( const Vector& origin )
{
	if ( gpGlobals->maxClients == 1 )
	{
		AddAllPlayers();
	}
	else
	{
		CPlayerBitVec playerbits;
		engine->Message_DetermineMulticastRecipients( true, origin, playerbits );
		AddPlayersFromBitMask( playerbits );
	}
}

bool CRecipientFilter::IsInitMessage( void ) const
{
	return m_bInitMessage;
}

void CRecipientFilter::MakeInitMessage( void )
{
	m_bInitMessage = true;
}

void CRecipientFilter::UsePredictionRules( void )
{
	if ( m_bUsingPredictionRules )
		return;

	m_bUsingPredictionRules = true;

	// Cull list now, if needed
	if ( GetRecipientCount() == 0 )
		return;

	CBasePlayer *pPlayer = ToBasePlayer( (CBaseEntity*)g_RecipientFilterPredictionSystem.GetSuppressHost() );

	if ( pPlayer)
	{
		RemoveRecipient( pPlayer );

		if ( pPlayer->IsSplitScreenPlayer() )
		{
			RemoveRecipient( pPlayer->GetSplitScreenPlayerOwner() );
		}
		else
		{
			std::vector< CHandle< CBasePlayer > > &players = pPlayer->GetSplitScreenAndPictureInPicturePlayers();
			for ( int i = 0; i < std::ssize( players ); ++i )
			{
				CBasePlayer *pl = players[i];
				if ( !pl )
				{
					continue;
				}
				RemoveRecipient( pl );
			}
		}
	}
}

void CRecipientFilter::RemoveSplitScreenPlayers()
{
	for ( int i = GetRecipientCount() - 1; i >= 0; --i )
	{
		int idx = m_Recipients[ i ];
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( idx );
		if ( !pPlayer || !pPlayer->IsSplitScreenPlayer() )
			continue;

		m_Recipients.erase( m_Recipients.begin() + i );
	}
}

// THIS FUNCTION IS SUFFICIENT FOR PORTAL2 SPECIFIC CIRCUMSTANCES
// AND MAY OR MAY NOT FUNCTION AS EXPECTED WHEN USED WITH MULTIPLE
// SPLITSCREEN CLIENTS NETWORKED TOGETHER, ETC.
void CRecipientFilter::ReplaceSplitScreenPlayersWithOwners()
{
	// coop
	if( gpGlobals->maxClients >= 2 ) 
	{
		int count = std::ssize( m_Recipients );
		ks::small_vector< int, 4 > playerOwners;
		for( int i = 0; i < count; ++i )
		{
			// If this is a split screen player
			CBasePlayer* pPlayer = UTIL_PlayerByIndex( m_Recipients[i] );
			if( pPlayer && pPlayer->IsSplitScreenPlayer() )
			{
				// Add its owner to the recipients. If it doesn't exist, abort.
				const CBasePlayer* pOwnerPlayer = pPlayer->GetSplitScreenPlayerOwner();
				if( pOwnerPlayer )
					playerOwners.push_back( pOwnerPlayer->entindex() );
				else
					return;
			}
		}

		if( std::ssize( playerOwners ) > 0 )
		{
			// Remove all split screen players
			RemoveSplitScreenPlayers();

			// Add all owner players
			count = std::ssize( m_Recipients );
			ks::ensure_size( m_Recipients, count + std::ssize( playerOwners ) );
			V_memcpy( m_Recipients.data() + count, playerOwners.data(), std::ssize( playerOwners ) * sizeof( int ) );

			// Remove duplicates
			RemoveDuplicateRecipients();
		}
	}
}

bool CRecipientFilter::IsUsingPredictionRules( void ) const
{
	return m_bUsingPredictionRules;
}

bool CRecipientFilter::	IgnorePredictionCull( void ) const
{
	return m_bIgnorePredictionCull;
}

void CRecipientFilter::SetIgnorePredictionCull( bool ignore )
{
	m_bIgnorePredictionCull = ignore;
}

//-----------------------------------------------------------------------------
// Purpose: Simple class to create a filter for all players on a given team 
//-----------------------------------------------------------------------------
CTeamRecipientFilter::CTeamRecipientFilter( int team, bool isReliable )
{
	if (isReliable)
		MakeReliable();

	RemoveAllRecipients();

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );

		if ( !pPlayer )
		{
			continue;
		}

		if ( team == TEAM_SPECTATOR )
		{
			if ( pPlayer->GetTeamNumber() != TEAM_SPECTATOR && !pPlayer->IsHLTV() )
				continue;
		}
		else if ( pPlayer->GetTeamNumber() != team )
		{
			//If we're in the spectator team then we should be getting whatever messages the person I'm spectating gets.
			if ( ( pPlayer->IsHLTV() || pPlayer->GetTeamNumber() == TEAM_SPECTATOR ) && (pPlayer->GetObserverMode() == OBS_MODE_IN_EYE || pPlayer->GetObserverMode() == OBS_MODE_CHASE) )
			{
				if ( pPlayer->GetObserverTarget() )
				{
					if ( pPlayer->GetObserverTarget()->GetTeamNumber() != team )
						continue;
				}
			}
			else
			{
				continue;
			}
		}

		AddRecipient( pPlayer );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : origin - 
//			ATTN_NORM - 
//-----------------------------------------------------------------------------
void CPASAttenuationFilter::Filter( const Vector& origin, float attenuation /*= ATTN_NORM*/ )
{
	// Don't crop for attenuation in single player
	if ( gpGlobals->maxClients == 1 )
		return;

	// CPASFilter adds them by pure PVS in constructor
	if ( attenuation <= 0 )
	{
		AddAllPlayers();
		return;
	}

	// Now remove recipients that are outside sound radius
	float maxAudible = ( 2 * SOUND_NORMAL_CLIP_DIST ) / attenuation;

	int c = GetRecipientCount();
	
	for ( int i = c - 1; i >= 0; i-- )
	{
		int index = GetRecipientIndex( i );

		CBaseEntity *ent = CBaseEntity::Instance( index );
		if ( !ent || !ent->IsPlayer() )
		{
			Assert( 0 );
			continue;
		}

		CBasePlayer *player = ToBasePlayer( ent );
		if ( !player )
		{
			Assert( 0 );
			continue;
		}

		// never remove the HLTV or Replay bot
		if ( player->IsHLTV() || player->IsReplay() )
			continue;

		if ( player->EarPosition().DistTo(origin) <= maxAudible )
			continue;
		if ( std::ssize( player->GetSplitScreenAndPictureInPicturePlayers() ) )
		{
			std::vector< CHandle< CBasePlayer > > &list = player->GetSplitScreenAndPictureInPicturePlayers();
			bool bSend = false;
			for ( int k = 0; k < std::ssize( list ); k++ )
			{
				CBasePlayer *pl = list[k];
				if ( !pl )
				{
					continue;
				}
				if ( pl->EarPosition().DistTo(origin) <= maxAudible )
				{
					bSend = true;
					break;
				}
			}
			if ( bSend )
				continue;
		}
		RemoveRecipient( player );
	}
}

void CRecipientFilter::RemoveDuplicateRecipients()
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

CSingleUserAndReplayRecipientFilter::CSingleUserAndReplayRecipientFilter( CBasePlayer *pAddPlayer )
{
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );

		if ( !pPlayer )
		{
			continue;
		}
		Assert( pPlayer->entindex() == i );

		if ( pPlayer == pAddPlayer || pPlayer->IsHLTV() || pPlayer->IsReplay() )
		{
			// If we're predicting and this is not the first time we've predicted this sound
			//  then don't send it to the local player again.
			Assert( !IsUsingPredictionRules ());

			m_Recipients.push_back( i );
		}
	}
}
