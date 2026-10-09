#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.tier1;

class IFileSystem;

//-----------------------------------------------------------------------------
// Loads, unloads the file system DLL
//-----------------------------------------------------------------------------
bool FileSystem_LoadFileSystemModule( void );
void FileSystem_UnloadFileSystemModule( void );

CSysModule * FileSystem_LoadModule( const char* path );
void FileSystem_UnloadModule( CSysModule *pModule );

bool FileSystem_Init( );
void FileSystem_Shutdown( void );

// Sets the file system search path based on the game directory
bool FileSystem_SetGameDirectory( char const* pGameDir );

extern IFileSystem *g_pFileSystem;
