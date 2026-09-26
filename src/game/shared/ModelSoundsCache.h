//====== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef MODELSOUNDSCACHE_H
#define MODELSOUNDSCACHE_H

#include "cached_file_data.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include <flat_set>

#define MODELSOUNDSCACHE_VERSION		5

class CStudioHdr;


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

#endif // MODELSOUNDSCACHE_H
