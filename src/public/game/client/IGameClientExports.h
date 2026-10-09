#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.tier1;

//-----------------------------------------------------------------------------
// Purpose: Exports a set of functions for the GameUI interface to interact with the game client
//-----------------------------------------------------------------------------
class IGameClientExports : public IBaseInterface
{
public:
	// ingame voice manipulation
	virtual bool IsPlayerGameVoiceMuted(int playerIndex) = 0;
	virtual void MutePlayerGameVoice(int playerIndex) = 0;
	virtual void UnmutePlayerGameVoice(int playerIndex) = 0;

	// notification of gameui state changes
	virtual void OnGameUIActivated() = 0;
	virtual void OnGameUIHidden() = 0;

	virtual void DisplayAchievementPanel( ) = 0;
	virtual void ShutdownAchievementPanel( ) = 0;
	virtual int GetAchievementsPanelMinWidth( void ) const = 0;

	// if true, the gameui applies the blur effect
	virtual bool ClientWantsBlurEffect( void ) = 0;
};

inline constexpr const char *GAMECLIENTEXPORTS_INTERFACE_VERSION = "GameClientExports001";
