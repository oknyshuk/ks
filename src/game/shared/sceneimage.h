#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

class ISceneTokenProcessor;

class ISceneCompileStatus
{
public:
	virtual void UpdateStatus( char const *pchSceneName, bool bQuiet, int nIndex, int nCount ) = 0;
};

namespace ks { class byte_writer; }

class ISceneImage
{
public:
	virtual bool CreateSceneImageFile( ks::byte_writer &targetBuffer, char const *pchModPath, bool bLittleEndian, bool bQuiet, ISceneCompileStatus *Status ) = 0;
	// This will update the scenes.image file, or create a new one if it doesn't exist.  
	// The caller should pass in the existing .image file in targetBuffer!!!
	virtual bool UpdateSceneImageFile( ks::byte_writer &targetBuffer, char const *pchModPath, bool bLittleEndian, bool bQuiet, ISceneCompileStatus *Status, std::string *pFilesToUpdate, int nUpdateCount ) = 0;
};

extern ISceneImage *g_pSceneImage;
extern ISceneTokenProcessor *tokenprocessor;
