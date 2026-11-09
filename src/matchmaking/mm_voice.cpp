// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

import ks.tier1;
#include "mm_voice.h"

import std;

static XUID LocalPlayerXuid()
{
	IPlayerLocal *pLocal = g_pPlayerManager->GetLocalPlayer( XBX_GetPrimaryUserId() );
	return pLocal ? pLocal->GetXUID() : 0;
}

//
// Construction/destruction
//

CMatchVoice::CMatchVoice()
{
	;
}

CMatchVoice::~CMatchVoice()
{
	;
}

static CMatchVoice g_MatchVoice;
CMatchVoice *g_pMatchVoice = &g_MatchVoice;

//
// Implementation
//

// Whether remote player talking can be visualized / audible
bool CMatchVoice::CanPlaybackTalker( XUID xuidTalker )
{
	if ( IsMachineMutingLocalTalkers( xuidTalker ) )
	{
		return false;
	}

	if ( IsMachineMuted( xuidTalker ) )
	{
		return false;
	}

	return true;
}

// Whether we are explicitly muting a remote player
bool CMatchVoice::IsTalkerMuted( XUID xuidTalker )
{
	if ( ks::find_index( m_arrMutedTalkers, xuidTalker ) != -1 )
	{
		return true;
	}

	return false;
}

// Whether we are muting any player on the player's machine
bool CMatchVoice::IsMachineMuted( XUID xuidPlayer )
{
	return IsTalkerMuted( xuidPlayer );
}

// Check player-player voice privileges for machine blocking purposes
bool CMatchVoice::IsTalkerMutedWithPrivileges( int dwCtrlr, XUID xuidTalker )
{

	if ( ks::find_index( m_arrMutedTalkers, xuidTalker ) != -1 )
	{
		return true;
	}

	return false;
}

// Check if player machine is muting any of local players
bool CMatchVoice::IsMachineMutingLocalTalkers( XUID xuidPlayer )
{
	// Find the session and the talker within session members
	IMatchSession *pMatchSession = g_pMatchFramework->GetMatchSession();
	if ( !pMatchSession )
		return false;

	KeyValues *pSettings = pMatchSession->GetSessionSettings();

	KeyValues *pMachine = nullptr;
	SessionMembersFindPlayer( pSettings, xuidPlayer, &pMachine );
	if ( !pMachine )
		return false;

	// Find the local player record in the session
	XUID xuidLocalId = LocalPlayerXuid();
	KeyValues *pLocalMachine = nullptr;
	SessionMembersFindPlayer( pSettings, xuidLocalId, &pLocalMachine );
	if ( !pLocalMachine || pLocalMachine == pMachine )
		return false;
	int numLocalPlayers = pLocalMachine->GetInt( "numPlayers" );

	// Check the mutelist on the machine
	if ( KeyValues *pMutelist = pMachine->FindKey( "Mutelist" ) )
	{
		for ( KeyValues *val = pMutelist->GetFirstValue(); val; val = val->GetNextValue() )
		{
			XUID xuidMuted = val->GetUint64();
			if ( !xuidMuted )
				continue;

			for ( int iLocal = 0; iLocal < numLocalPlayers; ++ iLocal )
			{
				XUID xuidLocal = pLocalMachine->GetUint64( CFmtStr( "player%d/xuid", iLocal ) );
				if ( xuidMuted == xuidLocal )
				{
					return true;
				}
			}
		}
	}

	return false;
}

// Whether voice recording mode is currently active
bool CMatchVoice::IsVoiceRecording()
{
	return false;
}

// Enable or disable voice recording
void CMatchVoice::SetVoiceRecording( bool bRecordingEnabled )
{
}

// Enable or disable voice mute for a given talker
void CMatchVoice::MuteTalker( XUID xuidTalker, bool bMute )
{
	if ( !xuidTalker )
	{
		if ( !bMute )
			m_arrMutedTalkers.clear();
	}
	else
	{
		ks::swap_erase_first( m_arrMutedTalkers, xuidTalker );
		if ( bMute )
		{
			m_arrMutedTalkers.push_back( xuidTalker );
		}
	}
	
	g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues( "OnSysMuteListChanged" ) );
}

static void voice_reset_mutelist( const CCommand &args );
static ConCommand voice_reset_mutelist_command( "voice_reset_mutelist", voice_reset_mutelist, "Reset all mute information for all players who were ever muted." );
static void voice_reset_mutelist( const CCommand &args )
{
	g_pMatchVoice->MuteTalker( 0ull, false );
	Msg( "Mute list cleared.\n" );
}

