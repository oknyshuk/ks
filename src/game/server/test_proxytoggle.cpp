//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "reflect_sendtable.h"
#include "reflect_annotations.h"
#include "baseentity.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CTest_ProxyToggle_Networkable;
static CTest_ProxyToggle_Networkable *g_pTestObj = 0;
static bool g_bEnableProxy = true;


// ---------------------------------------------------------------------------------------- //
// CTest_ProxyToggle_Networkable
// ---------------------------------------------------------------------------------------- //

namespace DT_ProxyToggle_ProxiedData { extern SendTable g_SendTable; }
void *SendProxy_TestProxyToggle( const SendProp *pProp, const void *pStruct,
    const void *pVarData, CSendProxyRecipients *pRecipients,
    int objectID );

class [[= ks::reflect::NetTable{ .name = "DT_ProxyToggle_ProxiedData", .base = false } ]]
      [[= ks::reflect::NetTable{ .name = "DT_ProxyToggle" } ]]
      [[= ks::reflect::SubTable<"blah", &DT_ProxyToggle_ProxiedData::g_SendTable, SendProxy_TestProxyToggle, true>{} ]]
      CTest_ProxyToggle_Networkable : public CBaseEntity
{
public:
	DECLARE_CLASS( CTest_ProxyToggle_Networkable, CBaseEntity );
	DECLARE_SERVERCLASS();

			CTest_ProxyToggle_Networkable()
			{
				m_WithProxy = 1241;
				g_pTestObj = this;
			}

			~CTest_ProxyToggle_Networkable()
			{
				g_pTestObj = nullptr;
			}

	int UpdateTransmitState()
	{
		return SetTransmitState( FL_EDICT_ALWAYS );
	}

	CNetworkVar( int, m_WithProxy, [[= ks::reflect::Net{ .bits = -1, .table = "DT_ProxyToggle_ProxiedData" } ]] );
};

void* SendProxy_TestProxyToggle( const SendProp *pProp, const void *pStructBase, const void *pData, CSendProxyRecipients *pRecipients, int objectID )
{
	if ( g_bEnableProxy )
	{
		return (void*)pData;
	}
	else
	{
		pRecipients->ClearAllRecipients();
		return nullptr;
	}
}
REGISTER_SEND_PROXY_NON_MODIFIED_POINTER( SendProxy_TestProxyToggle );


// ---------------------------------------------------------------------------------------- //
// Datatables.
// ---------------------------------------------------------------------------------------- //

LINK_ENTITY_TO_CLASS( test_proxytoggle, CTest_ProxyToggle_Networkable );

IMPLEMENT_REFLECT_TABLE_IN( CTest_ProxyToggle_Networkable, DT_ProxyToggle_ProxiedData );

IMPLEMENT_REFLECT_SERVERCLASS( CTest_ProxyToggle_Networkable, DT_ProxyToggle )



// ---------------------------------------------------------------------------------------- //
// Console commands for this test.
// ---------------------------------------------------------------------------------------- //

void Test_ProxyToggle_EnableProxy( const CCommand &args )
{
	if ( args.ArgC() < 2 )
	{
		Error( "Test_ProxyToggle_EnableProxy: requires parameter (0 or 1)." );
	}

	g_bEnableProxy = !!atoi( args[ 1 ] );
}

void Test_ProxyToggle_SetValue( const CCommand &args )
{
	if ( args.ArgC() < 2 )
	{
		Error( "Test_ProxyToggle_SetValue: requires value parameter." );
	}
	else if ( !g_pTestObj )
	{
		Error( "Test_ProxyToggle_SetValue: no entity present." );
	}

	g_pTestObj->m_WithProxy = atoi( args[ 1 ] );
}

ConCommand cc_Test_ProxyToggle_EnableProxy( "Test_ProxyToggle_EnableProxy", Test_ProxyToggle_EnableProxy, 0, FCVAR_CHEAT );
ConCommand cc_Test_ProxyToggle_SetValue( "Test_ProxyToggle_SetValue", Test_ProxyToggle_SetValue, 0, FCVAR_CHEAT );


