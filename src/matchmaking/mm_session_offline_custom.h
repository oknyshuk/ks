#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

//
// CMatchSessionOfflineCustom
//
// Implementation of an offline session
// that allows customization before the actual
// game commences (like playing commentary mode
// or playing single-player)
//

#include "mm_session.h"
import ks.core.threadtools;
import ks.core.types;
import ks.tier1.keyvalues;

class CMatchSessionOfflineCustom : public IMatchSessionInternal
{
	// Methods of IMatchSession
public:
	// Get an internal pointer to session system-specific data
	KeyValues * GetSessionSystemData() { return nullptr; }
	
	// Get an internal pointer to session settings
	KeyValues * GetSessionSettings();

	// Update session settings, only changing keys and values need
	// to be passed and they will be updated
	void UpdateSessionSettings( KeyValues *pSettings );

	void UpdateTeamProperties( KeyValues *pTeamProperties );

	uint64 GetSessionID();

	// Issue a session command
	void Command( KeyValues *pCommand );

	// Run a frame update
	void Update();

	// Destroy the session object
	void Destroy();

	// Debug print a session object
	void DebugPrint();

	// Check if another session is joinable
	bool IsAnotherSessionJoinable( char const *pszAnotherSessionInfo ) { return true; }

	// Process event
	void OnEvent( KeyValues *pEvent );

public:
	explicit CMatchSessionOfflineCustom( KeyValues *pSettings );
	~CMatchSessionOfflineCustom();

protected:
	void InitializeGameSettings();

	//
	// Overrides
	//
protected:
	void OnGamePrepareLobbyForGame();

protected:
	KeyValues *m_pSettings;
	KeyValues::AutoDelete m_autodelete_pSettings;

	enum State_t
	{
		STATE_INIT,
		STATE_CONFIG,
		STATE_RUNNING
	};
	State_t m_eState;
	bool m_bExpectingServerReload;
};
