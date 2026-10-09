#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2008 Valve Corporation

class INetChannel;
class ISplitScreen
{
public:

	virtual bool			Init() = 0;
	virtual void			Shutdown() = 0;

	virtual bool			AddSplitScreenUser( int nSlot, int nPlayerIndex ) = 0;
	virtual bool			AddBaseUser( int nSlot, int nPlayerIndex ) = 0;
	virtual bool			RemoveSplitScreenUser( int nSlot, int nPlayerIndex ) = 0;
	virtual int				GetActiveSplitScreenPlayerSlot() = 0;
	virtual int				SetActiveSplitScreenPlayerSlot( int nSlot ) = 0;

	virtual bool			IsValidSplitScreenSlot( int nSlot ) = 0;
	virtual int				FirstValidSplitScreenSlot() = 0; // -1 == invalid
	virtual int				NextValidSplitScreenSlot( int nPreviousSlot ) = 0; // -1 == invalid

	virtual int				GetNumSplitScreenPlayers() = 0;
	virtual int				GetSplitScreenPlayerEntity( int nSlot ) = 0;
	virtual INetChannel		*GetSplitScreenPlayerNetChan( int nSlot ) = 0;

	virtual bool			IsDisconnecting( int nSlot ) = 0;
	virtual void			SetDisconnecting( int nSlot, bool bState ) = 0;

	virtual bool			SetLocalPlayerIsResolvable( char const *pchContext, int nLine, bool bResolvable ) = 0;
	virtual bool			IsLocalPlayerResolvable() = 0;
};

#ifndef DEDICATED
extern ISplitScreen *splitscreen;
#endif

class CSetActiveSplitScreenPlayerGuard
{
public:
	CSetActiveSplitScreenPlayerGuard( char const *pchContext, int nLine, int slot );
	~CSetActiveSplitScreenPlayerGuard();
private:
	char const *m_pchContext;
	int m_nLine;
	int	 m_nSaveSlot;
	bool m_bResolvable;
};

inline constexpr auto IS_LOCAL_PLAYER_RESOLVABLE = true;
template <typename T1, typename T2, typename T3> [[gnu::always_inline]] inline constexpr auto SET_LOCAL_PLAYER_RESOLVABLE( T1 a, T2 b, T3 c ) { return true; }
#define SET_ACTIVE_SPLIT_SCREEN_PLAYER_SLOT( x )

#define ACTIVE_SPLITSCREEN_PLAYER_GUARD( slot )

#define FOR_EACH_VALID_SPLITSCREEN_PLAYER( iteratorName ) for ( int iteratorName = 0; iteratorName == 0; ++iteratorName )

#define ASSERT_LOCAL_PLAYER_RESOLVABLE()
#define GET_ACTIVE_SPLITSCREEN_SLOT() ( 0 )
[[gnu::always_inline]] inline constexpr auto GET_NUM_SPLIT_SCREEN_PLAYERS() { return 1; }
template <typename T1> [[gnu::always_inline]] inline constexpr auto IS_VALID_SPLIT_SCREEN_SLOT( T1 i ) { return ( ( i ) == 0 ); }
