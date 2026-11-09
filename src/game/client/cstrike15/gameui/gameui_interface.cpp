// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Implements all the functions exported by the GameUI dll

#include "rocketui/rmlui.h"
#include "predictable_entity.h"
#include "cdll_util.h"
#include "util_shared.h"
#include "baseentity_shared.h"
#include "c_basecombatweapon.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "c_baseplayer.h"
#include "vphysics_interface.h"
#include "cdll_client_int.h"
// dgoodenough - io.h and direct.h don't exist on PS3
// PS3_BUILDFIX
// @wge Fix for OSX too.
// @wge Fix for OSX too.

#ifdef SendMessage
#undef SendMessage
#endif
																
#include "filesystem.h"
#include "gameui_interface.h"

// interface to engine
#include "engineinterface.h"

#include "bitmap/tgaloader.h"

#include "modinfo.h"
#include "game/client/IGameClientExports.h"
#include "materialsystem/imaterialsystem.h"
#include "matchmaking/imatchframework.h"
#include "iachievementmgr.h"
#include "IGameUIFuncs.h"
#include "iengineui.h"

// vgui2 interface
// note that GameUI project uses ..\vgui2\include, not ..\utils\vgui\include
#include "localize/ilocalize.h"
#include "tier3/tier3.h"
#include "GameUI/IGameUI.h"

// VGUI menu shell (CBaseModPanel/CCStrike15BasePanel) removed in VGUI teardown.
// Menu open/close is routed directly to the RmlUi documents below.
#include "../RocketUI/rkmenu_main.h"
#include "../RocketUI/rkhud_pausemenu.h"
#include "../RocketUI/rkhud_loadingscreen.h"

// dgoodenough - select correct stub header based on current console
// PS3_BUILDFIX

#include "engine/IEngineSound.h"
#include "gameui_util.h"

IEngineUI *engineuifuncs = nullptr;
// dgoodenough - xonline only exists on the 360.  All uses of xonline have had their
// protection changed like this one
// PS3_BUILDFIX
// FIXME we will have to put in something for Playstation Home.
IAchievementMgr *achievementmgr = nullptr;

class CGameUI;
CGameUI *g_pGameUI = nullptr;

static CGameUI g_GameUI;

//IRocketUI* RocketUI()
//{
//    return g_pRocketUI;
//}


static IGameClientExports *g_pGameClientExports = nullptr;
IGameClientExports *GameClientExports()
{
	return g_pGameClientExports;
}

//-----------------------------------------------------------------------------
// Purpose: singleton accessor
//-----------------------------------------------------------------------------
CGameUI &GameUI()
{
	return g_GameUI;
}

static InterfaceReg s_Reg_CGameUI_IGameUI( []() -> void * { return static_cast<IGameUI *>( &g_GameUI ); }, GAMEUI_INTERFACE_VERSION );;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CGameUI::CGameUI()
{
	g_pGameUI = this;
	m_bTryingToLoadFriends = false;
	m_iFriendsLoadPauseFrames = 0;
	m_iGameIP = 0;
	m_iGameConnectionPort = 0;
	m_bActivatedUI = false;
	m_szPreviousStatusText[0] = 0;
	m_bIsConsoleUI = false;
	m_bHasSavedThisMenuSession = false;
	m_bOpenProgressOnStart = false;
	m_iPlayGameStartupSound = 0;
	m_nBackgroundMusicGUID = 0;
	m_bBackgroundMusicDesired = false;
	m_nBackgroundMusicVersion = RandomInt( 1, MAX_BACKGROUND_MUSIC );
	m_flBackgroundMusicStopTime = -1.0;
	m_pMusicExtension = nullptr;
	m_pPreviewMusicExtension = nullptr;
	m_flMainMenuMusicVolume = -1;
	m_flMasterMusicVolume = -1;
	m_flQuestAudioTimeEnd = 0;
	m_flMasterMusicVolumeSavedForMissionAudio = -1;
	m_flMenuMusicVolumeSavedForMissionAudio = -1;
	m_nQuestAudioGUID = 0;

}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CGameUI::~CGameUI()
{
	g_pGameUI = nullptr;
}


