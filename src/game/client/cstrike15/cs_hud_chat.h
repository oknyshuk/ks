//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#ifndef CS_HUD_CHAT_H
#define CS_HUD_CHAT_H
#ifdef _WIN32
#pragma once
#endif

#include <hud_basechat.h>

//--------------------------------------------------------------------------------------------------------------
class CHudChatLine : public CBaseHudChatLine
{
};

//-----------------------------------------------------------------------------
// Purpose: The prompt and text entry area for chat messages
//-----------------------------------------------------------------------------
class CHudChatInputLine : public CBaseHudChatInputLine
{
};

class CHudChat : public CBaseHudChat
{
	typedef CBaseHudChat BaseClass;

public:
	explicit CHudChat( const char *pElementName );

	virtual void	CreateChatInputLine( void );
	virtual void	CreateChatLines( void );

	virtual void	Init( void );
	virtual void	Reset( void );

	virtual void	ChatPrintf(int iPlayerIndex, int iFilter, PRINTF_FORMAT_STRING const char* fmt, ...) FMTFUNCTION(4, 5);

	bool			MsgFunc_SayText2( const CCSUsrMsg_SayText2 &msg );
	bool			MsgFunc_RadioText( const CCSUsrMsg_RadioText &msg );
	bool			MsgFunc_RawAudio( const CCSUsrMsg_RawAudio &msg );

	int				GetChatInputOffset( void );

	virtual Color	GetTextColorForClient( TextColor colorNum, int clientIndex );
	virtual Color	GetClientColor( int clientIndex );

	virtual int GetFilterForString( const char *pString );
};

#endif	//CS_HUD_CHAT_H
