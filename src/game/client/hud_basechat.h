//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef HUD_BASECHAT_H
#define HUD_BASECHAT_H
#ifdef _WIN32
#pragma once
#endif

#include "hudelement.h"
#include <color.h>


#define CHATLINE_NUM_FLASHES 8.0f
#define CHATLINE_FLASH_TIME 5.0f
#define CHATLINE_FADE_TIME 1.0f

#define CHAT_HISTORY_ONE_OVER_FADE_TIME 4.0f;
#define CHAT_HISTORY_FADE_TIME 0.25f
#define CHAT_HISTORY_IDLE_TIME 15.0f
#define CHAT_HISTORY_IDLE_FADE_TIME 2.5f
#define CHAT_HISTORY_ALPHA 127

extern Color g_ColorBlue;
extern Color g_ColorRed;
extern Color g_ColorGreen;
extern Color g_ColorDarkGreen;
extern Color g_ColorYellow;
extern Color g_ColorGrey;

extern ConVar cl_showtextmsg;

enum ChatFilters
{
	CHAT_FILTER_NONE		= 0,
	CHAT_FILTER_JOINLEAVE	= 0x000001,
	CHAT_FILTER_NAMECHANGE	= 0x000002,
	CHAT_FILTER_PUBLICCHAT	= 0x000004,
	CHAT_FILTER_SERVERMSG	= 0x000008,
	CHAT_FILTER_TEAMCHANGE	= 0x000010,
	CHAT_FILTER_ACHIEVEMENT	= 0x000020,
};


//-----------------------------------------------------------------------------
enum TextColor
{
	COLOR_NORMAL =			1,
	COLOR_USEOLDCOLORS =	2,
	COLOR_PLAYERNAME =		3,
	COLOR_LOCATION =		4,
	COLOR_ACHIEVEMENT =		5,
	COLOR_AWARD =			6,
	COLOR_PENALTY =			7,
	COLOR_SILVER =			8,
	COLOR_GOLD =			9,

	COLOR_RARITY_FIRST =	10,
	COLOR_COMMON =			COLOR_RARITY_FIRST,
	COLOR_UNCOMMON =		11,
	COLOR_RARE =			12,
	COLOR_MYTHICAL =		13,
	COLOR_LEGENDARY =		14,
	COLOR_ANCIENT =			15,
	COLOR_IMMORTAL =		16,

	COLOR_RARITY_LAST =		COLOR_IMMORTAL,

	COLOR_MAX
};

//--------------------------------------------------------------------------------------------------------------
struct TextRange
{
	int start;
	int end;
	Color color;
};

void StripEndNewlineFromString( char *str );
void StripEndNewlineFromString( wchar_t *str );

char* ConvertCRtoNL( char *str );
wchar_t* ConvertCRtoNL( wchar_t *str );
wchar_t* ReadLocalizedString( const char *szString, OUT_Z_BYTECAP(outSizeInBytes) wchar_t *pOut, int outSizeInBytes, bool bStripNewline, char *originalString = NULL, int originalSize = 0 );
wchar_t* ReadChatTextString( const char *szString, OUT_Z_BYTECAP(outSizeInBytes) wchar_t *pOut, int outSizeInBytes, bool stripBugData = false );
char* RemoveColorMarkup( char *str );

//--------------------------------------------------------------------------------------------------------
/**
 * Simple utility function to allocate memory and duplicate a wide string
 */
inline wchar_t *CloneWString( const wchar_t *str )
{
	wchar_t *cloneStr = new wchar_t [ wcslen(str)+1 ];
	wcscpy( cloneStr, str );
	return cloneStr;
}

class CBaseHudChatLine
{
};

class CHudChatHistory
{
};

class CBaseHudChatEntry
{
};

class CBaseHudChatInputLine
{
};

class CHudChatFilterButton
{
};

class CHudChatFilterPanel
{
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CBaseHudChat : public CHudElement
{
	typedef CHudElement BaseClass;
public:
	DECLARE_MULTIPLY_INHERITED();

	enum
	{
		CHAT_INTERFACE_LINES = 6,
		MAX_CHARS_PER_LINE = 128
	};

	explicit CBaseHudChat( const char *pElementName );
	~CBaseHudChat();

	static CBaseHudChat *GetHudChat( void );

	virtual void	CreateChatInputLine( void );
	virtual void	CreateChatLines( void );
	
	virtual void	Init( void );

	void			LevelInit( const char *newmap );
	void			LevelShutdown( void );

	void			MsgFunc_TextMsg(const char *pszName, int iSize, void *pbuf);
	
	virtual void	Printf( int iFilter, PRINTF_FORMAT_STRING const char *fmt, ... );
	virtual void	ChatPrintf( int iPlayerIndex, int iFilter, PRINTF_FORMAT_STRING const char *fmt, ... ) FMTFUNCTION( 4, 5 );
	virtual void	ChatPrintfW( int iPlayerIndex, int iFilter, const wchar_t *wszNotice );
	
	virtual void	StartMessageMode( int iMessageModeType );
	virtual void	StopMessageMode( bool bFade = true );
	void			Send( void );

	virtual void	Reset();

	static int		m_nLineCounter;

	virtual int		GetChatInputOffset( void );

	// IGameEventListener interface:
	virtual void FireGameEvent( IGameEvent *event);

	CHudChatHistory			*GetChatHistory();

	void					FadeChatHistory();
	float					m_flHistoryFadeTime;
	float					m_flHistoryIdleTime;

	virtual bool			MsgFunc_SayText( const CCSUsrMsg_SayText &msg );
	virtual bool			MsgFunc_SayText2( const CCSUsrMsg_SayText2 &msg );
	virtual bool			MsgFunc_TextMsg( const CCSUsrMsg_TextMsg &msg );
	virtual void			MsgFunc_VoiceSubtitle( bf_read &msg );

	
	CBaseHudChatInputLine	*GetChatInput( void ) { return m_pChatInput; }
	CHudChatFilterPanel		*GetChatFilterPanel( void );

	virtual int				GetFilterFlags( void ) { return m_iFilterFlags; }
	void					SetFilterFlag( int iFilter );

	virtual void		SetChatPrompt( int iMessageModeType );

	//-----------------------------------------------------------------------------
	virtual Color	GetDefaultTextColor( void );
	virtual Color	GetTextColorForClient( TextColor colorNum, int clientIndex );
	virtual Color	GetClientColor( int clientIndex );

	virtual int		GetFilterForString( const char *pString );

	virtual const char *GetDisplayedSubtitlePlayerName( int clientIndex );

	bool			IsVoiceSubtitle( void ) { return m_bEnteringVoice; }
	void			SetVoiceSubtitleState( bool bState ) { m_bEnteringVoice = bState; }

protected:
	CBaseHudChatLine		*FindUnusedChatLine( void );

	CBaseHudChatInputLine	*m_pChatInput;
	CBaseHudChatLine		*m_ChatLine;
	int						m_iFontHeight;

	CHudChatHistory			*m_pChatHistory;

	CHudChatFilterButton	*m_pFiltersButton;
	CHudChatFilterPanel		*m_pFilterPanel;

private:	
	void			Clear( void );

	int				m_nMessageMode;

	int				m_iFilterFlags;
	bool			m_bEnteringVoice;
};

#endif // HUD_BASECHAT_H
