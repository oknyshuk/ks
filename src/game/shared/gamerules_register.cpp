// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#if defined(CLIENT_DLL)
#include "rocketui/rmlui.h"
#elif defined(GAME_DLL)
#endif
import std;
import ks.core;
#if defined(CLIENT_DLL)
import ks.tier1;
#include "predictable_entity.h"
#include "cdll_util.h"
#include "util_shared.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "cdll_client_int.h"
#include "debugoverlay_shared.h"
#elif defined(GAME_DLL)
import ks.math;
#include "string_t.h"
#include "vphysics_interface.h"
#include "const.h"
#include "edict.h"
#include "eiface.h"
#include "iserverentity.h"
#include "dt_send.h"
#include "shareddefs.h"
#include "ehandle.h"
#include "datamap.h"
#include "util.h"
#include "predictable_entity.h"
#include "predictableid.h"
#include "variant_t.h"
#include "takedamageinfo.h"
#include "touchlink.h"
#include "groundlink.h"
#include "base_transmit_proxy.h"
#include "soundflags.h"
#include "networkvar.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "basetoggle.h"
#include "igameevents.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "gamerules.h"
#include "entitylist.h"
#include "basetempentity.h"
#include "player.h"
#include "te.h"
#include "recipientfilter.h"
#endif
#include "gamerules_register.h"

// ------------------------------------------------------------------------------------------ //
// CGameRulesRegister implementation.
// ------------------------------------------------------------------------------------------ //

CGameRulesRegister* CGameRulesRegister::s_pHead = nullptr;


CGameRulesRegister::CGameRulesRegister( const char *pClassName, CreateGameRulesFn fn )
{
	m_pClassName = pClassName;
	m_pFn = fn;
	
	m_pNext = s_pHead;	// Add us to the global list.
	s_pHead = this;
}

void CGameRulesRegister::CreateGameRules()
{
	m_pFn();
}

CGameRulesRegister* CGameRulesRegister::FindByName( const char *pName )
{
	for ( CGameRulesRegister *pCur=s_pHead; pCur; pCur=pCur->m_pNext )
	{
		if ( V_stricmp( pName, pCur->m_pClassName ) == 0 )
			return pCur;
	}
	return nullptr;
}


// ------------------------------------------------------------------------------------------ //
// Functions to dispatch the messages to create the game rules object on the client.
// ------------------------------------------------------------------------------------------ //

inline constexpr const char *GAMERULES_STRINGTABLE_NAME = "GameRulesCreation";


#ifdef CLIENT_DLL

	#include "networkstringtable_clientdll.h"

	INetworkStringTable *g_StringTableGameRules = nullptr;

	void OnGameRulesCreationStringChanged( 
		void *object, 
		INetworkStringTable *stringTable, 
		int stringNumber, 
		const char *newString, 
		void const *newData )
	{
		// The server has created a new CGameRules object.
		delete g_pGameRules;
		g_pGameRules = nullptr;

		const char *pClassName = (const char*)newData;
		CGameRulesRegister *pReg = CGameRulesRegister::FindByName( pClassName );
		if ( !pReg )
			Error( "OnGameRulesCreationStringChanged: missing gamerules class '%s' on the client", pClassName );

		// Create the new game rules object.
		pReg->CreateGameRules();

		if ( !g_pGameRules )
		{
			Error( "OnGameRulesCreationStringChanged: game rules entity (%s) not created", pClassName );
		}
	}

	// On the client, we respond to string table changes on the server.
	void InstallStringTableCallback_GameRules( const char *tableName )
	{
		if ( 0 == V_strcasecmp( tableName, GAMERULES_STRINGTABLE_NAME ) )
		{
			g_StringTableGameRules = networkstringtable->FindTable( tableName );
			if ( g_StringTableGameRules )
				g_StringTableGameRules->SetStringChangedCallback( nullptr, OnGameRulesCreationStringChanged );
		}
	}

#else

	#include "networkstringtable_gamedll.h"

	INetworkStringTable *g_StringTableGameRules = nullptr;

	void CreateNetworkStringTables_GameRules()
	{
		// Create the string table used by 
		g_StringTableGameRules = networkstringtable->CreateStringTable( GAMERULES_STRINGTABLE_NAME, 1 );
	}

	void CreateGameRulesObject( const char *pClassName )
	{
		// Delete the old game rules object.
		delete g_pGameRules;
		g_pGameRules = nullptr;

		// Create a new game rules object.
		CGameRulesRegister *pReg = CGameRulesRegister::FindByName( pClassName );
		if ( !pReg )
			Error( "InitGameRules: missing gamerules class '%s' on the server", pClassName );
	
		pReg->CreateGameRules();
		if ( !g_pGameRules )
		{
			Error( "InitGameRules: game rules entity (%s) not created", pClassName );
		}

		// Make sure the client gets notification to make a new game rules object.
		contract_assert( g_StringTableGameRules );
		g_StringTableGameRules->AddString( true, "classname", std::strlen( pClassName ) + 1, pClassName );

		if ( g_pGameRules )
		{
			g_pGameRules->CreateCustomNetworkStringTables();
		}
	}			

#endif
