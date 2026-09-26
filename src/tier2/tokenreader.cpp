//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "tokenreader.h"
#include "tier0/platform.h"
#include "tier1/strtools.h"
#include "filesystem.h"
#include "tier0/dbg.h"


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
TokenReader::TokenReader(void)
{
	m_szFilename[0] = '\0';
	m_nPos = 0;
	m_bOpen = false;
	m_nLine = 1;
	m_nErrorCount = 0;
	m_bStuffed = false;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pszFilename - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool TokenReader::Open(const char *pszFilename)
{
	auto data = g_pFullFileSystem->ReadFile( pszFilename, nullptr );
	m_bOpen = data.has_value();
	m_file = data ? std::move( *data ) : std::vector< std::byte >();
	m_nPos = 0;
	Q_strncpy(m_szFilename, pszFilename, sizeof( m_szFilename ) );
	m_nLine = 1;
	m_nErrorCount = 0;
	m_bStuffed = false;
	return m_bOpen;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void TokenReader::Close()
{
	m_bOpen = false;
	m_file.clear();
	m_file.shrink_to_fit();
	m_nPos = 0;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *error - 
// Output : const char
//-----------------------------------------------------------------------------
const char *TokenReader::Error(char *error, ...)
{
	static char szErrorBuf[256];
	Q_snprintf(szErrorBuf, sizeof( szErrorBuf ), "File %s, line %d: ", m_szFilename, m_nLine);
	Q_strncat(szErrorBuf, error, sizeof( szErrorBuf ), COPY_ALL_CHARACTERS );
	m_nErrorCount++;
	return(szErrorBuf);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pszStore - 
//			nSize - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
trtoken_t TokenReader::GetString(char *pszStore, int nSize)
{
	if (nSize <= 0)
	{
		return TOKENERROR;
	}

	char szBuf[1024];

	//
	// Until we reach the end of this string or run out of room in
	// the destination buffer...
	//
	while (true)
	{
		//
		// Fetch the next batch of text from the file.
		//
		int writePos = 0;
		
		while ( !AtEnd() )
		{
			if ( PeekChar() == '\"' )
				break;
			if ( writePos < (sizeof(szBuf)-1) ) 
			{
				szBuf[writePos++] = GetChar();
			}
		}

		szBuf[writePos] = 0;
		if ( AtEnd() )
			return TOKENEOF;


		//
		// Transfer the text to the destination buffer.
		//
		char *pszSrc = szBuf;
		while ((*pszSrc != '\0') && (nSize > 1))
		{
			if (*pszSrc == '\n')
			{
				//
				// Newline encountered before closing quote -- unterminated string.
				//
				*pszStore = '\0';
				return TOKENSTRINGTOOLONG;
			}
			else if (*pszSrc != '\\')
			{
				*pszStore = *pszSrc;
				pszSrc++;
			}
			else
			{
				//
				// Backslash sequence - replace with the appropriate character.
				//
				pszSrc++;

				if (*pszSrc == 'n')
				{
					*pszStore = '\n';
				}

				pszSrc++;
			}

			pszStore++;
			nSize--;
		}

		if (*pszSrc != '\0')
		{
			//
			// Ran out of room in the destination buffer. 
			// terminate the string, and exit.
			//
			*pszStore = '\0';
			return TOKENSTRINGTOOLONG; 
		}

		//
		// Check for closing quote.
		//

		if (PeekChar() == '\"')
		{
			//
			// Eat the close quote and any whitespace.
			//
			GetChar();
			bool bCombineStrings = SkipWhiteSpace();

			//
			// Combine consecutive quoted strings if the combine strings character was
			// encountered between the two strings.
			//
			if (bCombineStrings && !AtEnd() && (PeekChar() == '\"'))
			{
				//
				// Eat the open quote and keep parsing this string.
				//
				GetChar();
			}
			else
			{
				//
				// Done with this string, terminate the string and exit.
				//
				*pszStore = '\0';
				return STRING;
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Returns the next token, allocating enough memory to store the token
//			plus a terminating NULL.
// Input  : pszStore - Pointer to a string that will be allocated.
// Output : Returns the type of token that was read, or TOKENERROR.
//-----------------------------------------------------------------------------
trtoken_t TokenReader::NextTokenDynamic(char **ppszStore)
{
	char szTempBuffer[8192];
	trtoken_t eType = NextToken(szTempBuffer, sizeof(szTempBuffer));

	int len = Q_strlen(szTempBuffer) + 1;
	*ppszStore = new char [len];
	Assert( *ppszStore );
	Q_strncpy(*ppszStore, szTempBuffer, len );

	return(eType);
}


//-----------------------------------------------------------------------------
// Purpose: Returns the next token.
// Input  : pszStore - Pointer to a string that will receive the token.
// Output : Returns the type of token that was read, or TOKENERROR.
//-----------------------------------------------------------------------------
trtoken_t TokenReader::NextToken(char *pszStore, int nSize)
{
	char *pStart = pszStore;

	//
	// If they stuffed a token, return that token.
	//
	if (m_bStuffed)
	{
		m_bStuffed = false;
		Q_strncpy( pszStore, m_szStuffed, nSize );
		return m_eStuffed;
	}
	
	SkipWhiteSpace();

	if ( AtEnd() )
		return TOKENEOF;

	char ch = PeekChar();

	//
	// Look for all the valid operators.
	//
	switch (ch)
	{
		case '@':
		case ',':
		case '!':
		case '+':
		case '&':
		case '*':
		case '$':
		case '.':
		case '=':
		case ':':
		case '[':
		case ']':
		case '(':
		case ')':
		case '{':
		case '}':
		case '\\':
		{
			pszStore[0] = ch;
			pszStore[1] = 0;
			GetChar();	// used char
			return OPERATOR;
		}
	}

	//
	// Look for the start of a quoted string.
	//
	if (ch == '\"')
	{
		GetChar();	// used char
		return GetString(pszStore, nSize);
	}

	//
	// Integers consist of numbers with an optional leading minus sign.
	//
	if (V_isdigit(ch) || (ch == '-'))
	{
		do
		{
			if ( (pszStore - pStart + 1) < nSize )
			{
				GetChar();	// used char
				*pszStore = ch;
				pszStore++;
			}

			ch = PeekChar();
			if (ch == '-')
			{
				GetChar();	// used char
				return TOKENERROR;
			}
		} while (V_isdigit(ch));
		
		//
		// No identifier characters are allowed contiguous with numbers.
		//
		if (V_isalpha(ch) || (ch == '_'))
		{
			GetChar();	// used char
			return TOKENERROR;
		}

		*pszStore = '\0';
		return INTEGER;
	}
 
	//
	// Identifiers consist of a consecutive string of alphanumeric
	// characters and underscores.
	//
	while ( V_isalpha(ch) || V_isdigit(ch) || (ch == '_') )
	{
		if ( (pszStore - pStart + 1) < nSize )
		{
			GetChar();	// used char
			*pszStore = ch;
			pszStore++;
		}

		ch = PeekChar();
	}

	*pszStore = '\0';
	return IDENT;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : ttype - 
//			*pszToken - 
//-----------------------------------------------------------------------------
void TokenReader::IgnoreTill(trtoken_t ttype, const char *pszToken)
{
	trtoken_t _ttype;
	char szBuf[1024];

	while(1)
	{
		_ttype = NextToken(szBuf, sizeof(szBuf));
		if(_ttype == TOKENEOF)
			return;
		if(_ttype == ttype)
		{
			if(IsToken(pszToken, szBuf))
			{
				Stuff(ttype, pszToken);
				return;
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : ttype - 
//			pszToken - 
//-----------------------------------------------------------------------------
void TokenReader::Stuff(trtoken_t eType, const char *pszToken)
{
	m_eStuffed = eType;
	Q_strncpy(m_szStuffed, pszToken, sizeof( m_szStuffed ) );
	m_bStuffed = true;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : ttype - 
//			pszToken - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool TokenReader::Expecting(trtoken_t ttype, const char *pszToken)
{
	char szBuf[1024];
	if (NextToken(szBuf, sizeof(szBuf)) != ttype || !IsToken(pszToken, szBuf))
	{
		return false;
	}
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pszStore - 
// Output : 
//-----------------------------------------------------------------------------
trtoken_t TokenReader::PeekTokenType(char *pszStore, int maxlen )
{
	if (!m_bStuffed)
	{
		m_eStuffed = NextToken(m_szStuffed, sizeof(m_szStuffed));
		m_bStuffed = true;
	}
	
	if (pszStore)
	{
		Q_strncpy(pszStore, m_szStuffed, maxlen );
	}

	return(m_eStuffed);
}


//-----------------------------------------------------------------------------
// Purpose: Gets the next non-whitespace character from the file.
// Input  : ch - Receives the character.
// Output : Returns true if the whitespace contained the combine strings
//			character '+', which is used to merge consecutive quoted strings.
//-----------------------------------------------------------------------------
bool TokenReader::SkipWhiteSpace(void)
{
	bool bCombineStrings = false;

	while ( !AtEnd() )
	{
		char ch = PeekChar();
		// whitespace, skip
		if ((ch == ' ') || (ch == '\t') || (ch == '\r') || (ch == 0))
		{
			GetChar();
			continue;
		}

		// string continuation
		if (ch == '+')
		{
			GetChar();
			bCombineStrings = true;
			continue;
		}
		// newline, count and advance
		if (ch == '\n')
		{
			GetChar();
			m_nLine++;
			continue;
		}

		// Deal with c++ style comments
		if ( m_nPos + 2 <= m_file.size() && PeekChar() == '/' && PeekChar( 1 ) == '/' )
		{
			// read complete line
			while ( !AtEnd() )
			{
				if ( GetChar() == '\n' )
					break;
			}
			m_nLine++;
			continue;
		}
		break;
	}
	return bCombineStrings;
}