//-----------------------------------------------------------------------------
// Purpose: Initialization
//-----------------------------------------------------------------------------
void CGameUI::Initialize( CreateInterfaceFn factory )
{
	// The tier libraries are shared with the engine, which has connected them.
	ConVar_Register( ks::client_registry().convars, FCVAR_CLIENTDLL );

	enginesound = (IEngineSound *)factory(IENGINESOUND_CLIENT_INTERFACE_VERSION, nullptr);
	engine = (IVEngineClient *)factory( VENGINE_CLIENT_INTERFACE_VERSION, nullptr );

	CGameUIConVarRef var( "gameui_xbox" );
	m_bIsConsoleUI = var.IsValid() && var.GetBool();

	// load localization file
	g_pLocalize->AddFile( "resource/gameui_%language%.txt", "GAME", true );

	// load mod info
	ModInfo().LoadCurrentGameInfo();

	// load localization file for kb_act.lst
	g_pLocalize->AddFile( "resource/valve_%language%.txt", "GAME", true );

	bool bFailed = false;
	engineuifuncs = (IEngineUI *)factory( VENGINE_UI_VERSION, nullptr );
	gameuifuncs = (IGameUIFuncs *)factory( VENGINE_GAMEUIFUNCS_VERSION, nullptr );
// dgoodenough - xonline only exists on the 360.
// PS3_BUILDFIX
	bFailed = !gameuifuncs || !engineuifuncs ||
// dgoodenough - xonline only exists on the 360.
// PS3_BUILDFIX
		!g_pMatchFramework;

	if ( bFailed )
	{
		Error( "CGameUI::Initialize() failed to get necessary interfaces\n" );
	}

	// VGUI base panel removed; the RmlUi documents host the menu. Nothing to construct here.
}

void CGameUI::PostInit()
{

}

//-----------------------------------------------------------------------------
// Purpose: connects to client interfaces
//-----------------------------------------------------------------------------
void CGameUI::Connect( CreateInterfaceFn gameFactory )
{
	g_pGameClientExports = (IGameClientExports *)gameFactory(GAMECLIENTEXPORTS_INTERFACE_VERSION, nullptr);

	achievementmgr = engine->GetAchievementMgr();

	if (!g_pGameClientExports)
	{
		Error("CGameUI::Initialize() failed to get necessary interfaces\n");
	}

	m_GameFactory = gameFactory;
}

