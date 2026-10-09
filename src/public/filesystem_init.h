#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "filesystem.h"

// If this option is on the command line, then filesystem_init won't bring up the vconfig
// dialog even if FS_ERRORMODE_VCONFIG is used.
inline constexpr const char *CMDLINEOPTION_NOVCONFIG = "-NoVConfig";

inline constexpr const char *GAMEDIR_TOKEN = "VProject";
inline constexpr const char *GAMEROOT_TOKEN = "VGame";
inline constexpr const char *CONTENTROOT_TOKEN = "VContent";


#define PATHSEPARATOR(c) ((c) == '/')


enum FSReturnCode_t
{
	FS_OK,
	FS_MISSING_GAMEINFO_FILE,
	FS_INVALID_GAMEINFO_FILE,
	FS_INVALID_PARAMETERS,
	FS_UNABLE_TO_INIT
};


enum FSErrorMode_t
{
	FS_ERRORMODE_AUTO,		// Call Error() in case of an error.
	FS_ERRORMODE_VCONFIG,	// Call Error() for errors and run vconfig when appropriate.
	FS_ERRORMODE_NONE,		// Just return FSReturnCode values and setup the string for FileSystem_GetLastErrorString.
};


class CFSSetupInfo
{
public:
	CFSSetupInfo();

// Inputs.
public:
	// If this is set, then the init code will look in this directory up to the root for gameinfo.txt.
	// It must be set for FileSystem_LoadSearchPaths to work.
	//
	// (default: null)
	const char		*m_pDirectoryName;

	// If this is true, then it won't look at -vproject, -game, or the vproject environment variable
	// to find gameinfo.txt. If this is true, then m_pDirectoryName must be set.
	//
	// (default: false)
	bool			m_bOnlyUseDirectoryName;

// Outputs (if it returns FS_OK).
public:
	char			m_GameInfoPath[512];	// The directory that gameinfo.txt lives in.
};	


class CFSLoadModuleInfo	: public CFSSetupInfo
{
public:
	CFSLoadModuleInfo();

// Inputs.
public:
	// Full path to the file system DLL (gotten from FileSystem_GetFileSystemDLLName).
	const char		*m_pFileSystemDLLName;

	// Passed to IFileSystem::Connect.
	CreateInterfaceFn	m_ConnectFactory;

// Outputs (if it returns FS_OK).
public:
	// The filesystem you got from FileSystem_LoadFileSystemModule.
	IFileSystem		*m_pFileSystem;
	CSysModule		*m_pModule;
};	


class CFSSearchPathsInit
{
public:
	CFSSearchPathsInit();

// Inputs.
public:
	// This specifies the directory where gameinfo.txt is. This must be set.
	const char		*m_pDirectoryName;

	// If this is set, then it will add a search path with _<language> appended to the pathname
	// for each search path with a path ID of "game".
	// (default: null)
	const char		*m_pLanguage;

	// This is the filesystem FileSystem_LoadSearchPaths is talking to.
	IFileSystem		*m_pFileSystem;

	// Supplied by the engine, which is the only caller that wants addons.
	void			(*m_pfnAddonSearchPaths)( IFileSystem *pFileSystem );

// Outputs.
public:
	// This is the location of the first search path called "game", which also becomes your "mod" search path.
	char			m_ModPath[512];
};


const char *GetVProjectCmdLineValue();


// Call this to use a bin directory relative to VPROJECT
void FileSystem_UseVProjectBinDir( bool bEnable );

// This is used by all things that use the application framework:
// Note that the application framework automatically takes care of step 1 if you use CFSApplication.
// Step 1: Ask filesystem_init for the name of the filesystem DLL to load
FSReturnCode_t FileSystem_GetFileSystemDLLName( char *pFileSystemDLL, int nMaxLen );

// Step 2: Use filesystem framework to load/connect/init that filesystem DLL
// -or- just set up the environment and get back the gameinfo.txt path
// The second method is used by the application framework, which wants to connect/init the filesystem itself
FSReturnCode_t FileSystem_LoadFileSystemModule( CFSLoadModuleInfo &info );
FSReturnCode_t FileSystem_SetupEnvironment( CFSSetupInfo &info );

// Step 3: Ask filesystem_init to set up the executable search path (FileSystem_SetBasePaths)

// Step 4: Load the search paths out of pGameDirectory\gameinfo.txt.
FSReturnCode_t FileSystem_LoadSearchPaths( CFSSearchPathsInit &initInfo );

// This is automatically done during step 3, but if you want to redo all the search
// paths (like Hammer does), you can call this to reset executable_path.
FSReturnCode_t FileSystem_SetBasePaths( IFileSystem *pFileSystem );

// Utility function to add the PLATFORM search path.
void FileSystem_AddSearchPath_Platform( IFileSystem *pFileSystem, const char *szGameInfoPath );

// See FSErrorMode_t. If you don't specify one here, then the default is FS_ERRORMODE_VCONFIG.
void FileSystem_SetErrorMode( FSErrorMode_t errorMode = FS_ERRORMODE_VCONFIG );

bool FileSystem_GetExecutableDir( char *exedir, int exeDirLen );

// Returns the last error.
const char *FileSystem_GetLastErrorString();

void Q_getwd( char *out, int outSize );

KeyValues* ReadKeyValuesFile( const char *pFilename );