static void voice_mute( const CCommand &args );
static ConCommand voice_mute_command( "voice_mute", voice_mute, "Mute a specific user" );
static void voice_mute( const CCommand &args )
{
	if ( args.ArgC() != 2 )
	{
		goto usage;
	}
	else
	{
		int iUserId = V_atoi( args.Arg( 1 ) );
		player_info_t pi;
		if ( !g_pMatchExtensions->GetIVEngineClient()->GetPlayerInfo( iUserId, &pi ) || !pi.xuid )
		{
			Msg( "Player# is invalid or refers to a bot, please use \"voice_show_mute\" command.\n" );
			goto usage;
		}

		g_pMatchVoice->MuteTalker( pi.xuid, true );
		if ( !g_pMatchExtensions->GetIVEngineClient()->GetDemoPlaybackParameters() )
		{
			Msg( "%s is now muted.\n", pi.name );
		}
		return;
	}

usage:
	Msg( "Example usage: voice_mute player#   -   where player# is a number that you can find with \"voice_show_mute\" command.\n" );
}

static void voice_unmute( const CCommand &args );
static ConCommand voice_unmute_command( "voice_unmute", voice_unmute, "Unmute a specific user, or `all` to unmute all connected players." );
static void voice_unmute( const CCommand &args )
{
	if ( args.ArgC() != 2 )
	{
		goto usage;
	}
	else
	{
		if ( !V_stricmp( "all", args.Arg(1) ) )
		{
			XUID xuidLocal = LocalPlayerXuid();
			int maxClients = g_pMatchExtensions->GetIVEngineClient()->GetMaxClients();
			for ( int i = 1; i <= maxClients; ++ i )
			{
				// Get the player info from the engine	
				player_info_t pi;
				if ( !g_pMatchExtensions->GetIVEngineClient()->GetPlayerInfo( i, &pi ) )
					continue;
				if ( !pi.xuid )
					continue;
				if ( pi.xuid == xuidLocal )
					continue;

				g_pMatchVoice->MuteTalker( pi.xuid, false );
			}
			Msg( "All connected players have been unmuted.\n" );
			return;
		}

		int iUserId = V_atoi( args.Arg( 1 ) );
		player_info_t pi;
		if ( !g_pMatchExtensions->GetIVEngineClient()->GetPlayerInfo( iUserId, &pi ) || !pi.xuid )
		{
			Msg( "Player# is invalid or refers to a bot, please use \"voice_show_mute\" command.\n" );
			goto usage;
		}

		g_pMatchVoice->MuteTalker( pi.xuid, false );
		if ( !g_pMatchExtensions->GetIVEngineClient()->GetDemoPlaybackParameters() )
		{
			Msg( "%s is now unmuted.\n", pi.name );
		}
		return;
	}

usage:
	Msg( "Example usage: voice_unmute {player#|all}   -   where player# is a number that you can find with \"voice_show_mute\" command, or all to unmute all connected players.\n" );
}

static void voice_show_mute( const CCommand &args );
static ConCommand voice_show_mute_command( "voice_show_mute", voice_show_mute, "Show whether current players are muted." );
static void voice_show_mute( const CCommand &args )
{
	if ( g_pMatchExtensions->GetIVEngineClient()->GetDemoPlaybackParameters() )
		return;

	bool bPrinted = false;
	XUID xuidLocal = LocalPlayerXuid();
	int maxClients = g_pMatchExtensions->GetIVEngineClient()->GetMaxClients();
	for ( int i = 1; i <= maxClients; ++ i )
	{
		// Get the player info from the engine	
		player_info_t pi;
		if ( !g_pMatchExtensions->GetIVEngineClient()->GetPlayerInfo( i, &pi ) )
			continue;
		if ( !pi.xuid )
			continue;
		if ( pi.xuid == xuidLocal )
			continue;

		if ( !bPrinted )
		{
			bPrinted = true;
			Msg( "Player#     Player Name\n" );
			Msg( "-------     ----------------\n" );
		}

		Msg( " % 2d %s %s\n", i, g_pMatchVoice->IsTalkerMuted( pi.xuid ) ? "(muted)" : "       ", pi.name );
	}
	
	if ( bPrinted )
	{
		Msg( "-------     ----------------\n" );
	}
	else
	{
		Msg( "No players currently connected who can be muted.\n" );
	}
}

//-----------------------------------------------------------------------------
#include "facade/imatchvoice.inl"
