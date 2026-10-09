// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "rocketui/rmlui.h"
import ks.core;
import std;
#include "precache_register.h"
#include "cdll_client_int.h"
#include "clientmode.h"

//================================================================
// Global startup and shutdown functions for game code in the DLL.
//================================================================

class CViewportClientSystem : public IGameSystem
{
public:
	CViewportClientSystem()
	{
	}

	virtual char const *Name() { return "CViewportClientSystem"; }
	virtual bool IsPerFrame() { return false; }

	// Init, shutdown
	virtual bool Init()
	{
		for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
		{
			ACTIVE_SPLITSCREEN_PLAYER_GUARD_UI( hh );
			GetClientMode()->Layout();
		}
		return true;
	}
	virtual void PostInit() {}
	virtual void Shutdown() {}
	virtual void LevelInitPreEntity() {}
	virtual void LevelInitPostEntity() {}
	virtual void LevelShutdownPreEntity() {}
	virtual void LevelShutdownPostEntity() {}
	virtual void SafeRemoveIfDesired() {}

	virtual void OnSave() {}
	virtual void OnRestore() {}

};

static CViewportClientSystem g_ViewportClientSystem;

IGameSystem *ViewportClientSystem()
{
	return &g_ViewportClientSystem;
}
