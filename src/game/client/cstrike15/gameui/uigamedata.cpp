// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2008 Valve Corporation

import std;

import ks.tier1;
#include "uigamedata.h"


#include "./GameUI/IGameUI.h"
#include "engineinterface.h"
#include "gameui_interface.h"
#include "filesystem.h"

// BaseModUI High-level windows

//#include "VFoundGames.h"
//#include "VFoundGroupGames.h"
//#include "VGameLobby.h"
//#include "VGenericConfirmation.h"
//#include "VGenericWaitScreen.h"
//#include "VInGameMainMenu.h"
//#include "VMainMenu.h"
//#include "VFooterPanel.h"
//#include "VAttractScreen.h"
//#include "VPasswordEntry.h"
// vgui controls
#include "localize/ilocalize.h"
#include "netmessages.h"
#include "gameui_util.h"
#include "cdll_client_int.h"



using namespace BaseModUI;

//setup in GameUI_Interface.cpp
// DWenger - Pulled out temporarily - extern const char *COM_GetModDirectory( void );

// DWenger - Pulled out temporarily - ConVar x360_audio_english("x360_audio_english", "0", 0, "Keeps track of whether we're forcing english in a localized language." );

ConVar demo_ui_enable( "demo_ui_enable", "", FCVAR_DEVELOPMENTONLY, "Suffix for the demo UI" );
ConVar demo_connect_string( "demo_connect_string", "", FCVAR_DEVELOPMENTONLY, "Connect string for demo UI" );

///Asyncronous Operations

ConVar mm_ping_max_green( "ping_max_green", "70" );
ConVar mm_ping_max_yellow( "ping_max_yellow", "140" );
ConVar mm_ping_max_red( "ping_max_red", "250" );

//=============================================================================

const tokenset_t< const char * > BaseModUI::s_characterPortraits[] =
{
	{ "",			"select_Random" },
	{ "random",		"select_Random" },

	//{ "BtnNamVet",	"select_Bill" },
	//{ "BtnTeenGirl",	"select_Zoey" },
	//{ "BtnBiker",		"select_Francis" },
	//{ "BtnManager",	"select_Louis" },

	{ "coach",		"s_panel_lobby_coach" },
	{ "producer",	"s_panel_lobby_producer" },
	{ "gambler",	"s_panel_lobby_gambler" },
	{ "mechanic",	"s_panel_lobby_mechanic" },

	{ "infected",	"s_panel_hand" },

	{ nullptr, "" }
};

//=============================================================================
// Xbox 360 Marketplace entry point
//=============================================================================
struct X360MarketPlaceEntryPoint
{
	DWORD dwEntryPoint;
	uint64 uiOfferID;
};
static X360MarketPlaceEntryPoint g_MarketplaceEntryPoint;


static void GoToMarketplaceForOffer()
{
// dgoodenough - this looks to be x360 specific, so tag it as such
// PS3_BUILDFIX
}

static void ShowMarketplaceUiForOffer()
{
// dgoodenough - this looks to be x360 specific, so tag it as such
// PS3_BUILDFIX
}


// Console command that's fired from the destructive action confirmation for joining a new session while already in a previous session.
static void confirm_join_new_session_exit_current( const CCommand &args );
static ConCommand confirm_join_new_session_exit_current_command( "confirm_join_new_session_exit_current", confirm_join_new_session_exit_current, "Confirm that we wish to join a new session, destroying a previous session", FCVAR_CLIENTCMD_CAN_EXECUTE | FCVAR_HIDDEN );
static void confirm_join_new_session_exit_current( const CCommand &args )
{
	g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( 
		new KeyValues( "OnInvite", "action", "join" ) );
}

//=============================================================================
//
//=============================================================================
CUIGameData* CUIGameData::m_Instance = 0;
bool CUIGameData::m_bModuleShutDown = false;

//=============================================================================
CUIGameData::CUIGameData() :
	m_CGameUIPostInit( false )
{
	m_LookSensitivity = 1.0f;

	m_flShowConnectionProblemTimer = 0.0f;
	m_flTimeLastFrame = Plat_FloatTime();
	m_bShowConnectionProblemActive = false;

	g_pMatchFramework->GetEventsSubscription()->Subscribe( this );

	m_bXUIOpen = false;

	m_bWaitingForStorageDeviceHandle = false;
	m_iStorageID = XBX_INVALID_STORAGE_ID;
	m_pAsyncJob = nullptr;

	m_pSelectStorageClient = nullptr;
}

//=============================================================================
CUIGameData::~CUIGameData()
{
	// Unsubscribe from events system
	g_pMatchFramework->GetEventsSubscription()->Unsubscribe( this );
}

//=============================================================================
CUIGameData* CUIGameData::Get()
{
	if ( !m_Instance && !m_bModuleShutDown )
	{
		m_Instance = new CUIGameData();
	}

	return m_Instance;
}

void CUIGameData::Shutdown()
{
	if ( !m_bModuleShutDown )
	{
		m_bModuleShutDown = true;
		delete m_Instance;
		m_Instance = nullptr;
	}
}


