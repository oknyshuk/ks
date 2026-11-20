// main precompiled header for client files

import std;
import ks.core.basetypes;
import ks.core.threadtools;
import ks.platform;
import ks.simd;
import ks.tier1.ns_address;
import ks.tier1.strtools;






#include "cdll_engine_int.h"
#include "client.h"

#include "demo.h"
#include "host.h"

#ifndef DEDICATED
#include "engineui.h"
#include "localize/ilocalize.h"
#endif
#include "cl_splitscreen.h"


class CSplitScreen : public ISplitScreen
{
public:
	CSplitScreen();

	bool			Init();
	void			Shutdown();

	bool			AddSplitScreenUser( int nSlot, int nPlayerIndex );
	bool			AddBaseUser( int nSlot, int nPlayerIndex );
	bool			RemoveSplitScreenUser( int nSlot, int nPlayerIndex );
	int				GetActiveSplitScreenPlayerSlot();
	int				SetActiveSplitScreenPlayerSlot( int slot );

	bool			IsValidSplitScreenSlot( int nSlot );
	int				FirstValidSplitScreenSlot(); // -1 == invalid
	int				NextValidSplitScreenSlot( int nPreviousSlot ); // -1 == invalid

	int				GetNumSplitScreenPlayers();
	int				GetSplitScreenPlayerEntity( int nSlot );
	INetChannel		*GetSplitScreenPlayerNetChan( int nSlot );

	bool			IsDisconnecting( int nSlot );
	void			SetDisconnecting( int nSlot, bool bState );

	bool			SetLocalPlayerIsResolvable( char const *pchContext, int nLine, bool bResolvable );
	bool			IsLocalPlayerResolvable();

	CClientState			&GetLocalPlayer( int nSlot = -1 );

	struct SplitSlot_t
	{
		SplitSlot_t() : m_nActiveSplitScreenPlayer( 0 ), m_bLocalPlayerResolvable( false ), m_bMainThread( false ) { }

		short			m_nActiveSplitScreenPlayer;
		// Can a call to C_BasePlayer::GetLocalPlayer be resolved in client .dll (inside a setactivesplitscreenuser scope?)
		unsigned short	m_bLocalPlayerResolvable : 1;
		unsigned short	m_bMainThread : 1;
		unsigned short	pad : 14;
	};

public:

	int FindSplitPlayerSlot( int nPlayerEntityIndex );

	struct SplitPlayer_t
	{
		SplitPlayer_t() : 
	m_bActive( false )
	{ 
	}

	bool			m_bActive;
	CClientState	m_Client;
	};

	SplitPlayer_t *m_SplitScreenPlayers[ MAX_SPLITSCREEN_CLIENTS ];
	int			m_nActiveSplitScreenUserCount;

	// Each thread (mainly an issue in the client .dll) can have it's own "active" context.  The per thread data is allocated as needed

	SplitSlot_t *GetSplitSlot();

	bool		m_bInitialized;
};

#include "facade/isplitscreen.inl"


static thread_local CSplitScreen::SplitSlot_t *	s_SplitSlot;


static CSplitScreen g_SplitScreenMgr;
ISplitScreen *splitscreen = &g_SplitScreenMgr;

CSplitScreen::CSplitScreen()
{
	m_bInitialized = false;
}


CSplitScreen::SplitSlot_t *CSplitScreen::GetSplitSlot()
{
	if ( !s_SplitSlot )
	{
		s_SplitSlot = new SplitSlot_t();
	}
	return s_SplitSlot;
}

bool CSplitScreen::Init()
{
	m_bInitialized = true;

	contract_assert( ThreadInMainThread() );

	SplitSlot_t *pSlot = GetSplitSlot();
	pSlot->m_bLocalPlayerResolvable = false;
	pSlot->m_nActiveSplitScreenPlayer = 0;
	pSlot->m_bMainThread = true;
	m_nActiveSplitScreenUserCount = 1;
	for ( int i = 0 ; i < MAX_SPLITSCREEN_CLIENTS; ++i )
	{
		m_SplitScreenPlayers[ i ] = new SplitPlayer_t();
		SplitPlayer_t *sp = m_SplitScreenPlayers[ i ];
		sp->m_bActive = ( i == 0 ) ? true : false;
		sp->m_Client.m_bSplitScreenUser = ( i != 0 ) ? true : false;
	}

	return true;
}

