// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

#include "mm_framework.h"
#include "mm_netmsgcontroller.h"

#include "matchsystem.h"

#include "mm_title_main.h"

// Stress testing events listeners every frame
ConVar mm_events_listeners_validation( "mm_events_listeners_validation", "0", FCVAR_DEVELOPMENTONLY );


//
// Implementation
//

CMatchFramework::CMatchFramework() :
	m_pMatchSession( nullptr ),
	m_bJoinTeamSession( false ),
	m_pTeamSessionSettings( nullptr )
{
}

CMatchFramework::~CMatchFramework()
{
	;
}

InitReturnVal_t CMatchFramework::Init()
{
	InitReturnVal_t ret = INIT_OK;

	ret = MM_Title_Init();
	if ( ret != INIT_OK )
		return ret;

	return INIT_OK;
}

void CMatchFramework::Shutdown()
{
	// Shutdown event system
	g_pMatchEventsSubscription->Shutdown();

	// Shutdown the title
	MM_Title_Shutdown();
}

void CMatchFramework::RunFrame()
{
	// Run frame listeners validation if requested
	if ( mm_events_listeners_validation.GetBool() )
	{
		g_pMatchEventsSubscription->BroadcastEvent( new KeyValues( "mm_events_listeners_validation" ) );
	}


	// Let the network mgr run
	g_pConnectionlessLanMgr->Update();

	// Let the match session run
	if ( m_pMatchSession )
		m_pMatchSession->Update();

	// Let the match title run frame
	g_pIMatchTitle->RunFrame();

	if ( m_bJoinTeamSession )
	{
		m_bJoinTeamSession = false;
		MatchSession( m_pTeamSessionSettings );
		m_pTeamSessionSettings->deleteThis();
		m_pTeamSessionSettings = nullptr;
	}
}

IMatchExtensions * CMatchFramework::GetMatchExtensions()
{
	return g_pMatchExtensions;
}

IMatchEventsSubscription * CMatchFramework::GetEventsSubscription()
{
	return g_pMatchEventsSubscription;
}

IMatchTitle * CMatchFramework::GetMatchTitle()
{
	return g_pIMatchTitle;
}

IMatchTitleGameSettingsMgr * CMatchFramework::GetMatchTitleGameSettingsMgr()
{
	return g_pIMatchTitleGameSettingsMgr;
}

IMatchNetworkMsgController * CMatchFramework::GetMatchNetworkMsgController()
{
	return g_pMatchNetMsgControllerBase;
}

IMatchSystem * CMatchFramework::GetMatchSystem()
{
	return g_pMatchSystem;
}

void CMatchFramework::ApplySettings( KeyValues* keyValues )
{
	g_pMatchExtensions->GetIServerGameDLL()->ApplyGameSettings( keyValues );
}


IMatchSession *CMatchFramework::GetMatchSession()
{
	return m_pMatchSession;
}

void CMatchFramework::CreateSession( KeyValues *pSettings )
{
	DevMsg( "CreateSession: \n");
	KeyValuesDumpAsDevMsg( pSettings );

	if ( !pSettings )
		return;

	IMatchSessionInternal *pMatchSessionNew = nullptr;

	//
	// Analyze the type of session requested to create
	//

	char const *szNetwork = pSettings->GetString( "system/network", "offline" );


	//
	// Process create session request
	//
	if ( !V_stricmp( "offline", szNetwork ) )
	{
		CMatchSessionOfflineCustom *pSession = new CMatchSessionOfflineCustom( pSettings );
		pMatchSessionNew = pSession;
	}
	else
	{
		Warning( "CreateSession: online sessions are not supported, use direct connect or LAN\n" );
	}

	if ( pMatchSessionNew )
	{
		CloseSession();
		m_pMatchSession = pMatchSessionNew;
	}
}

void CMatchFramework::MatchSession( KeyValues *pSettings )
{
	if ( !pSettings )
		return;

	DevMsg( "MatchSession: \n");
	KeyValuesDumpAsDevMsg( pSettings );

	IMatchSessionInternal *pMatchSessionNew = nullptr;

	//
	// Analyze what kind of client-side matchmaking
	// needs to happen.
	//

	char const *szAction = pSettings->GetString( "options/action", "" );

	//
	// Process match session request
	//
	if ( !V_stricmp( "joinsession", szAction ) )
	{
		Warning( "MatchSession: online sessions are not supported, use direct connect or LAN\n" );
	}
	else if ( !V_stricmp( "joininvitesession", szAction ) )
	{
	}
	else // "quickmatch" or "custommatch"
	{
		Warning( "MatchSession: online sessions are not supported, use direct connect or LAN\n" );
	}

	if ( pMatchSessionNew )
	{
		CloseSession();
		m_pMatchSession = pMatchSessionNew;
	}
}