//=============================================================================
void CUIGameData::RunFrame()
{

	// DWenger - Pulled out temporarily - RunFrame_Storage();

	// DWenger - Pulled out temporarily - RunFrame_Invite();

	// msmith - Put in the RunFrame for PS3.


	if ( m_flShowConnectionProblemTimer > 0.0f )
	{
		float flCurrentTime = Plat_FloatTime();
		float flTimeElapsed = ( flCurrentTime - m_flTimeLastFrame );

		m_flTimeLastFrame = flCurrentTime;

		if ( flTimeElapsed > 0.0f )
		{
			m_flShowConnectionProblemTimer -= flTimeElapsed;
		}

		if ( m_flShowConnectionProblemTimer > 0.0f )
		{
			// DWenger - Pulled out temporarily
			/*
			if ( !m_bShowConnectionProblemActive &&
				 !CBaseModPanel::GetSingleton().IsVisible() )
			{
				GameUI().ActivateGameUI();
				OpenWaitScreen( "#GameUI_RetryingConnectionToServer", 0.0f );
				m_bShowConnectionProblemActive = true;
			}
			*/
		}
		else
		{
			// DWenger - Pulled out temporarily
			/*
			if ( m_bShowConnectionProblemActive )
			{
				// Before closing this particular waitscreen we need to establish
				// a correct navback, otherwise it will not close - Vitaliy (bugbait #51272)
				if ( CBaseModFrame *pWaitScreen = CBaseModPanel::GetSingleton().GetWindow( WT_GENERICWAITSCREEN ) )
				{
					if ( !pWaitScreen->GetNavBack() )
					{
						if ( CBaseModFrame *pIngameMenu = CBaseModPanel::GetSingleton().GetWindow( WT_INGAMEMAINMENU ) )
							pWaitScreen->SetNavBack( pIngameMenu );
					}

					if ( !pWaitScreen->GetNavBack() )
					{
						// This waitscreen will fail to close, force the close!
						pWaitScreen->Close();
					}
				}

				CloseWaitScreen( NULL, "Connection Problems" );
				GameUI().HideGameUI();
				m_bShowConnectionProblemActive = false;
			}
			*/
		}
	}
}


//=============================================================================
void CUIGameData::OnGameUIPostInit()
{
	m_CGameUIPostInit = true;
}

//=============================================================================
bool CUIGameData::CanPlayer2Join()
{
	if ( demo_ui_enable.GetString()[0] )
		return false;

	return false;
}

//=============================================================================
void CUIGameData::OpenFriendRequestPanel(int index, uint64 playerXuid)
{
// dgoodenough - this looks to be x360 specific, so tag it as such
// PS3_BUILDFIX
}

//=============================================================================
void CUIGameData::OpenInviteUI( char const *szInviteUiType )
{
}

//=============================================================================
bool CUIGameData::SignedInToLive()
{
	
	return true;
}

bool CUIGameData::AnyUserSignedInToLiveWithMultiplayerDisabled()
{

	return false;
}

bool CUIGameData::CheckAndDisplayErrorIfOffline( CBaseModFrame *pCallerFrame, char const *szMsg )
{

	return false;
}

bool CUIGameData::CheckAndDisplayErrorIfNotSignedInToLive( CBaseModFrame *pCallerFrame )
{
		return false;

	if ( SignedInToLive() )
		return false;

	char const *szMsg = "";

	if ( AnyUserSignedInToLiveWithMultiplayerDisabled() )
	{
		szMsg = "#SFUI_MsgBx_NeedLiveNonGoldMsg";

	}
	else
	{
		szMsg = "#SFUI_MsgBx_NeedLiveSinglescreenMsg";

	}

	DisplayOkOnlyMsgBox( pCallerFrame, "#SFUI_XboxLive", szMsg );

	return true;
}

void CUIGameData::DisplayOkOnlyMsgBox( CBaseModFrame *pCallerFrame, const char *szTitle, const char *szMsg )
{
	// DWenger - Pulled out temporarily - GenericConfirmation* confirmation = 
		// DWenger - Pulled out temporarily - static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, pCallerFrame, false ) );
	// DWenger - Pulled out temporarily - GenericConfirmation::Data_t data;
	// DWenger - Pulled out temporarily - data.pWindowTitle = szTitle;
	// DWenger - Pulled out temporarily - data.pMessageText = szMsg;
	// DWenger - Pulled out temporarily - data.bOkButtonEnabled = true;
	// DWenger - Pulled out temporarily - confirmation->SetUsageData(data);
}

const char *CUIGameData::GetLocalPlayerName( int iController )
{
	static CGameUIConVarRef cl_names_debug( "cl_names_debug" );
	if ( cl_names_debug.GetInt() )
		return "WWWWWWWWWWWWWWW";

	IPlayer *player = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( iController );
	if ( !player )
	{
		return "";
	}

	return player->GetName();
}


//////////////////////////////////////////////////////////////////////////


//=============================================================================
void CUIGameData::SetLookSensitivity(float sensitivity)
{
	m_LookSensitivity = sensitivity;

	static CGameUIConVarRef joy_yawsensitivity("joy_yawsensitivity");
	if(joy_yawsensitivity.IsValid())
	{
		float defaultValue = std::atof(joy_yawsensitivity.GetDefault());
		joy_yawsensitivity.SetValue(defaultValue * sensitivity);
	}

	static CGameUIConVarRef joy_pitchsensitivity("joy_pitchsensitivity");
	if(joy_pitchsensitivity.IsValid())
	{
		float defaultValue = std::atof(joy_pitchsensitivity.GetDefault());
		joy_pitchsensitivity.SetValue(defaultValue * sensitivity);
	}
}

//=============================================================================
float CUIGameData::GetLookSensitivity()
{
	return m_LookSensitivity;
}

bool CUIGameData::IsXUIOpen()
{
	return m_bXUIOpen;
}

