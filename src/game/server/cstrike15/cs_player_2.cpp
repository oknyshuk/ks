// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

module;
#include <math.h>
#include "eiface.h"
#include "networkvar.h"
#include "responserules/response_types.h"
#include "server_class.h"
#include "studio.h"
#include "vphysics_interface.h"
#include "../ai_criteria.h"
#include "../entity_dispatch.h"
#include "../entity_dispatch_list.h"
#include "../entity_kinds_dump.h"
#include "../reflect_sendtable.h"
#include "../util.h"
#include "igameevents.h"
#include "platforminputdevice.h"
#include "reflect.h"
#include "usermessages_schema.h"
#include "../reflect_datamap.h"
module ks.server;

import std;
import ks.core.basetypes;
import ks.core.random;
import ks.game.cstrike15.cs_player_rank_shared;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.reflect_wirevar;
import ks.tier1.registry;
import ks.tier1.vector_ops;
import :fish;
import :vscript;
import :sharedvar;
import :baseentity;
import :player;
import :cstrike15.bot.bot;
import :cstrike15.bot.cs_bot;
import :cstrike15.cs_player;
import :cstrike15.cs_playeranimstate;
import :cstrike15.cs_team;
import :cstrike15.hostage.cs_simple_hostage;
import :entity_dispatch;
import :entity_kind;
import :nav_4;
import :entity_2;
import ks.game.shareddefs;
import :cstrike15.cs_player_2;
import :econ.econ_item_schema_minimal;
import :entityutil;
import ks.game.gamemovement;
import ks.game.mapentities_shared;
import ks.core.types;
import ks.core.vprof;
import ks.game.cstrike15.cs_achievementdefs;
import ks.math.ssemath;
import ks.math.vector2d;
import ks.math.vmatrix;
import ks.tier1.cvar;
import ks.tier1.exprevaluator;
import ks.tier1.iconvar;
import ks.tier1.icvar;
import ks.tier1.small_vector;
import ks.tier1.strtools;
import ks.game.ai_activity;

class BotProfile;
class CBaseEntity;
class CCSBot;
class CCSPlayer;
class CEconPersonaDataPublic;
class CEconQuestDefinition;
class CHostage;
class CNavArea;

using namespace ::ks::reflect;

// Purpose: CS's custom CPlayerResource

extern bool	g_fGameOver;

// Datatable
IMPLEMENT_REFLECT_SERVERCLASS( CCSPlayerResource, DT_CSPlayerResource )

LINK_ENTITY_TO_CLASS( cs_player_manager, CCSPlayerResource );

