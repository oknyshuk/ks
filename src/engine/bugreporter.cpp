//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#include "client_pch.h"
#include "enginebugreporter.h"
#include "bugreporter/bugreporter.h"
#include "tier1/interface.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

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
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CBugReporterDefaultUsername,IBugReporterDefaultUsername, INTERFACEVERSION_BUGREPORTER_DEFAULT_USER_NAME, g_ExposeBugreporterUsername );

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
	FILE *fh = fopen( "/proc/meminfo", "r" );
	if( fh )
	{
		char buf[ 256 ];
		const char szMemTotal[] = "MemTotal:";

		while( fgets( buf, sizeof( buf ), fh ) )
		{
			if ( !Q_strnicmp( buf, szMemTotal, sizeof( szMemTotal ) - 1 ) )
			{
				Ram = atoi( buf + sizeof( szMemTotal ) - 1 ) / 1024;
				break;
			}
		}

		fclose( fh );
	}
	return Ram;
}

void DisplaySystemVersion( char *osversion, int maxlen )
{
	osversion[ 0 ] = 0;
	FILE *fpKernelVer = fopen( "/proc/version_signature", "r" );

	if ( !fpKernelVer )
	{
		Q_strncat ( osversion, "Linux ", maxlen, COPY_ALL_CHARACTERS );
	}
	else
	{
		fgets( osversion, maxlen, fpKernelVer );
		osversion[ maxlen - 1 ] = 0;

		char *szlf = Q_strrchr( osversion, '\n' );
		if( szlf )
			*szlf = '\0';

		fclose( fpKernelVer );
	}
}
