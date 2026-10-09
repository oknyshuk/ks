#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class IFileSystem;
class CSysModule;


//-----------------------------------------------------------------------------
// Filesystem interface
//-----------------------------------------------------------------------------
extern IFileSystem *g_pFileSystem;


//-----------------------------------------------------------------------------
// Loads DLLs through the file system
//-----------------------------------------------------------------------------
CSysModule *FileSystem_LoadModule(const char *path);
void FileSystem_UnloadModule(CSysModule *pModule);

void FileSystem_SetWhitelistSpewFlags();
void FileSystem_UpdateAddonSearchPaths( IFileSystem *pFileSystem );