void CUIGameData::OpenWaitScreen( const char * messageText, float minDisplayTime, KeyValues *pSettings )
{
	// DWenger - Pulled out temporarily - if ( UI_IsDebug() )
	// DWenger - Pulled out temporarily - {
		// DWenger - Pulled out temporarily - Msg( "[GAMEUI] OpenWaitScreen( %s )\n", messageText );
	// DWenger - Pulled out temporarily - }

	// DWenger - Pulled out temporarily - WINDOW_TYPE wtActive = CBaseModPanel::GetSingleton().GetActiveWindowType();
	// DWenger - Pulled out temporarily - CBaseModFrame * backFrame = CBaseModPanel::GetSingleton().GetWindow( wtActive );
	// DWenger - Pulled out temporarily - if ( wtActive == WT_GENERICWAITSCREEN && backFrame )
	// DWenger - Pulled out temporarily - {
		// DWenger - Pulled out temporarily - backFrame = backFrame->GetNavBack();
		// DWenger - Pulled out temporarily - DevMsg( "CUIGameData::OpenWaitScreen - setting navback to %s instead of waitscreen\n", backFrame ? backFrame->GetName() : "NULL" );
	// DWenger - Pulled out temporarily - }
	// DWenger - Pulled out temporarily - if ( wtActive == WT_GENERICCONFIRMATION && backFrame )
	// DWenger - Pulled out temporarily - {
		// DWenger - Pulled out temporarily - DevWarning( "Cannot display waitscreen! Active window of higher priority: %s\n", backFrame->GetName() );
		// DWenger - Pulled out temporarily - return;
	// DWenger - Pulled out temporarily - }

	// DWenger - Pulled out temporarily - GenericWaitScreen* waitScreen = static_cast<GenericWaitScreen*>( 
		// DWenger - Pulled out temporarily - CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICWAITSCREEN, backFrame, false, pSettings ) );
	// DWenger - Pulled out temporarily - if( waitScreen )
	// DWenger - Pulled out temporarily - {
		// DWenger - Pulled out temporarily - waitScreen->SetNavBack( backFrame );
		// DWenger - Pulled out temporarily - waitScreen->ClearData();
		// DWenger - Pulled out temporarily - waitScreen->AddMessageText( messageText, minDisplayTime );
	// DWenger - Pulled out temporarily - }
}

void CUIGameData::UpdateWaitPanel( const char * messageText, float minDisplayTime )
{
	// DWenger - Pulled out temporarily - if ( UI_IsDebug() )
	// DWenger - Pulled out temporarily - {
		// DWenger - Pulled out temporarily - Msg( "[GAMEUI] UpdateWaitPanel( %s )\n", messageText );
	// DWenger - Pulled out temporarily - }

	// DWenger - Pulled out temporarily - GenericWaitScreen* waitScreen = static_cast<GenericWaitScreen*>( 
		// DWenger - Pulled out temporarily - CBaseModPanel::GetSingleton().GetWindow( WT_GENERICWAITSCREEN ) );
	// DWenger - Pulled out temporarily - if( waitScreen )
	// DWenger - Pulled out temporarily - {
		// DWenger - Pulled out temporarily - waitScreen->AddMessageText( messageText, minDisplayTime );
	// DWenger - Pulled out temporarily - }
}

void CUIGameData::UpdateWaitPanel( const wchar_t * messageText, float minDisplayTime )
{
	// DWenger - Pulled out temporarily - if ( UI_IsDebug() )
	// DWenger - Pulled out temporarily - {
		// DWenger - Pulled out temporarily - Msg( "[GAMEUI] UpdateWaitPanel( %S )\n", messageText );
	// DWenger - Pulled out temporarily - }

	// DWenger - Pulled out temporarily - GenericWaitScreen* waitScreen = static_cast<GenericWaitScreen*>( 
		// DWenger - Pulled out temporarily - CBaseModPanel::GetSingleton().GetWindow( WT_GENERICWAITSCREEN ) );
	// DWenger - Pulled out temporarily - if( waitScreen )
	// DWenger - Pulled out temporarily - {
		// DWenger - Pulled out temporarily - waitScreen->AddMessageText( messageText, minDisplayTime );
	// DWenger - Pulled out temporarily - }
}

void CUIGameData::NeedConnectionProblemWaitScreen ( void )
{
	m_flShowConnectionProblemTimer = 1.0f;
}

static void PasswordEntered()
{
	CUIGameData::Get()->FinishPasswordUI( true );
}

static void PasswordNotEntered()
{
	CUIGameData::Get()->FinishPasswordUI( false );
}

void CUIGameData::ShowPasswordUI( char const *pchCurrentPW )
{
	// DWenger - Pulled out temporarily - PasswordEntry *pwEntry = static_cast<PasswordEntry*>( CBaseModPanel::GetSingleton().OpenWindow( WT_PASSWORDENTRY, NULL, false ) );
	// DWenger - Pulled out temporarily - if ( pwEntry )
	// DWenger - Pulled out temporarily - {
		// DWenger - Pulled out temporarily - PasswordEntry::Data_t data;
		// DWenger - Pulled out temporarily - data.pWindowTitle = "#SFUI_PasswordEntry_Title";
		// DWenger - Pulled out temporarily - data.pMessageText = "#SFUI_PasswordEntry_Prompt";
		// DWenger - Pulled out temporarily - data.bOkButtonEnabled = true;
		// DWenger - Pulled out temporarily - data.bCancelButtonEnabled = true;
		// DWenger - Pulled out temporarily - data.m_szCurrentPW = pchCurrentPW;
		// DWenger - Pulled out temporarily - data.pfnOkCallback = PasswordEntered;
		// DWenger - Pulled out temporarily - data.pfnCancelCallback = PasswordNotEntered;
		// DWenger - Pulled out temporarily - pwEntry->SetUsageData(data);
	// DWenger - Pulled out temporarily - }
}

void CUIGameData::FinishPasswordUI( bool bOk )
{
	// DWenger - Pulled out temporarily - PasswordEntry *pwEntry = static_cast<PasswordEntry*>( CBaseModPanel::GetSingleton().GetWindow( WT_PASSWORDENTRY ) );
	// DWenger - Pulled out temporarily - if ( pwEntry )
	// DWenger - Pulled out temporarily - {
		// DWenger - Pulled out temporarily - if ( bOk )
		// DWenger - Pulled out temporarily - {
			// DWenger - Pulled out temporarily - char pw[ 256 ];
			// DWenger - Pulled out temporarily - pwEntry->GetPassword( pw, sizeof( pw ) );
			// DWenger - Pulled out temporarily - engine->SetConnectionPassword( pw );
		// DWenger - Pulled out temporarily - }
		// DWenger - Pulled out temporarily - else
		// DWenger - Pulled out temporarily - {
			// DWenger - Pulled out temporarily - engine->SetConnectionPassword( "" );
		// DWenger - Pulled out temporarily - }
	// DWenger - Pulled out temporarily - }
}

