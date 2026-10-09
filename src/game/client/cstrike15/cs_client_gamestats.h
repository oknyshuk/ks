#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "rocketui/rmlui.h"
import std;
#include "GameEventListener.h"
#include "cs_gamestats_shared.h"
#include "matchmaking/cstrike15/imatchext_cstrike15.h"
#include "gametypes.h"
#include "cs_gamerules.h"

struct PlayerStatData_t
{
	wchar_t*    pStatDisplayName;			// Localized display name of the stat
	int         iStatId;					// CSStatType_t enum value of stat
	int32       iStatValue;                 // Value of the stat
};

enum CSSyncStatValueDirection_t
{
	CSSTAT_WRITE_STAT,
	CSSTAT_READ_STAT
};

class CCSClientGameStats : public CAutoGameSystem, public CGameEventListener
{
public:
	CCSClientGameStats();
	virtual void PostInit();
	virtual void LevelShutdownPreEntity();

	int GetStatCount();

	PlayerStatData_t GetStatById(int id, int nUserSlot );

	void		ResetAllStats( int nUserSlot );
	void		ResetMatchStats( void );
	void		UpdateLastMatchStats( void );

	// [jhail] Reset the round stats 
	void		ResetRoundStats( void );

	void		IncrementMatchmakingData( const StatsCollection_t &stats );
	void		UpdateMatchmakingData( void );
	void		ResetMatchmakingData( MatchmakingDataScope mmDataScope );

	const StatsCollection_t&	GetLifetimeStats( int nUserSlot );
	const StatsCollection_t&	GetMatchStats( int nUserSlot );

	// [jhail] Retrieve the per-round stats
	const StatsCollection_t&	GetRoundStats( int nUserSlot );

	bool		MsgFunc_PlayerStatsUpdate( const ks::net::CCSUsrMsg_PlayerStatsUpdate &msg );

	bool ValidateTitleBlockVersion( struct TitleDataFieldsDescription_t const *pFields, class IPlayerLocal *pPlayerLocal, CSSyncStatValueDirection_t eOp, int titleBlockNo );

	bool SyncCSStatsToTitleData( int iController, CSSyncStatValueDirection_t eOp );
	bool SyncCSLoadoutsToTitleData( int iController, CSSyncStatValueDirection_t eOp );
	bool SyncCSMatchmakingDataToTitleData( int iController, CSSyncStatValueDirection_t eOp );
	bool SyncCSRankingDataToTitleData( int iController, CSSyncStatValueDirection_t eOp );



	CUserMessageBinder m_UMCMsgPlayerStatsUpdate;
	CUserMessageBinder m_UMCMsgXRankGet;
	CUserMessageBinder m_UMCMsgXRankUpd;

protected:
	void FireGameEvent( IGameEvent *event );

	void UpdateStats( const StatsCollection_t &stats, int nUserSlot );
	void CalculateMatchFavoriteWeapons();
	
private:
	StatsCollection_t				m_lifetimeStats[MAX_SPLITSCREEN_PLAYERS];
	StatsCollection_t				m_matchStats[MAX_SPLITSCREEN_PLAYERS];
	StatsCollection_t				m_roundStats[MAX_SPLITSCREEN_PLAYERS];

	int							m_matchMaxPlayerCount;
};

extern CCSClientGameStats g_CSClientGameStats;
