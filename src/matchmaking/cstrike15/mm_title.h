#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

#include "../mm_framework.h"

class CMatchTitle :
	public IMatchTitle,
	public IMatchEventsSink,
	public IGameEventListener2
{
	// Methods exposed for the framework
public:
	virtual InitReturnVal_t Init();
	virtual void Shutdown();

	// Methods of IMatchTitle
public:
	// Title ID
	virtual uint64 GetTitleID();

	// Service ID for XLSP
	virtual uint64 GetTitleServiceID();

	// Describe title settings using a bitwise combination of flags
	virtual uint64 GetTitleSettingsFlags();

	// Prepare network startup params for the title
	virtual void PrepareNetStartupParams( void *pNetStartupParams );

	// Get total number of players supported by the title
	virtual int GetTotalNumPlayersSupported();

	// Get a guest player name
	virtual char const * GetGuestPlayerName( int iUserIndex );

	// Decipher title data fields
	virtual TitleDataFieldsDescription_t const * DescribeTitleDataStorage();

	// Title achievements
	virtual TitleAchievementsDescription_t const * DescribeTitleAchievements();

	// Title avatar awards
	virtual TitleAvatarAwardsDescription_t const * DescribeTitleAvatarAwards();

	// Title DLC description
	virtual TitleDlcDescription_t const * DescribeTitleDlcs();

	// Run every frame
	virtual void RunFrame() {}

	// Sets up all necessary client-side convars and user info before
	// connecting to server
	virtual void PrepareClientForConnect( KeyValues *pSettings );

	// Start up a listen server with the given settings
	virtual bool StartServerMap( KeyValues *pSettings );

	// Methods of IMatchEventsSink
public:
	virtual void OnEvent( KeyValues *pEvent );

	// Methods of IGameEventListener2
public:
	// FireEvent is called by EventManager if event just occured
	// KeyValue memory will be freed by manager if not needed anymore
	virtual void FireGameEvent( IGameEvent *event );
	virtual int	 GetEventDebugID( void );

public:
	CMatchTitle();
	~CMatchTitle();
};

// Match title singleton
extern CMatchTitle *g_pMatchTitle;
