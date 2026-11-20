// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation



// First include standard libraries
import std;


import ks.core.cmdline;
import ks.core.dbg;
import ks.core.platform;
import ks.core.types;
import ks.core.vprof;
import ks.platform;
import ks.simd;
import ks.tier1.strtools;


static const int MAX_PARAMETER_LEN = 128;

//-----------------------------------------------------------------------------
// Instance singleton and expose interface to rest of code
//-----------------------------------------------------------------------------
ICommandLine *CommandLine()
{
	static ICommandLine s_CmdLine;
	return &s_CmdLine;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
ICommandLine::ICommandLine( void )
{
	m_pszCmdLine = nullptr;
	m_nParmCount = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
ICommandLine::~ICommandLine( void )
{
	CleanUpParms();
	delete[] m_pszCmdLine;
}


//-----------------------------------------------------------------------------
// Read commandline from file instead...
//-----------------------------------------------------------------------------
void ICommandLine::LoadParametersFromFile( const char *&pSrc, char *&pDst, intp maxDestLen, bool bInQuotes )
{
	// Suck out the file name
	char szFileName[ MAX_PATH ];
	char *pOut;
	char *pDestStart = pDst;

	if ( maxDestLen < 3 )
		return;

	// Skip the @ sign
	pSrc++;

	pOut = szFileName;

	char terminatingChar = ' ';
	if ( bInQuotes )
		terminatingChar = '\"';

	while ( *pSrc && *pSrc != terminatingChar )
	{
		*pOut++ = *pSrc++;
		if ( (pOut - szFileName) >= (MAX_PATH-1) )
			break;
	}

	*pOut = '\0';

	// Skip the space after the file name
	if ( *pSrc )
		pSrc++;

	// Now read in parameters from file
	std::FILE *fp = std::fopen( szFileName, "r" );
	if ( fp )
	{
		char c;
		c = (char)std::fgetc( fp );
		while ( c != std::char_traits<char>::eof() )
		{
			// Turn return characters into spaces
			if ( c == '\n' )
				c = ' ';

			*pDst++ = c;
			
			// Don't go past the end, and allow for our terminating space character AND a terminating null character.
			if ( (pDst - pDestStart) >= (maxDestLen-2) )
				break;

			// Get the next character, if there are more
			c = (char)std::fgetc( fp );
		}
	
		// Add a terminating space character
		*pDst++ = ' ';

		std::fclose( fp );
	}
	else
	{
		std::printf( "Parameter file '%s' not found, skipping...", szFileName );
	}
}


//-----------------------------------------------------------------------------
// Creates a command line from the arguments passed in
//-----------------------------------------------------------------------------
void ICommandLine::CreateCmdLine( int argc, char **argv )
{
	char cmdline[2048];
	cmdline[0] = 0;
	const int MAX_CHARS = sizeof(cmdline) - 1;
	cmdline[MAX_CHARS] = 0;
	for ( int i = 0; i < argc; ++i )
	{
		std::strncat( cmdline, "\"", MAX_CHARS );
		std::strncat( cmdline, argv[i], MAX_CHARS );
		std::strncat( cmdline, "\"", MAX_CHARS );
		std::strncat( cmdline, " ", MAX_CHARS );
	}

	CreateCmdLine( cmdline );
}


//-----------------------------------------------------------------------------
// Purpose: Create a command line from the passed in string
//  Note that if you pass in a @filename, then the routine will read settings
//  from a file instead of the command line
//-----------------------------------------------------------------------------
void ICommandLine::CreateCmdLine( const char *commandline )
{
	const bool bNoAutoArgs = (Plat_GetEnv("autoargs")) == nullptr;
	if ( m_pszCmdLine )
	{
		delete[] m_pszCmdLine;
	}

	char szFull[ 4096 ];

	char *pDst = szFull;
	const char *pSrc = commandline;

	bool bInQuotes = false;
	const char *pInQuotesStart = 0;
	while ( *pSrc )
	{
		// Is this an unslashed quote?
		if ( *pSrc == '"' )
		{
			if ( pSrc == commandline || ( pSrc[-1] != '/' && pSrc[-1] != '\\' ) )
			{
				bInQuotes = !bInQuotes;
				pInQuotesStart = pSrc + 1;
			}
		}

		if ( !bNoAutoArgs && *pSrc == '@' )
		{
			if ( pSrc == commandline || (!bInQuotes && V_isspace( pSrc[-1] )) || (bInQuotes && pSrc == pInQuotesStart) )
			{
				LoadParametersFromFile( pSrc, pDst, sizeof( szFull ) - (pDst - szFull), bInQuotes );
				continue;
			}
		}	
		
		// Don't go past the end.
		if ( (pDst - szFull) >= (sizeof( szFull ) - 1) )
			break;

		*pDst++ = *pSrc++;
	}

	*pDst = '\0';

	std::size_t len = std::strlen( szFull ) + 1;
	m_pszCmdLine = new char[len];
	std::memcpy( m_pszCmdLine, szFull, len );

	Plat_SetCommandLine( m_pszCmdLine );

	ParseCommandLine();
}


//-----------------------------------------------------------------------------
// Finds a string in another string with a case insensitive test
//-----------------------------------------------------------------------------
static char * _stristr( char * pStr, const char * pSearch )
{
	AssertValidStringPtr(pStr);
	AssertValidStringPtr(pSearch);

	if (!pStr || !pSearch) 
		return 0;

	char* pLetter = pStr;

	// Check the entire string
	while (*pLetter != 0)
	{
		// Skip over non-matches
		if (std::tolower((unsigned char)*pLetter) == std::tolower((unsigned char)*pSearch))
		{
			// Check for match
			char const* pMatch = pLetter + 1;
			char const* pTest = pSearch + 1;
			while (*pTest != 0)
			{
				// We've run off the end; don't bother.
				if (*pMatch == 0)
					return 0;

				if (std::tolower((unsigned char)*pMatch) != std::tolower((unsigned char)*pTest))
					break;

				++pMatch;
				++pTest;
			}

			// Found a match!
			if (*pTest == 0)
				return pLetter;
		}

		++pLetter;
	}

	return 0;
}


//-----------------------------------------------------------------------------
// Purpose: Remove specified string ( and any args attached to it ) from command line
// Input  : *pszParm - 
//-----------------------------------------------------------------------------
void ICommandLine::RemoveParm( const char *pszParm )
{
	if ( !m_pszCmdLine )
		return;

	// Search for first occurrence of pszParm
	char *p, *found;
	char *pnextparam;
	intp n;
	std::size_t curlen;

	p = m_pszCmdLine;
	while ( *p )
	{
		curlen = std::strlen( p );

		found = _stristr( p, pszParm );
		if ( !found )
			break;
			
		pnextparam = found + 1;
		bool bHadQuote = false;
		if ( found > m_pszCmdLine && found[-1] == '\"' )
			bHadQuote = true;
		
		while ( pnextparam && *pnextparam && (*pnextparam != ' ') && (*pnextparam != '\"') )
			pnextparam++;

		if ( pnextparam && ( static_cast<std::size_t>( pnextparam - found ) > std::strlen( pszParm ) ) )
		{
			p = pnextparam;
			continue;
		}

		while ( pnextparam && *pnextparam && (*pnextparam != '-') && (*pnextparam != '+') )
			pnextparam++;

		if ( bHadQuote )
		{
			found--;
		}

		if ( pnextparam && *pnextparam )
		{
			// We are either at the end of the string, or at the next param.  Just chop out the current param.
			n = curlen - ( pnextparam - p ); // # of characters after this param.
			std::memmove( found, pnextparam, n );

			found[n] = '\0';
		}
		else
		{
			// Clear out rest of string.
			n = pnextparam - found;
			std::memset( found, 0, n );
		}
	}

	// Strip and trailing ' ' characters left over.
	while ( 1 )
	{
		intp len = std::strlen( m_pszCmdLine );
		if ( len == 0 || m_pszCmdLine[ len - 1 ] != ' ' )
			break;
		
		m_pszCmdLine[len - 1] = '\0';
	}

	ParseCommandLine();
}


//-----------------------------------------------------------------------------
// Purpose: Append parameter and argument values to command line
// Input  : *pszParm - 
//			*pszValues - 
//-----------------------------------------------------------------------------
void ICommandLine::AppendParm( const char *pszParm, const char *pszValues )
{
	intp nNewLength = 0;
	char *pCmdString;

	nNewLength = std::strlen( pszParm );            // Parameter.
	if ( pszValues )
		nNewLength += std::strlen( pszValues ) + 1;  // Values + leading space character.
	nNewLength++; // Terminal 0;

	if ( !m_pszCmdLine )
	{
		m_pszCmdLine = new char[ nNewLength ];
		std::strcpy( m_pszCmdLine, pszParm );
		if ( pszValues )
		{
			std::strcat( m_pszCmdLine, " " );
			std::strcat( m_pszCmdLine, pszValues );
		}

		ParseCommandLine();
		return;
	}

	// Remove any remnants from the current Cmd Line.
	RemoveParm( pszParm );

	nNewLength += std::strlen( m_pszCmdLine ) + 1 + 1;

	pCmdString = new char[ nNewLength ];
	std::memset( pCmdString, 0, nNewLength );

	std::strcpy ( pCmdString, m_pszCmdLine ); // Copy old command line.
	std::strcat ( pCmdString, " " ); // Put in a space
	std::strcat ( pCmdString, pszParm );
	if ( pszValues )
	{
		std::strcat( pCmdString, " " );
		std::strcat( pCmdString, pszValues );
	}

	// Kill off the old one
	delete[] m_pszCmdLine;

	// Point at the new command line.
	m_pszCmdLine = pCmdString;

	ParseCommandLine();
}


//-----------------------------------------------------------------------------
// Purpose: Return current command line
// Output : const char
//-----------------------------------------------------------------------------
const char *ICommandLine::GetCmdLine( void ) const
{
	return m_pszCmdLine;
}


//-----------------------------------------------------------------------------
// Purpose: Search for the parameter in the current commandline
// Input  : *psz - 
//			**ppszValue - 
// Output : char
//-----------------------------------------------------------------------------
const char *ICommandLine::CheckParm( const char *psz, const char **ppszValue ) const
{
	if ( ppszValue )
		*ppszValue = nullptr;
	
	int i = FindParm( psz );
	if ( i == 0 )
		return nullptr;
	
	if ( ppszValue )
	{
		if ( (i+1) >= m_nParmCount )
		{
			*ppszValue = nullptr;
		}
		else
		{
			*ppszValue = m_ppParms[i+1];
		}
	}
	
	return m_ppParms[i];
}


//-----------------------------------------------------------------------------
// Adds an argument..
//-----------------------------------------------------------------------------
void ICommandLine::AddArgument( const char *pFirst, const char *pLast )
{
	if ( pLast == pFirst )
		return;

	if ( m_nParmCount >= MAX_PARAMETERS )
		Error( "ICommandLine::AddArgument: exceeded %d parameters", MAX_PARAMETERS );

	std::size_t nLen = ( pLast - pFirst ) + 1;
	m_ppParms[m_nParmCount] = new char[nLen];
	std::memcpy( m_ppParms[m_nParmCount], pFirst, nLen - 1 );
	m_ppParms[m_nParmCount][nLen - 1] = 0;

	++m_nParmCount;
}


//-----------------------------------------------------------------------------
// Parse command line...
//-----------------------------------------------------------------------------
void ICommandLine::ParseCommandLine()
{
	CleanUpParms();
	if (!m_pszCmdLine)
		return;

	const char *pChar = m_pszCmdLine;
	while ( *pChar && V_isspace(*pChar) )
	{
		++pChar;
	}

	bool bInQuotes = false;
	const char *pFirstLetter = nullptr;
	for ( ; *pChar; ++pChar )
	{
		if ( bInQuotes )
		{
			if ( *pChar != '\"' )
				continue;

			AddArgument( pFirstLetter, pChar );
			pFirstLetter = nullptr;
			bInQuotes = false;
			continue;
		}

		// Haven't started a word yet...
		if ( !pFirstLetter )
		{
			if ( *pChar == '\"' )
			{
				bInQuotes = true;
				pFirstLetter = pChar + 1;
				continue;
			}

			if ( V_isspace( *pChar ) )
				continue;

			pFirstLetter = pChar;
			continue;
		}

		// Here, we're in the middle of a word. Look for the end of it.
		if ( V_isspace( *pChar ) )
		{
			AddArgument( pFirstLetter, pChar );
			pFirstLetter = nullptr;
		}
	}

	if ( pFirstLetter )
	{
		AddArgument( pFirstLetter, pChar );
	}
}


//-----------------------------------------------------------------------------
// Individual command line arguments
//-----------------------------------------------------------------------------
void ICommandLine::CleanUpParms()
{
	for ( int i = 0; i < m_nParmCount; ++i )
	{
		delete [] m_ppParms[i];
		m_ppParms[i] = nullptr;
	}
	m_nParmCount = 0;
}


//-----------------------------------------------------------------------------
// Returns individual command line arguments
//-----------------------------------------------------------------------------
int ICommandLine::ParmCount() const
{
	return m_nParmCount;
}

int ICommandLine::FindParm( const char *psz ) const
{
	// Start at 1 so as to not search the exe name
	for ( int i = 1; i < m_nParmCount; ++i )
	{
		if ( !V_tier0_stricmp( psz, m_ppParms[i] ) )
			return i;
	}
	return 0;
}

bool ICommandLine::HasParm( const char *psz ) const
{
	return ( FindParm( psz ) != 0 );
}

const char* ICommandLine::GetParm( int nIndex ) const
{
	contract_assert( (nIndex >= 0) && (nIndex < m_nParmCount) );
	if ( (nIndex < 0) || (nIndex >= m_nParmCount) )
		return "";
	return m_ppParms[nIndex];
}
void ICommandLine::SetParm( int nIndex, char const *pParm )
{
	if ( pParm )
	{
		contract_assert( (nIndex >= 0) && (nIndex < m_nParmCount) );
		if ( (nIndex >= 0) && (nIndex < m_nParmCount) )
		{
			if ( m_ppParms[nIndex] )
				delete[] m_ppParms[nIndex];
			std::size_t nLen = std::strlen( pParm ) + 1;
			m_ppParms[nIndex] = new char[nLen];
			std::memcpy( m_ppParms[nIndex], pParm, nLen );
		}

	}

}


//-----------------------------------------------------------------------------
// Returns the argument after the one specified, or the default if not found
//-----------------------------------------------------------------------------
const char *ICommandLine::ParmValue( const char *psz, const char *pDefaultVal ) const
{
	int nIndex = FindParm( psz );
	if (( nIndex == 0 ) || (nIndex == m_nParmCount - 1))
		return pDefaultVal;

	// Probably another cmdline parameter instead of a valid arg if it starts with '+' or '-'
	if ( m_ppParms[nIndex + 1][0] == '-' || m_ppParms[nIndex + 1][0] == '+' )
		return pDefaultVal;

	return m_ppParms[nIndex + 1];
}

int	ICommandLine::ParmValue( const char *psz, int nDefaultVal ) const
{
	int nIndex = FindParm( psz );
	if (( nIndex == 0 ) || (nIndex == m_nParmCount - 1))
		return nDefaultVal;

	// Probably another cmdline parameter instead of a valid arg if it starts with '+' or '-'
	if ( m_ppParms[nIndex + 1][0] == '-' || m_ppParms[nIndex + 1][0] == '+' )
		return nDefaultVal;

	return std::atoi( m_ppParms[nIndex + 1] );
}

float ICommandLine::ParmValue( const char *psz, float flDefaultVal ) const
{
	int nIndex = FindParm( psz );
	if (( nIndex == 0 ) || (nIndex == m_nParmCount - 1))
		return flDefaultVal;

	// Probably another cmdline parameter instead of a valid arg if it starts with '+' or '-'
	if ( m_ppParms[nIndex + 1][0] == '-' || m_ppParms[nIndex + 1][0] == '+' )
		return flDefaultVal;

	return std::atof( m_ppParms[nIndex + 1] );
}
