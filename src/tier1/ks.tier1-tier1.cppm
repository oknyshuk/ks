// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 2005 Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.tier1.tier1;

import std;
import ks.platform;
import ks.simd;
export import ks.tier1.convar;

export extern "C++" {

//-----------------------------------------------------------------------------
// Call this to connect to/disconnect from all tier 1 libraries.
// It's up to the caller to check the globals it cares about to see if ones are missing
//-----------------------------------------------------------------------------
void ConnectTier1Libraries( CreateInterfaceFn *pFactoryList, int nFactoryCount );
void DisconnectTier1Libraries();

//-----------------------------------------------------------------------------
// Helper empty implementation of an IAppSystem for tier2 libraries
//-----------------------------------------------------------------------------
template< class IInterface, int ConVarFlag = 0 > 
class CTier1AppSystem : public CTier0AppSystem< IInterface >
{
	typedef CTier0AppSystem< IInterface > BaseClass;

public:
	virtual bool Connect( CreateInterfaceFn factory ) 
	{
		if ( !BaseClass::Connect( factory ) )
			return false;

		ConnectTier1Libraries( &factory, 1 );
		return true;
	}

	virtual void Disconnect() 
	{
		DisconnectTier1Libraries();
		BaseClass::Disconnect();
	}

	virtual InitReturnVal_t Init()
	{
		InitReturnVal_t nRetVal = BaseClass::Init();
		if ( nRetVal != INIT_OK )
			return nRetVal;

		if ( g_pCVar )
		{
			ConVar_Register( ConVarFlag );
		}
		return INIT_OK;
	}

	virtual void Shutdown()
	{
		if ( g_pCVar )
		{
			ConVar_Unregister( );
		}
		BaseClass::Shutdown( );
	}

	virtual AppSystemTier_t GetTier()
	{
		return APP_SYSTEM_TIER1;
	}
};

}