void CSplitScreen::Shutdown()
{
	contract_assert( ThreadInMainThread() );
	for ( int i = 0; i < MAX_SPLITSCREEN_CLIENTS; ++i )
	{
		delete m_SplitScreenPlayers[ i ];
		m_SplitScreenPlayers[ i ] = nullptr;
	}
}

bool CSplitScreen::AddBaseUser( int nSlot, int nPlayerIndex )
{
	contract_assert( ThreadInMainThread() );
	SplitPlayer_t *sp = m_SplitScreenPlayers[ nSlot ];
	sp->m_bActive = true;
	sp->m_Client.m_nSplitScreenSlot = nSlot;
	return true;
}

bool CSplitScreen::AddSplitScreenUser( int nSlot, int nPlayerEntityIndex )
{
	contract_assert( ThreadInMainThread() );
	SplitPlayer_t *sp = m_SplitScreenPlayers[ nSlot ];
	if ( sp->m_bActive == true )
	{
		contract_assert( sp->m_Client.m_nSplitScreenSlot == nSlot );
		contract_assert( sp->m_Client.m_nPlayerSlot == nPlayerEntityIndex - 1 );
		return true;
	}

	// Msg( "Attached %d to slot %d\n", nPlayerEntityIndex, nSlot );

	// 0.0.0.0:0 signifies a bot. It'll plumb all the way down to winsock calls but it won't make them.
	ns_address adr;
	adr.SetAddrType( NSAT_NETADR );
	adr.m_adr.SetIPAndPort( 0, 0 );

	char szName[ 256 ];
	V_snprintf( szName, sizeof( szName), "SPLIT%d", nSlot );

	sp->m_bActive = true;
	sp->m_Client.Clear();
	sp->m_Client.m_nPlayerSlot = nPlayerEntityIndex - 1;
	sp->m_Client.m_nSplitScreenSlot = nSlot;
	sp->m_Client.m_NetChannel = NET_CreateNetChannel( NS_CLIENT, &adr, szName, &sp->m_Client, nullptr, true );
	GetBaseLocalClient().m_NetChannel->AttachSplitPlayer( nSlot, sp->m_Client.m_NetChannel );
	sp->m_Client.m_nViewEntity = nPlayerEntityIndex;
	++m_nActiveSplitScreenUserCount;
	SetDisconnecting( nSlot, false );

	ClientDLL_OnSplitScreenStateChanged();

	return true;
}

bool CSplitScreen::RemoveSplitScreenUser( int nSlot, int nPlayerIndex )
{
	contract_assert( ThreadInMainThread() );
	// Msg( "Detached %d from slot %d\n", nPlayerIndex, nSlot );

	int idx = FindSplitPlayerSlot( nPlayerIndex );
	if ( idx != -1 )
	{
		SplitPlayer_t *sp = m_SplitScreenPlayers[ idx ];
		if ( sp->m_Client.m_NetChannel )
		{
			GetBaseLocalClient().m_NetChannel->DetachSplitPlayer( idx );
			sp->m_Client.m_NetChannel->Shutdown( "RemoveSplitScreenUser" );
			sp->m_Client.m_NetChannel = nullptr;
		}
		sp->m_Client.m_nPlayerSlot = -1;
		sp->m_bActive = false;
		SetDisconnecting( nSlot, true );
		--m_nActiveSplitScreenUserCount;
		ClientDLL_OnSplitScreenStateChanged();
	}
	return true;
}

int CSplitScreen::GetActiveSplitScreenPlayerSlot()
{
	return 0;
}

int CSplitScreen::SetActiveSplitScreenPlayerSlot( int slot )
{
	return 0;
}

int	CSplitScreen::GetNumSplitScreenPlayers()
{
	return m_nActiveSplitScreenUserCount;
}

int CSplitScreen::GetSplitScreenPlayerEntity( int nSlot )
{
	contract_assert( nSlot >= 0 && nSlot < host_state.max_splitscreen_players );
	contract_assert( host_state.max_splitscreen_players <= MAX_SPLITSCREEN_CLIENTS );
	if ( nSlot < 0 || nSlot >= host_state.max_splitscreen_players )
		return -1;
	if ( !m_SplitScreenPlayers[ nSlot ]->m_bActive )
		return -1;
	return m_SplitScreenPlayers[ nSlot ]->m_Client.m_nPlayerSlot + 1;
}

