#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.core.basetypes;
import ks.core.wchartypes;
import ks.platform;
import ks.simd;
import ks.tier1.iappsystem;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import std;

// the file cache can support persisting some calcs
struct SceneCachedData_t
{
	unsigned int	msecs;
	float			m_fLastSpeakSecs;
	int				numSounds;
	int				sceneId;
};

class ISceneFileCache
{
public:
	//--------------------------------------------------------
	// App system protocol. Not an IAppSystem: CAppSystemGroup drives it by kind (see AppSystemRef).
	//--------------------------------------------------------
	bool Connect( CreateInterfaceFn factory );
	void Disconnect();
	void *QueryInterface( const char *pInterfaceName );
	InitReturnVal_t Init();
	void Shutdown();
	const AppSystemInfo_t *GetDependencies() { return nullptr; }
	void Reconnect( CreateInterfaceFn factory, const char *pInterfaceName ) { ReconnectInterface( factory, pInterfaceName ); }


	// async implemenation
	std::size_t		GetSceneBufferSize( char const *filename );
	bool		GetSceneData( char const *filename, byte *buf, std::size_t bufsize );

	// persisted scene data, returns true if valid, false otherwise
	bool		GetSceneCachedData( char const *pFilename, SceneCachedData_t *pData );
	short		GetSceneCachedSound( int iScene, int iSound );
	const char	*GetSceneString( short stringId );

	// Physically reloads image from disk
	void		Reload();
};

inline constexpr const char *SCENE_FILE_CACHE_INTERFACE_VERSION = "SceneFileCache002";
