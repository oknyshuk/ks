#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Defines the interface that the GameUI dll exports

#ifndef GAMEUI_INTERFACE_H
#define GAMEUI_INTERFACE_H

#include "GameUI/IGameUI.h"


	inline constexpr const char *BACKGROUND_MUSIC_FILENAME = "mainmenu.mp3";

inline constexpr auto MAX_BACKGROUND_MUSIC = 3;


//-----------------------------------------------------------------------------
// Purpose: Implementation of GameUI's exposed interface 
//-----------------------------------------------------------------------------
class CGameUI : public IGameUI
{
public:
	CGameUI();
	~CGameUI();

	virtual void Initialize( CreateInterfaceFn appFactory );
	virtual void Connect( CreateInterfaceFn gameFactory );
	virtual void Start();
	virtual void Shutdown();
	virtual void RunFrame();
	virtual void PostInit();

	// plays the startup mp3 when GameUI starts
	void PlayGameStartupSound();

	// Engine wrappers for activating / hiding the gameUI
	void ActivateGameUI();
	void HideGameUI();

	// Toggle allowing the engine to hide the game UI with the escape key
	void PreventEngineHideGameUI();
	void AllowEngineHideGameUI();

	// notifications
	virtual void OnGameUIActivated();
	virtual void OnGameUIHidden();
	virtual void OLD_OnConnectToServer( const char *game, int IP, int port );	// OLD: use OnConnectToServer2
	virtual void OnConnectToServer2( const char *game, int IP, int connectionPort );
	virtual void OnDisconnectFromServer();
	virtual void OnLevelLoadingStarted( const char *levelName, bool bShowProgressDialog );
	virtual void OnLevelLoadingFinished( bool bError, const char *failureReason, const char *extendedReason );
	virtual void StartLoadingScreenForCommand( const char* command );
	virtual void StartLoadingScreenForKeyValues( KeyValues* keyValues );

	// progress
	virtual bool UpdateProgressBar(float progress, const char *statusText, bool showDialog = true );
	// Shows progress desc, returns previous setting... (used with custom progress bars )
	virtual bool SetShowProgressText( bool show );
	virtual bool UpdateSecondaryProgressBar(float progress, const wchar_t *desc );

	// Xbox 360
	virtual void ShowMessageDialog( const char* messageID, const char* titleID );

	virtual void CreateCommandMsgBox( const char* pszTitle, const char* pszMessage, bool showOk = true, bool showCancel = false, const char* okCommand = nullptr, const char* cancelCommand = nullptr, const char* closedCommand = nullptr, const char* pszLegend = nullptr );
	virtual void CreateCommandMsgBoxInSlot( ECommandMsgBoxSlot slot, const char* pszTitle, const char* pszMessage, bool showOk = true, bool showCancel = false, const char* okCommand = nullptr, const char* cancelCommand = nullptr, const char* closedCommand = nullptr, const char* pszLegend = nullptr );


	// Allows the level loading progress to show map-specific info
	virtual void SetProgressLevelName( const char *levelName );

 	virtual void NeedConnectionProblemWaitScreen();

	virtual void ShowPasswordUI( char const *pchCurrentPW );

	virtual bool LoadingProgressWantsIsolatedRender( bool bContextValid );

	virtual void RestoreTopLevelMenu();

 	virtual void SetProgressOnStart();
 

 	// state
 	bool IsInLevel();
 	bool IsInBackgroundLevel();
 	bool IsInMultiplayer();
 	bool IsConsoleUI();
 	bool HasSavedThisMenuSession();
 	void SetSavedThisMenuSession( bool bState );
 
 	void ShowLoadingBackgroundDialog();
	void HideLoadingBackgroundDialog();
	bool HasLoadingBackgroundDialog();

	virtual bool IsPlayingFullScreenVideo();

	virtual bool IsTransitionEffectEnabled();

	void SetBackgroundMusicDesired( bool bPlayMusic );

	void SetPreviewBackgroundMusic( const char * pchPreviewMusicPrefix );

	void ReleaseBackgroundMusic( void );

	void StartBackgroundMusicFade( void );

	void PlayQuestAudio( const char * pchAudioFile );
	void StopQuestAudio( void );
	bool IsQuestAudioPlaying( void );

private:

	virtual void StartProgressBar();
	virtual bool ContinueProgressBar(float progressFraction, bool showDialog = true );
	virtual void StopProgressBar(bool bError, const char *failureReason, const char *extendedReason = nullptr);
	virtual bool SetProgressBarStatusText(const char *statusText, bool showDialog = true );

	//!! these functions currently not implemented
	virtual void SetSecondaryProgressBar(float progress /* range [0..1] */);
	virtual void SetSecondaryProgressBarText( const wchar_t *desc );

	bool FindPlatformDirectory(char *platformDir, int bufferSize);
	void GetUpdateVersion( char *pszProd, char *pszVer);
	void ValidateCDKey();


	bool IsBackgroundMusicPlaying( void );
	void UpdateBackgroundMusic( void );

	CreateInterfaceFn m_GameFactory;

	bool m_bTryingToLoadFriends : 1;
	bool m_bActivatedUI : 1;
	bool m_bIsConsoleUI : 1;
	bool m_bHasSavedThisMenuSession : 1;
	bool m_bOpenProgressOnStart : 1;

	int m_iGameIP;
	int m_iGameConnectionPort;
	
	int m_iFriendsLoadPauseFrames;
	int m_iPlayGameStartupSound;

	char m_szPreviousStatusText[128];
	char m_szPlatformDir[MAX_PATH];

	int m_nBackgroundMusicGUID;
	bool m_bBackgroundMusicDesired;
	int m_nBackgroundMusicVersion;
	float m_flBackgroundMusicStopTime;
	const char *m_pMusicExtension;

	const char *m_pPreviewMusicExtension;
	float m_flMainMenuMusicVolume;
	float m_flMasterMusicVolume;

	float m_flQuestAudioTimeEnd;
	float m_flMasterMusicVolumeSavedForMissionAudio;
	float m_flMenuMusicVolumeSavedForMissionAudio;

	int m_nQuestAudioGUID;

	const char * m_pAudioFile;

};

// Purpose: singleton accessor
extern CGameUI &GameUI();

// expose client interface
extern IGameClientExports *GameClientExports();


#endif // GAMEUI_INTERFACE_H