void CMatchFramework::CloseSession()
{
	// Destroy the session
	if ( m_pMatchSession )
	{
		IMatchSessionInternal *pMatchSession = m_pMatchSession;
		m_pMatchSession = nullptr;
		pMatchSession->Destroy();

		g_pMatchEventsSubscription->BroadcastEvent( new KeyValues( "OnMatchSessionUpdate", "state", "closed" ) );
	}
}

bool CMatchFramework::IsOnlineGame( void )
{
	IMatchSession *pMatchSession = GetMatchSession();

	if ( pMatchSession ) 
	{
		KeyValues* kv = pMatchSession->GetSessionSettings();
		if ( kv )
		{
			char const *szMode = kv->GetString( "system/network", nullptr );
			if ( szMode && !V_stricmp( "LIVE", szMode ) )
			{
				return true;
			}
		}
	}
	return false;
}

void CMatchFramework::UpdateTeamProperties( KeyValues *pTeamProperties )
{
	if ( IMatchSession *pMatchSession = GetMatchSession() )
	{
		pMatchSession->UpdateTeamProperties( pTeamProperties );
	}
}

void CMatchFramework::OnEvent( KeyValues *pEvent )
{
	char const *szEvent = pEvent->GetName();

	if ( !V_stricmp( "mmF->CloseSession", szEvent ) )
	{
		CloseSession();
		return;
	}
	else if ( !V_stricmp( "OnMatchSessionUpdate", szEvent ) )
	{	
		KeyValues *pUpdate = pEvent->FindKey( "update" );
		
		if ( pUpdate )
		{
			const char *pAction = pUpdate->GetString( "options/action", "" );
			if ( !V_stricmp( "joinsession", pAction ) )
			{
				KeyValues *pTeamMembers = pUpdate->FindKey( "teamMembers" );

				if ( pTeamMembers )
				{
					// Received console team match settings from host
					// Find what team we are on

					int numPlayers = pTeamMembers->GetInt( "numPlayers" );
					int playerTeam = -1;

					int activeUer = XBX_GetPrimaryUserId();
					IPlayerLocal *player = g_pPlayerManager->GetLocalPlayer( activeUer );
					uint64 localPlayerId = player->GetXUID();

					for ( int i = 0; i < numPlayers; i++ )
					{
						KeyValues *pTeamPlayer = pTeamMembers->FindKey( CFmtStr( "player%d", i ) );
						uint64 playerId = pTeamPlayer->GetUint64( "xuid" );

						if ( playerId == localPlayerId )
						{
							int team = pTeamPlayer->GetInt( "team" );
							DevMsg( "Adding player %llu to team %d\n", playerId, team );
							playerTeam = team;
							break;
						}
					}
				
					m_pTeamSessionSettings = pUpdate->MakeCopy();
					m_pTeamSessionSettings->SetName( "settings ");

					// Delete the "teamMembers" key
					m_pTeamSessionSettings->RemoveSubKey( m_pTeamSessionSettings->FindKey( "teamMembers" ) );

					// Add "conteam" value
					m_pTeamSessionSettings->SetInt( "conteam", playerTeam );

					// Add the "sessionHostDataUnpacked" key
					KeyValues *pSessionHostDataSrc = pUpdate->FindKey( "sessionHostDataUnpacked" );
					if ( pSessionHostDataSrc )
					{
						KeyValues *pSessionHostDataDst = m_pTeamSessionSettings->CreateNewKey();
						pSessionHostDataDst->SetName( "sessionHostDataUnpacked" );
					
						pSessionHostDataSrc->CopySubkeys( pSessionHostDataDst );
					}

					m_bJoinTeamSession = true;
				}
			}
		}
	}

	//
	// Delegate to the managers
	//
	if ( g_pPlayerManager )
		g_pPlayerManager->OnEvent( pEvent );

	//
	// Delegate to the title
	//
	MM_Title_OnEvent( pEvent );

	//
	// Delegate to the session
	//
	if ( m_pMatchSession )
		m_pMatchSession->OnEvent( pEvent );
}

void CMatchFramework::SetCurrentMatchSession( IMatchSessionInternal *pNewMatchSession )
{
	m_pMatchSession = pNewMatchSession;
}

//-----------------------------------------------------------------------------
#include "facade/imatchframework.inl"
import ks.core.dbg;
import ks.core.platform;
import ks.core.threadtools;
import ks.core.types;
import ks.tier1.convar;
import ks.tier1.fmtstr;
import ks.tier1.iappsystem;
import ks.tier1.iconvar;
import ks.tier1.keyvalues;
import ks.tier1.strtools;
