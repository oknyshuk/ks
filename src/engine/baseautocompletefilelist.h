#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.tier1;

//-----------------------------------------------------------------------------
// Purpose: Simple helper class for doing autocompletion of all files in a specific directory by extension
//-----------------------------------------------------------------------------
class CBaseAutoCompleteFileList
{
public:
	CBaseAutoCompleteFileList( const char *cmdname, const char *subdir, const char *extension )
	{
		m_pszCommandName	= cmdname;
		m_pszSubDir			= subdir;
		m_pszExtension		= extension;
	}

	int AutoCompletionFunc( const char *partial, char commands[ COMMAND_COMPLETION_MAXITEMS ][ COMMAND_COMPLETION_ITEM_LENGTH ] );

private:
	const char	*m_pszCommandName;
	const char	*m_pszSubDir;
	const char	*m_pszExtension;
};