CCSPlayerResource::CCSPlayerResource( void )
{
	m_bEndMatchNextMapAllVoted = false;
	m_bPreferencesAssigned_T = false;
	m_bPreferencesAssigned_CT = false;
	std::memset( m_nAttemptedToGetColor, false, sizeof( m_nAttemptedToGetColor ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSPlayerResource::UpdatePlayerData( void )
{
	int i;

	m_iPlayerC4 = 0;
	m_iPlayerVIP = 0;

	int nTotalPlayingPlayers = 0;

	for ( i = 1; i <= gpGlobals->maxClients; i++ )
	{
		int botDifficulty = -1;
		CCSPlayer *pPlayer = (CCSPlayer*)UTIL_PlayerByIndex( i );
		
		bool bSetValidRanking = false;

		if ( pPlayer && pPlayer->IsConnected() )
		{
			if ( pPlayer->IsVIP() )
			{
				// we should only have one VIP
				contract_assert( m_iPlayerVIP == 0 );
				m_iPlayerVIP = i;
			}

			if ( pPlayer->HasC4() )
			{
				// we should only have one bomb
				m_iPlayerC4 = i;
			}

			m_iMVPs.Set( i, pPlayer->GetNumMVPs() );
			m_bHasDefuser.Set( i, pPlayer->HasDefuser() );
			m_bHasHelmet.Set( i, pPlayer->m_bHasHelmet );
			m_iArmor.Set( i, pPlayer->ArmorValue() );			
			m_iScore.Set( i, pPlayer->GetScore() );
			m_iTotalCashSpent.Set( i, pPlayer->GetTotalCashSpent() );
			m_iCashSpentThisRound.Set( i, pPlayer->GetCashSpentThisRound() );
			m_szClan.Set(i, AllocPooledString( pPlayer->GetClanTag() ) );

			m_nEndMatchNextMapVotes.Set( i, pPlayer->GetEndMatchNextMapVote() );

			m_nActiveCoinRank.Set( i, pPlayer->GetRank( MEDAL_CATEGORY_SEASON_COIN ) );

			m_nMusicID.Set( i, pPlayer->GetMusicID() );

			// UpdateAssassinationTargets();

			if ( CEconPersonaDataPublic const *pPublic = pPlayer->GetPersonaDataPublic() )
			{
				m_nPersonaDataPublicLevel.Set( i, pPublic->Obj().player_level() );
				m_nPersonaDataPublicCommendsLeader.Set( i, pPublic->Obj().commendation().cmd_leader() );
				m_nPersonaDataPublicCommendsTeacher.Set( i, pPublic->Obj().commendation().cmd_teaching() );
				m_nPersonaDataPublicCommendsFriendly.Set( i, pPublic->Obj().commendation().cmd_friendly() );
			}
			else
			{
				m_nPersonaDataPublicLevel.Set( i, -1 );
				m_nPersonaDataPublicCommendsLeader.Set( i, -1 );
				m_nPersonaDataPublicCommendsTeacher.Set( i, -1 );
				m_nPersonaDataPublicCommendsFriendly.Set( i, -1 );
			}

			if ( pPlayer->IsBot() )
			{
				CCSBot* pBot = dynamic_cast< CCSBot* >( pPlayer );

				if ( pBot )
				{
					// Retrieve and store the bot's difficulty level
					const BotProfile* pProfile = pBot->GetProfile();

					if ( pProfile )
					{
						botDifficulty = pProfile->GetMaxDifficulty();
					}

					m_iCompTeammateColor.Set( i, -2 );
					m_nAttemptedToGetColor[i] = true;
//					SetPlayerTeammateColor( i, false );
				
				}
			}
			else
			{
				if ( pPlayer->GetTeamNumber() != TEAM_SPECTATOR )
					nTotalPlayingPlayers++;
			
				SetPlayerTeammateColor( i, false );
			}

			if ( CSGameRules() && CSGameRules()->IsQueuedMatchmaking() )
			{
				for ( int k = 0; k < CCSGameRules::sm_QueuedServerReservation.rankings.size(); ++ k )
				{
					if ( CCSGameRules::sm_QueuedServerReservation.rankings[ k ].account_id == pPlayer->GetHumanPlayerAccountID() )
					{
						m_iCompetitiveRanking.Set( i, CCSGameRules::sm_QueuedServerReservation.rankings[ k ].rank_id );
						m_iCompetitiveWins.Set( i, CCSGameRules::sm_QueuedServerReservation.rankings[ k ].wins );
						bSetValidRanking = true;
						break;
					}
				}
			}
		}
		else
		{
			m_iMVPs.Set( i, 0 );
			m_bHasDefuser.Set( i, false );
			m_bHasHelmet.Set( i, false );
			m_iArmor.Set( i, 0 );		
			m_szClan.Set( i, MAKE_STRING( "" ) );
			m_nEndMatchNextMapVotes.Set( i, -1 );
			m_nActiveCoinRank.Set( i, -1 );
			m_nMusicID.Set( i, -1 );
			m_bIsAssassinationTarget.Set( i, 0 );

			m_nPersonaDataPublicLevel.Set( i, -1 );
			m_nPersonaDataPublicCommendsLeader.Set( i, -1 );
			m_nPersonaDataPublicCommendsTeacher.Set( i, -1 );
			m_nPersonaDataPublicCommendsFriendly.Set( i, -1 );

			m_iCompTeammateColor.Set( i, -1 );
			m_nAttemptedToGetColor[i] = false;
		}

		if ( !bSetValidRanking )
		{
			m_iCompetitiveRanking.Set( i, 0 );
			m_iCompetitiveWins.Set( i, 0 );
		}
		m_iBotDifficulty.Set( i, botDifficulty );
	}

	if ( g_fGameOver && nTotalPlayingPlayers > 0 && CSGameRules() && CSGameRules()->IsEndMatchVotingForNextMap() )
	{
		if ( m_bEndMatchNextMapAllVoted == false )
		{
			// ignore whether all players voted and just return whether or not we ran out of time
			if ( CSGameRules()->m_eEndMatchMapVoteState == CSGameRules()->k_EEndMatchMapVoteState_VoteTimeEnded ||
				CSGameRules()->m_eEndMatchMapVoteState == CSGameRules()->k_EEndMatchMapVoteState_AllPlayersVoted ||
				CSGameRules()->m_eEndMatchMapVoteState == CSGameRules()->k_EEndMatchMapVoteState_SelectingWinner ||
				CSGameRules()->m_eEndMatchMapVoteState == CSGameRules()->k_EEndMatchMapVoteState_SettingNextLevel ||
				CSGameRules()->m_eEndMatchMapVoteState == CSGameRules()->k_EEndMatchMapVoteState_VoteAllDone )
			{
				m_bEndMatchNextMapAllVoted = true;
			}
/*			m_bEndMatchNextMapAllVoted = (CSGameRules()->m_eEndMatchMapVoteState == CSGameRules()->k_EEndMatchMapVoteState_VoteTimeEnded);*/

// 			int nNumVotes = 0;
// 			int nHighestSingleVoteSlot = 0;
// 
// 			// this is done in three different places
// 			// TODO: make a function out of this
// 			int nVotes[MAX_ENDMATCH_VOTE_PANELS];
// 			for ( int i = 0; i < MAX_ENDMATCH_VOTE_PANELS; i++ )
// 				nVotes[i] = 0;
// 
// 			for ( int i = 1; i <= MAX_PLAYERS; i++ )
// 			{
// 				int nMapSlot = m_nEndMatchNextMapVotes[i];
// 				if ( nMapSlot != -1 && nMapSlot < MAX_ENDMATCH_VOTE_PANELS )
// 				{
// 					nVotes[nMapSlot] += 1;
// 					nNumVotes += 1;
// 				}
// 			}
// 			//////
// 
// 			for ( int i = 0; i < MAX_ENDMATCH_VOTE_PANELS + 1; i++ )
// 			{
// 				if ( nVotes[nHighestSingleVoteSlot] < nVotes[i] )
// 					nHighestSingleVoteSlot = i;
// 			}
// 
// 			if ( nTotalPlayingPlayers > 0 )
// 			{
// 				float flnVotesToSucceed = (float)nTotalPlayingPlayers * 0.501f;
// 				int nVotesToSucceed = ceil( flnVotesToSucceed );
// 
// 				if ( nNumVotes >= nTotalPlayingPlayers || nVotes[nHighestSingleVoteSlot] >= nVotesToSucceed || ( nextlevel.GetString() && *nextlevel.GetString() && engine->IsMapValid( nextlevel.GetString() ) ) )
// 					m_bEndMatchNextMapAllVoted = true;
// 			}
		}
	}
	else
	{
		// if we aren't at the end of the game or we don't have any players, no one has voted
		m_bEndMatchNextMapAllVoted = false;
	}

	int numHostages = std::ssize( g_Hostages );

	for ( i = 0; i < MAX_HOSTAGES; i++ )
	{
		if ( i >= numHostages )
		{
//			engine->Con_NPrintf( i, "Dead" );
			m_bHostageAlive.Set( i, false );
			m_isHostageFollowingSomeone.Set( i, false );
			m_iHostageEntityIDs.Set( i, 0 );
			continue;
		}

		CHostage* pHostage = g_Hostages[i];

		m_bHostageAlive.Set( i, pHostage->IsRescuable() );

		if ( pHostage->IsValid() )
		{
			m_iHostageEntityIDs.Set( i, pHostage->entindex() );
			m_isHostageFollowingSomeone.Set( i, pHostage->IsFollowingSomeone() );
//			engine->Con_NPrintf( i, "ID:%d Pos:(%.0f,%.0f,%.0f)", pHostage->entindex(), pHostage->GetAbsOrigin().x, pHostage->GetAbsOrigin().y, pHostage->GetAbsOrigin().z );
		}
		else
		{
//			engine->Con_NPrintf( i, "Invalid" );
		}
	}

	if( !m_foundGoalPositions )
	{
		// We only need to update these once a map, but we need the client to know about them.
		CBaseEntity* ent = nullptr;
		while ( ( ent = gEntList.FindEntityByClassname( ent, "func_bomb_target" ) ) != nullptr )
		{
			const Vector &pos = ent->WorldSpaceCenter();
			CNavArea *area = TheNavMesh->GetNearestNavArea( pos, true, 10000.0f, false, false );
			const char *placeName = (area) ? TheNavMesh->PlaceToName( area->GetPlace() ) : nullptr;
			if ( placeName == nullptr )
			{
				// The bomb site has no area or place name, so just choose A then B
				if ( m_bombsiteCenterA.Get().IsZero() )
				{
					m_bombsiteCenterA = pos;
				}
				else
				{
					m_bombsiteCenterB = pos;
				}
			}
			else
			{
				// The bomb site has a place name, so choose accordingly
				if( FStrEq( placeName, "BombsiteA" ) || FStrEq( placeName, "Bombsite" ) )
				{
					m_bombsiteCenterA = pos;
				}
				else
				{
					m_bombsiteCenterB = pos;
				}
			}
			m_foundGoalPositions = true;
		}

		int hostageRescue = 0;
		while ( (( ent = gEntList.FindEntityByClassname( ent, "func_hostage_rescue" ) ) != nullptr)  &&  (hostageRescue < MAX_HOSTAGE_RESCUES) )
		{
			const Vector &pos = ent->WorldSpaceCenter();
			m_hostageRescueX.Set( hostageRescue, (int) pos.x );	
			m_hostageRescueY.Set( hostageRescue, (int) pos.y );	
			m_hostageRescueZ.Set( hostageRescue, (int) pos.z );	

			hostageRescue++;
			m_foundGoalPositions = true;
		}
	}

	for ( int i=0; i < MAX_PLAYERS+1; i++ )
	{
		CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );

		bool bControllingBot = false;
		CCSPlayer *pControlledPlayer = nullptr;
		CCSPlayer *pControlledByPlayer = nullptr;

		if ( pPlayer && pPlayer->IsConnected() )
		{
			bControllingBot = pPlayer->IsControllingBot();
			pControlledPlayer = pPlayer->GetControlledBot();
			pControlledByPlayer = pPlayer->GetControlledByPlayer();
		}

		m_bControllingBot.Set( i, bControllingBot ? 1 : 0 );
		m_iControlledPlayer.Set( i, pControlledPlayer ? pControlledPlayer->entindex() : 0 );
		m_iControlledByPlayer.Set( i, pControlledByPlayer ? pControlledByPlayer->entindex() : 0 );

	}

	BaseClass::UpdatePlayerData();
}

void CCSPlayerResource::Spawn_Impl( void )
{
	m_iPlayerC4 = 0;
	m_iPlayerVIP = 0;
	m_bombsiteCenterA.Init();
	m_bombsiteCenterB.Init();
	m_foundGoalPositions = false;
	m_bPreferencesAssigned_CT = false;
	m_bPreferencesAssigned_T = false;
	std::memset( m_nAttemptedToGetColor, false, sizeof( m_nAttemptedToGetColor ) );

	for ( int i=0; i < MAX_HOSTAGES; i++ )
	{
		m_bHostageAlive.Set( i, 0 );
		m_isHostageFollowingSomeone.Set( i, 0 );
		m_iHostageEntityIDs.Set(i, 0);
	}

	for ( int i=0; i < MAX_HOSTAGE_RESCUES; i++ )
	{
		m_hostageRescueX.Set( i, 0 );
		m_hostageRescueY.Set( i, 0 );
		m_hostageRescueZ.Set( i, 0 );
	}

	for ( int i=0; i < MAX_PLAYERS+1; i++ )
	{
		m_iMVPs.Set( i, 0 );
		m_bHasDefuser.Set( i, false );
		m_bHasHelmet.Set( i, false );
		m_iArmor.Set( i, 0 );		
		m_iScore.Set( i, 0 );		
		m_iCompetitiveRanking.Set( i, 0 );
		m_iCompetitiveWins.Set( i, 0 );
		m_iCompTeammateColor.Set( i, -1 );
		m_iBotDifficulty.Set( i, -1 );
		m_szClan.Set( i, MAKE_STRING( "" ) );
		m_nActiveCoinRank.Set( i, -1 );
		m_nMusicID.Set( i, -1 );
		m_bIsAssassinationTarget.Set( i, 0 );

		m_nPersonaDataPublicLevel.Set( i, -1 );
		m_nPersonaDataPublicCommendsLeader.Set( i, -1 );
		m_nPersonaDataPublicCommendsTeacher.Set( i, -1 );
		m_nPersonaDataPublicCommendsFriendly.Set( i, -1 );
	}

	m_bEndMatchNextMapAllVoted = false;
	for ( int i=0; i < MAX_ENDMATCH_VOTE_PANELS+1; i++ )
	{
		m_nEndMatchNextMapVotes.Set( i, 0 );
	}

	BaseClass::Spawn_Impl();
}

const Vector CCSPlayerResource::GetBombsiteAPosition()
{
	return m_bombsiteCenterA;
}

const Vector CCSPlayerResource::GetBombsiteBPosition()
{
	return m_bombsiteCenterB;
}

const Vector CCSPlayerResource::GetHostageRescuePosition( int iIndex )
{
	if ( iIndex < 0 || iIndex >= MAX_HOSTAGE_RESCUES )
		return vec3_origin;

	Vector ret;

	ret.x = m_hostageRescueX[iIndex];
	ret.y = m_hostageRescueY[iIndex];
	ret.z = m_hostageRescueZ[iIndex];

	return ret;
}

//--------------------------------------------------------------------------------------------------------
int CCSPlayerResource::GetCompTeammateColor( int iIndex )
{
	CCSPlayer *pPlayer = ( CCSPlayer* )UTIL_PlayerByIndex( iIndex );
	if ( !pPlayer )
		return -1;

	if ( pPlayer->IsBot() )
		return -2;

	return m_iCompTeammateColor[iIndex];
}

void CCSPlayerResource::ResetPlayerTeammateColor( int index )
{
	CCSPlayer *pPlayer = ( CCSPlayer* )UTIL_PlayerByIndex( index );
	if ( !pPlayer )
		return;
		
	if ( CSGameRules() && CSGameRules()->IsPlayingAnyCompetitiveStrictRuleset() && CSGameRules()->IsQueuedMatchmaking() )
		return;

	int nTeamNum = pPlayer->GetTeamNumber();
	if ( nTeamNum > TEAM_SPECTATOR )
	{
		SetPlayerTeammateColor( index, true );
		return;
	}

	m_iCompTeammateColor.Set( index, -1 );
}

void CCSPlayerResource::ForcePlayersPickColors()
{
	m_bPreferencesAssigned_CT = true;
	m_bPreferencesAssigned_T = true;
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		m_nAttemptedToGetColor[i] = true;
}

void CCSPlayerResource::SetPlayerTeammateColor( int index, bool bReset )
{
	CCSPlayer *pPlayer = ( CCSPlayer* )UTIL_PlayerByIndex( index );
	if ( !pPlayer )
		return;

	m_nAttemptedToGetColor[index] = true;

	if ( !CSGameRules() || !CSGameRules()->IsPlayingAnyCompetitiveStrictRuleset() )
	{
		m_iCompTeammateColor.Set( index, -1 );
		return;
	}

 	if ( pPlayer->IsBot() )
 	{
 		m_iCompTeammateColor.Set( index, -2 );
 		return;
 	}

	int nTeamNum = pPlayer->GetTeamNumber();
	if ( nTeamNum > TEAM_SPECTATOR )
	{
		if ( CSGameRules() && CSGameRules()->IsPlayingAnyCompetitiveStrictRuleset() )
		{
			// check to see if we have a color already
			int idxThisPlayer = -1;

			// don't use the QMM code
			/*
			if ( CSGameRules()->IsQueuedMatchmaking() )
			{
				CCSPlayer *pThisPlayer = ( CCSPlayer* )UTIL_PlayerByIndex( index );
				CPlayerId steamID;
				pThisPlayer->GetSteamID( &steamID );
				int numTotalPlayers = 0;		
				static ConVarRef sv_mmqueue_reservation( "sv_mmqueue_reservation" );
				for ( char const *pszPrev = sv_mmqueue_reservation.GetString(), *pszNext = pszPrev;
					  ( pszNext = strchr( pszPrev, '[' ) ) != NULL; pszPrev = pszNext + 1 )
				{
					uint32 uiAccountId = 0;
					sscanf( pszNext, "[%x]", &uiAccountId );
					if ( uiAccountId && ( steamID.GetAccountID() == uiAccountId ) )
					{
						idxThisPlayer = numTotalPlayers;
					}
					++numTotalPlayers;
				}
			}
			*/

			// let all players have at least one crack at getting their prefered color before we start assigning loser colors
			if ( (nTeamNum == TEAM_TERRORIST && m_bPreferencesAssigned_T == false) ||
				 (nTeamNum == TEAM_CT && m_bPreferencesAssigned_CT == false) )
			{
				int nNumAttemptedToGetColor = 0;
				for ( int i = 1; i <= gpGlobals->maxClients; i++ )
				{
					CCSPlayer *pOtherPlayer = ( CCSPlayer* )UTIL_PlayerByIndex( i );
					if ( pOtherPlayer && pOtherPlayer->GetTeamNumber() == pPlayer->GetTeamNumber() )
					{
						if ( m_nAttemptedToGetColor[i] == true )
							nNumAttemptedToGetColor++;

						if ( nNumAttemptedToGetColor >= 5 )
						{
							if ( nTeamNum == TEAM_TERRORIST )
								m_bPreferencesAssigned_T = true;
							else
								m_bPreferencesAssigned_CT = true;

							break;
						}
					}
				}
			}

			// Valve MM gives us the player index - does this work?
			if ( idxThisPlayer > -1 )
			{
				m_iCompTeammateColor.Set( index, ( idxThisPlayer % 5 ) );
			}
			else if ( m_iCompTeammateColor[index] == -1 || bReset )//otherwise we have to do it ourselves
			{
				int nPreferredColor = pPlayer->GetTeammatePreferredColor( );
				if ( nPreferredColor == -1 )
				{
					pPlayer->InitTeammatePreferredColor( );
					nPreferredColor = pPlayer->GetTeammatePreferredColor( );
				}	

				// we didn't initialize, so try again another time
				if ( nPreferredColor == -1 )
					return;

				int nAssignedColor = m_iCompTeammateColor[index] > -1 ? m_iCompTeammateColor[index] : nPreferredColor;
				bool bColorInUse = false;
				for ( int ii = 0; ii < 5; ii++ )
				{
					nAssignedColor = nAssignedColor % 5;

					bColorInUse = false;
					for ( int j = 1; j <= gpGlobals->maxClients; j++ )
					{
						CCSPlayer *pOtherPlayer = ( CCSPlayer* )UTIL_PlayerByIndex( j );
						if ( pOtherPlayer && pOtherPlayer->GetTeamNumber( ) == pPlayer->GetTeamNumber( ) )
						{
							if ( nAssignedColor == m_iCompTeammateColor[j] && pOtherPlayer != pPlayer )
							{
								// All players should get a crack at getting their prefered color before a 
								// previously connected player crawls up the color scale and nabs it first
								if ( ( nTeamNum == TEAM_TERRORIST && m_bPreferencesAssigned_T == false) ||
									 ( nTeamNum == TEAM_CT && m_bPreferencesAssigned_CT == false ) )
									return;

								bColorInUse = true;
								nAssignedColor++;
								break;
							}
						}
					}

					if ( bColorInUse == false )
						break;
				}

				// somehow this failed
				contract_assert( ( !bColorInUse ) && "Trying to assign a color to a teammate, but all colors are already in use!" );

				nAssignedColor = bColorInUse == false ? nAssignedColor : -1;
				m_iCompTeammateColor.Set( index, nAssignedColor );
			}
		}
		else
			m_iCompTeammateColor.Set( index, -1 );
	}
}

bool CCSPlayerResource::IsAssassinationTarget( int index ) const
{
	return m_bIsAssassinationTarget[ index ];
}

bool Helper_DoesPlayerHaveAssassinateQuestForTeam( const CCSPlayer *pPlayer, int iTeamNum )
{
	// If this player has an assassination quest targeting this team, prefer not to pick them as the target
	CEconQuestDefinition *pQuest = GetItemSchema()->GetQuestDefinition( pPlayer->Inventory()->GetActiveQuestID() );
	return ( pQuest && IsAssassinationQuest( pQuest ) && ( ( int ) pQuest->GetTargetTeam() == iTeamNum ) );
}

bool Helper_ValidateAssassinationTarget( const CCSPlayer *pCurrentAssassinationTarget, int iTeamNum )
{
	// Validate current assassination target, pick new one if needed
	if ( !pCurrentAssassinationTarget || !pCurrentAssassinationTarget->IsConnected() ||
		pCurrentAssassinationTarget->GetTeamNumber() != iTeamNum || pCurrentAssassinationTarget->IsDead() ||
		pCurrentAssassinationTarget->IsControllingBot() || Helper_DoesPlayerHaveAssassinateQuestForTeam( pCurrentAssassinationTarget, iTeamNum ) )
	{
		return false;
	}

	return true;
}

ConVar sv_assassination_target_ratio( side_registry(), "sv_assassination_target_ratio", "5" );
void CCSPlayerResource::UpdateAssassinationTargets( const CEconQuestDefinition * pQuest )
{
	CCSTeam *pTeam = GetGlobalCSTeam( pQuest->GetTargetTeam() );
	if ( !pTeam )
		return;

	// 1 out of X players is an assassination target, no less than 1 and more more than MAX_ASSASSINATION_TARGETS.
	std::vector< CCSPlayer* > vecCandiates;
	auto iTargetsNeeded = Min( Max( 1, pTeam->GetHumanMembers( &vecCandiates ) / Max( 1, sv_assassination_target_ratio.GetInt() ) ), 3 );

	std::vector< CCSPlayer* > vecNotIdealPlayers;
	for ( int iter = (int)std::ssize( vecCandiates ) - 1; iter >= 0; iter-- )
	{
		CCSPlayer* pCur = vecCandiates[ iter ];
		// Validate current assassination targets, remove from candidate list
		if ( pCur->IsAssassinationTarget() )
		{
			// Still valid, then reduce count of needed targets
			if ( Helper_ValidateAssassinationTarget( pCur, pQuest->GetTargetTeam() ) )
			{
				iTargetsNeeded--;
			}
			else
			{
				// Prefer not to pick recently invalidated players
				m_bIsAssassinationTarget.GetForModify( pCur->entindex() ) = false;
				vecNotIdealPlayers.insert( vecNotIdealPlayers.begin(), pCur );
			}
			vecCandiates.erase( vecCandiates.begin() + iter );
		}
		else if ( Helper_DoesPlayerHaveAssassinateQuestForTeam( pCur, GetTeamNumber() ) )
		{
			// Prefer not to pick players with this quest 
			vecCandiates.erase( vecCandiates.begin() + iter );
			vecNotIdealPlayers.push_back( pCur );
		}
	}

	while ( iTargetsNeeded-- > 0 )
	{
		std::vector< CCSPlayer* > &vecBucket = !vecCandiates.empty() ? vecCandiates : vecNotIdealPlayers;
		CCSPlayer *pTarget = vecBucket[ RandomInt( 0, std::ssize( vecBucket ) - 1 ) ];
		ks::swap_erase_first( vecBucket, pTarget );
		m_bIsAssassinationTarget.GetForModify( pTarget->entindex() ) = true;
	}
}

class CAnimationLayer;
class CBaseAnimating;
class CBaseCombatCharacter;
class CBaseCombatWeapon;
class CBaseEntity;
class CBaseTrigger;
class CBaseViewModel;
class CCSPlayer;
class CCSWeaponInfo;
class CConfigurationForHighPriorityUseEntity_t;
class CEconItemView;
class CEffectData;
class CHostage;
class CRecipientFilter;
class CTakeDamageInfo;
class CWeaponCSBase;

using namespace ::ks::reflect;

ConVar sv_penetration_type( side_registry(), "sv_penetration_type", "1", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "What type of penertration to use. 0 = old CS, 1 = new penetration" );
ConVar sv_showimpacts_penetration( side_registry(), "sv_showimpacts_penetration", "0", FCVAR_REPLICATED | FCVAR_RELEASE, "Shows extra data when bullets penetrate. (use sv_showimpacts_time to increase time shown)" );

ConVar sv_showbullethits(side_registry(), "sv_showbullethits", "0", FCVAR_REPLICATED | FCVAR_RELEASE, "" );

ConVar sv_showimpacts(side_registry(), "sv_showimpacts", "0", FCVAR_REPLICATED | FCVAR_RELEASE, "Shows client (red) and server (blue) bullet impact point (1=both, 2=client-only, 3=server-only)" );
ConVar sv_showimpacts_time(side_registry(), "sv_showimpacts_time", "4", FCVAR_REPLICATED | FCVAR_RELEASE, "Duration bullet impact indicators remain before disappearing", true, 0.0f, true, 10.0f );
ConVar sv_showplayerhitboxes( side_registry(), "sv_showplayerhitboxes", "0", FCVAR_REPLICATED, "Show lag compensated hitboxes for the specified player index whenever a player fires." );

// friendly fire damage scalers
ConVar	ff_damage_reduction_grenade( side_registry(), "ff_damage_reduction_grenade", "0.25", FCVAR_REPLICATED | FCVAR_RELEASE, "How much to reduce damage done to teammates by a thrown grenade.  Range is from 0 - 1 (with 1 being damage equal to what is done to an enemy)" );
ConVar	ff_damage_reduction_grenade_self( side_registry(), "ff_damage_reduction_grenade_self", "1", FCVAR_REPLICATED | FCVAR_RELEASE, "How much to damage a player does to himself with his own grenade.  Range is from 0 - 1 (with 1 being damage equal to what is done to an enemy)" );
ConVar	ff_damage_reduction_bullets( side_registry(), "ff_damage_reduction_bullets", "0.1", FCVAR_REPLICATED | FCVAR_RELEASE, "How much to reduce damage done to teammates when shot.  Range is from 0 - 1 (with 1 being damage equal to what is done to an enemy)" );
ConVar	ff_damage_reduction_other( side_registry(), "ff_damage_reduction_other", "0.25", FCVAR_REPLICATED | FCVAR_RELEASE, "How much to reduce damage done to teammates by things other than bullets and grenades.  Range is from 0 - 1 (with 1 being damage equal to what is done to an enemy)" );
ConVar  ff_damage_bullet_penetration( side_registry(), "ff_damage_bullet_penetration", "0", FCVAR_REPLICATED | FCVAR_RELEASE, "If friendly fire is off, this will scale the penetration power and damage a bullet does when penetrating another friendly player", true, 0.0f, true, 1.0f );

ConVar sv_spec_use_tournament_content_standards( side_registry(), "sv_spec_use_tournament_content_standards", "0.0", FCVAR_REPLICATED | FCVAR_RELEASE );

extern ConVar mp_teammates_are_enemies;
extern ConVar mp_use_respawn_waves;
extern ConVar mp_respawn_on_death_ct;
extern ConVar mp_respawn_on_death_t;
extern ConVar mp_buy_allow_grenades;
extern ConVar mp_buy_anywhere;
extern ConVar mp_buy_during_immunity;
extern ConVar mp_free_armor;

#define	CS_MASK_SHOOT (MASK_SOLID|CONTENTS_DEBRIS)
static constexpr auto MAX_PENETRATION_DISTANCE = 90; // this is 7.5 feet

static constexpr auto CS_MAX_WALLBANG_TRAIL_LENGTH = 800;

void DispatchEffect( const char *pName, const CEffectData &data );

Vector CCSPlayer::Weapon_ShootPosition()
{
	Vector vecPos = BaseClass::Weapon_ShootPosition();

	// fail out to un-altered position
	if ( !m_bUseNewAnimstate || !m_PlayerAnimStateCSGO )
		return vecPos;

	// warning: the modify eye position call will query and set up bones
	// on the game server it is called when giving weapon items or firing bullets
	m_PlayerAnimStateCSGO->ModifyEyePosition( vecPos );

	return vecPos;
}

bool CCSPlayer::IsInBuyZone()
{
	if ( mp_buy_anywhere.GetInt() == 1 ||
		mp_buy_anywhere.GetInt() == GetTeamNumber() )
		return true;//m_bGunGameImmunity;

	return m_bInBuyZone;
}

bool CCSPlayer::IsInBuyPeriod()
{
	if ( mp_buy_during_immunity.GetInt() == 1 ||
		mp_buy_during_immunity.GetInt() == GetTeamNumber() )
	{
		return m_bGunGameImmunity;
	}
	else
	{

		return CSGameRules() ? !CSGameRules()->IsBuyTimeElapsed() : false;
	}
}

bool CCSPlayer::CanBuyDuringImmunity()
{
	return ( mp_buy_during_immunity.GetInt() == 1 ) || ( mp_buy_during_immunity.GetInt() == GetTeamNumber() ); 
}

bool CCSPlayer::IsAbleToInstantRespawn( void )
{
	if ( CSGameRules() )
	{
		switch( CSGameRules()->GetGamePhase() )
		{
		case GAMEPHASE_MATCH_ENDED:
		case GAMEPHASE_HALFTIME:
			return false;
		}

		if ( CSGameRules()->IsWarmupPeriod() )
			return true;
	}

	// if we use respawn waves AND the next respawn wave is past AND our team is able to respawn OR it is the warmup period
	return (	CSGameRules() && ( ( mp_respawn_on_death_ct.GetBool() && GetTeamNumber() == TEAM_CT ) || 
		( mp_respawn_on_death_t.GetBool() && GetTeamNumber() == TEAM_TERRORIST ) ) );
}

char const * CCSPlayer::IsAbleToApplySpray( trace_t *ptr, Vector *pvecForward, Vector *pvecRight )
{
	Vector forward, right;
	EyeVectors( &forward, &right );
	if ( pvecForward ) *pvecForward = forward;
	if ( pvecRight ) *pvecRight = right;

	if ( !IsAlive() || ( GetTeamNumber() != TEAM_CT && GetTeamNumber() != TEAM_TERRORIST ) )
		return "#SFUI_Notice_SprayPaint_NotAlive";

	trace_t	tr;

	Vector vecTraceOriginLocation = Weapon_ShootPosition();
	Vector vecTraceTestMaxDepth = vecTraceOriginLocation + forward * 96;
	UTIL_TraceLine( vecTraceOriginLocation,
		vecTraceTestMaxDepth,
		CONTENTS_SOLID | CONTENTS_MOVEABLE | CONTENTS_WINDOW,
		this, COLLISION_GROUP_NONE, &tr );

	if ( tr.fraction == 1.0 )
		return "#SFUI_Notice_SprayPaint_OutOfRange";

	// Have we hit something TOOLS/???
	if ( tr.surface.name && StringHasPrefix( tr.surface.name, "tools/" ) )
		return "#SFUI_Notice_SprayPaint_BadSurface";

	// Do a second trace to see if anything weird blocks the trace?
	// an example could be a destructible door, so spray would go through onto the wall behind it
	// then we should ignore the first trace obtained
	trace_t tr2;
	UTIL_TraceLine( vecTraceOriginLocation,
		vecTraceTestMaxDepth,
		CONTENTS_OPAQUE | CONTENTS_MONSTER,
		this, COLLISION_GROUP_NONE, &tr2 );
	if ( tr2.fraction + 0.001 < tr.fraction )
		return "#SFUI_Notice_SprayPaint_Obstructed";

	//
	// Now that we have the location+normal of the trace hit do some validation checks for
	// whether that's a good way to apply a decal
	//
	Vector vecHitToEye = ( vecTraceOriginLocation - tr.endpos );
	if ( vecHitToEye.LengthSqr() < 0.5f )
		return "#SFUI_Notice_SprayPaint_TooClose";

	/*vec_t lenToHit = */ vecHitToEye.NormalizeInPlace();
	if ( tr.plane.normal.Dot( vecHitToEye ) < 0.1f )
		return "#SFUI_Notice_SprayPaint_GrazingAngle"; // angle too grazing (or backfacing normal), 0.1 = 85 degrees

	if ( ptr )
		*ptr = tr;

	return nullptr; // all trace tests passed
}

float CCSPlayer::GetPlayerMaxSpeed()
{
	if ( GetMoveType() == MOVETYPE_NONE )
	{
		return CS_PLAYER_SPEED_STOPPED;
	}

	if ( IsObserver() )
	{
		// Player gets speed bonus in observer mode
		return CS_PLAYER_SPEED_OBSERVER;
	}

	bool bValidMoveState = ( State_Get() == STATE_ACTIVE || State_Get() == STATE_OBSERVER_MODE );
	if ( !bValidMoveState || m_bIsDefusing || m_bIsGrabbingHostage || (CSGameRules()->IsFreezePeriod() && !m_bCanMoveDuringFreezePeriod) )
	{
		// Player should not move during the freeze period
		return CS_PLAYER_SPEED_STOPPED;
	}

	float speed = BaseClass::GetPlayerMaxSpeed();

	speed = std::min( CS_PLAYER_SPEED_RUN, speed );
	
	if ( IsVIP() == true )  // VIP is slow due to the armour he's wearing
	{
		speed = CS_PLAYER_SPEED_VIP;
	}
	else if ( m_hCarriedHostage != nullptr )
	{
		speed = CS_PLAYER_SPEED_HAS_HOSTAGE;
	}
	else
	{
		CWeaponCSBase *pWeapon = dynamic_cast<CWeaponCSBase*>( GetActiveWeapon() );

		if ( pWeapon )
		{
			if ( HasShield() && IsShieldDrawn() )
			{
				speed = std::min( CS_PLAYER_SPEED_SHIELD, speed );	
			}
			else
			{
				speed = std::min( pWeapon->GetMaxSpeed(), speed );
			}
		}
	}

//	if ( HasExosuit() )
//		speed *= CS_PLAYER_SPEED_EXOSUIT_RUN_BONUS;

	return speed;
}

void CCSPlayer::GiveCarriedHostage( EHANDLE hHostage )
{
	if ( !IsAlive() )
		return;

	m_hCarriedHostage = hHostage;

	RefreshCarriedHostage( true );
}

void CCSPlayer::RefreshCarriedHostage( bool bForceCreate )
{
	if ( m_hCarriedHostage == nullptr )
		return;

	if ( m_hCarriedHostageProp == nullptr )
	{
		CHostageCarriableProp *pHostageProp = dynamic_cast< CHostageCarriableProp* >( CreateEntityByName( "hostage_carriable_prop" ) );

		if ( pHostageProp )
		{
			pHostageProp->SetAbsOrigin( GetAbsOrigin() );
			pHostageProp->SetSolid( SOLID_NONE );
			pHostageProp->SetModel( "models/hostage/hostage_carry.mdl" );
			pHostageProp->SetModelName( MAKE_STRING( "models/hostage/hostage_carry.mdl" ) );
			pHostageProp->SetParent( this );
			pHostageProp->SetOwnerEntity( this );
			pHostageProp->FollowEntity( this );
			m_hCarriedHostageProp = pHostageProp;

			CRecipientFilter filter;
			filter.MakeReliable();
			filter.AddRecipient( this );
			UTIL_ClientPrintFilter( filter, HUD_PRINTCENTER, "#Cstrike_TitlesTXT_CarryingHostage" );
		}
	}

	if ( bForceCreate && GetViewModel( 1 ) )
	{
		CBaseViewModel *vm = GetViewModel( 1 );
		UTIL_Remove( vm );
		m_hViewModel.Set( 1, INVALID_EHANDLE );
	}

	CPredictedViewModel *vm = nullptr;

	CBaseViewModel *pVM = GetViewModel( 1 );
	if ( pVM )
		vm = ( CPredictedViewModel * )pVM;
	else
	{
		vm = ( CPredictedViewModel * )CreateEntityByName( "predicted_viewmodel" );
		bForceCreate = true;
	}

	if ( vm )
	{
		vm->SetAbsOrigin( GetAbsOrigin() );
		vm->SetOwner( this );
		vm->SetIndex( 1 );
		int nAct = ACT_VM_IDLE;
		if ( bForceCreate )
		{
			nAct = ACT_VM_DRAW;
			DispatchSpawn( vm );
		}
		vm->FollowEntity( this, false );
		vm->SetModel( "models/hostage/v_hostage_arm.mdl" );

		int	idealSequence = vm->SelectWeightedSequence( ( Activity )nAct );
		if ( idealSequence >= 0 )
		{
			vm->SendViewModelMatchingSequence( idealSequence );
		}
		vm->SetShouldIgnoreOffsetAndAccuracy( true );

		m_hViewModel.Set( 1, vm );

		m_hHostageViewModel = vm;
	}

}

void CCSPlayer::RemoveCarriedHostage( void )
{
	m_hCarriedHostage = nullptr;

	if ( m_hCarriedHostageProp )
	{
		CBaseAnimating *pHostageProp = dynamic_cast< CBaseAnimating* >( m_hCarriedHostageProp.Get() );
		if ( pHostageProp )
		{
			pHostageProp->FollowEntity( nullptr );
			UTIL_Remove( pHostageProp );
		}

		m_hCarriedHostageProp = nullptr;
	}

	if ( m_hHostageViewModel || dynamic_cast<CPredictedViewModel*>(GetViewModel( 1 )) )
	{
		CPredictedViewModel *pHostageVM = dynamic_cast< CPredictedViewModel* >( m_hHostageViewModel.Get() );
		if ( !pHostageVM )
			pHostageVM = dynamic_cast<CPredictedViewModel*>(GetViewModel( 1 ));

		if ( pHostageVM )
		{
			pHostageVM->FollowEntity( nullptr );
			UTIL_Remove( pHostageVM );
		}

		m_hHostageViewModel = INVALID_EHANDLE;

		m_hViewModel.Set( 1, INVALID_EHANDLE );
	}
}

void CCSPlayer::GetBulletTypeParameters( 
	int iBulletType, 
	float &fPenetrationPower, 
	float &flPenetrationDistance )
{
	if ( sv_penetration_type.GetInt() == 1 )
	{
		fPenetrationPower = 35;
		flPenetrationDistance = 3000.0;
		return;
	}

	//MIKETODO: make ammo types come from a script file.
	if ( IsAmmoType( iBulletType, BULLET_PLAYER_50AE ) )
	{
		fPenetrationPower = 30;
		flPenetrationDistance = 1000.0;
	}
	else if ( IsAmmoType( iBulletType, BULLET_PLAYER_762MM ) )
	{
		fPenetrationPower = 39;
		flPenetrationDistance = 5000.0;
	}
	else if (	IsAmmoType( iBulletType, BULLET_PLAYER_556MM ) || 
				IsAmmoType( iBulletType, BULLET_PLAYER_556MM_SMALL ) ||
				IsAmmoType( iBulletType, BULLET_PLAYER_556MM_BOX ) )
	{
		fPenetrationPower = 35;
		flPenetrationDistance = 4000.0;
	}
	else if ( IsAmmoType( iBulletType, BULLET_PLAYER_338MAG ) )
	{
		fPenetrationPower = 45;
		flPenetrationDistance = 8000.0;
	}
	else if ( IsAmmoType( iBulletType, BULLET_PLAYER_9MM ) )
	{
		fPenetrationPower = 21;
		flPenetrationDistance = 800.0;
	}
	else if ( IsAmmoType( iBulletType, BULLET_PLAYER_BUCKSHOT ) )
	{
		fPenetrationPower = 0;
		flPenetrationDistance = 0.0;
	}
	else if ( IsAmmoType( iBulletType, BULLET_PLAYER_45ACP ) )
	{
		fPenetrationPower = 15;
		flPenetrationDistance = 500.0;
	}
	else if (	IsAmmoType( iBulletType, BULLET_PLAYER_357SIG ) ||
				IsAmmoType( iBulletType, BULLET_PLAYER_357SIG_SMALL ) ||
				IsAmmoType( iBulletType, BULLET_PLAYER_357SIG_P250 ) ||
				IsAmmoType( iBulletType, BULLET_PLAYER_357SIG_MIN ) )
	{
		fPenetrationPower = 25;
		flPenetrationDistance = 800.0;
	}
	else if ( IsAmmoType( iBulletType, BULLET_PLAYER_57MM ) )
	{
		fPenetrationPower = 30;
		flPenetrationDistance = 2000.0;
	}
	else if ( IsAmmoType( iBulletType, AMMO_TYPE_TASERCHARGE ) )
	{
		fPenetrationPower = 0;
		flPenetrationDistance = 0.0;
	}
	else
	{
		// What kind of ammo is this?
		contract_assert( false );
		fPenetrationPower = 0;
		flPenetrationDistance = 0.0;
	}
}

static bool TraceToExit( Vector start, Vector dir, Vector &end, trace_t &trEnter, trace_t &trExit, float flStepSize, float flMaxDistance )
{
	float flDistance = 0;
	Vector last = start;
	int nStartContents = 0;

	while ( flDistance <= flMaxDistance )
	{
		flDistance += flStepSize;

		end = start + ( flDistance * dir );

		Vector vecTrEnd = end - ( flStepSize * dir );

		if ( nStartContents == 0 )
			nStartContents = UTIL_PointContents( end, CS_MASK_SHOOT|CONTENTS_HITBOX );

		int nCurrentContents = UTIL_PointContents( end, CS_MASK_SHOOT|CONTENTS_HITBOX );

		if ( (nCurrentContents & CS_MASK_SHOOT) == 0 || ((nCurrentContents & CONTENTS_HITBOX) && nStartContents != nCurrentContents) )
		{
			// this gets a bit more complicated and expensive when we have to deal with displacements
			UTIL_TraceLine( end, vecTrEnd, CS_MASK_SHOOT|CONTENTS_HITBOX, nullptr, &trExit );

			// we exited the wall into a player's hitbox
			if ( trExit.startsolid == true && (trExit.surface.flags & SURF_HITBOX)/*( nStartContents & CONTENTS_HITBOX ) == 0 && (nCurrentContents & CONTENTS_HITBOX)*/ )
			{
				// do another trace, but skip the player to get the actual exit surface 
				UTIL_TraceLine( end, start, CS_MASK_SHOOT, trExit.Ent<CBaseEntity>(), COLLISION_GROUP_NONE, &trExit );
				if ( trExit.DidHit() && trExit.startsolid == false )
				{
					end = trExit.endpos;
					return true;
				}
			}
			else if ( trExit.DidHit() && trExit.startsolid == false )
			{
				bool bStartIsNodraw = !!( trEnter.surface.flags & (SURF_NODRAW) );
				bool bExitIsNodraw = !!( trExit.surface.flags & (SURF_NODRAW) );
				if ( bExitIsNodraw && IsBreakableEntity( trExit.Ent<CBaseEntity>() ) && IsBreakableEntity( trEnter.Ent<CBaseEntity>() ) )
				{
					// we have a case where we have a breakable object, but the mapper put a nodraw on the backside
					end = trExit.endpos;
					return true;
				}
				else if ( bExitIsNodraw == false || (bStartIsNodraw && bExitIsNodraw) ) // exit nodraw is only valid if our entrace is also nodraw
				{
					Vector vecNormal = trExit.plane.normal;
					float flDot = dir.Dot( vecNormal );
					if ( flDot <= 1.0f )
					{
						// get the real end pos
						end = end - ( (flStepSize * trExit.fraction) * dir );
						return true;
					}
				}
			}
			else if ( TraceDidHitNonWorldEntity( trEnter ) && IsBreakableEntity( trEnter.Ent<CBaseEntity>() ) )
			{
				// if we hit a breakable, make the assumption that we broke it if we can't find an exit (hopefully..)
				// fake the end pos
				trExit = trEnter;
				trExit.endpos = start + ( 1.0f * dir );
				return true;
			}
		}
	}

	return false;
}

inline void UTIL_TraceLineIgnoreTwoEntities( const Vector& vecAbsStart, const Vector& vecAbsEnd, unsigned int mask, 
					 const IHandleEntity *ignore, const IHandleEntity *ignore2, int collisionGroup, trace_t *ptr )
{
	Ray_t ray;
	ray.Init( vecAbsStart, vecAbsEnd );
	CTraceFilterSkipTwoEntities traceFilter( ignore, ignore2, collisionGroup );
	enginetrace->TraceRay( ray, mask, &traceFilter, ptr );
	if( r_visualizetraces.GetBool() )
	{
		DebugDrawLine( ptr->startpos, ptr->endpos, 255, 0, 0, true, -1.0f );
	}
}

void CCSPlayer::CheckForWeaponFiredAchievement()
{
	// Removed the Smorgasbord achievement... for now
}

bool CCSPlayer::IsPrimaryOrSecondaryWeapon( CSWeaponType nType )
{
	if ( nType == WEAPONTYPE_PISTOL || nType == WEAPONTYPE_SUBMACHINEGUN || nType == WEAPONTYPE_RIFLE ||  
		nType == WEAPONTYPE_SHOTGUN || nType == WEAPONTYPE_SNIPER_RIFLE || nType == WEAPONTYPE_MACHINEGUN )
	{
		return true;
	}

	return false;
}

bool CCSPlayer::IsOtherSameTeam( int nTeam )
{
	return GetTeamNumber() == nTeam;
}

bool CCSPlayer::IsOtherEnemy( CCSPlayer *pPlayer )
{
	if ( !pPlayer )
		return false;

	// we are never an enemy of ourselves
	if ( entindex() == pPlayer->entindex() )
		return false;
	
	int nOtherTeam = pPlayer->GetAssociatedTeamNumber();

	int nTeam = GetAssociatedTeamNumber();

	if ( mp_teammates_are_enemies.GetBool() && nTeam == nOtherTeam )
	{
		return true;
	}

	return nTeam != nOtherTeam;
}

uint32 CCSPlayer::GetActiveQuestID( void ) const
{
	uint32 unQuestID = 
	 m_Inventory.GetActiveQuestID(); 

	return unQuestID;
}

QuestProgress::Reason CCSPlayer::GetQuestProgressReason( void ) const
{

	return m_nQuestProgressReason;
}

bool CCSPlayer::IsAssassinationTarget( void ) const
{
	CCSPlayerResource* pCSPR =
		CSPlayerResource();

	if ( !pCSPR )
		return false;
	
	return pCSPR->IsAssassinationTarget( entindex() );
}

bool CCSPlayer::IsOtherEnemy( int nEntIndex )
{
	CCSPlayer *pPlayer = (CCSPlayer*) UTIL_PlayerByIndex( nEntIndex );
	if ( !pPlayer )
	{
		// client doesn't have a pointer to enemy players outside our PVS

		return false;
	}

	return IsOtherEnemy( pPlayer );
}

bool CCSPlayer::IsOtherEnemyAndPlaying( int nEntIndex )
{
	int nOtherTeam = 0;

	CCSPlayer *pPlayer = (CCSPlayer*) UTIL_PlayerByIndex( nEntIndex );
	if ( !pPlayer )
	{
		return false;
	}

	if ( ( GetTeamNumber() == TEAM_CT && nOtherTeam == TEAM_TERRORIST ) ||
		( GetTeamNumber() == TEAM_TERRORIST && nOtherTeam == TEAM_CT ) )
		return true;

	return false;
}

extern ConVar sv_buy_status_override;
bool CCSPlayer::CanPlayerBuy( bool display )
{
	if ( !CSGameRules() )
		return false;

	// is the player alive?
	if ( m_lifeState != LIFE_ALIVE )
	{
		return false;
	}

	// is the player in a buy zone?
	if ( !IsInBuyZone() )
	{
		return false;
	}

	// Don't allow buying in the last few seconds of warmup because everybody should be freezed, but sometimes people aren't
	// also fixes buy on the very moment that round starts which might cause the bought weapon to spawn, but touched by the
	// player in the actual match time next frame and have a powerful gun for the first pistol round.
	if ( CSGameRules()->IsWarmupPeriod() && ( CSGameRules()->GetWarmupPeriodEndTime() - 3 < gpGlobals->curtime ) )
		return false;

	// Even if buytime has run out, if buy time is tied to immunity then we let the player buy if the menu is still open.
	if ( !IsInBuyPeriod() && CanBuyDuringImmunity() )
			return IsBuyMenuOpen();

	CCSGameRules* mp = CSGameRules();

	if ( mp->m_bCTCantBuy && ( GetTeamNumber() == TEAM_CT ) )
	{
		if ( display == true )
			ClientPrint( this, HUD_PRINTCENTER, "#CT_cant_buy" );

		return false;
	}

	if ( mp->m_bTCantBuy && ( GetTeamNumber() == TEAM_TERRORIST ) )
	{
		if ( display == true )
			ClientPrint( this, HUD_PRINTCENTER, "#Terrorist_cant_buy" );

		return false;
	}

	if ( CSGameRules()->IsPlayingCoopGuardian() )
	{
		if ( CSGameRules()->IsWarmupPeriod() == false &&
			CSGameRules()->m_flGuardianBuyUntilTime < gpGlobals->curtime )
		{
			int nTeam = CSGameRules()->IsHostageRescueMap() ? TEAM_TERRORIST : TEAM_CT;
			if ( GetTeamNumber() == nTeam )
			{
				if ( display == true )
				{
					int iBuyStatus = sv_buy_status_override.GetInt();
					if ( iBuyStatus > 0 && ( ( nTeam == TEAM_CT && iBuyStatus != 1 ) || ( nTeam == TEAM_TERRORIST && iBuyStatus != 2 ) ) )
						ClientPrint( this, HUD_PRINTCENTER, "#SFUI_BuyMenu_CantBuy" );
					else
						ClientPrint( this, HUD_PRINTCENTER, "#SFUI_BuyMenu_CantBuyTilNextWave" );
				}
				return false;
			}
		}
	}

	int buyTime = mp_buytime.GetInt();

	if ( mp->IsBuyTimeElapsed() && !CanBuyDuringImmunity() )
	{
		if ( display == true )
		{
			char strBuyTime[16];
			V_snprintf( strBuyTime, sizeof( strBuyTime ), "%d", buyTime );
			ClientPrint( this, HUD_PRINTCENTER, "#Cant_buy", strBuyTime );
		}

		return false;
	}

	if ( mp->IsWarmupPeriod() && mp->IsPlayingCooperativeGametype() )
		return true;

	if ( mp->IsPlayingCoopMission() )
		return true;

	contract_assert( ( !m_bIsVIP ) && " There's no VIP in CSGO. Yet. If implementing, fix this GAME_DLL only code." );

	if ( m_bIsVIP )
	{
		if ( display == true )
			ClientPrint( this, HUD_PRINTCENTER, "#VIP_cant_buy" );

		return false;
	}

	return true;
}

bool CCSPlayer::GetUseConfigurationForHighPriorityUseEntity( CBaseEntity *pEntity, CConfigurationForHighPriorityUseEntity_t &cfg )
{
	if ( dynamic_cast<CPlantedC4*>( pEntity ) )
	{
		if ( CSGameRules() && CSGameRules()->IsBombDefuseMap() &&
			( this->GetTeamNumber() == TEAM_CT ) )
		{
			cfg.m_pEntity = pEntity;
		}
		else
		{
			// it's a high-priority entity, but not used by the player team
			cfg.m_pEntity = nullptr;
		}
		cfg.m_ePriority = cfg.k_EPriority_Bomb;
		cfg.m_eDistanceCheckType = cfg.k_EDistanceCheckType_2D;
		cfg.m_pos = pEntity->GetAbsOrigin() + Vector( 0, 0, 3 );
		cfg.m_flMaxUseDistance = 62;		// Cannot use if > 62 units away
		cfg.m_flLosCheckDistance = 36;		// Check LOS if > 36 units away (2D)
		cfg.m_flDotCheckAngle = -0.7;		// 0.7 taken from Goldsrc, +/- ~45 degrees
		cfg.m_flDotCheckAngleMax = -0.5;	// 0.3 for it going outside the range during continuous use (120-degree cone)
		return true;
	}
	else if ( dynamic_cast<CHostage*>( pEntity ) )
	{
		cfg.m_pEntity = pEntity;
		cfg.m_ePriority = cfg.k_EPriority_Hostage;
		cfg.m_eDistanceCheckType = cfg.k_EDistanceCheckType_3D;
		cfg.m_pos = pEntity->EyePosition();
		cfg.m_flMaxUseDistance = 62;		// Cannot use if > 62 units away
		cfg.m_flLosCheckDistance = 32;		// Check LOS if > 32 units away (2D)
		cfg.m_flDotCheckAngle = -0.7;		// 0.7 taken from Goldsrc, +/- ~45 degrees
		cfg.m_flDotCheckAngleMax = -0.5;	// 0.5 for it going outside the range during continuous use (120-degree cone)
		return true;
	}
	return false;
}

CBaseEntity *CCSPlayer::GetUsableHighPriorityEntity( void )
{
	// This is done separately since there might be something blocking our LOS to it
	// but we might want to use it anyway if it's close enough.  This should eliminate
	// the vast majority of bomb placement exploits (places where the bomb can be planted
	// but can't be "used".  This also mimics goldsrc cstrike behavior.

	CBaseEntity *pEntsNearPlayer[64];
	// 64 is the distance in Goldsrc.  However since Goldsrc did distance from the player's origin and we're doing distance from the player's eye, make the radius a bit bigger.
	int iEntsNearPlayer = UTIL_EntitiesInSphere( pEntsNearPlayer, 64, EyePosition(), 72, FL_OBJECT );
	if( iEntsNearPlayer != 0 )
	{
		CConfigurationForHighPriorityUseEntity_t cfgBestHighPriorityEntity;
		cfgBestHighPriorityEntity.m_pEntity = nullptr;
		cfgBestHighPriorityEntity.m_ePriority = cfgBestHighPriorityEntity.k_EPriority_Default;

		for( int i = 0; i != iEntsNearPlayer; ++i )
		{
			CBaseEntity *pEntity = pEntsNearPlayer[i];
			contract_assert( pEntity != nullptr );
			CConfigurationForHighPriorityUseEntity_t cfgUseSettings;
			if ( !GetUseConfigurationForHighPriorityUseEntity( pEntity, cfgUseSettings ) )
				continue; // not a high-priority entity
			if ( !cfgUseSettings.m_pEntity )
				continue; // not used by the player
			if ( cfgUseSettings.m_ePriority < cfgBestHighPriorityEntity.m_ePriority )
				continue; // we already have a higher priority entity
			
			if ( !cfgUseSettings.UseByPlayerNow( this, cfgUseSettings.k_EPlayerUseType_Start ) )
				continue; // cannot start use by the player right now
				
			// This high-priority entity passes the checks, remember it as best
			if ( cfgUseSettings.IsBetterForUseThan( cfgBestHighPriorityEntity ) )
				cfgBestHighPriorityEntity = cfgUseSettings;
		}

		return cfgBestHighPriorityEntity.m_pEntity;
	}

	return nullptr;
}

bool CConfigurationForHighPriorityUseEntity_t::IsBetterForUseThan( CConfigurationForHighPriorityUseEntity_t const &other ) const
{
	if ( !m_pEntity )
		return false;
	if ( !other.m_pEntity )
		return true;
	if ( m_ePriority < other.m_ePriority )
		return false;
	if ( m_ePriority > other.m_ePriority )
		return true;
	if ( m_flDotCheckAngleMax < other.m_flDotCheckAngleMax ) // We are looking at it with a better angle
		return true;
	if ( m_flMaxUseDistance < other.m_flMaxUseDistance ) // This entity is closer to user
		return true;
	return false;
}

bool CConfigurationForHighPriorityUseEntity_t::UseByPlayerNow( CCSPlayer *pPlayer, EPlayerUseType_t ePlayerUseType )
{
	if ( !pPlayer )
		return false;

	// entity is close enough, now make sure the player is facing the bomb.
	float flDistTo = std::numeric_limits<float>::max();
	switch ( m_eDistanceCheckType )
	{
	case k_EDistanceCheckType_2D:
		flDistTo = pPlayer->EyePosition().AsVector2D().DistTo( m_pos.AsVector2D() );
		break;
	case k_EDistanceCheckType_3D:
		flDistTo = pPlayer->EyePosition().DistTo( m_pos );
		break;
	default:
		contract_assert( false );
	}
	// UTIL_EntitiesInSphere gives strange results where I can find it when my eyes are at an angle, but not when I'm right on top of it
	// because of that, make sure it's in our radius, but check the 2d los and make sure we are as close or closer than we need to be in 1.6
	if ( flDistTo > m_flMaxUseDistance )
		return false;

	// if it's more than 36 units away (2d), we should check LOS
	if ( flDistTo > m_flLosCheckDistance )
	{
		trace_t tr;
		UTIL_TraceLine( pPlayer->EyePosition(), m_pos, (MASK_VISIBLE|CONTENTS_WATER|CONTENTS_SLIME), pPlayer, COLLISION_GROUP_DEBRIS, &tr );
		// if we can't trace to the bomb at this distance, then we fail
		if ( tr.fraction < 0.98 )
			return false;
	}

	Vector vecLOS = pPlayer->EyePosition() - m_pos;
	Vector forward;
	AngleVectors( pPlayer->EyeAngles(), &forward, nullptr, nullptr );

	vecLOS.NormalizeInPlace();

	float flDot = DotProduct(forward, vecLOS);
	float flCheckAngle = ( ePlayerUseType == k_EPlayerUseType_Start ) ? m_flDotCheckAngle : m_flDotCheckAngleMax;
	if ( flDot >= flCheckAngle )
		return false;

	// Remember the actual settings of this entity
	m_flDotCheckAngle = m_flDotCheckAngleMax = flDot;
	m_flLosCheckDistance = m_flMaxUseDistance = flDistTo;
	return true;
}
ConVar sv_server_verify_blood_on_player( side_registry(), "sv_server_verify_blood_on_player", "1", FCVAR_CHEAT | FCVAR_REPLICATED );

static const int kMaxNumPenetrationsSupported = 4;
struct DelayedDamageInfoData_t
{
	CTakeDamageInfo m_info;
	trace_t m_tr;

	typedef ks::small_vector< DelayedDamageInfoData_t, kMaxNumPenetrationsSupported > Array;
};

extern ConVar cl_weapon_debug_print_accuracy;
extern ConVar cl_weapon_debug_show_accuracy;
extern ConVar cl_weapon_debug_show_accuracy_duration;
void CCSPlayer::FireBullet( 
	Vector vecSrc,	// shooting postion
	const QAngle &shootAngles,  //shooting angle
	float flDistance, // max distance 
	float flPenetration, // the power of the penetration
	int nPenetrationCount,
	int iBulletType, // ammo type
	int iDamage, // base damage
	float flRangeModifier, // damage range modifier
	CBaseEntity *pevAttacker, // shooter
	bool bDoEffects,
	float xSpread, float ySpread
	)
{
	float fCurrentDamage = iDamage;   // damage of the bullet at it's current trajectory
	float flCurrentDistance = 0.0;  //distance that the bullet has traveled so far
		
	Vector vecDirShooting, vecRight, vecUp;
	AngleVectors( shootAngles, &vecDirShooting, &vecRight, &vecUp );
	
	// MIKETODO: put all the ammo parameters into a script file and allow for CS-specific params.
	float flPenetrationPower = 0;		// thickness of a wall that this bullet can penetrate
	float flPenetrationDistance = 0;	// distance at which the bullet is capable of penetrating a wall
	float flDamageModifier = 0.5f;		// default modification of bullets power after they go through a wall.
	float flPenetrationModifier = 1.0f;

	GetBulletTypeParameters( iBulletType, flPenetrationPower, flPenetrationDistance );

	// we use the max penetrations on this gun to figure out how much penetration it's capable of
	if ( sv_penetration_type.GetInt() == 1 )
		flPenetrationPower = flPenetration; 

	if ( !pevAttacker )
		pevAttacker = this;  // the default attacker is ourselves

	// add the spray 
	Vector vecDir = vecDirShooting + xSpread * vecRight + ySpread * vecUp;

	VectorNormalize( vecDir );

	//Adrian: visualize server/client player positions
	//This is used to show where the lag compesator thinks the player should be at.

	// [pfreese] Track number player entities killed with this bullet
	int iPenetrationKills = 0;
	int numPlayersHit = 0;

	// [menglish] Increment the shots fired for this player
	CCS_GameStats.Event_ShotFired( this, GetActiveWeapon() );
	m_bulletsFiredSinceLastSpawn++;
	CheckForWeaponFiredAchievement();

	bool bFirstHit = true;

	const CBaseCombatCharacter *lastPlayerHit = nullptr;	// this includes players, bots, and hostages

	bool bBulletHitPlayer = false;

	MDLCACHE_CRITICAL_SECTION();

	DelayedDamageInfoData_t::Array arrPendingDamage;

	bool bShotHitTeammate = false;

	float flDist_aim = 0;
	Vector vHitLocation = Vector( 0,0,0 );

	while ( fCurrentDamage > 0 )
	{
		Vector vecEnd = vecSrc + vecDir * (flDistance-flCurrentDistance);

		trace_t tr; // main enter bullet trace

		UTIL_TraceLineIgnoreTwoEntities( vecSrc, vecEnd, CS_MASK_SHOOT|CONTENTS_HITBOX, this, lastPlayerHit, COLLISION_GROUP_NONE, &tr );
		{
			CTraceFilterSkipTwoEntities filter( this, lastPlayerHit, COLLISION_GROUP_NONE );

			// Check for player hitboxes extending outside their collision bounds
			const float rayExtension = 40.0f;
			UTIL_ClipTraceToPlayers( vecSrc, vecEnd + vecDir * rayExtension, CS_MASK_SHOOT|CONTENTS_HITBOX, &filter, &tr );
		}

		if ( !flDist_aim )
		{
			flDist_aim = ( tr.fraction != 1.0 ) ? ( tr.startpos - tr.endpos ).Length() : 0;
		}

		if ( flDist_aim )
		{
			vHitLocation = tr.endpos;
		}

		lastPlayerHit = dynamic_cast<const CBaseCombatCharacter *>(tr.Ent<CBaseEntity>());

		if ( sv_showbullethits.GetInt() == 1 && !lastPlayerHit )
		{
			trace_t tr_bulletmiss;
			UTIL_TraceLine( vecSrc, vecEnd, CS_MASK_SHOOT, this, COLLISION_GROUP_NONE, &tr_bulletmiss );

			CCSPlayer *playerMissed = ToCSPlayer( tr_bulletmiss.Ent<CBaseEntity>() );
			if ( tr_bulletmiss.DidHit() && !tr_bulletmiss.startsolid && playerMissed )
			{
				Vector vecPelvisPos;
				QAngle angTemp;
				playerMissed->GetBonePosition( 0, vecPelvisPos, angTemp );

				Vector vecMissPos;
				CalcClosestPointOnLine( vecPelvisPos, vecSrc, vecEnd, vecMissPos );

				CStudioHdr *pStudioHdr = playerMissed->GetModelPtr();
				mstudiohitboxset_t *set = pStudioHdr->pHitboxSet( playerMissed->m_nHitboxSet );

				float flClosestHitboxDistance = std::numeric_limits<float>::max();
				Vector vecClosestHitboxPos = vecMissPos;
				int nClosestHitboxIndex = -1;

				Vector vecPos;
				QAngle angAng;
				for ( int n = 0; n < set->numhitboxes; n++ )
				{
					mstudiobbox_t *pbox = set->pHitbox( n );
					playerMissed->GetHitboxBonePosition( pbox->bone, vecPos, angAng, pbox->angOffsetOrientation );

					float flDist = vecPos.DistToSqr( tr_bulletmiss.endpos );

					if ( flDist < flClosestHitboxDistance )
					{
						flClosestHitboxDistance = flDist;
						vecClosestHitboxPos = vecPos;
						nClosestHitboxIndex = n;
					}
				}

				if ( nClosestHitboxIndex > -1 )
				{
					CalcClosestPointOnLine( vecClosestHitboxPos, vecSrc, vecEnd, vecMissPos );

					Vector vecMissDir = (vecEnd - vecSrc).Normalized();
					vecMissPos += vecMissDir * 15;

					int nBoneIndex = playerMissed->GetHitboxBone( nClosestHitboxIndex );					

					// build a matrix from the trace hit start and end position
					matrix3x4_t matWorldSpaceBulletHit;
					VectorMatrix( vecSrc - vecEnd, matWorldSpaceBulletHit );
					PositionMatrix( vecMissPos, matWorldSpaceBulletHit );
				
					// get the transform of the bone that owns the hitbox
					matrix3x4_t matBoneToWorldTransform;
					playerMissed->GetBoneTransform( nBoneIndex, matBoneToWorldTransform );

					// get the local transform of the hit transform relative to the bone transform
					matrix3x4_t matHitLocal;
					MatrixInvert( matBoneToWorldTransform, matHitLocal );
					MatrixMultiply( matHitLocal, matWorldSpaceBulletHit, matHitLocal );

					Vector vecPosTemp;
					QAngle angAngTemp;
					MatrixAngles( matHitLocal, angAngTemp, vecPosTemp );

					IGameEvent * bullet_hit_marker_event = gameeventmanager->CreateEvent( "add_bullet_hit_marker" );
					if ( bullet_hit_marker_event )
					{
						Vector vecBHitStart = vecSrc - (vecUp * 4) + (vecRight * 5);
						bullet_hit_marker_event->SetInt( "userid", playerMissed->GetUserID() );
						bullet_hit_marker_event->SetInt( "bone", nBoneIndex );
						bullet_hit_marker_event->SetFloat( "pos_x", vecPosTemp.x );
						bullet_hit_marker_event->SetFloat( "pos_y", vecPosTemp.y );
						bullet_hit_marker_event->SetFloat( "pos_z", vecPosTemp.z );
						bullet_hit_marker_event->SetFloat( "ang_x", angAngTemp.x );
						bullet_hit_marker_event->SetFloat( "ang_y", angAngTemp.y );
						bullet_hit_marker_event->SetFloat( "ang_z", angAngTemp.z );
						bullet_hit_marker_event->SetFloat( "start_x", vecBHitStart.x );
						bullet_hit_marker_event->SetFloat( "start_y", vecBHitStart.y );
						bullet_hit_marker_event->SetFloat( "start_z", vecBHitStart.z );
						bullet_hit_marker_event->SetBool( "hit", false );
						gameeventmanager->FireEvent( bullet_hit_marker_event );
					}
				}
			}
		}

		if ( lastPlayerHit )
		{
			if ( lastPlayerHit->GetTeamNumber() == GetTeamNumber() )
			{
				bShotHitTeammate = true;
			}

			bBulletHitPlayer = true;
		}

		if ( tr.fraction == 1.0f )
			break; // we didn't hit anything, stop tracing shoot

		bFirstHit = false;
		//
		// Propogate a bullet impact event
		// @todo Add this for shotgun pellets (which dont go thru here)
		//
		IGameEvent * event = gameeventmanager->CreateEvent( "bullet_impact" );
		if ( event )
		{
			event->SetInt( "userid", GetUserID() );
			event->SetFloat( "x", tr.endpos.x );
			event->SetFloat( "y", tr.endpos.y );
			event->SetFloat( "z", tr.endpos.z );
			gameeventmanager->FireEvent( event );
		}
		FirePerfStatsEvent( PERF_STATS_BULLET ); // client-only event

		/************* MATERIAL DETECTION ***********/
		surfacedata_t *pSurfaceData = physprops->GetSurfaceData( tr.surface.surfaceProps );
		int iEnterMaterial = pSurfaceData->game.material;

		flPenetrationModifier = pSurfaceData->game.penetrationModifier;
		flDamageModifier = pSurfaceData->game.damageModifier;

		bool hitGrate = ( tr.contents & CONTENTS_GRATE ) != 0;

		// bullet registration recording ( server )
		// This code allowed us to measure discrepency between client and server bullet hits.
		// It became obsolete when we started using a separate seed for client and server
		// to eliminate 'rage' hacks.
		//
		CSingleUserRecipientFilter user( this );

		ks::net::CCSUsrMsg_ReportHit msg;
		msg.pos_x = tr.endpos.x;
		msg.pos_y = tr.endpos.y;
		msg.pos_z = tr.endpos.z;
		msg.timestamp = gpGlobals->realtime;

		// only compare shots that were server hits
		if ( tr.Ent<CBaseEntity>() && tr.Ent<CBaseEntity>()->IsPlayer() )
		{
			SendUserMessage( user, ks::net::CS_UM_ReportHit, msg );
		}
		
		// end bullet registration recording

		if ( sv_showbullethits.GetInt() == 1 && tr.Ent<CBaseEntity>() && tr.Ent<CBaseEntity>()->IsPlayer() )
		{
			CCSPlayer *pPlayer = ToCSPlayer( tr.Ent<CBaseEntity>() );
			IGameEvent * bullet_hit_marker_event = gameeventmanager->CreateEvent( "add_bullet_hit_marker" );
			if ( bullet_hit_marker_event )
			{
				Vector vecHitPos;
				QAngle angHitAng;
				int nBoneIndex;

				pPlayer->GetBulletHitLocalBoneOffset( tr, nBoneIndex, vecHitPos, angHitAng );

				Vector vecBHitStart = vecSrc - (vecUp * 4) + (vecRight * 5);
				bullet_hit_marker_event->SetInt( "userid", pPlayer->GetUserID() );
				bullet_hit_marker_event->SetInt( "bone", nBoneIndex );
				bullet_hit_marker_event->SetFloat( "pos_x", vecHitPos.x );
				bullet_hit_marker_event->SetFloat( "pos_y", vecHitPos.y );
				bullet_hit_marker_event->SetFloat( "pos_z", vecHitPos.z );
				bullet_hit_marker_event->SetFloat( "ang_x", angHitAng.x );
				bullet_hit_marker_event->SetFloat( "ang_y", angHitAng.y );
				bullet_hit_marker_event->SetFloat( "ang_z", angHitAng.z );
				bullet_hit_marker_event->SetFloat( "start_x", vecBHitStart.x );
				bullet_hit_marker_event->SetFloat( "start_y", vecBHitStart.y );
				bullet_hit_marker_event->SetFloat( "start_z", vecBHitStart.z );
				bullet_hit_marker_event->SetBool( "hit", true );

				gameeventmanager->FireEvent( bullet_hit_marker_event );
			}
		}

		if ( sv_showimpacts.GetInt() == 1 || sv_showimpacts.GetInt() == 3 )
		{
			// draw blue server impact markers
			NDebugOverlay::Box( tr.endpos, Vector(-2,-2,-2), Vector(2,2,2), 0,0,255,127, sv_showimpacts_time.GetFloat() );
		}

		// client-server hit comparison.
		if ( tr.Ent<CBaseEntity>() && tr.Ent<CBaseEntity>()->IsPlayer() && !IsControllingBot()  )
		{
			if ( m_totalHitsOnServer < 255 ) // clamp at 8 bits
				m_totalHitsOnServer++;
		}

		// draw green boxes where the shot originated from
		//NDebugOverlay::Box( vecSrc, Vector(-1,-1,-1), Vector(1,1,1), 0,255,90,90, 10 );

		//calculate the damage based on the distance the bullet travelled.
		flCurrentDistance += tr.fraction * (flDistance-flCurrentDistance);
		fCurrentDamage *= std::pow (flRangeModifier, (flCurrentDistance / 500));

		// the value of iPenetration when the round reached its max penetration distance
		int nPenetrationAtMaxDistance = 0;
		// save off how many penetrations this bullet had in case we reached max distance and stomp the value later
		int const numPenetrationsInitiallyAllowedForThisBullet = nPenetrationCount;

		// check if we reach penetration distance, no more penetrations after that
		// or if our modifyer is super low, just stop the bullet
		if ( (flCurrentDistance > flPenetrationDistance && flPenetration > 0 ) ||
			flPenetrationModifier < 0.1 )
		{
			nPenetrationAtMaxDistance = 0;
			// Setting nPenetrationCount to zero prevents the bullet from penetrating object at max distance
			// and will no longer trace beyond the exit point, however "numPenetrationsInitiallyAllowedForThisBullet"
			// is saved off to allow correct determination whether the hit on the object at max distance had
			// *previously* penetrated anything or not. In case of a direct hit over 3000 units the saved off
			// value would be max penetrations value and will determine a direct hit and not a penetration hit.
			// However it is important that all tracing further stops past this point (as the code does at
			// the time of writing) because otherwise next trace will think that 4 penetrations have already
			// occurred.
			nPenetrationCount = 0;
		}

		// This just keeps track of sounds for AIs (it doesn't play anything).
		CSoundEnt::InsertSound( SOUND_BULLET_IMPACT, tr.endpos, 400, 0.2f, this );

		int iDamageType = DMG_BULLET | DMG_NEVERGIB;
		CWeaponCSBase* pActiveWeapon = GetActiveCSWeapon();
		if ( pActiveWeapon && pActiveWeapon->IsA( WEAPON_TASER ) )
		{
			iDamageType = DMG_SHOCK | DMG_NEVERGIB;
		}

		if( bDoEffects )
		{
			// See if the bullet ended up underwater + started out of the water
			if ( enginetrace->GetPointContents( tr.endpos, MASK_WATER ) & (CONTENTS_WATER|CONTENTS_SLIME) )
			{	
				trace_t waterTrace;
				UTIL_TraceLine( vecSrc, tr.endpos, (MASK_SHOT|CONTENTS_WATER|CONTENTS_SLIME), this, COLLISION_GROUP_NONE, &waterTrace );
				
				if( waterTrace.allsolid != 1 )
				{
					CEffectData	data;
 					data.m_vOrigin = waterTrace.endpos;
					data.m_vNormal = waterTrace.plane.normal;
					data.m_flScale = random_valve->RandomFloat( 8, 12 );

					if ( waterTrace.contents & CONTENTS_SLIME )
					{
						data.m_fFlags |= FX_WATER_IN_SLIME;
					}

					DispatchEffect( "gunshotsplash", data );
				}
			}
			else
			{
				//Do Regular hit effects

				// Don't decal nodraw surfaces
				if ( !( tr.surface.flags & (SURF_SKY|SURF_NODRAW|SURF_HINT|SURF_SKIP) ) )
				{
					//CBaseEntity *pEntity = tr.Ent<CBaseEntity>();
					UTIL_ImpactTrace( &tr, iDamageType );
				}
			}
		}

		// decal players on the server to eliminate the disparity between where the client thinks the decal went and where it actually went
		// we want to eliminate the case where a player sees a blood decal on someone, but they are at 100 health
		if ( sv_server_verify_blood_on_player.GetBool() && tr.DidHit() && tr.Ent<CBaseEntity>() && tr.Ent<CBaseEntity>()->IsPlayer() )
		{
			UTIL_ImpactTrace( &tr, iDamageType );
		}

		// add damage to entity that we hit
		
		CBaseEntity *pEntity = tr.Ent<CBaseEntity>();	

	//
	// DAMAGE MUST BE DEFERRED TILL LATER IF WE DECIDE TO SHIP IT
	//
// 		if ( sv_shoot_dropped_grenades.GetBool() )
// 		{
// 			CBaseCSGrenade* pWeapon = dynamic_cast<CBaseCSGrenade*>( pEntity );
			//Only detonate shot grenades if they have been dropped in the world longer than the grace period.
			//This prevents shooting at players and they miraculously explode - because you shot their grenade the instant they died
// 			if ( pWeapon && gpGlobals->curtime > (pWeapon->m_flDroppedAtTime + sv_shoot_dropped_grenades_grace_time.GetFloat()) )
// 			{
// 				pWeapon->ShotDetonate( this, pWeapon->GetCSWpnData() );
// 				pWeapon->AddSolidFlags( FSOLID_NOT_SOLID );
// 				pWeapon->AddEffects( EF_NODRAW );
// 				UTIL_Remove( pWeapon );
// 			}
// 		}
		// [pfreese] Check if enemy players were killed by this bullet, and if so,
		// add them to the iPenetrationKills count

		DelayedDamageInfoData_t &delayedDamage = arrPendingDamage.emplace_back();
		delayedDamage.m_tr = tr;

		int nObjectsPenetrated = kMaxNumPenetrationsSupported - ( numPenetrationsInitiallyAllowedForThisBullet + nPenetrationAtMaxDistance );
		CTakeDamageInfo &info = delayedDamage.m_info;
		info.Set( pevAttacker, pevAttacker, GetActiveWeapon(), fCurrentDamage, iDamageType, 0, nObjectsPenetrated );

		// [dkorus] note:  This is the number of players hit up to this point, not the total number this bullet WILL hit.
		info.SetDamagedOtherPlayers( numPlayersHit );

		// Set the bullet ID so that we can later track all the enemies that are damage by the same bullet
		info.SetBulletID( GetBulletGroup(), pActiveWeapon ? (int)pActiveWeapon->m_flRecoilIndex : 0 );
		info.SetAmmoType( iBulletType );
		CalculateBulletDamageForce( &info, iBulletType, vecDir, tr.endpos );

		bool bWasAlive = pEntity->IsAlive();

		// === Damage applied later ===

		if ( bWasAlive && pEntity->IsPlayer() && IsOtherEnemy( pEntity->entindex() ) )
		{
			numPlayersHit++;
		}

		if ( sv_showimpacts_penetration.GetInt() > 0 )
		{
			char text[4];
			V_snprintf( text, sizeof( text ), "^" );
			char text2[32];
			V_snprintf( text2, sizeof( text2 ), "%s%d", ( sv_showimpacts_penetration.GetInt() == 2 ) ? "" : "DAMAGE APPLIED:  ", (int)std::ceil(fCurrentDamage) );
			char text3[32];
			// convert to meters
			//(100%% of shots will fall within a 30cm circle.)
			float flDistMeters = ( flCurrentDistance*0.0254 );
			if ( flDistMeters >= 1.0 )
				V_snprintf( text3, sizeof( text3 ), "%s%0.1fm", ( sv_showimpacts_penetration.GetInt() == 2 ) ? "" : "TOTAL DISTANCE:  ", flDistMeters );
			else
				V_snprintf( text3, sizeof( text3 ), "%s%0.1fcm", ( sv_showimpacts_penetration.GetInt() == 2 ) ? "" : "TOTAL DISTANCE:  ", flDistMeters/0.01 );

			Vector textPos = tr.endpos;

			NDebugOverlay::EntityTextAtPosition( textPos, 1, text, sv_showimpacts_time.GetFloat(), 225, 128, 64, 255 );
			NDebugOverlay::EntityTextAtPosition( textPos, 2, text2, sv_showimpacts_time.GetFloat(), 255, 64, 0, 255 );
			NDebugOverlay::EntityTextAtPosition( textPos, 3, text3, sv_showimpacts_time.GetFloat(), 255, 128, 64, 255 );

			NDebugOverlay::Box( tr.endpos, Vector( -0.8, -0.8, -0.8 ), Vector( 0.8, 0.8, 0.8 ), 255, 100, 50, 64, sv_showimpacts_time.GetFloat() );
		}

		// [dkorus] note: values are changed inside of HandleBulletPenetration
		bool bulletStopped = HandleBulletPenetration( flPenetration, iEnterMaterial, hitGrate, tr, vecDir, pSurfaceData, flPenetrationModifier,
			flDamageModifier, bDoEffects, iDamageType, flPenetrationPower, nPenetrationCount, vecSrc, flDistance,
			flCurrentDistance, fCurrentDamage );

		// [dkorus] bulletStopped is true if the bullet can no longer continue penetrating materials
		if ( bulletStopped )
			break;

	}

	if ( bBulletHitPlayer && !bShotHitTeammate )
	{	// Guarantee that the bullet that hit an enemy trumps the player viewangles
		// that are locked in for the duration of the server simulation ticks
		m_iLockViewanglesTickNumber = gpGlobals->tickcount;
		m_qangLockViewangles = pl.v_angle;
	}

	for ( auto &pendingDamage : arrPendingDamage )
	{
		ClearMultiDamage();

		CTakeDamageInfo &info = pendingDamage.m_info;
		trace_t &tr = pendingDamage.m_tr;

		CBaseEntity *pEntity = tr.Ent<CBaseEntity>();
		bool bWasAlive = pEntity->IsAlive();

		pEntity->DispatchTraceAttack( info, vecDir, &tr );
		TraceAttackToTriggers( info, tr.startpos, tr.endpos, vecDir );

		ApplyMultiDamage();

		if ( bWasAlive && !pEntity->IsAlive() && pEntity->IsPlayer() && IsOtherEnemy( pEntity->entindex() ) )
		{
			++iPenetrationKills;
		}
	}

	// [pfreese] If we killed at least two enemies with a single bullet, award the
	// TWO_WITH_ONE_SHOT achievement

	if ( iPenetrationKills >= 2 )
	{
		AwardAchievement( CSKillTwoWithOneShot );
	}

}

// [dkorus] helper for FireBullet
//			changes iPenetration to updated value
//			returns true if we should stop processing more hits after this one
//			returns false if we can continue processing
bool CCSPlayer::HandleBulletPenetration( float &flPenetration,
										 int &iEnterMaterial,
										 bool &hitGrate,
										 trace_t &tr,
										 Vector &vecDir,
										 surfacedata_t *pSurfaceData,
										 float flPenetrationModifier,
										 float flDamageModifier,
										 bool bDoEffects,
										 int iDamageType,
										 float flPenetrationPower,
										 int &nPenetrationCount,
										 Vector &vecSrc,
										 float flDistance,
										 float flCurrentDistance,
										 float &fCurrentDamage)
{
	bool bIsNodraw = !!( tr.surface.flags & (SURF_NODRAW) );

	bool bFailedPenetrate = false;

	// check if bullet can penetrarte another entity
	if ( nPenetrationCount == 0 && !hitGrate && !bIsNodraw 
		 && iEnterMaterial != CHAR_TEX_GLASS && iEnterMaterial != CHAR_TEX_GRATE )
		bFailedPenetrate = true; // no, stop

	// If we hit a grate with iPenetration == 0, stop on the next thing we hit
	if ( flPenetration <= 0 || nPenetrationCount <= 0 )
		bFailedPenetrate = true;

	Vector penetrationEnd;

	// find exact penetration exit
	trace_t exitTr;
	if ( !TraceToExit( tr.endpos, vecDir, penetrationEnd, tr, exitTr, 4, MAX_PENETRATION_DISTANCE ) )
	{
		// ended in solid
		if ( (UTIL_PointContents ( tr.endpos, CS_MASK_SHOOT ) & CS_MASK_SHOOT) == 0 )
		{
			bFailedPenetrate = true;
		}
	}
	
	if ( bFailedPenetrate == true )
	{
		float flTraceDistance = VectorLength( penetrationEnd - tr.endpos );

		// this is copy pasted from below, it should probably be its own function
		float flPenMod = std::max<std::common_type_t<int, float>>( 0, ( 1 / flPenetrationModifier ) );
		float flPercentDamageChunk = fCurrentDamage * 0.15;
		float flDamageLostImpact = flPercentDamageChunk + std::max<std::common_type_t<int, double>>( 0, ( 3/ flPenetrationPower ) * 1.18 ) * (flPenMod * 2.8);

		float flLostDamageObject = ( ( flPenMod * ( flTraceDistance*flTraceDistance ) ) / 24 );
		float flTotalLostDamage = flDamageLostImpact + flLostDamageObject;
		DisplayPenetrationDebug( tr.endpos, penetrationEnd, flTraceDistance, fCurrentDamage, flDamageLostImpact, flTotalLostDamage, tr.surface.surfaceProps, -100 );
		return true;
	}

	//debugoverlay->AddBoxOverlay( exitTr.endpos, Vector(-1,-1,-1), Vector(1,1,1), QAngle( 0, 0, 0), 255,255,0,127, 400 );

	// get material at exit point
	surfacedata_t *pExitSurfaceData = physprops->GetSurfaceData( exitTr.surface.surfaceProps );
	int iExitMaterial = pExitSurfaceData->game.material;

	// new penetration method
	if ( sv_penetration_type.GetInt() == 1 )
	{
		// percent of total damage lost automatically on impacting a surface
		float flDamLostPercent = 0.16;

		// since some railings in de_inferno are CONTENTS_GRATE but CHAR_TEX_CONCRETE, we'll trust the
		// CONTENTS_GRATE and use a high damage modifier.
		if ( hitGrate || bIsNodraw || iEnterMaterial == CHAR_TEX_GLASS || iEnterMaterial == CHAR_TEX_GRATE )
		{
			// If we're a concrete grate (TOOLS/TOOLSINVISIBLE texture) allow more penetrating power.
			if ( iEnterMaterial == CHAR_TEX_GLASS || iEnterMaterial == CHAR_TEX_GRATE )
			{
				flPenetrationModifier = 3.0f;
				flDamLostPercent = 0.05;
			}
			else
				flPenetrationModifier = 1.0f;

			flDamageModifier = 0.99f;
		}
		else if ( iEnterMaterial == CHAR_TEX_FLESH && ff_damage_reduction_bullets.GetFloat() == 0 
				  && tr.Ent<CBaseEntity>() && tr.Ent<CBaseEntity>()->IsPlayer() && tr.Ent<CBaseEntity>()->GetTeamNumber() == GetTeamNumber() )
		{
			if ( ff_damage_bullet_penetration.GetFloat() == 0 )
			{
				// don't allow penetrating players when FF is off
				flPenetrationModifier = 0;
				return true;
			}

			flPenetrationModifier = ff_damage_bullet_penetration.GetFloat();
			flDamageModifier = ff_damage_bullet_penetration.GetFloat();
		}
		else
		{
			// check the exit material and average the exit and entrace values
			float flExitPenetrationModifier = pExitSurfaceData->game.penetrationModifier;
			float flExitDamageModifier = pExitSurfaceData->game.damageModifier;
			flPenetrationModifier = (flPenetrationModifier + flExitPenetrationModifier)/2;
			flDamageModifier = (flDamageModifier + flExitDamageModifier)/2;
		}

		// if enter & exit point is wood we assume this is 
		// a hollow crate and give a penetration bonus
		if ( iEnterMaterial == iExitMaterial )
		{
			if( iExitMaterial == CHAR_TEX_WOOD || iExitMaterial == CHAR_TEX_CARDBOARD )
			{
				flPenetrationModifier = 3;
			}
			else if ( iExitMaterial == CHAR_TEX_PLASTIC )
			{
				flPenetrationModifier = 2;
			}
		}

		float flTraceDistance = VectorLength( exitTr.endpos - tr.endpos );

		float flPenMod = std::max<std::common_type_t<int, float>>( 0, ( 1 / flPenetrationModifier ));

		float flPercentDamageChunk = fCurrentDamage * flDamLostPercent;
		float flPenWepMod = flPercentDamageChunk + std::max<std::common_type_t<int, double>>( 0, ( 3/ flPenetrationPower ) * 1.25 ) * (flPenMod * 3.0);

		float flLostDamageObject = ((flPenMod * (flTraceDistance*flTraceDistance)) / 24);
		float flTotalLostDamage = flPenWepMod + flLostDamageObject;

		if ( sv_showimpacts_penetration.GetInt() > 0 )
		{
			Vector vecStart = tr.endpos;
			Vector vecEnd = penetrationEnd;
			float flTotalTraceDistance = VectorLength( penetrationEnd - tr.endpos );

			DisplayPenetrationDebug( vecStart, vecEnd, flTotalTraceDistance, fCurrentDamage, flPenWepMod, flTotalLostDamage, tr.surface.surfaceProps, exitTr.surface.surfaceProps );
		}
		
		// reduce damage power each time we hit something other than a grate
		fCurrentDamage -= std::max<std::common_type_t<int, float>>( 0, flTotalLostDamage );

		if ( fCurrentDamage < 1 )
			return true;

		// penetration was successful

		// bullet did penetrate object, exit Decal
		if ( bDoEffects )
		{
			UTIL_ImpactTrace( &exitTr, iDamageType );
		}

		// decal players on the server to eliminate the disparity between where the client thinks the decal went and where it actually went
		// we want to eliminate the case where a player sees a blood decal on someone, but they are at 100 health
		if ( sv_server_verify_blood_on_player.GetBool() && tr.DidHit() && tr.Ent<CBaseEntity>() && tr.Ent<CBaseEntity>()->IsPlayer() )
		{
			UTIL_ImpactTrace( &tr, iDamageType );
		}

		//setup new start end parameters for successive trace

		//flPenetrationPower -= (flTraceDistance/2) / flPenMod;
		flCurrentDistance += flTraceDistance;

		// NDebugOverlay::Box( exitTr.endpos, Vector(-2,-2,-2), Vector(2,2,2), 0,255,0,127, 8 );

		vecSrc = exitTr.endpos;
		flDistance = (flDistance - flCurrentDistance) * 0.5;

		nPenetrationCount--;
		return false;
	}
	else
	{
		// since some railings in de_inferno are CONTENTS_GRATE but CHAR_TEX_CONCRETE, we'll trust the
		// CONTENTS_GRATE and use a high damage modifier.
		if ( hitGrate || bIsNodraw )
		{
			// If we're a concrete grate (TOOLS/TOOLSINVISIBLE texture) allow more penetrating power.
			flPenetrationModifier = 1.0f;
			flDamageModifier = 0.99f;
		}
		else
		{
			// Check the exit material to see if it is has less penetration than the entrance material.
			float flExitPenetrationModifier = pExitSurfaceData->game.penetrationModifier;
			float flExitDamageModifier = pExitSurfaceData->game.damageModifier;
			if ( flExitPenetrationModifier < flPenetrationModifier )
			{
				flPenetrationModifier = flExitPenetrationModifier;
			}
			if ( flExitDamageModifier < flDamageModifier )
			{
				flDamageModifier = flExitDamageModifier;
			}
		}

		// if enter & exit point is wood or metal we assume this is 
		// a hollow crate or barrel and give a penetration bonus
		if ( iEnterMaterial == iExitMaterial )
		{
			if ( iExitMaterial == CHAR_TEX_WOOD ||
				 iExitMaterial == CHAR_TEX_METAL )
			{
				flPenetrationModifier *= 2;
			}
		}

		float flTraceDistance = VectorLength( exitTr.endpos - tr.endpos );

		// check if bullet has enough power to penetrate this distance for this material
		if ( flTraceDistance > ( flPenetrationPower * flPenetrationModifier ) )
			return true; // bullet hasn't enough power to penetrate this distance

		// reduce damage power each time we hit something other than a grate
		fCurrentDamage *= flDamageModifier;

		// penetration was successful

		// bullet did penetrate object, exit Decal
		if ( bDoEffects )
		{
			UTIL_ImpactTrace( &exitTr, iDamageType );
		}

		// decal players on the server to eliminate the disparity between where the client thinks the decal went and where it actually went
		// we want to eliminate the case where a player sees a blood decal on someone, but they are at 100 health
		if ( sv_server_verify_blood_on_player.GetBool() && tr.DidHit() && tr.Ent<CBaseEntity>() && tr.Ent<CBaseEntity>()->IsPlayer() )
		{
			UTIL_ImpactTrace( &tr, iDamageType );
		}

		//setup new start end parameters for successive trace

		flPenetrationPower -= flTraceDistance / flPenetrationModifier;
		flCurrentDistance += flTraceDistance;

		// NDebugOverlay::Box( exitTr.endpos, Vector(-2,-2,-2), Vector(2,2,2), 0,255,0,127, 8 );

		vecSrc = exitTr.endpos;
		flDistance = ( flDistance - flCurrentDistance ) * 0.5;

		// reduce penetration counter
		nPenetrationCount--;
		return false;
	}
}

void CCSPlayer::DisplayPenetrationDebug( Vector vecEnter, Vector vecExit, float flDistance, float flInitialDamage, float flDamageLostImpact, float flTotalLostDamage, short nEnterSurf, short nExitSurf )
{
	if ( sv_showimpacts_penetration.GetInt() > 0 )
	{
		Vector vecStart = vecEnter;
		Vector vecEnd = vecExit;
		float flTotalTraceDistance = VectorLength( vecExit - vecEnter );
		//float flEnd = flTotalTraceDistance;
		//short nExitSurf = exitTr.surface.surfaceProps;
		if ( flTotalLostDamage >= flInitialDamage )
		{
			nExitSurf = -100;

			float flLostDamageObject = (flTotalLostDamage - flDamageLostImpact) ;
			//float flLostLinear = sqrt(flDistance * (flLostDamageObject)) * 24;
			float flFrac = std::max<std::common_type_t<int, float>>( 0, ( flInitialDamage - flDamageLostImpact ) / flLostDamageObject );
			vecEnd = ( vecEnd - vecStart );
			VectorNormalize( vecEnd );
			vecEnd = vecStart + ( vecEnd*flTotalTraceDistance*flFrac );

			if ( flDamageLostImpact >= flInitialDamage )
			{
				flDistance = 0;
				vecStart = vecEnd;
			}

			flTotalLostDamage = ( int )std::ceil( flInitialDamage );
		}

		Vector textPos = vecEnd;
		
		char text[64];
		if ( flTotalLostDamage < flInitialDamage )
		{
			
			float flDistMeters = ( flDistance*0.0254 );
			if ( flDistMeters >= 1.0 )
				V_snprintf( text, sizeof( text ), "%s%0.1fm", ( sv_showimpacts_penetration.GetInt() == 2 ) ? "" : "THICKNESS:		", flDistMeters );
			else
				V_snprintf( text, sizeof( text ), "%s%0.1fcm", ( sv_showimpacts_penetration.GetInt() == 2 ) ? "" : "THICKNESS:		", flDistMeters / 0.01 );

		}
		else
		{
			V_snprintf( text, sizeof( text ), "%s", "STOPPED!" );
		}
		
		NDebugOverlay::EntityTextAtPosition( textPos, -3, text, sv_showimpacts_time.GetFloat(), 220, 128, 128, 255 );

		char text3[64];
		V_snprintf( text3, sizeof( text3 ), "%s%0.1f", ( sv_showimpacts_penetration.GetInt() == 2 ) ? "-" : "LOST DAMAGE:		", flTotalLostDamage );

		NDebugOverlay::EntityTextAtPosition( textPos, -2, text3, sv_showimpacts_time.GetFloat(), 90, 22, 0, 160 );

		char textmat1[64];
		V_snprintf( textmat1, sizeof( textmat1 ), "%s", physprops->GetPropName( nEnterSurf ) );
		NDebugOverlay::EntityTextAtPosition( vecStart, -1, textmat1, sv_showimpacts_time.GetFloat(), 0, 255, 0, 128 );

		if ( nExitSurf != -100 )
		{
			NDebugOverlay::Box( vecStart, Vector( -0.4, -0.4, -0.4 ), Vector( 0.4, 0.4, 0.4 ), 0, 255, 0, 128, sv_showimpacts_time.GetFloat() );		

			char textmat2[64];
			V_snprintf( textmat2, sizeof( textmat2 ), "%s", ( nExitSurf == -1 ) ? "" : physprops->GetPropName( nExitSurf ) );
			NDebugOverlay::Box( vecEnd, Vector( -0.4, -0.4, -0.4 ), Vector( 0.4, 0.4, 0.4 ), 0, 128, 255, 128, sv_showimpacts_time.GetFloat() );
			NDebugOverlay::EntityTextAtPosition( vecEnd, -1, textmat2, sv_showimpacts_time.GetFloat(), 0, 128, 255, 128 );

			if ( flDistance > 0 && vecStart != vecEnd )
				NDebugOverlay::Line( vecStart, vecEnd, 0, 190, 190, true, sv_showimpacts_time.GetFloat() );
		}
		else
		{
			// different color
			NDebugOverlay::Box( vecStart, Vector( -0.4, -0.4, -0.4 ), Vector( 0.4, 0.4, 0.4 ), 160, 255, 0, 128, sv_showimpacts_time.GetFloat() );
			NDebugOverlay::Line( vecStart, vecEnd, 190, 190, 0, true, sv_showimpacts_time.GetFloat() );
		}	
	}
}

void CCSPlayer::ImpactTrace_Impl( trace_t *pTrace, int iDamageType, const char *pCustomImpactName )
{

	static ConVar *violence_hblood = cvar->FindVar( "violence_hblood" );
	if ( violence_hblood && !violence_hblood->GetBool() )
		return;

	CVProfScope _( "CCSPlayer::ImpactTrace", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	contract_assert( pTrace->Ent<CBaseEntity>() );

	CBaseEntity *pEntity = pTrace->Ent<CBaseEntity>();

	// Build the impact data
	CEffectData data;
	data.m_vOrigin = pTrace->endpos;
	data.m_vStart = pTrace->startpos;
	data.m_nSurfaceProp = pTrace->surface.surfaceProps;
	if ( data.m_nSurfaceProp < 0 )
	{
		data.m_nSurfaceProp = 0;
	}
	data.m_nDamageType = iDamageType;
	data.m_nHitBox = pTrace->hitbox;
	data.m_nEntIndex = pEntity->entindex();
	data.m_fFlags |= EFFECTDATA_SERVER_IGNOREPREDICTIONCULL;

	if ( sv_server_verify_blood_on_player.GetBool() )
	{
		data.m_vOrigin -= GetAbsOrigin();
		data.m_vStart -= GetAbsOrigin();
		data.m_bPositionsAreRelativeToEntity = true;
	}

	// Send it on its way
	if ( !pCustomImpactName )
	{
		DispatchEffect( "Impact", data );
	}
	else
	{
		DispatchEffect( pCustomImpactName, data );
	}
}

void CCSPlayer::UpdateStepSound( surfacedata_t *psurface, const Vector &vecOrigin, const Vector &vecVelocity  )
{
	if ( IsBot() && IsDormant() )
		return;

	if (!IsAlive())
		return;

	float speedSqr = vecVelocity.LengthSqr();

	float flWalkSpeed = (CS_PLAYER_SPEED_RUN * CS_PLAYER_SPEED_WALK_MODIFIER);

	if ( ( speedSqr < flWalkSpeed * flWalkSpeed ) || m_bIsWalking )
	{
		if ( speedSqr < 10.0 )
		{
			// If we stop, reset the step sound tracking.
			// This makes step sounds play a consistent time after
			// we start running making it easier to co-ordinate suit and
			// step sounds.
			SetStepSoundTime( STEPSOUNDTIME_NORMAL, false );
		}

		return; // player is not running, no footsteps
	}

	BaseClass::UpdateStepSound( psurface, vecOrigin, vecVelocity  );
}

ConVar weapon_recoil_view_punch_extra( side_registry(), "weapon_recoil_view_punch_extra", "0.055", FCVAR_RELEASE | FCVAR_CHEAT | FCVAR_REPLICATED, "Additional (non-aim) punch added to view from recoil" );

void CCSPlayer::KickBack( float fAngle, float fMagnitude )
{
	QAngle angleVelocity(0,0,0);
	angleVelocity[YAW] = -std::sinf(DEG2RAD(fAngle)) * fMagnitude;
	angleVelocity[PITCH] = -std::cosf(DEG2RAD(fAngle)) * fMagnitude;
	angleVelocity += m_Local.m_aimPunchAngleVel.Get();
	SetAimPunchAngleVelocity( angleVelocity );

	// this bit gives additional punch to the view (screen shake) to make the kick back a bit more visceral
	QAngle viewPunch = GetViewPunchAngle();
	float fViewPunchMagnitude = fMagnitude * weapon_recoil_view_punch_extra.GetFloat();
	viewPunch[YAW] -= sinf(DEG2RAD(fAngle)) * fViewPunchMagnitude;
	viewPunch[PITCH] -= cosf(DEG2RAD(fAngle)) * fViewPunchMagnitude;
	SetViewPunchAngle(viewPunch);
}

QAngle CCSPlayer::GetAimPunchAngle()
{

	if ( PlatformInputDevice::IsInputDeviceAPointer( GetPlayerInputDevice() ) )
	{
		return m_Local.m_aimPunchAngle.Get() * weapon_recoil_scale_motion_controller.GetFloat();
	}
	else
	{
		return m_Local.m_aimPunchAngle.Get() * weapon_recoil_scale.GetFloat();
	}
}

QAngle CCSPlayer::GetRawAimPunchAngle() const
{
	return m_Local.m_aimPunchAngle.Get();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CCSPlayer::GetDefaultCrouchedFOV( void ) const
{
	return GetDefaultFOV();
}

bool CCSPlayer::CanMove() const
{
	// When we're in intro camera mode, it's important to return false here 
	// so our physics object doesn't fall out of the world.
	if ( GetMoveType() == MOVETYPE_NONE )
		return false;

	if ( IsObserver() )
		return true; // observers can move all the time

	bool bValidMoveState = (State_Get() == STATE_ACTIVE || State_Get() == STATE_OBSERVER_MODE);
			
	if ( m_bIsDefusing || m_bIsGrabbingHostage || !bValidMoveState || (CSGameRules()->IsFreezePeriod() && !m_bCanMoveDuringFreezePeriod) )
	{
		return false;
	}
	else
	{
		// Can't move while planting C4.
		CC4 *pC4 = dynamic_cast< CC4* >( GetActiveWeapon() );
		if ( pC4 && pC4->m_bStartedArming )
			return false;

		return true;
	}
}

unsigned int CCSPlayer::PhysicsSolidMaskForEntity_Impl( void ) const
{
	if ( !CSGameRules()->IsTeammateSolid() )
	{
		switch ( GetTeamNumber() )
		{
			case TEAM_UNASSIGNED:
				return MASK_PLAYERSOLID;
			case LAST_SHARED_TEAM:
				return MASK_PLAYERSOLID;
			case TEAM_TERRORIST:
				return MASK_PLAYERSOLID | CONTENTS_TEAM1;
			case TEAM_CT:
				return MASK_PLAYERSOLID | CONTENTS_TEAM2;
		}
	}

	return MASK_PLAYERSOLID;
}

void CCSPlayer::OnJump( float fImpulse )
{
	CWeaponCSBase* pActiveWeapon = GetActiveCSWeapon();
	if ( pActiveWeapon != nullptr )
		pActiveWeapon->OnJump(fImpulse);
}

void CCSPlayer::OnLand( float fVelocity )
{

	CWeaponCSBase* pActiveWeapon = GetActiveCSWeapon();
	if (pActiveWeapon != nullptr)
		pActiveWeapon->OnLand(fVelocity);

	if ( fVelocity > 270 )
	{
		CRecipientFilter filter;

		filter.AddAllPlayers();
		// the client plays it's own sound
		filter.RemoveRecipient( this );
		
			EmitSound(filter, entindex(), "Default.Land");

			if (!m_pSurfaceData)
				return;

			unsigned short stepSoundName = m_pSurfaceData->sounds.runStepLeft;
			if (!stepSoundName)
				return;

			IPhysicsSurfaceProps *physprops = MoveHelper()->GetSurfaceProps();

			const char *pRawSoundName = physprops->GetString(stepSoundName);

			char szStep[512];

			if (GetTeamNumber() == TEAM_TERRORIST)
			{
				V_snprintf(szStep, sizeof(szStep), "t_%s", pRawSoundName);
			}
			else
			{
				V_snprintf(szStep, sizeof(szStep), "ct_%s", pRawSoundName);
			}

			EmitSound(filter, entindex(), szStep);
			
	}

}

//-------------------------------------------------------------------------------------------------------------------------------
/**
* Track the last time we were on a ladder, along with the ladder's normal and where we
* were grabbing it, so we don't reach behind us and grab it again as we are trying to
* dismount.
*/
void CCSPlayer::SurpressLadderChecks( const Vector& pos, const Vector& normal )
{
	m_ladderSurpressionTimer.Start( 1.0f );
	m_lastLadderPos = pos;
	m_lastLadderNormal = normal;
}

//-------------------------------------------------------------------------------------------------------------------------------
/**
* Prevent us from re-grabbing the same ladder we were just on:
*  - if the timer is elapsed, let us grab again
*  - if the normal is different, let us grab
*  - if the 2D pos is very different, let us grab, since it's probably a different ladder
*/
bool CCSPlayer::CanGrabLadder( const Vector& pos, const Vector& normal )
{
	if ( m_ladderSurpressionTimer.GetRemainingTime() <= 0.0f )
	{
		return true;
	}

	const float MaxDist = 64.0f;
	if ( pos.AsVector2D().DistToSqr( m_lastLadderPos.AsVector2D() ) < MaxDist * MaxDist )
	{
		return false;
	}

	if ( normal != m_lastLadderNormal )
	{
		return true;
	}

	return false;
}

void CCSPlayer::SetAnimation( PLAYER_ANIM playerAnim )
{
	// In CS, its CPlayerAnimState object manages ALL the animation state.
	return;
}

CWeaponCSBase* CCSPlayer::CSAnim_GetActiveWeapon()
{
	return GetActiveCSWeapon();
}

bool CCSPlayer::CSAnim_CanMove()
{
	return CanMove();
}

int CCSPlayer::GetCarryLimit( CSWeaponID weaponId )
{
	const CCSWeaponInfo *pWeaponInfo = GetWeaponInfo( weaponId );
	if ( pWeaponInfo == nullptr )
		return 0;

	if ( pWeaponInfo->GetWeaponType() == WEAPONTYPE_GRENADE )
	{
		return GetAmmoDef()->MaxCarry( pWeaponInfo->GetPrimaryAmmoType( ), this );	// We still use player-stored ammo for grenades.
	}

	return 1;
}

extern ConVar mp_weapons_allow_zeus;
extern ConVar mp_weapons_allow_typecount;
AcquireResult::Type CCSPlayer::CanAcquire( CSWeaponID weaponId, AcquireMethod::Type acquireMethod, CEconItemView *pItem )
{
	const CCSWeaponInfo *pWeaponInfo = nullptr;
	if ( weaponId == WEAPON_NONE && (pItem == nullptr || !pItem->IsValid()) )
		return AcquireResult::InvalidItem;

	if ( pItem && pItem->IsValid() )
	{
		weaponId = WeaponIdFromString( pItem->GetStaticData()->GetItemClass() );
		if ( weaponId == WEAPON_NONE )
			return AcquireResult::InvalidItem;

		pWeaponInfo = GetWeaponInfo( weaponId );
	}
	else
		pWeaponInfo = GetWeaponInfo( weaponId );

	if ( pWeaponInfo == nullptr )
		return AcquireResult::InvalidItem;

	AcquireResult::Type nGamerulesResult = CSGameRules()->IsWeaponAllowed( pWeaponInfo, GetTeamNumber(), pItem );
	if ( nGamerulesResult != AcquireResult::Allowed )
	{
		return nGamerulesResult;
	}

	int nType = pWeaponInfo->GetWeaponType( pItem );

// 	if ( acquireMethod == AcquireMethod::Buy )
// 	{
// 		bool bFoundInLoadout = false;
// 		int nStartSearchPos = LOADOUT_POSITION_INVALID;
// 
// 		if ( nType == WEAPONTYPE_PISTOL )
// 			nStartSearchPos = LOADOUT_POSITION_SECONDARY0;
// 		else if ( nType == WEAPONTYPE_SHOTGUN || nType == WEAPONTYPE_MACHINEGUN )
// 			nStartSearchPos = LOADOUT_POSITION_HEAVY0;
// 		else if ( nType == WEAPONTYPE_SUBMACHINEGUN )
// 			nStartSearchPos = LOADOUT_POSITION_SMG0;
// 		else if ( nType == WEAPONTYPE_RIFLE || nType == WEAPONTYPE_SNIPER_RIFLE )
// 			nStartSearchPos = LOADOUT_POSITION_RIFLE0;
// 
// 		// make sure that we have this item equipped in our inventory loadout
// 		for ( int i = nStartSearchPos; i < (nStartSearchPos+6); ++i )
// 		{
// 			CEconItemView *pItemView = Inventory()->GetItemInLoadout( GetTeamNumber(), i );
// 			if ( pItemView && pItemView->GetStaticData() )
// 			{
// 				if ( pItemView == pItem || ( V_stricmp( pItemView->GetStaticData()->GetDefinitionName(), WeaponIdAsString( weaponId ) ) == 0 ) )
// 				{
// 					bFoundInLoadout = true;
// 					break;
// 				}
// 			}
// 		}
// 		if ( nStartSearchPos != LOADOUT_POSITION_INVALID && !bFoundInLoadout )
// 			return AcquireResult::NotAllowedForPurchase;
// 	}

	if ( nType == WEAPONTYPE_GRENADE )
	{
		if ( mp_buy_allow_grenades.GetBool() == false )
		{
			if ( acquireMethod == AcquireMethod::Buy )
				return AcquireResult::NotAllowedForPurchase;
		}

		// make sure we aren't exceeding the ammo max for this grenade type
		int carryLimitThisGrenade = GetCarryLimit( weaponId );

		int carryLimitAllGrenades = ammo_grenade_limit_total.GetInt();

		CBaseCombatWeapon* pGrenadeWeapon = ( pItem && pItem->IsValid() ) ? CSWeapon_OwnsThisType( pItem ) : Weapon_OwnsThisType( WeaponIdAsString( weaponId ) );
		if ( pGrenadeWeapon != nullptr )
		{
			int nAmmoType = pGrenadeWeapon->GetPrimaryAmmoType();

			if( nAmmoType != -1 )
			{
				int thisGrenadeCarried = GetAmmoCount(nAmmoType );
				if ( thisGrenadeCarried >= carryLimitThisGrenade )
				{
					return AcquireResult::ReachedGrenadeTypeLimit;
				}
			}
		}

		// count how many grenades of any type the player is currently carrying
		int allGrenadesCarried = 0;
		for ( int i = 0; i < MAX_WEAPONS; ++i )
		{
			CWeaponCSBase* pWeapon = dynamic_cast<CWeaponCSBase*>( GetWeapon( i) );
			if ( pWeapon != nullptr && pWeapon->IsKindOf( WEAPONTYPE_GRENADE ) )
			{
				int nAmmoType = pWeapon->GetPrimaryAmmoType();
				if( nAmmoType != -1 )
				{
					allGrenadesCarried += GetAmmoCount( nAmmoType );
				}
			}
		}

		if ( allGrenadesCarried >= carryLimitAllGrenades )
		{
			return AcquireResult::ReachedGrenadeTotalLimit;
		}

		// don't allow players with an inferno spawning weapon to pick up another inferno spawning weapon
		if ( weaponId == WEAPON_INCGRENADE )
		{
			 if ( Weapon_OwnsThisType( "weapon_molotov" ) )
				 return AcquireResult::AlreadyOwned;
		}
		else if ( weaponId == WEAPON_MOLOTOV )
		{
			if ( Weapon_OwnsThisType( "weapon_incgrenade" ) )
				return AcquireResult::AlreadyOwned;
		}
	}
	else if ( nType == WEAPONTYPE_STACKABLEITEM )
	{
		int carryLimit = GetAmmoDef()->MaxCarry( pWeaponInfo->GetPrimaryAmmoType(), this );

		CBaseCombatWeapon* pItemWeapon = ( pItem && pItem->IsValid() ) ? CSWeapon_OwnsThisType( pItem ) : Weapon_OwnsThisType( WeaponIdAsString( weaponId ) );
		if ( pItemWeapon != nullptr )
		{
			int nAmmoType = pItemWeapon->GetPrimaryAmmoType();

			if ( nAmmoType != -1 )
			{
				int thisCarried = GetAmmoCount( nAmmoType );
				if ( thisCarried >= carryLimit )
				{
					return AcquireResult::ReachedGrenadeTypeLimit;
				}
			}
		}
	}
	else if ( weaponId == ITEM_KEVLAR )
	{
		if ( mp_free_armor.GetBool() )
		{
			if ( acquireMethod == AcquireMethod::Buy )
				return AcquireResult::NotAllowedForPurchase;
		}

		if ( ArmorValue() >= 100 )
		{
			return AcquireResult::AlreadyOwned;
		}
	}
	else if ( weaponId == ITEM_ASSAULTSUIT )
	{
		if ( mp_free_armor.GetBool() )
		{
			if ( acquireMethod == AcquireMethod::Buy )
				return AcquireResult::NotAllowedForPurchase;
		}

		if ( m_bHasHelmet && !m_bHasHeavyArmor /*&& ArmorValue() >= 100*/ )
		{
			return AcquireResult::AlreadyOwned;
		}
	}
	else if ( weaponId == ITEM_HEAVYASSAULTSUIT )
	{
		if ( m_bHasHeavyArmor && ArmorValue() >= 200 )
		{
			return AcquireResult::AlreadyOwned;
		}
	}
	else if ( weaponId == ITEM_DEFUSER || weaponId == ITEM_CUTTERS )
	{
		if ( CSGameRules() && (CSGameRules()->IsPlayingGunGameDeathmatch() || CSGameRules()->IsPlayingCoopGuardian()) )
		{
			if ( acquireMethod == AcquireMethod::Buy )
				return AcquireResult::NotAllowedForPurchase;
		}

		if ( m_bHasDefuser )
			return AcquireResult::AlreadyOwned;
	}
	else if ( weaponId == WEAPON_C4 )
	{
		// TODO[pmf]: Data drive this from the scripts
		if ( acquireMethod == AcquireMethod::Buy )
			return AcquireResult::NotAllowedForPurchase;
	}
	else if ( CSWeapon_OwnsThisType( pItem ) )	
	{
		return AcquireResult::AlreadyOwned;
	}

	// special case for limiting taser to classic casual; data drive this if it becomes more complex
	if ( weaponId == WEAPON_TASER )
	{
		if ( !mp_weapons_allow_zeus.GetBool() )
			return AcquireResult::NotAllowedForPurchase;
		else if ( ( mp_weapons_allow_zeus.GetInt() > 0 ) && ( m_iWeaponPurchasesThisRound[ weaponId ] >= mp_weapons_allow_zeus.GetInt() ) )
			return AcquireResult::AlreadyPurchased;
		else
			return AcquireResult::Allowed;
	}

	// additional constraints for purchasing weapons
	if ( acquireMethod == AcquireMethod::Buy )
	{
		if ( pWeaponInfo->GetUsedByTeam( pItem ) != TEAM_UNASSIGNED && GetTeamNumber() != pWeaponInfo->GetUsedByTeam( pItem ) )
		{
			return AcquireResult::NotAllowedByTeam;
		}

		// special case for flashbangs - no limit
		if ( weaponId == WEAPON_FLASHBANG )
		{
			return AcquireResult::Allowed;
		}

		// don't allow purchasing multiple grenades of a given type per round (even if the player throws the purchased one)
		if ( pWeaponInfo->GetWeaponType() == WEAPONTYPE_GRENADE && !CSGameRules()->IsPlayingCooperativeGametype() )
		{
			// limit the number of purchases to one more than the number we are allowed to carry
			int carryLimitThisGrenade = GetAmmoDef()->MaxCarry( pWeaponInfo->GetPrimaryAmmoType(), this ); // We still use player-stored ammo for grenades.

			// for smoke grenade, we are only allow to buy exactly the amount we are allowed to carry, with other weapons, we can purchase one more than what we can carry per round
			if ( weaponId == WEAPON_SMOKEGRENADE && carryLimitThisGrenade > 0 )
				carryLimitThisGrenade--;

			if ( m_iWeaponPurchasesThisRound[weaponId] > carryLimitThisGrenade )
				return AcquireResult::AlreadyPurchased;
		}

		if ( weaponId == WEAPON_KNIFE || weaponId == WEAPON_KNIFE_GG )
		{ 
			return AcquireResult::NotAllowedForPurchase; 
		}

		// Validate that each player can buy only so many instances of same weapon type
		if ( mp_weapons_allow_typecount.GetInt() == 0 )
			return AcquireResult::NotAllowedForPurchase;
		else if ( ( mp_weapons_allow_typecount.GetInt() > 0 ) && ( m_iWeaponPurchasesThisRound[weaponId] >= mp_weapons_allow_typecount.GetInt() ) )
			return AcquireResult::AlreadyPurchased;
	}

	return AcquireResult::Allowed;
}

//-----------------------------------------------------------------------------
// Purpose: Returns weapon if already owns a weapon of this class
//-----------------------------------------------------------------------------
CWeaponCSBase* CCSPlayer::CSWeapon_OwnsThisType( CEconItemView *pItem ) const
{
	/** Removed for partner depot **/
	return nullptr;
}

//************************************
// Determine the current cash cost of a weapon for this particular player
// Parameter: CSWeaponID weaponId
//************************************
int CCSPlayer::GetWeaponPrice( CSWeaponID weaponId, const CEconItemView *pWepView ) const
{

	if ( !pWepView && ( weaponId == WEAPON_NONE ) )
		return -1;
		
	bool bHasFullArmor = (ArmorValue() >= 100);

	// special case handling for reduced cost of Kevlar + helmet
	if ( weaponId == ITEM_ASSAULTSUIT )
	{
		int iCost = ITEM_PRICE_ASSAULTSUIT;
		if ( bHasFullArmor && !m_bHasHelmet )
			iCost -= ITEM_PRICE_KEVLAR;
		//else if ( m_bHasHelmet )
		//	iCost = ITEM_PRICE_KEVLAR;

		return iCost;
	}

	// special case handling for reduced cost of Kevlar
	if ( weaponId == ITEM_KEVLAR )
	{
		int iCost = ITEM_PRICE_KEVLAR;

		return iCost;
	}
	// special case handling for reduced cost of Kevlar
	if ( weaponId == ITEM_HEAVYASSAULTSUIT )
	{
		int iCost = ITEM_PRICE_HEAVYASSAULTSUIT;

		return iCost;
	}

//	if ( weaponId == ITEM_EXOSUIT )
//	{
//		int iCost = ITEM_PRICE_EXOSUIT;
//
//		return iCost;
//	}

	const CCSWeaponInfo* pWeaponInfo = GetWeaponInfo( weaponId );
	return ( pWeaponInfo ) ? pWeaponInfo->GetWeaponPrice( pWepView ) : 0;
}

bool CCSPlayer::HasWeaponOfType( int nWeaponID ) const
{
	for ( int i = 0; i < WeaponCount(); ++i )
	{		
		CWeaponCSBase *pWeapon = dynamic_cast< CWeaponCSBase* > ( GetWeapon( i ) );

		if ( pWeapon && pWeapon->GetCSWeaponID() == nWeaponID )
		{
			return true;
		}
	}

	return false;
}

CEconItemView *CCSPlayer::GetEquippedItemInLoadoutSlotOrBaseItem( int iLoadoutSlot )
{
	CEconItemView *pBaseItem = CSInventoryManager()->GetBaseItemForTeam( GetTeamNumber(), iLoadoutSlot );

	bool bRandomCosmetics = false;

	// Bots and controlled bots always return the base item unless we're randomizing
	if ( !bRandomCosmetics && ( IsBot() || IsControllingBot() ) )
		return pBaseItem;

	CEconItemView *pResult = Inventory()->GetInventoryItemByItemID( m_EquippedLoadoutItemIndices[iLoadoutSlot] );
	if ( !pResult || !pResult->IsValid() )
	{
		pResult = pBaseItem;
	}

	return pResult;
}

bool CCSPlayer::UpdateDispatchLayer( CAnimationLayer *pLayer, CStudioHdr *pWeaponStudioHdr, int iSequence )
{
	if ( !pWeaponStudioHdr || !pLayer )
	{
		if ( pLayer )
			pLayer->m_nDispatchedDst = ACT_INVALID;
		return false;
	}	

	if ( pLayer->m_pDispatchedStudioHdr != pWeaponStudioHdr || pLayer->m_nDispatchedSrc != iSequence || pLayer->m_nDispatchedDst >= pWeaponStudioHdr->GetNumSeq() )
	{
		pLayer->m_pDispatchedStudioHdr = pWeaponStudioHdr;
		pLayer->m_nDispatchedSrc = iSequence;
		if ( pWeaponStudioHdr )
		{
			const char *pszSeqName = GetSequenceName( iSequence );
			
#ifdef DEBUG
			if ( V_stristr( pszSeqName, "default" ) )
			{
				contract_assert( false && "Warning: weapon is attempting to play its default sequence as a dispatched anim.\n" );
			}
#endif

			// check if the weapon has a CT or T specific version of this sequence (denoted by a _t or ct suffix)
			if ( GetTeamNumber() == TEAM_TERRORIST )
			{
				char pszLayerNameT[128];
				V_sprintf_safe( pszLayerNameT, "%s_t", pszSeqName );
				int nTeamSpecificSequenceIndex = pWeaponStudioHdr->LookupSequence( pszLayerNameT );
				if ( nTeamSpecificSequenceIndex > 0 )
				{
					pLayer->m_nDispatchedDst = nTeamSpecificSequenceIndex;
					return true;
				}
			}

			pLayer->m_nDispatchedDst = pWeaponStudioHdr->LookupSequence( pszSeqName );
		}
		else
		{
			pLayer->m_nDispatchedDst = ACT_INVALID;
		}
	}
	return (pLayer->m_nDispatchedDst > 0 );
}

bool CCSPlayer::UpdateLayerWeaponDispatch( CAnimationLayer *pLayer, int iSequence )
{
	CBaseCombatWeapon *pWeapon = GetActiveWeapon();
	if ( pWeapon )
	{
		CBaseWeaponWorldModel *pWeaponWorldModel = pWeapon->GetWeaponWorldModel();
		if ( pWeaponWorldModel )
		{
			return UpdateDispatchLayer( pLayer, pWeaponWorldModel->GetModelPtr(), iSequence );
		}
	}
	return UpdateDispatchLayer( pLayer, nullptr, iSequence );
}

float CCSPlayer::GetLayerSequenceCycleRate( CAnimationLayer *pLayer, int iSequence ) 
{ 
	UpdateLayerWeaponDispatch( pLayer, iSequence );
	if ( pLayer->m_nDispatchedDst != ACT_INVALID )
	{
		// weapon world model overrides rate
		return GetSequenceCycleRate( pLayer->m_pDispatchedStudioHdr, pLayer->m_nDispatchedDst );
	}
	return BaseClass::GetLayerSequenceCycleRate( pLayer, iSequence );
}

//--------------------------------------------------------------------------------------------------------------

static constexpr auto MATERIAL_NAME_LENGTH = 16;

class CFootstepControl : public CBaseTrigger
{
	[[= ks::reflect::NetTable{ .name = "DT_FootstepControl" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CFootstepControl, CBaseTrigger );
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	 int UpdateTransmitState_Impl( void );
	 void Spawn_Impl( void );

	CNetworkVar( string_t, m_source, [[= ks::reflect::Net{} ]] [[= ks::reflect::Key{ .name = "Source" } ]] );
	CNetworkVar( string_t, m_destination, [[= ks::reflect::Net{} ]] [[= ks::reflect::Key{ .name = "Destination" } ]] );
};

LINK_ENTITY_TO_CLASS( func_footstep_control, CFootstepControl );

IMPLEMENT_REFLECT_DATAMAP( CFootstepControl )

IMPLEMENT_REFLECT_SERVERCLASS( CFootstepControl, DT_FootstepControl )

int CFootstepControl::UpdateTransmitState_Impl( void )
{
	return SetTransmitState( FL_EDICT_ALWAYS );
}

void CFootstepControl::Spawn_Impl( void )
{
	InitTrigger();
}

#undef CS_MASK_SHOOT
