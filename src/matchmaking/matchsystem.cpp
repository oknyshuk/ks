//========= Copyright � 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "mm_framework.h"

#include "matchsystem.h"
#include "playermanager.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


static CMatchSystem s_MatchSystem;
CMatchSystem *g_pMatchSystem = &s_MatchSystem;

bool IsValidXNKID( XNKID xnkid )
{
	for(int i = 0; i < 8; ++i)
	{
		if(xnkid.ab[i])
			return true;
	}
	return false;
}

CMatchSystem::CMatchSystem()
{
}

CMatchSystem::~CMatchSystem()
{
}

IPlayerManager * CMatchSystem::GetPlayerManager()
{
	return g_pPlayerManager;
}

IMatchVoice * CMatchSystem::GetMatchVoice()
{
	return g_pMatchVoice;
}


IServerManager * CMatchSystem::GetUserGroupsServerManager()
{
	return NULL;
}

ISearchManager * CMatchSystem::CreateGameSearchManager( KeyValues *pParams )
{
	return NULL;
}

IDatacenter * CMatchSystem::GetDatacenter()
{
	return NULL;
}

IDlcManager * CMatchSystem::GetDlcManager()
{
	return NULL;
}

void CMatchSystem::Update()
{
	if ( g_pPlayerManager )
		g_pPlayerManager->Update();
}

