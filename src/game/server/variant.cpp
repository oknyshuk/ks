// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2008 Valve Corporation

module;
#include <stdlib.h>
#include <math.h>
#include <stddef.h>
#include "datamap.h"
#include "edict.h"
#include "ivoiceserver.h"
#include "networkvar.h"
#include "usermessages_schema.h"
#include "datamodel/dmattributetypes.h"
#include "eiface.h"
#include "igameevents.h"
#include "responserules/response_types.h"
#include "server_class.h"
#include "studio.h"
#include "vphysics_interface.h"
#include "ai_criteria.h"
#include "baseentity.h"
#include "entity_dispatch.h"
#include "entity_dispatch_list.h"
#include "entity_kinds_dump.h"
#include "reflect_sendtable.h"
#include "util.h"
#include "vote_controller.h"
#include "vscript/ivscript.h"
#include "vscript_game.h"
#include "reflect.h"
#include "reflect_datamap.h"
#include "declare_class.h"
#include "editor_sendcommand.h"
#include "model_types.h"
#include "worldsize.h"
#include "filesystem.h"
#include "string_t.h"
#include "tier2/tier2_logging.h"
#include "shareddefs.h"
module ks.server;

import :baseentity;
import :fish;
import :sharedvar;
import :vscript;
import ks.game.mapentities_shared;
import :nav_4;
import :player;
import ks.game.shareddefs;
import std;
import ks.core.dbg;
import ks.core.platform;
import ks.platform;
import ks.simd;
import ks.tier1.bit_ops;
import ks.tier1.convar;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.reflect_wirevar;
import ks.tier1.registry;
import :cstrike15.bot.bot;
import :entityutil;
import ks.game.gamemovement;
import :entity_2;
import ks.core.types;
import ks.tier1.iconvar;
import ks.tier1.strtools;
import ks.tier1.vector_ops;
import :cstrike15.cs_player;
import :cstrike15.cs_playeranimstate;
import :entity_dispatch;
import :entity_kind;
import :cstrike15.bot.cs_bot;
import :cstrike15.cs_player_2;
import :cstrike15.hostage.cs_simple_hostage;
import :econ.econ_item_schema_minimal;
import ks.core.cmdline;
import ks.core.logging;
import ks.core.tslist;
import ks.core.vprof;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.vector;
import ks.tier1.fmtstr;
import ks.tier1.keyvalues;
import ks.game.ai_activity;
import ks.tier1.byte_reader;
import ks.tier1.intern_pool;
import ks.core.mem;
import ks.core.threadtools;

class CBaseEntity;

void variant_t::SetEntity( CBaseEntity *val ) 
{ 
	eVal = val;
	fieldType = FIELD_EHANDLE; 
}

class CBaseEntity;
class CBasePlayer;

using namespace ::ks::reflect;

static constexpr auto UPDATE_INTERVAL = 0.3;

// These are stored off as CVoiceGameMgr is created and deleted.
CPlayerBitVec	g_PlayerModEnable;		// Set to 1 for each player if the player wants to use voice in this mod.
										// (If it's zero, then the server reports that the game rules are saying the
										// player can't hear anyone).

CPlayerBitVec	g_BanMasks[VOICE_MAX_PLAYERS];	// Tells which players don't want to hear each other.
												// These are indexed as clients and each bit represents a client
												// (so player entity is bit+1).

CPlayerBitVec	g_SentGameRulesMasks[VOICE_MAX_PLAYERS];	// These store the masks we last sent to each client so we can determine if
CPlayerBitVec	g_SentBanMasks[VOICE_MAX_PLAYERS];			// we need to resend them.
CPlayerBitVec	g_bWantModEnable;

ConVar voice_serverdebug( side_registry(), "voice_serverdebug", "0" );

CVoiceGameMgr g_VoiceGameMgr;

// ------------------------------------------------------------------------ //
// Static helpers.
// ------------------------------------------------------------------------ //

// Find a player with a case-insensitive name search.
static CBasePlayer* FindPlayerByName(const char *pTestName)
{
	for(int i=1; i <= gpGlobals->maxClients; i++)
	{
		edict_t *pEdict = INDEXENT(i);
		if(pEdict)
		{
			CBaseEntity *pEnt = CBaseEntity::Instance(pEdict);
			if(pEnt && pEnt->IsPlayer())
			{			
				const char *pNetName = STRING(pEnt->GetEntityName());
				if(stricmp(pNetName, pTestName) == 0)
				{
					return (CBasePlayer*)pEnt;
				}
			}
		}
	}

	return nullptr;
}

static void VoiceServerDebug( const char *pFmt, ... )
{
	char msg[4096];
	std::va_list marker;

	if( !voice_serverdebug.GetInt() )
		return;

	__builtin_va_start( marker, pFmt );
	std::vsnprintf( msg, sizeof(msg), pFmt, marker );
	__builtin_va_end( marker );

	Msg( "%s", msg );
}

CVoiceGameMgr* GetVoiceGameMgr()
{
	return &g_VoiceGameMgr;
}

// ------------------------------------------------------------------------ //
// CVoiceGameMgr.
// ------------------------------------------------------------------------ //

CVoiceGameMgr::CVoiceGameMgr()
{
	m_UpdateInterval = 0;
	m_nMaxPlayers = 0;
	m_iProximityDistance = -1;
}

CVoiceGameMgr::~CVoiceGameMgr()
{
}

bool CVoiceGameMgr::Init(
	IVoiceGameMgrHelper *pHelper,
	int maxClients)
{		  
	m_pHelper = pHelper;
	m_nMaxPlayers = VOICE_MAX_PLAYERS < maxClients ? VOICE_MAX_PLAYERS : maxClients;

	return true;
}

void CVoiceGameMgr::SetHelper(IVoiceGameMgrHelper *pHelper)
{
	m_pHelper = pHelper;
}

void CVoiceGameMgr::Update(double frametime)
{
	// Only update periodically.
	m_UpdateInterval += frametime;
	if(m_UpdateInterval < UPDATE_INTERVAL)
		return;

	UpdateMasks();
}

void CVoiceGameMgr::ClientConnected(struct edict_t *pEdict)
{
	int index = ENTINDEX(pEdict) - 1;
	
	// Clear out everything we use for deltas on this guy.
	g_bWantModEnable[index] = true;
	g_SentGameRulesMasks[index].reset();
	g_SentBanMasks[index].reset();
}

bool CVoiceGameMgr::ClientCommand( CBasePlayer *pPlayer, const CCommand &args )
{
	int playerClientIndex = pPlayer->entindex() - 1;
	if(playerClientIndex < 0 || playerClientIndex >= m_nMaxPlayers)
	{
		VoiceServerDebug( "CVoiceGameMgr::ClientCommand: cmd %s from invalid client (%d)\n", args[0], playerClientIndex );
		return true;
	}

	bool bBan = stricmp( args[0], "vban" ) == 0;
	if( bBan && args.ArgC() >= 2 )
	{
		for(int i=1; i < args.ArgC(); i++)
		{
			unsigned int mask = 0;
			std::sscanf( args[i], "%x", &mask);

			if( i <= VOICE_MAX_PLAYERS_DW )
			{
				VoiceServerDebug( "CVoiceGameMgr::ClientCommand: vban (0x%x) from %d\n", mask, playerClientIndex );
				ks::set_word( g_BanMasks[playerClientIndex], i-1, mask );
			}
			else
			{
				VoiceServerDebug( "CVoiceGameMgr::ClientCommand: invalid index (%d)\n", i );
			}
		}

		// Force it to update the masks now.
		//UpdateMasks();		
		return true;
	}
	else if(stricmp( args[0], "VModEnable") == 0 && args.ArgC() >= 2)
	{
		VoiceServerDebug( "CVoiceGameMgr::ClientCommand: VModEnable (%d)\n", !!std::atoi( args[1] ) );
		g_PlayerModEnable[playerClientIndex] = !!atoi( args[1] );
		g_bWantModEnable[playerClientIndex] = false;
		//UpdateMasks();		
		return true;
	}
	else
	{
		return false;
	}
}

void CVoiceGameMgr::UpdateMasks()
{
	m_UpdateInterval = 0;

	// Note: We now check sv_alltalk in the CanPlayerHearPlayer function, so that function is the authority on whether we can hear another player

	for(int iClient=0; iClient < m_nMaxPlayers; iClient++)
	{
		CBaseEntity *pEnt = UTIL_PlayerByIndex(iClient+1);
		if(!pEnt || !pEnt->IsPlayer())
			continue;

		CBasePlayer *pPlayer = (CBasePlayer*)pEnt;

		CSingleUserRecipientFilter user( pPlayer );

		// Request the state of their "VModEnable" cvar.
		if(g_bWantModEnable[iClient])
		{
			ks::net::CCSUsrMsg_RequestState msg;
			SendUserMessage( user, ks::net::CS_UM_RequestState, msg );

			// Since this is reliable, only send it once
			g_bWantModEnable[iClient] = false;
		}

		CPlayerBitVec gameRulesMask;
		CPlayerBitVec ProximityMask;
		bool		bProximity = false;
		if( g_PlayerModEnable[iClient] )
		{
			// Build a mask of who they can hear based on the game rules.
			for(int iOtherClient=0; iOtherClient < m_nMaxPlayers; iOtherClient++)
			{
				CBaseEntity *pEnt = UTIL_PlayerByIndex(iOtherClient+1);
				if ( pEnt && pEnt->IsPlayer() && 
					 m_pHelper->CanPlayerHearPlayer( pPlayer, static_cast<CBasePlayer*>( pEnt ), bProximity ) )
				{
					gameRulesMask[iOtherClient] = true;
					ProximityMask[iOtherClient] = bProximity;
				}
			}
		}

		// If this is different from what the client has, send an update. 
		if(gameRulesMask != g_SentGameRulesMasks[iClient] || 
			g_BanMasks[iClient] != g_SentBanMasks[iClient])
		{
			g_SentGameRulesMasks[iClient] = gameRulesMask;
			g_SentBanMasks[iClient] = g_BanMasks[iClient];

			ks::net::CCSUsrMsg_VoiceMask msg;

			int dw;
			for(dw=0; dw < VOICE_MAX_PLAYERS_DW; dw++)
			{
				ks::net::CCSUsrMsg_VoiceMask::PlayerMask *playerMask = &msg.player_masks.emplace_back();
				playerMask->game_rules_mask = ks::get_word( gameRulesMask, dw );
				playerMask->ban_masks = ks::get_word( g_BanMasks[iClient], dw );
			}
			msg.player_mod_enable = !!g_PlayerModEnable[iClient];

			SendUserMessage( user, ks::net::CS_UM_VoiceMask, msg );
		}

		// Tell the engine.
		for(int iOtherClient=0; iOtherClient < m_nMaxPlayers; iOtherClient++)
		{
			bool bCanHear = gameRulesMask[iOtherClient] && !g_BanMasks[iClient][iOtherClient];
			g_pVoiceServer->SetClientListening( iClient+1, iOtherClient+1, bCanHear );

			if ( bCanHear )
			{
				g_pVoiceServer->SetClientProximity( iClient+1, iOtherClient+1, !!ProximityMask[iOtherClient] );
			}
		}
	}
}

bool CVoiceGameMgr::IsPlayerIgnoringPlayer( int iTalker, int iListener )
{
	return !!g_BanMasks[iListener-1][iTalker-1];
}

void CVoiceGameMgr::SetProximityDistance( int iDistance )
{
	m_iProximityDistance = iDistance;
}

bool CVoiceGameMgr::CheckProximity( int iDistance )
{
	if ( m_iProximityDistance >= iDistance )
		return true;

	return false;
}

class CBaseEntity;
class CBasePlayer;
class CVoteController;

using namespace ::ks::reflect;

// Purpose: Base VoteController.  Handles holding and voting on issues.

// Datatable
IMPLEMENT_REFLECT_SERVERCLASS( CVoteController, DT_VoteController )

LINK_ENTITY_TO_CLASS( vote_controller, CVoteController );

CVoteController *g_voteControllerGlobal = nullptr;
CVoteController *g_voteControllerCT = nullptr;
CVoteController *g_voteControllerT = nullptr;

ConVar sv_vote_timer_duration(side_registry(), "sv_vote_timer_duration", "15", FCVAR_RELEASE, "How long to allow voting on an issue");
ConVar sv_vote_command_delay(side_registry(), "sv_vote_command_delay", "2", FCVAR_RELEASE, "How long after a vote passes until the action happens", false, 0, true, 4.5);
ConVar sv_allow_votes(side_registry(), "sv_allow_votes", "1", FCVAR_RELEASE, "Allow voting?");
ConVar sv_vote_failure_timer(side_registry(), "sv_vote_failure_timer", "300", FCVAR_RELEASE, "A vote that fails cannot be re-submitted for this long");
ConVar sv_vote_creation_timer(side_registry(), "sv_vote_creation_timer", "120", FCVAR_RELEASE, "How often someone can individually call a vote.");
// default value of the sv_vote_quorum_ratio is 0.501 so on a 32 player server, you will still need 1 more than half, otherwise at 0.6, you would need 20 people to vote yes instead of 17
ConVar sv_vote_quorum_ratio( side_registry(), "sv_vote_quorum_ratio", "0.501", FCVAR_RELEASE, "The minimum ratio of players needed to vote on an issue to resolve it.", true, 0.01, true, 1.0 );
ConVar sv_vote_allow_spectators( side_registry(), "sv_vote_allow_spectators", "0", FCVAR_RELEASE, "Allow spectators to initiate votes?" );
ConVar sv_vote_count_spectator_votes( side_registry(), "sv_vote_count_spectator_votes", "0", FCVAR_RELEASE, "Allow spectators to vote on issues?" );
ConVar sv_vote_allow_in_warmup( side_registry(), "sv_vote_allow_in_warmup", "0", FCVAR_RELEASE, "Allow voting during warmup?" );
ConVar sv_vote_disallow_kick_on_match_point( side_registry(), "sv_vote_disallow_kick_on_match_point", "0", FCVAR_RELEASE, "Disallow vote kicking on the match point round." );

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CommandListIssues( void )
{
	CBasePlayer *commandIssuer = UTIL_GetCommandClient() ;

	if ( !commandIssuer )
		return;

	// list team-specific issues
	if ( commandIssuer->GetTeamVoteController() )
	{
		commandIssuer->GetTeamVoteController()->ListIssues( commandIssuer );
	}

	// and always list global issues
	if ( g_voteControllerGlobal )
	{
		g_voteControllerGlobal->ListIssues( commandIssuer );
	}

}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
ConCommand ListIssues(side_registry(), "listissues", CommandListIssues, "List all the issues that can be voted on.", FCVAR_GAMEDLL_FOR_REMOTE_CLIENTS );

//-----------------------------------------------------------------------------
// Purpose: This should eventually ask the player what team they are voting on
// to take into account different idle / spectator rules.
//-----------------------------------------------------------------------------

