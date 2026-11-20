// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#if defined(CLIENT_DLL)
#elif defined(GAME_DLL)
#endif
import std;
#if defined(CLIENT_DLL)
#elif defined(GAME_DLL)
#endif
import ks.core.platform;
import ks.core.types;
import ks.platform;
import ks.simd;
import ks.tier1.appinstance;
import ks.tier1.keyvalues;
import ks.tier1.strtools;
#include "platform/linux/linux_support.h"


static int FillDataStruct( FIND_DATA *dat )
{
	if ( dat->numMatches < 0 )
		return -1;

	ks::platform::directory_entry entry;
	if ( !ks::platform::listing_entry( dat->listing, (unsigned long)dat->numMatches, &entry ) )
		return -1;

	dat->dwFileAttributes = entry.info.is_directory ? FILE_ATTRIBUTE_DIRECTORY : 0;
	V_snprintf( dat->cFileName, sizeof(dat->cFileName), "%s", entry.name );

	dat->numMatches--;
	return 1;
}

HANDLE FindFirstFile( const char *fileName, FIND_DATA *dat )
{
	char nameStore[k_cMaxPath];
	char *dir = nullptr;
	int iret = -1;

	V_strncpy( nameStore, fileName, sizeof( nameStore ) );

	if ( std::strrchr( nameStore, '/' ) )
	{
		dir = nameStore;
		while ( std::strrchr( dir, '/' ) )
		{
			// zero this with the dir name
			dir = std::strrchr( nameStore, '/' );
			*dir = '\0';

			dir = nameStore;
			ks::platform::file_info dirCheck{};
			if ( ks::platform::stat_path( dir, &dirCheck ) && dirCheck.is_directory )
				break;
		}
	}
	else
	{
		// couldn't find a dir seperator...
		return (HANDLE)-1;
	}

	if ( std::strlen( dir ) > 0 )
	{
		V_strncpy( dat->cBaseDir, dir, sizeof( dat->cBaseDir ) );
		dat->listing = ks::platform::list_directory( dir, fileName + std::strlen( dir ) + 1 );
		if ( dat->listing != nullptr )
		{
			dat->numMatches = (int)ks::platform::listing_count( dat->listing ) - 1;
			iret = FillDataStruct( dat );
			if ( iret < 0 )
			{
				ks::platform::free_listing( dat->listing );
				dat->listing = nullptr;
			}
		}
	}

	return (HANDLE)(intp)iret;
}

bool FindNextFile( HANDLE handle, FIND_DATA *dat )
{
	if ( dat->numMatches < 0 )
	{
		if ( dat->listing != nullptr )
		{
			ks::platform::free_listing( dat->listing );
			dat->listing = nullptr;
		}
		return false; // no matches left
	}

	FillDataStruct( dat );
	return true;
}

bool FindClose( HANDLE handle )
{
	return true;
}


const char *findFileInDirCaseInsensitive( const char *file, char *pFileNameOut )
{
	const char *dirSep = std::strrchr( file, '/' );
	if ( !dirSep )
	{
		dirSep = std::strrchr( file, '\\' );
		if ( !dirSep )
			return nullptr;
	}

	char dirName[MAX_PATH];
	std::size_t dirLength = (std::size_t)( dirSep - file );
	if ( dirLength > sizeof( dirName ) - 1 )
		dirLength = sizeof( dirName ) - 1;
	std::memcpy( dirName, file, dirLength );
	dirName[dirLength] = '\0';

	if ( !ks::platform::find_name_ignoring_case( dirName, dirSep + 1, pFileNameOut, MAX_PATH ) )
	{
		V_strncpy( pFileNameOut, file, MAX_PATH );
		V_strlower( pFileNameOut );
	}

	return pFileNameOut;
}
