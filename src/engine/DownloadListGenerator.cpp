// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.tier1;
#include "DownloadListGenerator.h"
#include "filesystem.h"
#include "filesystem_engine.h"
#include "sys.h"
#include "cmd.h"
#include "common.h"
#include "quakedef.h"
#include "vengineserver_impl.h"
#include "checksum_engine.h"
import std;

CDownloadListGenerator g_DownloadListGenerator;
CDownloadListGenerator &DownloadListGenerator()
{
	return g_DownloadListGenerator;
}

ConVar	sv_logdownloadlist( "sv_logdownloadlist", "0" );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CDownloadListGenerator::CDownloadListGenerator()
	: m_AlreadyWrittenFileNames( true )
{
	m_hReslistFile = FILESYSTEM_INVALID_HANDLE;
	m_pStringTable = nullptr;
	m_mapName[0] = 0;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDownloadListGenerator::SetStringTable( INetworkStringTable *pStringTable )
{

	m_pStringTable = pStringTable;

	// reset the duplication list
	m_AlreadyWrittenFileNames.clear();

	// add in the bsp file to the list, and its node graph and nav mesh
	char path[k_cMaxPath];
	V_snprintf(path, sizeof(path), "maps\\%s.bsp", m_mapName);
	OnResourcePrecached(path);

	bool useNodeGraph = true;
	KeyValues *modinfo = new KeyValues("ModInfo");
	if ( modinfo->LoadFromFile( g_pFileSystem, "gameinfo.txt" ) )
	{
		useNodeGraph = modinfo->GetInt( "nodegraph", 1 ) != 0;
	}
	modinfo->deleteThis();

	if ( useNodeGraph )
	{
		V_snprintf(path, sizeof(path), "maps\\graphs\\%s.ain", m_mapName);
		OnResourcePrecached(path);
	}

	V_snprintf(path, sizeof(path), "maps\\%s.nav", m_mapName);
	OnResourcePrecached(path);

	char resfilename[MAX_OSPATH];
	V_snprintf( resfilename, sizeof( resfilename), "maps/%s.res", m_mapName );

	KeyValues::AutoDelete resfilekeys( "resourcefiles" );
	if ( resfilekeys->LoadFromFile( g_pFileSystem, resfilename, "GAME" ) )
	{
		for ( KeyValues *pKey = resfilekeys->GetFirstSubKey(); pKey != nullptr; pKey = pKey->GetNextKey() )
		{
			OnResourcePrecached( pKey->GetName() );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: call to mark level load/end
//-----------------------------------------------------------------------------
void CDownloadListGenerator::OnLevelLoadStart(const char *levelName)
{

	// close the previous level reslist, if any
	if (m_hReslistFile != FILESYSTEM_INVALID_HANDLE)
	{
		g_pFileSystem->Close(m_hReslistFile);
		m_hReslistFile = FILESYSTEM_INVALID_HANDLE;
	}

	// reset the duplication list
	m_AlreadyWrittenFileNames.clear();

	if ( sv_logdownloadlist.GetBool() )
	{
		// open the new level reslist
		char path[MAX_OSPATH];
		g_pFileSystem->CreateDirHierarchy( "DownloadLists", "MOD" );
		V_snprintf(path, sizeof(path), "DownloadLists/%s.lst", levelName);
		m_hReslistFile = g_pFileSystem->Open(path, "wt", "GAME");
	}

	// add a slash to the end of com_gamedir, so we can only deal with files for this mod
	V_snprintf( m_gameDir, sizeof(m_gameDir), "%s/", com_gamedir );
	V_FixSlashes( m_gameDir, '/' );

	// save off the map name
	V_strcpy_safe( m_mapName, levelName );
}

//-----------------------------------------------------------------------------
// Purpose: call to mark level load/end
//-----------------------------------------------------------------------------
void CDownloadListGenerator::OnLevelLoadEnd()
{

	if ( m_hReslistFile != FILESYSTEM_INVALID_HANDLE )
	{
		g_pFileSystem->Close(m_hReslistFile);
		m_hReslistFile = FILESYSTEM_INVALID_HANDLE;
	}
	m_pStringTable = nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: logs and handles mdl files being precaches
//-----------------------------------------------------------------------------
void CDownloadListGenerator::OnModelPrecached(const char *relativePathFileName)
{

	if (V_strstr(relativePathFileName, ".vmt"))
	{
		// it's a materials file, make sure that it starts in the materials directory, and we get the .vtf
		char file[k_cMaxPath];

		if ( StringHasPrefix( relativePathFileName, "materials" ) )
		{
			V_strncpy(file, relativePathFileName, sizeof(file));
		}
		else
		{
			// prepend the materials directory
			V_snprintf(file, sizeof(file), "materials\\%s", relativePathFileName);
		}

		OnResourcePrecached(file);

		// get the matching vtf file
		char *ext = V_strstr(file, ".vmt");
		if (ext)
		{
			V_strncpy(ext, ".vtf", 5);
			OnResourcePrecached(file);
		}
	}
	else
	{
		OnResourcePrecached(relativePathFileName);
	}
}

//-----------------------------------------------------------------------------
// Purpose: logs sound file access
//-----------------------------------------------------------------------------
void CDownloadListGenerator::OnSoundPrecached(const char *relativePathFileName)
{

	// skip any special characters
	if (!V_isalnum(relativePathFileName[0]))
	{
		++relativePathFileName;
	}

	// prepend the sound/ directory if necessary
	char file[k_cMaxPath];
	if ( StringHasPrefix( relativePathFileName, "sound" ) )
	{
		V_strncpy(file, relativePathFileName, sizeof(file));
	}
	else
	{
		// prepend the materials directory
		V_snprintf(file, sizeof(file), "sound\\%s", relativePathFileName);
	}

	OnResourcePrecached(file);
}

//-----------------------------------------------------------------------------
// Purpose: logs the precache as a file access
//-----------------------------------------------------------------------------
void CDownloadListGenerator::OnResourcePrecached( const char *pRelativePathFileName )
{
	// not supporting

	// ignore empty string
	if ( pRelativePathFileName[0] == 0 )
		return;

	// ignore files that start with '*' since they signify special models
	if ( pRelativePathFileName[0] == '*' )
		return;

	if ( V_IsAbsolutePath( pRelativePathFileName ) )
	{
		Warning( "*** CDownloadListGenerator::OnResourcePrecached: Encountered full path %s!\n", pRelativePathFileName );
		return;
	}

	char pRelativePath[MAX_PATH];
	V_strncpy( pRelativePath, pRelativePathFileName, sizeof(pRelativePath) );
	V_FixSlashes( pRelativePath, '/' );

	// make sure the filename hasn't already been written
	if ( m_AlreadyWrittenFileNames.contains( pRelativePath ) )
		return;

	// record in list, so we don't write it again
	m_AlreadyWrittenFileNames.intern( pRelativePath );

	// don't allow files for download the server doesn't have
	if ( !g_pFileSystem->FileExists( pRelativePath, "GAME" ) )
		return;	

	// add extras for mdl's
	char *pExt = const_cast< char* >( V_GetFileExtension( pRelativePath ) );
	if ( !V_stricmp( pExt, "mdl" ) )
	{
		V_strncpy(pExt, "vvd", 10);
		OnResourcePrecached(pRelativePath);

		V_strncpy(pExt, "ani", 10);
		OnResourcePrecached(pRelativePath);

		V_strncpy(pExt, "dx90.vtx", 10);
		OnResourcePrecached(pRelativePath);

		V_strncpy(pExt, "phy", 10);
		OnResourcePrecached(pRelativePath);

		V_strncpy(pExt, "jpg", 10);
		OnResourcePrecached(pRelativePath);
	}

	FileHandle_t handle = m_hReslistFile;
	if ( handle != FILESYSTEM_INVALID_HANDLE )
	{
		g_pFileSystem->Write("\"", 1, handle);
		g_pFileSystem->Write( pRelativePath, V_strlen(pRelativePath), handle );
		g_pFileSystem->Write("\"\n", 2, handle);
	}
	if ( m_pStringTable )
	{
		m_pStringTable->AddString( true, pRelativePath );
	}
}


//-----------------------------------------------------------------------------
// Purpose: marks a precached file as needing a specific CRC on the client
//-----------------------------------------------------------------------------
void CDownloadListGenerator::ForceExactFile( const char *relativePathFileName, ConsistencyType consistency )
{

	if ( !m_pStringTable )
		return;

	if ( consistency != CONSISTENCY_EXACT && consistency != CONSISTENCY_SIMPLE_MATERIAL )
	{
		consistency = CONSISTENCY_EXACT;
	}

	CRC32_t crc;
	bool error = true;
	char file[k_cMaxPath];
	const char *filePtr = relativePathFileName;

	if (V_strstr(relativePathFileName, ".vmt") || V_strstr(relativePathFileName, ".vtf"))
	{
		// it's a materials file, make sure that it starts in the materials directory, and we get the .vtf
		if ( StringHasPrefix(relativePathFileName, "materials" ) )
		{
			V_strncpy(file, relativePathFileName, sizeof(file));
		}
		else
		{
			// prepend the materials directory
			V_snprintf(file, sizeof(file), "materials\\%s", relativePathFileName);
		}
		error = !CRC_File( &crc, file );
		filePtr = file;
	}
	else
	{
		error = !CRC_File( &crc, relativePathFileName );
	}

	if ( error )
	{
		DevWarning( "Failed to CRC %s\n", relativePathFileName );
	}
	else
	{
		char relativeFileName[k_cMaxPath];
		V_strncpy( relativeFileName, filePtr, sizeof( relativeFileName ) );
		V_FixSlashes( relativeFileName, '/' );

		ExactFileUserData userData;
		userData.consistencyType = consistency;
		userData.crc = crc;

		int index = m_pStringTable->FindStringIndex( relativeFileName );
		if ( index != INVALID_STRING_INDEX )
		{
			m_pStringTable->SetStringUserData( index, sizeof( ExactFileUserData ), &userData );
		}
		else
		{
			m_pStringTable->AddString( true, relativeFileName, sizeof( ExactFileUserData ), &userData );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: marks a precached model as having a maximum size on the client
//-----------------------------------------------------------------------------
void CDownloadListGenerator::ForceModelBounds( const char *relativePathFileName, const Vector &mins, const Vector &maxs )
{

	if ( !m_pStringTable )
		return;

	if ( !relativePathFileName )
		relativePathFileName = "";

	if (!V_stristr(relativePathFileName, ".mdl"))
	{
		DevWarning( "Warning - trying to enforce model bounds on %s\n", relativePathFileName );
		return;
	}

	char relativeFileName[k_cMaxPath];
	V_strncpy( relativeFileName, relativePathFileName, sizeof( relativeFileName ) );
	V_FixSlashes( relativeFileName, '/' );

	ModelBoundsUserData userData;
	userData.consistencyType = CONSISTENCY_BOUNDS;
	userData.mins = mins;
	userData.maxs = maxs;

	int index = m_pStringTable->FindStringIndex( relativeFileName );
	if ( index != INVALID_STRING_INDEX )
	{
		m_pStringTable->SetStringUserData( index, sizeof( ModelBoundsUserData ), &userData );
	}
	else
	{
		m_pStringTable->AddString( true, relativeFileName, sizeof( ModelBoundsUserData ), &userData );
	}
}