INetChannel *CSplitScreen::GetSplitScreenPlayerNetChan( int nSlot )
{
	contract_assert( nSlot >= 0 && nSlot < host_state.max_splitscreen_players );
	contract_assert( host_state.max_splitscreen_players <= MAX_SPLITSCREEN_CLIENTS );
	if ( nSlot < 0 || nSlot >= host_state.max_splitscreen_players )
		return nullptr;
	if ( !m_SplitScreenPlayers[ nSlot ]->m_bActive )
		return nullptr;
	return m_SplitScreenPlayers[ nSlot ]->m_Client.m_NetChannel;
}

bool CSplitScreen::IsValidSplitScreenSlot( int nSlot )
{
	contract_assert( host_state.max_splitscreen_players <= MAX_SPLITSCREEN_CLIENTS );
	if ( nSlot < 0 || nSlot >= host_state.max_splitscreen_players )
		return false;
	return m_SplitScreenPlayers[ nSlot ]->m_bActive;
}

int CSplitScreen::FirstValidSplitScreenSlot()
{
	return 0;
}

int CSplitScreen::NextValidSplitScreenSlot( int nPreviousSlot )
{
	for ( ;; )
	{
		++nPreviousSlot;
		if ( nPreviousSlot >= host_state.max_splitscreen_players )
		{
			return -1;
		}

		if ( m_SplitScreenPlayers[ nPreviousSlot ]->m_bActive )
		{
			break;
		}

	}
	return nPreviousSlot;
}

int CSplitScreen::FindSplitPlayerSlot( int nPlayerEntityIndex )
{
	int nPlayerSlot = nPlayerEntityIndex - 1;

	contract_assert( host_state.max_splitscreen_players <= MAX_SPLITSCREEN_CLIENTS );
	for ( int i = 1 ; i < host_state.max_splitscreen_players ; ++i )
	{
		if ( m_SplitScreenPlayers[ i ]->m_Client.m_nPlayerSlot == nPlayerSlot )
		{
			return i;
		}
	}
	return -1;
}

bool CSplitScreen::IsDisconnecting( int nSlot )
{
	contract_assert( nSlot >= 0 && nSlot < host_state.max_splitscreen_players );
	contract_assert( host_state.max_splitscreen_players <= MAX_SPLITSCREEN_CLIENTS );
	if ( nSlot < 0 || nSlot >= host_state.max_splitscreen_players )
		return false;

	return ( m_SplitScreenPlayers[ nSlot ]->m_Client.m_nSignonState == SIGNONSTATE_NONE ) ? true : false;
}

void CSplitScreen::SetDisconnecting( int nSlot, bool bState )
{
	contract_assert( nSlot >= 0 && nSlot < host_state.max_splitscreen_players );
	contract_assert( host_state.max_splitscreen_players <= MAX_SPLITSCREEN_CLIENTS );
	if ( nSlot < 0 || nSlot >= host_state.max_splitscreen_players )
		return;

	contract_assert( nSlot != 0 );
	m_SplitScreenPlayers[ nSlot ]->m_Client.m_nSignonState = bState ? SIGNONSTATE_NONE : SIGNONSTATE_FULL;
}

CClientState &CSplitScreen::GetLocalPlayer( int nSlot /*= -1*/ )
{
	if ( nSlot == -1 )
	{
		return m_SplitScreenPlayers[ GetActiveSplitScreenPlayerSlot() ]->m_Client;
	}
	return m_SplitScreenPlayers[ nSlot ]->m_Client;
}

bool CSplitScreen::SetLocalPlayerIsResolvable( char const *pchContext, int line, bool bResolvable )
{
	SplitSlot_t *pSlot = GetSplitSlot();
	contract_assert( pSlot );
	bool bPrev = pSlot->m_bLocalPlayerResolvable;
	pSlot->m_bLocalPlayerResolvable = bResolvable;
	return bPrev;
}

bool CSplitScreen::IsLocalPlayerResolvable()
{

	return true;

}

// Singleton client state
CClientState &GetLocalClient( int nSlot /*= -1*/ )
{
	return g_SplitScreenMgr.GetLocalPlayer( nSlot );
}

CClientState &GetBaseLocalClient()
{
	return g_SplitScreenMgr.GetLocalPlayer( 0 );
}
