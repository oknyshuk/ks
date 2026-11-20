// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
// Copyright (c) 2026 oknyshuk

// CTextConsoleUnix.cpp: Unix implementation of the TextConsole class.
//
//////////////////////////////////////////////////////////////////////



#include <cstdio>
import ks.core.cmdline;
import ks.core.platform;
import ks.platform;
import ks.simd;
import ks.tier1.interfaces;
import ks.tier1.keyvalues;
#include "TextConsoleUnix.h"

// un-needed include...
//#include "resource.h"
//#include "../ifilesystem.h"
#include "filesystem.h"

import std;



inline constexpr const char *CONSOLE_LOG_FILE = "console.log";

bool CTextConsoleUnix::Init( )
{
	tty = stdout;
	
	// this code is for echo-ing key presses to the connected tty
	// (which is != STDOUT) 
	if ( ks::platform::terminal_attached() )
	{
		tty = std::fopen( "/dev/tty", "w+" );	
		if ( !tty )
		{	
			std::printf("Unable to open tty(%s) for output\n", "/dev/tty" );
			tty = stdout;
		}
		else
		{
			std::setbuf( tty, nullptr ); // turn buffering off
		}
	}
	else
	{
		tty = std::fopen( "/dev/null", "w+" );
		if ( !tty )
		{
			tty = stdout;
		}
	}

	/* Disable canonical mode, set buffer size to 1 byte, disable echo */
	termStored = ks::platform::terminal_enter_raw_mode();

	m_bConDebug = CommandLine()->FindParm( "-condebug" ) != 0;
	if ( m_bConDebug && CommandLine()->FindParm( "-conclearlog" ) )
	{
		g_pFullFileSystem->RemoveFile( CONSOLE_LOG_FILE, "GAME" );
	}

	return CTextConsole::Init();
}


void CTextConsoleUnix::ShutDown( void )
{
	ks::platform::terminal_restore( termStored );
	termStored = nullptr;

	CTextConsole::ShutDown();
}


// return 0 if the kb isn't hit
int CTextConsoleUnix::kbhit( void )
{
  /* Must be in raw or cbreak mode for this to work correctly. */
  return ks::platform::terminal_input_ready();
}


char * CTextConsoleUnix::GetLine( void )
{
	if ( !kbhit() ) // early return for 99.999% case :)
		return nullptr;

	escape_sequence_t es;

	es = ESCAPE_CLEAR;
	ks::platform::terminal_block_job_signals( true );


	while ( 1 )
	{
		char	ch;
		int		nLen;

		if ( !kbhit() )
			break;
		ch = 0;
        	long numRead = ks::platform::terminal_read( &ch, 1 );
		if ( !numRead )
			break;
		
		switch (ch)
		{
		case '\n':	// Enter
			es = ESCAPE_CLEAR;

			nLen = ReceiveNewline();
			if ( nLen )
			{
				ks::platform::terminal_block_job_signals( false );
				return m_szConsoleText;	
			}
			break;

		case 127:		// Backspace
		case '\b':		// Backspace
			es = ESCAPE_CLEAR;
			ReceiveBackspace();
			break;

		case '\t':	// TAB
			es = ESCAPE_CLEAR;
			ReceiveTab();
			break;

		case 27:	// Escape character
			es = ESCAPE_RECEIVED;
			break;

		case '[':	// 2nd part of escape sequence
		case 'O':
		case 'o':
			switch( es )
			{
			case ESCAPE_CLEAR:
			case ESCAPE_BRACKET_RECEIVED:
				es = ESCAPE_CLEAR;
				ReceiveStandardChar( ch );
				break;

			case ESCAPE_RECEIVED:
				es = ESCAPE_BRACKET_RECEIVED;
				break;
			}
			break;

		case 'A':
			if ( es == ESCAPE_BRACKET_RECEIVED )
			{
				es = ESCAPE_CLEAR;
				ReceiveUpArrow();
			}
			else
			{
				es = ESCAPE_CLEAR;
				ReceiveStandardChar( ch );
			}
			break;

		case 'B':
			if ( es == ESCAPE_BRACKET_RECEIVED )
			{
				es = ESCAPE_CLEAR;
				ReceiveDownArrow();
			}
			else
			{
				es = ESCAPE_CLEAR;
				ReceiveStandardChar( ch );
			}
			break;

		case 'C':
			if ( es == ESCAPE_BRACKET_RECEIVED )
			{
				es = ESCAPE_CLEAR;
				ReceiveRightArrow();
			}
			else
			{
				es = ESCAPE_CLEAR;
				ReceiveStandardChar( ch );
			}
			break;

		case 'D':
			if ( es == ESCAPE_BRACKET_RECEIVED )
			{
				es = ESCAPE_CLEAR;
				ReceiveLeftArrow();
			}
			else
			{
				es = ESCAPE_CLEAR;
				ReceiveStandardChar( ch );
			}
			break;

		default:
			if ( es != ESCAPE_BRACKET_RECEIVED )	// Just eat this char if it's an unsupported escape
			{
				if ( ( ch >= ' ') && ( ch <= '~' ) )	// dont' accept nonprintable chars
				{
					es = ESCAPE_CLEAR;
					ReceiveStandardChar( ch );
				}
			}
			break;
        }

		std::fflush( stdout );
	}

	ks::platform::terminal_block_job_signals( false );
	return nullptr;
}


void CTextConsoleUnix::PrintRaw( char * pszMsg, int nChars )
{
	if ( m_bConDebug )
	{
		FileHandle_t fh = g_pFullFileSystem->Open( CONSOLE_LOG_FILE, "a" );
		if ( fh != FILESYSTEM_INVALID_HANDLE )
		{
			if ( nChars == 0 )
			{
				g_pFullFileSystem->Write( pszMsg, std::strlen( pszMsg ), fh );
			}
			else
			{
				g_pFullFileSystem->Write( pszMsg, nChars, fh );
			}
			g_pFullFileSystem->Close( fh );
		}
	}

	if ( nChars == 0 )
	{
		std::printf( "%s", pszMsg );
	}
	else
	{
		int nCount;

		for ( nCount = 0; nCount < nChars; nCount++ )
		{
			std::printf( "%c", pszMsg[ nCount ] );
		}
	}
}


void CTextConsoleUnix::Echo( char * pszMsg, int nChars )
{
	if ( nChars == 0 )
	{
		std::fprintf( tty, "%s", pszMsg );
	}
	else
	{
		int nCount;

		for ( nCount = 0; nCount < nChars; nCount++ )
		{
			std::fprintf( tty, "%c", pszMsg[ nCount ] );
		}
	}
}

int CTextConsoleUnix::GetWidth( void )
{
	int				nWidth;

	nWidth = ks::platform::terminal_columns();
 
 /*	if ( nWidth <= 0 )
	{
		char * ss;

		ss = getenv( "COLUMNS" );

		if ( ss )
		{
			nWidth = atoi( ss );
		}
    }
*/

	if ( nWidth <= 1 )
	{
		nWidth = 80;
	}

	return nWidth;    
} 
