//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Client side CTeam class
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_TEAM_H
#define C_TEAM_H

#include "reflect_annotations.h"
#include "shareddefs.h"
#include "utlvector.h"
#include "client_thinklist.h"


class C_BasePlayer;

// named by the BareArray<> annotation below; both live in c_team.cpp
void RecvProxy_PlayerList( const CRecvProxyData *pData, void *pStruct, void *pOut );
void RecvProxyArrayLength_PlayerArray( void *pStruct, int objectID, int currentArrayLength );

class [[= ks::reflect::NetTable{ .name = "DT_Team", .base = false } ]]
      [[= ks::reflect::BareArray<"player_array_element", "\"player_array\"", MAX_PLAYERS, 0,
            SIZEOF_IGNORE, ks::reflect::Net{},
            RecvProxy_PlayerList, RecvProxyArrayLength_PlayerArray>{} ]]
      C_Team : public C_BaseEntity
{
	DECLARE_CLASS( C_Team, C_BaseEntity );
public:
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

					C_Team();
	virtual			~C_Team();

	virtual void	PreDataUpdate( DataUpdateType_t updateType );

	// Data Handling
	virtual const char	*Get_Name( void );
	virtual const char	*Get_ClanName( void );
	virtual const char	*Get_FlagImageString( void );
	virtual const char	*Get_LogoImageString( void );
	int				Get_Score( void )				{ return m_scoreTotal; }
	int				Get_Score_First_Half( void )	{ return m_scoreFirstHalf; }
	int				Get_Score_Second_Half( void )	{ return m_scoreSecondHalf; }	
	int				Get_Score_Overtime( void )		{ return m_scoreOvertime; }
	uint32			GetClanID( void )				{ return m_iClanID; }
	
	virtual int		Get_Deaths( void );
	virtual int		Get_Ping( void );

	// Player Handling
	virtual int		Get_Number_Players( void );
	virtual bool	ContainsPlayer( int iPlayerIndex );
	C_BasePlayer*	GetPlayer( int idx );

	// for shared code, use the same function name
	virtual int		GetNumPlayers( void ) { return Get_Number_Players(); }

	virtual int		GetGGLeader( int nTeam );

	int		GetTeamNumber() const;

	void	RemoveAllPlayers();


// IClientThinkable overrides.
public:

	virtual	void				ClientThink();


public:

	// Data received from the server
	CUtlVector< int > m_aPlayers;
	[[= ks::reflect::Pred{ .flags = FTYPEDESC_PRIVATE } ]] [[= ks::reflect::Net{} ]] char	m_szTeamname[ MAX_TEAM_NAME_LENGTH ];
	[[= ks::reflect::Pred{ .flags = FTYPEDESC_PRIVATE } ]] [[= ks::reflect::Net{} ]] char	m_szClanTeamname[ MAX_TEAM_NAME_LENGTH ];
	[[= ks::reflect::Pred{ .flags = FTYPEDESC_PRIVATE } ]] [[= ks::reflect::Net{} ]] char	m_szTeamFlagImage[ MAX_TEAM_FLAG_ICON_LENGTH ];
	[[= ks::reflect::Pred{ .flags = FTYPEDESC_PRIVATE } ]] [[= ks::reflect::Net{} ]] char	m_szTeamLogoImage[ MAX_TEAM_LOGO_ICON_LENGTH ];
	[[= ks::reflect::Pred{ .flags = FTYPEDESC_PRIVATE } ]] [[= ks::reflect::Net{} ]] char	m_szTeamMatchStat[ MAX_PATH ];
	[[= ks::reflect::Pred{ .flags = FTYPEDESC_PRIVATE } ]] [[= ks::reflect::Net{} ]] int		m_scoreTotal;
	[[= ks::reflect::Pred{ .flags = FTYPEDESC_PRIVATE } ]] [[= ks::reflect::Net{} ]] int		m_scoreFirstHalf;
	[[= ks::reflect::Pred{ .flags = FTYPEDESC_PRIVATE } ]] [[= ks::reflect::Net{} ]] int		m_scoreSecondHalf;	
	[[= ks::reflect::Pred{ .flags = FTYPEDESC_PRIVATE } ]] [[= ks::reflect::Net{} ]] int		m_scoreOvertime;
	[[= ks::reflect::Net{} ]] int		m_nGGLeaderEntIndex_CT;
	[[= ks::reflect::Net{} ]] int		m_nGGLeaderEntIndex_T;
	[[= ks::reflect::Pred{ .flags = FTYPEDESC_PRIVATE } ]] [[= ks::reflect::Net{} ]] uint32	m_iClanID;

	// Data for the scoreboard
	[[= ks::reflect::Pred{ .flags = FTYPEDESC_PRIVATE } ]] int		m_iDeaths;
	[[= ks::reflect::Pred{ .flags = FTYPEDESC_PRIVATE } ]] int		m_iPing;
	[[= ks::reflect::Pred{ .flags = FTYPEDESC_PRIVATE } ]] int		m_iPacketloss;
	[[= ks::reflect::Pred{ .flags = FTYPEDESC_PRIVATE } ]] [[= ks::reflect::Net{} ]] int		m_iTeamNum;
	[[= ks::reflect::Pred{ .flags = FTYPEDESC_PRIVATE } ]] [[= ks::reflect::Net{} ]] int		m_bSurrendered;
	[[= ks::reflect::Net{} ]] int		m_numMapVictories;
};


// Global list of client side team entities
extern CUtlVector< C_Team * > g_Teams;

// Global team handling functions
C_Team *GetLocalTeam( void );
C_Team *GetGlobalTeam( int iTeamNumber );
C_Team *GetPlayersTeam( int iPlayerIndex );
C_Team *GetPlayersTeam( C_BasePlayer *pPlayer );
bool ArePlayersOnSameTeam( int iPlayerIndex1, int iPlayerIndex2 );
extern int GetNumberOfTeams( void );

#endif // C_TEAM_H
