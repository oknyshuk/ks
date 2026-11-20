#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

#include "../mm_framework.h"
import ks.core.threadtools;
import ks.core.types;
import ks.tier1.iappsystem;
import ks.tier1.keyvalues;

class CMatchTitle : public IMatchTitle
{
	// Methods exposed for the framework
public:
	InitReturnVal_t Init();
	void Shutdown();

	// Methods of IMatchTitle
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

	// Title DLC description
	TitleDlcDescription_t const * DescribeTitleDlcs();

	// Run every frame
	void RunFrame() {}

	// Sets up all necessary client-side convars and user info before
	// connecting to server
	void PrepareClientForConnect( KeyValues *pSettings );

	// Start up a listen server with the given settings
	bool StartServerMap( KeyValues *pSettings );

	// Match events
public:
	void OnEvent( KeyValues *pEvent );

	// Game event listener
public:
	// FireEvent is called by EventManager if event just occured
	// KeyValue memory will be freed by manager if not needed anymore
	void FireGameEvent( IGameEvent *event );

public:
	CMatchTitle();
	~CMatchTitle();
};

// Match title singleton
extern CMatchTitle *g_pMatchTitle;