char const * CUIGameData::GetPlayerName( XUID playerID, char const *szPlayerNameSpeculative )
{
	static CGameUIConVarRef cl_names_debug( "cl_names_debug" );
	if ( cl_names_debug.GetInt() )
		return "WWWWWWWWWWWWWWW";

	return szPlayerNameSpeculative;
}

static void ui_reloadscheme( const CCommand &args );
static ConCommand ui_reloadscheme_command( "ui_reloadscheme", ui_reloadscheme, "Reloads the resource files for the active UI window", 0 );
static void ui_reloadscheme( const CCommand &args )
{
	g_pFullFileSystem->SyncDvdDevCache();
	CUIGameData::Get()->ReloadScheme();
}

void CUIGameData::ReloadScheme()
{
	// DWenger - Pulled out temporarily - CBaseModPanel::GetSingleton().ReloadScheme();
	// DWenger - Pulled out temporarily - CBaseModFrame *window = CBaseModPanel::GetSingleton().GetWindow( CBaseModPanel::GetSingleton().GetActiveWindowType() );
	// DWenger - Pulled out temporarily - if( window )
	// DWenger - Pulled out temporarily - {
		// DWenger - Pulled out temporarily - window->ReloadSettings();
	// DWenger - Pulled out temporarily - }
	// DWenger - Pulled out temporarily - CBaseModFooterPanel *footer = CBaseModPanel::GetSingleton().GetFooterPanel();
	// DWenger - Pulled out temporarily - if( footer )
	// DWenger - Pulled out temporarily - {
		// DWenger - Pulled out temporarily - footer->ReloadSettings();
	// DWenger - Pulled out temporarily - }
}

CBaseModFrame * CUIGameData::GetParentWindowForSystemMessageBox()
{
	// DWenger - Pulled out temporarily - WINDOW_TYPE wtActive = CBaseModPanel::GetSingleton().GetActiveWindowType();
	// DWenger - Pulled out temporarily - WINDOW_PRIORITY wPriority = CBaseModPanel::GetSingleton().GetActiveWindowPriority();

	// DWenger - Pulled out temporarily - CBaseModFrame *pCandidate = CBaseModPanel::GetSingleton().GetWindow( wtActive );

	// DWenger - Pulled out temporarily - if ( pCandidate )
	// DWenger - Pulled out temporarily - {
		// DWenger - Pulled out temporarily - if ( wPriority >= WPRI_WAITSCREEN && wPriority <= WPRI_MESSAGE )
		// DWenger - Pulled out temporarily - {
			// DWenger - Pulled out temporarily - if ( UI_IsDebug() )
			// DWenger - Pulled out temporarily - {
				// DWenger - Pulled out temporarily - DevMsg( "GetParentWindowForSystemMessageBox: using navback of %s\n", pCandidate->GetName() );
			// DWenger - Pulled out temporarily - }

			// Message would not be able to nav back to waitscreen or another message
			// DWenger - Pulled out temporarily - pCandidate = pCandidate->GetNavBack();
		// DWenger - Pulled out temporarily - }
		// DWenger - Pulled out temporarily - else if ( wPriority > WPRI_MESSAGE )
		// DWenger - Pulled out temporarily - {
			// DWenger - Pulled out temporarily - if ( UI_IsDebug() )
			// DWenger - Pulled out temporarily - {
				// DWenger - Pulled out temporarily - DevMsg( "GetParentWindowForSystemMessageBox: using NULL since a higher priority window is open %s\n", pCandidate->GetName() );
			// DWenger - Pulled out temporarily - }

			// Message would not be able to nav back to a higher level priority window
			// DWenger - Pulled out temporarily - pCandidate = NULL;
		// DWenger - Pulled out temporarily - }
	// DWenger - Pulled out temporarily - }

	// DWenger - Pulled out temporarily - return pCandidate;
	return nullptr;	// DWenger - temporary code
}

bool CUIGameData::IsActiveSplitScreenPlayerSpectating( void )
{
// 	int iLocalPlayerTeam;
// 	if ( GameClientExports()->GetPlayerTeamIdByUserId( -1, iLocalPlayerTeam ) )
// 	{
// 		if ( iLocalPlayerTeam != GameClientExports()->GetTeamId_Survivor() &&
// 			 iLocalPlayerTeam != GameClientExports()->GetTeamId_Infected() )
// 			return true;
// 	}

	return false;
}

struct ServerCookie_t
{
	uint64 m_uiCookie;
	double m_dblTimeCached;
};
std::map< uint64, ServerCookie_t > g_mapServerCookies;
static uint64 Helper_GetServerCookie( uint64 gsid )
{
	auto it = g_mapServerCookies.find( gsid );
	if ( it == g_mapServerCookies.end() )
		return 0;
	if ( Plat_FloatTime() - it->second.m_dblTimeCached > 60.0f )
		return 0;
	return it->second.m_uiCookie;
}

