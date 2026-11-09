// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: CS's custom C_PlayerResource

#include "rocketui/rmlui.h"
#include "predictable_entity.h"
#include "cdll_util.h"
#include "util_shared.h"
#include "baseentity_shared.h"
#include "c_basecombatweapon.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "cdll_client_int.h"
#include "reflect_recvtable.h"
#include "reflect_annotations.h"
#include "c_cs_playerresource.h"
#include <shareddefs.h>
#include <cs_shareddefs.h>
#include "localize/ilocalize.h"
#include "cs_gamerules.h"
#include "c_cs_player.h"
#include "c_cs_team.h"
#include "cs_item_inventory.h"

IMPLEMENT_REFLECT_CLIENTCLASS( C_CS_PlayerResource, DT_CSPlayerResource, CCSPlayerResource )

ConVar cl_show_playernames_max_chars_console( "cl_show_playernames_max_chars_console", "0", FCVAR_CLIENTDLL | FCVAR_DEVELOPMENTONLY, "Shows all player names (including bots) as 16 W's." );

extern ConVar cl_spec_use_tournament_content_standards;
extern ConVar sv_spec_use_tournament_content_standards;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_CS_PlayerResource::C_CS_PlayerResource()
{
	m_Colors[TEAM_TERRORIST] = COLOR_RED;
	m_Colors[TEAM_CT] = COLOR_BLUE;
	memset( m_iMVPs, 0, sizeof( m_iMVPs ) );
	memset( m_bHasHelmet, 0, sizeof( m_bHasHelmet ) );
	memset( m_bHasDefuser, 0, sizeof( m_bHasDefuser ) );
	memset( m_iArmor, 0, sizeof( m_iArmor ) );	
	memset( m_iScore, 0, sizeof( m_iScore ) );		
	memset( m_iCompetitiveRanking, 0, sizeof( m_iCompetitiveRanking ) );	
	memset( m_iCompetitiveWins, 0, sizeof( m_iCompetitiveWins ) );	
	memset( m_iCompTeammateColor, 0, sizeof( m_iCompTeammateColor ) );
	memset( m_nEndMatchNextMapVotes, 0, sizeof( m_nEndMatchNextMapVotes ) );
	memset( m_nActiveCoinRank, 0, sizeof( m_nActiveCoinRank ) );
	memset( m_nMusicID, 0, sizeof( m_nMusicID ) );
	memset( m_bIsAssassinationTarget, 0, sizeof( m_bIsAssassinationTarget ) );

	memset( m_nPersonaDataPublicLevel, 0, sizeof( m_nPersonaDataPublicLevel ) );
	memset( m_nPersonaDataPublicCommendsLeader, 0, sizeof( m_nPersonaDataPublicCommendsLeader ) );
	memset( m_nPersonaDataPublicCommendsTeacher, 0, sizeof( m_nPersonaDataPublicCommendsTeacher ) );
	memset( m_nPersonaDataPublicCommendsFriendly, 0, sizeof( m_nPersonaDataPublicCommendsFriendly ) );

	m_bDisableAssassinationTargetNameOverride = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_CS_PlayerResource::~C_CS_PlayerResource()
{
}

bool C_CS_PlayerResource::IsVIP(int iIndex )
{
	return m_iPlayerVIP == iIndex;
}

bool C_CS_PlayerResource::HasC4(int iIndex )
{
	return m_iPlayerC4 == iIndex;
}

bool C_CS_PlayerResource::IsHostageAlive(int iIndex)
{
	if ( iIndex < 0 || iIndex >= MAX_HOSTAGES )
		return false;

	return m_bHostageAlive[iIndex];
}

bool C_CS_PlayerResource::IsHostageFollowingSomeone(int iIndex)
{
	if ( iIndex < 0 || iIndex >= MAX_HOSTAGES )
		return false;

	return m_isHostageFollowingSomeone[iIndex];
}

int C_CS_PlayerResource::GetHostageEntityID(int iIndex)
{
	if ( iIndex < 0 || iIndex >= MAX_HOSTAGES )
		return -1;

	return m_iHostageEntityIDs[iIndex];
}

const Vector C_CS_PlayerResource::GetBombsiteAPosition()
{
	if ( CSGameRules() && CSGameRules()->IsBombDefuseMap() == false )
		return vec3_origin;

	return m_bombsiteCenterA;
}

const Vector C_CS_PlayerResource::GetBombsiteBPosition()
{
	if ( CSGameRules() && CSGameRules()->IsBombDefuseMap() == false )
		return vec3_origin;

	return m_bombsiteCenterB;
}

const Vector C_CS_PlayerResource::GetHostageRescuePosition( int iIndex )
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
int C_CS_PlayerResource::GetNumMVPs( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return false;

	return m_iMVPs[iIndex];
} 

//--------------------------------------------------------------------------------------------------------
bool C_CS_PlayerResource::HasDefuser( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return false;

	return m_bHasDefuser[iIndex];
}

//--------------------------------------------------------------------------------------------------------
bool C_CS_PlayerResource::HasHelmet( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return false;

	return m_bHasHelmet[iIndex];
}

//--------------------------------------------------------------------------------------------------------
int C_CS_PlayerResource::GetArmor( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return 0;

	return m_iArmor[iIndex];
}

//--------------------------------------------------------------------------------------------------------
int C_CS_PlayerResource::GetScore( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return 0;

	return m_iScore[iIndex];
}

//--------------------------------------------------------------------------------------------------------
int C_CS_PlayerResource::GetCompetitiveRanking( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return 0;

	if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
	{
		if ( pParameters->m_bAnonymousPlayerIdentity )
			return 0;
	}

	return m_iCompetitiveRanking[iIndex];
}

//--------------------------------------------------------------------------------------------------------
int C_CS_PlayerResource::GetCompetitiveWins( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return 0;

	if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
	{
		if ( pParameters->m_bAnonymousPlayerIdentity )
			return 0;
	}

	return m_iCompetitiveWins[iIndex];
}

//--------------------------------------------------------------------------------------------------------
int C_CS_PlayerResource::GetCompTeammateColor( int iIndex )
{
	if ( !IsConnected( iIndex ) || !CSGameRules( ) )
		return -1;

	if ( !CSGameRules( )->IsPlayingAnyCompetitiveStrictRuleset( ) )
		return -1;

	if ( IsFakePlayer( iIndex ) )
		return -2;

	return m_iCompTeammateColor[iIndex];
}

//--------------------------------------------------------------------------------------------------------
int C_CS_PlayerResource::GetTotalCashSpent( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return 0;

	return m_iTotalCashSpent[iIndex];
}

//--------------------------------------------------------------------------------------------------------
int C_CS_PlayerResource::GetCashSpentThisRound( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return 0;

	return m_iCashSpentThisRound[iIndex];
}

//--------------------------------------------------------------------------------------------------------
int C_CS_PlayerResource::GetEndMatchNextMapVote( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return -1;

	return m_nEndMatchNextMapVotes[iIndex];
}

int C_CS_PlayerResource::GetActiveCoinRank( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return 0;

	if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
	{
		if ( pParameters->m_bAnonymousPlayerIdentity )
			return 0;
	}

	return m_nActiveCoinRank[iIndex];
}

int C_CS_PlayerResource::GetPersonaDataPublicLevel( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return 0;

	if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
	{
		if ( pParameters->m_bAnonymousPlayerIdentity )
			return 0;
	}

	return m_nPersonaDataPublicLevel[ iIndex ];
}

int C_CS_PlayerResource::GetPersonaDataPublicCommendsLeader( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return 0;

	if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
	{
		if ( pParameters->m_bAnonymousPlayerIdentity )
			return 0;
	}

	return m_nPersonaDataPublicCommendsLeader[ iIndex ];
}

int C_CS_PlayerResource::GetPersonaDataPublicCommendsTeacher( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return 0;

	if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
	{
		if ( pParameters->m_bAnonymousPlayerIdentity )
			return 0;
	}

	return m_nPersonaDataPublicCommendsTeacher[ iIndex ];
}

int C_CS_PlayerResource::GetPersonaDataPublicCommendsFriendly( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return 0;

	if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
	{
		if ( pParameters->m_bAnonymousPlayerIdentity )
			return 0;
	}

	return m_nPersonaDataPublicCommendsFriendly[ iIndex ];
}


int C_CS_PlayerResource::GetMusicID( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return 0;

	if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
	{
		if ( pParameters->m_bAnonymousPlayerIdentity )
			return 0;
	}

	return m_nMusicID[ iIndex ];
}



void C_CS_PlayerResource::OnDataChanged(DataUpdateType_t updateType)
{
	BaseClass::OnDataChanged( updateType );

}

bool C_CS_PlayerResource::EndMatchNextMapAllVoted( void )
{
	return m_bEndMatchNextMapAllVoted;
}

const char *C_CS_PlayerResource::GetClanTag( int iIndex )
{
	if ( iIndex < 1 || iIndex > MAX_PLAYERS )
	{
		contract_assert( false );
		return "";
	}

	if ( !IsConnected( iIndex ) )
		return "";

	if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
	{
		if ( pParameters->m_bAnonymousPlayerIdentity )
			return "";
	}

	return m_szClan[iIndex];
}

bool C_CS_PlayerResource::IsControllingBot( int index )
{
	return m_bControllingBot[ index ];
}

int C_CS_PlayerResource::GetControlledPlayer( int index )
{
	return m_iControlledPlayer[ index ];
}

int C_CS_PlayerResource::GetControlledByPlayer( int index )
{
	return m_iControlledByPlayer[ index ];
}

bool C_CS_PlayerResource::IsAssassinationTarget( int index )
{
	if ( m_bDisableAssassinationTargetNameOverride )
		return false;

	return m_bIsAssassinationTarget[ index ];
}


C_CS_PlayerResource * GetCSResources( void )
{
	return ( C_CS_PlayerResource* ) g_PR;
}

const char *C_CS_PlayerResource::GetPlayerName( int index )
{
	if ( cl_show_playernames_max_chars_console.GetBool() )
		return "WWWWWWWWWWWWWWWW";

	CEconQuestDefinition *pQuestDef = CSGameRules()->GetActiveAssassinationQuest();
	if ( IsAssassinationTarget( index ) && pQuestDef )
	{
		static char szAssassinationTargetName[ MAX_PLAYER_NAME_LENGTH ];
		const char* szLocToken = Helper_GetLocalPlayerAssassinationQuestLocToken( pQuestDef );
		if ( szLocToken )
		{
			V_UnicodeToUTF8( g_pLocalize->Find( szLocToken ), szAssassinationTargetName, MAX_PLAYER_NAME_LENGTH );
			return szAssassinationTargetName;
		}
	}

	return BaseClass::GetPlayerName( index );
}

void C_CS_PlayerResource::UpdatePlayerName( int slot )
{
	if ( slot < 1 || slot > MAX_PLAYERS )
	{
		Error( "UpdatePlayerName with bogus slot %d\n", slot );
		return;
	}

	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer( );

	char const *pchPlayerName = nullptr;
	player_info_t sPlayerInfo;

	bool bUseTournamentContentStandards = cl_spec_use_tournament_content_standards.GetBool( ) || sv_spec_use_tournament_content_standards.GetBool( );
	// is this spectator insisting to use official pro player names?

	bool bGetProPlayerName = bUseTournamentContentStandards;

	if ( IsConnected( slot ) && ( pLocalPlayer->IsSpectator( ) || pLocalPlayer->IsHLTV( ) ) && bGetProPlayerName )
	{
		CProPlayerData const *pProPlayerData = pProPlayerData = GEconItemSchema( ).GetProPlayerDataByAccountID( GetXuid( index ) );

		if ( pProPlayerData )
		{
			pchPlayerName = pProPlayerData->GetName( );
		}
	}

	// not a pro or we don't care.
	 if ( !pchPlayerName && 
		 IsConnected( slot ) &&
		engine->GetPlayerInfo( slot, &sPlayerInfo ) )
	{
		pchPlayerName = sPlayerInfo.name;

		if ( sPlayerInfo.fakeplayer && pchPlayerName && *pchPlayerName )
		{	// also avoids censorship in pre-localized names
			UpdateAsLocalizedFakePlayerName( slot, pchPlayerName );
			return;
		}
	}

	if ( !pchPlayerName )
		pchPlayerName = PLAYER_UNCONNECTED_NAME;

	if ( !m_szName[ slot ] || V_stricmp( m_szName[ slot ], pchPlayerName ) )
	{
		m_szName[ slot ] = AllocPooledString( pchPlayerName );
	}
}

int C_CS_PlayerResource::GetBotDifficulty( int index )
{
	return m_iBotDifficulty[index];
}

const wchar_t* C_CS_PlayerResource::GetDecoratedPlayerName( int index, wchar_t* buffer, int buffsize, EDecoratedPlayerNameFlag_t flags )
{
	if ( IsConnected( index ) )
	{
		bool addBotToNameIfControllingBot = !!(flags & k_EDecoratedPlayerNameFlag_AddBotToNameIfControllingBot);
		bool useNameOfControllingPlayer = !(flags & k_EDecoratedPlayerNameFlag_DontUseNameOfControllingPlayer);
		bool bShowClanName = !(flags & k_EDecoratedPlayerNameFlag_DontShowClanName);
		bool bMakeStringSafe = !(flags & k_EDecoratedPlayerNameFlag_DontMakeStringSafe);
		bool bSkipAssassinationTargetName = !!(flags & k_EDecoratedPlayerNameFlag_DontUseAssassinationTargetName); 

		int nameIndex = index;
		int controlledBy = GetControlledByPlayer( index );
		int nBotControlStringType = 0; // normal name

		if( controlledBy && useNameOfControllingPlayer )
		{
			nBotControlStringType = 1;// BOT ( name )
			nameIndex = controlledBy;

			// HACK: We want to have a fake name for this player if they're our target
			// but we want to show the real player name if they're controlling a bot AND show the fake name in the scoreboard
			// for their dead player entry. 
			contract_assert( m_bDisableAssassinationTargetNameOverride == false );
			m_bDisableAssassinationTargetNameOverride = true;
		}
		else if ( IsFakePlayer( index ) && !cl_show_playernames_max_chars_console.GetBool() && CSGameRules() && !CSGameRules()->IsPlayingCooperativeGametype() )
		{
			nBotControlStringType = 2; // BOT name
		}
		else if ( IsControllingBot( index ) && addBotToNameIfControllingBot )
		{
			nBotControlStringType = 1; // BOT ( name )
			contract_assert( m_bDisableAssassinationTargetNameOverride == false );
			m_bDisableAssassinationTargetNameOverride = true;
		}


		wchar_t wide_name[MAX_PLAYER_NAME_LENGTH];
		wide_name[0] = L'\0';
		char nameBuf[MAX_PLAYER_NAME_LENGTH] = {0};

		if ( nBotControlStringType == 2 && GetBotDifficulty( index ) >= 0 )
		{
			ConVarRef sv_show_bot_difficulty_in_name( "sv_show_bot_difficulty_in_name" );

			if ( sv_show_bot_difficulty_in_name.GetBool() )
			{
				// Show bot difficulty as part of the bot name
				V_snprintf( nameBuf, std::size( nameBuf ) - 1, "%s[%d]", GetPlayerName( nameIndex ), GetBotDifficulty( index ) );
			}
			else
			{
				// Hide bot difficulty
				V_snprintf( nameBuf, std::size( nameBuf ) - 1, "%s", GetPlayerName( nameIndex ) );
			}
		}
		else if ( GetCoachingTeam( index ) != 0 )
		{
			char const *szCoach = nullptr;
			switch( GetCoachingTeam( index ) )
			{
			case TEAM_TERRORIST:
				szCoach = "#SFUI_coach_name_t";
				break;
			case TEAM_CT:
				szCoach = "#SFUI_coach_name_ct";
				break;
			}

			if ( szCoach )
			{
				V_strcpy_safe( nameBuf, g_pLocalize->FindAsUTF8( szCoach ) );
				V_strcat_safe( nameBuf, " " );
			}
			V_strcat_safe( nameBuf, GetPlayerName( nameIndex ) );
		}
		else
		{
			//wchar_t wszClanTag[ MAX_PLAYER_NAME_LENGTH ];
			char szClan[MAX_PLAYER_NAME_LENGTH];
			if ( bShowClanName && V_strlen( GetClanTag( index ) ) > 1 )
			{
				const char* optionalSpace = "";
				if ( GetClanTag( index )[0] == '#' )
				{
					optionalSpace = " ";
				}
				V_snprintf( szClan, sizeof( szClan ), "%s%s ", optionalSpace, GetClanTag( index ) );
			}
			else
			{
				szClan[0] = 0;
			}
			//g_pLocalize->ConvertANSIToUnicode( szClan, wszClanTag, sizeof( wszClanTag ) );

			V_snprintf( nameBuf, std::size( nameBuf ) - 1, "%s%s", szClan, GetPlayerName( nameIndex ) );
		}

		V_UTF8ToUnicode( nameBuf /*GetPlayerName( nameIndex )*/, wide_name, sizeof( wide_name ) );

		wchar_t *pSafeWideName = wide_name;

		if ( !nBotControlStringType ) // normal name
		{
			if ( IsAssassinationTarget( index ) && !bSkipAssassinationTargetName )
			{
				Helper_GetDecoratedAssassinationTargetName( CSGameRules()->GetActiveAssassinationQuest(), buffer, buffsize/sizeof(wchar_t) );
			}
			else
			{
				V_wcsncpy( buffer, pSafeWideName, buffsize );
			}
		}
		else
		{
			const char* translationID = ( nBotControlStringType == 1 ) ? "#SFUI_bot_controlled_by" : "#SFUI_bot_decorated_name";
			g_pLocalize->ConstructString( buffer, buffsize, g_pLocalize->Find( translationID ), 1, pSafeWideName );
		}
	}
	else
	{
		*buffer = L'\0';
	}

	m_bDisableAssassinationTargetNameOverride = false;

	return buffer;
}
