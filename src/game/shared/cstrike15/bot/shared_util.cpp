// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: dll-agnostic routines (no dll dependencies here)

// Author: Matthew D. Campbell (matt@turtlerockstudios.com), 2003

import std;
import ks.tier1;

#include "shared_util.h"

static char s_shared_token[ 1500 ];
static char s_shared_quote = '\"';

//--------------------------------------------------------------------------------------------------------------
char * SharedVarArgs(const char *format, ...)
{
	std::va_list argptr;
	const int BufLen = 1024;
	const int NumBuffers = 4;
	static char string[NumBuffers][BufLen];
	static int curstring = 0;
	
	curstring = ( curstring + 1 ) % NumBuffers;

	__builtin_va_start (argptr, format);
	std::vsnprintf( string[curstring], BufLen, format, argptr );
	__builtin_va_end (argptr);

	return string[curstring];  
}

//--------------------------------------------------------------------------------------------------------------
char * BufPrintf(char *buf, int& len, const char *fmt, ...)
{
	if (len <= 0)
		return nullptr;

	std::va_list argptr;

	__builtin_va_start(argptr, fmt);
	std::vsnprintf(buf, len, fmt, argptr);
	__builtin_va_end(argptr);
	// Make sure the buffer is null-terminated.
	buf[ len - 1 ] = 0;

	len -= std::strlen(buf);
	return buf + std::strlen(buf);
}


//--------------------------------------------------------------------------------------------------------------
// dgoodenough - PS3 needs this guy as well.
// PS3_BUILDFIX
//--------------------------------------------------------------------------------------------------------------
const char * NumAsString( int val )
{
	const int BufLen = 16;
	static char buf[BufLen];
	int len = BufLen;
	BufPrintf( buf, len, "%d", val );
	return buf;
}

//--------------------------------------------------------------------------------------------------------
/**
 * Returns the token parsed by SharedParse()
 */
char *SharedGetToken( void )
{
	return s_shared_token;
}

//--------------------------------------------------------------------------------------------------------
/**
 * Returns the token parsed by SharedParse()
 */
void SharedSetQuoteChar( char c )
{
	s_shared_quote = c;
}

//--------------------------------------------------------------------------------------------------------
/**
 * Parse a token out of a string
 */
const char *SharedParse( const char *data )
{
	int             c;
	int             len;
	
	len = 0;
	s_shared_token[0] = 0;
	
	if (!data)
		return nullptr;
		
// skip whitespace
skipwhite:
	while ( (c = *data) <= ' ')
	{
		if (c == 0)
			return nullptr;                    // end of file;
		data++;
	}
	
// skip // comments
	if (c=='/' && data[1] == '/')
	{
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}
	

// handle quoted strings specially
	if (c == s_shared_quote)
	{
		data++;
		while (1)
		{
			c = *data++;
			if (c==s_shared_quote || !c)
			{
				s_shared_token[len] = 0;
				return data;
			}
			s_shared_token[len] = c;
			len++;
		}
	}

// parse single characters
	if (c=='{' || c=='}'|| c==')'|| c=='(' || c=='\'' || c == ',' )
	{
		s_shared_token[len] = c;
		len++;
		s_shared_token[len] = 0;
		return data+1;
	}

// parse a regular word
	do
	{
		s_shared_token[len] = c;
		data++;
		len++;
		c = *data;
	if (c=='{' || c=='}'|| c==')'|| c=='(' || c=='\'' || c == ',' )
			break;
	} while (c>32);
	
	s_shared_token[len] = 0;
	return data;
}

//--------------------------------------------------------------------------------------------------------
/**
 * Returns true if additional data is waiting to be processed on this line
 */
bool SharedTokenWaiting( const char *buffer )
{
	const char *p;

	p = buffer;
	while ( *p && *p!='\n')
	{
		if ( !V_isspace( *p ) || V_isalnum( *p ) )
			return true;

		p++;
	}

	return false;
}