//-----------------------------------------------------------------------------
// Purpose: Searches for GameStartup*.mp3 files in the sound/ui folder and plays one
//-----------------------------------------------------------------------------
void CGameUI::PlayGameStartupSound()
{
	// CS15 not using this path. using Portal 2 style MP3 looping
	// L4D not using this path, L4D UI now handling with background menu movies   	
	return;

	if ( CommandLine()->FindParm( "-nostartupsound" ) )
		return;

	FileFindHandle_t fh;

	std::vector< std::string > fileNames;

	char path[ 512 ];
	V_snprintf( path, sizeof( path ), "sound/ui/gamestartup*.mp3" );
	V_FixSlashes( path );

	char const *fn = g_pFullFileSystem->FindFirstEx( path, "MOD", &fh );
	if ( fn )
	{
		do
		{
			char ext[ 10 ];
			V_ExtractFileExtension( fn, ext, sizeof( ext ) );

			if ( !V_stricmp( ext, "mp3" ) )
			{
				char temp[ 512 ];
				V_snprintf( temp, sizeof( temp ), "ui/%s", fn );

				V_FixSlashes( temp );
				fileNames.push_back( temp );
			}
	
			fn = g_pFullFileSystem->FindNext( fh );

		} while ( fn );

		g_pFullFileSystem->FindClose( fh );
	}

	// did we find any?
	if ( !fileNames.empty() )
	{
// dgoodenough - SystemTime is absent on PS3, just select first file for now
// PS3_BUILDFIX
// FIXME - we need to find some sort of entropy here and select based on that.
// @wge Fix for OSX too.
		int index = 0;
		if ( ks::is_valid_index( fileNames, index ) )
		{
			char found[ 512 ];

			// escape chars "*#" make it stream, and be affected by snd_musicvolume
			V_snprintf( found, sizeof( found ), "play *#%s", fileNames[index].c_str() );

			engine->ClientCmd_Unrestricted( found );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called to setup the game UI
//-----------------------------------------------------------------------------
void CGameUI::Start()
{
	// determine platform location for configuration
	if ( !FindPlatformDirectory( m_szPlatformDir, sizeof( m_szPlatformDir ) ) )
		return;

	// setup config file directory
	char szConfigDir[512];
	V_strncpy( szConfigDir, m_szPlatformDir, sizeof( szConfigDir ) );
	V_strncat( szConfigDir, "config", sizeof( szConfigDir ), COPY_ALL_CHARACTERS );

	Msg( "Config directory: %s\n", szConfigDir );

	g_pFullFileSystem->AddSearchPath(szConfigDir, "CONFIG");
	g_pFullFileSystem->CreateDirHierarchy("", "CONFIG");

	g_pFullFileSystem->AddSearchPath( "platform", "PLATFORM" );

	// localization
	g_pLocalize->AddFile( "resource/platform_%language%.txt");
	g_pLocalize->AddFile( "resource/vgui_%language%.txt");

	// ********************************************************************
	// The following is commented out to keep intro music from playing
	// before the intro movie:
	//
	//// Delay playing the startup music until two frames
	//// this allows cbuf commands that occur on the first frame that may start a map
	//m_iPlayGameStartupSound = 2;
	//SetBackgroundMusicDesired( true );
	// ********************************************************************

	// now we are set up to check every frame to see if we can friends/server browser
	m_bTryingToLoadFriends = true;
	m_iFriendsLoadPauseFrames = 1;
}

//-----------------------------------------------------------------------------
// Purpose: Validates the user has a cdkey in the registry
//-----------------------------------------------------------------------------
void CGameUI::ValidateCDKey()
{
}

//-----------------------------------------------------------------------------
// Purpose: Finds which directory the platform resides in
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CGameUI::FindPlatformDirectory(char *platformDir, int bufferSize)
{
	platformDir[0] = '\0';

	if ( platformDir[0] == '\0' )
	{
		// setup using path relative to game
		if ( ks::platform::current_directory( platformDir, (unsigned long)bufferSize ) )
		{
			V_AppendSlash( platformDir, bufferSize );
			V_strncat(platformDir, "platform", bufferSize, COPY_ALL_CHARACTERS );
			V_AppendSlash( platformDir, bufferSize );
			return true;
		}

		Error( "Unable to determine platform directory\n" );
		return false;
	}

	return (platformDir[0] != 0);
}

//-----------------------------------------------------------------------------
// Purpose: Called to Shutdown the game UI system
//-----------------------------------------------------------------------------
void CGameUI::Shutdown()
{

	// notify all the modules of Shutdown

	ModInfo().FreeModInfo();
	
	ConVar_Unregister( ks::client_registry().convars );
}

//-----------------------------------------------------------------------------
// Purpose: just wraps an engine call to activate the gameUI
//-----------------------------------------------------------------------------
void CGameUI::ActivateGameUI()
{
	engine->ExecuteClientCmd("gameui_activate");
	// Lock the UI to a particular player
	SetGameUIActiveSplitScreenPlayerSlot( engine->GetActiveSplitScreenPlayerSlot() );
}

//-----------------------------------------------------------------------------
// Purpose: just wraps an engine call to hide the gameUI
//-----------------------------------------------------------------------------
void CGameUI::HideGameUI()
{
	engine->ExecuteClientCmd("gameui_hide");
}

//-----------------------------------------------------------------------------
// Purpose: Toggle allowing the engine to hide the game UI with the escape key
//-----------------------------------------------------------------------------
void CGameUI::PreventEngineHideGameUI()
{
	engine->ExecuteClientCmd("gameui_preventescape");
}

//-----------------------------------------------------------------------------
// Purpose: Toggle allowing the engine to hide the game UI with the escape key
//-----------------------------------------------------------------------------
void CGameUI::AllowEngineHideGameUI()
{
	engine->ExecuteClientCmd("gameui_allowescape");
}

//-----------------------------------------------------------------------------
// Purpose: Activate the game UI
//-----------------------------------------------------------------------------
void CGameUI::OnGameUIActivated()
{
	bool bWasActive = m_bActivatedUI;
	m_bActivatedUI = true;

	// Lock the UI to a particular player
	if ( !bWasActive )
	{
		SetGameUIActiveSplitScreenPlayerSlot( engine->GetActiveSplitScreenPlayerSlot() );
	}

	// pause the server in case it is pausable
	engine->ClientCmd_Unrestricted( "setpause nomsg" );

	SetSavedThisMenuSession( false );

	// Route menu activation directly to the RmlUi documents.
	if ( IsInLevel() )
	{
		RocketPauseMenuDocument::ShowPanel( true, true );
	}
	else
	{
		if ( RocketPauseMenuDocument::IsActive() )
			RocketPauseMenuDocument::ShowPanel( false, true );
		if ( !RocketMainMenuDocument::IsActive() )
			RocketMainMenuDocument::LoadDialog();
		RocketMainMenuDocument::ShowPanel( true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Hides the game ui, in whatever state it's in
//-----------------------------------------------------------------------------
void CGameUI::OnGameUIHidden()
{
	bool bWasActive = m_bActivatedUI;
	m_bActivatedUI = false;

	// unpause the game when leaving the UI
	engine->ClientCmd_Unrestricted( "unpause nomsg" );

	if ( RocketPauseMenuDocument::IsActive() )
		RocketPauseMenuDocument::ShowPanel( false, true );

	// Hide the main menu too when the game UI is dismissed (e.g. after connecting
	// to a server); otherwise it stays drawn over gameplay.
	if ( RocketMainMenuDocument::IsActive() )
		RocketMainMenuDocument::ShowPanel( false );

	// Restore to default
	if ( bWasActive )
	{
		SetGameUIActiveSplitScreenPlayerSlot( 0 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: paints all the vgui elements
//-----------------------------------------------------------------------------
void CGameUI::RunFrame()
{

	// Play the start-up music the first time we run frame
	if ( m_iPlayGameStartupSound > 0 )
	{
		m_iPlayGameStartupSound--;		
	}
	else
	{
		UpdateBackgroundMusic();
	}

	if ( m_bTryingToLoadFriends && m_iFriendsLoadPauseFrames-- < 1  )
	{
		// clear the loading flag
		m_bTryingToLoadFriends = false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called when the game connects to a server
//-----------------------------------------------------------------------------
void CGameUI::OLD_OnConnectToServer(const char *game, int IP, int port)
{
	// Nobody should use this anymore.
	// Use OnConnectToServer2 instead.
	contract_assert( false );
	OnConnectToServer2( game, IP, port );
}

//-----------------------------------------------------------------------------
// Purpose: Called when the game connects to a server
//-----------------------------------------------------------------------------
void CGameUI::OnConnectToServer2(const char *game, int IP, int connectionPort)
{
	m_iGameIP = IP;
	m_iGameConnectionPort = connectionPort;
}



//-----------------------------------------------------------------------------
// Purpose: Called when the game disconnects from a server
//-----------------------------------------------------------------------------
void CGameUI::OnDisconnectFromServer()
{
	m_iGameIP = 0;
	m_iGameConnectionPort = 0;

	IGameEvent *event = gameeventmanager->CreateEvent( "client_disconnect" );
	if ( event )
	{
		gameeventmanager->FireEventClientSide( event );
	}

}

//-----------------------------------------------------------------------------
// Purpose: activates the loading dialog on level load start
//-----------------------------------------------------------------------------
void CGameUI::OnLevelLoadingStarted( const char *levelName, bool bShowProgressDialog )
{
	GameUI().HideGameUI();

	char mapName[MAX_PATH];
	V_strcpy_safe( mapName, levelName ? levelName : "" );
	V_FixSlashes( mapName, '/' );


	if ( bShowProgressDialog )
	{
		StartProgressBar();
	}

	if ( !RocketLoadingScreenDocument::IsVisible() )
		RocketLoadingScreenDocument::ShowPanel( true );

	// Don't play the start game sound if this happens before we get to the first frame
	m_iPlayGameStartupSound = 0;
}

extern ConVar devCheatSkipInputLocking;
//-----------------------------------------------------------------------------
// Purpose: closes any level load dialog
//-----------------------------------------------------------------------------
void CGameUI::OnLevelLoadingFinished(bool bError, const char *failureReason, const char *extendedReason)
{
	StopProgressBar( bError, failureReason, extendedReason );

	// notify all the modules

	HideLoadingBackgroundDialog();

#if defined ( CSTRIKE_DLL )
	// ditto cstrike
	HideGameUI();
#endif
	
}

//-----------------------------------------------------------------------------
// Purpose: Updates progress bar
// Output : Returns true if screen should be redrawn
//-----------------------------------------------------------------------------
bool CGameUI::UpdateProgressBar(float progress, const char *statusText, bool showDialog )
{
	// if either the progress bar or the status text changes, redraw the screen
	bool bRedraw = false;

	if ( ContinueProgressBar( progress, showDialog ) )
	{
		bRedraw = true;
	}

	if ( SetProgressBarStatusText( statusText, showDialog ) )
	{
		bRedraw = true;
	}

	return bRedraw;
}

//-----------------------------------------------------------------------------
// Purpose: Updates progress bar
// Output : Returns true if screen should be redrawn
//-----------------------------------------------------------------------------
bool CGameUI::UpdateSecondaryProgressBar(float progress, const wchar_t *desc )
{
	// if either the progress bar or the status text changes, redraw the screen
	SetSecondaryProgressBar( progress );
	SetSecondaryProgressBarText( desc );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameUI::SetProgressLevelName( const char *levelName )
{
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameUI::StartProgressBar()
{
	// open a loading dialog
	m_szPreviousStatusText[0] = 0;
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the screen should be updated
//-----------------------------------------------------------------------------
bool CGameUI::ContinueProgressBar( float progressFraction, bool showDialog )
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: stops progress bar, displays error if necessary
//-----------------------------------------------------------------------------
void CGameUI::StopProgressBar(bool bError, const char *failureReason, const char *extendedReason)
{
// CStrike15 handles error messages elsewhere. (ClientModeCSFullscreen::OnEvent)
}

//-----------------------------------------------------------------------------
// Purpose: sets loading info text
//-----------------------------------------------------------------------------
bool CGameUI::SetProgressBarStatusText(const char *statusText, bool showDialog )
{
	if (!statusText)
		return false;

	if (!stricmp(statusText, m_szPreviousStatusText))
		return false;

	V_strncpy(m_szPreviousStatusText, statusText, sizeof(m_szPreviousStatusText));
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameUI::SetSecondaryProgressBar(float progress /* range [0..1] */)
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameUI::SetSecondaryProgressBarText( const wchar_t *desc )
{
	if (!desc)
		return;
}

//-----------------------------------------------------------------------------
// Purpose: Returns prev settings
//-----------------------------------------------------------------------------
bool CGameUI::SetShowProgressText( bool show )
{
    return false;
}


//-----------------------------------------------------------------------------
// Purpose: returns true if we're currently playing the game
//-----------------------------------------------------------------------------
bool CGameUI::IsInLevel()
{
	const char *levelName = engine->GetLevelName();
	if (levelName && levelName[0] && !engine->IsLevelMainMenuBackground())
	{
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: returns true if we're at the main menu and a background level is loaded
//-----------------------------------------------------------------------------
bool CGameUI::IsInBackgroundLevel()
{
	const char *levelName = engine->GetLevelName();
	if (levelName && levelName[0] && engine->IsLevelMainMenuBackground())
	{
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: returns true if we're in a multiplayer game
//-----------------------------------------------------------------------------
bool CGameUI::IsInMultiplayer()
{
	return (IsInLevel() && engine->GetMaxClients() > 1);
}

//-----------------------------------------------------------------------------
// Purpose: returns true if we're console ui
//-----------------------------------------------------------------------------
bool CGameUI::IsConsoleUI()
{
	return m_bIsConsoleUI;
}

//-----------------------------------------------------------------------------
// Purpose: returns true if we've saved without closing the menu
//-----------------------------------------------------------------------------
bool CGameUI::HasSavedThisMenuSession()
{
	return m_bHasSavedThisMenuSession;
}

void CGameUI::SetSavedThisMenuSession( bool bState )
{
	m_bHasSavedThisMenuSession = bState;
}

//-----------------------------------------------------------------------------
// Purpose: Makes the loading background dialog visible, if one has been set
//-----------------------------------------------------------------------------
void CGameUI::ShowLoadingBackgroundDialog()
{
}


//-----------------------------------------------------------------------------
// Purpose: Hides the loading background dialog, if one has been set
//-----------------------------------------------------------------------------
void CGameUI::HideLoadingBackgroundDialog()
{
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether a loading background dialog has been set
//-----------------------------------------------------------------------------
bool CGameUI::HasLoadingBackgroundDialog()
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Xbox 360 calls from engine to GameUI 
//-----------------------------------------------------------------------------
void CGameUI::ShowMessageDialog( const char* messageID, const char* titleID )
{
}

void CGameUI::CreateCommandMsgBox( const char* pszTitle, const char* pszMessage, bool showOk, bool showCancel, const char* okCommand, const char* cancelCommand, const char* closedCommand, const char* pszLegend )
{
}

void CGameUI::CreateCommandMsgBoxInSlot( ECommandMsgBoxSlot slot, const char* pszTitle, const char* pszMessage, bool showOk, bool showCancel, const char* okCommand, const char* cancelCommand, const char* closedCommand, const char* pszLegend )
{
}


//-----------------------------------------------------------------------------

void CGameUI::NeedConnectionProblemWaitScreen()
{
}

void CGameUI::ShowPasswordUI( char const *pchCurrentPW )
{
}

//-----------------------------------------------------------------------------
void CGameUI::SetProgressOnStart()
{
	m_bOpenProgressOnStart = true;
}


bool CGameUI::IsPlayingFullScreenVideo()
{
	return false;
}

bool CGameUI::IsTransitionEffectEnabled()
{
	return false;
}

void CGameUI::StartLoadingScreenForCommand( const char* command )
{
}

void CGameUI::StartLoadingScreenForKeyValues( KeyValues* keyValues )
{
}


bool CGameUI::IsBackgroundMusicPlaying( void )
{
	if ( m_nBackgroundMusicGUID == 0 )
	{
		return false;
	}

	return enginesound->IsSoundStillPlaying( m_nBackgroundMusicGUID );
}

void CGameUI::ReleaseBackgroundMusic( void )
{
	if ( m_nBackgroundMusicGUID == 0 )
		return;
	
	enginesound->StopSoundByGuid( m_nBackgroundMusicGUID, true );


	m_nBackgroundMusicGUID = 0;
}

//The way to loop an MP3 is just to constantly check if it is playing and restart it otherwise
constexpr auto MENUMUSIC_FADETIME = 1.34;

void CGameUI::UpdateBackgroundMusic( void )
{
	if ( m_bBackgroundMusicDesired && !enginesound->GetPreventSound() )
	{	
		const char * pNewMusicExtension = "";

		uint32 unMusicID = 0;
		bool bIsPreview = false;

		if ( m_pPreviewMusicExtension && m_pPreviewMusicExtension[0] )
		{
			pNewMusicExtension = m_pPreviewMusicExtension;
			bIsPreview = true;
		}

		if ( !IsBackgroundMusicPlaying() && !enginesound->IsMoviePlaying() )
		{

			m_flBackgroundMusicStopTime = -1.0;

			char sMusicKit[128];

			m_pMusicExtension = pNewMusicExtension;

			if ( !StringIsEmpty( m_pMusicExtension ) && ( unMusicID > 1 || bIsPreview ) )
			{
				V_sprintf_safe( sMusicKit, "music/%s/%s", m_pMusicExtension, BACKGROUND_MUSIC_FILENAME );
			}
			else
			{
				m_nBackgroundMusicVersion++;

				if ( m_nBackgroundMusicVersion == 1 )
				{
					V_sprintf_safe( sMusicKit, "music/valve_csgo_02/%s", BACKGROUND_MUSIC_FILENAME );
				}
				else
				{
					m_nBackgroundMusicVersion = 0;
					V_sprintf_safe( sMusicKit, "music/valve_csgo_01/%s", BACKGROUND_MUSIC_FILENAME );
				}
			}
			m_nBackgroundMusicGUID = enginesound->EmitAmbientSound( sMusicKit, 1.0 );
			
		}
		else if ( !FStrEq( pNewMusicExtension, m_pMusicExtension ) )
		{
			ReleaseBackgroundMusic();
		}
		else if( ( m_flBackgroundMusicStopTime > -1.0 ) )
		{
			float flDelta = gpGlobals->curtime - m_flBackgroundMusicStopTime;
			float flFadeAmount = 1.0 - ( flDelta / MENUMUSIC_FADETIME );
			enginesound->SetVolumeByGuid( m_nBackgroundMusicGUID, flFadeAmount );
			if( flFadeAmount < .05 )
			{
				SetBackgroundMusicDesired( false );
			}
		}

		if ( m_nQuestAudioGUID && ( m_flQuestAudioTimeEnd < gpGlobals->curtime ) )	// resume main menu music after quest audio is complete
		{
			//		SetBackgroundMusicDesired( true );

			static ConVarRef snd_musicvolume( "snd_musicvolume" );
			static ConVarRef snd_menumusic_volume( "snd_menumusic_volume" );

			snd_musicvolume.SetValue( m_flMasterMusicVolumeSavedForMissionAudio );
			snd_menumusic_volume.SetValue( m_flMenuMusicVolumeSavedForMissionAudio );

			m_flQuestAudioTimeEnd = 0;
			m_nQuestAudioGUID = 0;
		}
	}
// 	else if ( m_pAudioFile && m_pAudioFile[ 0 ] )
// 	{
// 		if ( !IsBackgroundMusicPlaying() )
// 		{
// 			m_nBackgroundMusicGUID = enginesound->EmitAmbientSound( m_pAudioFile, 1.0 );
// 		}
// 	}
	else
	{
		ReleaseBackgroundMusic();
	}
}
void CGameUI::StartBackgroundMusicFade( void )
{
	m_flBackgroundMusicStopTime = gpGlobals->curtime;
}
void CGameUI::SetBackgroundMusicDesired( bool bPlayMusic )
{
	m_bBackgroundMusicDesired = bPlayMusic;
}

bool CGameUI::LoadingProgressWantsIsolatedRender( bool bContextValid )
{
	return false;
}

void CGameUI::RestoreTopLevelMenu()
{
	RocketMainMenuDocument::RestorePanel();
}

void CGameUI::SetPreviewBackgroundMusic( const char * pchPreviewMusicPrefix )
{
	static ConVarRef snd_menumusic_volume("snd_menumusic_volume");
	static ConVarRef snd_musicvolume("snd_musicvolume");

	// if we're previewing music, store the current volumes and set them to default.
	if ( pchPreviewMusicPrefix && pchPreviewMusicPrefix[0] )
	{
		m_flMainMenuMusicVolume = snd_menumusic_volume.GetFloat();
		m_flMasterMusicVolume = snd_musicvolume.GetFloat();

		snd_menumusic_volume.SetValue( snd_menumusic_volume.GetDefault() );
		snd_musicvolume.SetValue( snd_musicvolume.GetDefault() );
		
	}
	else
	{
		if ( m_flMainMenuMusicVolume != -1 )	// stored music is not -1 so we are previewing
		{
			snd_menumusic_volume.SetValue( m_flMainMenuMusicVolume );
			snd_musicvolume.SetValue( m_flMasterMusicVolume );
		}

		m_flMainMenuMusicVolume = -1;
		m_flMasterMusicVolume = -1;
	}

	m_pPreviewMusicExtension = pchPreviewMusicPrefix;

	ReleaseBackgroundMusic();	// Even when we preview the musickit we have, we still want to restart the track.

}

void CGameUI::PlayQuestAudio( const char * pchAudioFile )
{
constexpr auto MAINMENU_QUEST_AUDIO_MASTER_VOLUME = 1.0;
constexpr auto MAINMENU_QUEST_AUDIO_MENUMUSIC_VOLUME = 0.01;

	if ( !pchAudioFile || !pchAudioFile[0] )
		return;

	if ( m_nQuestAudioGUID )
		return;

	m_flQuestAudioTimeEnd = gpGlobals->curtime + enginesound->GetSoundDuration( pchAudioFile );

//	SetBackgroundMusicDesired( false );

	static ConVarRef snd_musicvolume( "snd_musicvolume" );

	m_flMasterMusicVolumeSavedForMissionAudio = snd_musicvolume.GetFloat();
	if ( ( snd_musicvolume.GetFloat() < MAINMENU_QUEST_AUDIO_MASTER_VOLUME ) )
	{
		snd_musicvolume.SetValue( ( float )MAINMENU_QUEST_AUDIO_MASTER_VOLUME );
	}

	static ConVarRef snd_menumusic_volume("snd_menumusic_volume");

	m_flMenuMusicVolumeSavedForMissionAudio = snd_menumusic_volume.GetFloat();
	if ( ( snd_menumusic_volume.GetFloat() > MAINMENU_QUEST_AUDIO_MENUMUSIC_VOLUME ) )
	{	
		snd_menumusic_volume.SetValue( ( float )MAINMENU_QUEST_AUDIO_MENUMUSIC_VOLUME );
	}


	m_nQuestAudioGUID = enginesound->EmitAmbientSound( pchAudioFile, 1.0 );

	if (m_nQuestAudioGUID == 0)
	{
		// Playing music failed for some reason so immediately set the music volume
		// back to its initial value.
		snd_musicvolume.SetValue( m_flMasterMusicVolumeSavedForMissionAudio );
		snd_menumusic_volume.SetValue( m_flMenuMusicVolumeSavedForMissionAudio );
		m_flQuestAudioTimeEnd = gpGlobals->curtime;
	}


}

void CGameUI::StopQuestAudio( void )
{
	enginesound->StopSoundByGuid( m_nQuestAudioGUID, true );

	if ( m_flQuestAudioTimeEnd )
		m_flQuestAudioTimeEnd = gpGlobals->curtime;

}

bool CGameUI::IsQuestAudioPlaying( void )
{
	return ( ( m_nQuestAudioGUID != 0 ) ? true : false );

}
