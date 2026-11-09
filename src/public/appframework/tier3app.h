#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1999 Valve Corporation

// Purpose: The application objects for apps that use tier3

#include "appframework/tier2app.h"
#include "tier3/tier3.h"

//-----------------------------------------------------------------------------
// The application object for apps that use tier3
//-----------------------------------------------------------------------------
class CTier3FSApp : public CTier2FSApp
{
	typedef CTier2FSApp BaseClass;

public:
	// Application hooks
	bool PreInit()
	{
		if ( !BaseClass::PreInit() )
			return false;

		CreateInterfaceFn factory = GetFactory();
		ConnectTier3Libraries( &factory, 1 );
		return true;			
	}

	void PostShutdown()
	{
		DisconnectTier3Libraries();
		BaseClass::PostShutdown();
	}
};


//-----------------------------------------------------------------------------
// The application object for apps that use tier3
//-----------------------------------------------------------------------------
class CTier3DmFSApp : public CTier2DmFSApp
{
	typedef CTier2DmFSApp BaseClass;

public:
	// Application hooks
	bool PreInit()
	{
		if ( !BaseClass::PreInit() )
			return false;

		CreateInterfaceFn factory = GetFactory();
		ConnectTier3Libraries( &factory, 1 );
		return true;			
	}

	void PostShutdown()
	{
		DisconnectTier3Libraries();
		BaseClass::PostShutdown();
	}
};