int GetVoterTeam( CBaseEntity *pEntity )
{
	if ( !pEntity )
		return TEAM_UNASSIGNED;

	CBasePlayer *pPlayer = ToBasePlayer( pEntity );
	if ( !pPlayer )
		return TEAM_UNASSIGNED;

	int iTeam = pPlayer->GetAssociatedTeamNumber( );

	return iTeam;

}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
static void callvote( const CCommand &args );
static ConCommand callvote_command( side_registry(), "callvote", callvote, "Start a vote on an issue.", FCVAR_GAMEDLL_FOR_REMOTE_CLIENTS );
static void callvote( const CCommand &args )
{
	if ( !g_voteControllerGlobal || !g_voteControllerCT || !g_voteControllerT )
	{
		DevMsg( "Vote Controllers Not Found!\n" );
		return;
	}

	CBasePlayer *pVoteCaller = UTIL_GetCommandClient();
	int iEntindex = 99;
	if ( pVoteCaller )
		iEntindex = pVoteCaller->entindex();

	if ( !sv_vote_allow_spectators.GetBool() && pVoteCaller && pVoteCaller->IsSpectator() )
	{
		g_voteControllerGlobal->SendVoteFailedMessage( VOTE_FAILED_SPECTATOR, pVoteCaller );
		return;

	}

	// Parameters
	char szEmptyDetails[ MAX_VOTE_DETAILS_LENGTH ];
	szEmptyDetails[ 0 ] = '\0';
	const char *arg2 = args[ 1 ];
	const char *arg3 = args.ArgC() >= 3 ? args[ 2 ] : szEmptyDetails;

	CVoteController *pVoteController;
	if ( pVoteCaller )
	{
		pVoteController = pVoteCaller->GetTeamVoteController();
	}
	else
	{
		pVoteController = g_voteControllerGlobal;
	}

	// If we don't have any arguments, invoke VoteSetup UI
	if( args.ArgC() < 2 )
	{
		pVoteController->SetupVote( iEntindex );

		return;
	}

	if ( g_voteControllerGlobal->HasIssue( arg2 ) )
	{
		g_voteControllerGlobal->CreateVote( iEntindex, arg2, arg3 );
	}
	else if ( pVoteController->HasIssue( arg2 ) )
	{
		pVoteController->CreateVote( iEntindex, arg2, arg3 );
	}
	else
	{
		DevMsg( "Vote Issue Not Found!\n" );
		return;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CVoteController::~CVoteController()
{
	if ( g_voteControllerGlobal == this )	{	g_voteControllerGlobal = nullptr; }
	else if ( g_voteControllerCT == this )	{	g_voteControllerCT = nullptr; }
	else if ( g_voteControllerT == this ) 	{	g_voteControllerT = nullptr; }

	for( int issueIndex = 0; issueIndex < std::ssize( m_potentialIssues ); ++issueIndex )
	{
		delete m_potentialIssues[issueIndex];
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CVoteController::ResetData( void )
{
	m_iActiveIssueIndex = INVALID_ISSUE;

	for ( int index = 0; index < m_nVoteOptionCount.Count(); index++ )
	{
		m_nVoteOptionCount.Set( index, 0 );
	}

	m_nPotentialVotes = 0;
	m_acceptingVotesTimer.Invalidate();
	m_executeCommandTimer.Invalidate();
	m_iEntityHoldingVote = -1;
	m_iOnlyTeamToVote = TEAM_INVALID;
	m_bIsYesNoVote = true;

	for( int voteIndex = 0; voteIndex < MAX_PLAYERS; ++voteIndex )
	{
		m_nVotesCast[voteIndex] = VOTE_UNCAST;
	}
	m_arrVotedUsers.clear();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CVoteController::Spawn_Impl( void )
{
	ResetData();

	BaseClass::Spawn_Impl();

	SetThink( &CVoteController::VoteControllerThink );
	SetNextThink( gpGlobals->curtime );

}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CVoteController::UpdateTransmitState_Impl( void )
{
	// ALWAYS transmit to all clients.
	return SetTransmitState( FL_EDICT_ALWAYS );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CVoteController::CanTeamCastVote( int iTeam ) const
{
	if ( m_iOnlyTeamToVote == TEAM_INVALID )
		return true;

	return iTeam == m_iOnlyTeamToVote;
}

//-----------------------------------------------------------------------------
// Purpose: Handles menu-driven setup of Voting
//-----------------------------------------------------------------------------
bool CVoteController::SetupVote( int iEntIndex )
{
	CBasePlayer *pVoteCaller = UTIL_PlayerByIndex( iEntIndex );
	if( !pVoteCaller )
		return false;

	bool bAllowVotes = sv_allow_votes.GetBool();
	if ( bAllowVotes && std::ssize( m_potentialIssues ) )
	{
		CSingleUserRecipientFilter filter( pVoteCaller );
		filter.MakeReliable();
		
		ks::net::CCSUsrMsg_VoteSetup msg;
		
		for( int iIndex = 0; iIndex < std::ssize( m_potentialIssues ); ++iIndex )
		{
			CBaseIssue *pCurrentIssue = m_potentialIssues[iIndex];
			
			if ( pCurrentIssue )
			{
				if ( pCurrentIssue->IsEnabled() )
				{
					msg.potential_issues.emplace_back( pCurrentIssue->GetTypeString() );
				}
				else
				{
					char szDisabledIssueStr[MAX_COMMAND_LENGTH + 12];
					V_strcpy( szDisabledIssueStr, pCurrentIssue->GetTypeString() );
					V_strcat( szDisabledIssueStr, " (Disabled on Server)", sizeof(szDisabledIssueStr) );

					msg.potential_issues.emplace_back( szDisabledIssueStr );
				}
			}
		}

		SendUserMessage( filter, ks::net::CS_UM_VoteSetup, msg );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Handles console-driven setup of Voting
//-----------------------------------------------------------------------------
bool CVoteController::CreateVote( int iEntIndex, const char *pszTypeString, const char *pszDetailString )
{
	// Terrible Hack:  Dedicated servers pass 99 as the EntIndex
	bool bDedicatedServer = ( iEntIndex == 99 ) ? true : false;

	CBasePlayer *pVoteCaller = UTIL_PlayerByIndex( iEntIndex );

	if ( !pVoteCaller && !bDedicatedServer )
		return false;

	if( !sv_allow_votes.GetBool() )
	{
		SendVoteFailedMessage( VOTE_FAILED_DISABLED, pVoteCaller );

		return false;
	}

	// Already running a vote?
	if ( IsAVoteInProgress() )
	{
		// send a message to the user to who tried to vote that their vote failed and why
		ClientPrint( pVoteCaller, HUD_PRINTCENTER, "#SFUI_vote_failed_vote_in_progress" );
		return false;
	}
	
	// Find the issue the user is asking for
	for( int issueIndex = 0; issueIndex < std::ssize( m_potentialIssues ); ++issueIndex )
	{
		CBaseIssue *pCurrentIssue = m_potentialIssues[issueIndex];
		if ( !pCurrentIssue )
			return false;
		
		if( FStrEq( pszTypeString, pCurrentIssue->GetTypeString() ) )
		{
			vote_create_failed_t nErrorCode = VOTE_FAILED_GENERIC;
			int nTime = 0;

			if( pCurrentIssue->CanCallVote( iEntIndex, pszTypeString, pszDetailString, nErrorCode, nTime ) )
			{
				// Prevent spamming commands
				if ( pVoteCaller && !pCurrentIssue->ShouldIgnoreCreationTimer() )
				{
					int nTimeLeft = sv_vote_creation_timer.GetFloat() - pVoteCaller->GetLastHeldVoteTimer().GetElapsedTime();
					if( pVoteCaller->GetLastHeldVoteTimer().HasStarted() && nTimeLeft > 1 )
					{
						SendVoteFailedMessage( VOTE_FAILED_RATE_EXCEEDED, pVoteCaller, nTimeLeft );
						return false;
					}
				}

				// if this is not an instant voting issue ( i.e. tournament pause match )

				if (pCurrentIssue->GetVotesRequiredToPass() > 1 )
				{
					// can't call it if there's a global vote in progress.
					if ( g_voteControllerGlobal && g_voteControllerGlobal->IsAVoteInProgress() )
					{
						// send a message to the user to who tried to vote that their vote failed and why
						ClientPrint( pVoteCaller, HUD_PRINTCENTER, "#SFUI_vote_failed_vote_in_progress" );
						return false;
					}

					// can't call it if this is a global vote and the other team is mid-vote.

					CVoteController * pOtherTeamVoteController = g_voteControllerGlobal;

					if ( pVoteCaller )
					{
						switch ( pVoteCaller->GetAssociatedTeamNumber() )
						{
							case TEAM_CT:
								pOtherTeamVoteController = g_voteControllerT;
								break;

							case TEAM_TERRORIST:
								pOtherTeamVoteController = g_voteControllerCT;
								break;

							default:
								break;
						}
					}

					if ( ( this == g_voteControllerGlobal ) && ( pOtherTeamVoteController && pOtherTeamVoteController->IsAVoteInProgress() ) )
					{
						// send a message to the user to who tried to vote that their vote failed and why
						ClientPrint( pVoteCaller, HUD_PRINTCENTER, "#SFUI_vote_failed_vote_in_progress" );
						return false;
					}
				}

				// Establish a bunch of data on this particular issue
				m_iEntityHoldingVote = iEntIndex;
				pCurrentIssue->SetIssueDetails( pszDetailString );
				m_bIsYesNoVote = pCurrentIssue->IsYesNoVote();
				m_iActiveIssueIndex = issueIndex;
				
				m_iOnlyTeamToVote = TEAM_INVALID;
				if ( !bDedicatedServer && pCurrentIssue->IsAllyRestrictedVote() )
					m_iOnlyTeamToVote = GetVoterTeam( pVoteCaller );
				
				// Now get our choices
				m_VoteOptions.clear();
				pCurrentIssue->GetVoteOptions( m_VoteOptions );
				int nNumVoteOptions = std::ssize( m_VoteOptions );
				if ( nNumVoteOptions >= 2 )
				{
					IGameEvent *event = gameeventmanager->CreateEvent( "vote_options" );
					if ( event )
					{
						event->SetInt( "count", nNumVoteOptions );
						for ( int iIndex = 0; iIndex < nNumVoteOptions; iIndex++ )
						{
							char szNumber[2];
							V_snprintf( szNumber, sizeof( szNumber ), "%i", iIndex + 1 );

							char szOptionName[8] = "option";
							V_strncat( szOptionName, szNumber, sizeof( szOptionName ), COPY_ALL_CHARACTERS );

							event->SetString( szOptionName, m_VoteOptions[iIndex] );
						}
						gameeventmanager->FireEvent( event );
					}
				}
				else
				{
					contract_assert( nNumVoteOptions >= 2 );
				}

				// Get the data out to the client
				for( int playerIndex = 1; playerIndex <= MAX_PLAYERS; ++playerIndex )
				{
					CBasePlayer *pPlayer = UTIL_PlayerByIndex( playerIndex );

					if ( pPlayer && pPlayer->GetTeamNumber() != TEAM_SPECTATOR )
					{
						CSingleUserRecipientFilter filter( pPlayer );					
						filter.MakeReliable();
						
						ks::net::CCSUsrMsg_VoteStart msg;
						msg.team = m_iOnlyTeamToVote;			// move into the filter
						msg.ent_idx = m_iEntityHoldingVote;
						msg.vote_type = pCurrentIssue->GetVoteIssue();
						msg.disp_str = pCurrentIssue->GetDisplayString();
						msg.details_str = pCurrentIssue->GetDetailsString();
						msg.other_team_str = pCurrentIssue->GetOtherTeamDisplayString();
						msg.is_yes_no_vote = m_bIsYesNoVote;
						SendUserMessage( filter, ks::net::CS_UM_VoteStart, msg );
					}
				}

				UTIL_LogPrintf( "Vote started \"%s %s\" from #%u %s\n",
					pCurrentIssue->GetTypeString(),
					pCurrentIssue->GetDetailsString(),
					m_iEntityHoldingVote,
					pVoteCaller ? GameLogSystem()->FormatPlayer( pVoteCaller ) : "n/a" );

				m_nPotentialVotes = pCurrentIssue->CountPotentialVoters();
				// FIX TO MAKE REMATCH ONLY
				float flVoteDuration = CSGameRules()->IsQueuedMatchmaking() ? 45.0f : sv_vote_timer_duration.GetFloat();
				m_acceptingVotesTimer.Start( flVoteDuration );
				pCurrentIssue->OnVoteStarted();

				// Force the vote holder to agree with a Yes/No vote
				if ( m_bIsYesNoVote && !bDedicatedServer )
				{
					TryCastVote( iEntIndex, "Option1" );
				}

				if ( !bDedicatedServer )
				{
					pVoteCaller->GetLastHeldVoteTimer().Start();
				}

				return true;
			}
			else
			{
				if ( !bDedicatedServer )
				{
					SendVoteFailedMessage( pCurrentIssue->MakeVoteFailErrorCodeForClients( nErrorCode ), pVoteCaller, nTime );
				}
			}
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Sent to everyone, unless we pass a player pointer
//-----------------------------------------------------------------------------
void CVoteController::SendVoteFailedMessage( vote_create_failed_t nReason, CBasePlayer *pVoteCaller, int nTime )
{
	// driller: need to merge all failure case stuff into a single path
	if ( pVoteCaller ) 
	{
		CSingleUserRecipientFilter user( pVoteCaller );
		user.MakeReliable();

		ks::net::CCSUsrMsg_CallVoteFailed msg;
		msg.reason = nReason;
		msg.time = nTime;
		SendUserMessage( user, ks::net::CS_UM_CallVoteFailed, msg );
	}
	else
	{
		UTIL_LogPrintf("Vote failed \"%s %s\" \n",
			m_potentialIssues[m_iActiveIssueIndex]->GetTypeString(),
			m_potentialIssues[m_iActiveIssueIndex]->GetDetailsString() );

		CBroadcastRecipientFilter filter;
		filter.MakeReliable();

		ks::net::CCSUsrMsg_VoteFailed msg;
		msg.team = m_iOnlyTeamToVote;
		msg.reason = nReason;
		SendUserMessage( filter, ks::net::CS_UM_VoteFailed, msg );
	}
}

//-----------------------------------------------------------------------------
// Purpose:  Player generated a vote command.  i.e. /vote option1
//-----------------------------------------------------------------------------
CVoteController::TryCastVoteResult CVoteController::TryCastVote( int iEntIndex, const char *pszVoteString )
{
	if( !sv_allow_votes.GetBool() )
		return CAST_FAIL_SERVER_DISABLE;

	if( iEntIndex > MAX_PLAYERS )
		return CAST_FAIL_SYSTEM_ERROR;

	if( m_iActiveIssueIndex == INVALID_ISSUE )
		return CAST_FAIL_NO_ACTIVE_ISSUE;

	if( m_executeCommandTimer.HasStarted() )
		return CAST_FAIL_VOTE_CLOSED;

	CBaseEntity *pVoter = UTIL_EntityByIndex( iEntIndex );
	if ( !IsValidVoter( ToBasePlayer( pVoter ) ) )
		return CAST_FAIL_TEAM_RESTRICTED;

	if( m_potentialIssues[m_iActiveIssueIndex] && m_potentialIssues[m_iActiveIssueIndex]->IsAllyRestrictedVote() )
	{
		CBaseEntity *pVoteHolder = UTIL_EntityByIndex( m_iEntityHoldingVote );

		if( ( pVoteHolder == nullptr ) || ( pVoter == nullptr ) || ( GetVoterTeam( pVoteHolder ) != GetVoterTeam( pVoter ) ) )
		{
			return CAST_FAIL_TEAM_RESTRICTED;
		}
	}

	// Look for a previous vote
	int nOldVote = VOTE_UNCAST;
	if ( iEntIndex < MAX_PLAYERS )
	{
		nOldVote = m_nVotesCast[iEntIndex];
	}
#ifndef DEBUG
	if( nOldVote != VOTE_UNCAST )
	{
		return CAST_FAIL_NO_CHANGES;
	}
#endif // !DEBUG

	// Which option are they voting for?
	int nCurrentVote = VOTE_UNCAST;
	if ( !StringHasPrefix( pszVoteString, "Option" ) )
		return CAST_FAIL_SYSTEM_ERROR;

	nCurrentVote = (CastVote)( std::atoi( pszVoteString + V_strlen( "Option" ) ) - 1 );

	if ( nCurrentVote < VOTE_OPTION1 || nCurrentVote > VOTE_OPTION5 )
		return CAST_FAIL_SYSTEM_ERROR;
	
	// They're changing their vote
#ifdef DEBUG
	if ( nOldVote != VOTE_UNCAST )
	{
		if( nOldVote == nCurrentVote )
		{
			return CAST_FAIL_DUPLICATE;
		}
		VoteChoice_Decrement( nOldVote );
	}
#endif // DEBUG

	// With a Yes/No vote, slam anything past "No" to No
	if ( m_potentialIssues[m_iActiveIssueIndex]->IsYesNoVote() )
	{
		if ( nCurrentVote > VOTE_OPTION2 )
			nCurrentVote = VOTE_OPTION2;
	}

#ifndef DEBUG
	if ( CBasePlayer *pBasePlayerVoter = ToBasePlayer( pVoter ) )
	{
		if ( uint64 uiPlayerID = pBasePlayerVoter->GetPlayerIDAsUInt64() )
		{
			if ( ks::find_index( m_arrVotedUsers, uiPlayerID ) == -1 )
			{
				m_arrVotedUsers.push_back( uiPlayerID );	// remember that this user already voted
			}
			else
			{
				return CAST_FAIL_NO_CHANGES;
			}
		}
	}
#endif

	// Register and track this vote
	VoteChoice_Increment( nCurrentVote );
	m_nVotesCast[iEntIndex] = nCurrentVote;

	// Tell the client-side UI
	IGameEvent *event = gameeventmanager->CreateEvent( "vote_cast" );
	if ( event )
	{
		event->SetInt( "vote_option", nCurrentVote );
		event->SetInt( "team", m_iOnlyTeamToVote );
		event->SetInt( "entityid", iEntIndex );
		gameeventmanager->FireEvent( event );
	}

	UTIL_LogPrintf( "Vote cast \"%s %s\" from #%u %s option%d\n",
		m_potentialIssues[m_iActiveIssueIndex]->GetTypeString(),
		m_potentialIssues[m_iActiveIssueIndex]->GetDetailsString(),
		iEntIndex,
		pVoter ? GameLogSystem()->FormatPlayer( pVoter ) : "n/a",
		nCurrentVote );

	CheckForEarlyVoteClose();

	return CAST_OK;
}

//-----------------------------------------------------------------------------
// Purpose:  Increments the vote count for a particular vote option 
//			 i.e. nVoteChoice = 0 might mean a Yes vote
//-----------------------------------------------------------------------------
void CVoteController::VoteChoice_Increment( int nVoteChoice )
{
	if ( nVoteChoice < VOTE_OPTION1 || nVoteChoice > VOTE_OPTION5 )
		return;

	int nValue = m_nVoteOptionCount.Get( nVoteChoice );
	m_nVoteOptionCount.Set( nVoteChoice, ++nValue );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CVoteController::VoteChoice_Decrement( int nVoteChoice )
{
	if ( nVoteChoice < VOTE_OPTION1 || nVoteChoice > VOTE_OPTION5 )
		return;

	int nValue = m_nVoteOptionCount.Get( nVoteChoice );
	m_nVoteOptionCount.Set( nVoteChoice, --nValue );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CVoteController::VoteControllerThink( void )
{
	if ( !ks::is_valid_index( m_potentialIssues, m_iActiveIssueIndex ) )
	{
		SetNextThink( gpGlobals->curtime + 0.5f );

		return;
	}

	CBaseIssue *pActiveIssue = m_potentialIssues[m_iActiveIssueIndex];

	int nWinningVoteOption = GetWinningVoteOption();
	if ( !pActiveIssue->IsYesNoVote() || nWinningVoteOption >= std::ssize( m_VoteOptions ) )
	{
		contract_assert( nWinningVoteOption >= 0 && nWinningVoteOption < std::ssize( m_VoteOptions ) );
		Msg( "Trying to resolve a vote that is not a YES/NO vote, YES/NO votes aren't currently supported! \n");
		return;
	}

	bool bVoteHasFinished = false;
	bool bVoteShouldBailOnNos = (pActiveIssue->GetVoteIssue() == VOTEISSUE_REMATCH);

	// Vote time is up - process the result
	if( m_acceptingVotesTimer.HasStarted() )
	{	
		int nNumVotesYES = m_nVoteOptionCount[VOTE_OPTION1];
		int nNumVotesNO = m_nVoteOptionCount[VOTE_OPTION2];

		//int nVoteTally = nNumVotesYES + nNumVotesNO;

		bool bVotePassed = false;
	
		int nVotesToSucceed = pActiveIssue->GetVotesRequiredToPass();

		// Have we exceeded the required ratio of Voted-vs-Abstained?
		if ( nNumVotesYES >= nVotesToSucceed )
		{
			// bail early, we succeeded
			bVotePassed = true;
			bVoteHasFinished = true;
		}
		else if ( bVoteShouldBailOnNos && (m_nPotentialVotes - nNumVotesNO) < nVotesToSucceed )
		{
			// failed because too many no votes
			SendVoteFailedMessage( pActiveIssue->MakeVoteFailErrorCodeForClients( VOTE_FAILED_YES_MUST_EXCEED_NO ) );
			bVotePassed = false;
			bVoteHasFinished = true;
		}
		else if ( m_acceptingVotesTimer.IsElapsed() )
		{
			// timed out, not enough people voted yes
			SendVoteFailedMessage( pActiveIssue->MakeVoteFailErrorCodeForClients( VOTE_FAILED_QUORUM_FAILURE ) );
			bVotePassed = false;
			bVoteHasFinished = true;
		}

		if ( bVoteHasFinished )
		{
			// for record-keeping
			if ( pActiveIssue->IsYesNoVote() )
			{
				pActiveIssue->SetYesNoVoteCount( m_nVoteOptionCount[VOTE_OPTION1],  m_nVoteOptionCount[VOTE_OPTION2], m_nPotentialVotes );
			}

			m_acceptingVotesTimer.Invalidate();

			if ( bVotePassed )
			{
				m_executeCommandTimer.Start( pActiveIssue->GetCommandDelay() );
				m_resetVoteTimer.Start( 5.0 );

				CBaseEntity *pVoteHolder = UTIL_EntityByIndex( m_iEntityHoldingVote );
				CBasePlayer *pVoteHolderPlayer = ( pVoteHolder && pVoteHolder->IsPlayer() ) ? (CBasePlayer *)( pVoteHolder ) : nullptr;
				if( pVoteHolderPlayer )
				{
					pVoteHolderPlayer->GetLastHeldVoteTimer().Invalidate();	// You can go ahead and make a new vote since yours passed.
				}

				UTIL_LogPrintf("Vote succeeded \"%s %s\" from #%u %s\n",
					pActiveIssue->GetTypeString(),
					pActiveIssue->GetDetailsString(),
					m_iEntityHoldingVote,
					pVoteHolderPlayer ? GameLogSystem()->FormatPlayer( pVoteHolderPlayer ) : "n/a" );

				CBroadcastRecipientFilter filter;
				filter.MakeReliable();

				ks::net::CCSUsrMsg_VotePass msg;
				msg.team = m_iOnlyTeamToVote;
				msg.vote_type = pActiveIssue->GetVoteIssue();
				msg.disp_str = pActiveIssue->GetVotePassedString();
				msg.details_str = pActiveIssue->GetDetailsString();
				SendUserMessage( filter, ks::net::CS_UM_VotePass, msg );
			}
			else
			{
				pActiveIssue->OnVoteFailed();
				m_resetVoteTimer.Start( 5.0 );
			}
		}
	}

	// Vote passed - execute the command
	if( m_executeCommandTimer.HasStarted() && m_executeCommandTimer.IsElapsed() )
	{

		m_executeCommandTimer.Invalidate();
		m_potentialIssues[m_iActiveIssueIndex]->ExecuteCommand();
	}

	if ( m_resetVoteTimer.HasStarted() && m_resetVoteTimer.IsElapsed() )
	{
		ResetData();
		m_resetVoteTimer.Invalidate();
	}

	SetNextThink( gpGlobals->curtime + 0.1f );
}

//-----------------------------------------------------------------------------
// Purpose: End the vote early if everyone's voted
//-----------------------------------------------------------------------------
void CVoteController::CheckForEarlyVoteClose( void )
{
	int nVoteTally = 0;
	for ( int index = 0; index < MAX_VOTE_OPTIONS; index++ )
	{
		nVoteTally += m_nVoteOptionCount.Get( index );
	}

	if( nVoteTally >= m_nPotentialVotes )
	{
		m_acceptingVotesTimer.Start( 0 );	// Run the timer out right now
	}
}

#ifdef DEBUG  // Don't want to do this check for debug builds (so we can test with bots)
	ConVar sv_vote_ignore_bots( side_registry(), "sv_vote_ignore_bots", "0" );
    #define SV_VOTE_IGNORE_BOTS sv_vote_ignore_bots.GetBool()
#else
#endif

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CVoteController::IsValidVoter( CBasePlayer *pWhom )
{
	if ( pWhom == nullptr )
		return false;

	if ( !pWhom->IsConnected() )
		return false;

	if ( !sv_vote_allow_spectators.GetBool() || !sv_vote_count_spectator_votes.GetBool() )
	{
		if ( pWhom->GetTeamNumber() != TEAM_TERRORIST && pWhom->GetTeamNumber() != TEAM_CT )
			return false;
	}

	if ( true )  // Don't want to do this check for debug builds (so we can test with bots)
	{
		if ( pWhom->IsBot() )
			return false;

		if ( pWhom->IsFakeClient() )
			return false;
	}

	if ( pWhom->IsHLTV() )
		return false;

	if ( pWhom->IsReplay() )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CVoteController::RegisterIssue( CBaseIssue *pszNewIssue )
{
	m_potentialIssues.push_back( pszNewIssue );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CVoteController::ListIssues( CBasePlayer *pForWhom )
{
	if( !sv_allow_votes.GetBool() )
		return;

	ClientPrint( pForWhom, HUD_PRINTCONSOLE, "---Vote commands---\n" );

	for( int issueIndex = 0; issueIndex < std::ssize( m_potentialIssues ); ++issueIndex )
	{
		CBaseIssue *pCurrentIssue = m_potentialIssues[issueIndex];
		pCurrentIssue->ListIssueDetails( pForWhom );
	}
	ClientPrint( pForWhom, HUD_PRINTCONSOLE, "--- End Vote commands---\n" );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CVoteController::GetWinningVoteOption( void )
{
	if ( m_potentialIssues[m_iActiveIssueIndex]->IsYesNoVote() )
	{
		return ( m_nVoteOptionCount[VOTE_OPTION1] > m_nVoteOptionCount[VOTE_OPTION2] ) ? VOTE_OPTION1 : VOTE_OPTION2;
	}
	else
	{
		std::vector< int > pVoteCounts;

		// Which option had the most votes?
		// driller:  Need to handle ties
		int nHighest = m_nVoteOptionCount[0];
		for ( int iIndex = 0; iIndex < m_nVoteOptionCount.Count(); iIndex ++ )
		{
			nHighest = ( ( nHighest < m_nVoteOptionCount[iIndex] ) ? m_nVoteOptionCount[iIndex] : nHighest );
			pVoteCounts.push_back( m_nVoteOptionCount[iIndex] );
		}
		
		m_nHighestCountIndex = -1;
		for ( int iIndex = 0; iIndex < m_nVoteOptionCount.Count(); iIndex++ )
		{
			if ( m_nVoteOptionCount[iIndex] == nHighest )
			{
				m_nHighestCountIndex = iIndex;
				// henryg: break on first match, not last. this avoids a crash
				// if we are all tied at zero and we pick something beyond the
				// last vote option. this code really ought to ignore attempts
				// to tally votes for options beyond the last valid one!
				break;
			}
		}

		return m_nHighestCountIndex;
	}

	return -1;
}

bool CVoteController::HasIssue( const char *pszIssue )
{
	for ( int issueIndex = 0; issueIndex < std::ssize( m_potentialIssues ); ++issueIndex )
	{
		CBaseIssue *pCurrentIssue = m_potentialIssues[ issueIndex ];
		if ( !pCurrentIssue )
			return false;

		if ( FStrEq( pszIssue, pCurrentIssue->GetTypeString( ) ) )
			return true;
	}

	return false;
}

void CVoteController::EndVoteImmediately( void )
{
	if ( !IsAVoteInProgress( ) )
		return;

	CBaseIssue *pActiveIssue = m_potentialIssues[ m_iActiveIssueIndex ];

	// for record-keeping
	if ( pActiveIssue->IsYesNoVote( ) )
	{
		pActiveIssue->SetYesNoVoteCount( m_nVoteOptionCount[ VOTE_OPTION1 ], m_nVoteOptionCount[ VOTE_OPTION2 ], m_nPotentialVotes );
	}

	SendVoteFailedMessage( pActiveIssue->MakeVoteFailErrorCodeForClients( VOTE_FAILED_QUORUM_FAILURE ) );
	pActiveIssue->OnVoteFailed( );
	m_resetVoteTimer.Start( 5.0 );

	m_acceptingVotesTimer.Invalidate( );
}

//-----------------------------------------------------------------------------
// Purpose: BaseIssue
//-----------------------------------------------------------------------------
CBaseIssue::CBaseIssue( const char *pszTypeString, CVoteController *pVoteController )
{
	V_strcpy_safe( m_szTypeString, pszTypeString );

	m_iNumYesVotes = 0;
	m_iNumNoVotes = 0;
	m_iNumPotentialVotes = 0;

	m_pVoteController = pVoteController;

	ASSERT( pVoteController );
	if ( pVoteController )
	{
		pVoteController->RegisterIssue( this );
		
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CBaseIssue::~CBaseIssue()
{
	for ( int index = 0; index < std::ssize( m_FailedVotes ); index++ )
	{
		FailedVote *pFailedVote = m_FailedVotes[index];
		delete pFailedVote;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const char *CBaseIssue::GetTypeString( void )
{
	return m_szTypeString;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const char *CBaseIssue::GetDetailsString( void )
{
	return m_szDetailsString;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBaseIssue::SetIssueDetails( const char *pszDetails )
{
	V_strcpy_safe( m_szDetailsString, pszDetails );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CBaseIssue::IsAllyRestrictedVote( void )
{
	return false;
}

int CBaseIssue::GetVotesRequiredToPass( void )
{
	// TODO: to reduce risk of new bugs, this logic was preserved as-is from VoteControllerThink. But it can/should be cleaned up for legibility.
	int nPotentialVoters = CountPotentialVoters();
	
	// BUGBUG: disconnecting/reconnecting players during the vote can affect the final tally, so we will use the larger number here:
	if ( m_pVoteController && ( m_pVoteController->GetPotentialVotes( ) > nPotentialVoters ) )
		nPotentialVoters = m_pVoteController->GetPotentialVotes( );

	int nVotesToSucceed = 0;

	if ( CSGameRules() && CSGameRules()->IsQueuedMatchmaking() 
		 && CSGameRules()->IsPlayingAnyCompetitiveStrictRuleset() && !IsAllyRestrictedVote() )
	{	// in queued matchmaking must have 100% voting to succeed (unless kick vote)
		nVotesToSucceed = CSGameRules()->m_numQueuedMatchmakingAccounts;
	}
	// Unanimous votes require all attending humans (which might be less than 10 players)
	else if ( IsUnanimousVoteToPass() )
	{
		nVotesToSucceed = std::max( 1, nPotentialVoters );
	}
	else
	{
		float flnVotesToSucceed = ( CSGameRules() && CSGameRules()->IsPlayingAnyCompetitiveStrictRuleset() ) ? std::max( 1, nPotentialVoters - 1 ) : ( ( float )nPotentialVoters * sv_vote_quorum_ratio.GetFloat() );
		nVotesToSucceed = std::ceil( flnVotesToSucceed );
	}

	return nVotesToSucceed;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const char *CBaseIssue::GetVotePassedString( void )
{
	return "Unknown vote passed.";
}

float CBaseIssue::GetFailedVoteLockOutTime( void )
{
	return sv_vote_failure_timer.GetFloat();
}

//-----------------------------------------------------------------------------
// Purpose:  Store failures to prevent vote spam
//-----------------------------------------------------------------------------
void CBaseIssue::OnVoteFailed( void )
{
	// Check for an existing match
	for ( int index = 0; index < std::ssize( m_FailedVotes ); index++ )
	{
		FailedVote *pFailedVote = m_FailedVotes[index];
		if ( V_strncmp( pFailedVote->szFailedVoteParameter, GetDetailsString(), std::size( pFailedVote->szFailedVoteParameter ) - 1 ) == 0 )
		{
			pFailedVote->flLockoutTime = gpGlobals->curtime + GetFailedVoteLockOutTime();

			return;
		}
	}

	// Need to create a new one
	FailedVote *pNewFailedVote = new FailedVote;
	int iIndex = std::ssize( m_FailedVotes ); m_FailedVotes.push_back( pNewFailedVote );
	V_strcpy_safe( m_FailedVotes[iIndex]->szFailedVoteCommand, GetTypeString() );
	V_strcpy_safe( m_FailedVotes[iIndex]->szFailedVoteParameter, GetDetailsString() );
	m_FailedVotes[iIndex]->flLockoutTime = gpGlobals->curtime + GetFailedVoteLockOutTime();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CBaseIssue::CanTeamCallVote( int iTeam ) const
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CBaseIssue::CanCallVote( int iEntIndex, const char *pszCommand, const char *pszDetails, vote_create_failed_t &nFailCode, int &nTime )
{
	// Automated server vote - don't bother testing against it
	if ( iEntIndex == 99 )
		return true;

	// Bogus player
	if( iEntIndex == -1 )
		return false;

	if ( !sv_vote_allow_in_warmup.GetBool() && CSGameRules() && CSGameRules()->IsWarmupPeriod() && !IsEnabledDuringWarmup() )
	{
		nFailCode = VOTE_FAILED_WAITINGFORPLAYERS;
		return false;
	}

	CBaseEntity *pVoteCaller = UTIL_EntityByIndex( iEntIndex );
	if( pVoteCaller && !CanTeamCallVote( GetVoterTeam( pVoteCaller ) ) )
	{
		nFailCode = VOTE_FAILED_TEAM_CANT_CALL;
		return false;
	}

	if ( IsVoteCallExclusiveToSpectators( ) )
	{
		CBasePlayer *pVoteCallerPlayer = ToBasePlayer( pVoteCaller );

		if ( !pVoteCallerPlayer || !pVoteCallerPlayer->IsSpectator( ) )
		{
			nFailCode = VOTE_FAILED_TEAM_CANT_CALL;
			return false;
		}
	}

	// Only few votes are actually allowed in queue matchmaking mode
	if ( CSGameRules() && CSGameRules()->IsQueuedMatchmaking() && !IsEnabledInQueuedMatchmaking() )
	{
		nFailCode = VOTE_FAILED_ISSUE_DISABLED;
		return false;
	}

	// Disable all voting after rematch vote is initiated at the end of the match
	if ( CSGameRules() && CSGameRules()->IsQueuedMatchmaking() && ( CSGameRules()->m_eQueuedMatchmakingRematchState >= CSGameRules()->k_EQueuedMatchmakingRematchState_VoteToRematchInProgress ) )
	{
		nFailCode = VOTE_FAILED_ISSUE_DISABLED;
		return false;
	}

	// don't let kick votes happen on match point or last round
	if ( CSGameRules() && (CSGameRules()->IsQueuedMatchmaking() || sv_vote_disallow_kick_on_match_point.GetBool()) && 
		 CSGameRules()->IsLastRoundOfMatch() && CSGameRules()->IsMatchPoint() && FStrEq( VOTEISSUE_NAME_KICK, pszCommand ) )
	{
		nFailCode = VOTE_FAILED_ISSUE_DISABLED;
		return false;
	}

	// Did this fail recently?
	for( int iIndex = 0; iIndex < std::ssize( m_FailedVotes ); iIndex++ )
	{
		FailedVote *pCurrentFailure = m_FailedVotes[iIndex];
		int nTimeRemaining = pCurrentFailure->flLockoutTime - gpGlobals->curtime;
		bool bFailed = false;

		if ( nTimeRemaining > 1 )
		{
			if ( V_strlen( pCurrentFailure->szFailedVoteCommand ) > 0 && FStrEq( pCurrentFailure->szFailedVoteCommand, pszCommand ) )
			{
				// If this issue requires a parameter, see if we're voting for the same one again (i.e. changelevel ctf_2fort)
				if ( V_strlen( pCurrentFailure->szFailedVoteParameter ) > 0 && FStrEq( pCurrentFailure->szFailedVoteParameter, pszDetails ) )
				{
					bFailed = true;
					if ( FStrEq( VOTEISSUE_NAME_CHANGELEVEL, pszCommand ) )
					{
						nFailCode = VOTE_FAILED_FAILED_RECENT_CHANGEMAP;	
					}
					else if ( FStrEq( VOTEISSUE_NAME_KICK, pszCommand ) )
					{
						nFailCode = VOTE_FAILED_FAILED_RECENT_KICK;	
					}
					else
					{
						nFailCode = VOTE_FAILED_FAILED_RECENTLY;	
					}
				}
				else
				{
					if ( FStrEq( VOTEISSUE_NAME_SWAPTEAMS, pszCommand ) )
					{
						nFailCode = VOTE_FAILED_FAILED_RECENT_SWAPTEAMS;	
						bFailed = true;
					}
					else if ( FStrEq( VOTEISSUE_NAME_SCRAMBLE, pszCommand ) )
					{
						nFailCode = VOTE_FAILED_FAILED_RECENT_SCRAMBLETEAMS;	
						bFailed = true;
					}
					else if ( FStrEq( VOTEISSUE_NAME_RESTARTGAME, pszCommand ) )
					{
						nFailCode = VOTE_FAILED_FAILED_RECENT_RESTART;	
						bFailed = true;
					}
				}
			}
			// Otherwise we have a parameter-less vote, so just check the lockout timer (i.e. restartgame)
			else
			{
				bFailed = true;
				nFailCode = VOTE_FAILED_FAILED_RECENTLY;	
			}
		}

		if ( bFailed )
		{	
			nTime = nTimeRemaining;
			return false;
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CBaseIssue::CountPotentialVoters( void )
{
	int nTotalPlayers = 0;

	for( int playerIndex = 1; playerIndex <= MAX_PLAYERS; ++playerIndex )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( playerIndex );
		if ( m_pVoteController && m_pVoteController->IsValidVoter( pPlayer ) )
		{
			if ( m_pVoteController->CanTeamCastVote( GetVoterTeam( pPlayer ) ) )
			{
				nTotalPlayers++;
			}
		}
	}

	return nTotalPlayers;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CBaseIssue::GetNumberVoteOptions( void )
{
	return 2;  // The default issue is Yes/No (so 2), but it can be anywhere between 1 and MAX_VOTE_COUNT
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CBaseIssue::IsYesNoVote( void )
{
	return true;  // Default
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBaseIssue::SetYesNoVoteCount( int iNumYesVotes, int iNumNoVotes, int iNumPotentialVotes )
{
	m_iNumYesVotes = iNumYesVotes;
	m_iNumNoVotes = iNumNoVotes;
	m_iNumPotentialVotes = iNumPotentialVotes;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBaseIssue::ListStandardNoArgCommand( CBasePlayer *forWhom, const char *issueString )
{
	char szBuffer[MAX_COMMAND_LENGTH];
	V_snprintf( szBuffer, MAX_COMMAND_LENGTH, "callvote %s\n", issueString );
	ClientPrint( forWhom, HUD_PRINTCONSOLE, szBuffer );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseIssue::GetVoteOptions( std::vector< const char* > &vecNames )
{
	// The default vote issue is a Yes/No vote
	vecNames.insert( vecNames.begin(), "Yes" );
	vecNames.push_back( "No" );

	return true;
}

float CBaseIssue::GetCommandDelay( void )
{
	return sv_vote_command_delay.GetFloat();
}

#undef SV_VOTE_IGNORE_BOTS

class CBaseAnimating;
class CBaseEntity;
class CBasePlayer;
class CScriptKeyValues;

using namespace ::ks::reflect;

//lwss - hardcode nut file here instead of regenerating it each time. I don't think it will change for now.
//#include "vscript_server_nut.h"
//Generated with: `perl texttoarray.pl ../../game/server/vscript_server.nut g_Script_vscript_server > /tmp/file.h`
static const unsigned char g_Script_vscript_server[] = {
    0x2f,0x2f,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x20,0x43,0x6f,0x70,0x79,0x72,0x69,0x67,
    0x68,0x74,0x20,0xa9,0x20,0x32,0x30,0x30,0x38,0x2c,0x20,0x56,0x61,0x6c,0x76,0x65,0x20,0x43,0x6f,0x72,
    0x70,0x6f,0x72,0x61,0x74,0x69,0x6f,0x6e,0x2c,0x20,0x41,0x6c,0x6c,0x20,0x72,0x69,0x67,0x68,0x74,0x73,
    0x20,0x72,0x65,0x73,0x65,0x72,0x76,0x65,0x64,0x2e,0x20,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x0a,
    0x2f,0x2f,0x0a,0x2f,0x2f,0x20,0x50,0x75,0x72,0x70,0x6f,0x73,0x65,0x3a,0x0a,0x2f,0x2f,0x0a,0x2f,0x2f,
    0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,
    0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,
    0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,
    0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x0a,0x0a,0x66,
    0x75,0x6e,0x63,0x74,0x69,0x6f,0x6e,0x20,0x55,0x6e,0x69,0x71,0x75,0x65,0x53,0x74,0x72,0x69,0x6e,0x67,
    0x28,0x20,0x73,0x74,0x72,0x69,0x6e,0x67,0x20,0x3d,0x20,0x22,0x22,0x20,0x29,0x0a,0x7b,0x0a,0x09,0x72,
    0x65,0x74,0x75,0x72,0x6e,0x20,0x44,0x6f,0x55,0x6e,0x69,0x71,0x75,0x65,0x53,0x74,0x72,0x69,0x6e,0x67,
    0x28,0x20,0x73,0x74,0x72,0x69,0x6e,0x67,0x2e,0x74,0x6f,0x73,0x74,0x72,0x69,0x6e,0x67,0x28,0x29,0x20,
    0x29,0x3b,0x0a,0x7d,0x0a,0x0a,0x66,0x75,0x6e,0x63,0x74,0x69,0x6f,0x6e,0x20,0x45,0x6e,0x74,0x46,0x69,
    0x72,0x65,0x28,0x20,0x74,0x61,0x72,0x67,0x65,0x74,0x2c,0x20,0x61,0x63,0x74,0x69,0x6f,0x6e,0x2c,0x20,
    0x76,0x61,0x6c,0x75,0x65,0x20,0x3d,0x20,0x6e,0x75,0x6c,0x6c,0x2c,0x20,0x64,0x65,0x6c,0x61,0x79,0x20,
    0x3d,0x20,0x30,0x2e,0x30,0x2c,0x20,0x61,0x63,0x74,0x69,0x76,0x61,0x74,0x6f,0x72,0x20,0x3d,0x20,0x6e,
    0x75,0x6c,0x6c,0x20,0x29,0x0a,0x7b,0x0a,0x09,0x69,0x66,0x20,0x28,0x20,0x21,0x76,0x61,0x6c,0x75,0x65,
    0x20,0x29,0x0a,0x09,0x7b,0x0a,0x09,0x09,0x76,0x61,0x6c,0x75,0x65,0x20,0x3d,0x20,0x22,0x22,0x3b,0x0a,
    0x09,0x7d,0x0a,0x09,0x0a,0x09,0x6c,0x6f,0x63,0x61,0x6c,0x20,0x63,0x61,0x6c,0x6c,0x65,0x72,0x20,0x3d,
    0x20,0x6e,0x75,0x6c,0x6c,0x3b,0x0a,0x09,0x69,0x66,0x20,0x28,0x20,0x22,0x73,0x65,0x6c,0x66,0x22,0x20,
    0x69,0x6e,0x20,0x74,0x68,0x69,0x73,0x20,0x29,0x0a,0x09,0x7b,0x0a,0x09,0x09,0x63,0x61,0x6c,0x6c,0x65,
    0x72,0x20,0x3d,0x20,0x73,0x65,0x6c,0x66,0x3b,0x0a,0x09,0x09,0x69,0x66,0x20,0x28,0x20,0x21,0x61,0x63,
    0x74,0x69,0x76,0x61,0x74,0x6f,0x72,0x20,0x29,0x0a,0x09,0x09,0x7b,0x0a,0x09,0x09,0x09,0x61,0x63,0x74,
    0x69,0x76,0x61,0x74,0x6f,0x72,0x20,0x3d,0x20,0x73,0x65,0x6c,0x66,0x3b,0x0a,0x09,0x09,0x7d,0x0a,0x09,
    0x7d,0x0a,0x09,0x0a,0x09,0x44,0x6f,0x45,0x6e,0x74,0x46,0x69,0x72,0x65,0x28,0x20,0x74,0x61,0x72,0x67,
    0x65,0x74,0x2e,0x74,0x6f,0x73,0x74,0x72,0x69,0x6e,0x67,0x28,0x29,0x2c,0x20,0x61,0x63,0x74,0x69,0x6f,
    0x6e,0x2e,0x74,0x6f,0x73,0x74,0x72,0x69,0x6e,0x67,0x28,0x29,0x2c,0x20,0x76,0x61,0x6c,0x75,0x65,0x2e,
    0x74,0x6f,0x73,0x74,0x72,0x69,0x6e,0x67,0x28,0x29,0x2c,0x20,0x64,0x65,0x6c,0x61,0x79,0x2c,0x20,0x61,
    0x63,0x74,0x69,0x76,0x61,0x74,0x6f,0x72,0x2c,0x20,0x63,0x61,0x6c,0x6c,0x65,0x72,0x20,0x29,0x3b,0x20,
    0x0a,0x7d,0x0a,0x0a,0x66,0x75,0x6e,0x63,0x74,0x69,0x6f,0x6e,0x20,0x5f,0x5f,0x52,0x65,0x70,0x6c,0x61,
    0x63,0x65,0x43,0x6c,0x6f,0x73,0x75,0x72,0x65,0x73,0x28,0x20,0x73,0x63,0x72,0x69,0x70,0x74,0x2c,0x20,
    0x73,0x63,0x6f,0x70,0x65,0x20,0x29,0x0a,0x7b,0x0a,0x09,0x69,0x66,0x20,0x28,0x20,0x21,0x73,0x63,0x6f,
    0x70,0x65,0x20,0x29,0x0a,0x09,0x7b,0x0a,0x09,0x09,0x73,0x63,0x6f,0x70,0x65,0x20,0x3d,0x20,0x67,0x65,
    0x74,0x72,0x6f,0x6f,0x74,0x74,0x61,0x62,0x6c,0x65,0x28,0x29,0x3b,0x0a,0x09,0x7d,0x0a,0x09,0x0a,0x09,
    0x6c,0x6f,0x63,0x61,0x6c,0x20,0x74,0x65,0x6d,0x70,0x50,0x61,0x72,0x65,0x6e,0x74,0x20,0x3d,0x20,0x7b,
    0x20,0x67,0x65,0x74,0x72,0x6f,0x6f,0x74,0x74,0x61,0x62,0x6c,0x65,0x20,0x3d,0x20,0x66,0x75,0x6e,0x63,
    0x74,0x69,0x6f,0x6e,0x28,0x29,0x20,0x7b,0x20,0x72,0x65,0x74,0x75,0x72,0x6e,0x20,0x6e,0x75,0x6c,0x6c,
    0x3b,0x20,0x7d,0x20,0x7d,0x3b,0x0a,0x09,0x6c,0x6f,0x63,0x61,0x6c,0x20,0x74,0x65,0x6d,0x70,0x20,0x3d,
    0x20,0x7b,0x20,0x72,0x75,0x6e,0x73,0x63,0x72,0x69,0x70,0x74,0x20,0x3d,0x20,0x73,0x63,0x72,0x69,0x70,
    0x74,0x20,0x7d,0x3b,0x0a,0x09,0x64,0x65,0x6c,0x65,0x67,0x61,0x74,0x65,0x20,0x74,0x65,0x6d,0x70,0x50,
    0x61,0x72,0x65,0x6e,0x74,0x20,0x3a,0x20,0x74,0x65,0x6d,0x70,0x3b,0x0a,0x09,0x0a,0x09,0x74,0x65,0x6d,
    0x70,0x2e,0x72,0x75,0x6e,0x73,0x63,0x72,0x69,0x70,0x74,0x28,0x29,0x0a,0x09,0x66,0x6f,0x72,0x65,0x61,
    0x63,0x68,0x28,0x20,0x6b,0x65,0x79,0x2c,0x76,0x61,0x6c,0x20,0x69,0x6e,0x20,0x74,0x65,0x6d,0x70,0x20,
    0x29,0x0a,0x09,0x7b,0x0a,0x09,0x09,0x69,0x66,0x20,0x28,0x20,0x74,0x79,0x70,0x65,0x6f,0x66,0x28,0x76,
    0x61,0x6c,0x29,0x20,0x3d,0x3d,0x20,0x22,0x66,0x75,0x6e,0x63,0x74,0x69,0x6f,0x6e,0x22,0x20,0x26,0x26,
    0x20,0x6b,0x65,0x79,0x20,0x21,0x3d,0x20,0x22,0x72,0x75,0x6e,0x73,0x63,0x72,0x69,0x70,0x74,0x22,0x20,
    0x29,0x0a,0x09,0x09,0x7b,0x0a,0x09,0x09,0x09,0x70,0x72,0x69,0x6e,0x74,0x6c,0x28,0x20,0x22,0x20,0x20,
    0x20,0x52,0x65,0x70,0x6c,0x61,0x63,0x69,0x6e,0x67,0x20,0x22,0x20,0x2b,0x20,0x6b,0x65,0x79,0x20,0x29,
    0x3b,0x0a,0x09,0x09,0x09,0x73,0x63,0x6f,0x70,0x65,0x5b,0x6b,0x65,0x79,0x5d,0x20,0x3c,0x2d,0x20,0x76,
    0x61,0x6c,0x3b,0x0a,0x09,0x09,0x7d,0x0a,0x09,0x7d,0x0a,0x7d,0x0a,0x0a,0x2f,0x2a,0x0a,0x55,0x4e,0x44,
    0x4f,0x4e,0x45,0x20,0x46,0x4f,0x52,0x20,0x50,0x4f,0x52,0x54,0x41,0x4c,0x32,0x20,0x42,0x52,0x41,0x4e,
    0x43,0x48,0x3a,0x0a,0x57,0x65,0x27,0x72,0x65,0x20,0x6e,0x6f,0x74,0x20,0x73,0x75,0x69,0x6e,0x67,0x20,
    0x74,0x68,0x65,0x20,0x61,0x75,0x74,0x6f,0x2d,0x63,0x6f,0x6e,0x6e,0x65,0x63,0x74,0x69,0x6e,0x67,0x20,
    0x6f,0x66,0x20,0x6f,0x75,0x74,0x70,0x75,0x74,0x73,0x2c,0x20,0x61,0x6c,0x77,0x61,0x79,0x73,0x20,0x63,
    0x61,0x6c,0x6c,0x69,0x6e,0x67,0x20,0x43,0x6f,0x6e,0x6e,0x65,0x63,0x74,0x4f,0x75,0x70,0x75,0x74,0x20,
    0x65,0x78,0x70,0x6c,0x69,0x63,0x69,0x74,0x6c,0x79,0x20,0x69,0x6e,0x20,0x6f,0x75,0x72,0x20,0x73,0x63,
    0x72,0x69,0x70,0x74,0x73,0x2e,0x0a,0x54,0x68,0x65,0x20,0x72,0x65,0x67,0x65,0x78,0x70,0x20,0x6f,0x62,
    0x6a,0x65,0x63,0x74,0x20,0x64,0x6f,0x65,0x73,0x6e,0x27,0x74,0x20,0x73,0x61,0x76,0x65,0x2f,0x6c,0x6f,
    0x61,0x64,0x20,0x70,0x72,0x6f,0x70,0x65,0x72,0x6c,0x79,0x20,0x61,0x6e,0x64,0x20,0x63,0x61,0x75,0x73,
    0x65,0x73,0x20,0x61,0x20,0x63,0x72,0x61,0x73,0x68,0x20,0x77,0x68,0x65,0x6e,0x20,0x75,0x73,0x65,0x64,
    0x20,0x74,0x6f,0x20,0x6d,0x61,0x74,0x63,0x68,0x20,0x61,0x66,0x74,0x65,0x72,0x20,0x61,0x20,0x73,0x61,
    0x76,0x65,0x2f,0x6c,0x6f,0x61,0x64,0x2e,0x0a,0x49,0x6e,0x73,0x74,0x65,0x61,0x64,0x20,0x6f,0x66,0x20,
    0x66,0x69,0x78,0x69,0x6e,0x67,0x20,0x74,0x68,0x69,0x73,0x2c,0x20,0x77,0x65,0x27,0x72,0x65,0x20,0x64,
    0x69,0x73,0x61,0x62,0x6c,0x69,0x6e,0x67,0x20,0x74,0x68,0x65,0x20,0x66,0x65,0x61,0x74,0x75,0x72,0x65,
    0x2e,0x20,0x49,0x66,0x20,0x74,0x68,0x69,0x73,0x20,0x63,0x6c,0x61,0x73,0x73,0x20,0x6f,0x66,0x20,0x70,
    0x72,0x6f,0x62,0x6c,0x65,0x6d,0x20,0x63,0x6f,0x6d,0x65,0x73,0x20,0x75,0x70,0x20,0x6d,0x6f,0x72,0x65,
    0x20,0x77,0x65,0x20,0x6d,0x69,0x67,0x68,0x74,0x0a,0x72,0x65,0x76,0x69,0x73,0x69,0x74,0x2c,0x20,0x6f,
    0x74,0x68,0x65,0x72,0x77,0x69,0x73,0x65,0x20,0x77,0x65,0x27,0x6c,0x6c,0x20,0x6c,0x65,0x61,0x76,0x65,
    0x20,0x69,0x66,0x20,0x6f,0x66,0x66,0x20,0x61,0x6e,0x64,0x20,0x62,0x72,0x6f,0x6b,0x65,0x6e,0x2e,0x0a,
    0x0a,0x5f,0x5f,0x4f,0x75,0x74,0x70,0x75,0x74,0x73,0x50,0x61,0x74,0x74,0x65,0x72,0x6e,0x20,0x3c,0x2d,
    0x20,0x72,0x65,0x67,0x65,0x78,0x70,0x28,0x22,0x5e,0x4f,0x6e,0x2e,0x2a,0x4f,0x75,0x74,0x70,0x75,0x74,
    0x24,0x22,0x29,0x3b,0x0a,0x0a,0x66,0x75,0x6e,0x63,0x74,0x69,0x6f,0x6e,0x20,0x43,0x6f,0x6e,0x6e,0x65,
    0x63,0x74,0x4f,0x75,0x74,0x70,0x75,0x74,0x73,0x28,0x20,0x74,0x61,0x62,0x6c,0x65,0x20,0x29,0x0a,0x7b,
    0x0a,0x09,0x63,0x6f,0x6e,0x73,0x74,0x20,0x6e,0x43,0x68,0x61,0x72,0x73,0x54,0x6f,0x53,0x74,0x72,0x69,
    0x70,0x20,0x3d,0x20,0x36,0x3b,0x0a,0x09,0x66,0x6f,0x72,0x65,0x61,0x63,0x68,0x28,0x20,0x6b,0x65,0x79,
    0x2c,0x20,0x76,0x61,0x6c,0x20,0x69,0x6e,0x20,0x74,0x61,0x62,0x6c,0x65,0x20,0x29,0x0a,0x09,0x7b,0x0a,
    0x09,0x09,0x69,0x66,0x20,0x28,0x20,0x74,0x79,0x70,0x65,0x6f,0x66,0x28,0x20,0x76,0x61,0x6c,0x20,0x29,
    0x20,0x3d,0x3d,0x20,0x22,0x66,0x75,0x6e,0x63,0x74,0x69,0x6f,0x6e,0x22,0x20,0x26,0x26,0x20,0x5f,0x5f,
    0x4f,0x75,0x74,0x70,0x75,0x74,0x73,0x50,0x61,0x74,0x74,0x65,0x72,0x6e,0x2e,0x6d,0x61,0x74,0x63,0x68,
    0x28,0x20,0x6b,0x65,0x79,0x20,0x29,0x20,0x29,0x0a,0x09,0x09,0x7b,0x0a,0x09,0x09,0x09,0x2f,0x2f,0x70,
    0x72,0x69,0x6e,0x74,0x6c,0x28,0x6b,0x65,0x79,0x2e,0x73,0x6c,0x69,0x63,0x65,0x28,0x20,0x30,0x2c,0x20,
    0x6e,0x43,0x68,0x61,0x72,0x73,0x54,0x6f,0x53,0x74,0x72,0x69,0x70,0x20,0x29,0x20,0x29,0x3b,0x0a,0x09,
    0x09,0x09,0x74,0x61,0x62,0x6c,0x65,0x2e,0x73,0x65,0x6c,0x66,0x2e,0x43,0x6f,0x6e,0x6e,0x65,0x63,0x74,
    0x4f,0x75,0x74,0x70,0x75,0x74,0x28,0x20,0x6b,0x65,0x79,0x2e,0x73,0x6c,0x69,0x63,0x65,0x28,0x20,0x30,
    0x2c,0x20,0x6b,0x65,0x79,0x2e,0x6c,0x65,0x6e,0x28,0x29,0x20,0x2d,0x20,0x6e,0x43,0x68,0x61,0x72,0x73,
    0x54,0x6f,0x53,0x74,0x72,0x69,0x70,0x20,0x29,0x2c,0x20,0x6b,0x65,0x79,0x20,0x29,0x3b,0x0a,0x09,0x09,
    0x7d,0x0a,0x09,0x7d,0x0a,0x7d,0x0a,0x2a,0x2f,0x0a,0x0a,0x66,0x75,0x6e,0x63,0x74,0x69,0x6f,0x6e,0x20,
    0x49,0x6e,0x63,0x6c,0x75,0x64,0x65,0x53,0x63,0x72,0x69,0x70,0x74,0x28,0x20,0x6e,0x61,0x6d,0x65,0x2c,
    0x20,0x73,0x63,0x6f,0x70,0x65,0x20,0x3d,0x20,0x6e,0x75,0x6c,0x6c,0x20,0x29,0x0a,0x7b,0x0a,0x09,0x69,
    0x66,0x20,0x28,0x20,0x73,0x63,0x6f,0x70,0x65,0x20,0x3d,0x3d,0x20,0x6e,0x75,0x6c,0x6c,0x20,0x29,0x0a,
    0x09,0x7b,0x0a,0x09,0x09,0x73,0x63,0x6f,0x70,0x65,0x20,0x3d,0x20,0x74,0x68,0x69,0x73,0x3b,0x0a,0x09,
    0x7d,0x0a,0x09,0x72,0x65,0x74,0x75,0x72,0x6e,0x20,0x3a,0x3a,0x44,0x6f,0x49,0x6e,0x63,0x6c,0x75,0x64,
    0x65,0x53,0x63,0x72,0x69,0x70,0x74,0x28,0x20,0x6e,0x61,0x6d,0x65,0x2c,0x20,0x73,0x63,0x6f,0x70,0x65,
    0x20,0x29,0x3b,0x0a,0x7d,0x0a,0x0a,0x2f,0x2f,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,
    0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,
    0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,
    0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x0a,0x2f,0x2f,0x20,0x54,0x65,0x78,0x74,0x20,0x64,0x75,0x6d,0x70,0x20,
    0x74,0x68,0x69,0x73,0x20,0x73,0x63,0x6f,0x70,0x65,0x27,0x73,0x20,0x63,0x6f,0x6e,0x74,0x65,0x6e,0x74,
    0x73,0x20,0x74,0x6f,0x20,0x74,0x68,0x65,0x20,0x63,0x6f,0x6e,0x73,0x6f,0x6c,0x65,0x2e,0x0a,0x2f,0x2f,
    0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,
    0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,
    0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x2d,0x0a,0x66,0x75,
    0x6e,0x63,0x74,0x69,0x6f,0x6e,0x20,0x5f,0x5f,0x44,0x75,0x6d,0x70,0x53,0x63,0x6f,0x70,0x65,0x28,0x20,
    0x64,0x65,0x70,0x74,0x68,0x2c,0x20,0x74,0x61,0x62,0x6c,0x65,0x20,0x29,0x0a,0x7b,0x0a,0x09,0x6c,0x6f,
    0x63,0x61,0x6c,0x20,0x69,0x6e,0x64,0x65,0x6e,0x74,0x3d,0x66,0x75,0x6e,0x63,0x74,0x69,0x6f,0x6e,0x28,
    0x20,0x63,0x6f,0x75,0x6e,0x74,0x20,0x29,0x0a,0x09,0x7b,0x0a,0x09,0x09,0x6c,0x6f,0x63,0x61,0x6c,0x20,
    0x69,0x3b,0x0a,0x09,0x09,0x66,0x6f,0x72,0x28,0x20,0x69,0x20,0x3d,0x20,0x30,0x20,0x3b,0x20,0x69,0x20,
    0x3c,0x20,0x63,0x6f,0x75,0x6e,0x74,0x20,0x3b,0x20,0x69,0x2b,0x2b,0x20,0x29,0x0a,0x09,0x09,0x7b,0x0a,
    0x09,0x09,0x09,0x70,0x72,0x69,0x6e,0x74,0x28,0x22,0x20,0x20,0x20,0x22,0x29,0x3b,0x0a,0x09,0x09,0x7d,
    0x0a,0x09,0x7d,0x0a,0x09,0x0a,0x20,0x20,0x20,0x20,0x66,0x6f,0x72,0x65,0x61,0x63,0x68,0x28,0x6b,0x65,
    0x79,0x2c,0x20,0x76,0x61,0x6c,0x75,0x65,0x20,0x69,0x6e,0x20,0x74,0x61,0x62,0x6c,0x65,0x29,0x0a,0x20,
    0x20,0x20,0x20,0x7b,0x0a,0x09,0x09,0x69,0x6e,0x64,0x65,0x6e,0x74,0x28,0x64,0x65,0x70,0x74,0x68,0x29,
    0x3b,0x0a,0x09,0x09,0x70,0x72,0x69,0x6e,0x74,0x28,0x20,0x6b,0x65,0x79,0x20,0x29,0x3b,0x0a,0x20,0x20,
    0x20,0x20,0x20,0x20,0x20,0x20,0x73,0x77,0x69,0x74,0x63,0x68,0x20,0x28,0x74,0x79,0x70,0x65,0x28,0x76,
    0x61,0x6c,0x75,0x65,0x29,0x29,0x0a,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x7b,0x0a,0x20,0x20,0x20,
    0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x63,0x61,0x73,0x65,0x20,0x22,0x74,0x61,0x62,0x6c,0x65,
    0x22,0x3a,0x0a,0x09,0x09,0x09,0x09,0x70,0x72,0x69,0x6e,0x74,0x28,0x22,0x28,0x54,0x41,0x42,0x4c,0x45,
    0x29,0x5c,0x6e,0x22,0x29,0x3b,0x0a,0x09,0x09,0x09,0x09,0x69,0x6e,0x64,0x65,0x6e,0x74,0x28,0x64,0x65,
    0x70,0x74,0x68,0x29,0x3b,0x0a,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
    0x20,0x20,0x70,0x72,0x69,0x6e,0x74,0x28,0x22,0x7b,0x5c,0x6e,0x22,0x29,0x3b,0x0a,0x20,0x20,0x20,0x20,
    0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x5f,0x5f,0x44,0x75,0x6d,0x70,0x53,0x63,
    0x6f,0x70,0x65,0x28,0x20,0x64,0x65,0x70,0x74,0x68,0x20,0x2b,0x20,0x31,0x2c,0x20,0x76,0x61,0x6c,0x75,
    0x65,0x29,0x3b,0x0a,0x09,0x09,0x09,0x09,0x69,0x6e,0x64,0x65,0x6e,0x74,0x28,0x64,0x65,0x70,0x74,0x68,
    0x29,0x3b,0x0a,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x70,
    0x72,0x69,0x6e,0x74,0x28,0x22,0x7d,0x22,0x29,0x3b,0x0a,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
    0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x62,0x72,0x65,0x61,0x6b,0x3b,0x0a,0x20,0x20,0x20,0x20,0x20,0x20,
    0x20,0x20,0x20,0x20,0x20,0x20,0x63,0x61,0x73,0x65,0x20,0x22,0x61,0x72,0x72,0x61,0x79,0x22,0x3a,0x0a,
    0x09,0x09,0x09,0x09,0x70,0x72,0x69,0x6e,0x74,0x28,0x22,0x28,0x41,0x52,0x52,0x41,0x59,0x29,0x5c,0x6e,
    0x22,0x29,0x3b,0x0a,0x09,0x09,0x09,0x09,0x69,0x6e,0x64,0x65,0x6e,0x74,0x28,0x64,0x65,0x70,0x74,0x68,
    0x29,0x3b,0x0a,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x70,
    0x72,0x69,0x6e,0x74,0x28,0x22,0x5b,0x5c,0x6e,0x22,0x29,0x0a,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
    0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x5f,0x5f,0x44,0x75,0x6d,0x70,0x53,0x63,0x6f,0x70,0x65,0x28,
    0x20,0x64,0x65,0x70,0x74,0x68,0x20,0x2b,0x20,0x31,0x2c,0x20,0x76,0x61,0x6c,0x75,0x65,0x29,0x3b,0x0a,
    0x09,0x09,0x09,0x09,0x69,0x6e,0x64,0x65,0x6e,0x74,0x28,0x64,0x65,0x70,0x74,0x68,0x29,0x3b,0x0a,0x20,
    0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x70,0x72,0x69,0x6e,0x74,
    0x28,0x22,0x5d,0x22,0x29,0x3b,0x0a,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
    0x20,0x20,0x20,0x62,0x72,0x65,0x61,0x6b,0x3b,0x0a,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
    0x20,0x20,0x63,0x61,0x73,0x65,0x20,0x22,0x73,0x74,0x72,0x69,0x6e,0x67,0x22,0x3a,0x0a,0x20,0x20,0x20,
    0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x70,0x72,0x69,0x6e,0x74,0x28,0x22,
    0x20,0x3d,0x20,0x5c,0x22,0x22,0x29,0x3b,0x0a,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
    0x20,0x20,0x20,0x20,0x20,0x70,0x72,0x69,0x6e,0x74,0x28,0x76,0x61,0x6c,0x75,0x65,0x29,0x3b,0x0a,0x20,
    0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x70,0x72,0x69,0x6e,0x74,
    0x28,0x22,0x5c,0x22,0x22,0x29,0x3b,0x0a,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
    0x20,0x20,0x20,0x20,0x62,0x72,0x65,0x61,0x6b,0x3b,0x0a,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
    0x20,0x20,0x20,0x64,0x65,0x66,0x61,0x75,0x6c,0x74,0x3a,0x0a,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
    0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x70,0x72,0x69,0x6e,0x74,0x28,0x22,0x20,0x3d,0x20,0x22,0x29,
    0x3b,0x0a,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x70,0x72,
    0x69,0x6e,0x74,0x28,0x76,0x61,0x6c,0x75,0x65,0x29,0x3b,0x0a,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
    0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x62,0x72,0x65,0x61,0x6b,0x3b,0x0a,0x20,0x20,0x20,0x20,0x20,
    0x20,0x20,0x20,0x7d,0x0a,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x70,0x72,0x69,0x6e,0x74,0x28,0x22,
    0x5c,0x6e,0x22,0x29,0x3b,0x20,0x20,0x0a,0x09,0x7d,0x0a,0x7d,0x0a,0x0a,0x00
};
/*//========== Copyright (c) 2008, Valve Corporation, All rights reserved. ========
//
// Purpose:
//
//=============================================================================

function UniqueString( string = "" )
{
	return DoUniqueString( string.tostring() );
}

function EntFire( target, action, value = null, delay = 0.0, activator = null )
{
	if ( !value )
	{
		value = "";
	}

	local caller = null;
	if ( "self" in this )
	{
		caller = self;
		if ( !activator )
		{
			activator = self;
		}
	}

	DoEntFire( target.tostring(), action.tostring(), value.tostring(), delay, activator, caller );
}

function __ReplaceClosures( script, scope )
{
	if ( !scope )
	{
		scope = getroottable();
	}

	local tempParent = { getroottable = function() { return null; } };
	local temp = { runscript = script };
	delegate tempParent : temp;

	temp.runscript()
	foreach( key,val in temp )
	{
		if ( typeof(val) == "function" && key != "runscript" )
		{
			printl( "   Replacing " + key );
			scope[key] <- val;
		}
	}
}

//UNDONE FOR PORTAL2 BRANCH:
//We're not suing the auto-connecting of outputs, always calling ConnectOuput explicitly in our scripts.
//The regexp object doesn't save/load properly and causes a crash when used to match after a save/load.
//Instead of fixing this, we're disabling the feature. If this class of problem comes up more we might
//revisit, otherwise we'll leave if off and broken.
//
//__OutputsPattern <- regexp("^On.*Output$");
//
//function ConnectOutputs( table )
//{
//	const nCharsToStrip = 6;
//	foreach( key, val in table )
//	{
//		if ( typeof( val ) == "function" && __OutputsPattern.match( key ) )
//		{
//			//printl(key.slice( 0, nCharsToStrip ) );
//			table.self.ConnectOutput( key.slice( 0, key.len() - nCharsToStrip ), key );
//		}
//	}
//}

function IncludeScript( name, scope = null )
{
	if ( scope == null )
	{
		scope = this;
	}
	return ::DoIncludeScript( name, scope );
}

//---------------------------------------------------------
// Text dump this scope's contents to the console.
//---------------------------------------------------------
function __DumpScope( depth, table )
{
	local indent=function( count )
	{
		local i;
		for( i = 0 ; i < count ; i++ )
		{
			print("   ");
		}
	}

    foreach(key, value in table)
    {
		indent(depth);
		print( key );
        switch (type(value))
        {
            case "table":
				print("(TABLE)\n");
				indent(depth);
                print("{\n");
                __DumpScope( depth + 1, value);
				indent(depth);
                print("}");
                break;
            case "array":
				print("(ARRAY)\n");
				indent(depth);
                print("[\n")
                __DumpScope( depth + 1, value);
				indent(depth);
                print("]");
                break;
            case "string":
                print(" = \"");
                print(value);
                print("\"");
                break;
            default:
                print(" = ");
                print(value);
                break;
        }
        print("\n");
	}
}*/
//lwss end

extern ScriptClassDesc_t * GetScriptDesc( CBaseEntity * );
ConVar script_connect_debugger_on_mapspawn( side_registry(), "script_connect_debugger_on_mapspawn", "0" );

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
class CScriptEntityIterator
{
public:
	HSCRIPT First() { return Next(nullptr); }

	HSCRIPT Next( HSCRIPT hStartEntity )
	{
		return ToHScript( gEntList.NextEnt( ToEnt( hStartEntity ) ) );
	}

	HSCRIPT CreateByClassname( const char *className )
	{
		return ToHScript( CreateEntityByName( className ) );
	}

	HSCRIPT FindByClassname( HSCRIPT hStartEntity, const char *szName )
	{
		return ToHScript( gEntList.FindEntityByClassname( ToEnt( hStartEntity ), szName ) );
	}

	HSCRIPT FindByName( HSCRIPT hStartEntity, const char *szName )
	{
		return ToHScript( gEntList.FindEntityByName( ToEnt( hStartEntity ), szName ) );
	}

	HSCRIPT FindInSphere( HSCRIPT hStartEntity, const Vector &vecCenter, float flRadius )
	{
		return ToHScript( gEntList.FindEntityInSphere( ToEnt( hStartEntity ), vecCenter, flRadius ) );
	}

	HSCRIPT FindByTarget( HSCRIPT hStartEntity, const char *szName )
	{
		return ToHScript( gEntList.FindEntityByTarget( ToEnt( hStartEntity ), szName ) );
	}

	HSCRIPT FindByModel( HSCRIPT hStartEntity, const char *szModelName )
	{
		return ToHScript( gEntList.FindEntityByModel( ToEnt( hStartEntity ), szModelName ) );
	}

	HSCRIPT FindByNameNearest( const char *szName, const Vector &vecSrc, float flRadius )
	{
		return ToHScript( gEntList.FindEntityByNameNearest( szName, vecSrc, flRadius ) );
	}

	HSCRIPT FindByNameWithin( HSCRIPT hStartEntity, const char *szName, const Vector &vecSrc, float flRadius )
	{
		return ToHScript( gEntList.FindEntityByNameWithin( ToEnt( hStartEntity ), szName, vecSrc, flRadius ) );
	}

	HSCRIPT FindByClassnameNearest( const char *szName, const Vector &vecSrc, float flRadius )
	{
		return ToHScript( gEntList.FindEntityByClassnameNearest( szName, vecSrc, flRadius ) );
	}

	HSCRIPT FindByClassnameWithin( HSCRIPT hStartEntity , const char *szName, const Vector &vecSrc, float flRadius )
	{
		return ToHScript( gEntList.FindEntityByClassnameWithin( ToEnt( hStartEntity ), szName, vecSrc, flRadius ) );
	}

private:
} g_ScriptEntityIterator;

BEGIN_SCRIPTDESC_ROOT_NAMED( CScriptEntityIterator, "CEntities", SCRIPT_SINGLETON "The global list of entities" )
	DEFINE_SCRIPTFUNC( First, "Begin an iteration over the list of entities" )
	DEFINE_SCRIPTFUNC( Next, "Continue an iteration over the list of entities, providing reference to a previously found entity" )
	DEFINE_SCRIPTFUNC( CreateByClassname, "Creates an entity by classname" )
	DEFINE_SCRIPTFUNC( FindByClassname, "Find entities by class name. Pass 'null' to start an iteration, or reference to a previously found entity to continue a search"  )
	DEFINE_SCRIPTFUNC( FindByName, "Find entities by name. Pass 'null' to start an iteration, or reference to a previously found entity to continue a search"  )
	DEFINE_SCRIPTFUNC( FindInSphere, "Find entities within a radius. Pass 'null' to start an iteration, or reference to a previously found entity to continue a search"  )
	DEFINE_SCRIPTFUNC( FindByTarget, "Find entities by targetname. Pass 'null' to start an iteration, or reference to a previously found entity to continue a search"  )
	DEFINE_SCRIPTFUNC( FindByModel, "Find entities by model name. Pass 'null' to start an iteration, or reference to a previously found entity to continue a search"  )
	DEFINE_SCRIPTFUNC( FindByNameNearest, "Find entities by name nearest to a point."  )
	DEFINE_SCRIPTFUNC( FindByNameWithin, "Find entities by name within a radius. Pass 'null' to start an iteration, or reference to a previously found entity to continue a search"  )
	DEFINE_SCRIPTFUNC( FindByClassnameNearest, "Find entities by class name nearest to a point."  )
	DEFINE_SCRIPTFUNC( FindByClassnameWithin, "Find entities by class name within a radius. Pass 'null' to start an iteration, or reference to a previously found entity to continue a search"  )
END_SCRIPTDESC();

// ----------------------------------------------------------------------------
// KeyValues access - CBaseEntity::ScriptGetKeyFromModel returns root KeyValues
// ----------------------------------------------------------------------------

BEGIN_SCRIPTDESC_ROOT( CScriptKeyValues, "Wrapper class over KeyValues instance" )
	DEFINE_SCRIPT_CONSTRUCTOR()	
	DEFINE_SCRIPTFUNC_NAMED( ScriptFindKey, "FindKey", "Given a KeyValues object and a key name, find a KeyValues object associated with the key name" );
	DEFINE_SCRIPTFUNC_NAMED( ScriptGetFirstSubKey, "GetFirstSubKey", "Given a KeyValues object, return the first sub key object" );
	DEFINE_SCRIPTFUNC_NAMED( ScriptGetNextKey, "GetNextKey", "Given a KeyValues object, return the next key object in a sub key group" );
	DEFINE_SCRIPTFUNC_NAMED( ScriptGetKeyValueInt, "GetKeyInt", "Given a KeyValues object and a key name, return associated integer value" );
	DEFINE_SCRIPTFUNC_NAMED( ScriptGetKeyValueFloat, "GetKeyFloat", "Given a KeyValues object and a key name, return associated float value" );
	DEFINE_SCRIPTFUNC_NAMED( ScriptGetKeyValueBool, "GetKeyBool", "Given a KeyValues object and a key name, return associated bool value" );
	DEFINE_SCRIPTFUNC_NAMED( ScriptGetKeyValueString, "GetKeyString", "Given a KeyValues object and a key name, return associated string value" );
	DEFINE_SCRIPTFUNC_NAMED( ScriptIsKeyValueEmpty, "IsKeyEmpty", "Given a KeyValues object and a key name, return true if key name has no value" );
	DEFINE_SCRIPTFUNC_NAMED( ScriptReleaseKeyValues, "ReleaseKeyValues", "Given a root KeyValues object, release its contents" );
END_SCRIPTDESC();

HSCRIPT CScriptKeyValues::ScriptFindKey( const char *pszName )
{
	KeyValues *pKeyValues = m_pKeyValues->FindKey(pszName);
	if ( pKeyValues == nullptr )
		return nullptr;

	CScriptKeyValues *pScriptKey = new CScriptKeyValues( pKeyValues );

	// UNDONE: who calls ReleaseInstance on this??
	HSCRIPT hScriptInstance = g_pScriptVM->RegisterInstance( pScriptKey );
	return hScriptInstance;
}

HSCRIPT CScriptKeyValues::ScriptGetFirstSubKey( void )
{
	KeyValues *pKeyValues = m_pKeyValues->GetFirstSubKey();
	if ( pKeyValues == nullptr )
		return nullptr;

	CScriptKeyValues *pScriptKey = new CScriptKeyValues( pKeyValues );

	// UNDONE: who calls ReleaseInstance on this??
	HSCRIPT hScriptInstance = g_pScriptVM->RegisterInstance( pScriptKey );
	return hScriptInstance;
}

HSCRIPT CScriptKeyValues::ScriptGetNextKey( void )
{
	KeyValues *pKeyValues = m_pKeyValues->GetNextKey();
	if ( pKeyValues == nullptr )
		return nullptr;

	CScriptKeyValues *pScriptKey = new CScriptKeyValues( pKeyValues );

	// UNDONE: who calls ReleaseInstance on this??
	HSCRIPT hScriptInstance = g_pScriptVM->RegisterInstance( pScriptKey );
	return hScriptInstance;
}

int CScriptKeyValues::ScriptGetKeyValueInt( const char *pszName )
{
	int i = m_pKeyValues->GetInt( pszName );
	return i;
}

float CScriptKeyValues::ScriptGetKeyValueFloat( const char *pszName )
{
	float f = m_pKeyValues->GetFloat( pszName );
	return f;
}

const char *CScriptKeyValues::ScriptGetKeyValueString( const char *pszName )
{
	const char *psz = m_pKeyValues->GetString( pszName );
	return psz;
}

bool CScriptKeyValues::ScriptIsKeyValueEmpty( const char *pszName )
{
	bool b = m_pKeyValues->IsEmpty( pszName );
	return b;
}

bool CScriptKeyValues::ScriptGetKeyValueBool( const char *pszName )
{
	bool b = m_pKeyValues->GetBool( pszName );
	return b;
}

void CScriptKeyValues::ScriptReleaseKeyValues( )
{
	m_pKeyValues->deleteThis();
	m_pKeyValues = nullptr;
}

// constructors
CScriptKeyValues::CScriptKeyValues( KeyValues *pKeyValues )
{
	m_pKeyValues = pKeyValues;
}

// destructor
CScriptKeyValues::~CScriptKeyValues( )
{
	if (m_pKeyValues)
	{
		m_pKeyValues->deleteThis();
	}
	m_pKeyValues = nullptr;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static float Time()
{
	return gpGlobals->curtime;
}

static float FrameTime()
{
	return gpGlobals->frametime;
}

static void SendToConsole( const char *pszCommand )
{
	CBasePlayer *pPlayer = UTIL_GetLocalPlayerOrListenServerHost();
	if ( !pPlayer )
	{
		DevMsg ("Cannot execute \"%s\", no player\n", pszCommand );
		return;
	}

	engine->ClientCommand( pPlayer->edict(), "%s", pszCommand );
}

static void SendToConsoleServer( const char *pszCommand )
{
	// Parse the text into distinct commands
	const char *pCurrentCommand = pszCommand;
	int nOffsetToNextCommand;
	int	nLen = V_strlen( pszCommand );
	for( ; nLen > 0; nLen -= nOffsetToNextCommand+1, pCurrentCommand += nOffsetToNextCommand+1 )
	{
		// find a \n or ; line break
		int nCommandLength;
		UTIL_GetNextCommandLength( pCurrentCommand, nLen, &nCommandLength, &nOffsetToNextCommand );
		if ( nCommandLength <= 0 )
			continue;

		engine->ServerCommand( UTIL_VarArgs( "whitelistcmd %.*s\n", nCommandLength, pCurrentCommand ) );
	}
}

static const char *GetMapName()
{
	return STRING( gpGlobals->mapname );
}

extern ConVar	loopsingleplayermaps;

static bool LoopSinglePlayerMaps()
{
	return loopsingleplayermaps.GetBool();
}

static const char *DoUniqueString( const char *pszBase )
{
	static char szBuf[512];
	g_pScriptVM->GenerateUniqueKey( pszBase, szBuf, std::size( szBuf ) );
	return szBuf;
}

static void DoEntFire( const char *pszTarget, const char *pszAction, const char *pszValue, float delay, HSCRIPT hActivator, HSCRIPT hCaller )
{
	const char *target = "", *action = "Use";
	variant_t value;

	target = STRING( AllocPooledString( pszTarget ) );

	// Don't allow them to run anything on a point_servercommand unless they're the host player. Otherwise they can ent_fire
	// and run any command on the server. Admittedly, they can only do the ent_fire if sv_cheats is on, but 
	// people complained about users resetting the rcon password if the server briefly turned on cheats like this:
	//    give point_servercommand
	//    ent_fire point_servercommand command "rcon_password mynewpassword"
	if ( gpGlobals->maxClients > 1 && V_stricmp( target, "point_servercommand" ) == 0 )
	{
		return;
	}

	if ( *pszAction )
	{
		action = STRING( AllocPooledString( pszAction ) );
	}
	if ( *pszValue )
	{
		value.SetString( AllocPooledString( pszValue ) );
	}
	if ( delay < 0 )
	{
		delay = 0;
	}

	g_EventQueue.AddEvent( target, action, value, delay, ToEnt(hActivator), ToEnt(hCaller) );
}

static void DoRecordAchievementEvent( const char *pszAchievementname, int iPlayerIndex )
{
	if ( iPlayerIndex < 0 )
	{
		DevWarning( "DoRecordAchievementEvent called with invalid player index (%s, %d)!\n", pszAchievementname, iPlayerIndex );
		return;
	}
	CBasePlayer *pPlayer = nullptr;
	if ( iPlayerIndex > 0 )
	{
		pPlayer = UTIL_PlayerByIndex( iPlayerIndex );
		if ( !pPlayer )
		{
			DevWarning( "DoRecordAchievementEvent called with a player index that doesn't resolve to a player (%s, %d)!\n", pszAchievementname, iPlayerIndex );
			return;
		}
	}
	UTIL_RecordAchievementEvent( pszAchievementname, pPlayer );
}

bool DoIncludeScript( const char *pszScript, HSCRIPT hScope )
{
	if ( !VScriptRunScript( pszScript, hScope, true ) )
	{
		g_pScriptVM->RaiseException( CFmtStr( "Failed to include script \"%s\"", ( pszScript ) ? pszScript : "unknown" ) );
		return false;
	}
	return true;
}

int GetDeveloperLevel()
{
	return developer.GetInt();
}

static void ScriptDispatchParticleEffect( const char *pszParticleName, const Vector &vOrigin, const Vector &vAngle )
{
	QAngle qAngle;
	VectorAngles( vAngle, qAngle );
	DispatchParticleEffect( pszParticleName, vOrigin, qAngle );
}

HSCRIPT CreateProp( const char *pszEntityName, const Vector &vOrigin, const char *pszModelName, int iAnim )
{
	CBaseAnimating *pBaseEntity = (CBaseAnimating *)CreateEntityByName( pszEntityName );
	pBaseEntity->SetAbsOrigin( vOrigin );
	pBaseEntity->SetModel( pszModelName );
	pBaseEntity->SetPlaybackRate( 1.0f );

	int iSequence = pBaseEntity->SelectWeightedSequence( (Activity)iAnim );

	if ( iSequence != -1 )
	{
		pBaseEntity->SetSequence( iSequence );
	}

	return ToHScript( pBaseEntity );
}

//--------------------------------------------------------------------------------------------------
// Use an entity's script instance to add an entity IO event (used for firing events on unnamed entities from vscript)
//--------------------------------------------------------------------------------------------------
static void DoEntFireByInstanceHandle( HSCRIPT hTarget, const char *pszAction, const char *pszValue, float delay, HSCRIPT hActivator, HSCRIPT hCaller )
{
	const char *action = "Use";
	variant_t value;

	if ( *pszAction )
	{
		action = STRING( AllocPooledString( pszAction ) );
	}
	if ( *pszValue )
	{
		value.SetString( AllocPooledString( pszValue ) );
	}
	if ( delay < 0 )
	{
		delay = 0;
	}

	CBaseEntity* pTarget = ToEnt(hTarget);

	if ( !pTarget )
	{
		Warning( "VScript error: DoEntFire was passed an invalid entity instance.\n" );
		return;
	}

	g_EventQueue.AddEvent( pTarget, action, value, delay, ToEnt(hActivator), ToEnt(hCaller) );
}

static float ScriptTraceLine( const Vector &vecStart, const Vector &vecEnd, HSCRIPT entIgnore )
{
	// UTIL_TraceLine( vecAbsStart, vecAbsEnd, MASK_BLOCKLOS, pLooker, COLLISION_GROUP_NONE, ptr );
	trace_t tr;
	CBaseEntity *pLooker = ToEnt(entIgnore);
	UTIL_TraceLine( vecStart, vecEnd, MASK_NPCWORLDSTATIC, pLooker, COLLISION_GROUP_NONE, &tr);
	if (tr.fractionleftsolid && tr.startsolid)
	{
		return 1.0 - tr.fractionleftsolid;
	}
	else
	{
		return tr.fraction;
	}
}

bool VScriptServerInit()
{
	if( scriptmanager != nullptr )
	{
		ScriptLanguage_t scriptLanguage = SL_DEFAULT;

		char const *pszScriptLanguage;
		if ( CommandLine()->CheckParm( "-scriptlang", &pszScriptLanguage ) )
		{
			if( !V_stricmp(pszScriptLanguage, "gamemonkey") )
			{
				scriptLanguage = SL_GAMEMONKEY;
			}
			else if( !V_stricmp(pszScriptLanguage, "squirrel") )
			{
				scriptLanguage = SL_SQUIRREL;
			}
			else if( !V_stricmp(pszScriptLanguage, "python") )
			{
				scriptLanguage = SL_PYTHON;
			}
			else
			{
				DevWarning("-server_script does not recognize a language named '%s'. virtual machine did NOT start.\n", pszScriptLanguage );
				scriptLanguage = SL_NONE;
			}

		}
		if( scriptLanguage != SL_NONE )
		{
			if ( g_pScriptVM == nullptr )
				g_pScriptVM = scriptmanager->CreateVM( scriptLanguage );

			if( g_pScriptVM )
			{
				Log_Msg( LOG_VScript, "VSCRIPT: Started VScript virtual machine using script language '%s'\n", g_pScriptVM->GetLanguageName() );
				ScriptRegisterFunctionNamed( g_pScriptVM, UTIL_ShowMessageAll, "ShowMessage", "Print a hud message on all clients" );

				ScriptRegisterFunction( g_pScriptVM, SendToConsole, "Send a string to the console as a command" );
				ScriptRegisterFunction( g_pScriptVM, SendToConsoleServer, "Send a string that gets executed on the server as a ServerCommand" );
				ScriptRegisterFunction( g_pScriptVM, GetMapName, "Get the name of the map.");
				ScriptRegisterFunction( g_pScriptVM, LoopSinglePlayerMaps, "Run the single player maps in a continuous loop.");

				ScriptRegisterFunctionNamed( g_pScriptVM, ScriptTraceLine, "TraceLine", "given 2 points & ent to ignore, return fraction along line that hits world or models" );

				ScriptRegisterFunction( g_pScriptVM, Time, "Get the current server time" );
				ScriptRegisterFunction( g_pScriptVM, FrameTime, "Get the time spent on the server in the last frame" );
				ScriptRegisterFunction( g_pScriptVM, DoEntFire, SCRIPT_ALIAS( "EntFire", "Generate and entity i/o event" ) );
				ScriptRegisterFunctionNamed( g_pScriptVM, DoEntFireByInstanceHandle, "EntFireByHandle", "Generate and entity i/o event. First parameter is an entity instance." );
				ScriptRegisterFunction( g_pScriptVM, DoUniqueString, SCRIPT_ALIAS( "UniqueString", "Generate a string guaranteed to be unique across the life of the script VM, with an optional root string. Useful for adding data to tables when not sure what keys are already in use in that table." ) );
				ScriptRegisterFunctionNamed( g_pScriptVM, ScriptCreateSceneEntity, "CreateSceneEntity", "Create a scene entity to play the specified scene." );
				ScriptRegisterFunctionNamed( g_pScriptVM, NDebugOverlay::Box, "DebugDrawBox", "Draw a debug overlay box" );
				ScriptRegisterFunctionNamed( g_pScriptVM, NDebugOverlay::Line, "DebugDrawLine", "Draw a debug overlay box" );
				ScriptRegisterFunction( g_pScriptVM, DoIncludeScript, "Execute a script (internal)" );
				ScriptRegisterFunction( g_pScriptVM, CreateProp, "Create a physics prop" );
				ScriptRegisterFunctionNamed( g_pScriptVM, DoRecordAchievementEvent, "RecordAchievementEvent", "Records achievement event or progress" );
				ScriptRegisterFunction( g_pScriptVM, GetDeveloperLevel, "Gets the level of 'developer'" );
				ScriptRegisterFunctionNamed( g_pScriptVM, ScriptDispatchParticleEffect, "DispatchParticleEffect", "Dispatches a one-off particle system" );

				g_pScriptVM->RegisterAllClasses();
				
				if ( GameRules() )
				{
					GameRules()->RegisterScriptFunctions();
				}

				g_pScriptVM->RegisterInstance( &g_ScriptEntityIterator, "Entities" );

				if ( scriptLanguage == SL_SQUIRREL )
				{
					g_pScriptVM->Run( g_Script_vscript_server );
				}

				VScriptRunScript( "mapspawn", false );

				if ( script_connect_debugger_on_mapspawn.GetBool() )
				{
					g_pScriptVM->ConnectDebugger();
				}
				return true;
			}
			else
			{
				DevWarning("VM Did not start!\n");
			}
		}
	}
	else
	{
		Log_Msg( LOG_VScript, "\nVSCRIPT: Scripting is disabled.\n" );
	}
	g_pScriptVM = nullptr;
	return false;
}

void VScriptServerTerm()
{
	if( g_pScriptVM != nullptr )
	{
		if( g_pScriptVM )
		{
			scriptmanager->DestroyVM( g_pScriptVM );
			g_pScriptVM = nullptr;
		}
	}
}

bool VScriptServerReplaceClosures( const char *pszScriptName, HSCRIPT hScope, bool bWarnMissing )
{
	if ( !g_pScriptVM )
	{
		return false;
	}

	HSCRIPT hReplaceClosuresFunc = g_pScriptVM->LookupFunction( "__ReplaceClosures" );
	if ( !hReplaceClosuresFunc )
	{
		return false;
	}
	HSCRIPT hNewScript =  VScriptCompileScript( pszScriptName, bWarnMissing );
	if ( !hNewScript )
	{
		g_pScriptVM->ReleaseFunction( hReplaceClosuresFunc );
		return false;
	}

	g_pScriptVM->Call( hReplaceClosuresFunc, nullptr, true, nullptr, hNewScript, hScope );
	g_pScriptVM->ReleaseFunction( hReplaceClosuresFunc );
	g_pScriptVM->ReleaseScript( hNewScript );
	return true;
}

static void script_reload_code( const CCommand &args );
static ConCommand script_reload_code_command( side_registry(), "script_reload_code", script_reload_code, "Execute a vscript file, replacing existing functions with the functions in the run script" );
static void script_reload_code( const CCommand &args )
{
	if ( !*args[1] )
	{
		Log_Warning( LOG_VScript, "No script specified\n" );
		return;
	}

	if ( !g_pScriptVM )
	{
		Log_Warning( LOG_VScript, "Scripting disabled or no server running\n" );
		return;
	}

	VScriptServerReplaceClosures( args[1], nullptr, true );
}

static void script_reload_entity_code( const CCommand &args );
static ConCommand script_reload_entity_code_command( side_registry(), "script_reload_entity_code", script_reload_entity_code, "Execute all of this entity's VScripts, replacing existing functions with the functions in the run scripts" );
extern CBaseEntity *GetNextCommandEntity( CBasePlayer *pPlayer, const char *name, CBaseEntity *ent );
static void script_reload_entity_code( const CCommand &args )
{

	const char *pszTarget = "";
	if ( *args[1] )
	{
		pszTarget = args[1];
	}

	if ( !g_pScriptVM )
	{
		Log_Warning( LOG_VScript, "Scripting disabled or no server running\n" );
		return;
	}

	CBasePlayer *pPlayer = UTIL_GetCommandClient();
	if ( !pPlayer )
		return;

	CBaseEntity *pEntity = nullptr;
	while ( (pEntity = GetNextCommandEntity( pPlayer, pszTarget, pEntity )) != nullptr )
	{
		if ( pEntity->m_ScriptScope.IsInitialized() && pEntity->m_iszVScripts != NULL_STRING )
		{
			char szScriptsList[255];
			V_strcpy_safe( szScriptsList, STRING(pEntity->m_iszVScripts) );
			std::vector< std::string > szScripts;
			V_SplitString( szScriptsList, " ", szScripts);

			for( int i = 0 ; i < std::ssize( szScripts ) ; i++ )
			{
				VScriptServerReplaceClosures( szScripts[i].c_str(), pEntity->m_ScriptScope, true );
			}
		}
	}
}

static void script_reload_think( const CCommand &args );
static ConCommand script_reload_think_command( side_registry(), "script_reload_think", script_reload_think, "Execute an activation script, replacing existing functions with the functions in the run script" );
extern CBaseEntity *GetNextCommandEntity( CBasePlayer *pPlayer, const char *name, CBaseEntity *ent );
static void script_reload_think( const CCommand &args )
{

	const char *pszTarget = "";
	if ( *args[1] )
	{
		pszTarget = args[1];
	}

	if ( !g_pScriptVM )
	{
		Log_Warning( LOG_VScript, "Scripting disabled or no server running\n" );
		return;
	}

	CBasePlayer *pPlayer = UTIL_GetCommandClient();
	if ( !pPlayer )
		return;

	CBaseEntity *pEntity = nullptr;
	while ( (pEntity = GetNextCommandEntity( pPlayer, pszTarget, pEntity )) != nullptr )
	{
		if ( pEntity->m_ScriptScope.IsInitialized() && pEntity->m_iszScriptThinkFunction != NULL_STRING )
		{
			VScriptServerReplaceClosures( STRING(pEntity->m_iszScriptThinkFunction), pEntity->m_ScriptScope, true );
		}
	}
}

class CVScriptGameSystem : public CAutoGameSystemPerFrame
{
public:
	// Inherited from IAutoServerSystem
	virtual void LevelInitPreEntity( void )
	{
		// <sergiy> Note: we may need script VM garbage collection at this point in the future. Currently, VM does not persist 
		//          across level boundaries. GC is not necessary because our scripts are supposed to never create circular references
		//          and everything else is handled with ref counting. For the case of bugs creating circular references, the plan is to add
		//          diagnostics that detects such loops and warns the developer.
		
		m_bAllowEntityCreationInScripts = true;
		VScriptServerInit();
	}

	virtual void LevelInitPostEntity( void )
	{
		m_bAllowEntityCreationInScripts = false;
	}

	virtual void LevelShutdownPostEntity( void )
	{
		VScriptServerTerm();
	}

	virtual void FrameUpdatePostEntityThink() 
	{ 
		if ( g_pScriptVM )
			g_pScriptVM->Frame( gpGlobals->frametime );
	}

	bool m_bAllowEntityCreationInScripts;
};

CVScriptGameSystem g_VScriptGameSystem;

bool IsEntityCreationAllowedInScripts( void )
{
	return g_VScriptGameSystem.m_bAllowEntityCreationInScripts;
}

static short VSCRIPT_SERVER_SAVE_RESTORE_VERSION = 2;

//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------

//-------------------------------------

//-----------------------------------------------------------------------------

bool CBaseEntityScriptInstanceHelper::ToString( void *p, char *pBuf, int bufSize )	
{
	CBaseEntity *pEntity = (CBaseEntity *)p;
	if ( pEntity->GetEntityName() != NULL_STRING )
	{
		V_snprintf( pBuf, bufSize, "([%d] %s: %s)", pEntity->entindex(), STRING(pEntity->m_iClassname), STRING( pEntity->GetEntityName() ) );
	}
	else
	{
		V_snprintf( pBuf, bufSize, "([%d] %s)", pEntity->entindex(), STRING(pEntity->m_iClassname) );
	}
	return true; 
}

void *CBaseEntityScriptInstanceHelper::BindOnRead( HSCRIPT hInstance, void *pOld, const char *pszId )
{
	return nullptr;
}

CBaseEntityScriptInstanceHelper g_BaseEntityScriptInstanceHelper;

class CBaseEntity;

using namespace ::ks::reflect;

IScriptVM * g_pScriptVM;
extern ScriptClassDesc_t * GetScriptDesc( CBaseEntity * );

LoggingChannelID_t LOG_VScript = LoggingSystem_RegisterLoggingChannel( "VScript", nullptr, LCF_CONSOLE_ONLY, LS_WARNING );;
HSCRIPT VScriptCompileScript( const char *pszScriptName, bool bWarnMissing )
{
	if ( !g_pScriptVM )
	{
		return nullptr;
	}

	static const char *pszExtensions[] =
	{
		"",		// SL_NONE
		".gm",	// SL_GAMEMONKEY
		".nut",	// SL_SQUIRREL
		".lua", // SL_LUA
		".py",  // SL_PYTHON
	};

	const char *pszVMExtension = pszExtensions[g_pScriptVM->GetLanguage()];
	const char *pszIncomingExtension = V_strrchr( pszScriptName , '.' );
	if ( pszIncomingExtension && V_strcmp( pszIncomingExtension, pszVMExtension ) != 0 )
	{
		Log_Warning( LOG_VScript, "Script file type does not match VM type\n" );
		return nullptr;
	}

	CFmtStr scriptPath;
	if ( pszIncomingExtension )
	{
		scriptPath.sprintf( "scripts/vscripts/%s", pszScriptName );
	}
	else
	{	
		scriptPath.sprintf( "scripts/vscripts/%s%s", pszScriptName,  pszVMExtension );
	}

	const char *pBase;
	std::string scriptText;

	if ( g_pScriptVM->GetLanguage() == SL_PYTHON )
	{
		// python auto-loads raw or precompiled modules - don't load data here
		pBase = nullptr;
	}
	else
	{
		auto scriptData = filesystem->ReadFile( scriptPath, "GAME" );

		if( !scriptData )
		{
			Log_Warning( LOG_VScript, "Script not found (%s) \n", scriptPath.operator const char *() );
			contract_assert( "Error running script" );
		}

		if ( scriptData )
		{
			scriptText.assign( ks::text_view( *scriptData ) );
		}
		pBase = scriptText.empty() ? nullptr : scriptText.c_str();

		if ( !pBase || !*pBase )
		{
			return nullptr;
		}
	}

	const char *pszFilename = V_strrchr( scriptPath, '/' );
	pszFilename++;
	HSCRIPT hScript = g_pScriptVM->CompileScript( pBase, pszFilename );
	if ( !hScript )
	{
		Log_Warning( LOG_VScript, "FAILED to compile and execute script file named %s\n", scriptPath.operator const char *() );
		contract_assert( "Error running script" );
	}
	return hScript;
}

static int g_ScriptServerRunScriptDepth;

bool VScriptRunScript( const char *pszScriptName, HSCRIPT hScope, bool bWarnMissing )
{
	if ( !g_pScriptVM )
	{
		return false;
	}

	if ( !pszScriptName || !*pszScriptName )
	{
		Log_Warning( LOG_VScript, "Cannot run script: NULL script name\n" );
		return false;
	}

	// Prevent infinite recursion in VM
	if ( g_ScriptServerRunScriptDepth > 16 )
	{
		Log_Warning( LOG_VScript, "IncludeScript stack overflow\n" );
		return false;
	}

	g_ScriptServerRunScriptDepth++;
	HSCRIPT	hScript = VScriptCompileScript( pszScriptName, bWarnMissing );
	bool bSuccess = false;
	if ( hScript )
	{
		if ( gpGlobals->maxClients == 1 )
		{
			CBaseEntity *pPlayer = UTIL_GetLocalPlayer();
			if ( pPlayer )
			{
				g_pScriptVM->SetValue( "player", pPlayer->GetScriptInstance() );
			}
		}
		bSuccess = ( g_pScriptVM->Run( hScript, hScope ) != SCRIPT_ERROR );
		if ( !bSuccess )
		{
			Log_Warning( LOG_VScript, "Error running script named %s\n", pszScriptName );
			contract_assert( "Error running script" );
		}
	}
	g_ScriptServerRunScriptDepth--;
	return bSuccess;
}

static void script( const CCommand &args );
static ConCommand script_command( side_registry(), "script", script, "Run the text as a script" );
static void script( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	if ( !*args[1] )
	{
		Log_Warning( LOG_VScript, "No function name specified\n" );
		return;
	}

	if ( !g_pScriptVM )
	{
		Log_Warning( LOG_VScript, "Scripting disabled or no server running\n" );
		return;
	}

	const char *pszScript = args.GetCommandString();

	pszScript += 6;
	
	while ( *pszScript == ' ' )
	{
		pszScript++;
	}

	if ( !*pszScript )
	{
		return;
	}

	if ( *pszScript != '\"' )
	{
		g_pScriptVM->Run( pszScript );
	}
	else
	{
		pszScript++;
		const char *pszEndQuote = pszScript;
		while ( *pszEndQuote !=  '\"' )
		{
			pszEndQuote++;
		}
		if ( !*pszEndQuote )
		{
			return;
		}
		*((char *)pszEndQuote) = 0;
		g_pScriptVM->Run( pszScript );
		*((char *)pszEndQuote) = '\"';
	}
}

static void script_execute( const CCommand &args );
static ConCommand script_execute_command( side_registry(), "script_execute", script_execute, "Run a vscript file" );
static void script_execute( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	if ( !*args[1] )
	{
		Log_Warning( LOG_VScript, "No script specified\n" );
		return;
	}

	if ( !g_pScriptVM )
	{
		Log_Warning( LOG_VScript, "Scripting disabled or no server running\n" );
		return;
	}

	VScriptRunScript( args[1], true );
}

static void script_debug( const CCommand &args );
static ConCommand script_debug_command( side_registry(), "script_debug", script_debug, "Connect the vscript VM to the script debugger" );
static void script_debug( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	if ( !g_pScriptVM )
	{
		Log_Warning( LOG_VScript, "Scripting disabled or no server running\n" );
		return;
	}
	g_pScriptVM->ConnectDebugger();
}

static void script_help( const CCommand &args );
static ConCommand script_help_command( side_registry(), "script_help", script_help, "Output help for script functions, optionally with a search string" );
static void script_help( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	if ( !g_pScriptVM )
	{
		Log_Warning( LOG_VScript, "Scripting disabled or no server running\n" );
		return;
	}
	const char *pszArg1 = "*";
	if ( *args[1] )
	{
		pszArg1 = args[1];
	}

	g_pScriptVM->Run( CFmtStr( "PrintHelp( \"%s\" );", pszArg1 ) );
}

static void script_dump_all( const CCommand &args );
static ConCommand script_dump_all_command( side_registry(), "script_dump_all", script_dump_all, "Dump the state of the VM to the console" );
static void script_dump_all( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	if ( !g_pScriptVM )
	{
		Log_Warning( LOG_VScript, "Scripting disabled or no server running\n" );
		return;
	}
	g_pScriptVM->DumpState();
}

class CBaseEntity;

using namespace ::ks::reflect;

// Purpose: An effect for a single bullet passing through a body of water.
//			The slug quickly decelerates, leaving a trail of bubbles behind it.
//			TODO: make clientside

static constexpr auto WATERBULLET_INITIAL_SPEED = 1000.0;
static constexpr auto WATERBULLET_STOP_TIME = 0.5; // how long it takes a bullet in water to come to a stop!

inline constexpr auto WATERBULLET_DECAY = ( WATERBULLET_INITIAL_SPEED / WATERBULLET_STOP_TIME );

LINK_ENTITY_TO_CLASS( waterbullet, CWaterBullet );

IMPLEMENT_REFLECT_SERVERCLASS( CWaterBullet, DT_WaterBullet )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWaterBullet::Precache_Impl()
{
	PrecacheModel( "models/weapons/w_bullet.mdl" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWaterBullet::Spawn( const Vector &vecOrigin, const Vector &vecDir )
{
	Precache();

	SetSolid( SOLID_BBOX );
	SetModel( "models/weapons/w_bullet.mdl" );
	UTIL_SetSize( this, vec3_origin, vec3_origin );

	SetMoveType( MOVETYPE_FLY );

	SetGravity( 0.0 );

	QAngle angles;
	SetAbsOrigin( vecOrigin );
	
	SetAbsVelocity( vecDir * 1500.0f );
	VectorAngles( GetAbsVelocity(), angles );
	SetAbsAngles( angles );

	SetCollisionGroup( COLLISION_GROUP_DEBRIS );
	SetTouch( &CWaterBullet::Touch );

	SetThink( &CWaterBullet::BulletThink );
	SetNextThink( gpGlobals->curtime );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CWaterBullet::BulletThink()
{
	//NDebugOverlay::Line( GetAbsOrigin(), GetAbsOrigin() - GetAbsVelocity() * 0.1, 255, 255, 255, false, 1 );
	SetNextThink( gpGlobals->curtime + 0.05 );

/*
	QAngle angles = GetAbsAngles();
	angles.x += random->RandomInt( -6, 6 );
	angles.y += random->RandomInt( -6, 6 );
	SetAbsAngles( angles );
*/

	Vector forward;
	AngleVectors( GetAbsAngles(), &forward );
	SetAbsVelocity( forward * 1500.0f );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWaterBullet::Touch_Impl( CBaseEntity *pOther )
{
	Vector	vecDir = GetAbsVelocity();
	float speed = VectorNormalize( vecDir );

	Vector	vecStart = GetAbsOrigin() - ( vecDir * 8 );
	Vector	vecEnd = GetAbsOrigin() + ( vecDir * speed );

	trace_t	tr;
	UTIL_TraceLine( vecStart, vecEnd, MASK_SHOT, nullptr, &tr );
	UTIL_ImpactTrace( &tr, DMG_BULLET );

	UTIL_Remove( this );
}

class CBaseEntity;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Shadow control entity.

//------------------------------------------------------------------------------
// FIXME: This really should inherit from something	more lightweight
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Purpose : Water LOD control entity
//------------------------------------------------------------------------------
class CWaterLODControl : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_WaterLODControl", .base = false } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CWaterLODControl, CBaseEntity );

	CWaterLODControl();

	void Spawn_Impl( void );
	using BaseClass::KeyValue_Impl;
	bool KeyValue_Impl( const char *szKeyName, const char *szValue );
	int  UpdateTransmitState_Impl();
	void SetCheapWaterStartDistance( inputdata_t &inputdata );
	void SetCheapWaterEndDistance( inputdata_t &inputdata );

	int	ObjectCaps_Impl( void ) { return BaseClass::ObjectCaps_Impl() & ~FCAP_ACROSS_TRANSITION; }

	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

private:
	CNetworkVar( float, m_flCheapWaterStartDistance, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "cheapwaterstartdistance" } ]] [[= ks::reflect::Key{ .name = "SetCheapWaterStartDistance", .input = true } ]] );
	CNetworkVar( float, m_flCheapWaterEndDistance, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "cheapwaterenddistance" } ]] [[= ks::reflect::Key{ .name = "SetCheapWaterEndDistance", .input = true } ]] );
};

LINK_ENTITY_TO_CLASS(water_lod_control, CWaterLODControl);

IMPLEMENT_REFLECT_DATAMAP( CWaterLODControl )

IMPLEMENT_REFLECT_SERVERCLASS( CWaterLODControl, DT_WaterLODControl )

CWaterLODControl::CWaterLODControl()
{
	m_flCheapWaterStartDistance = 1000.0f;
	m_flCheapWaterEndDistance = 2000.0f;
}

//------------------------------------------------------------------------------
// Purpose : Send even though we don't have a model
//------------------------------------------------------------------------------
int CWaterLODControl::UpdateTransmitState_Impl()
{
	// ALWAYS transmit to all clients.
	return SetTransmitState( FL_EDICT_ALWAYS );
}

bool CWaterLODControl::KeyValue_Impl( const char *szKeyName, const char *szValue )
{
	if ( FStrEq( szKeyName, "cheapwaterstartdistance" ) )
	{
		m_flCheapWaterStartDistance = std::atof( szValue );
		return true;
	}

	if ( FStrEq( szKeyName, "cheapwaterenddistance" ) )
	{
		m_flCheapWaterEndDistance = std::atof( szValue );
		return true;
	}

	return BaseClass::KeyValue_Impl( szKeyName, szValue );
}

//------------------------------------------------------------------------------
// Purpose :
//------------------------------------------------------------------------------
void CWaterLODControl::Spawn_Impl( void )
{
	Precache();
	SetSolid( SOLID_NONE );
}

//------------------------------------------------------------------------------
// Input values
//------------------------------------------------------------------------------
void CWaterLODControl::SetCheapWaterStartDistance( inputdata_t &inputdata )
{
	m_flCheapWaterStartDistance = std::atof( inputdata.value.String() );
}

void CWaterLODControl::SetCheapWaterEndDistance( inputdata_t &inputdata )
{
	m_flCheapWaterEndDistance = std::atof( inputdata.value.String() );
}

class CBaseEntity;
class CBasePlayer;

using namespace ::ks::reflect;

// Purpose: Namespace for functions having to do with WC Edit mode

// UNDONE: Reduce some dependency here!

Vector *g_EntityPositions = nullptr;
QAngle *g_EntityOrientations = nullptr;
string_t *g_EntityClassnames = nullptr;

//-----------------------------------------------------------------------------
// Purpose: Saves the entity's position for future communication with Hammer
//-----------------------------------------------------------------------------
void NWCEdit::RememberEntityPosition( CBaseEntity *pEntity )
{
	if ( !(pEntity->ObjectCaps() & FCAP_WCEDIT_POSITION) )
		return;

	if ( !g_EntityPositions )
	{
		g_EntityPositions = new Vector[NUM_ENT_ENTRIES];
		g_EntityOrientations = new QAngle[NUM_ENT_ENTRIES];
		// have to save these too because some entities change the classname on spawn (e.g. prop_physics_override, physics_prop)
		g_EntityClassnames = new string_t[NUM_ENT_ENTRIES];
	}
	int index = pEntity->entindex();
	g_EntityPositions[index] = pEntity->GetAbsOrigin();
	g_EntityOrientations[index] = pEntity->GetAbsAngles();
	g_EntityClassnames[index] = pEntity->m_iClassname;
}

//-----------------------------------------------------------------------------
// Purpose: Sends Hammer an update to the current position
//-----------------------------------------------------------------------------
void NWCEdit::UpdateEntityPosition( CBaseEntity *pEntity )
{
	const Vector &newPos = pEntity->GetAbsOrigin();
	const QAngle &newAng = pEntity->GetAbsAngles();

	DevMsg( 1, "%s\n   origin %f %f %f\n   angles %f %f %f\n", pEntity->GetClassname(), newPos.x, newPos.y, newPos.z, newAng.x, newAng.y, newAng.z );
	if ( Ragdoll_IsPropRagdoll(pEntity) )
	{
		char tmp[2048];
		Ragdoll_GetAngleOverrideString( tmp, sizeof(tmp), pEntity );
		DevMsg( 1, "pose: %s\n", tmp );
	}

	if ( !(pEntity->ObjectCaps() & FCAP_WCEDIT_POSITION) )
		return;
	
	// can't do this unless in edit mode
	if ( !engine->IsInEditMode() )
		return;

	int entIndex = pEntity->entindex();
	Vector pos = g_EntityPositions[entIndex];
	EditorSendResult_t result = Editor_BadCommand;
	const char *pClassname = STRING(g_EntityClassnames[entIndex]);

	if ( pEntity->GetModel() && modelinfo->GetModelType(pEntity->GetModel()) == mod_brush )
	{
		QAngle xformAngles;
		RotationDelta( g_EntityOrientations[entIndex], newAng, &xformAngles );
		if ( xformAngles.Length() > 1e-4 )
		{
			result = Editor_RotateEntity( pClassname, pos.x, pos.y, pos.z, xformAngles );
		}
		else
		{
			// don't call through for an identity rotation, may just increase error
			result = Editor_OK;
		}
	}
	else
	{
		if ( Ragdoll_IsPropRagdoll(pEntity) )
		{
			char tmp[2048];
			Ragdoll_GetAngleOverrideString( tmp, sizeof(tmp), pEntity );
			result = Editor_SetKeyValue( pClassname, pos.x, pos.y, pos.z, "angleOverride", tmp );
			if ( result != Editor_OK )
				goto error;
		}
		result = Editor_SetKeyValue( pClassname, pos.x, pos.y, pos.z, "angles", CFmtStr("%f %f %f", newAng.x, newAng.y, newAng.z) );
	}
	if ( result != Editor_OK )
		goto error;

	result = Editor_SetKeyValue( pClassname, pos.x, pos.y, pos.z, "origin", CFmtStr("%f %f %f", newPos.x, newPos.y, newPos.z) );
	if ( result != Editor_OK )
		goto error;

	NDebugOverlay::EntityBounds(pEntity, 0, 255, 0, 0 ,5);
	// save the update
	RememberEntityPosition( pEntity );
	return;

error:
	NDebugOverlay::EntityBounds(pEntity, 255, 0, 0, 0 ,5);
}

/// This is an entity used by the hammer_update_safe_entities command. It allows designers
/// to specify objects that should be ignored. It stores an array of sixteen strings
/// which may correspond to names. Designers may ignore more than sixteen objects by 
/// placing more than one of these in a level.
class CWC_UpdateIgnoreList : public CBaseEntity
{
public:
	DECLARE_CLASS( CWC_UpdateIgnoreList, CBaseEntity );

	enum { MAX_IGNORELIST_NAMES = 16 }; ///< the number of names in the array below

	inline const string_t &GetName( int x ) const { return m_nIgnoredEntityNames[x]; } 

protected:
	// the list of names to ignore
	[[= ks::reflect::Key{ .name = "IgnoredName16", .index = 15 } ]] [[= ks::reflect::Key{ .name = "IgnoredName15", .index = 14 } ]] [[= ks::reflect::Key{ .name = "IgnoredName14", .index = 13 } ]] [[= ks::reflect::Key{ .name = "IgnoredName13", .index = 12 } ]] [[= ks::reflect::Key{ .name = "IgnoredName12", .index = 11 } ]] [[= ks::reflect::Key{ .name = "IgnoredName11", .index = 10 } ]] [[= ks::reflect::Key{ .name = "IgnoredName10", .index = 9 } ]] [[= ks::reflect::Key{ .name = "IgnoredName09", .index = 8 } ]] [[= ks::reflect::Key{ .name = "IgnoredName08", .index = 7 } ]] [[= ks::reflect::Key{ .name = "IgnoredName07", .index = 6 } ]] [[= ks::reflect::Key{ .name = "IgnoredName06", .index = 5 } ]] [[= ks::reflect::Key{ .name = "IgnoredName05", .index = 4 } ]] [[= ks::reflect::Key{ .name = "IgnoredName04", .index = 3 } ]] [[= ks::reflect::Key{ .name = "IgnoredName03", .index = 2 } ]] [[= ks::reflect::Key{ .name = "IgnoredName02", .index = 1 } ]] [[= ks::reflect::Key{ .name = "IgnoredName01", .index = 0 } ]] string_t m_nIgnoredEntityNames[MAX_IGNORELIST_NAMES]; 

public:
	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( hammer_updateignorelist, CWC_UpdateIgnoreList );

IMPLEMENT_REFLECT_DATAMAP( CWC_UpdateIgnoreList )

static void hammer_update_entity( const CCommand &args );
static ConCommand hammer_update_entity_command( side_registry(), "hammer_update_entity", hammer_update_entity, "Updates the entity's position/angles when in edit mode" );
static void hammer_update_entity( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	if ( args.ArgC() < 2 )
	{
		CBasePlayer *pPlayer = UTIL_GetCommandClient();
		trace_t tr;
		Vector forward;
		pPlayer->EyeVectors( &forward );
		UTIL_TraceLine(pPlayer->EyePosition(), pPlayer->EyePosition() + forward * MAX_COORD_RANGE,
			MASK_SHOT_HULL|CONTENTS_GRATE|CONTENTS_DEBRIS, pPlayer, COLLISION_GROUP_NONE, &tr );

		if ( tr.DidHit() && !TraceDidHitWorld( tr ) )
		{
			NWCEdit::UpdateEntityPosition( tr.Ent<CBaseEntity>() );
		}
	}
	else
	{
		CBaseEntity *pEnt = nullptr;
		while ((pEnt = gEntList.FindEntityGeneric( pEnt, args[1] ) ) != nullptr)
		{
			NWCEdit::UpdateEntityPosition( pEnt );
		}
	}
}

static void hammer_update_safe_entities( const CCommand &args );
static ConCommand hammer_update_safe_entities_command( side_registry(), "hammer_update_safe_entities", hammer_update_safe_entities, "Updates entities in the map that can safely be updated (don't have parents or are affected by constraints). Also excludes entities mentioned in any hammer_updateignorelist objects in this map." );
static void hammer_update_safe_entities( const CCommand &args )
{
	int iCount = 0;
	CBaseEntity *pEnt = nullptr;

	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	Msg("\n====================================================\nPerforming Safe Entity Update\n" );

	// first look for any exclusion objects -- these are entities that list specific things to be ignored.
	// All the names that are inside them, we store into a hash table (here implemented through a local
	// intern_pool)

	ks::intern_pool ignoredNames( true ); // Case insensitive.
	while ( (pEnt = gEntList.FindEntityByClassname( pEnt, "hammer_updateignorelist" )) != nullptr )
	{
		// for each name in each of those strings, add it to the symbol table.
		CWC_UpdateIgnoreList *piglist = static_cast<CWC_UpdateIgnoreList *>(pEnt);
		for (int ii = 0 ; ii < CWC_UpdateIgnoreList::MAX_IGNORELIST_NAMES ; ++ii )
		{
			if (!!piglist->GetName(ii))  // if not null
			{	// add to symtab
				ignoredNames.intern(piglist->GetName(ii).ToCStr());
			}
		}
	}

	if ( ignoredNames.size() > 0 )
	{
		Msg( "Ignoring %d specified targetnames.\n", (int)ignoredNames.size() );
	}

	// now iterate through everything in the world
	for ( pEnt = gEntList.FirstEnt(); pEnt != nullptr; pEnt = gEntList.NextEnt(pEnt) )
	{
		if ( !(pEnt->ObjectCaps() & FCAP_WCEDIT_POSITION) )
			continue;

		// If we have a parent, or any children, we're not safe to update
		if ( pEnt->GetMoveParent() || pEnt->FirstMoveChild() )
			continue;

		IPhysicsObject *pPhysics = pEnt->VPhysicsGetObject();
		if ( !pPhysics )
			continue;
		// If we are affected by any constraints, we're not safe to update
		if ( pPhysics->IsAttachedToConstraint( Ragdoll_IsPropRagdoll(pEnt) )  )
			continue;
		// Motion disabled?
		if ( !pPhysics->IsMoveable() )
			continue;

		// ignore brush models (per bug 61318)
		if ( dynamic_cast<CPhysBox *>(pEnt) )
			continue;

		// explicitly excluded by designer?
		if ( ignoredNames.contains(pEnt->GetEntityName().ToCStr()) )
			continue;

		NWCEdit::UpdateEntityPosition( pEnt );
		iCount++;
	}

	Msg("Updated %d entities.\n", iCount);
}

class CEconItemDefinition;
class CEconItemView;

using namespace ::ks::reflect;

// Purpose: Weapon data file parsing, shared by game & client dlls.

// Static default item definition for fallback (never returns NULL for backwards compatibility)
static CEconItemDefinition s_DefaultItemDef;

CEconItemDefinition *CEconItemView::GetItemDefinition() const
{
	return m_pItemDef ? m_pItemDef : &s_DefaultItemDef;
}

CEconItemDefinition *CEconItemView::GetStaticData() const
{
	return m_pItemDef ? m_pItemDef : &s_DefaultItemDef;
}

CWeaponDatabase g_WeaponDatabase;

extern void LoadEquipmentData();

// The sound categories found in the weapon classname.txt files
// This needs to match the WeaponSound_t enum in weapon_parse.h
const char *pWeaponSoundCategories[ NUM_SHOOT_SOUND_TYPES ] = 
{
	"empty",
	"single_shot",
	"single_shot_accurate",
	"single_shot_npc",
	"double_shot",
	"double_shot_npc",
	"burst",
	"reload",
	"reload_npc",
	"melee_miss",
	"melee_hit",
	"melee_hit_world",
	"special1",
	"special2",
	"special3",
	"taunt",
	"nearlyempty",
	"fastreload"
};

int GetWeaponSoundFromString( const char *pszString )
{
	for ( int i = EMPTY; i < NUM_SHOOT_SOUND_TYPES; i++ )
	{
		if ( !V_stricmp(pszString,pWeaponSoundCategories[i]) )
			return (WeaponSound_t)i;
	}
	return -1;
}

// Item flags that we parse out of the file.
typedef struct
{
	const char *m_pFlagName;
	int m_iFlagValue;
} itemFlags_t;
itemFlags_t g_ItemFlags[8] =
{
	{ "ITEM_FLAG_SELECTONEMPTY",	ITEM_FLAG_SELECTONEMPTY },
	{ "ITEM_FLAG_NOAUTORELOAD",		ITEM_FLAG_NOAUTORELOAD },
	{ "ITEM_FLAG_NOAUTOSWITCHEMPTY", ITEM_FLAG_NOAUTOSWITCHEMPTY },
	{ "ITEM_FLAG_LIMITINWORLD",		ITEM_FLAG_LIMITINWORLD },
	{ "ITEM_FLAG_EXHAUSTIBLE",		ITEM_FLAG_EXHAUSTIBLE },
	{ "ITEM_FLAG_DOHITLOCATIONDMG", ITEM_FLAG_DOHITLOCATIONDMG },
	{ "ITEM_FLAG_NOAMMOPICKUPS",	ITEM_FLAG_NOAMMOPICKUPS },
	{ "ITEM_FLAG_NOITEMPICKUP",		ITEM_FLAG_NOITEMPICKUP }
};

// Find a weapon slot, assuming the weapon's data has already been loaded.
WEAPON_FILE_INFO_HANDLE LookupWeaponInfoSlot( const char *name )
{
	return g_WeaponDatabase.FindWeaponInfo( name );
}

FileWeaponInfo_t *GetFileWeaponInfoFromHandle( WEAPON_FILE_INFO_HANDLE handle )
{
	return g_WeaponDatabase.GetFileWeaponInfoFromHandle( handle );
}

void PrecacheFileWeaponInfoDatabase()
{
	g_WeaponDatabase.PrecacheAllWeapons();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : WEAPON_FILE_INFO_HANDLE
//-----------------------------------------------------------------------------
WEAPON_FILE_INFO_HANDLE GetInvalidWeaponInfoHandle( void )
{
	return (WEAPON_FILE_INFO_HANDLE)-1;
}

KeyValues* ReadEncryptedKVFile( IFileSystem *filesystem, const char *szFilenameWithoutExtension, const unsigned char *pICEKey, bool bForceReadEncryptedFile )
{
	contract_assert( std::strchr( szFilenameWithoutExtension, '.' ) == nullptr );
	char szFullName[512];

	const char *pSearchPath = "GAME";

	// Open the weapon data file, and abort if we can't
	KeyValues *pKV = new KeyValues( "WeaponDatafile" );
	pKV->UsesEscapeSequences( true );

	V_snprintf(szFullName,sizeof(szFullName), "%s.txt", szFilenameWithoutExtension);

	if ( bForceReadEncryptedFile || !pKV->LoadFromFile( filesystem, szFullName, pSearchPath ) ) // try to load the normal .txt file first
	{
		if ( pICEKey )
		{
			V_snprintf(szFullName,sizeof(szFullName), "%s.ctx", szFilenameWithoutExtension); // fall back to the .ctx file

			FileHandle_t f = filesystem->Open( szFullName, "rb", pSearchPath );

			if (!f)
			{
				pKV->deleteThis();
				return nullptr;
			}
			// load file into a null-terminated buffer
			int fileSize = filesystem->Size(f);
			char *buffer = (char*)MemAllocScratch(fileSize + 1);
		
			contract_assert( buffer );
		
			filesystem->Read(buffer, fileSize, f); // read into local buffer
			buffer[fileSize] = 0; // null terminate file as EOF
			filesystem->Close( f );	// close file after reading

			UTIL_DecodeICE( (unsigned char*)buffer, fileSize, pICEKey );

			bool retOK = pKV->LoadFromBuffer( szFullName, buffer, filesystem );

			MemFreeScratch();

			if ( !retOK )
			{
				pKV->deleteThis();
				return nullptr;
			}
		}
		else
		{
			pKV->deleteThis();
			return nullptr;
		}
	}

	return pKV;
}

std::vector< WeaponInfoLookup* > FileWeaponInfo_t::ms_vecWeaponInfoLookup;

bool							FileWeaponInfo_t::ms_bWeaponInfoLookupInitialized;

WeaponInfoLookup::WeaponInfoLookup( size_t offset, _fieldtypes p_fieldType, const char *szAttribClassName )
{
	m_nWeaponParseDataOffset = offset;
	m_fieldType = p_fieldType;
	m_iszAttribClassName.Set( szAttribClassName );
}

WeaponInfoLookup::WeaponInfoLookup( const WeaponInfoLookup &WepInfoLookup )
{
	m_nWeaponParseDataOffset = WepInfoLookup.m_nWeaponParseDataOffset;
	m_fieldType = WepInfoLookup.m_fieldType;
	m_iszAttribClassName.Set( STRING( WepInfoLookup.m_iszAttribClassName.Get() ) );
}

//-----------------------------------------------------------------------------
// FileWeaponInfo_t implementation.
//-----------------------------------------------------------------------------

FileWeaponInfo_t::FileWeaponInfo_t()
{
	bParsedScript = false;
	bLoadedHudElements = false;
	szClassName[0] = 0;
	szPrintName[0] = 0;

	szViewModel[0] = 0;
	szWorldModel[0] = 0;
	szAnimationPrefix[0] = 0;
	iSlot = 0;
	iPosition = 0;
	iMaxClip1 = 0;
	iMaxClip2 = 0;
	iDefaultClip1 = 0;
	iDefaultClip2 = 0;
	iWeight = 0;
	iRumbleEffect = -1;
	bAutoSwitchTo = false;
	bAutoSwitchFrom = false;
	iFlags = 0;
	szAmmo1[0] = 0;
	szAmmo2[0] = 0;
	szAIAddOn[0] = 0;
	std::memset( aShootSounds, 0, sizeof( aShootSounds ) );
	iAmmoType = 0;
	iAmmo2Type = 0;
	m_bMeleeWeapon = false;
	iSpriteCount = 0;
	iconActive = 0;
	iconInactive = 0;
	iconAmmo = 0;
	iconAmmo2 = 0;
	iconCrosshair = 0;
	iconAutoaim = 0;
	iconZoomedCrosshair = 0;
	iconZoomedAutoaim = 0;
	bShowUsageHint = false;
	m_bAllowFlipping = true;
	m_bBuiltRightHanded = true;

	if ( !ms_bWeaponInfoLookupInitialized )
	{
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( FileWeaponInfo_t, iMaxClip1 ), FIELD_INTEGER, "primary clip size" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( FileWeaponInfo_t, iMaxClip2 ), FIELD_INTEGER, "secondary clip size" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( FileWeaponInfo_t, iDefaultClip1 ), FIELD_INTEGER, "primary default clip size" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( FileWeaponInfo_t, iDefaultClip2 ), FIELD_INTEGER, "secondary default clip size" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( FileWeaponInfo_t, iSlot ), FIELD_INTEGER, "bucket slot" ) );

		std::ranges::stable_sort( ms_vecWeaponInfoLookup, CWeaponInfoLookupListLess() );
		ms_bWeaponInfoLookupInitialized = true;
	}
}

void FileWeaponInfo_t::Parse( KeyValues *pKeyValuesData, const char *szWeaponName )
{
	// Okay, we tried at least once to look this up...
	bParsedScript = true;

	// Classname
	V_strncpy( szClassName, szWeaponName, MAX_WEAPON_STRING );
	// Printable name
	V_strncpy( szPrintName, pKeyValuesData->GetString( "printname", WEAPON_PRINTNAME_MISSING ), MAX_WEAPON_STRING );
	// View model & world model
	V_strncpy( szViewModel, pKeyValuesData->GetString( "viewmodel" ), MAX_WEAPON_STRING );
	V_strncpy( szWorldModel, pKeyValuesData->GetString( "playermodel" ), MAX_WEAPON_STRING );

	V_StripExtension( szWorldModel, szWorldDroppedModel, sizeof( szWorldDroppedModel ) );
	V_strcat_safe( szWorldDroppedModel, "_dropped.mdl" );
	
	V_strncpy( szAnimationPrefix, pKeyValuesData->GetString( "anim_prefix" ), MAX_WEAPON_PREFIX );
	iSlot = pKeyValuesData->GetInt( "bucket", 0 );
	iPosition = pKeyValuesData->GetInt( "bucket_position", 0 );
	
	// Use the console (X360) buckets if hud_fastswitch is set to 2.
	iMaxClip1 = pKeyValuesData->GetInt( "clip_size", WEAPON_NOCLIP );					// Max primary clips gun can hold (assume they don't use clips by default)
	iMaxClip2 = pKeyValuesData->GetInt( "clip2_size", WEAPON_NOCLIP );					// Max secondary clips gun can hold (assume they don't use clips by default)
	iDefaultClip1 = pKeyValuesData->GetInt( "default_clip", iMaxClip1 );		// amount of primary ammo placed in the primary clip when it's picked up
	iDefaultClip2 = pKeyValuesData->GetInt( "default_clip2", iMaxClip2 );		// amount of secondary ammo placed in the secondary clip when it's picked up
	iWeight = pKeyValuesData->GetInt( "weight", 0 );

	iRumbleEffect = pKeyValuesData->GetInt( "rumble", -1 );
	
	// LAME old way to specify item flags.
	// Weapon scripts should use the flag names.
	iFlags = pKeyValuesData->GetInt( "item_flags", ITEM_FLAG_LIMITINWORLD );

	for ( int i=0; i < std::size( g_ItemFlags ); i++ )
	{
		int iVal = pKeyValuesData->GetInt( g_ItemFlags[i].m_pFlagName, -1 );
		if ( iVal == 0 )
		{
			iFlags &= ~g_ItemFlags[i].m_iFlagValue;
		}
		else if ( iVal == 1 )
		{
			iFlags |= g_ItemFlags[i].m_iFlagValue;
		}
	}

	bShowUsageHint = pKeyValuesData->GetBool( "showusagehint", false );
	bAutoSwitchTo = pKeyValuesData->GetBool( "autoswitchto", true );
	bAutoSwitchFrom = pKeyValuesData->GetBool( "autoswitchfrom", true );
	m_bBuiltRightHanded = pKeyValuesData->GetBool( "BuiltRightHanded", true );
	m_bAllowFlipping = pKeyValuesData->GetBool( "AllowFlipping", true );
	m_bMeleeWeapon = pKeyValuesData->GetBool( "MeleeWeapon", false );

	// Primary ammo used
	const char *pAmmo = pKeyValuesData->GetString( "primary_ammo", "None" );
	if ( std::strcmp("None", pAmmo) == 0 )
		V_strncpy( szAmmo1, "", sizeof( szAmmo1 ) );
	else
		V_strncpy( szAmmo1, pAmmo, sizeof( szAmmo1 )  );
	iAmmoType = GetAmmoDef()->Index( szAmmo1 );
	
	// Secondary ammo used
	pAmmo = pKeyValuesData->GetString( "secondary_ammo", "None" );
	if ( std::strcmp("None", pAmmo) == 0)
		V_strncpy( szAmmo2, "", sizeof( szAmmo2 ) );
	else
		V_strncpy( szAmmo2, pAmmo, sizeof( szAmmo2 )  );
	iAmmo2Type = GetAmmoDef()->Index( szAmmo2 );

	// AI AddOn
	const char *pAIAddOn = pKeyValuesData->GetString( "ai_addon", "ai_addon_basecombatweapon" );
	if ( std::strcmp("None", pAIAddOn) == 0)
		V_strncpy( szAIAddOn, "", sizeof( szAIAddOn ) );
	else
		V_strncpy( szAIAddOn, pAIAddOn, sizeof( szAIAddOn )  );

	// Now read the weapon sounds
	std::memset( aShootSounds, 0, sizeof( aShootSounds ) );
	KeyValues *pSoundData = pKeyValuesData->FindKey( "SoundData" );
	if ( pSoundData )
	{
		for ( int i = EMPTY; i < NUM_SHOOT_SOUND_TYPES; i++ )
		{
			const char *soundname = pSoundData->GetString( pWeaponSoundCategories[i] );
			if ( soundname && soundname[0] )
			{
				V_strncpy( aShootSounds[i], soundname, MAX_WEAPON_STRING );
			}
			else
			{
				// FIXME: find a better way to populate defaults
			
				if ( i == NEARLYEMPTY ) // explicit check because it looks like most weapon sound categories don't fall back by default
				{
					V_sprintf_safe( aShootSounds[i], "Default.%s", pWeaponSoundCategories[i] );
				}
			}
		}
	}
}

const char* FileWeaponInfo_t::GetWorldModel( const CEconItemView* pWepView, int iTeam ) const
{
	if ( pWepView && pWepView->IsValid() )
	{
		const char *pchWorldOverride = pWepView->GetStaticData()->GetEntityOverrideModel();
		if ( pchWorldOverride )
		{
			return pchWorldOverride;
		}

		return pWepView->GetItemDefinition()->GetWorldDisplayModel();
	}
	else
	{
		return szWorldModel;
	}
}

const char* FileWeaponInfo_t::GetViewModel( const CEconItemView* pWepView, int iTeam ) const
{
	if ( pWepView && pWepView->IsValid() )
	{
		const char *pchViewOverride = pWepView->GetStaticData()->GetViewOverrideModel();
		if ( pchViewOverride )
		{
			return pchViewOverride;
		}

		return pWepView->GetItemDefinition()->GetBasePlayerDisplayModel();
	}
	else
	{
		return szViewModel;
	}
}

const char* FileWeaponInfo_t::GetWorldDroppedModel( const CEconItemView* pWepView, int iTeam ) const
{
	if ( pWepView && pWepView->IsValid() )
	{
		const char *pchWorldDroppedModel = pWepView->GetItemDefinition()->GetWorldDroppedModel();
		if ( pchWorldDroppedModel )
		{
			return pchWorldDroppedModel;
		}
	}
	
	return szWorldDroppedModel;
}

const char* FileWeaponInfo_t::GetPrimaryAmmo( const CEconItemView* pWepView ) const
{

	if ( pWepView && pWepView->IsValid() )
	{
		// TODO: replace visual data with attributes when attributes support strings.
		const char *pszString = pWepView->GetStaticData()->GetPrimaryAmmo();

		if ( pszString )
		{
			return pszString;
		}
	}

	return szAmmo1;
}

int FileWeaponInfo_t::GetPrimaryAmmoType( const CEconItemView* pWepView ) const
{

	if ( pWepView && pWepView->IsValid() )
	{
		// TODO: replace visual data with attributes when attributes support strings.
		const char *pszString = GetPrimaryAmmo( pWepView );

		if ( pszString )
		{
			return GetAmmoDef()->Index( pszString );
		}
	}

	return iAmmoType;
}

static int WeaponInfoLookupCompare( WeaponInfoLookup * const * src1, WeaponInfoLookup * const * src2 )
{
	if ( ( *src1 )->m_iszAttribClassName.Get() == ( *src2 )->m_iszAttribClassName.Get() )
		return 0;

	if ( ( *src1 )->m_iszAttribClassName.Get() < ( *src2 )->m_iszAttribClassName.Get() )
		return -1;

	return 1;
}

// Convert a schema attribute classname into an index of g_WeaponInfoTable
int FileWeaponInfo_t::GetIndexofAttribute( string_t iszAttribClassName ) const
{
	if ( ms_vecWeaponInfoLookup.empty() )
		return -1;

	if ( ms_vecWeaponInfoLookup[ 0 ]->m_iszAttribClassName.IsSerialNumberOutOfDate() )
	{
		// If the strings are out of date, rebuild and resort them
		for ( WeaponInfoLookup *pWeaponInfoLookup : ms_vecWeaponInfoLookup )
		{
			pWeaponInfoLookup->m_iszAttribClassName.Get();
		}

		ks::qsort( ms_vecWeaponInfoLookup, &WeaponInfoLookupCompare );
	}

	WeaponInfoLookup searchKey;
	searchKey.m_iszAttribClassName.SetFastNoCopy( iszAttribClassName );
	auto it = std::ranges::lower_bound( ms_vecWeaponInfoLookup, &searchKey, CWeaponInfoLookupListLess() );
	if ( it == ms_vecWeaponInfoLookup.end() || CWeaponInfoLookupListLess()( &searchKey, *it ) )
		return -1;
	return (int)( it - ms_vecWeaponInfoLookup.begin() );
}

static void weapon_reload_database( const CCommand &args );
static ConCommand weapon_reload_database_command( side_registry(), "weapon_reload_database", weapon_reload_database, "Reload the weapon database", FCVAR_CHEAT | FCVAR_GAMEDLL| FCVAR_SERVER_CAN_EXECUTE );
static void weapon_reload_database( const CCommand &args )
{
	g_WeaponDatabase.LoadManifest();
	IGameEvent *event = gameeventmanager->CreateEvent( "weapon_reload_database" );
	if ( event )
	{
		gameeventmanager->FireEventClientSide( event );
	}
}

CWeaponDatabase::CWeaponDatabase() : m_bPreCached( false )
{
}

bool CWeaponDatabase::Init()
{
	ListenForGameEvent( "weapon_reload_database" );
	return LoadManifest();
}

bool CWeaponDatabase::LoadManifest()
{
	// FIXME[pmf]: this is defined as a virtual in g_pGameRules, but it isn't construction yet...
	const unsigned char szEncryptionKey[] = "d7NSuLq2";

	// KeyValues::AutoDelete manifest = ReadEncryptedKVFile( filesystem, "scripts/weapon_manifest.txt", szEncryptionKey );
	KeyValues::AutoDelete manifest( "weaponscripts" );
	manifest->UsesEscapeSequences( true );
	if ( manifest->LoadFromFile( filesystem, "scripts/weapon_manifest.txt", "GAME" ) )
	{
		for ( KeyValues *sub = manifest->GetFirstSubKey(); sub != nullptr ; sub = sub->GetNextKey() )
		{
			if ( !V_stricmp( sub->GetName(), "file" ) )
			{
				char fileBase[512];
				V_FileBase( sub->GetString(), fileBase, sizeof(fileBase) );

				LoadWeaponDataFromFile( filesystem, fileBase, szEncryptionKey );
			}
			else
			{
				Error( "Expecting 'file', got %s\n", sub->GetName() );
			}
		}
	}

	LoadEquipmentData();

	return true;
}

void CWeaponDatabase::PrecacheAllWeapons()
{
// NOTE[pmf]: removed this precache check for now, since it seems that the caches are getting evicted on map change
// 	if ( m_bPreCached )
// 		return;

	for ( const auto &[name, handle] : m_WeaponInfoDatabase )
	{
		const char* szClassName = name.c_str();

		UTIL_PrecacheOther( szClassName );
	}
	m_bPreCached = true;
}

void CWeaponDatabase::RefreshAllWeapons()
{
	for ( const auto &[name, handle] : m_WeaponInfoDatabase )
	{
		FileWeaponInfo_t* pWeaponInfo = m_WeaponInfos[handle].get();
		if ( pWeaponInfo )
		{
			pWeaponInfo->RefreshDynamicParameters();
		}
	}
}

WEAPON_FILE_INFO_HANDLE CWeaponDatabase::FindWeaponInfo( const char *name )
{
	auto it = m_WeaponInfoDatabase.find( name );
	return it != m_WeaponInfoDatabase.end() ? it->second : GetInvalidWeaponInfoHandle();
}

WEAPON_FILE_INFO_HANDLE CWeaponDatabase::FindOrCreateWeaponInfo( const char *name )
{
	// Complain about duplicately defined metaclass names...
	auto it = m_WeaponInfoDatabase.find( name );
	if ( it != m_WeaponInfoDatabase.end() )
	{
		return it->second;
	}

	WEAPON_FILE_INFO_HANDLE lookup = (WEAPON_FILE_INFO_HANDLE)m_WeaponInfos.size();
	contract_assert( lookup != GetInvalidWeaponInfoHandle() );
	m_WeaponInfos.emplace_back( CreateWeaponInfo() );
	m_WeaponInfoDatabase.emplace( name, lookup );
	return lookup;
}

static FileWeaponInfo_t * Helper_GetNullWeaponInfo()
{
	static FileWeaponInfo_t gNullWeaponInfo;
	return &gNullWeaponInfo;
}

FileWeaponInfo_t * CWeaponDatabase::GetFileWeaponInfoFromHandle( WEAPON_FILE_INFO_HANDLE handle )
{
	if ( handle >= m_WeaponInfos.size() )
	{
		return Helper_GetNullWeaponInfo();
	}

	return m_WeaponInfos[ handle ].get();
}

//-----------------------------------------------------------------------------
// Purpose: Read data on weapon from script file
// Output:  true  - if data2 successfully read
//			false - if data load fails
//-----------------------------------------------------------------------------
bool CWeaponDatabase::LoadWeaponDataFromFile( IFileSystem* filesystem, const char *szWeaponName, const unsigned char *pICEKey )
{
	char sz[128];
	V_snprintf( sz, sizeof( sz ), "scripts/%s", szWeaponName );
	KeyValues *pKV = ReadEncryptedKVFile( filesystem, sz, pICEKey );
	if ( !pKV )
		return false;

	WEAPON_FILE_INFO_HANDLE handle = FindOrCreateWeaponInfo( szWeaponName );
	FileWeaponInfo_t *pFileInfo = GetFileWeaponInfoFromHandle( handle );
	contract_assert( pFileInfo );

	pFileInfo->Parse( pKV, szWeaponName );

	pKV->deleteThis();

	return true;
}

void CWeaponDatabase::Reset()
{
	m_WeaponInfoDatabase.clear();
	m_WeaponInfos.clear();

	m_bPreCached = false;
}

void CWeaponDatabase::FireGameEvent( IGameEvent *pEvent )
{
}

static const char *g_ProficiencyNames[] = 
{
	"Poor",
	"Average",
	"Good",
	"Very Good",
	"Perfect"
};

const char *GetWeaponProficiencyName( WeaponProficiency_t proficiency )
{
	static_assert( std::size( g_ProficiencyNames ) == WEAPON_PROFICIENCY_PERFECT + 1 ); // Hey, update that there table!

	if ( proficiency < 0 || proficiency > WEAPON_PROFICIENCY_PERFECT )
		return "<<Invalid>>";
	return g_ProficiencyNames[proficiency];
}

class CBaseEntity;
struct vphysics_objectstress_t;

using namespace ::ks::reflect;

// Purpose: 'Button' which activates after a specified amount of weight is touching it.

class CWeightButton : public CBaseEntity
{
public:

	DECLARE_DATADESC();
	DECLARE_CLASS( CWeightButton, CBaseEntity ); 

	void Spawn_Impl( void );
	bool CreateVPhysics_Impl( void );

	[[= ks::reflect::Key{ .name = "OnPressed" } ]] COutputEvent m_OnPressed;				// After threshold weight has been reached
	[[= ks::reflect::Key{ .name = "OnReleased" } ]] COutputEvent m_OnReleased;				// After weight has been removed to go below weight threshold

	[[= ks::reflect::Key{ .name = "WeightToActivate" } ]] float m_fStressToActivate;				// Amount of weight required to activate
	bool m_bHasBeenPressed;					// Once the button has been pressed, fire one 
											// output until the weight is reduced below the threshold

	void TriggerThink ( void );

};

LINK_ENTITY_TO_CLASS( func_weight_button, CWeightButton );

IMPLEMENT_REFLECT_DATAMAP( CWeightButton )

void CWeightButton::Spawn_Impl()
{
	BaseClass::Spawn_Impl();

	// Convert movedir from angles to a vector
	SetMoveType( MOVETYPE_VPHYSICS );
	SetSolid( SOLID_VPHYSICS );
 	SetModel( STRING( GetModelName() ) );
	CreateVPhysics();
	SetThink( &CWeightButton::TriggerThink );
	SetNextThink( gpGlobals->curtime + TICK_INTERVAL );
	m_bHasBeenPressed = false;
}

//-----------------------------------------------------------------------------
// Purpose: Create VPhysics collision for this entity
//-----------------------------------------------------------------------------
bool CWeightButton::CreateVPhysics_Impl()
{
	VPhysicsInitShadow( false, false );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Every second, check total stress and fire an output if we have reached 
//			our threshold. If the stress is relieved below our threshold, fire a different output.
//-----------------------------------------------------------------------------
void CWeightButton::TriggerThink( void )
{
	vphysics_objectstress_t vpobj_StressOut;
	IPhysicsObject* pMyPhysics = VPhysicsGetObject();

	if ( !pMyPhysics )
	{
		SetNextThink( TICK_NEVER_THINK );
		return;
	}

 	float fStress = CalculateObjectStress( pMyPhysics, this, &vpobj_StressOut );

//	fStress = vpobj_StressOut.receivedStress;

	if ( fStress > m_fStressToActivate && !m_bHasBeenPressed )
	{
		m_OnPressed.FireOutput( this, this );
		m_bHasBeenPressed = true;
	}
	else if ( fStress < m_fStressToActivate && m_bHasBeenPressed )
	{
		m_OnReleased.FireOutput( this, this );
		m_bHasBeenPressed = false;
	}

	// think every tick
	SetNextThink( gpGlobals->curtime + TICK_INTERVAL );
}
