// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// main precompiled header for client files

import std;
import ks.tier1;








#ifndef DEDICATED
#include "engineui.h"
#include "localize/ilocalize.h"
#endif
#include "enginebugreporter.h"
#include "bugreporter/bugreporter.h"

static ConVar bugreporter_username( "bugreporter_username", "", FCVAR_ARCHIVE, "Username to use for bugreporter" );

// Evil hack shim to expose convar to bugreporter_filequeue
class CBugReporterDefaultUsername : public IBugReporterDefaultUsername
{
public:
	virtual char const	*GetDefaultUsername() const
	{
		return bugreporter_username.GetString();
	}
};

static CBugReporterDefaultUsername g_ExposeBugreporterUsername;

class CStubBugReporter : public IEngineBugReporter
{
public:
	virtual void		Init( void ) {}
	virtual void		Shutdown( void ) {}

	virtual bool		ShouldPause() const { return false; }

	virtual bool		IsVisible() const { return false; }

	virtual int			GetBugSubmissionCount() const { return 0; }
	virtual void		ClearBugSubmissionCount() {}
};

static CStubBugReporter g_BugReporter;
IEngineBugReporter *bugreporter = &g_BugReporter;

unsigned long GetRam()
{
	unsigned long Ram = 0;
	std::FILE *fh = std::fopen( "/proc/meminfo", "r" );
	if( fh )
	{
		char buf[ 256 ];
		const char szMemTotal[] = "MemTotal:";

		while( std::fgets( buf, sizeof( buf ), fh ) )
		{
			if ( !V_strnicmp( buf, szMemTotal, sizeof( szMemTotal ) - 1 ) )
			{
				Ram = std::atoi( buf + sizeof( szMemTotal ) - 1 ) / 1024;
				break;
			}
		}

		std::fclose( fh );
	}
	return Ram;
}

void DisplaySystemVersion( char *osversion, int maxlen )
{
	osversion[ 0 ] = 0;
	std::FILE *fpKernelVer = std::fopen( "/proc/version_signature", "r" );

	if ( !fpKernelVer )
	{
		V_strncat ( osversion, "Linux ", maxlen, COPY_ALL_CHARACTERS );
	}
	else
	{
		std::fgets( osversion, maxlen, fpKernelVer );
		osversion[ maxlen - 1 ] = 0;

		char *szlf = V_strrchr( osversion, '\n' );
		if( szlf )
			*szlf = '\0';

		std::fclose( fpKernelVer );
	}
}
