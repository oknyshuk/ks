//====== Copyright (c) 1996-2007, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "SceneCache.h"
#include "choreoscene.h"
#include "choreoevent.h"


extern ISoundEmitterSystemBase *soundemitterbase;
CChoreoScene *BlockingLoadScene( const char *filename );

CSceneCache::CSceneCache()
{
	msecs = 0;
}

//CSceneCache::CSceneCache( const CSceneCache& src )
//{
//	msecs  = src.msecs;
//	sounds = src.sounds;
//}

int	CSceneCache::GetSoundCount() const
{
	return std::ssize( sounds );
}

char const *CSceneCache::GetSoundName( int index )
{
	return soundemitterbase->GetSoundName( sounds.begin()[ index ] );
}

void CSceneCache::Save( ks::byte_writer& buf )
{
	buf.write< unsigned int >( msecs );

	unsigned short c = GetSoundCount();
	buf.write< short >( c );
	
	Assert( std::ssize( sounds ) <= 65536 );

	for ( int i = 0; i < c; ++i )
	{
		buf.write_cstring( GetSoundName( i ) );
	}
}

void CSceneCache::Restore( ks::byte_reader& buf )
{
	MEM_ALLOC_CREDIT();

	msecs = buf.read< unsigned int >();

	unsigned short c = (unsigned short)buf.read< short >();

	for ( int i = 0; i < c; ++i )
	{
		char soundname[ 512 ];
		buf.read_cstring( soundname, sizeof( soundname ) );

		int idx = soundemitterbase->GetSoundIndex( soundname );
		if ( soundemitterbase->IsValidIndex( idx ) )
		{
			if ( !sounds.contains( idx ) )
			{
				sounds.insert( idx );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Static method
// Input  : *event - 
//			soundlist - 
//-----------------------------------------------------------------------------
void CSceneCache::PrecacheSceneEvent( CChoreoEvent *event, std::flat_multiset< int > &soundlist )
{
	if ( !event || event->GetType() != CChoreoEvent::SPEAK )
		return;

	int idx = soundemitterbase->GetSoundIndex( event->GetParameters() );
	if ( soundemitterbase->IsValidIndex( idx ) )
	{
		MEM_ALLOC_CREDIT();
		soundlist.insert( idx );
	}

	if ( event->GetCloseCaptionType() == CChoreoEvent::CC_MASTER )
	{
		char tok[ CChoreoEvent::MAX_CCTOKEN_STRING ];
		if ( event->GetPlaybackCloseCaptionToken( tok, sizeof( tok ) ) )
		{
			int idx = soundemitterbase->GetSoundIndex( tok );
			if ( soundemitterbase->IsValidIndex( idx ) && !soundlist.contains( idx ) )
			{
				MEM_ALLOC_CREDIT();
				soundlist.insert( idx );
			}
		}
	}
}

void CSceneCache::Rebuild( char const *filename )
{
	msecs = 0;
	sounds.clear();

	CChoreoScene *scene = BlockingLoadScene( filename );
	if ( scene )
	{
		// Walk all events looking for SPEAK events
		CChoreoEvent *event;
		int c = scene->GetNumEvents();
		for ( int i = 0; i < c; ++i )
		{
			event = scene->GetEvent( i );
			PrecacheSceneEvent( event, sounds );
		}

		// Update scene duration, too
		msecs = (int)( scene->FindStopTime() * 1000.0f + 0.5f );

		delete scene;
	}
}
