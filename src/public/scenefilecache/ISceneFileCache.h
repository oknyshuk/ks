#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.tier1;
import std;

// the file cache can support persisting some calcs
struct SceneCachedData_t
{
	unsigned int	msecs;
	float			m_fLastSpeakSecs;
	int				numSounds;
	int				sceneId;
};

class ISceneFileCache : public IAppSystem
{
public:

	// async implemenation
	virtual std::size_t		GetSceneBufferSize( char const *filename ) = 0;
	virtual bool		GetSceneData( char const *filename, byte *buf, std::size_t bufsize ) = 0;

	// persisted scene data, returns true if valid, false otherwise
	virtual bool		GetSceneCachedData( char const *pFilename, SceneCachedData_t *pData ) = 0;
	virtual short		GetSceneCachedSound( int iScene, int iSound ) = 0;
	virtual const char	*GetSceneString( short stringId ) = 0;

	// Physically reloads image from disk
	virtual void		Reload() = 0;
};

inline constexpr const char *SCENE_FILE_CACHE_INTERFACE_VERSION = "SceneFileCache002";
