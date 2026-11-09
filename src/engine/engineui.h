#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
// Copyright (c) 2026 oknyshuk
import ks.tier1;

// Purpose: Defines the interface that the GameUI dll exports

#include "iengineui.h"
#include "inputsystem/ButtonCode.h"

//-----------------------------------------------------------------------------
// Foward declarations
//-----------------------------------------------------------------------------
class IMatSystemSurface;
class IGameUI;
struct InputEvent_t;
struct InputContextHandle_t__;
using InputContextHandle_t = InputContextHandle_t__ *;

IGameUI* GetGameUI( void );


//-----------------------------------------------------------------------------
// Global singleton interfaces related to VGUI 
//-----------------------------------------------------------------------------

// enumeration of level loading progress bar spots
enum LevelLoadingProgress_e
{
	PROGRESS_INVALID = -2,
	PROGRESS_DEFAULT = -1,

	PROGRESS_NONE,
	PROGRESS_CHANGELEVEL,
	PROGRESS_SPAWNSERVER,
	PROGRESS_LOADWORLDMODEL,
	PROGRESS_CRCMAP,
	PROGRESS_CRCCLIENTDLL,
	PROGRESS_CREATENETWORKSTRINGTABLES,
	PROGRESS_PRECACHEWORLD,
	PROGRESS_CLEARWORLD,
	PROGRESS_LEVELINIT,
	PROGRESS_PRECACHE,
	PROGRESS_ACTIVATESERVER,
	PROGRESS_BEGINCONNECT,
	PROGRESS_SIGNONCHALLENGE,
	PROGRESS_SIGNONCONNECT,
	PROGRESS_SIGNONCONNECTED,
	PROGRESS_PROCESSSERVERINFO,
	PROGRESS_PROCESSSTRINGTABLE,
	PROGRESS_SIGNONNEW,
	PROGRESS_SENDCLIENTINFO,
	PROGRESS_SENDSIGNONDATA,
	PROGRESS_SIGNONSPAWN,
	PROGRESS_CREATEENTITIES,
	PROGRESS_FULLYCONNECTED,
	PROGRESS_PRECACHELIGHTING,
	PROGRESS_READYTOPLAY,
	PROGRESS_HIGHESTITEM,	// must be last item in list
};



//-----------------------------------------------------------------------------
// Purpose: Centerpoint for handling all user interface in the engine
//-----------------------------------------------------------------------------
class IEngineUIInternal : public IEngineUI
{
public:

	void Init();
	void Connect();
	void Shutdown();
	bool SetVGUIDirectories();
	bool IsInitialized() const;
	CreateInterfaceFn GetGameUIFactory();
	bool Key_Event( const InputEvent_t &event );
	void UpdateButtonState( const InputEvent_t &event );
	void PostInit();

	// handlers for game UI (main menu)
	void ActivateGameUI();
	bool HideGameUI();
	bool IsGameUIVisible();

	// console
	void ShowConsole();
	void HideConsole();
	bool IsConsoleVisible();
	void ClearConsole();

	void HideDebugSystem();

	// level loading
	void OnLevelLoadingStarted( const char *levelName, bool bLocalServer );
	void OnLevelLoadingFinished();
	void NotifyOfServerConnect(const char *game, int IP, int connectionPort);
	void NotifyOfServerDisconnect();
	void EnabledProgressBarForNextLoad();
	void UpdateProgressBar(LevelLoadingProgress_e progress, bool showDialog = true );
	void UpdateCustomProgressBar( float progress, const wchar_t *desc );
	void StartCustomProgress();
	void FinishCustomProgress();
	void UpdateSecondaryProgressBarWithFile( float progress, const char *pDesc, int nBytesTotal );
	void UpdateSecondaryProgressBar( float progress, const wchar_t *desc );
	void ShowErrorMessage();
	void HideLoadingPlaque();
	void StartLoadingScreenForCommand( const char* command );
	void StartLoadingScreenForKeyValues( KeyValues* keyValues );

	// Should pause?
	bool ShouldPause();
	void SetGameDLLPanelsVisible( bool show );
	// Allows the level loading progress to show map-specific info
	void SetProgressLevelName( const char *levelName );

	void Simulate();

	void SetNotAllowedToHideGameUI( bool bNotAllowedToHide );
	void SetNotAllowedToShowGameUI( bool bNotAllowedToShow );

	void NeedConnectionProblemWaitScreen();
	void ShowPasswordUI( char const *pchCurrentPW );
	void OnToolModeChanged( bool bGameMode );

	InputContextHandle_t GetGameUIInputContext();

	bool IsPlayingFullScreenVideo();
};

// Purpose: singleton accessor
#ifndef DEDICATED
extern IEngineUIInternal *EngineUI();
#endif
