#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: The CS game stats header

import ks.tier1;
#include "cs_blackmarket.h"
#include "GameStats.h"
#include "cs_gamestats_shared.h"
#include "GameEventListener.h"
#include "weapon_csbase.h"
import std;

// forward declares
class CBreakableProp;

const float cDisseminationTimeHigh = 0.25f;		// Time interval for high priority stats sent to the player
const float cDisseminationTimeLow = 2.5f;	// Time interval for medium priority stats sent to the player

int GetCSLevelIndex( const char *pLevelName );

typedef struct
{
	char szGameName[8];
	byte iVersion;
	char szMapName[32];
	char ipAddr[4];
	short port;
	int serverid;
} gamestats_header_t;

typedef struct
{
	gamestats_header_t	header;
	short	iMinutesPlayed;

	short	iTerroristVictories[CS_NUM_LEVELS];
	short	iCounterTVictories[CS_NUM_LEVELS];
	short	iBlackMarketPurchases[WEAPON_MAX];

	short	iAutoBuyPurchases;
	short	iReBuyPurchases;
	short	iAutoBuyM4A1Purchases;
	short	iAutoBuyAK47Purchases;
	short	iAutoBuyFamasPurchases;
	short	iAutoBuyGalilPurchases;
	short	iAutoBuyGalilARPurchases;
	short	iAutoBuyVestHelmPurchases;
	short	iAutoBuyVestPurchases;

} cs_gamestats_t;



struct WeaponStats
{
	int shots;
	int hits;
	int kills;
	int damage;
};

//=============================================================================
//
// CS Game Stats Class
//
class CCSGameStats : public CBaseGameStats, public CGameEventListener, public CAutoGameSystemPerFrame
{
public:

	// Constructor/Destructor.
	CCSGameStats( void );
	~CCSGameStats( void );

	virtual void Clear( void );
	virtual bool Init();
	virtual void PreClientUpdate();
	
	// Overridden events
	virtual void Event_LevelInit( void );
	virtual void Event_LevelShutdown( float flElapsed );
	virtual void Event_ShotFired( CBasePlayer *pPlayer, CBaseCombatWeapon* pWeapon );
	virtual void Event_ShotHit( CBasePlayer *pPlayer, const CTakeDamageInfo &info );
	virtual void Event_PlayerKilled( CBasePlayer *pPlayer, const CTakeDamageInfo &info );
	virtual void Event_PlayerKilled_PreWeaponDrop( CBasePlayer *pPlayer, const CTakeDamageInfo &info );
	virtual void Event_PlayerConnected( CBasePlayer *pPlayer );
	virtual void Event_PlayerDisconnected( CBasePlayer *pPlayer );
	virtual void Event_WindowShattered( CBasePlayer *pPlayer );
	virtual void Event_PlayerDamage( CBasePlayer *pBasePlayer, const CTakeDamageInfo &info );
	virtual void Event_PlayerKilledOther( CBasePlayer *pAttacker, CBaseEntity *pVictim, const CTakeDamageInfo &info );

	// CSS specific events
    void Event_BombPlanted( CCSPlayer *pPlayer );
    void Event_BombDefused( CCSPlayer *pPlayer );
	void Event_BombExploded( CCSPlayer *pPlayer );
	void Event_MoneyEarned( CCSPlayer *pPlayer, int moneyEarned );
	void Event_MoneySpent( CCSPlayer* pPlayer, int moneySpent, const char *pItemName );
    void Event_HostageRescued( CCSPlayer *pPlayer );
    void Event_PlayerSprayedDecal( CCSPlayer*pPlayer );
	void Event_AllHostagesRescued();
	void Event_BreakProp( CCSPlayer *pPlayer, CBreakableProp *pProp );
	void Event_PlayerDonatedWeapon (CCSPlayer* pPlayer);
	void Event_PlayerDominatedOther( CCSPlayer* pAttacker, CCSPlayer* pVictim);
	void Event_PlayerRevenge( CCSPlayer* pAttacker );
	void Event_PlayerAvengedTeammate( CCSPlayer* pAttacker, CCSPlayer* pAvengedPlayer );
	void Event_MVPEarned( CCSPlayer* pPlayer );	
	void Event_KnifeUse( CCSPlayer* pPlayer, bool bStab, int iDamage );

	virtual void FireGameEvent( IGameEvent *event );

	void UpdatePlayerRoundStats(int winner);
	void DumpMatchWeaponMetrics();

	const PlayerStats_t&		FindPlayerStats( CBasePlayer *pPlayer ) const;
	void						ResetPlayerStats( CBasePlayer *pPlayer );
	void						ResetKillHistory( CBasePlayer *pPlayer );
	void						ResetRoundStats();
	void						ResetPlayerClassMatchStats();

	const StatsCollection_t&	GetTeamStats( int iTeamIndex ) const;
	void						ResetAllTeamStats();
	void						ResetAllStats();
	void						ResetWeaponStats();
 	void						IncrementTeamStat( int iTeamIndex, int iStatIndex, int iAmount );
	void                        CalcDominationAndRevenge( CCSPlayer *pAttacker, CCSPlayer *pVictim, int *piDeathFlags );
    void                        CalculateOverkill(CCSPlayer* pAttacker, CCSPlayer* pVictim);
	void						PlayerKilled( CBasePlayer *pVictim, const CTakeDamageInfo &info );

	void						IncrementStat( CCSPlayer* pPlayer, CSStatType_t statId, int iValue, bool bPlayerOnly = false, bool bIncludeBotController = false );

	void						SendStatsToPlayer( CCSPlayer * pPlayer, int iMinStatPriority );

protected:	
	void						SetStat( CCSPlayer *pPlayer, CSStatType_t statId, int iValue );
	void						TrackKillStats( CCSPlayer *pAttacker, CCSPlayer *pVictim );

private:
	PlayerStats_t				m_aPlayerStats[MAX_PLAYERS+1];	// List of stats for each player for current life - reset after each death    
	StatsCollection_t			m_aTeamStats[TEAM_MAXCOUNT - FIRST_GAME_TEAM];

	float						m_fDisseminationTimerLow;		// how long since last medium priority stat update
	float						m_fDisseminationTimerHigh;		// how long since last high priority stat update

	int							m_numberOfRoundsForDirectAverages;
	int							m_numberOfTerroristEntriesForDirectAverages;
	int							m_numberOfCounterTerroristEntriesForDirectAverages;

	ks::idict< CSStatType_t >	m_PropStatTable;

	WeaponStats					m_weaponStats[WEAPON_MAX][WeaponMode_MAX];
};

extern CCSGameStats CCS_GameStats;
