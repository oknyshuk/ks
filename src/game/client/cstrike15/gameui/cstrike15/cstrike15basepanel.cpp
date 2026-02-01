//===== Copyright  1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Main panel for CS:GO UI
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "cstrike15basepanel.h"

#include "engineinterface.h"
#include "steam/steam_api.h"
#include "vguisystemmoduleloader.h"

#include "gameui_interface.h"
#include "uigamedata.h"
#include "cdll_client_int.h"

#include "vgui/ILocalize.h"

#include "../RocketUI/rkmenu_main.h"
#include "../RocketUI/rkhud_pausemenu.h"

#if defined ( _PS3 )

#include "ps3/saverestore_ps3_api_ui.h"
#include "sysutil/sysutil_savedata.h"
#include "sysutil/sysutil_gamecontent.h"
#include "cell/sysmodule.h"
static int s_nPs3SaveStorageSizeKB = 5*1024;
static int s_nPs3TrophyStorageSizeKB = 0;

#include "steamoverlay/isteamoverlaymgr.h"

#endif


#include "cdll_util.h"
#include "c_baseplayer.h"
#include "c_cs_player.h"
#include "inputsystem/iinputsystem.h"

#include "cstrike15_gcmessages.pb.h"

#if defined( _X360 )
#include "xparty.h" // For displaying the Party Voice -> Game Voice notification, per requirements
#include "xbox/xbox_launch.h"
#endif

#include "cs_gamerules.h"
#include "clientmode_csnormal.h"
#include "c_cs_playerresource.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static CCStrike15BasePanel *g_pCStrike15BasePanel = NULL;

// [jason] For tracking the last team played by the main user (TEAM_CT by default)
ConVar player_teamplayedlast( "player_teamplayedlast", "3", FCVAR_ARCHIVE | FCVAR_ARCHIVE_GAMECONSOLE | FCVAR_SS );
// for tracking whether the player dismissed the community server warning message
ConVar player_nevershow_communityservermessage( "player_nevershow_communityservermessage", "0", FCVAR_ARCHIVE | FCVAR_ARCHIVE_GAMECONSOLE | FCVAR_SS );

static bool s_bSteamOverlayPositionNeedsToBeSet = true;
static void FnSteamOverlayChangeCallback( IConVar *var, const char *pOldValue, float flOldValue )
{
	s_bSteamOverlayPositionNeedsToBeSet = true;
}
ConVar ui_steam_overlay_notification_position( "ui_steam_overlay_notification_position", "topleft", FCVAR_ARCHIVE, "Steam overlay notification position", FnSteamOverlayChangeCallback );

float g_flReadyToCheckForPCBootInvite = 0;
static ConVar connect_lobby( "connect_lobby", "", FCVAR_HIDDEN, "Sets the lobby ID to connect to on start." );


#ifdef _PS3

static CPS3SaveRestoreAsyncStatus s_PS3SaveAsyncStatus;
enum SaveInitializeState_t
{
	SIS_DEFAULT,
	SIS_INIT_REQUESTED,
	SIS_FINISHED
};
static SaveInitializeState_t s_ePS3SaveInitState;

#endif

//-----------------------------------------------------------------------------
// Purpose: singleton accessor
//-----------------------------------------------------------------------------
CBaseModPanel *BasePanel()
{
	return g_pCStrike15BasePanel;
}

