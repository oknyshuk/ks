#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

import ks.core.types;
import ks.platform;
import ks.simd;
import ks.tier1.keyvalues;

inline constexpr const char *TITLE_DATA_PREFIX = "";


struct TitleDataFieldsDescription_t
{
	enum DataType_t
	{
		DT_0		= 0,
		DT_uint8	= 8,
		DT_BITFIELD = 9,
		DT_uint16	= 16,
		DT_uint32	= 32,
		DT_float	= 33,
		DT_uint64	= 64
	};

	enum DataBlock_t
	{
		DB_TD1		= 0,
		DB_TD2		= 1,
		DB_TD3		= 2,

		DB_TD_COUNT  = 3
	};

	char const *m_szFieldName;
	DataBlock_t m_iTitleDataBlock;
	DataType_t m_eDataType;
	union
	{
		int m_numBytesOffset;
		int m_nUserDataValue0;
	};
};

struct TitleAchievementsDescription_t
{
	char const *m_szAchievementName;			// Name by which achievement can be awarded and queried
	int m_idAchievement;						// Achievement ID on the platform
	int m_numComponents;						// Number of achievement component title data bits
};

struct TitleAvatarAwardsDescription_t
{
	char const *m_szAvatarAwardName;			// Name by which avatar award can be awarded and queried
	int m_idAvatarAward;						// Avatar award ID on the platform
	char const *m_szTitleDataBitfieldStatName;	// Name of a bitfield in title data storage representing whether avatar award has been earned
};

struct TitleDlcDescription_t
{
	uint64 m_uiLicenseMaskId;
	int m_idDlcAppId;
	int m_idDlcPackageId;
	char const *m_szTitleDataBitfieldStatName;	// Name of a bitfield in title data storage representing whether dlc has been discovered installed
};

enum TitleSettingsFlags_t
{
	MATCHTITLE_SETTING_MULTIPLAYER		= ( 1 << 0 ),	// Title wants network sockets initialization
	MATCHTITLE_SETTING_NODEDICATED		= ( 1 << 1 ),	// Title doesn't support dedicated servers
	MATCHTITLE_PLAYERMGR_DISABLED		= ( 1 << 2 ),	// Title doesn't need friends presence poll
	MATCHTITLE_SERVERMGR_DISABLED		= ( 1 << 3 ),	// Title doesn't need group servers poll
	MATCHTITLE_INVITE_ONLY_SINGLE_USER	= ( 1 << 4 ),	// Accepted game invite forcefully disables splitscreen (e.g.: 1-on-1 games)
	MATCHTITLE_VOICE_INGAME				= ( 1 << 5 ),	// When in active gameplay lobby system doesn't transmit voice
	MATCHTITLE_XLSPPATCH_SUPPORTED		= ( 1 << 6 ),	// Title supports xlsp patch containers
};

class IMatchTitle
{
public:
	// Title ID
	uint64 GetTitleID();

	// Service ID for XLSP
	uint64 GetTitleServiceID();

	// Describe title settings using a bitwise combination of flags
	uint64 GetTitleSettingsFlags();

	// Prepare network startup params for the title
	void PrepareNetStartupParams( void *pNetStartupParams );

	// Get total number of players supported by the title
	int GetTotalNumPlayersSupported();

	// Get a guest player name
	char const * GetGuestPlayerName( int iUserIndex );

	// Decipher title data fields
	TitleDataFieldsDescription_t const * DescribeTitleDataStorage();

	// Title achievements
	TitleAchievementsDescription_t const * DescribeTitleAchievements();

	// Title avatar awards
	TitleAvatarAwardsDescription_t const * DescribeTitleAvatarAwards();

	// Sets up all necessary client-side convars and user info before
	// connecting to server
	void PrepareClientForConnect( KeyValues *pSettings );

	// Start up a listen server with the given settings
	bool StartServerMap( KeyValues *pSettings );

	// Title DLC description
	TitleDlcDescription_t const * DescribeTitleDlcs();

	// Run every frame
	void RunFrame();
};

//
// Matchmaking title settings extension interface
//

class IMatchTitleGameSettingsMgr
{
public:
	// Extends server game details
	void ExtendServerDetails( KeyValues *pDetails, KeyValues *pRequest );
	
	// Adds the essential part of game details to be broadcast
	void ExtendLobbyDetailsTemplate( KeyValues *pDetails, char const *szReason, KeyValues *pFullSettings );

	// Initializes full game settings from potentially abbreviated game settings
	void InitializeGameSettings( KeyValues *pSettings, char const *szReason );

	// Sets the bspname key given a mapgroup
	void SetBspnameFromMapgroup( KeyValues *pSettings );

	// Extends game settings update packet before it gets merged with
	// session settings and networked to remote clients
	void ExtendGameSettingsUpdateKeys( KeyValues *pSettings, KeyValues *pUpdateDeleteKeys );

	// Executes the command on the session settings, this function on host
	// is allowed to modify Members/Game subkeys and has to fill in modified players KeyValues
	// When running on a remote client "ppPlayersUpdated" is NULL and players cannot
	// be modified
	void ExecuteCommand( KeyValues *pCommand, KeyValues *pSessionSystemData, KeyValues *pSettings, KeyValues **ppPlayersUpdated );

	// Prepares the host lobby for game or adjust settings of new players who
	// join a game in progress, this function is allowed to modify
	// Members/Game subkeys and has to fill in modified players KeyValues
	void PrepareLobbyForGame( KeyValues *pSettings, KeyValues **ppPlayersUpdated );
};
