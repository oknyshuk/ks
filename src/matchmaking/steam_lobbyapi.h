//===== Copyright � 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#ifndef STEAM_LOBBYAPI_H
#define STEAM_LOBBYAPI_H

#ifdef _WIN32
#pragma once
#endif

#if !defined( NO_STEAM )

void Steam_WriteLeaderboardData( KeyValues *pViewDescription, KeyValues *pViewData );

#endif

// CSteamLobbyObject - stub for Steam lobby functionality
struct CSteamLobbyObject
{
	enum LobbyState_t
	{
		STATE_DEFAULT = 0,
		STATE_ACTIVE_GAME,
		STATE_DISCONNECTED_FROM_STEAM
	};

	uint64 m_uiLobbyID;
	LobbyState_t m_eLobbyState;

	CSteamLobbyObject() : m_uiLobbyID( 0 ), m_eLobbyState( STATE_DEFAULT ) {}

	uint64 GetSessionId() const { return m_uiLobbyID; }
};

#endif

