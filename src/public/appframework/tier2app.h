#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1999 Valve Corporation

// Purpose: The application object for apps that use tier2

import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.iappsystem;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.tier1;
#include "appframework/AppFramework.h"
#include "tier2/tier2dm.h"
import std;

//-----------------------------------------------------------------------------
// The application object for apps that use tier2
//-----------------------------------------------------------------------------
class CTier2FSApp : public CFSAppSystemGroup
{
	typedef CFSAppSystemGroup BaseClass;

public:
	// Application hooks
	bool PreInit()
	{
		CreateInterfaceFn factory = GetFactory();
		ConnectTier1Libraries( &factory, 1 );
		ConVar_Register( 0 );
		ConnectTier2Libraries( &factory, 1 );
		return true;			
	}

	void PostShutdown()
	{
		DisconnectTier2Libraries();
		ConVar_Unregister();
		DisconnectTier1Libraries();
	}

	void Destroy()
	{
	}
};


//-----------------------------------------------------------------------------
// The application object for apps that use tier2 and datamodel
//-----------------------------------------------------------------------------
class CTier2DmFSApp : public CTier2FSApp
{
	typedef CTier2FSApp BaseClass;

public:
	// Application hooks
	bool PreInit()
	{
		if ( !BaseClass::PreInit() )
			return false;

		CreateInterfaceFn factory = GetFactory();
		if ( !ConnectDataModel( factory ) )
			return false;

		InitReturnVal_t nRetVal = InitDataModel();
		return ( nRetVal == INIT_OK );
	}

	void PostShutdown()
	{
		ShutdownDataModel();
		DisconnectDataModel();
		BaseClass::PostShutdown();
	}
};
