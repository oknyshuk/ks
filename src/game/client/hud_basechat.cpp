//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "hud_basechat.h"

#include "iclientmode.h"
#include "hud_macros.h"
#include "engine/IEngineSound.h"
#include "text_message.h"
#include "localize/ilocalize.h"
#include "uicenterprint.h"
#include <keyvalues.h>
#include "c_playerresource.h"
#include "cstrike15/c_cs_playerresource.h"
#include "multiplay_gamerules.h"
#include "time.h"
#include "filesystem.h"

#ifndef NO_STEAM
#include "steam/steam_api.h"
#endif

#if defined( _X360 )
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define CHAT_WIDTH_PERCENTAGE 0.6f

ConVar hud_saytext_time( "hud_saytext_time", "12", 0 );
ConVar cl_showtextmsg( "cl_showtextmsg", "1", 0, "Enable/disable text messages printing on the screen." );
ConVar cl_chat_active( "cl_chat_active", "0" );
ConVar cl_chatfilters( "cl_chatfilters", "63", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Stores the chat filter settings " );
ConVar cl_chatfilter_version( "cl_chatfilter_version", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE | FCVAR_HIDDEN, "Stores the chat filter version" );

const int kChatFilterVersion = 1;

Color g_ColorBlue( 153, 204, 255, 255 );
Color g_ColorRed( 255, 63.75, 63.75, 255 );
Color g_ColorGreen( 153, 255, 153, 255 );
Color g_ColorDarkGreen( 64, 255, 64, 255 );
Color g_ColorYellow( 255, 178.5, 0.0, 255 );
Color g_ColorGrey( 204, 204, 204, 255 );


// removes all color markup characters, so Msg can deal with the string properly
// returns a pointer to str
char* RemoveColorMarkup( char *str )
{
	char *out = str;
	for ( char *in = str; *in != 0; ++in )
	{
		if ( *in > 0 && *in < COLOR_MAX )
		{
			continue;
		}
		*out = *in;
		++out;
	}
	*out = 0;

	return str;
}

// converts all '\r' characters to '\n', so that the engine can deal with the properly
// returns a pointer to str
char* ConvertCRtoNL( char *str )
{
	for ( char *ch = str; *ch != 0; ch++ )
		if ( *ch == '\r' )
			*ch = '\n';
	return str;
}

// converts all '\r' characters to '\n', so that the engine can deal with the properly
// returns a pointer to str
wchar_t* ConvertCRtoNL( wchar_t *str )
{
	for ( wchar_t *ch = str; *ch != 0; ch++ )
		if ( *ch == L'\r' )
			*ch = L'\n';
	return str;
}

void StripEndNewlineFromString( char *str )
{
	int s = strlen( str ) - 1;
	if ( s >= 0 )
	{
		if ( str[s] == '\n' || str[s] == '\r' )
			str[s] = 0;
	}
}

void StripEndNewlineFromString( wchar_t *str )
{
	int s = wcslen( str ) - 1;
	if ( s >= 0 )
	{
		if ( str[s] == L'\n' || str[s] == L'\r' )
			str[s] = 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Reads a string from the current message and checks if it is translatable
//-----------------------------------------------------------------------------
wchar_t* ReadLocalizedString( const char *szString, wchar_t *pOut, int outSize, bool bStripNewline, char *originalString, int originalSize )
{
	if ( originalString )
	{
		Q_strncpy( originalString, szString, originalSize );
	}

	const wchar_t *pBuf = g_pLocalize->Find( szString );
	if ( pBuf )
	{
		wcsncpy( pOut, pBuf, outSize/sizeof( wchar_t) );
		pOut[outSize/sizeof( wchar_t)-1] = 0;
	}
	else
	{
		g_pLocalize->ConvertANSIToUnicode( szString, pOut, outSize );
	}

	if ( bStripNewline )
		StripEndNewlineFromString( pOut );

	return pOut;
}

//-----------------------------------------------------------------------------
// Purpose: Reads a string from the current message, converts it to unicode, and strips out color codes
//-----------------------------------------------------------------------------
wchar_t* ReadChatTextString( const char *szString, wchar_t *pOut, int outSize, bool stripBugData )
{
	if ( outSize <= 0 )
		return pOut;

	// Allow localizing player names
	pOut[0] = 0;
	if ( const char *pszEntIndex = StringAfterPrefix( szString, "#ENTNAME[" ) )
	{
		int iEntIndex = V_atoi( pszEntIndex );
		if ( C_CS_PlayerResource *pCSPR = ( C_CS_PlayerResource* ) GameResources() )
		{
			pCSPR->GetDecoratedPlayerName( iEntIndex, pOut, outSize, ( EDecoratedPlayerNameFlag_t ) ( k_EDecoratedPlayerNameFlag_DontUseNameOfControllingPlayer | k_EDecoratedPlayerNameFlag_DontUseAssassinationTargetName ) );
		}
		if ( !pOut[0] )
		{
			if ( const char *pszCloseBracket = V_strnchr( pszEntIndex, ']', 64 ) )
				szString = pszCloseBracket + 1;
		}
	}

	if ( !pOut[0] )
	{
		g_pLocalize->ConvertANSIToUnicode( szString, pOut, outSize );
		StripEndNewlineFromString( pOut );
	}

	// converts color control characters into control characters for the normal color
	for ( wchar_t *test = pOut; test && *test; ++test )
	{
		if ( *test && ( *test < COLOR_MAX ) )
		{
			*test = COLOR_NORMAL;
		}
	}

	return pOut;
}

CBaseHudChat *g_pHudChat = NULL;

CBaseHudChat *CBaseHudChat::GetHudChat( void )
{
	Assert( g_pHudChat );
	return g_pHudChat;
}

int CBaseHudChat::m_nLineCounter = 1;
//-----------------------------------------------------------------------------
// Purpose: Text chat input/output hud element
//-----------------------------------------------------------------------------
CBaseHudChat::CBaseHudChat( const char *pElementName )
: CHudElement( pElementName )
{
	Assert( g_pHudChat == NULL );
	g_pHudChat = this;

	m_nMessageMode = MM_NONE;
	cl_chat_active.SetValue( m_nMessageMode );

	SetHiddenBits( HIDEHUD_CHAT );

	m_iFilterFlags = cl_chatfilters.GetInt();
}

CBaseHudChat::~CBaseHudChat()
{
	g_pHudChat = NULL;
}

void CBaseHudChat::CreateChatInputLine( void )
{
}

void CBaseHudChat::CreateChatLines( void )
{
}

CHudChatFilterPanel *CBaseHudChat::GetChatFilterPanel( void )
{
	return m_pFilterPanel;
}

void CBaseHudChat::Reset( void )
{
	Clear();
}

CHudChatHistory *CBaseHudChat::GetChatHistory( void )
{
	return m_pChatHistory;
}

void CBaseHudChat::Init( void )
{
	ListenForGameEvent( "hltv_chat" );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pszName - 
//			iSize - 
//			*pbuf - 
//-----------------------------------------------------------------------------
bool CBaseHudChat::MsgFunc_SayText( const CCSUsrMsg_SayText &msg )
{
	int client = msg.ent_idx();
	const char *szString =  msg.text().c_str();
	bool bWantsToChat = msg.chat() ? true : false;

	if ( bWantsToChat )
	{
		// print raw chat text
		ChatPrintf( client, CHAT_FILTER_NONE, "%s", szString );
	}
	else
	{
		// try to lookup translated string
		Printf( CHAT_FILTER_NONE, "%s", hudtextmessage->LookupString( szString ) );
	}

	CLocalPlayerFilter filter;
	C_BaseEntity::EmitSound( filter, SOUND_FROM_LOCAL_PLAYER, "HudChat.Message" );

	return true;
}

int CBaseHudChat::GetFilterForString( const char *pString )
{
	if ( !Q_stricmp( pString, "#HL_Name_Change" ) ) 
	{
		return CHAT_FILTER_NAMECHANGE;
	}

	return CHAT_FILTER_NONE;
}


//-----------------------------------------------------------------------------
// Purpose: Reads in a player's Chat text from the server
//-----------------------------------------------------------------------------
bool CBaseHudChat::MsgFunc_SayText2( const CCSUsrMsg_SayText2 &msg )
{
	// Got message during connection
	if ( !g_PR )
		return true;;

	int client = msg.ent_idx();
	bool bWantsToChat = msg.chat() ? true : false;

	wchar_t szBuf[6][256];
	char untranslated_msg_text[256];
	wchar_t *msg_text = ReadLocalizedString( msg.msg_name().c_str(), szBuf[0], sizeof( szBuf[0] ), false, untranslated_msg_text, sizeof( untranslated_msg_text ) );

	// keep reading strings and using C format strings for subsituting the strings into the localised text string
	ReadChatTextString ( msg.params(0).c_str(), szBuf[1], sizeof( szBuf[1] ) );		// player name
	ReadChatTextString ( msg.params(1).c_str(), szBuf[2], sizeof( szBuf[2] ), true );		// chat text
	ReadLocalizedString( msg.params(2).c_str(), szBuf[3], sizeof( szBuf[3] ), true );
	ReadLocalizedString( msg.params(3).c_str(), szBuf[4], sizeof( szBuf[4] ), true );

	g_pLocalize->ConstructString( szBuf[5], sizeof( szBuf[5] ), msg_text, 4, szBuf[1], szBuf[2], szBuf[3], szBuf[4] );

	char ansiString[512];
	g_pLocalize->ConvertUnicodeToANSI( ConvertCRtoNL( szBuf[5] ), ansiString, sizeof( ansiString ) );

	if ( bWantsToChat )
	{
		int iFilter = CHAT_FILTER_NONE;

		if ( client > 0 && ( g_PR->GetTeam( client ) != g_PR->GetTeam( GetLocalPlayerIndex() )) )
		{
			iFilter = CHAT_FILTER_PUBLICCHAT;
		}

		// print raw chat text
		ChatPrintf( client, iFilter, "%s", ansiString );

		CLocalPlayerFilter filter;
		C_BaseEntity::EmitSound( filter, SOUND_FROM_LOCAL_PLAYER, "HudChat.Message" );
	}
	else
	{
		// print raw chat text
		ChatPrintf( client, GetFilterForString( untranslated_msg_text), "%s", ansiString );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Message handler for text messages
// displays a string, looking them up from the titles.txt file, which can be localised
// parameters:
//   byte:   message direction  ( HUD_PRINTCONSOLE, HUD_PRINTNOTIFY, HUD_PRINTCENTER, HUD_PRINTTALK )
//   string: message
// optional parameters:
//   string: message parameter 1
//   string: message parameter 2
//   string: message parameter 3
//   string: message parameter 4
// any string that starts with the character '#' is a message name, and is used to look up the real message in titles.txt
// the next ( optional) one to four strings are parameters for that string ( which can also be message names if they begin with '#')
//-----------------------------------------------------------------------------
bool CBaseHudChat::MsgFunc_TextMsg( const CCSUsrMsg_TextMsg &msg )
{
	char szString[2048] = {};
	int msg_dest = msg.msg_dst();

	wchar_t szBuf[5][256] = {};
	wchar_t outputBuf[256] = {};

	for ( int i=0; i<5; ++i )
	{
		// Allow localizing player names
		if ( const char *pszEntIndex = StringAfterPrefix( msg.params(i).c_str(), "#ENTNAME[" ) )
		{
			int iEntIndex = V_atoi( pszEntIndex );
			wchar_t wszPlayerName[MAX_DECORATED_PLAYER_NAME_LENGTH] = {};
			if ( C_CS_PlayerResource *pCSPR = ( C_CS_PlayerResource* ) GameResources() )
			{
				pCSPR->GetDecoratedPlayerName( iEntIndex, wszPlayerName, sizeof( wszPlayerName ), ( EDecoratedPlayerNameFlag_t ) ( k_EDecoratedPlayerNameFlag_DontUseNameOfControllingPlayer | k_EDecoratedPlayerNameFlag_DontUseAssassinationTargetName ) );
			}
			if ( wszPlayerName[0] )
			{
				szString[0] = 0;
				V_wcscpy_safe( szBuf[ i ], wszPlayerName );
			}
			else if ( const char *pszEndBracket = V_strnchr( pszEntIndex, ']', 64 ) )
			{
				V_strcpy_safe( szString, pszEndBracket + 1 );
			}
			else
			{
				V_strcpy_safe( szString, msg.params(i).c_str() );
			}
		}
		else
		{
			V_strcpy_safe( szString, msg.params(i).c_str() );
		}

		if ( szString[0] )
		{
		    static char tmpStrBuf[1024];
			V_strncpy( tmpStrBuf, hudtextmessage->LookupString( szString, &msg_dest ), sizeof(tmpStrBuf) );
			bool bTranslated = false;
			if ( tmpStrBuf[ 0 ] == '#' )	// only translate parameters intended as localization tokens
			{
				const wchar_t *pBuf = g_pLocalize->Find( tmpStrBuf );
				if ( pBuf )
				{
					// Copy pBuf into szBuf[i].
					int nMaxChars = sizeof( szBuf[ i ] ) / sizeof( wchar_t );
					wcsncpy( szBuf[ i ], pBuf, nMaxChars );
					szBuf[ i ][ nMaxChars - 1 ] = 0;
					bTranslated = true;
				}
			}

			if ( !bTranslated )
			{
				if ( i )
				{
					StripEndNewlineFromString( tmpStrBuf );  // these strings are meant for substitution into the main strings, so cull the automatic end newlines
				}
				g_pLocalize->ConvertANSIToUnicode( tmpStrBuf, szBuf[ i ], sizeof( szBuf[ i ] ) );
			}
		}
	}

	if ( !cl_showtextmsg.GetInt() )
		return true;

	int len;
	switch ( msg_dest )
	{
	case HUD_PRINTCENTER:
		g_pLocalize->ConstructString( outputBuf, sizeof( outputBuf), szBuf[0], 4, szBuf[1], szBuf[2], szBuf[3], szBuf[4] );
		GetCenterPrint()->Print( ConvertCRtoNL( outputBuf ) );
		break;

	case HUD_PRINTNOTIFY:
		g_pLocalize->ConstructString( outputBuf, sizeof( outputBuf), szBuf[0], 4, szBuf[1], szBuf[2], szBuf[3], szBuf[4] );
		g_pLocalize->ConvertUnicodeToANSI( outputBuf, szString, sizeof( szString) );
		len = strlen( szString );
		if ( len && szString[len-1] != '\n' && szString[len-1] != '\r' )
		{
			Q_strncat( szString, "\n", sizeof( szString), 1 );
		}
		Msg( "%s", ConvertCRtoNL( szString ) );
		break;

	case HUD_PRINTTALK:
		g_pLocalize->ConstructString( outputBuf, sizeof( outputBuf), szBuf[0], 4, szBuf[1], szBuf[2], szBuf[3], szBuf[4] );
		g_pLocalize->ConvertUnicodeToANSI( outputBuf, szString, sizeof( szString) );
		len = strlen( szString );
		if ( len && szString[len-1] != '\n' && szString[len-1] != '\r' )
		{
			Q_strncat( szString, "\n", sizeof( szString), 1 );
		}
		Printf( CHAT_FILTER_NONE, "%s", ConvertCRtoNL( szString ) );
		break;

	case HUD_PRINTCONSOLE:
		g_pLocalize->ConstructString( outputBuf, sizeof( outputBuf), szBuf[0], 4, szBuf[1], szBuf[2], szBuf[3], szBuf[4] );
		g_pLocalize->ConvertUnicodeToANSI( outputBuf, szString, sizeof( szString) );
		len = strlen( szString );
		if ( len && szString[len-1] != '\n' && szString[len-1] != '\r' )
		{
			Q_strncat( szString, "\n", sizeof( szString), 1 );
		}
		Msg( "%s", ConvertCRtoNL( szString ) );
		break;
	}

	return true;
}

void CBaseHudChat::MsgFunc_VoiceSubtitle( bf_read &msg )
{
	// Got message during connection
	if ( !g_PR )
		return;

	if ( !cl_showtextmsg.GetInt() )
		return;

	char szString[2048];
	char szPrefix[64];	//( Voice)
	wchar_t szBuf[128];

	int client = msg.ReadByte();
	int iMenu = msg.ReadByte();
	int iItem = msg.ReadByte();

	const char *pszSubtitle = "";

	CGameRules *pGameRules = GameRules();

	CMultiplayRules *pMultiRules = dynamic_cast< CMultiplayRules * >( pGameRules );

	Assert( pMultiRules );

	if ( pMultiRules )
	{
		pszSubtitle = pMultiRules->GetVoiceCommandSubtitle( iMenu, iItem );
	}

	SetVoiceSubtitleState( true );

	const wchar_t *pBuf = g_pLocalize->Find( pszSubtitle );
	if ( pBuf )
	{
		// Copy pBuf into szBuf[i].
		int nMaxChars = sizeof( szBuf ) / sizeof( wchar_t );
		wcsncpy( szBuf, pBuf, nMaxChars );
		szBuf[nMaxChars-1] = 0;
	}
	else
	{
		g_pLocalize->ConvertANSIToUnicode( pszSubtitle, szBuf, sizeof( szBuf) );
	}

	int len;
	g_pLocalize->ConvertUnicodeToANSI( szBuf, szString, sizeof( szString) );
	len = strlen( szString );
	if ( len && szString[len-1] != '\n' && szString[len-1] != '\r' )
	{
		Q_strncat( szString, "\n", sizeof( szString), 1 );
	}

	const wchar_t *pVoicePrefix = g_pLocalize->Find( "#Voice" );
	g_pLocalize->ConvertUnicodeToANSI( pVoicePrefix, szPrefix, sizeof( szPrefix) );
	
	ChatPrintf( client, CHAT_FILTER_NONE, "%c(%s) %s%c: %s", COLOR_PLAYERNAME, szPrefix, GetDisplayedSubtitlePlayerName( client ), COLOR_NORMAL, ConvertCRtoNL( szString ) );

	SetVoiceSubtitleState( false );
}

const char *CBaseHudChat::GetDisplayedSubtitlePlayerName( int clientIndex )
{
	return g_PR->GetPlayerName( clientIndex );
}

//-----------------------------------------------------------------------------
// Purpose: Allow inheriting classes to change this spacing behavior
//-----------------------------------------------------------------------------
int CBaseHudChat::GetChatInputOffset( void )
{
	return m_iFontHeight;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *fmt - 
//			... - 
//-----------------------------------------------------------------------------
void CBaseHudChat::Printf( int iFilter, const char *fmt, ... )
{
	va_list marker;
	char msg[4096];

	va_start( marker, fmt);
	Q_vsnprintf( msg, sizeof( msg), fmt, marker);
	va_end( marker);

	ChatPrintf( 0, iFilter, "%s", msg );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseHudChat::StartMessageMode( int iMessageModeType )
{
	m_nMessageMode = iMessageModeType;
	cl_chat_active.SetValue( m_nMessageMode );

	m_flHistoryFadeTime = gpGlobals->curtime + CHAT_HISTORY_FADE_TIME;

	engine->ClientCmd_Unrestricted( "gameui_preventescapetoshow\n" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseHudChat::SetChatPrompt( int iMessageModeType )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseHudChat::StopMessageMode( bool bFade )
{
	engine->ClientCmd_Unrestricted( "gameui_allowescapetoshow\n" );

	m_nMessageMode = MM_NONE;
	cl_chat_active.SetValue( m_nMessageMode );

	if ( bFade )
	{
		m_flHistoryFadeTime = gpGlobals->curtime + CHAT_HISTORY_FADE_TIME;
	}
	else
	{
		m_flHistoryFadeTime = gpGlobals->curtime;
	}
}


void CBaseHudChat::FadeChatHistory( void )
{
}

void CBaseHudChat::SetFilterFlag( int iFilter )
{
	m_iFilterFlags = iFilter;

	cl_chatfilters.SetValue( m_iFilterFlags );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Color CBaseHudChat::GetTextColorForClient( TextColor colorNum, int clientIndex )
{
	Color c;
	switch ( colorNum )
	{
	case COLOR_PLAYERNAME:
		c = GetClientColor( clientIndex );
	break;

	case COLOR_LOCATION:
		c = g_ColorDarkGreen;
		break;

	case COLOR_ACHIEVEMENT:
		c = GetDefaultTextColor();
		break;

	default:
		c = GetDefaultTextColor();
	}

	return Color( c[0], c[1], c[2], 255 );
}

//-----------------------------------------------------------------------------
Color CBaseHudChat::GetDefaultTextColor( void )
{
	return g_ColorYellow;
}

//-----------------------------------------------------------------------------
Color CBaseHudChat::GetClientColor( int clientIndex )
{
	if ( clientIndex == 0 ) // console msg
	{
		return g_ColorGreen;
	}
	else if( g_PR )
	{
		return g_ColorGrey;
	}

	return g_ColorYellow;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : CBaseHudChatLine
//-----------------------------------------------------------------------------
CBaseHudChatLine *CBaseHudChat::FindUnusedChatLine( void )
{
	return m_ChatLine;
}

void CBaseHudChat::Send( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseHudChat::Clear( void )
{
	// Kill input prompt
	StopMessageMode();

	m_flHistoryFadeTime = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *newmap - 
//-----------------------------------------------------------------------------
void CBaseHudChat::LevelInit( const char *newmap )
{
	Clear();

	// [pfreese] initialize new chat filters to defaults. We do this because
	// unused filter bits are zero, and we might want them on for new filters that
	// are added.
	//
	// Also, we have to do this here instead of somewhere more sensible like the 
	// c'tor or Init() method, because cvars are currently loaded twice: once
	// during initialization from the local file, and later ( after HUD elements
	// have been construction and initialized) from Steam Cloud remote storage.

	switch ( cl_chatfilter_version.GetInt() )
	{
	case 0:
		m_iFilterFlags |= CHAT_FILTER_ACHIEVEMENT;
		// fall through
	case kChatFilterVersion:
		break;
	}

	if ( cl_chatfilter_version.GetInt() != kChatFilterVersion )
	{
		cl_chatfilters.SetValue( m_iFilterFlags );
		cl_chatfilter_version.SetValue( kChatFilterVersion );
	}
}

void CBaseHudChat::LevelShutdown( void )
{
	Clear();
}

void	CBaseHudChat::ChatPrintfW( int iPlayerIndex, int iFilter, const wchar_t *wszNotice )
{
#if defined( _PS3 ) && !defined( NO_STEAM )
	if ( !steamapicontext->SteamFriends() || steamapicontext->SteamFriends()->GetUserRestrictions() )
		return; // user not eligible to chat
#endif

	if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
	{
		if ( pParameters->m_bAnonymousPlayerIdentity )
			return; // cannot print potentially personal details
	}

	char ansi[4096];
	g_pLocalize->ConvertUnicodeToANSI( wszNotice, ansi, sizeof( ansi ) );
	ChatPrintf( iPlayerIndex, iFilter, "%s", ansi );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *fmt - 
//			... - 
//-----------------------------------------------------------------------------
void CBaseHudChat::ChatPrintf( int iPlayerIndex, int iFilter, const char *fmt, ... )
{
#if defined( _PS3 ) && !defined( NO_STEAM )
	if ( !steamapicontext->SteamFriends() || steamapicontext->SteamFriends()->GetUserRestrictions() )
		return; // user not eligible to chat
#endif

	if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
	{
		if ( pParameters->m_bAnonymousPlayerIdentity )
			return; // cannot print potentially personal details
	}

	va_list marker;
	char msg[4096];

	va_start( marker, fmt);
	Q_vsnprintf( msg, sizeof( msg), fmt, marker);
	va_end( marker);

	// Strip any trailing '\n'
	if ( strlen( msg ) > 0 && msg[ strlen( msg )-1 ] == '\n' )
	{
		msg[ strlen( msg ) - 1 ] = 0;
	}

	// Strip leading \n characters ( or notify/color signifiers ) for empty string check
	char *pmsg = msg;
	while ( *pmsg && ( *pmsg == '\n' || ( *pmsg > 0 && *pmsg < COLOR_MAX ) ) )
	{
		pmsg++;
	}

	if ( !*pmsg )
		return;

	// Now strip just newlines, since we want the color info for printing
	pmsg = msg;
	while ( *pmsg && ( *pmsg == '\n' ) )
	{
		pmsg++;
	}

	if ( !*pmsg )
		return;

	if ( iFilter != CHAT_FILTER_NONE )
	{
#ifdef PORTAL2
		if ( iFilter & ( CHAT_FILTER_JOINLEAVE | CHAT_FILTER_TEAMCHANGE ) )
			return;
#endif
		if ( !( iFilter & GetFilterFlags() ) )
			return;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseHudChat::FireGameEvent( IGameEvent *event )
{
	const char *eventname = event->GetName();

	if ( Q_strcmp( "hltv_chat", eventname ) == 0 )
	{
		C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();

		if ( !player )
			return;
		
		ChatPrintf( player->entindex(), CHAT_FILTER_NONE, "(GOTV) %s", event->GetString( "text" ) );
	}
}
