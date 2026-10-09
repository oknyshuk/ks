#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
// Copyright (c) 2026 oknyshuk

// TextConsoleUnix.h: Unix interface for the TextConsole class.
//
//////////////////////////////////////////////////////////////////////

#if !defined TEXTCONSOLE_UNIX_H
#define TEXTCONSOLE_UNIX_H

import ks.platform;
import std;
#include "textconsole.h"

typedef enum
{
    ESCAPE_CLEAR = 0,
    ESCAPE_RECEIVED,
    ESCAPE_BRACKET_RECEIVED
} escape_sequence_t;


class CTextConsoleUnix : public CTextConsole
{
public:
	virtual ~CTextConsoleUnix()
	{
	};

	bool		Init();
	void		ShutDown( void );
	void		PrintRaw( char * pszMsg, int nChars = 0 );
	void		Echo( char * pszMsg, int nChars = 0 );
	char *		GetLine( void );
	int			GetWidth( void );

private:
	int kbhit( void );

	bool m_bConDebug;

	ks::platform::terminal_state *termStored = nullptr;
	std::FILE *tty;
};




#endif // !defined
