#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation
import ks.core;

// Purpose: Contains all utility methods for the new game UI system

#include "igamesystem.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
struct InputEvent_t;


//-----------------------------------------------------------------------------
// Create a game system for UI
//-----------------------------------------------------------------------------
class CGameUIGameSystem : public CBaseGameSystemPerFrame
{
	// Inherited from IGameSystem
public:
	virtual bool Init();
	virtual void PostInit();
	virtual void Update( float frametime );
	virtual void Shutdown();

	// Other public methods
public:
	// Init any render targets needed by the UI.
	void InitRenderTargets();
	IMaterialProxy *CreateProxy( const char *proxyName );

	// Renders the game UI
	void Render( const Rect_t &viewport, float flCurrentTime );

	// Send an input event to the game ui
	bool RegisterInputEvent( const InputEvent_t &iEvent );

	// Reloads game GUI sounds
	void ReloadSounds();

private:
	void PrecacheGameUISounds();
};

extern CGameUIGameSystem *g_pGameUIGameSystem;
