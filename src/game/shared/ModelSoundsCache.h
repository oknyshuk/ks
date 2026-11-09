#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "cached_file_data.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"

inline constexpr auto MODELSOUNDSCACHE_VERSION = 5;



#pragma pack(1)
class CModelSoundsCache : public IBaseCacheInfo
{
public:
	std::flat_set< int > sounds;

	CModelSoundsCache();
	CModelSoundsCache( const CModelSoundsCache& src );

	void PrecacheSoundList();

	virtual void Save( ks::byte_writer& buf );
	virtual void Restore( ks::byte_reader& buf );
	virtual void Rebuild( char const *filename );

	static void FindOrAddScriptSound( std::flat_set< int >& sounds, char const *soundname );
	static void BuildAnimationEventSoundList( CStudioHdr *hdr, std::flat_set< int >& sounds );
private:
	char const *GetSoundName( int index );
};
#pragma pack()
