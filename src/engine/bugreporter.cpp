// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// main precompiled header for client files

import std;
import ks.tier1;

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
