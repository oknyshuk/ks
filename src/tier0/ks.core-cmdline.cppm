// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.core.cmdline;

import ks.core.platform;

export extern "C++" {

//-----------------------------------------------------------------------------
// Purpose: Interface to engine command line
//-----------------------------------------------------------------------------
class ICommandLine
{
public:
	// Construction
	ICommandLine( void );
	~ICommandLine( void );

	void		CreateCmdLine( const char *commandline );
	void		CreateCmdLine( int argc, char **argv );
	const char	*GetCmdLine( void ) const;

	// Check whether a particular parameter exists
	const char	*CheckParm( const char *psz, const char **ppszValue = 0 ) const;
	// A bool return of whether param exists, useful for just checking if param that is just a flag is set
	bool		HasParm( const char *psz ) const;
	void		RemoveParm( const char *parm );
	void		AppendParm( const char *pszParm, const char *pszValues );

	// Returns the argument after the one specified, or the default if not found
	const char	*ParmValue( const char *psz, const char *pDefaultVal = 0 ) const;
	int			ParmValue( const char *psz, int nDefaultVal ) const;
	float		ParmValue( const char *psz, float flDefaultVal ) const;

	// Gets at particular parameters
	int			ParmCount() const;
	int			FindParm( const char *psz ) const;	// Returns 0 if not found.
	const char* GetParm( int nIndex ) const;

	// copies the string passwed
	void SetParm( int nIndex, char const *pNewParm );

	const char **GetParms() const { return (const char**)m_ppParms; }

private:
	enum
	{
		MAX_PARAMETER_LEN = 128,
		MAX_PARAMETERS = 256,
	};

	// When the commandline contains @name, it reads the parameters from that file
	void LoadParametersFromFile( const char *&pSrc, char *&pDst, intp maxDestLen, bool bInQuotes );

	// Parse command line...
	void ParseCommandLine();

	// Frees the command line arguments
	void CleanUpParms();

	// Adds an argument..
	void AddArgument( const char *pFirst, const char *pLast );

	// Copy of actual command line
	char *m_pszCmdLine;

	// Pointers to each argument...
	int m_nParmCount;
	char *m_ppParms[MAX_PARAMETERS];
};

//-----------------------------------------------------------------------------
// Gets a singleton to the commandline interface
// NOTE: The #define trickery here is necessary for backwards compat:
// this interface used to lie in the vstdlib library.
//-----------------------------------------------------------------------------
ICommandLine *CommandLine();

//-----------------------------------------------------------------------------
// Process related functions
//-----------------------------------------------------------------------------
const tchar *Plat_GetCommandLine();
// helper function for OS's that don't have a ::GetCommandLine() call
void Plat_SetCommandLine( const char *cmdLine );

}
