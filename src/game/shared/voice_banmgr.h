#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "const.h"

// This class manages the (persistent) list of squelched players.
class CVoiceBanMgr
{
public:

				CVoiceBanMgr();
				~CVoiceBanMgr();	

	// Init loads the list of squelched players from disk.
	bool		Init(const char *pGameDir);
	void		Term();

	// Saves the state into voice_squelch.dt.
	void		SaveState(const char *pGameDir);

	bool		GetPlayerBan(char const playerID[SIGNED_GUID_LEN]);
	void		SetPlayerBan(char const playerID[SIGNED_GUID_LEN], bool bSquelch);


protected:

	class BannedPlayer
	{
	public:
		char			m_PlayerID[SIGNED_GUID_LEN];
		BannedPlayer	*m_pPrev, *m_pNext;
	};

	void			Clear();
	BannedPlayer*	InternalFindPlayerSquelch(char const playerID[SIGNED_GUID_LEN]);
	BannedPlayer*	AddBannedPlayer(char const playerID[SIGNED_GUID_LEN]);


protected:

	BannedPlayer	m_PlayerHash[256];
};