void CUIGameData::OnEvent( KeyValues *pEvent )
{
	ks::ScratchArena<> scratch;
	char const *szEvent = pEvent->GetName();

	if ( !V_stricmp( "OnSysXUIEvent", szEvent ) )
	{
		m_bXUIOpen = !V_stricmp( "opening", pEvent->GetString( "action", "" ) );
	}
	else if ( !V_stricmp( "OnProfileUnavailable", szEvent ) )
	{
		// Activate game ui to see the dialog
		// DWenger - Pulled out temporarily - if ( !CBaseModPanel::GetSingleton().IsVisible() )
		// DWenger - Pulled out temporarily - {
			// DWenger - Pulled out temporarily - engine->ExecuteClientCmd( "gameui_activate" );
		// DWenger - Pulled out temporarily - }

		// Pop a message dialog if their storage device was changed
		// DWenger - Pulled out temporarily - GenericConfirmation* confirmation = static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION,
			// DWenger - Pulled out temporarily - GetParentWindowForSystemMessageBox(), false ) );
		// DWenger - Pulled out temporarily - GenericConfirmation::Data_t data;

		// DWenger - Pulled out temporarily - data.pWindowTitle = "#SFUI_MsgBx_AchievementNotWrittenTitle";
		// DWenger - Pulled out temporarily - data.pMessageText = "#SFUI_MsgBx_AchievementNotWritten"; 
		// DWenger - Pulled out temporarily - data.bOkButtonEnabled = true;

		// DWenger - Pulled out temporarily - confirmation->SetUsageData( data );
	}
	else if ( !V_stricmp( "OnInvite", szEvent ) )
	{
		// Check if the user just accepted invite
		if ( !V_stricmp( "accepted", pEvent->GetString( "action" ) ) )
		{
			// Check if we have an outstanding session
			IMatchSession *pIMatchSession = g_pMatchFramework->GetMatchSession();
			if ( !pIMatchSession )
			{
				// DWenger - Pulled out temporarily - Invite_Connecting();
				return;
			}

			// User is accepting an invite and has an outstanding
			// session, TCR requires confirmation of destructive actions
			if ( int *pnConfirmed = ( int * ) pEvent->GetPtr( "confirmed" ) )
			{
				*pnConfirmed = 0;
			}

			// Show the prompt to confirm they wish to join a new session
			GameUI().CreateCommandMsgBoxInSlot(
				CMB_SLOT_FULL_SCREEN, 
				"#SFUI_Confirm_JoinAnotherGameTitle", 
				"#SFUI_Confirm_JoinAnotherGameText", 
				true, 
				true, 
				"confirm_join_new_session_exit_current\n", 
				nullptr, 
				nullptr, 
				nullptr );
		}
		else if ( !V_stricmp( "storage", pEvent->GetString( "action" ) ) )
		{
			// DWenger - Pulled out temporarily - if ( !Invite_IsStorageDeviceValid() )
			// DWenger - Pulled out temporarily - {
				// DWenger - Pulled out temporarily - if ( int *pnConfirmed = ( int * ) pEvent->GetPtr( "confirmed" ) )
				// DWenger - Pulled out temporarily - {
					// DWenger - Pulled out temporarily - *pnConfirmed = 0;	// make the invite accepting code wait
				// DWenger - Pulled out temporarily - }
			// DWenger - Pulled out temporarily - }
		}
		else if ( !V_stricmp( "error", pEvent->GetString( "action" ) ) )
		{
			char const *szReason = pEvent->GetString( "error", "" );

			if ( XBX_GetNumGameUsers() < 2 )
			{
				RemapText_t arrText[] = {
					{ "", "#InviteError_Unknown", RemapText_t::MATCH_FULL },
					{ "NotOnline", "#InviteError_NotOnline1", RemapText_t::MATCH_FULL },
					{ "NoMultiplayer", "#InviteError_NoMultiplayer1", RemapText_t::MATCH_FULL },
					{ "SameConsole", "#InviteError_SameConsole1", RemapText_t::MATCH_FULL },
					{ nullptr, nullptr, RemapText_t::MATCH_FULL }
				};

				szReason = RemapText_t::RemapRawText( arrText, szReason );
			}
			else
			{
				RemapText_t arrText[] = {
					{ "", "#InviteError_Unknown", RemapText_t::MATCH_FULL },
					{ "NotOnline", "#InviteError_NotOnline2", RemapText_t::MATCH_FULL },
					{ "NoMultiplayer", "#InviteError_NoMultiplayer2", RemapText_t::MATCH_FULL },
					{ "SameConsole", "#InviteError_SameConsole2", RemapText_t::MATCH_FULL },
					{ nullptr, nullptr, RemapText_t::MATCH_FULL }
				};

				szReason = RemapText_t::RemapRawText( arrText, szReason );
			}

			// Show the message box
			// DWenger - Pulled out temporarily - GenericConfirmation* confirmation = static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION,
				// DWenger - Pulled out temporarily - GetParentWindowForSystemMessageBox(), false ) );

			// DWenger - Pulled out temporarily - GenericConfirmation::Data_t data;

			// DWenger - Pulled out temporarily - data.pWindowTitle = "#SFUI_XboxLive";
			// DWenger - Pulled out temporarily - data.pMessageText = szReason;
			// DWenger - Pulled out temporarily - data.bOkButtonEnabled = true;

			// DWenger - Pulled out temporarily - confirmation->SetUsageData(data);
		}
	}
	else if ( !V_stricmp( "OnSysStorageDevicesChanged", szEvent ) )
	{

		// If a storage device change is in progress, the simply ignore
		// the notification callback, but pop the dialog
		if ( m_pSelectStorageClient )
		{
			DevWarning( "Ignored OnSysStorageDevicesChanged while the storage selection was in progress...\n" );
		}

		// Activate game ui to see the dialog
		// DWenger - Pulled out temporarily - if ( !CBaseModPanel::GetSingleton().IsVisible() )
		// DWenger - Pulled out temporarily - {
			// DWenger - Pulled out temporarily - engine->ExecuteClientCmd( "gameui_activate" );
		// DWenger - Pulled out temporarily - }

		// Pop a message dialog if their storage device was changed
		// DWenger - Pulled out temporarily - GenericConfirmation* confirmation = static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION,
			// DWenger - Pulled out temporarily - GetParentWindowForSystemMessageBox(), false ) );
		// DWenger - Pulled out temporarily - GenericConfirmation::Data_t data;

		// DWenger - Pulled out temporarily - data.pWindowTitle = "#GameUI_Console_StorageRemovedTitle";
		// DWenger - Pulled out temporarily - data.pMessageText = "#SFUI_MsgBx_StorageDeviceRemoved"; 
		// DWenger - Pulled out temporarily - data.bOkButtonEnabled = true;
		
		// DWenger - Pulled out temporarily - extern void OnStorageDevicesChangedSelectNewDevice();
		// DWenger - Pulled out temporarily - data.pfnOkCallback = m_pSelectStorageClient ? NULL : &OnStorageDevicesChangedSelectNewDevice;	// No callback if already in the middle of selecting a storage device

		// DWenger - Pulled out temporarily - confirmation->SetUsageData( data );
	}
	else if ( !V_stricmp( "OnSysInputDevicesChanged", szEvent ) )
	{
		unsigned int nInactivePlayers = 0;  // Number of users on the spectating team (ie. idle), or disconnected in this call
		int iOldSlot = engine->GetActiveSplitScreenPlayerSlot();
		int nDisconnectedDevices = pEvent->GetInt( "mask" );
		for ( unsigned int nSlot = 0; nSlot < XBX_GetNumGameUsers(); ++nSlot, nDisconnectedDevices >>= 1 )
		{
			engine->SetActiveSplitScreenPlayerSlot( nSlot );
			
			// See if this player is spectating (ie. idle)
			bool bSpectator = IsActiveSplitScreenPlayerSpectating();
			if ( bSpectator )
			{
				nInactivePlayers++;
			}
		
			if ( nDisconnectedDevices & 0x1 )
			{
				// Only count disconnections if that player wasn't idle
				if ( !bSpectator )
				{
					nInactivePlayers++;
				}

				engine->ClientCmd( "go_away_from_keyboard" );
			}
		}
		engine->SetActiveSplitScreenPlayerSlot( iOldSlot );

		// If all the spectators and all the disconnections account for all possible users, we need to pop a message
		// Also, if the GameUI is up, always show the disconnection message
		// DWenger - Pulled out temporarily - if ( CBaseModPanel::GetSingleton().IsVisible() || nInactivePlayers == XBX_GetNumGameUsers() )
		// DWenger - Pulled out temporarily - {
			// DWenger - Pulled out temporarily - if ( !CBaseModPanel::GetSingleton().IsVisible() )
			// DWenger - Pulled out temporarily - {
				// DWenger - Pulled out temporarily - engine->ExecuteClientCmd( "gameui_activate" );
			// DWenger - Pulled out temporarily - }

			// Pop a message if a valid controller was removed!
			// DWenger - Pulled out temporarily - GenericConfirmation* confirmation = static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION,
				// DWenger - Pulled out temporarily - GetParentWindowForSystemMessageBox(), false ) );
			
			// DWenger - Pulled out temporarily - GenericConfirmation::Data_t data;
			// DWenger - Pulled out temporarily - data.pWindowTitle = "#SFUI_MsgBx_ControllerUnpluggedTitle";
			// DWenger - Pulled out temporarily - data.pMessageText = "#SFUI_MsgBx_ControllerUnplugged";
			// DWenger - Pulled out temporarily - data.bOkButtonEnabled = true;

			// DWenger - Pulled out temporarily - confirmation->SetUsageData(data);
		// DWenger - Pulled out temporarily - }
	}
	else if ( !V_stricmp( "OnMatchPlayerMgrReset", szEvent ) )
	{
		char const *szReason = pEvent->GetString( "reason", "" );
		bool bShowDisconnectedMsgBox = true;
		if ( !V_stricmp( szReason, "GuestSignedIn" ) )
		{
			char const *szDestroyedSessionState = pEvent->GetString( "settings/game/state", "lobby" );
			if ( !V_stricmp( "lobby", szDestroyedSessionState ) )
				bShowDisconnectedMsgBox = false;
		}

		engine->HideLoadingPlaque(); // This may not go away unless we force it to hide

		// Go to the attract screen
		// DWenger - Pulled out temporarily - CBaseModPanel::GetSingleton().CloseAllWindows( CBaseModPanel::CLOSE_POLICY_EVEN_MSGS );

		// Show the message box
		// DWenger - Pulled out temporarily - GenericConfirmation* confirmation = bShowDisconnectedMsgBox ? static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, NULL, false ) ) : NULL;
		// DWenger - Pulled out temporarily - CAttractScreen::SetAttractMode( CAttractScreen::ATTRACT_GAMESTART );
		// DWenger - Pulled out temporarily - CBaseModPanel::GetSingleton().OpenWindow( WT_ATTRACTSCREEN, NULL );

		// DWenger - Pulled out temporarily - if ( confirmation )
		// DWenger - Pulled out temporarily - {
			// DWenger - Pulled out temporarily - GenericConfirmation::Data_t data;

			// DWenger - Pulled out temporarily - data.pWindowTitle = "#SFUI_MsgBx_SignInChangeC";
			// DWenger - Pulled out temporarily - data.pMessageText = "#SFUI_MsgBx_SignInChange";
			// DWenger - Pulled out temporarily - data.bOkButtonEnabled = true;

			// DWenger - Pulled out temporarily - if ( !V_stricmp( szReason, "GuestSignedIn" ) )
			// DWenger - Pulled out temporarily - {
				// DWenger - Pulled out temporarily - data.pWindowTitle = "#SFUI_MsgBx_DisconnectedFromSession";	// "Disconnect"
				// DWenger - Pulled out temporarily - data.pMessageText = "#SFUI_MsgBx_SignInChange";				// "Sign-in change has occured."
			// DWenger - Pulled out temporarily - }

			// DWenger - Pulled out temporarily - confirmation->SetUsageData(data);

		// DWenger - Pulled out temporarily - }
	}
	else if ( !V_stricmp( "OnEngineDisconnectReason", szEvent ) )
	{
		char const *szReason = pEvent->GetString( "reason", "" );

		if ( char const *szDisconnectHdlr = pEvent->GetString( "disconnecthdlr", nullptr ) )
		{
			// If a disconnect handler was set during the event, then we don't interfere with
			// the dialog explaining disconnection, just let the disconnect handler do everything.
			return;
		}

		RemapText_t arrText[] = {
			{ "", "#DisconnectReason_Unknown", RemapText_t::MATCH_FULL },
			{ "Lost connection to LIVE", "#DisconnectReason_LostConnectionToLIVE", RemapText_t::MATCH_FULL },
			{ "Player removed from host session", "#DisconnectReason_PlayerRemovedFromSession", RemapText_t::MATCH_SUBSTR },
			{ "Connection to server timed out", "#SFUI_MsgBx_DisconnectedFromServer", RemapText_t::MATCH_SUBSTR },
			{ "Server shutting down", "#SFUI_MsgBx_DisconnectedServerShuttingDown", RemapText_t::MATCH_SUBSTR },
			{ "Added to banned list", "#SessionError_Kicked", RemapText_t::MATCH_SUBSTR },
			{ "Kicked and banned", "#SessionError_Kicked", RemapText_t::MATCH_SUBSTR },
			{ "You have been voted off", "#SessionError_Kicked", RemapText_t::MATCH_SUBSTR },
			{ "All players idle", "#L4D_ServerShutdownIdle", RemapText_t::MATCH_SUBSTR },
			{ nullptr, nullptr, RemapText_t::MATCH_FULL }
		};

		szReason = RemapText_t::RemapRawText( arrText, szReason );

		//
		// Go back to main menu and display the disconnection reason
		//
		engine->HideLoadingPlaque(); // This may not go away unless we force it to hide

		// Go to the main menu
		// DWenger - Pulled out temporarily - CBaseModPanel::GetSingleton().CloseAllWindows( CBaseModPanel::CLOSE_POLICY_EVEN_MSGS );

		// Show the message box
		// DWenger - Pulled out temporarily - GenericConfirmation* confirmation = static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, NULL, false ) );
		// DWenger - Pulled out temporarily - CBaseModPanel::GetSingleton().OpenWindow( WT_MAINMENU, NULL );

		// DWenger - Pulled out temporarily - GenericConfirmation::Data_t data;

		// DWenger - Pulled out temporarily - data.pWindowTitle = "#SFUI_MsgBx_DisconnectedFromSession";	// "Disconnect"
		// DWenger - Pulled out temporarily - data.pMessageText = szReason;
		// DWenger - Pulled out temporarily - data.bOkButtonEnabled = true;

		// DWenger - Pulled out temporarily - confirmation->SetUsageData(data);
	}
	else if ( !V_stricmp( "OnEngineEndGame", szEvent ) )
	{
		// If we are connected and there was no session object to handle the event
		if ( !g_pMatchFramework->GetMatchSession() )
		{
			// Issue the disconnect command
			engine->ExecuteClientCmd( "disconnect" );
		}
	}
	else if ( !V_stricmp( "OnMatchSessionUpdate", szEvent ) )
	{
		if ( !V_stricmp( "error", pEvent->GetString( "state", "" ) ) )
		{
			g_pMatchFramework->CloseSession();

			char chErrorMsgBuffer[128] = {0};
			char chErrorTitleBuffer[128] = {0};
			char const *szError = pEvent->GetString( "error", "" );
			char const *szErrorTitle = "#SFUI_MsgBx_DisconnectedFromSession";

			RemapText_t arrText[] = {
				{ "", "#SessionError_Unknown", RemapText_t::MATCH_FULL },
				{ "n/a", "#SessionError_NotAvailable", RemapText_t::MATCH_FULL },
				{ "create", "#SessionError_Create", RemapText_t::MATCH_FULL },
				{ "createclient", "#SessionError_NotAvailable", RemapText_t::MATCH_FULL },
				{ "connect", "#SessionError_Connect", RemapText_t::MATCH_FULL },
				{ "full", "#SessionError_Full", RemapText_t::MATCH_FULL },
				{ "lock", "#SessionError_Lock", RemapText_t::MATCH_FULL },
				{ "kicked", "#SessionError_Kicked", RemapText_t::MATCH_FULL },
				{ "migrate", "#SessionError_Migrate", RemapText_t::MATCH_FULL },
				{ "nomap", "#SessionError_NoMap", RemapText_t::MATCH_FULL },
				{ nullptr, nullptr, RemapText_t::MATCH_FULL }
			};

			szError = RemapText_t::RemapRawText( arrText, szError );

			if ( !V_stricmp( "turequired", szError ) )
			{
				// Special case for TU required message
				// If we have a localization string for the TU message then this means that the other box
				// is running and older version of the TU
				char const *szTuRequiredCode = pEvent->GetString( "turequired" );
				CFmtStr strLocKey( "#SessionError_TU_Required_%s", szTuRequiredCode );
				if ( g_pLocalize->Find( strLocKey ) )
				{
					V_strncpy( chErrorMsgBuffer, strLocKey, sizeof( chErrorMsgBuffer ) );
					szError = chErrorMsgBuffer;
				}
				else
				{
					szError = "#SessionError_TU_RequiredMessage";
				}
				szErrorTitle = "#SessionError_TU_RequiredTitle";
			}

			// Go to the main menu
			// DWenger - Pulled out temporarily - CBaseModPanel::GetSingleton().CloseAllWindows( CBaseModPanel::CLOSE_POLICY_EVEN_MSGS );

			// Show the message box
			// DWenger - Pulled out temporarily - GenericConfirmation* confirmation = static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, NULL, false ) );
			// DWenger - Pulled out temporarily - CBaseModPanel::GetSingleton().OpenWindow( WT_MAINMENU, NULL );

			// DWenger - Pulled out temporarily - GenericConfirmation::Data_t data;

			// DWenger - Pulled out temporarily - data.pWindowTitle = szErrorTitle;
			// DWenger - Pulled out temporarily - data.pMessageText = szError;
			// DWenger - Pulled out temporarily - data.bOkButtonEnabled = true;

			if ( !V_stricmp( "dlcrequired", szError ) )
			{
				// Special case for DLC required message
				uint64 uiDlcRequiredMask = pEvent->GetUint64( "dlcrequired" );
				int iDlcRequired = 0;

				// Find the first DLC in the reported missing mask that is required
				for ( int k = 1; k < sizeof( uiDlcRequiredMask ); ++ k )
				{
					if ( uiDlcRequiredMask & ( 1ull << k ) )
					{
						iDlcRequired = k;
						break;
					}
				}

				CFmtStr strLocKey( "#SessionError_DLC_RequiredTitle_%d", iDlcRequired );
				if ( !g_pLocalize->Find( strLocKey ) )
					iDlcRequired = 0;

				// Try to figure out if this DLC is paid/free/unknown
				KeyValues *kvDlcDetails = new KeyValues( "" );
				KeyValues::AutoDelete autodelete_kvDlcDetails( kvDlcDetails );
				if ( !kvDlcDetails->LoadFromFile( g_pFullFileSystem, "resource/UI/BaseModUI/dlcdetailsinfo.res", "MOD" ) )
					kvDlcDetails = nullptr;

				// Determine the DLC offer ID
				uint64 uiDlcOfferID = 0ull;
				if ( 1 != std::sscanf( kvDlcDetails->GetString( CFmtStr( "dlc%d/offerid", iDlcRequired ) ), "0x%llx", &uiDlcOfferID ) )
					uiDlcOfferID = 0ull;
				
				// Format the strings
				bool bKicked = !V_stricmp( pEvent->GetString( "action" ), "kicked" );
				wchar_t const *wszLine1 = g_pLocalize->Find( CFmtStr( "#SessionError_DLC_Required%s_%d", bKicked ? "Kicked" : "Join", iDlcRequired ) );
				wchar_t const *wszLine2 = g_pLocalize->Find( CFmtStr( "#SessionError_DLC_Required%s_%d", uiDlcOfferID ? "Offer" : "Message", iDlcRequired ) );
				
				int numBytesTwoLines = ( V_wcslen( wszLine1 ) + V_wcslen( wszLine2 ) + 4 ) * sizeof( wchar_t );
				wchar_t *pwszTwoLines = ( wchar_t * ) scratch.AllocBytes( numBytesTwoLines );
				V_snwprintf( pwszTwoLines, numBytesTwoLines, L"%s%s", wszLine1, wszLine2 );
				// DWenger - Pulled out temporarily - data.pMessageTextW = pwszTwoLines;
				// DWenger - Pulled out temporarily - data.pMessageText = NULL;

				V_snprintf( chErrorTitleBuffer, sizeof( chErrorTitleBuffer ), "#SessionError_DLC_RequiredTitle_%d", iDlcRequired );
				// DWenger - Pulled out temporarily - data.pWindowTitle = chErrorTitleBuffer;

				if ( uiDlcOfferID )
				{
					// DWenger - Pulled out temporarily - data.bCancelButtonEnabled = true;
					// DWenger - Pulled out temporarily - data.pfnOkCallback = GoToMarketplaceForOffer;
					
					g_MarketplaceEntryPoint.uiOfferID = uiDlcOfferID;
					g_MarketplaceEntryPoint.dwEntryPoint = kvDlcDetails->GetInt( CFmtStr( "dlc%d/type", iDlcRequired ) );
				}
			}
			
			// DWenger - Pulled out temporarily - confirmation->SetUsageData(data);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
// A bunch of helper KeyValues hierarchy readers
//
//
//////////////////////////////////////////////////////////////////////////


bool GameModeHasDifficulty( char const *szGameMode )
{
	return !V_stricmp( szGameMode, "coop" ) || !V_stricmp( szGameMode, "realism" );
}

char const * GameModeGetDefaultDifficulty( char const *szGameMode )
{
	if ( !GameModeHasDifficulty( szGameMode ) )
		return "normal";

	ACTIVE_SPLITSCREEN_PLAYER_GUARD( GET_ACTIVE_SPLITSCREEN_SLOT() );
	IPlayerLocal *pProfile = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( XBX_GetActiveUserId() );
	if ( !pProfile )
		return "normal";

	UserProfileData const &upd = pProfile->GetPlayerProfileData();
	switch ( upd.difficulty )
	{
	case 1: return "easy";
	case 2: return "hard";
	default: return "normal";
	}
}

bool GameModeHasRoundLimit( char const *szGameMode )
{
	return !V_stricmp( szGameMode, "scavenge" ) || !V_stricmp( szGameMode, "teamscavenge" );
}

bool GameModeIsSingleChapter( char const *szGameMode )
{
	return !V_stricmp( szGameMode, "survival" ) || !V_stricmp( szGameMode, "scavenge" ) || !V_stricmp( szGameMode, "teamscavenge" );
}


// DWenger - Pulled out temporarily
/*
const char *COM_GetModDirectory()
{
	static char modDir[MAX_PATH];
	if ( V_strlen( modDir ) == 0 )
	{
		const char *gamedir = CommandLine()->ParmValue("-game", CommandLine()->ParmValue( "-defaultgamedir", "hl2" ) );
		V_strncpy( modDir, gamedir, sizeof(modDir) );
		if ( strchr( modDir, '/' ) || strchr( modDir, '\\' ) )
		{
			V_StripLastDir( modDir, sizeof(modDir) );
			int dirlen = V_strlen( modDir );
			V_strncpy( modDir, gamedir + dirlen, sizeof(modDir) - dirlen );
		}
	}

	return modDir;
}
*/

uint64 GetDlcInstalledMask()
{
	static ConVarRef mm_dlcs_mask_fake( "mm_dlcs_mask_fake" );
	char const *szFakeDlcsString = mm_dlcs_mask_fake.GetString();
	if ( *szFakeDlcsString )
		return std::atoi( szFakeDlcsString );

	static ConVarRef mm_dlcs_mask_extras( "mm_dlcs_mask_extras" );
	uint64 uiDLCmask = ( unsigned ) mm_dlcs_mask_extras.GetInt();

	bool bSearchPath = false;
	int numDLCs = g_pFullFileSystem->IsAnyDLCPresent( &bSearchPath );

	for ( int j = 0; j < numDLCs; ++ j )
	{
		unsigned int uiDlcHeader = 0;
		if ( !g_pFullFileSystem->GetAnyDLCInfo( j, &uiDlcHeader, nullptr, 0 ) )
			continue;

		int idDLC = DLC_LICENSE_ID( uiDlcHeader );
		if ( idDLC < 1 || idDLC >= 31 )
			continue;	// unsupported DLC id

		uiDLCmask |= ( 1ull << idDLC );
	}

	return uiDLCmask;
}
