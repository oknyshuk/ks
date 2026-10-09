// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2007 Valve Corporation

import std;

#include "mm_framework.h"

// GCSDK uses the console log channel

void LinkMatchmakingLib()
{
	// This function is required for the linker to include CMatchFramework
}

static CMatchFramework g_MatchFramework;
CMatchFramework *g_pMMF = &g_MatchFramework;
static InterfaceReg s_Reg_CMatchFramework_IMatchFramework( []() -> void * { return static_cast<IMatchFramework *>( &g_MatchFramework ); }, IMATCHFRAMEWORK_VERSION_STRING );;

//
// IAppSystem implementation
//

static CreateInterfaceFn s_pfnDelegateFactory;
static void * InternalFactory( const char *pName, int *pReturnCode )
{
	if ( pReturnCode )
	{
		*pReturnCode = IFACE_OK;
	}

	// Try to get interface via delegate
	if ( void *pInterface = s_pfnDelegateFactory ? s_pfnDelegateFactory( pName, pReturnCode ) : nullptr )
	{
		return pInterface;
	}

	// Try to get internal interface
	if ( void *pInterface = Sys_GetFactoryThis()( pName, pReturnCode ) )
	{
		return pInterface;
	}

	// Failed
	if ( pReturnCode )
	{
		*pReturnCode = IFACE_FAILED;
	}
	return nullptr;	
}

namespace
{

typedef void * (CMatchExtensions::* ExtFn_t)();

struct MatchExtInterface_t
{
	char const *m_szName;
	ExtFn_t m_pfnGetInterface;
	bool m_bConnected;
};

static MatchExtInterface_t s_table[] =
{
	{ LOCALIZE_INTERFACE_VERSION,		(ExtFn_t) &CMatchExtensions::GetILocalize,			false },
	{ INETSUPPORT_VERSION_STRING,		(ExtFn_t) &CMatchExtensions::GetINetSupport,		false },
	{ IENGINEVOICE_INTERFACE_VERSION,	(ExtFn_t) &CMatchExtensions::GetIEngineVoice,		false },
	{ VENGINE_CLIENT_INTERFACE_VERSION,	(ExtFn_t) &CMatchExtensions::GetIVEngineClient,		false },
	{ INTERFACEVERSION_VENGINESERVER,	(ExtFn_t) &CMatchExtensions::GetIVEngineServer,		false },
	{ INTERFACEVERSION_GAMEEVENTSMANAGER2, (ExtFn_t) &CMatchExtensions::GetIGameEventManager2, false },
	{ nullptr, nullptr, false }
};

};

bool CMatchFramework::Connect( CreateInterfaceFn factory )
{
	contract_assert( !s_pfnDelegateFactory );

	s_pfnDelegateFactory = factory;

	CreateInterfaceFn ourFactory = InternalFactory;
	ConnectTier1Libraries( &ourFactory, 1 );
	ConnectTier2Libraries( &ourFactory, 1 );
	ConVar_Register();

	// Get our extension interfaces
	for ( MatchExtInterface_t *ptr = s_table; ptr->m_szName; ++ ptr )
	{
		if ( !ptr->m_bConnected
			 // && !(g_pMatchExtensions->*(ptr->m_pfnGetInterface))()
			 )
		{
			void *pvInterface = ourFactory( ptr->m_szName, nullptr );
			if ( pvInterface )
			{
				g_pMatchExtensions->RegisterExtensionInterface( ptr->m_szName, pvInterface );
				ptr->m_bConnected = true;
			}
		}
	}

	s_pfnDelegateFactory = nullptr;

	MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f );

	return true;
}

void CMatchFramework::Disconnect()
{
	for ( MatchExtInterface_t *ptr = s_table; ptr->m_szName; ++ ptr )
	{
		if ( ptr->m_bConnected )
		{
			void *pvInterface = (g_pMatchExtensions->*(ptr->m_pfnGetInterface))();
			contract_assert( pvInterface );
			g_pMatchExtensions->UnregisterExtensionInterface( ptr->m_szName, pvInterface );
			ptr->m_bConnected = false;
		}
	}

	DisconnectTier2Libraries();
	ConVar_Unregister();
	DisconnectTier1Libraries();
}

void * CMatchFramework::QueryInterface( const char *pInterfaceName )
{
	if ( !V_stricmp( pInterfaceName, IMATCHFRAMEWORK_VERSION_STRING ) )
		return static_cast< IMatchFramework* >( this );

	return nullptr;
}

bool IsLocalClientConnectedToServer()
{
	return
		( g_pMatchExtensions &&
		  g_pMatchExtensions->GetIVEngineClient() &&
		  ( g_pMatchExtensions->GetIVEngineClient()->IsConnected() ||
		    g_pMatchExtensions->GetIVEngineClient()->IsDrawingLoadingImage() ||
			g_pMatchExtensions->GetIVEngineClient()->IsTransitioningToLoad() ) );
}