//-----------------------------------------------------------------------------
// Purpose: singleton accessor (constructs)
//-----------------------------------------------------------------------------
CBaseModPanel *BasePanelSingleton()
{
	if ( !g_pCStrike15BasePanel )
	{
		g_pCStrike15BasePanel = new CCStrike15BasePanel();
	}
	return g_pCStrike15BasePanel;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CCStrike15BasePanel::CCStrike15BasePanel() :
	BaseClass( "CStrike15BasePanel" ),
	m_pSplitScreenSignon( NULL ),
	m_OnClosedCommand( ON_CLOSED_NULL ),
	m_bMigratingActive( false ),
	m_bShowRequiredGameVoiceChannelUI( false ),
    m_bNeedToStartIntroMovie( true ),
    m_bTestedStaticIntroMovieDependencies( false ),
	m_bStartLogoIsShowing( false ),
	m_bServerBrowserWarningRaised( false ),
	m_bCommunityQuickPlayWarningRaised( false ),
	m_bCommunityServerWarningIssued( false ),
	m_bGameIsShuttingDown( false )
{
	g_pMatchFramework->GetEventsSubscription()->Subscribe( this );

#if defined ( _X360 )
	if ( xboxsystem )
		xboxsystem->UpdateArcadeTitleUnlockStatus();
#endif
}

//-----------------------------------------------------------------------------
CCStrike15BasePanel::~CCStrike15BasePanel()
{
	m_bGameIsShuttingDown = true;

	StopListeningForAllEvents();

	// [jason] Release any screens that may still be active on shutdown so we don't leak memory
	DismissAllMainMenuScreens();
}

#if defined( _X360 )
void CCStrike15BasePanel::Xbox_PromptSwitchToGameVoiceChannel( void )
{
	// Clear this flag by default - only enable it if we actually raise the UI
	m_bShowRequiredGameVoiceChannelUI = false;

	// We must be in an Online game in order to proceed
	bool bOnline = g_pMatchFramework && g_pMatchFramework->IsOnlineGame();

	if ( bOnline && Xbox_IsPartyChatEnabled() )
	{
		if ( BaseModUI::CUIGameData::Get()->IsXUIOpen() )
		{
			// Wait for the XUI to go away before prompting for this
			m_GameVoiceChannelRecheckTimer.Start( 1.f );
		}
		else
		{
			m_bShowRequiredGameVoiceChannelUI = true;
			XShowRequiredGameVoiceChannelUI();
		}
	}
}

bool CCStrike15BasePanel::Xbox_IsPartyChatEnabled( void )
{
	XPARTY_USER_LIST xpUserList;
	if  ( XPartyGetUserList( &xpUserList ) != XPARTY_E_NOT_IN_PARTY )
	{
		for ( DWORD idx = 0; idx < xpUserList.dwUserCount; ++idx )
		{
			// Detect if the local user is in the Party, and is using Party Voice channel
			if ( xpUserList.Users[idx].dwFlags & ( XPARTY_USER_ISLOCAL | XPARTY_USER_ISINPARTYVOICE ) )
			{
				return true;
			}
		}
	}
	return false;
}
#endif

void CCStrike15BasePanel::OnEvent( KeyValues *pEvent )
{
	const char *pEventName = pEvent->GetName();

#if defined( _X360 )
	//
	// Handler for switching the Xbox LIVE voice channel (Game vs Party chat channels)
	if ( !Q_strcmp( pEventName, "OnLiveVoicechatAway" ) )
	{
		bool bNotTitleChat = ( pEvent->GetInt( "NotTitleChat", 0 ) != 0 );

		// If we switched to non-Game Chat, then prompt the user to switch back if he's in an Online game
		if ( bNotTitleChat )
		{
			Xbox_PromptSwitchToGameVoiceChannel();
		}
		else
		{
			m_bShowRequiredGameVoiceChannelUI = false;
			m_GameVoiceChannelRecheckTimer.Invalidate();
		}
	}

	// Detect the next system XUI closed event
	if ( !Q_stricmp( pEventName, "OnSysXUIEvent" ) 	&&
		 !Q_stricmp( "closed", pEvent->GetString( "action", "" ) ) )
	{
		if ( m_bShowRequiredGameVoiceChannelUI )
		{
			// If it was the game voice channel UI that closed, wait a few ticks to see if Game Chat is re-enabled
			m_GameVoiceChannelRecheckTimer.Start( 1.0f );
		}
	}
#endif

	if ( !Q_strcmp( pEventName, "OnSysSigninChange" ) )
	{
		if ( !Q_stricmp( "signout", pEvent->GetString( "action", "" ) ) )
		{
			int primaryID = pEvent->GetInt( "user0", -1 );

			ACTIVE_SPLITSCREEN_PLAYER_GUARD( GET_ACTIVE_SPLITSCREEN_SLOT() );
			if ( XBX_GetSlotByUserId( primaryID ) != -1 )
			{
				// go to the start screen
				// $TODO ?? display message stating you got booted back to start cuz you signed out
				if ( GameUI().IsInLevel() )
				{
					m_bForceStartScreen = true;
					engine->ClientCmd_Unrestricted( "disconnect" );
				}
				else
				{
					HandleOpenCreateStartScreen();
				}
			}
		}
		if ( !Q_stricmp( "signin", pEvent->GetString( "action", "" ) ) )
		{
#if defined ( _X360 )
			xboxsystem->UpdateArcadeTitleUnlockStatus();
#endif
		}

		UpdateRichPresenceInfo();
	}
#if defined( _X360 )
	else if ( !Q_stricmp( pEventName, "OnEngineDisconnectReason" ) )
	{
		if ( char const *szDisconnectHdlr = pEvent->GetString( "disconnecthdlr", NULL ) )
		{
			if ( !Q_stricmp( szDisconnectHdlr, "lobby" ) )
			{
				// Make sure the main menu is hidden
				CCreateMainMenuScreenScaleform::ShowPanel( false, true );

				// Flag the main menu to stay hidden during migration, unless we cancel
				m_bMigratingActive = true;

				OnOpenMessageBox( "#SFUI_MainMenu_MigrateHost_Title", "#SFUI_MainMenu_MigrateHost_Message", "#SFUI_MainMenu_MigrateHost_Navigation", (MESSAGEBOX_FLAG_CANCEL | MESSAGEBOX_FLAG_BOX_CLOSED), this );
			}
		}
		else
		{
			// Clear the migrating flag in all other disconnect cases
			m_bMigratingActive = false;

			// Clear the flags governing game voice channel for Xbox when we disconnect as well
			m_bShowRequiredGameVoiceChannelUI = false;
			m_GameVoiceChannelRecheckTimer.Invalidate();
		}
	}
	else if ( !Q_stricmp( pEventName, "OnEngineLevelLoadingStarted" ) || !V_stricmp( pEventName, "LoadingScreenOpened" ) )
	{
		// Clear the migrating flag once level loading starts
		m_bMigratingActive = false;
	}
#endif // _X360
	else if ( !Q_stricmp( pEventName, "OnDemoFileEndReached" ) )
	{
		if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
		{
			if ( pParameters->m_bAnonymousPlayerIdentity &&
				pParameters->m_uiLockFirstPersonAccountID &&
				pParameters->m_uiCaseID )
			{
#if defined( INCLUDE_SCALEFORM )
				SFHudOverwatchResolutionPanel::LoadDialog();
#endif
			}
		}
		engine->ClientCmd_Unrestricted( "disconnect" );
	}
}

void CCStrike15BasePanel::FireGameEvent( IGameEvent *event )
{
	const char *type = event->GetName();
	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();

	if ( StringHasPrefix( type, "player_team" ) )
	{
		if ( pLocalPlayer )
		{
			int newTeam = event->GetInt( "team" );
			int userId = event->GetInt( "userid" );

			if ( pLocalPlayer->GetUserID() == userId )
			{
				if ( newTeam == TEAM_CT || newTeam == TEAM_TERRORIST )
					player_teamplayedlast.SetValue( newTeam );
			}
		}
	}
	else if ( StringHasPrefix( type, "cs_game_disconnected" ) )
	{
		if ( GameUI().IsInLevel() )
		{
			// Ensure we remove any pending dialogs as soon as we receive notification that the client is disconnecting
			//	(fixes issue with the quit dialog staying up when you "disconnect" via console window)
			//  Passing in false to indicate we do not wish to dismiss CCommandMsgBoxes, which indicate error codes/kick reasons/etc
#if defined( INCLUDE_SCALEFORM )
			CMessageBoxScaleform::UnloadAllDialogs( false );
#endif
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//
// Avatars conversion to rgb
//
#include "imageutils.h"
CON_COMMAND_F( cl_avatar_convert_rgb, "Converts all png avatars in the avatars directory to rgb", FCVAR_RELEASE | FCVAR_CHEAT )
{
	FileFindHandle_t hFind = NULL;
	for ( char const *szFileName = g_pFullFileSystem->FindFirst( "avatars/*.png", &hFind );
		szFileName && *szFileName; szFileName = g_pFullFileSystem->FindNext( hFind ) )
	{
		CFmtStr sFile( "avatars/%s", szFileName );
		char chRgbFile[MAX_PATH] = {};
		V_strcpy_safe( chRgbFile, sFile );
		V_SetExtension( chRgbFile, ".rgb", MAX_PATH );

		int width = 0, height = 0;
		ConversionErrorType cet = CE_ERROR_WRITING_OUTPUT_FILE;
		unsigned char *pbImgData = ImgUtl_ReadPNGAsRGBA( sFile, width, height, cet );

		if ( ( cet == CE_SUCCESS ) && ( width == 64 ) && ( height == 64 ) && pbImgData )
		{
			// trim alpha for size
			for ( int y = 0; y < 64; ++y ) for ( int x = 0; x < 64; ++x )
			{
				V_memmove( pbImgData + y * 64 * 3 + x * 3, pbImgData + y * 64 * 4 + x * 4, 3 );
			}
			CUtlBuffer bufWriteExternal( pbImgData, 64*64*3, CUtlBuffer::READ_ONLY );
			bufWriteExternal.SeekPut( CUtlBuffer::SEEK_HEAD, 64*64*3 );
			if ( g_pFullFileSystem->WriteFile( chRgbFile, "MOD", bufWriteExternal ) )
			{
				Msg( "Converted rgb '%s'->'%s'.\n", sFile.Access(), chRgbFile );
			}
			else
			{
				Warning( "Failed to save converted rgb '%s'->'%s'.\n", sFile.Access(), chRgbFile );
			}
		}
		else
		{
			Warning( "Invalid conversion source '%s' (%d/%p; %dx%d), expecting 64x64 PNG.\n", sFile.Access(), cet, pbImgData, width, height );
		}

		if ( pbImgData )
		{
			free( pbImgData );
		}
	}
	g_pFullFileSystem->FindClose( hFind );
}

void CCStrike15BasePanel::OnOpenServerBrowser()
{
#if !defined(_GAMECONSOLE)
#if defined(INCLUDE_SCALEFORM)
	if (!m_bCommunityServerWarningIssued && player_nevershow_communityservermessage.GetBool() == 0)
	{
		OnOpenMessageBoxThreeway("#SFUI_MainMenu_ServerBrowserWarning_Title", "#SFUI_MainMenu_ServerBrowserWarning_Text2", "#SFUI_MainMenu_ServerBrowserWarning_Legend", "#SFUI_MainMenu_ServerBrowserWarning_NeverShow", (MESSAGEBOX_FLAG_OK | MESSAGEBOX_FLAG_CANCEL | MESSAGEBOX_FLAG_TERTIARY), this);
		m_bServerBrowserWarningRaised = true;
	}
	else
	{
		g_VModuleLoader.ActivateModule("Servers");
	}
#else // !INCLUDE_SCALEFORM
	g_VModuleLoader.ActivateModule("Servers");
#endif // INCLUDE_SCALEFORM
#endif // !_GAMECONSOLE
}

void CCStrike15BasePanel::OnOpenCreateStartScreen( void )
{
}

void CCStrike15BasePanel::DismissStartScreen()
{
}

bool CCStrike15BasePanel::IsStartScreenActive()
{
    return false;
}

void CCStrike15BasePanel::OnOpenCreateMainMenuScreen( void )
{
    ListenForGameEvent( "player_team" );
    ListenForGameEvent( "cs_game_disconnected" );

    RocketMainMenuDocument::LoadDialog( );
}

void CCStrike15BasePanel::DismissMainMenuScreen( void )
{
    RocketMainMenuDocument::UnloadDialog( );
}

void CCStrike15BasePanel::DismissAllMainMenuScreens( bool bHideMainMenuOnly )
{
    // Either hide the menus, or tear them down.
    if( bHideMainMenuOnly )
    {
        if( CCStrike15BasePanel::IsRocketMainMenuEnabled() )
            RocketMainMenuDocument::ShowPanel( false );
    }
    else
    {
        CCStrike15BasePanel::DismissMainMenuScreen();
        CCStrike15BasePanel::DismissPauseMenu();
    }

    // Close all menu screens that may have been opened from main or pause menu
}

void CCStrike15BasePanel::RestoreMainMenuScreen( void )
{

}

void CCStrike15BasePanel::RestoreMPGameMenu( void )
{

}

void CCStrike15BasePanel::ShowRocketMainMenu( bool bShow )
{
    if( bShow && !IsRocketMainMenuEnabled() )
        return;

    RocketMainMenuDocument::ShowPanel( bShow );
}

bool CCStrike15BasePanel::IsRocketMainMenuActive( void )
{
    return RocketMainMenuDocument::IsActive();
}

void CCStrike15BasePanel::OnOpenCreateSingleplayerGameDialog( bool bMatchmakingFilter )
{
    /* Removed for partner depot */
}

void CCStrike15BasePanel::OnOpenCreateMultiplayerGameDialog( void )
{
    // Continue to support the vgui create server dialog
    CBaseModPanel::OnOpenCreateMultiplayerGameDialog();
}

void CCStrike15BasePanel::OnOpenCreateMultiplayerGameCommunity( void )
{

}


void CCStrike15BasePanel::DoCommunityQuickPlay( void )
{
}

void CCStrike15BasePanel::OnOpenCreateLobbyScreen( bool bIsHost )
{
}

void CCStrike15BasePanel::OnOpenLobbyBrowserScreen( bool bIsHost )
{
}

void CCStrike15BasePanel::UpdateLobbyScreen( )
{
}

void CCStrike15BasePanel::UpdateMainMenuScreen()
{
}

void CCStrike15BasePanel::UpdateLobbyBrowser( )
{
}

void CCStrike15BasePanel::ShowMatchmakingStatus( void )
{
}

void CCStrike15BasePanel::OnOpenPauseMenu( void )
{
    ConMsg("OnOpenPauseMenu\n");
    ShowRocketPauseMenu( true );
    //CBaseModPanel::OnOpenPauseMenu();
}

void CCStrike15BasePanel::OnOpenMouseDialog()
{
}

void CCStrike15BasePanel::OnOpenKeyboardDialog()
{
}

void CCStrike15BasePanel::OnOpenControllerDialog( void )
{
}

void CCStrike15BasePanel::OnOpenMotionControllerMoveDialog()
{
}

void CCStrike15BasePanel::OnOpenMotionControllerSharpshooterDialog()
{
}

void CCStrike15BasePanel::OnOpenMotionControllerDialog()
{
}

void CCStrike15BasePanel::OnOpenMotionCalibrationDialog()
{
}

void CCStrike15BasePanel::OnOpenVideoSettingsDialog()
{
}


void CCStrike15BasePanel::OnOpenOptionsQueued()
{
}

void CCStrike15BasePanel::OnOpenAudioSettingsDialog()
{
}

void CCStrike15BasePanel::OnOpenSettingsDialog( void )
{
}

void CCStrike15BasePanel::OnOpenHowToPlayDialog( void )
{
}

void CCStrike15BasePanel::DismissPauseMenu( void )
{
    ShowRocketPauseMenu( false );
}

void CCStrike15BasePanel::RestorePauseMenu( void )
{
    ShowRocketPauseMenu( true );
}

void CCStrike15BasePanel::ShowRocketPauseMenu( bool bShow )
{
    RocketPauseMenuDocument::ShowPanel( bShow, true );
}

bool CCStrike15BasePanel::IsRocketPauseMenuActive( void )
{
    return RocketPauseMenuDocument::IsActive();
}

bool CCStrike15BasePanel::IsRocketPauseMenuVisible( void )
{
    return RocketPauseMenuDocument::IsVisible();
}

void CCStrike15BasePanel::OnOpenDisconnectConfirmationDialog( void )
{
}

void CCStrike15BasePanel::OnOpenQuitConfirmationDialog( bool bForceToDesktop )
{
}


void CCStrike15BasePanel::OnOpenMedalsDialog( )
{
}

void CCStrike15BasePanel::OnOpenStatsDialog( )
{
}

void CCStrike15BasePanel::CloseMedalsStatsDialog( )
{
}

void CCStrike15BasePanel::OnOpenLeaderboardsDialog( )
{
}

void CCStrike15BasePanel::OnOpenCallVoteDialog( )
{
}

void CCStrike15BasePanel::OnOpenMarketplace( )
{
}

void CCStrike15BasePanel::UpdateLeaderboardsDialog( )
{
}

void CCStrike15BasePanel::CloseLeaderboardsDialog( )
{
}

void CCStrike15BasePanel::OnOpenUpsellDialog( void )
{
}

void CCStrike15BasePanel::StartExitingProcess( void )
{
    //TODO: rocketui shutdown here.
    CBaseModPanel::StartExitingProcess();
}

void CCStrike15BasePanel::RunFrame( void )
{
    CBaseModPanel::RunFrame();
}

void CCStrike15BasePanel::LockInput( void )
{
    CBaseModPanel::LockInput();
}

void CCStrike15BasePanel::UnlockInput( void )
{
    CBaseModPanel::UnlockInput();
}
