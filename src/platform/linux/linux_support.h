#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.core.platform;
import ks.platform;
import ks.simd;
import ks.tier1.appinstance;
import ks.tier1.keyvalues;

inline constexpr int FILE_ATTRIBUTE_DIRECTORY = 0x4000;

struct FIND_DATA
{
	// public data
	int dwFileAttributes;
	char cFileName[k_cMaxPath]; // the file name returned from the call
	char cBaseDir[k_cMaxPath]; // the root dir for this find
	
	int numMatches;
	ks::platform::directory_listing *listing;
};

using WIN32_FIND_DATA = FIND_DATA;

HANDLE FindFirstFile( const char *findName, FIND_DATA *dat);
bool FindNextFile(HANDLE handle, FIND_DATA *dat);
bool FindClose(HANDLE handle);
const char *findFileInDirCaseInsensitive(const char* path, char *file);
