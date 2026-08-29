//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "mm_title.h"
#include "matchmaking/cstrike15/imatchext_cstrike15.h"
#include "inputsystem/iinputsystem.h"
#include "platforminputdevice.h"
#include "netmessages_signon.h"

#include "steam/isteamuserstats.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static ConVar cl_titledataversionblock1( "cl_titledataversionblock1", "14", FCVAR_DEVELOPMENTONLY, "stats for console title data block1 i/o version." );
static ConVar cl_titledataversionblock2( "cl_titledataversionblock2", "8", FCVAR_DEVELOPMENTONLY, "stats for console title data block2 i/o version." );
static ConVar cl_titledataversionblock3( "cl_titledataversionblock3", "48", FCVAR_DEVELOPMENTONLY, "stats for console title data block3 i/o version." );

static TitleDataFieldsDescription_t const * PrepareTitleDataStorageDescription()
{


#define TD_ENTRY( szName, nTD, eDataType, numBytesOffset ) \
	{ \
	TitleDataFieldsDescription_t aTDFD = { szName, TitleDataFieldsDescription_t::nTD, TitleDataFieldsDescription_t::eDataType, numBytesOffset }; \
	s_tdfd.AddToTail( aTDFD ); \
}


	static CUtlVector< TitleDataFieldsDescription_t > s_tdfd;




	// END MARKER
	TD_ENTRY( (const char*) NULL, DB_TD3, DT_0, 0 )

#undef TD_ENTRY


	return s_tdfd.Base();
}

TitleDataFieldsDescription_t const * CMatchTitle::DescribeTitleDataStorage()
{
	static TitleDataFieldsDescription_t const *s_pTDFD = PrepareTitleDataStorageDescription();
	return s_pTDFD;
}

TitleAchievementsDescription_t const * CMatchTitle::DescribeTitleAchievements()
{
	static TitleAchievementsDescription_t tad[] =
	{
//#include "left4dead2.xhelp.achtitledesc.txt"
		// END MARKER
		{ NULL, 0 }
	};

	return tad;
}

TitleAvatarAwardsDescription_t const * CMatchTitle::DescribeTitleAvatarAwards()
{
	static TitleAvatarAwardsDescription_t taad[] =
	{
//#include "left4dead2.xhelp.avawtitledesc.txt"
		// END MARKER
		{ NULL, 0 }
	};

	return taad;
}

TitleDlcDescription_t const * CMatchTitle::DescribeTitleDlcs()
{
	static TitleDlcDescription_t tdlcs[] =
	{
		//{ PORTAL2_DLCID_COOP_BOT_SKINS,		PORTAL2_DLC_APPID_COOP_BOT_SKINS,	PORTAL2_DLC_PKGID_COOP_BOT_SKINS,	"DLC.0x12" },
		//{ PORTAL2_DLCID_COOP_BOT_HELMETS,	PORTAL2_DLC_APPID_COOP_BOT_HELMETS,	PORTAL2_DLC_PKGID_COOP_BOT_HELMETS,	"DLC.0x13" },
		//{ PORTAL2_DLCID_COOP_BOT_ANTENNA,	PORTAL2_DLC_APPID_COOP_BOT_ANTENNA,	PORTAL2_DLC_PKGID_COOP_BOT_ANTENNA,	"DLC.0x14" },
		// END MARKER
		{ 0, 0, 0 }
	};

	return tdlcs;
}

