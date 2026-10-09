#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "cached_file_data.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
import std;

class CChoreoEvent;

inline constexpr auto SCENECACHE_VERSION = 7;


#pragma pack(1)
class CSceneCache : public IBaseCacheInfo
{
public:
	unsigned int		msecs;
	std::flat_multiset< int > sounds;

	CSceneCache();
//	CSceneCache( const CSceneCache& src );

	int	GetSoundCount() const;
	char const *GetSoundName( int index );

	virtual void Save( ks::byte_writer& buf );
	virtual void Restore( ks::byte_reader& buf );
	virtual void Rebuild( char const *filename );

	static unsigned int ComputeSoundScriptFileTimestampChecksum();
	static void PrecacheSceneEvent( CChoreoEvent *event, std::flat_multiset< int > &soundlist );
};
#pragma pack()