// Title leaderboards
KeyValues * CMatchTitle::DescribeTitleLeaderboard( char const *szLeaderboardView )
{
	if ( StringAfterPrefix( szLeaderboardView, "WINS_" ) )
	{
		KeyValues *pSettings = KeyValues::FromString( "SteamLeaderboard",
			" :score wins_ratio "								// :score is the leaderboard value mapped to game name "besttime"
			" :payloadformat { "										// This describes the payload format.
				" payload0 { "
					" :score total_wins"
					" :format int "
					" :upload sum "
				" } "
				" payload1 { "
					" :score total_losses "
					" :format int "
					" :upload sum "
				" } "
				" payload2 { "
					" :score win_as_ct "
					" :format int "
					" :upload sum "
				" } "
				" payload3 { "
					" :score win_as_t "
					" :format int "
					" :upload sum "
				" } "
				" payload4 { "
					" :score loss_as_ct "
					" :format int "
					" :upload sum "
				" } "
				" payload5 { "
					" :score loss_as_t "
					" :format int "
					" :upload sum "
				" } "
			" } "
			);

		pSettings->SetString( ":scoreformula", "( payload0 / max( payload0 + payload1, 1 ) ) * ( min( payload0 + payload1, 20 ) / 20 ) * 10000000" );
		pSettings->SetInt( ":sort", k_ELeaderboardSortMethodDescending );			// Sort order when fetching and displaying leaderboard data
		pSettings->SetInt( ":format", k_ELeaderboardDisplayTypeNumeric );	// Note: this is actually 1/100th seconds type, Steam change pending
		pSettings->SetInt( ":upload", k_ELeaderboardUploadScoreMethodForceUpdate );	// Upload method when writing to leaderboard

		return pSettings;
	}
	else if ( StringAfterPrefix( szLeaderboardView, "CS_" ) )
	{
		KeyValues *pSettings = KeyValues::FromString( "SteamLeaderboard",
			" :score average_contribution "								// :score is the leaderboard value mapped to game name "besttime"
			" :payloadformat { "										// This describes the payload format.
				" payload0 { "
					" :score mvp_awards"
					" :format int "
					" :upload sum "
				" } "
				" payload1 { "
					" :score rounds_played "
					" :format int "
					" :upload sum "
				" } "
				" payload2 { "
					" :score total_contribution "
					" :format int "
					" :upload sum "
				" } "
				" payload3 { "
					" :score kills "
					" :format int "
					" :upload sum "
				" } "
				" payload4 { "
					" :score deaths "
					" :format int "
					" :upload sum "
				" } "
				" payload5 { "
					" :score damage "
					" :format int "
					" :upload sum "
				" } "
			" } "
			);

		pSettings->SetString( ":scoreformula", "( payload2 /  max( payload1, 1 ) )" );
		pSettings->SetInt( ":sort", k_ELeaderboardSortMethodDescending );			// Sort order when fetching and displaying leaderboard data
		pSettings->SetInt( ":format", k_ELeaderboardDisplayTypeNumeric );	// Note: this is actually 1/100th seconds type, Steam change pending
		pSettings->SetInt( ":upload", k_ELeaderboardUploadScoreMethodForceUpdate );	// Upload method when writing to leaderboard

		return pSettings;
	}
	else if ( StringAfterPrefix( szLeaderboardView, "KD_" ) )
	{
		KeyValues *pSettings = KeyValues::FromString( "SteamLeaderboard",
			" :score kd_ratio "														// :score is the leaderboard value mapped to game name "besttime"
			" :payloadformat { "													// This describes the payload format.
				" payload0 { "
					" :score kills"
					" :format int "
					" :upload sum "
				" } "
				" payload1 { "
					" :score deaths "
					" :format int "
					" :upload sum "
				" } "
				" payload2 { "
					" :score rounds_played "
					" :format int "
					" :upload sum "
				" } "
				" payload3 { "
					" :score shots_fired "
					" :format int "
					" :upload sum "
				" } "
				" payload4 { "
					" :score head_shots "
					" :format int "
					" :upload sum "
				" } "
				" payload5 { "
					" :score shots_hit "
					" :format int "
					" :upload sum "
				" } "
			" } "
			);

		pSettings->SetString( ":scoreformula", "( payload0 / max( payload1, 1 ) ) * ( min( payload2, 20 ) / 20 ) * 10000000" );
		pSettings->SetInt( ":sort", k_ELeaderboardSortMethodDescending );			// Sort order when fetching and displaying leaderboard data
		pSettings->SetInt( ":format", k_ELeaderboardDisplayTypeNumeric );	// Note: this is actually 1/100th seconds type, Steam change pending
		pSettings->SetInt( ":upload", k_ELeaderboardUploadScoreMethodForceUpdate );	// Upload method when writing to leaderboard

		return pSettings;
	}
	else if ( StringAfterPrefix( szLeaderboardView, "STARS_" ) )
	{
		KeyValues *pSettings = KeyValues::FromString( "SteamLeaderboard",
			" :score numstars "														// :score is the leaderboard value mapped to game name "besttime"
			" :scoresum 1 "
			" :payloadformat { "													// This describes the payload format.
				" payload0 { "
					" :score bombs_planted "
					" :format int "
					" :upload sum "
				" } "
				" payload1 { "
					" :score bombs_detonated "
					" :format int "
					" :upload sum "
				" } "
				" payload2 { "
					" :score bombs_defused "
					" :format int "
					" :upload sum "
				" } "
				" payload3 { "
					" :score hostages_rescued "
					" :format int "
					" :upload sum "
				" } "
			" } "
			);

		pSettings->SetInt( ":sort", k_ELeaderboardSortMethodDescending );			// Sort order when fetching and displaying leaderboard data
		pSettings->SetInt( ":format", k_ELeaderboardDisplayTypeNumeric );	// Note: this is actually 1/100th seconds type, Steam change pending
		pSettings->SetInt( ":upload", k_ELeaderboardUploadScoreMethodKeepBest );	// Upload method when writing to leaderboard

		return pSettings;
	}
	else if ( StringAfterPrefix( szLeaderboardView, "GP_" ) )
	{
		KeyValues *pSettings = KeyValues::FromString( "SteamLeaderboard",
			" :score num_rounds "														// :score is the leaderboard value mapped to game name "besttime"
			" :scoresum 1 "
			" :payloadformat { "													// This describes the payload format.
				" payload0 { "
					" :score time_played "
					" :format uint64 "
					" :upload sum "
				" } "
				" payload1 { "
					" :score time_played_ct "
					" :format uint64 "
					" :upload sum "
				" } "
				" payload2 { "
					" :score time_played_t "
					" :format uint64 "
					" :upload sum "
				" } "
				" payload3 { "
					" :score total_medals "
					" :format int "
					" :upload last "  // the last value written is the authoritative value of total achievement medals unlocked
				" } "
			" } "
			);

		pSettings->SetInt( ":sort", k_ELeaderboardSortMethodDescending );			// Sort order when fetching and displaying leaderboard data
		pSettings->SetInt( ":format", k_ELeaderboardDisplayTypeNumeric );	// Note: this is actually 1/100th seconds type, Steam change pending
		pSettings->SetInt( ":upload", k_ELeaderboardUploadScoreMethodKeepBest );	// Upload method when writing to leaderboard

		return pSettings;
	}


/*
	// Check if this is a survival leaderboard
	if ( char const *szSurvivalMap = StringAfterPrefix( szLeaderboardView, "survival_" ) )
	{

		if ( true || false )
		{
			KeyValues *pSettings = KeyValues::FromString( "SteamLeaderboard",
				" :score besttime "														// :score is the leaderboard value mapped to game name "besttime"
				);

			pSettings->SetInt( ":sort", k_ELeaderboardSortMethodDescending );			// Sort order when fetching and displaying leaderboard data
			pSettings->SetInt( ":format", k_ELeaderboardDisplayTypeTimeMilliSeconds );	// Note: this is actually 1/100th seconds type, Steam change pending
			pSettings->SetInt( ":upload", k_ELeaderboardUploadScoreMethodKeepBest );	// Upload method when writing to leaderboard

			return pSettings;
		}
	}
*/

	return NULL;
}

